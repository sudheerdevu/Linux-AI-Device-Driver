// SPDX-License-Identifier: GPL-2.0
/*
 * AI Accelerator Driver - Main Module
 *
 * Educational kernel module demonstrating driver concepts for AI hardware.
 * This module provides:
 * - Character device interface
 * - DMA buffer management
 * - IOCTL command handling
 * - Memory-mapped I/O simulation
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/ktime.h>

#include "ai_accel.h"
#include "../include/uapi/ai_accel.h"

#define DRIVER_NAME "ai_accel"
#define DRIVER_VERSION 0x010000  /* 1.0.0 */

/* Module parameters */
static int simulate = 1;
module_param(simulate, int, 0644);
MODULE_PARM_DESC(simulate, "Simulation mode (default: 1)");

static int num_engines = 4;
module_param(num_engines, int, 0644);
MODULE_PARM_DESC(num_engines, "Number of compute engines (default: 4)");

/* Global state */
static dev_t ai_dev_number;
static struct class *ai_class;
static struct ai_device *ai_dev;

/* Device structure */
struct ai_device {
    struct cdev cdev;
    struct device *dev;
    struct mutex lock;
    
    /* Handle management */
    struct idr buffer_idr;
    struct idr model_idr;
    atomic_t fence_counter;
    
    /* Device capabilities */
    struct ai_device_caps caps;
    
    /* Statistics */
    atomic64_t total_inferences;
    atomic64_t total_bytes_processed;
};

/* Buffer tracking */
struct ai_buffer {
    void *cpu_addr;
    dma_addr_t dma_addr;
    size_t size;
    u32 flags;
};

/* Model tracking */
struct ai_model {
    void *data;
    size_t size;
    u32 flags;
};

/* Inference context */
struct ai_inference_ctx {
    u64 fence;
    struct completion done;
    s32 status;
    struct ai_profile_data profile;
};

/*
 * File operations
 */

static int ai_open(struct inode *inode, struct file *file)
{
    struct ai_device *dev = container_of(inode->i_cdev, struct ai_device, cdev);
    file->private_data = dev;
    
    pr_debug("ai_accel: device opened\n");
    return 0;
}

static int ai_release(struct inode *inode, struct file *file)
{
    pr_debug("ai_accel: device closed\n");
    return 0;
}

static ssize_t ai_read(struct file *file, char __user *buf,
                       size_t count, loff_t *ppos)
{
    /* Could return device status or statistics */
    return 0;
}

static ssize_t ai_write(struct file *file, const char __user *buf,
                        size_t count, loff_t *ppos)
{
    /* Could accept configuration or commands */
    return count;
}

/*
 * IOCTL handlers
 */

static int ai_ioctl_get_caps(struct ai_device *dev, void __user *arg)
{
    if (copy_to_user(arg, &dev->caps, sizeof(dev->caps)))
        return -EFAULT;
    return 0;
}

static int ai_ioctl_alloc(struct ai_device *dev, void __user *arg)
{
    struct ai_alloc_request req;
    struct ai_buffer *buf;
    int handle;
    
    if (copy_from_user(&req, arg, sizeof(req)))
        return -EFAULT;
    
    if (req.size == 0 || req.size > dev->caps.max_alloc_size)
        return -EINVAL;
    
    buf = kzalloc(sizeof(*buf), GFP_KERNEL);
    if (!buf)
        return -ENOMEM;
    
    buf->size = req.size;
    buf->flags = req.flags;
    
    if (simulate) {
        /* Simulation: allocate regular memory */
        buf->cpu_addr = vzalloc(req.size);
        buf->dma_addr = (dma_addr_t)(unsigned long)buf->cpu_addr;
    } else {
        /* Real: allocate DMA coherent memory */
        buf->cpu_addr = dma_alloc_coherent(dev->dev, req.size,
                                           &buf->dma_addr, GFP_KERNEL);
    }
    
    if (!buf->cpu_addr) {
        kfree(buf);
        return -ENOMEM;
    }
    
    mutex_lock(&dev->lock);
    handle = idr_alloc(&dev->buffer_idr, buf, 1, 0, GFP_KERNEL);
    mutex_unlock(&dev->lock);
    
    if (handle < 0) {
        if (simulate)
            vfree(buf->cpu_addr);
        else
            dma_free_coherent(dev->dev, buf->size, buf->cpu_addr, buf->dma_addr);
        kfree(buf);
        return handle;
    }
    
    req.handle = handle;
    req.dma_addr = buf->dma_addr;
    
    if (copy_to_user(arg, &req, sizeof(req))) {
        mutex_lock(&dev->lock);
        idr_remove(&dev->buffer_idr, handle);
        mutex_unlock(&dev->lock);
        if (simulate)
            vfree(buf->cpu_addr);
        else
            dma_free_coherent(dev->dev, buf->size, buf->cpu_addr, buf->dma_addr);
        kfree(buf);
        return -EFAULT;
    }
    
    pr_debug("ai_accel: allocated buffer handle=%d size=%llu\n", handle, req.size);
    return 0;
}

static int ai_ioctl_free(struct ai_device *dev, void __user *arg)
{
    struct ai_free_request req;
    struct ai_buffer *buf;
    
    if (copy_from_user(&req, arg, sizeof(req)))
        return -EFAULT;
    
    mutex_lock(&dev->lock);
    buf = idr_find(&dev->buffer_idr, req.handle);
    if (buf) {
        idr_remove(&dev->buffer_idr, req.handle);
    }
    mutex_unlock(&dev->lock);
    
    if (!buf)
        return -EINVAL;
    
    if (simulate)
        vfree(buf->cpu_addr);
    else
        dma_free_coherent(dev->dev, buf->size, buf->cpu_addr, buf->dma_addr);
    kfree(buf);
    
    pr_debug("ai_accel: freed buffer handle=%llu\n", req.handle);
    return 0;
}

static int ai_ioctl_load_model(struct ai_device *dev, void __user *arg)
{
    struct ai_load_model_request req;
    struct ai_model *model;
    int handle;
    
    if (copy_from_user(&req, arg, sizeof(req)))
        return -EFAULT;
    
    if (req.model_size == 0 || req.model_size > dev->caps.max_alloc_size)
        return -EINVAL;
    
    model = kzalloc(sizeof(*model), GFP_KERNEL);
    if (!model)
        return -ENOMEM;
    
    model->size = req.model_size;
    model->flags = req.flags;
    model->data = vmalloc(req.model_size);
    
    if (!model->data) {
        kfree(model);
        return -ENOMEM;
    }
    
    if (copy_from_user(model->data, (void __user *)req.model_data, req.model_size)) {
        vfree(model->data);
        kfree(model);
        return -EFAULT;
    }
    
    mutex_lock(&dev->lock);
    handle = idr_alloc(&dev->model_idr, model, 1, 0, GFP_KERNEL);
    mutex_unlock(&dev->lock);
    
    if (handle < 0) {
        vfree(model->data);
        kfree(model);
        return handle;
    }
    
    req.model_handle = handle;
    
    if (copy_to_user(arg, &req, sizeof(req))) {
        mutex_lock(&dev->lock);
        idr_remove(&dev->model_idr, handle);
        mutex_unlock(&dev->lock);
        vfree(model->data);
        kfree(model);
        return -EFAULT;
    }
    
    pr_debug("ai_accel: loaded model handle=%d size=%llu\n", handle, req.model_size);
    return 0;
}

static int ai_ioctl_submit(struct ai_device *dev, void __user *arg)
{
    struct ai_inference_request req;
    u64 fence;
    ktime_t start, end;
    
    if (copy_from_user(&req, arg, sizeof(req)))
        return -EFAULT;
    
    /* Validate handles */
    mutex_lock(&dev->lock);
    if (!idr_find(&dev->model_idr, req.model_handle) ||
        !idr_find(&dev->buffer_idr, req.input_handle) ||
        !idr_find(&dev->buffer_idr, req.output_handle)) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }
    mutex_unlock(&dev->lock);
    
    /* Generate fence */
    fence = atomic_inc_return(&dev->fence_counter);
    
    /* Simulate inference (in real driver, this would be async) */
    start = ktime_get();
    
    if (simulate) {
        /* Simulate some processing time */
        usleep_range(100, 200);
    } else {
        /* Real implementation would:
         * 1. Build command buffer
         * 2. Submit to hardware
         * 3. Wait for completion (or return for async)
         */
    }
    
    end = ktime_get();
    
    atomic64_inc(&dev->total_inferences);
    atomic64_add(req.input_size + req.output_size, &dev->total_bytes_processed);
    
    req.fence = fence;
    
    if (copy_to_user(arg, &req, sizeof(req)))
        return -EFAULT;
    
    pr_debug("ai_accel: inference submitted fence=%llu duration=%lldns\n",
             fence, ktime_to_ns(ktime_sub(end, start)));
    return 0;
}

static long ai_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct ai_device *dev = file->private_data;
    void __user *uarg = (void __user *)arg;
    
    if (_IOC_TYPE(cmd) != AI_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > AI_IOC_MAXNR)
        return -ENOTTY;
    
    switch (cmd) {
    case AI_IOC_GET_CAPS:
        return ai_ioctl_get_caps(dev, uarg);
    case AI_IOC_ALLOC:
        return ai_ioctl_alloc(dev, uarg);
    case AI_IOC_FREE:
        return ai_ioctl_free(dev, uarg);
    case AI_IOC_LOAD_MODEL:
        return ai_ioctl_load_model(dev, uarg);
    case AI_IOC_SUBMIT:
        return ai_ioctl_submit(dev, uarg);
    default:
        return -ENOTTY;
    }
}

static int ai_mmap(struct file *file, struct vm_area_struct *vma)
{
    /* Allow mapping device memory/buffers to userspace */
    /* Implementation depends on actual use case */
    return -ENOSYS;
}

static const struct file_operations ai_fops = {
    .owner          = THIS_MODULE,
    .open           = ai_open,
    .release        = ai_release,
    .read           = ai_read,
    .write          = ai_write,
    .unlocked_ioctl = ai_ioctl,
    .mmap           = ai_mmap,
};

/*
 * Sysfs attributes
 */

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
                            char *buf)
{
    return sprintf(buf, "%d.%d.%d\n",
                   (DRIVER_VERSION >> 16) & 0xFF,
                   (DRIVER_VERSION >> 8) & 0xFF,
                   DRIVER_VERSION & 0xFF);
}
static DEVICE_ATTR_RO(version);

static ssize_t total_inferences_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%llu\n", atomic64_read(&ai_dev->total_inferences));
}
static DEVICE_ATTR_RO(total_inferences);

static struct attribute *ai_attrs[] = {
    &dev_attr_version.attr,
    &dev_attr_total_inferences.attr,
    NULL
};

static struct attribute_group ai_attr_group = {
    .attrs = ai_attrs,
};

/*
 * Module init/exit
 */

static int __init ai_accel_init(void)
{
    int ret;
    
    pr_info("ai_accel: initializing driver (simulate=%d)\n", simulate);
    
    /* Allocate device structure */
    ai_dev = kzalloc(sizeof(*ai_dev), GFP_KERNEL);
    if (!ai_dev)
        return -ENOMEM;
    
    /* Initialize device state */
    mutex_init(&ai_dev->lock);
    idr_init(&ai_dev->buffer_idr);
    idr_init(&ai_dev->model_idr);
    atomic_set(&ai_dev->fence_counter, 0);
    atomic64_set(&ai_dev->total_inferences, 0);
    atomic64_set(&ai_dev->total_bytes_processed, 0);
    
    /* Set capabilities */
    ai_dev->caps.version = DRIVER_VERSION;
    ai_dev->caps.hw_version = simulate ? 0 : 0x100;
    ai_dev->caps.num_engines = num_engines;
    ai_dev->caps.max_batch_size = 32;
    ai_dev->caps.memory_size = 1ULL << 30;  /* 1 GB */
    ai_dev->caps.max_alloc_size = 256ULL << 20;  /* 256 MB */
    ai_dev->caps.features = AI_FEAT_FP32 | AI_FEAT_FP16 | AI_FEAT_INT8 | AI_FEAT_BATCH;
    
    /* Allocate device number */
    ret = alloc_chrdev_region(&ai_dev_number, 0, 1, DRIVER_NAME);
    if (ret < 0) {
        pr_err("ai_accel: failed to allocate device number\n");
        goto err_alloc;
    }
    
    /* Create device class */
    ai_class = class_create(THIS_MODULE, DRIVER_NAME);
    if (IS_ERR(ai_class)) {
        ret = PTR_ERR(ai_class);
        pr_err("ai_accel: failed to create class\n");
        goto err_class;
    }
    
    /* Initialize and add cdev */
    cdev_init(&ai_dev->cdev, &ai_fops);
    ai_dev->cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&ai_dev->cdev, ai_dev_number, 1);
    if (ret) {
        pr_err("ai_accel: failed to add cdev\n");
        goto err_cdev;
    }
    
    /* Create device */
    ai_dev->dev = device_create(ai_class, NULL, ai_dev_number, NULL, DRIVER_NAME);
    if (IS_ERR(ai_dev->dev)) {
        ret = PTR_ERR(ai_dev->dev);
        pr_err("ai_accel: failed to create device\n");
        goto err_device;
    }
    
    /* Add sysfs attributes */
    ret = sysfs_create_group(&ai_dev->dev->kobj, &ai_attr_group);
    if (ret) {
        pr_warn("ai_accel: failed to create sysfs group\n");
    }
    
    pr_info("ai_accel: driver initialized (major=%d)\n", MAJOR(ai_dev_number));
    return 0;

err_device:
    cdev_del(&ai_dev->cdev);
err_cdev:
    class_destroy(ai_class);
err_class:
    unregister_chrdev_region(ai_dev_number, 1);
err_alloc:
    kfree(ai_dev);
    return ret;
}

static void __exit ai_accel_exit(void)
{
    pr_info("ai_accel: unloading driver\n");
    
    sysfs_remove_group(&ai_dev->dev->kobj, &ai_attr_group);
    device_destroy(ai_class, ai_dev_number);
    cdev_del(&ai_dev->cdev);
    class_destroy(ai_class);
    unregister_chrdev_region(ai_dev_number, 1);
    
    /* Clean up any remaining allocations */
    idr_destroy(&ai_dev->buffer_idr);
    idr_destroy(&ai_dev->model_idr);
    
    kfree(ai_dev);
    
    pr_info("ai_accel: driver unloaded\n");
}

module_init(ai_accel_init);
module_exit(ai_accel_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("AI Infrastructure Team");
MODULE_DESCRIPTION("Educational AI Accelerator Driver");
MODULE_VERSION("1.0.0");
