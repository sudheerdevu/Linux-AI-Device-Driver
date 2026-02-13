# AI Accelerator Driver Documentation

## Overview

This module implements a Linux kernel driver for educational purposes, demonstrating key concepts used in real AI accelerator drivers.

## Installation

```bash
# Build the module
cd driver
make

# Load the module
sudo insmod ai_accel.ko

# Verify loaded
lsmod | grep ai_accel
dmesg | tail -20

# Create device node (if not using udev)
sudo mknod /dev/ai_accel0 c $(cat /sys/class/ai_accel/ai_accel0/dev | cut -d: -f1) 0
```

## IOCTL Interface

| IOCTL | Description |
|-------|-------------|
| `AI_IOC_ALLOC` | Allocate device memory |
| `AI_IOC_FREE` | Free device memory |
| `AI_IOC_SUBMIT` | Submit inference job |
| `AI_IOC_WAIT` | Wait for job completion |
| `AI_IOC_GET_INFO` | Get device information |

## Memory Management

The driver simulates a memory allocator for device memory:

```c
struct ai_accel_alloc_req req = {
    .size = 4096,
    .flags = AI_MEM_DEVICE
};

ioctl(fd, AI_IOC_ALLOC, &req);
// req.handle now contains the memory handle
```

## Job Submission

Jobs are submitted via IOCTL and processed (simulated) by the driver:

```c
struct ai_accel_submit_req submit = {
    .input_handle = input_mem,
    .output_handle = output_mem,
    .op_type = AI_OP_INFERENCE,
    .flags = 0
};

ioctl(fd, AI_IOC_SUBMIT, &submit);
// submit.job_id contains the job ID
```

## Unload

```bash
sudo rmmod ai_accel
```
