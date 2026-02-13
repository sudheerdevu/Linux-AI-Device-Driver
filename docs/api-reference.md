# AI Accelerator Driver - API Reference

## Overview

This document provides a complete API reference for the AI Accelerator driver and userspace library.

## Kernel IOCTL Interface

### Header Files

```c
#include <linux/ioctl.h>
#include "ai_accel.h"
```

### IOCTL Magic Number

```c
#define AI_ACCEL_IOC_MAGIC 'A'
```

---

### AI_ACCEL_IOC_GET_INFO

Get device information.

**Direction:** Read  
**Parameter:** `struct ai_accel_info *`

```c
struct ai_accel_info {
    char name[64];              /* Device name string */
    uint32_t version_major;     /* Driver version major */
    uint32_t version_minor;     /* Driver version minor */
    uint32_t version_patch;     /* Driver version patch */
    uint64_t device_memory_size; /* Total device memory (bytes) */
    uint64_t device_memory_free; /* Available device memory */
    uint32_t max_batch_size;    /* Maximum supported batch size */
    uint64_t max_input_size;    /* Maximum input buffer size */
    uint64_t max_output_size;   /* Maximum output buffer size */
    uint32_t supported_ops;     /* Bitmask of supported operations */
    uint32_t max_compute_units; /* Number of compute units */
    uint32_t max_frequency_mhz; /* Maximum frequency in MHz */
    uint32_t memory_bandwidth_gbps; /* Memory bandwidth GB/s */
};
```

**Returns:** 0 on success, negative error code on failure

**Example:**
```c
struct ai_accel_info info;
int ret = ioctl(fd, AI_ACCEL_IOC_GET_INFO, &info);
if (ret == 0) {
    printf("Device: %s\n", info.name);
    printf("Memory: %lu MB\n", info.device_memory_size / (1024*1024));
}
```

---

### AI_ACCEL_IOC_ALLOC_MEM

Allocate device memory buffer.

**Direction:** Read/Write  
**Parameter:** `struct ai_accel_mem_alloc *`

```c
struct ai_accel_mem_alloc {
    uint64_t size;    /* [in] Requested size, [out] Actual allocated size */
    uint64_t handle;  /* [out] Memory handle for future operations */
    uint32_t flags;   /* [in] Allocation flags */
};

/* Allocation flags */
#define AI_MEM_FLAG_CACHED     0x01  /* Allocate cached memory */
#define AI_MEM_FLAG_CONTIGUOUS 0x02  /* Require contiguous allocation */
```

**Returns:** 0 on success, -ENOMEM if out of memory

---

### AI_ACCEL_IOC_FREE_MEM

Free previously allocated device memory.

**Direction:** Write  
**Parameter:** `struct ai_accel_mem_free *`

```c
struct ai_accel_mem_free {
    uint64_t handle;  /* Memory handle to free */
    uint64_t size;    /* Size hint for deallocation */
};
```

**Returns:** 0 on success, -EINVAL if invalid handle

---

### AI_ACCEL_IOC_SUBMIT_INFERENCE

Submit an inference job for execution.

**Direction:** Read/Write  
**Parameter:** `struct ai_accel_inference *`

```c
struct ai_accel_inference {
    uint64_t input_data;     /* Input data pointer or handle */
    uint64_t input_size;     /* Input data size in bytes */
    uint64_t output_data;    /* Output buffer pointer or handle */
    uint64_t output_size;    /* Output buffer size in bytes */
    uint32_t batch_size;     /* Batch size */
    uint32_t flags;          /* Operation flags */
    int32_t status;          /* [out] Completion status */
    uint64_t latency_ns;     /* [out] Execution latency in nanoseconds */
};

/* Status codes */
#define AI_STATUS_SUCCESS     0
#define AI_STATUS_ERROR      -1
#define AI_STATUS_TIMEOUT    -2
#define AI_STATUS_CANCELLED  -3
```

**Returns:** 0 on success

---

### AI_ACCEL_IOC_GET_STATS

Get device statistics.

**Direction:** Read  
**Parameter:** `struct ai_accel_stats *`

```c
struct ai_accel_stats {
    uint64_t total_inferences;   /* Total inference count */
    uint64_t total_bytes_in;     /* Total input bytes processed */
    uint64_t total_bytes_out;    /* Total output bytes produced */
    uint64_t memory_used;        /* Current memory usage */
    uint64_t memory_total;       /* Total device memory */
    uint32_t active_jobs;        /* Currently executing jobs */
    uint32_t completed_jobs;     /* Successfully completed jobs */
    uint32_t failed_jobs;        /* Failed jobs */
    uint64_t average_latency_ns; /* Average job latency */
};
```

---

### AI_ACCEL_IOC_SET_POWER

Set device power mode.

**Direction:** Write  
**Parameter:** `uint32_t power_mode`

```c
/* Power modes */
#define AI_POWER_MODE_DEFAULT  0  /* Automatic power management */
#define AI_POWER_MODE_LOW      1  /* Low power, reduced performance */
#define AI_POWER_MODE_BALANCED 2  /* Balanced power/performance */
#define AI_POWER_MODE_HIGH     3  /* Maximum performance */
#define AI_POWER_MODE_MAX      4  /* Unrestricted (may overheat) */
```

---

### AI_ACCEL_IOC_WAIT

Wait for job completion.

**Direction:** Read/Write  
**Parameter:** `struct ai_accel_wait *`

```c
struct ai_accel_wait {
    uint64_t job_id;      /* Job ID to wait for */
    uint32_t timeout_ms;  /* Timeout in milliseconds */
    int32_t status;       /* [out] Job completion status */
    int32_t result;       /* [out] Job result code */
};
```

---

## Userspace Library API (libaidrv)

### Library Lifecycle

#### ai_init
```c
ai_error_t ai_init(void);
```
Initialize the library. Must be called before any other function.

**Returns:** `AI_SUCCESS` or error code

#### ai_shutdown
```c
void ai_shutdown(void);
```
Clean up library resources.

#### ai_get_version
```c
const char* ai_get_version(void);
```
Get library version string (e.g., "1.0.0").

#### ai_get_error_string
```c
const char* ai_get_error_string(ai_error_t error);
```
Get human-readable error description.

---

### Device Management

#### ai_get_device_count
```c
ai_error_t ai_get_device_count(int* count);
```
Get number of available devices.

#### ai_open_device
```c
ai_error_t ai_open_device(int device_index, ai_device_t* device);
```
Open device by index. Returns handle in `device`.

#### ai_close_device
```c
ai_error_t ai_close_device(ai_device_t device);
```
Close device and release resources.

#### ai_get_device_info
```c
ai_error_t ai_get_device_info(ai_device_t device, ai_device_info_t* info);
```
Get device information.

#### ai_get_device_stats
```c
ai_error_t ai_get_device_stats(ai_device_t device, ai_stats_t* stats);
```
Get device statistics.

#### ai_set_power_mode
```c
ai_error_t ai_set_power_mode(ai_device_t device, ai_power_mode_t mode);
```
Set device power mode.

---

### Memory Management

#### ai_alloc_buffer
```c
ai_error_t ai_alloc_buffer(ai_device_t device, size_t size, ai_buffer_t* buffer);
```
Allocate device memory buffer.

**Parameters:**
- `device` - Device handle
- `size` - Buffer size in bytes
- `buffer` - Pointer to receive buffer handle

#### ai_free_buffer
```c
ai_error_t ai_free_buffer(ai_buffer_t buffer);
```
Free device memory buffer.

#### ai_copy_to_device
```c
ai_error_t ai_copy_to_device(ai_buffer_t buffer, const void* src, 
                              size_t size, size_t offset);
```
Copy data from host to device buffer.

#### ai_copy_from_device
```c
ai_error_t ai_copy_from_device(ai_buffer_t buffer, void* dst,
                                size_t size, size_t offset);
```
Copy data from device buffer to host.

#### ai_map_buffer / ai_unmap_buffer
```c
ai_error_t ai_map_buffer(ai_buffer_t buffer, void** ptr);
ai_error_t ai_unmap_buffer(ai_buffer_t buffer);
```
Map/unmap buffer for zero-copy access.

---

### Model Management

#### ai_load_model
```c
ai_error_t ai_load_model(ai_device_t device, const char* path, ai_model_t* model);
```
Load model from file.

#### ai_load_model_from_memory
```c
ai_error_t ai_load_model_from_memory(ai_device_t device, const void* data,
                                      size_t size, ai_model_t* model);
```
Load model from memory buffer.

#### ai_unload_model
```c
ai_error_t ai_unload_model(ai_model_t model);
```
Unload model and free resources.

#### ai_get_model_input / ai_get_model_output
```c
ai_error_t ai_get_model_input(ai_model_t model, int index, ai_tensor_desc_t* desc);
ai_error_t ai_get_model_output(ai_model_t model, int index, ai_tensor_desc_t* desc);
```
Get input/output tensor descriptors.

---

### Inference

#### ai_run_inference
```c
ai_error_t ai_run_inference(ai_model_t model,
                            ai_buffer_t* inputs, int num_inputs,
                            ai_buffer_t* outputs, int num_outputs,
                            const ai_inference_params_t* params);
```
Run synchronous inference.

**Parameters:**
- `model` - Loaded model handle
- `inputs` - Array of input buffer handles
- `num_inputs` - Number of inputs
- `outputs` - Array of output buffer handles
- `num_outputs` - Number of outputs
- `params` - Inference parameters (NULL for defaults)

#### ai_submit_inference
```c
ai_error_t ai_submit_inference(ai_model_t model,
                                ai_buffer_t* inputs, int num_inputs,
                                ai_buffer_t* outputs, int num_outputs,
                                const ai_inference_params_t* params,
                                ai_job_t* job);
```
Submit asynchronous inference job.

#### ai_wait_job
```c
ai_error_t ai_wait_job(ai_job_t job, uint32_t timeout_ms);
```
Wait for job completion.

#### ai_check_job
```c
ai_error_t ai_check_job(ai_job_t job, int* complete);
```
Check if job is complete (non-blocking).

#### ai_get_job_result
```c
ai_error_t ai_get_job_result(ai_job_t job, uint64_t* latency_ns);
```
Get job result and latency.

#### ai_release_job
```c
void ai_release_job(ai_job_t job);
```
Release job handle.

---

### Profiling

#### ai_enable_profiling / ai_disable_profiling
```c
ai_error_t ai_enable_profiling(ai_device_t device);
ai_error_t ai_disable_profiling(ai_device_t device);
```
Enable/disable hardware profiling.

#### ai_get_profile_data
```c
ai_error_t ai_get_profile_data(ai_device_t device, void* data, 
                                size_t size, size_t* actual_size);
```
Retrieve profiling data.

---

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | `AI_SUCCESS` | Operation completed successfully |
| -1 | `AI_ERROR_INVALID_HANDLE` | Invalid device/buffer/model handle |
| -2 | `AI_ERROR_INVALID_PARAM` | Invalid parameter value |
| -3 | `AI_ERROR_NO_MEMORY` | Out of memory |
| -4 | `AI_ERROR_DEVICE_NOT_FOUND` | Device not found |
| -5 | `AI_ERROR_DRIVER_ERROR` | Driver/kernel error |
| -6 | `AI_ERROR_TIMEOUT` | Operation timed out |
| -7 | `AI_ERROR_BUSY` | Device is busy |
| -8 | `AI_ERROR_NOT_SUPPORTED` | Operation not supported |

---

## Usage Example

```c
#include "libaidrv.h"

int main() {
    ai_device_t device;
    ai_model_t model;
    ai_buffer_t input, output;
    
    // Initialize library
    ai_init();
    
    // Open device
    ai_open_device(0, &device);
    
    // Load model
    ai_load_model(device, "model.onnx", &model);
    
    // Allocate buffers
    ai_alloc_buffer(device, 1024*1024, &input);
    ai_alloc_buffer(device, 1024*1024, &output);
    
    // Prepare input data
    float* data = malloc(1024*1024);
    // ... fill data ...
    ai_copy_to_device(input, data, 1024*1024, 0);
    
    // Run inference
    ai_run_inference(model, &input, 1, &output, 1, NULL);
    
    // Get results
    ai_copy_from_device(output, data, 1024*1024, 0);
    
    // Cleanup
    ai_free_buffer(input);
    ai_free_buffer(output);
    ai_unload_model(model);
    ai_close_device(device);
    ai_shutdown();
    
    free(data);
    return 0;
}
```
