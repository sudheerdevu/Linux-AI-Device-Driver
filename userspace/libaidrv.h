/* SPDX-License-Identifier: MIT */
/*
 * AI Accelerator Userspace Library Header
 * 
 * This library provides a high-level C API for interacting with
 * AI accelerator hardware through the kernel driver.
 */

#ifndef LIBAIDRV_H
#define LIBAIDRV_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version Information */
#define LIBAIDRV_VERSION_MAJOR 1
#define LIBAIDRV_VERSION_MINOR 0
#define LIBAIDRV_VERSION_PATCH 0

/* Error Codes */
typedef enum {
    AI_SUCCESS = 0,
    AI_ERROR_INVALID_HANDLE = -1,
    AI_ERROR_INVALID_PARAM = -2,
    AI_ERROR_NO_MEMORY = -3,
    AI_ERROR_DEVICE_NOT_FOUND = -4,
    AI_ERROR_DRIVER_ERROR = -5,
    AI_ERROR_TIMEOUT = -6,
    AI_ERROR_BUSY = -7,
    AI_ERROR_NOT_SUPPORTED = -8,
    AI_ERROR_UNKNOWN = -99
} ai_error_t;

/* Data Types */
typedef enum {
    AI_DTYPE_FLOAT32 = 0,
    AI_DTYPE_FLOAT16 = 1,
    AI_DTYPE_INT8 = 2,
    AI_DTYPE_INT16 = 3,
    AI_DTYPE_INT32 = 4,
    AI_DTYPE_UINT8 = 5,
    AI_DTYPE_BFLOAT16 = 6
} ai_dtype_t;

/* Power Modes */
typedef enum {
    AI_POWER_DEFAULT = 0,
    AI_POWER_LOW = 1,
    AI_POWER_BALANCED = 2,
    AI_POWER_HIGH = 3,
    AI_POWER_MAX = 4
} ai_power_mode_t;

/* Opaque handle types */
typedef struct ai_device_s* ai_device_t;
typedef struct ai_buffer_s* ai_buffer_t;
typedef struct ai_model_s* ai_model_t;
typedef struct ai_job_s* ai_job_t;

/* Device Information Structure */
typedef struct {
    char name[64];
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint64_t device_memory_total;
    uint64_t device_memory_free;
    uint32_t max_batch_size;
    uint32_t max_compute_units;
    uint32_t max_frequency_mhz;
    uint32_t memory_bandwidth_gbps;
} ai_device_info_t;

/* Statistics Structure */
typedef struct {
    uint64_t total_inferences;
    uint64_t total_bytes_processed;
    uint64_t average_latency_ns;
    uint64_t min_latency_ns;
    uint64_t max_latency_ns;
    uint32_t active_jobs;
    uint32_t completed_jobs;
    uint32_t failed_jobs;
    float utilization_percent;
    float power_usage_watts;
    float temperature_celsius;
} ai_stats_t;

/* Tensor Descriptor */
typedef struct {
    ai_dtype_t dtype;
    uint32_t ndim;
    uint32_t shape[8];
    uint32_t strides[8];
    size_t size_bytes;
} ai_tensor_desc_t;

/* Inference Parameters */
typedef struct {
    uint32_t batch_size;
    uint32_t timeout_ms;
    ai_power_mode_t power_mode;
    int async;
    void (*completion_callback)(ai_job_t job, void* user_data);
    void* user_data;
} ai_inference_params_t;

/*
 * Library Initialization
 */

/**
 * Initialize the AI accelerator library
 * Must be called before any other library functions
 * @return AI_SUCCESS on success, error code on failure
 */
ai_error_t ai_init(void);

/**
 * Cleanup and shutdown the library
 * Should be called when done using the library
 */
void ai_shutdown(void);

/**
 * Get library version string
 * @return Version string (e.g., "1.0.0")
 */
const char* ai_get_version(void);

/**
 * Get human-readable error description
 * @param error Error code
 * @return Error description string
 */
const char* ai_get_error_string(ai_error_t error);

/*
 * Device Management
 */

/**
 * Get number of available AI accelerator devices
 * @param count Pointer to store device count
 * @return AI_SUCCESS on success
 */
ai_error_t ai_get_device_count(int* count);

/**
 * Open an AI accelerator device
 * @param device_index Device index (0 to count-1)
 * @param device Pointer to store device handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_open_device(int device_index, ai_device_t* device);

/**
 * Close an AI accelerator device
 * @param device Device handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_close_device(ai_device_t device);

/**
 * Get device information
 * @param device Device handle
 * @param info Pointer to store device info
 * @return AI_SUCCESS on success
 */
ai_error_t ai_get_device_info(ai_device_t device, ai_device_info_t* info);

/**
 * Get device statistics
 * @param device Device handle
 * @param stats Pointer to store statistics
 * @return AI_SUCCESS on success
 */
ai_error_t ai_get_device_stats(ai_device_t device, ai_stats_t* stats);

/**
 * Set device power mode
 * @param device Device handle
 * @param mode Power mode
 * @return AI_SUCCESS on success
 */
ai_error_t ai_set_power_mode(ai_device_t device, ai_power_mode_t mode);

/*
 * Memory Management
 */

/**
 * Allocate device memory buffer
 * @param device Device handle
 * @param size Size in bytes
 * @param buffer Pointer to store buffer handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_alloc_buffer(ai_device_t device, size_t size, ai_buffer_t* buffer);

/**
 * Free device memory buffer
 * @param buffer Buffer handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_free_buffer(ai_buffer_t buffer);

/**
 * Copy data from host to device
 * @param buffer Device buffer
 * @param src Host source pointer
 * @param size Size in bytes
 * @param offset Offset in device buffer
 * @return AI_SUCCESS on success
 */
ai_error_t ai_copy_to_device(ai_buffer_t buffer, const void* src, 
                              size_t size, size_t offset);

/**
 * Copy data from device to host
 * @param buffer Device buffer
 * @param dst Host destination pointer
 * @param size Size in bytes
 * @param offset Offset in device buffer
 * @return AI_SUCCESS on success
 */
ai_error_t ai_copy_from_device(ai_buffer_t buffer, void* dst,
                                size_t size, size_t offset);

/**
 * Map buffer for zero-copy access (if supported)
 * @param buffer Device buffer
 * @param ptr Pointer to store mapped address
 * @return AI_SUCCESS on success
 */
ai_error_t ai_map_buffer(ai_buffer_t buffer, void** ptr);

/**
 * Unmap previously mapped buffer
 * @param buffer Device buffer
 * @return AI_SUCCESS on success
 */
ai_error_t ai_unmap_buffer(ai_buffer_t buffer);

/*
 * Model Management
 */

/**
 * Load a model from file
 * @param device Device handle
 * @param path Path to model file (ONNX, etc.)
 * @param model Pointer to store model handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_load_model(ai_device_t device, const char* path, ai_model_t* model);

/**
 * Load a model from memory
 * @param device Device handle
 * @param data Model data buffer
 * @param size Buffer size
 * @param model Pointer to store model handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_load_model_from_memory(ai_device_t device, const void* data,
                                      size_t size, ai_model_t* model);

/**
 * Unload a model
 * @param model Model handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_unload_model(ai_model_t model);

/**
 * Get model input tensor descriptor
 * @param model Model handle
 * @param index Input index
 * @param desc Pointer to store descriptor
 * @return AI_SUCCESS on success
 */
ai_error_t ai_get_model_input(ai_model_t model, int index, ai_tensor_desc_t* desc);

/**
 * Get model output tensor descriptor
 * @param model Model handle
 * @param index Output index
 * @param desc Pointer to store descriptor
 * @return AI_SUCCESS on success
 */
ai_error_t ai_get_model_output(ai_model_t model, int index, ai_tensor_desc_t* desc);

/*
 * Inference
 */

/**
 * Run synchronous inference
 * @param model Model handle
 * @param inputs Array of input buffers
 * @param num_inputs Number of inputs
 * @param outputs Array of output buffers
 * @param num_outputs Number of outputs
 * @param params Inference parameters (NULL for defaults)
 * @return AI_SUCCESS on success
 */
ai_error_t ai_run_inference(ai_model_t model,
                            ai_buffer_t* inputs, int num_inputs,
                            ai_buffer_t* outputs, int num_outputs,
                            const ai_inference_params_t* params);

/**
 * Submit asynchronous inference job
 * @param model Model handle
 * @param inputs Array of input buffers
 * @param num_inputs Number of inputs
 * @param outputs Array of output buffers
 * @param num_outputs Number of outputs
 * @param params Inference parameters
 * @param job Pointer to store job handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_submit_inference(ai_model_t model,
                                ai_buffer_t* inputs, int num_inputs,
                                ai_buffer_t* outputs, int num_outputs,
                                const ai_inference_params_t* params,
                                ai_job_t* job);

/**
 * Wait for job completion
 * @param job Job handle
 * @param timeout_ms Timeout in milliseconds (0 for infinite)
 * @return AI_SUCCESS on completion, AI_ERROR_TIMEOUT on timeout
 */
ai_error_t ai_wait_job(ai_job_t job, uint32_t timeout_ms);

/**
 * Check if job is complete
 * @param job Job handle
 * @param complete Pointer to store completion status
 * @return AI_SUCCESS on success
 */
ai_error_t ai_check_job(ai_job_t job, int* complete);

/**
 * Get job result/status
 * @param job Job handle
 * @param latency_ns Pointer to store latency (optional, can be NULL)
 * @return AI_SUCCESS if job succeeded, error code if job failed
 */
ai_error_t ai_get_job_result(ai_job_t job, uint64_t* latency_ns);

/**
 * Release job handle
 * @param job Job handle
 */
void ai_release_job(ai_job_t job);

/*
 * Profiling
 */

/**
 * Enable profiling
 * @param device Device handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_enable_profiling(ai_device_t device);

/**
 * Disable profiling
 * @param device Device handle
 * @return AI_SUCCESS on success
 */
ai_error_t ai_disable_profiling(ai_device_t device);

/**
 * Get profiling data for last inference
 * @param device Device handle
 * @param data Buffer to store profiling data
 * @param size Buffer size
 * @param actual_size Actual data size written
 * @return AI_SUCCESS on success
 */
ai_error_t ai_get_profile_data(ai_device_t device, void* data, 
                                size_t size, size_t* actual_size);

#ifdef __cplusplus
}
#endif

#endif /* LIBAIDRV_H */
