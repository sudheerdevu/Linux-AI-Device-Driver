/**
 * Userspace test program for AI accelerator driver
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "../include/uapi/ai_accel.h"

#define DEVICE_PATH "/dev/ai_accel0"

void test_memory_allocation(int fd) {
    struct ai_accel_alloc_req req = {
        .size = 4096,
        .flags = AI_MEM_DEVICE
    };
    
    printf("Testing memory allocation...\n");
    if (ioctl(fd, AI_IOC_ALLOC, &req) < 0) {
        perror("AI_IOC_ALLOC failed");
        return;
    }
    printf("  Allocated handle: %u\n", req.handle);
    
    // Free the memory
    struct ai_accel_free_req free_req = { .handle = req.handle };
    if (ioctl(fd, AI_IOC_FREE, &free_req) < 0) {
        perror("AI_IOC_FREE failed");
    }
    printf("  Memory freed successfully\n");
}

void test_job_submission(int fd) {
    printf("Testing job submission...\n");
    
    // Allocate input buffer
    struct ai_accel_alloc_req input_req = { .size = 1024, .flags = AI_MEM_DEVICE };
    struct ai_accel_alloc_req output_req = { .size = 1024, .flags = AI_MEM_DEVICE };
    
    if (ioctl(fd, AI_IOC_ALLOC, &input_req) < 0) {
        perror("Input allocation failed");
        return;
    }
    if (ioctl(fd, AI_IOC_ALLOC, &output_req) < 0) {
        perror("Output allocation failed");
        return;
    }
    
    // Submit job
    struct ai_accel_submit_req submit = {
        .input_handle = input_req.handle,
        .output_handle = output_req.handle,
        .op_type = AI_OP_INFERENCE,
        .flags = 0
    };
    
    if (ioctl(fd, AI_IOC_SUBMIT, &submit) < 0) {
        perror("Job submission failed");
    } else {
        printf("  Job submitted, ID: %u\n", submit.job_id);
    }
    
    // Cleanup
    struct ai_accel_free_req free1 = { .handle = input_req.handle };
    struct ai_accel_free_req free2 = { .handle = output_req.handle };
    ioctl(fd, AI_IOC_FREE, &free1);
    ioctl(fd, AI_IOC_FREE, &free2);
}

void test_device_info(int fd) {
    struct ai_accel_info info;
    
    printf("Testing device info...\n");
    if (ioctl(fd, AI_IOC_GET_INFO, &info) < 0) {
        perror("AI_IOC_GET_INFO failed");
        return;
    }
    
    printf("  Device: %s\n", info.name);
    printf("  Compute units: %u\n", info.num_compute_units);
    printf("  Memory: %lu MB\n", info.memory_size / (1024 * 1024));
    printf("  Firmware: v%u.%u\n", info.fw_major, info.fw_minor);
}

int main(int argc, char *argv[]) {
    int fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        printf("Make sure the ai_accel module is loaded:\n");
        printf("  sudo insmod driver/ai_accel.ko\n");
        return 1;
    }
    
    printf("AI Accelerator Driver Test\n");
    printf("===========================\n\n");
    
    test_device_info(fd);
    printf("\n");
    
    test_memory_allocation(fd);
    printf("\n");
    
    test_job_submission(fd);
    
    close(fd);
    printf("\nAll tests completed.\n");
    return 0;
}
