/* SPDX-License-Identifier: MIT */
/*
 * AI Accelerator Userspace Library Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "libaidrv.h"
#include "../include/uapi/ai_accel.h"

#define AI_DEVICE_PATH "/dev/ai_accel"
#define MAX_DEVICES 16

/* Internal structures */
struct ai_device_s {
    int fd;
    int index;
    ai_device_info_t info;
    pthread_mutex_t lock;
    int profiling_enabled;
};

struct ai_buffer_s {
    ai_device_t device;
    uint64_t handle;
    size_t size;
    void* mapped_ptr;
    int is_mapped;
};

struct ai_model_s {
    ai_device_t device;
    void* model_data;
    size_t model_size;
    int num_inputs;
    int num_outputs;
    ai_tensor_desc_t* inputs;
    ai_tensor_desc_t* outputs;
};

struct ai_job_s {
    ai_device_t device;
    uint64_t job_id;
    int complete;
    ai_error_t result;
    uint64_t latency_ns;
    void (*callback)(ai_job_t, void*);
    void* user_data;
};

/* Global state */
static int g_initialized = 0;
static pthread_mutex_t g_init_lock = PTHREAD_MUTEX_INITIALIZER;

/* Error strings */
static const char* error_strings[] = {
    [0] = "Success",
    [1] = "Invalid handle",
    [2] = "Invalid parameter",
    [3] = "Out of memory",
    [4] = "Device not found",
    [5] = "Driver error",
    [6] = "Operation timed out",
    [7] = "Device busy",
    [8] = "Operation not supported",
};

/*
 * Library Initialization
 */

ai_error_t ai_init(void)
{
    pthread_mutex_lock(&g_init_lock);
    
    if (g_initialized) {
        pthread_mutex_unlock(&g_init_lock);
        return AI_SUCCESS;
    }
    
    /* Check if driver is loaded */
    if (access(AI_DEVICE_PATH, F_OK) != 0) {
        pthread_mutex_unlock(&g_init_lock);
        return AI_ERROR_DEVICE_NOT_FOUND;
    }
    
    g_initialized = 1;
    pthread_mutex_unlock(&g_init_lock);
    
    return AI_SUCCESS;
}

void ai_shutdown(void)
{
    pthread_mutex_lock(&g_init_lock);
    g_initialized = 0;
    pthread_mutex_unlock(&g_init_lock);
}

const char* ai_get_version(void)
{
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             LIBAIDRV_VERSION_MAJOR,
             LIBAIDRV_VERSION_MINOR,
             LIBAIDRV_VERSION_PATCH);
    return version;
}

const char* ai_get_error_string(ai_error_t error)
{
    int idx = -error;
    if (idx >= 0 && idx < (int)(sizeof(error_strings)/sizeof(error_strings[0])))
        return error_strings[idx];
    return "Unknown error";
}

/*
 * Device Management
 */

ai_error_t ai_get_device_count(int* count)
{
    if (!g_initialized)
        return AI_ERROR_INVALID_HANDLE;
    if (!count)
        return AI_ERROR_INVALID_PARAM;
    
    /* Check how many devices exist */
    *count = 0;
    char path[64];
    
    for (int i = 0; i < MAX_DEVICES; i++) {
        snprintf(path, sizeof(path), "%s%d", AI_DEVICE_PATH, i);
        if (access(path, F_OK) == 0) {
            (*count)++;
        } else if (i == 0) {
            /* Check base device without number */
            if (access(AI_DEVICE_PATH, F_OK) == 0) {
                *count = 1;
            }
            break;
        } else {
            break;
        }
    }
    
    /* At minimum, assume 1 device if base path exists */
    if (*count == 0 && access(AI_DEVICE_PATH, F_OK) == 0)
        *count = 1;
    
    return AI_SUCCESS;
}

ai_error_t ai_open_device(int device_index, ai_device_t* device)
{
    if (!g_initialized)
        return AI_ERROR_INVALID_HANDLE;
    if (!device)
        return AI_ERROR_INVALID_PARAM;
    
    char path[64];
    int count;
    ai_get_device_count(&count);
    
    if (device_index >= count)
        return AI_ERROR_DEVICE_NOT_FOUND;
    
    if (count == 1) {
        snprintf(path, sizeof(path), "%s", AI_DEVICE_PATH);
    } else {
        snprintf(path, sizeof(path), "%s%d", AI_DEVICE_PATH, device_index);
    }
    
    struct ai_device_s* dev = calloc(1, sizeof(struct ai_device_s));
    if (!dev)
        return AI_ERROR_NO_MEMORY;
    
    dev->fd = open(path, O_RDWR);
    if (dev->fd < 0) {
        free(dev);
        return AI_ERROR_DRIVER_ERROR;
    }
    
    dev->index = device_index;
    pthread_mutex_init(&dev->lock, NULL);
    
    /* Get device info */
    struct ai_accel_info kinfo;
    if (ioctl(dev->fd, AI_ACCEL_IOC_GET_INFO, &kinfo) == 0) {
        strncpy(dev->info.name, kinfo.name, sizeof(dev->info.name));
        dev->info.version_major = kinfo.version_major;
        dev->info.version_minor = kinfo.version_minor;
        dev->info.version_patch = kinfo.version_patch;
        dev->info.device_memory_total = kinfo.device_memory_size;
        dev->info.device_memory_free = kinfo.device_memory_free;
        dev->info.max_batch_size = kinfo.max_batch_size;
        dev->info.max_compute_units = kinfo.max_compute_units;
        dev->info.max_frequency_mhz = kinfo.max_frequency_mhz;
        dev->info.memory_bandwidth_gbps = kinfo.memory_bandwidth_gbps;
    }
    
    *device = dev;
    return AI_SUCCESS;
}

ai_error_t ai_close_device(ai_device_t device)
{
    if (!device)
        return AI_ERROR_INVALID_HANDLE;
    
    pthread_mutex_destroy(&device->lock);
    close(device->fd);
    free(device);
    
    return AI_SUCCESS;
}

ai_error_t ai_get_device_info(ai_device_t device, ai_device_info_t* info)
{
    if (!device || !info)
        return AI_ERROR_INVALID_PARAM;
    
    memcpy(info, &device->info, sizeof(*info));
    return AI_SUCCESS;
}

ai_error_t ai_get_device_stats(ai_device_t device, ai_stats_t* stats)
{
    if (!device || !stats)
        return AI_ERROR_INVALID_PARAM;
    
    struct ai_accel_stats kstats;
    
    pthread_mutex_lock(&device->lock);
    int ret = ioctl(device->fd, AI_ACCEL_IOC_GET_STATS, &kstats);
    pthread_mutex_unlock(&device->lock);
    
    if (ret < 0)
        return AI_ERROR_DRIVER_ERROR;
    
    stats->total_inferences = kstats.total_inferences;
    stats->total_bytes_processed = kstats.total_bytes_in + kstats.total_bytes_out;
    stats->average_latency_ns = kstats.average_latency_ns;
    stats->active_jobs = kstats.active_jobs;
    stats->completed_jobs = kstats.completed_jobs;
    stats->failed_jobs = kstats.failed_jobs;
    
    return AI_SUCCESS;
}

ai_error_t ai_set_power_mode(ai_device_t device, ai_power_mode_t mode)
{
    if (!device)
        return AI_ERROR_INVALID_HANDLE;
    
    pthread_mutex_lock(&device->lock);
    int ret = ioctl(device->fd, AI_ACCEL_IOC_SET_POWER, (unsigned long)mode);
    pthread_mutex_unlock(&device->lock);
    
    return (ret < 0) ? AI_ERROR_DRIVER_ERROR : AI_SUCCESS;
}

/*
 * Memory Management
 */

ai_error_t ai_alloc_buffer(ai_device_t device, size_t size, ai_buffer_t* buffer)
{
    if (!device || !buffer || size == 0)
        return AI_ERROR_INVALID_PARAM;
    
    struct ai_buffer_s* buf = calloc(1, sizeof(struct ai_buffer_s));
    if (!buf)
        return AI_ERROR_NO_MEMORY;
    
    struct ai_accel_mem_alloc alloc = { .size = size };
    
    pthread_mutex_lock(&device->lock);
    int ret = ioctl(device->fd, AI_ACCEL_IOC_ALLOC_MEM, &alloc);
    pthread_mutex_unlock(&device->lock);
    
    if (ret < 0) {
        free(buf);
        return AI_ERROR_NO_MEMORY;
    }
    
    buf->device = device;
    buf->handle = alloc.handle;
    buf->size = alloc.size;
    buf->mapped_ptr = NULL;
    buf->is_mapped = 0;
    
    *buffer = buf;
    return AI_SUCCESS;
}

ai_error_t ai_free_buffer(ai_buffer_t buffer)
{
    if (!buffer)
        return AI_ERROR_INVALID_HANDLE;
    
    if (buffer->is_mapped) {
        ai_unmap_buffer(buffer);
    }
    
    struct ai_accel_mem_free mfree = {
        .handle = buffer->handle,
        .size = buffer->size
    };
    
    pthread_mutex_lock(&buffer->device->lock);
    ioctl(buffer->device->fd, AI_ACCEL_IOC_FREE_MEM, &mfree);
    pthread_mutex_unlock(&buffer->device->lock);
    
    free(buffer);
    return AI_SUCCESS;
}

ai_error_t ai_copy_to_device(ai_buffer_t buffer, const void* src, 
                              size_t size, size_t offset)
{
    if (!buffer || !src)
        return AI_ERROR_INVALID_PARAM;
    if (offset + size > buffer->size)
        return AI_ERROR_INVALID_PARAM;
    
    /* For this implementation, use mapped memory or ioctl */
    /* Simplified: use mmap and memcpy */
    void* ptr;
    ai_error_t err = ai_map_buffer(buffer, &ptr);
    if (err != AI_SUCCESS)
        return err;
    
    memcpy((char*)ptr + offset, src, size);
    
    ai_unmap_buffer(buffer);
    return AI_SUCCESS;
}

ai_error_t ai_copy_from_device(ai_buffer_t buffer, void* dst,
                                size_t size, size_t offset)
{
    if (!buffer || !dst)
        return AI_ERROR_INVALID_PARAM;
    if (offset + size > buffer->size)
        return AI_ERROR_INVALID_PARAM;
    
    void* ptr;
    ai_error_t err = ai_map_buffer(buffer, &ptr);
    if (err != AI_SUCCESS)
        return err;
    
    memcpy(dst, (char*)ptr + offset, size);
    
    ai_unmap_buffer(buffer);
    return AI_SUCCESS;
}

ai_error_t ai_map_buffer(ai_buffer_t buffer, void** ptr)
{
    if (!buffer || !ptr)
        return AI_ERROR_INVALID_PARAM;
    
    if (buffer->is_mapped) {
        *ptr = buffer->mapped_ptr;
        return AI_SUCCESS;
    }
    
    buffer->mapped_ptr = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE,
                               MAP_SHARED, buffer->device->fd, buffer->handle);
    
    if (buffer->mapped_ptr == MAP_FAILED) {
        buffer->mapped_ptr = NULL;
        return AI_ERROR_DRIVER_ERROR;
    }
    
    buffer->is_mapped = 1;
    *ptr = buffer->mapped_ptr;
    return AI_SUCCESS;
}

ai_error_t ai_unmap_buffer(ai_buffer_t buffer)
{
    if (!buffer)
        return AI_ERROR_INVALID_HANDLE;
    
    if (!buffer->is_mapped)
        return AI_SUCCESS;
    
    munmap(buffer->mapped_ptr, buffer->size);
    buffer->mapped_ptr = NULL;
    buffer->is_mapped = 0;
    
    return AI_SUCCESS;
}

/*
 * Model Management (Simplified Implementation)
 */

ai_error_t ai_load_model(ai_device_t device, const char* path, ai_model_t* model)
{
    if (!device || !path || !model)
        return AI_ERROR_INVALID_PARAM;
    
    FILE* f = fopen(path, "rb");
    if (!f)
        return AI_ERROR_DEVICE_NOT_FOUND;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    void* data = malloc(size);
    if (!data) {
        fclose(f);
        return AI_ERROR_NO_MEMORY;
    }
    
    if (fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return AI_ERROR_DRIVER_ERROR;
    }
    fclose(f);
    
    ai_error_t err = ai_load_model_from_memory(device, data, size, model);
    free(data);
    
    return err;
}

ai_error_t ai_load_model_from_memory(ai_device_t device, const void* data,
                                      size_t size, ai_model_t* model)
{
    if (!device || !data || !model || size == 0)
        return AI_ERROR_INVALID_PARAM;
    
    struct ai_model_s* m = calloc(1, sizeof(struct ai_model_s));
    if (!m)
        return AI_ERROR_NO_MEMORY;
    
    m->model_data = malloc(size);
    if (!m->model_data) {
        free(m);
        return AI_ERROR_NO_MEMORY;
    }
    
    memcpy(m->model_data, data, size);
    m->model_size = size;
    m->device = device;
    
    /* Default: assume 1 input, 1 output (would parse model format in real impl) */
    m->num_inputs = 1;
    m->num_outputs = 1;
    m->inputs = calloc(1, sizeof(ai_tensor_desc_t));
    m->outputs = calloc(1, sizeof(ai_tensor_desc_t));
    
    *model = m;
    return AI_SUCCESS;
}

ai_error_t ai_unload_model(ai_model_t model)
{
    if (!model)
        return AI_ERROR_INVALID_HANDLE;
    
    free(model->model_data);
    free(model->inputs);
    free(model->outputs);
    free(model);
    
    return AI_SUCCESS;
}

ai_error_t ai_get_model_input(ai_model_t model, int index, ai_tensor_desc_t* desc)
{
    if (!model || !desc)
        return AI_ERROR_INVALID_PARAM;
    if (index >= model->num_inputs)
        return AI_ERROR_INVALID_PARAM;
    
    memcpy(desc, &model->inputs[index], sizeof(*desc));
    return AI_SUCCESS;
}

ai_error_t ai_get_model_output(ai_model_t model, int index, ai_tensor_desc_t* desc)
{
    if (!model || !desc)
        return AI_ERROR_INVALID_PARAM;
    if (index >= model->num_outputs)
        return AI_ERROR_INVALID_PARAM;
    
    memcpy(desc, &model->outputs[index], sizeof(*desc));
    return AI_SUCCESS;
}

/*
 * Inference
 */

ai_error_t ai_run_inference(ai_model_t model,
                            ai_buffer_t* inputs, int num_inputs,
                            ai_buffer_t* outputs, int num_outputs,
                            const ai_inference_params_t* params)
{
    if (!model || !inputs || !outputs)
        return AI_ERROR_INVALID_PARAM;
    if (num_inputs < 1 || num_outputs < 1)
        return AI_ERROR_INVALID_PARAM;
    
    struct ai_accel_inference inf = {
        .input_data = (uint64_t)inputs[0]->handle,
        .input_size = inputs[0]->size,
        .output_data = (uint64_t)outputs[0]->handle,
        .output_size = outputs[0]->size,
        .batch_size = params ? params->batch_size : 1,
    };
    
    pthread_mutex_lock(&model->device->lock);
    int ret = ioctl(model->device->fd, AI_ACCEL_IOC_SUBMIT_INFERENCE, &inf);
    pthread_mutex_unlock(&model->device->lock);
    
    if (ret < 0)
        return AI_ERROR_DRIVER_ERROR;
    
    return (inf.status == AI_STATUS_SUCCESS) ? AI_SUCCESS : AI_ERROR_DRIVER_ERROR;
}

ai_error_t ai_submit_inference(ai_model_t model,
                                ai_buffer_t* inputs, int num_inputs,
                                ai_buffer_t* outputs, int num_outputs,
                                const ai_inference_params_t* params,
                                ai_job_t* job)
{
    if (!job)
        return AI_ERROR_INVALID_PARAM;
    
    struct ai_job_s* j = calloc(1, sizeof(struct ai_job_s));
    if (!j)
        return AI_ERROR_NO_MEMORY;
    
    j->device = model->device;
    j->callback = params ? params->completion_callback : NULL;
    j->user_data = params ? params->user_data : NULL;
    
    /* For sync implementation, run immediately */
    ai_error_t err = ai_run_inference(model, inputs, num_inputs, 
                                       outputs, num_outputs, params);
    
    j->complete = 1;
    j->result = err;
    
    *job = j;
    return AI_SUCCESS;
}

ai_error_t ai_wait_job(ai_job_t job, uint32_t timeout_ms)
{
    if (!job)
        return AI_ERROR_INVALID_HANDLE;
    
    /* In async implementation, would wait on completion */
    /* For sync, job is already complete */
    (void)timeout_ms;
    
    return job->complete ? AI_SUCCESS : AI_ERROR_TIMEOUT;
}

ai_error_t ai_check_job(ai_job_t job, int* complete)
{
    if (!job || !complete)
        return AI_ERROR_INVALID_PARAM;
    
    *complete = job->complete;
    return AI_SUCCESS;
}

ai_error_t ai_get_job_result(ai_job_t job, uint64_t* latency_ns)
{
    if (!job)
        return AI_ERROR_INVALID_HANDLE;
    
    if (latency_ns)
        *latency_ns = job->latency_ns;
    
    return job->result;
}

void ai_release_job(ai_job_t job)
{
    if (job)
        free(job);
}

/*
 * Profiling
 */

ai_error_t ai_enable_profiling(ai_device_t device)
{
    if (!device)
        return AI_ERROR_INVALID_HANDLE;
    
    device->profiling_enabled = 1;
    return AI_SUCCESS;
}

ai_error_t ai_disable_profiling(ai_device_t device)
{
    if (!device)
        return AI_ERROR_INVALID_HANDLE;
    
    device->profiling_enabled = 0;
    return AI_SUCCESS;
}

ai_error_t ai_get_profile_data(ai_device_t device, void* data, 
                                size_t size, size_t* actual_size)
{
    if (!device || !data || !actual_size)
        return AI_ERROR_INVALID_PARAM;
    
    if (!device->profiling_enabled)
        return AI_ERROR_NOT_SUPPORTED;
    
    /* Would retrieve from driver in real implementation */
    *actual_size = 0;
    return AI_SUCCESS;
}
