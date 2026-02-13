/* SPDX-License-Identifier: GPL-2.0 */
/*
 * AI Accelerator Driver - User/Kernel Interface
 *
 * This header defines the interface between userspace and the kernel driver.
 * It's included by both kernel code and userspace applications.
 */

#ifndef _UAPI_AI_ACCEL_H_
#define _UAPI_AI_ACCEL_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define AI_ACCEL_DEV_NAME "ai_accel"
#define AI_ACCEL_MAX_DEVICES 16

/* IOCTL magic number */
#define AI_IOC_MAGIC 'A'

/*
 * Data structures for IOCTL commands
 */

/* Device capabilities */
struct ai_device_caps {
    __u32 version;          /* Driver version */
    __u32 hw_version;       /* Hardware version */
    __u32 num_engines;      /* Number of compute engines */
    __u32 max_batch_size;   /* Maximum batch size */
    __u64 memory_size;      /* Device memory in bytes */
    __u64 max_alloc_size;   /* Maximum single allocation */
    __u32 features;         /* Feature flags */
    __u32 reserved[5];
};

/* Feature flags */
#define AI_FEAT_FP32        (1 << 0)
#define AI_FEAT_FP16        (1 << 1)
#define AI_FEAT_INT8        (1 << 2)
#define AI_FEAT_INT4        (1 << 3)
#define AI_FEAT_SPARSE      (1 << 4)
#define AI_FEAT_BATCH       (1 << 5)

/* Memory allocation request */
struct ai_alloc_request {
    __u64 size;             /* Requested size */
    __u32 flags;            /* Allocation flags */
    __u32 reserved;
    __u64 handle;           /* Returned handle */
    __u64 dma_addr;         /* DMA address (if applicable) */
};

/* Allocation flags */
#define AI_ALLOC_CACHED     (1 << 0)
#define AI_ALLOC_WRITECOMBINE (1 << 1)
#define AI_ALLOC_COHERENT   (1 << 2)

/* Free request */
struct ai_free_request {
    __u64 handle;
};

/* Inference submission */
struct ai_inference_request {
    __u64 model_handle;     /* Handle to loaded model */
    __u64 input_handle;     /* Handle to input buffer */
    __u64 output_handle;    /* Handle to output buffer */
    __u32 input_size;       /* Input data size */
    __u32 output_size;      /* Expected output size */
    __u32 flags;            /* Execution flags */
    __u32 priority;         /* Scheduling priority */
    __u64 user_data;        /* User context */
    __u64 fence;            /* Returned fence for completion */
};

/* Inference flags */
#define AI_INFER_SYNC       (1 << 0)  /* Synchronous execution */
#define AI_INFER_ASYNC      (1 << 1)  /* Asynchronous execution */
#define AI_INFER_PROFILING  (1 << 2)  /* Enable profiling */

/* Wait for completion */
struct ai_wait_request {
    __u64 fence;            /* Fence to wait on */
    __u64 timeout_ns;       /* Timeout in nanoseconds */
    __s32 status;           /* Returned status */
    __u32 reserved;
};

/* Status codes */
#define AI_STATUS_SUCCESS       0
#define AI_STATUS_PENDING       1
#define AI_STATUS_TIMEOUT       -1
#define AI_STATUS_ERROR         -2
#define AI_STATUS_INVALID       -3
#define AI_STATUS_NOMEM         -4

/* Profiling data */
struct ai_profile_data {
    __u64 fence;
    __u64 submit_ns;        /* Submission timestamp */
    __u64 start_ns;         /* Execution start timestamp */
    __u64 end_ns;           /* Execution end timestamp */
    __u64 hw_cycles;        /* Hardware cycles used */
    __u64 memory_read;      /* Bytes read from memory */
    __u64 memory_write;     /* Bytes written to memory */
    __u32 engine_id;        /* Engine that executed */
    __u32 reserved[3];
};

/* Model loading */
struct ai_load_model_request {
    __u64 model_data;       /* Pointer to model data */
    __u64 model_size;       /* Model size in bytes */
    __u32 flags;            /* Loading flags */
    __u32 reserved;
    __u64 model_handle;     /* Returned model handle */
};

/* Model unloading */
struct ai_unload_model_request {
    __u64 model_handle;
};

/*
 * IOCTL commands
 */
#define AI_IOC_GET_CAPS         _IOR(AI_IOC_MAGIC, 0, struct ai_device_caps)
#define AI_IOC_ALLOC            _IOWR(AI_IOC_MAGIC, 1, struct ai_alloc_request)
#define AI_IOC_FREE             _IOW(AI_IOC_MAGIC, 2, struct ai_free_request)
#define AI_IOC_LOAD_MODEL       _IOWR(AI_IOC_MAGIC, 3, struct ai_load_model_request)
#define AI_IOC_UNLOAD_MODEL     _IOW(AI_IOC_MAGIC, 4, struct ai_unload_model_request)
#define AI_IOC_SUBMIT           _IOWR(AI_IOC_MAGIC, 5, struct ai_inference_request)
#define AI_IOC_WAIT             _IOWR(AI_IOC_MAGIC, 6, struct ai_wait_request)
#define AI_IOC_GET_PROFILE      _IOWR(AI_IOC_MAGIC, 7, struct ai_profile_data)

/* Maximum IOCTL number */
#define AI_IOC_MAXNR 7

#endif /* _UAPI_AI_ACCEL_H_ */
