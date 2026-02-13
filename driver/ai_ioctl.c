// SPDX-License-Identifier: GPL-2.0
/*
 * AI Accelerator Device Driver - IOCTL Handlers Module
 *
 * This module implements all ioctl command handlers for the AI accelerator driver.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include "ai_accel.h"
#include "../include/uapi/ai_accel.h"

/* Internal context for ioctl operations */
struct ai_ioctl_context {
    struct ai_accel_device *dev;
    struct file *filp;
    pid_t pid;
    u64 total_inferences;
    u64 total_bytes_transferred;
};

/**
 * ai_ioctl_get_info - Get device information
 * @ctx: IOCTL context
 * @arg: User-space pointer to ai_accel_info structure
 */
static int ai_ioctl_get_info(struct ai_ioctl_context *ctx, unsigned long arg)
{
    struct ai_accel_info info;
    struct ai_accel_device *adev = ctx->dev;

    memset(&info, 0, sizeof(info));

    /* Fill device information */
    strscpy(info.name, "AI Accelerator v1.0", sizeof(info.name));
    info.version_major = 1;
    info.version_minor = 0;
    info.version_patch = 0;

    /* Hardware capabilities */
    info.max_batch_size = 64;
    info.max_input_size = 16 * 1024 * 1024;  /* 16 MB */
    info.max_output_size = 16 * 1024 * 1024; /* 16 MB */
    info.supported_ops = AI_OP_INFERENCE | AI_OP_TRAINING | AI_OP_PROFILING;

    /* Memory information */
    info.device_memory_size = adev ? adev->mem_size : 0;
    info.device_memory_free = adev ? adev->mem_size - adev->mem_used : 0;

    /* Performance capabilities */
    info.max_compute_units = 64;
    info.max_frequency_mhz = 2000;
    info.memory_bandwidth_gbps = 400;

    if (copy_to_user((void __user *)arg, &info, sizeof(info)))
        return -EFAULT;

    return 0;
}

/**
 * ai_ioctl_alloc_memory - Allocate device memory
 * @ctx: IOCTL context
 * @arg: User-space pointer to ai_accel_mem_alloc structure
 */
static int ai_ioctl_alloc_memory(struct ai_ioctl_context *ctx, unsigned long arg)
{
    struct ai_accel_mem_alloc alloc;
    struct ai_accel_device *adev = ctx->dev;
    void *mem;
    dma_addr_t dma_handle;
    int ret = 0;

    if (copy_from_user(&alloc, (void __user *)arg, sizeof(alloc)))
        return -EFAULT;

    /* Validate size */
    if (alloc.size == 0 || alloc.size > (64 * 1024 * 1024))
        return -EINVAL;

    /* Align size to page boundary */
    alloc.size = PAGE_ALIGN(alloc.size);

    /* Check available memory */
    if (adev->mem_used + alloc.size > adev->mem_size)
        return -ENOMEM;

    /* Allocate DMA-coherent memory */
    mem = dma_alloc_coherent(adev->dev, alloc.size, &dma_handle, GFP_KERNEL);
    if (!mem)
        return -ENOMEM;

    /* Return handle to userspace */
    alloc.handle = (u64)dma_handle;
    adev->mem_used += alloc.size;

    if (copy_to_user((void __user *)arg, &alloc, sizeof(alloc))) {
        dma_free_coherent(adev->dev, alloc.size, mem, dma_handle);
        adev->mem_used -= alloc.size;
        return -EFAULT;
    }

    return ret;
}

/**
 * ai_ioctl_free_memory - Free device memory
 * @ctx: IOCTL context
 * @arg: User-space pointer to ai_accel_mem_free structure
 */
static int ai_ioctl_free_memory(struct ai_ioctl_context *ctx, unsigned long arg)
{
    struct ai_accel_mem_free mfree;
    struct ai_accel_device *adev = ctx->dev;

    if (copy_from_user(&mfree, (void __user *)arg, sizeof(mfree)))
        return -EFAULT;

    /* In a real driver, we would track allocations and free properly */
    /* For demonstration, just update memory usage */
    if (mfree.size <= adev->mem_used)
        adev->mem_used -= mfree.size;

    return 0;
}

/**
 * ai_ioctl_submit_inference - Submit inference job
 * @ctx: IOCTL context
 * @arg: User-space pointer to ai_accel_inference structure
 */
static int ai_ioctl_submit_inference(struct ai_ioctl_context *ctx, unsigned long arg)
{
    struct ai_accel_inference inf;
    struct ai_accel_device *adev = ctx->dev;
    void *input_data = NULL;
    void *output_data = NULL;
    int ret = 0;

    if (copy_from_user(&inf, (void __user *)arg, sizeof(inf)))
        return -EFAULT;

    /* Validate sizes */
    if (inf.input_size == 0 || inf.input_size > (16 * 1024 * 1024))
        return -EINVAL;
    if (inf.output_size == 0 || inf.output_size > (16 * 1024 * 1024))
        return -EINVAL;
    if (inf.batch_size == 0 || inf.batch_size > 64)
        return -EINVAL;

    /* Allocate kernel buffers */
    input_data = kvmalloc(inf.input_size, GFP_KERNEL);
    if (!input_data)
        return -ENOMEM;

    output_data = kvmalloc(inf.output_size, GFP_KERNEL);
    if (!output_data) {
        kvfree(input_data);
        return -ENOMEM;
    }

    /* Copy input from user space */
    if (copy_from_user(input_data, (void __user *)inf.input_data, inf.input_size)) {
        ret = -EFAULT;
        goto out;
    }

    /* Simulate inference (in real driver, submit to hardware queue) */
    inf.latency_ns = ktime_get_ns();

    /* Execute inference - simplified simulation */
    memcpy(output_data, input_data, min(inf.input_size, inf.output_size));

    inf.latency_ns = ktime_get_ns() - inf.latency_ns;
    inf.status = AI_STATUS_SUCCESS;

    /* Copy output to user space */
    if (copy_to_user((void __user *)inf.output_data, output_data, inf.output_size)) {
        ret = -EFAULT;
        goto out;
    }

    /* Update result structure */
    if (copy_to_user((void __user *)arg, &inf, sizeof(inf))) {
        ret = -EFAULT;
        goto out;
    }

    ctx->total_inferences++;
    ctx->total_bytes_transferred += inf.input_size + inf.output_size;

out:
    kvfree(input_data);
    kvfree(output_data);
    return ret;
}

/**
 * ai_ioctl_get_stats - Get device statistics
 * @ctx: IOCTL context
 * @arg: User-space pointer to ai_accel_stats structure
 */
static int ai_ioctl_get_stats(struct ai_ioctl_context *ctx, unsigned long arg)
{
    struct ai_accel_stats stats;
    struct ai_accel_device *adev = ctx->dev;

    memset(&stats, 0, sizeof(stats));

    stats.total_inferences = ctx->total_inferences;
    stats.total_bytes_in = ctx->total_bytes_transferred / 2;
    stats.total_bytes_out = ctx->total_bytes_transferred / 2;
    stats.memory_used = adev ? adev->mem_used : 0;
    stats.memory_total = adev ? adev->mem_size : 0;
    stats.active_jobs = 0;  /* Would track from job queue */
    stats.completed_jobs = ctx->total_inferences;
    stats.failed_jobs = 0;
    stats.average_latency_ns = 1000000;  /* 1ms placeholder */

    if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
        return -EFAULT;

    return 0;
}

/**
 * ai_ioctl_set_power_mode - Set device power mode
 * @ctx: IOCTL context
 * @arg: Power mode value
 */
static int ai_ioctl_set_power_mode(struct ai_ioctl_context *ctx, unsigned long arg)
{
    u32 mode = (u32)arg;

    if (mode > AI_POWER_MODE_MAX)
        return -EINVAL;

    /* In real driver, configure hardware power state */
    pr_info("ai_accel: Setting power mode to %u\n", mode);

    return 0;
}

/**
 * ai_ioctl_wait_completion - Wait for job completion
 * @ctx: IOCTL context
 * @arg: User-space pointer to ai_accel_wait structure
 */
static int ai_ioctl_wait_completion(struct ai_ioctl_context *ctx, unsigned long arg)
{
    struct ai_accel_wait wait;

    if (copy_from_user(&wait, (void __user *)arg, sizeof(wait)))
        return -EFAULT;

    /* In real driver, wait on completion queue */
    /* For now, return immediately as completed */
    wait.status = AI_STATUS_SUCCESS;
    wait.result = 0;

    if (copy_to_user((void __user *)arg, &wait, sizeof(wait)))
        return -EFAULT;

    return 0;
}

/**
 * ai_accel_ioctl - Main IOCTL dispatcher
 * @filp: File pointer
 * @cmd: IOCTL command
 * @arg: Command argument
 */
long ai_accel_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct ai_ioctl_context ctx;
    int ret = 0;

    /* Initialize context */
    ctx.dev = filp->private_data;
    ctx.filp = filp;
    ctx.pid = current->pid;
    ctx.total_inferences = 0;
    ctx.total_bytes_transferred = 0;

    /* Validate magic number */
    if (_IOC_TYPE(cmd) != AI_ACCEL_IOC_MAGIC)
        return -ENOTTY;

    /* Check access permissions */
    if (_IOC_DIR(cmd) & _IOC_READ) {
        if (!access_ok((void __user *)arg, _IOC_SIZE(cmd)))
            return -EFAULT;
    }
    if (_IOC_DIR(cmd) & _IOC_WRITE) {
        if (!access_ok((void __user *)arg, _IOC_SIZE(cmd)))
            return -EFAULT;
    }

    switch (cmd) {
    case AI_ACCEL_IOC_GET_INFO:
        ret = ai_ioctl_get_info(&ctx, arg);
        break;

    case AI_ACCEL_IOC_ALLOC_MEM:
        ret = ai_ioctl_alloc_memory(&ctx, arg);
        break;

    case AI_ACCEL_IOC_FREE_MEM:
        ret = ai_ioctl_free_memory(&ctx, arg);
        break;

    case AI_ACCEL_IOC_SUBMIT_INFERENCE:
        ret = ai_ioctl_submit_inference(&ctx, arg);
        break;

    case AI_ACCEL_IOC_GET_STATS:
        ret = ai_ioctl_get_stats(&ctx, arg);
        break;

    case AI_ACCEL_IOC_SET_POWER:
        ret = ai_ioctl_set_power_mode(&ctx, arg);
        break;

    case AI_ACCEL_IOC_WAIT:
        ret = ai_ioctl_wait_completion(&ctx, arg);
        break;

    default:
        ret = -ENOTTY;
        break;
    }

    return ret;
}
EXPORT_SYMBOL_GPL(ai_accel_ioctl);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AI Accelerator IOCTL Handlers Module");
MODULE_AUTHOR("AI Performance Engineering");
