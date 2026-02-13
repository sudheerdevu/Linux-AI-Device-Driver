// SPDX-License-Identifier: GPL-2.0
/*
 * AI Accelerator Device Driver - DMA Operations Module
 * 
 * This module handles Direct Memory Access operations for efficient
 * data transfer between host memory and accelerator memory.
 */

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include "ai_accel.h"

/* DMA buffer descriptor */
struct ai_dma_buffer {
    void *cpu_addr;              /* CPU virtual address */
    dma_addr_t dma_addr;         /* DMA bus address */
    size_t size;                 /* Buffer size */
    enum dma_data_direction dir; /* Transfer direction */
    struct scatterlist *sg;      /* Scatter-gather list */
    int sg_count;                /* Number of SG entries */
    bool mapped;                 /* Is buffer mapped? */
};

/* DMA transfer context */
struct ai_dma_transfer {
    struct ai_dma_buffer *src;
    struct ai_dma_buffer *dst;
    struct completion done;
    int status;
    u64 bytes_transferred;
    ktime_t start_time;
    ktime_t end_time;
};

/* DMA channel pool */
#define AI_DMA_CHANNELS 4
static struct dma_chan *dma_channels[AI_DMA_CHANNELS];
static DEFINE_SPINLOCK(channel_lock);
static unsigned long channel_bitmap;

/**
 * ai_dma_init - Initialize DMA subsystem
 * @dev: Parent device
 *
 * Returns 0 on success, negative error code on failure
 */
int ai_dma_init(struct device *dev)
{
    int i, ret = 0;
    dma_cap_mask_t mask;

    dma_cap_zero(mask);
    dma_cap_set(DMA_MEMCPY, mask);
    dma_cap_set(DMA_SG, mask);

    spin_lock(&channel_lock);
    channel_bitmap = 0;

    for (i = 0; i < AI_DMA_CHANNELS; i++) {
        dma_channels[i] = dma_request_channel(mask, NULL, NULL);
        if (!dma_channels[i]) {
            dev_warn(dev, "Failed to request DMA channel %d\n", i);
            /* Continue with available channels */
        } else {
            set_bit(i, &channel_bitmap);
        }
    }
    spin_unlock(&channel_lock);

    if (channel_bitmap == 0) {
        dev_err(dev, "No DMA channels available\n");
        ret = -ENODEV;
    }

    return ret;
}
EXPORT_SYMBOL_GPL(ai_dma_init);

/**
 * ai_dma_exit - Cleanup DMA subsystem
 */
void ai_dma_exit(void)
{
    int i;

    spin_lock(&channel_lock);
    for (i = 0; i < AI_DMA_CHANNELS; i++) {
        if (dma_channels[i]) {
            dma_release_channel(dma_channels[i]);
            dma_channels[i] = NULL;
        }
    }
    channel_bitmap = 0;
    spin_unlock(&channel_lock);
}
EXPORT_SYMBOL_GPL(ai_dma_exit);

/**
 * ai_dma_alloc_buffer - Allocate DMA-capable buffer
 * @dev: Device for DMA operations
 * @size: Buffer size in bytes
 * @dir: DMA transfer direction
 *
 * Returns pointer to buffer descriptor or NULL on failure
 */
struct ai_dma_buffer *ai_dma_alloc_buffer(struct device *dev, size_t size,
                                          enum dma_data_direction dir)
{
    struct ai_dma_buffer *buf;

    buf = kzalloc(sizeof(*buf), GFP_KERNEL);
    if (!buf)
        return NULL;

    buf->size = size;
    buf->dir = dir;
    buf->mapped = false;

    /* Allocate DMA coherent memory */
    buf->cpu_addr = dma_alloc_coherent(dev, size, &buf->dma_addr, GFP_KERNEL);
    if (!buf->cpu_addr) {
        kfree(buf);
        return NULL;
    }

    return buf;
}
EXPORT_SYMBOL_GPL(ai_dma_alloc_buffer);

/**
 * ai_dma_free_buffer - Free DMA buffer
 * @dev: Device for DMA operations
 * @buf: Buffer to free
 */
void ai_dma_free_buffer(struct device *dev, struct ai_dma_buffer *buf)
{
    if (!buf)
        return;

    if (buf->cpu_addr)
        dma_free_coherent(dev, buf->size, buf->cpu_addr, buf->dma_addr);

    if (buf->sg)
        kfree(buf->sg);

    kfree(buf);
}
EXPORT_SYMBOL_GPL(ai_dma_free_buffer);

/**
 * ai_dma_map_user_buffer - Map user buffer for DMA
 * @dev: Device for DMA operations
 * @user_addr: User-space virtual address
 * @size: Buffer size
 * @dir: Transfer direction
 *
 * Returns buffer descriptor or ERR_PTR on failure
 */
struct ai_dma_buffer *ai_dma_map_user_buffer(struct device *dev,
                                              void __user *user_addr,
                                              size_t size,
                                              enum dma_data_direction dir)
{
    struct ai_dma_buffer *buf;
    struct page **pages;
    int nr_pages;
    int ret;

    buf = kzalloc(sizeof(*buf), GFP_KERNEL);
    if (!buf)
        return ERR_PTR(-ENOMEM);

    /* Calculate number of pages */
    nr_pages = DIV_ROUND_UP(size + offset_in_page(user_addr), PAGE_SIZE);

    pages = kvmalloc_array(nr_pages, sizeof(struct page *), GFP_KERNEL);
    if (!pages) {
        kfree(buf);
        return ERR_PTR(-ENOMEM);
    }

    /* Pin user pages */
    ret = pin_user_pages_fast((unsigned long)user_addr, nr_pages,
                              dir == DMA_FROM_DEVICE ? FOLL_WRITE : 0,
                              pages);
    if (ret < 0) {
        kvfree(pages);
        kfree(buf);
        return ERR_PTR(ret);
    }

    if (ret != nr_pages) {
        unpin_user_pages(pages, ret);
        kvfree(pages);
        kfree(buf);
        return ERR_PTR(-EFAULT);
    }

    /* Allocate and initialize scatter-gather list */
    buf->sg = kvmalloc_array(nr_pages, sizeof(struct scatterlist), GFP_KERNEL);
    if (!buf->sg) {
        unpin_user_pages(pages, nr_pages);
        kvfree(pages);
        kfree(buf);
        return ERR_PTR(-ENOMEM);
    }

    sg_init_table(buf->sg, nr_pages);

    /* Fill scatter-gather entries */
    for (int i = 0; i < nr_pages; i++) {
        unsigned int offset = (i == 0) ? offset_in_page(user_addr) : 0;
        unsigned int len = min_t(size_t,
                                PAGE_SIZE - offset,
                                size - (i * PAGE_SIZE - offset_in_page(user_addr)));
        sg_set_page(&buf->sg[i], pages[i], len, offset);
    }

    /* Map for DMA */
    buf->sg_count = dma_map_sg(dev, buf->sg, nr_pages, dir);
    if (buf->sg_count == 0) {
        unpin_user_pages(pages, nr_pages);
        kvfree(buf->sg);
        kvfree(pages);
        kfree(buf);
        return ERR_PTR(-EIO);
    }

    buf->size = size;
    buf->dir = dir;
    buf->mapped = true;

    kvfree(pages);
    return buf;
}
EXPORT_SYMBOL_GPL(ai_dma_map_user_buffer);

/* DMA completion callback */
static void ai_dma_callback(void *data)
{
    struct ai_dma_transfer *xfer = data;

    xfer->end_time = ktime_get();
    xfer->status = 0;
    complete(&xfer->done);
}

/**
 * ai_dma_transfer_sync - Perform synchronous DMA transfer
 * @dev: Device for DMA operations
 * @dst_addr: Destination DMA address
 * @src_addr: Source DMA address
 * @size: Transfer size
 * @timeout_ms: Timeout in milliseconds
 *
 * Returns 0 on success, negative error code on failure
 */
int ai_dma_transfer_sync(struct device *dev, dma_addr_t dst_addr,
                         dma_addr_t src_addr, size_t size,
                         unsigned int timeout_ms)
{
    struct ai_dma_transfer xfer;
    struct dma_chan *chan = NULL;
    struct dma_async_tx_descriptor *tx;
    dma_cookie_t cookie;
    int ret;
    int i;

    /* Find available channel */
    spin_lock(&channel_lock);
    for (i = 0; i < AI_DMA_CHANNELS; i++) {
        if (test_bit(i, &channel_bitmap) && dma_channels[i]) {
            chan = dma_channels[i];
            break;
        }
    }
    spin_unlock(&channel_lock);

    if (!chan)
        return -ENODEV;

    /* Prepare transfer */
    init_completion(&xfer.done);
    xfer.status = -EINPROGRESS;
    xfer.bytes_transferred = 0;
    xfer.start_time = ktime_get();

    /* Create DMA descriptor */
    tx = dmaengine_prep_dma_memcpy(chan, dst_addr, src_addr, size,
                                    DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
    if (!tx)
        return -ENOMEM;

    tx->callback = ai_dma_callback;
    tx->callback_param = &xfer;

    /* Submit transfer */
    cookie = dmaengine_submit(tx);
    ret = dma_submit_error(cookie);
    if (ret)
        return ret;

    /* Start DMA */
    dma_async_issue_pending(chan);

    /* Wait for completion */
    ret = wait_for_completion_timeout(&xfer.done,
                                       msecs_to_jiffies(timeout_ms));
    if (ret == 0) {
        dmaengine_terminate_sync(chan);
        return -ETIMEDOUT;
    }

    xfer.bytes_transferred = size;
    return xfer.status;
}
EXPORT_SYMBOL_GPL(ai_dma_transfer_sync);

/**
 * ai_dma_transfer_async - Initiate asynchronous DMA transfer
 * @dev: Device for DMA operations
 * @dst_addr: Destination DMA address
 * @src_addr: Source DMA address
 * @size: Transfer size
 * @callback: Completion callback
 * @callback_data: Data passed to callback
 *
 * Returns transfer cookie on success, negative error code on failure
 */
dma_cookie_t ai_dma_transfer_async(struct device *dev, dma_addr_t dst_addr,
                                    dma_addr_t src_addr, size_t size,
                                    dma_async_tx_callback callback,
                                    void *callback_data)
{
    struct dma_chan *chan = NULL;
    struct dma_async_tx_descriptor *tx;
    dma_cookie_t cookie;
    int i;

    /* Find available channel */
    spin_lock(&channel_lock);
    for (i = 0; i < AI_DMA_CHANNELS; i++) {
        if (test_bit(i, &channel_bitmap) && dma_channels[i]) {
            chan = dma_channels[i];
            break;
        }
    }
    spin_unlock(&channel_lock);

    if (!chan)
        return -ENODEV;

    tx = dmaengine_prep_dma_memcpy(chan, dst_addr, src_addr, size,
                                    DMA_CTRL_ACK | DMA_PREP_INTERRUPT);
    if (!tx)
        return -ENOMEM;

    tx->callback = callback;
    tx->callback_param = callback_data;

    cookie = dmaengine_submit(tx);
    if (!dma_submit_error(cookie))
        dma_async_issue_pending(chan);

    return cookie;
}
EXPORT_SYMBOL_GPL(ai_dma_transfer_async);

/**
 * ai_dma_sync_for_cpu - Sync buffer for CPU access
 * @dev: Device for DMA operations
 * @buf: DMA buffer
 */
void ai_dma_sync_for_cpu(struct device *dev, struct ai_dma_buffer *buf)
{
    if (!buf || !buf->mapped)
        return;

    if (buf->sg)
        dma_sync_sg_for_cpu(dev, buf->sg, buf->sg_count, buf->dir);
    else
        dma_sync_single_for_cpu(dev, buf->dma_addr, buf->size, buf->dir);
}
EXPORT_SYMBOL_GPL(ai_dma_sync_for_cpu);

/**
 * ai_dma_sync_for_device - Sync buffer for device access
 * @dev: Device for DMA operations
 * @buf: DMA buffer
 */
void ai_dma_sync_for_device(struct device *dev, struct ai_dma_buffer *buf)
{
    if (!buf || !buf->mapped)
        return;

    if (buf->sg)
        dma_sync_sg_for_device(dev, buf->sg, buf->sg_count, buf->dir);
    else
        dma_sync_single_for_device(dev, buf->dma_addr, buf->size, buf->dir);
}
EXPORT_SYMBOL_GPL(ai_dma_sync_for_device);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AI Accelerator DMA Operations Module");
MODULE_AUTHOR("AI Performance Engineering");
