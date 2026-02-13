# AI Accelerator Driver Architecture

## Overview

The AI Accelerator driver provides a Linux kernel interface for hardware AI acceleration devices. It follows a modular architecture with clear separation of concerns.

## Component Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           USER SPACE                                         │
│                                                                             │
│  ┌───────────────────────┐    ┌───────────────────────┐                    │
│  │   User Application    │    │   libaidrv Library    │                    │
│  │   (ML Framework)      │◄──►│   (High-Level API)    │                    │
│  └───────────────────────┘    └───────────────────────┘                    │
│                                         │                                   │
│                                         │ ioctl(), mmap(), read(), write()  │
│                                         ▼                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                           KERNEL SPACE                                       │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                     AI Accelerator Driver                            │   │
│  │                                                                      │   │
│  │  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐        │   │
│  │  │   ai_accel.c   │  │   ai_ioctl.c   │  │   ai_dma.c     │        │   │
│  │  │   (Core)       │  │   (IOCTL)      │  │   (DMA Ops)    │        │   │
│  │  └───────┬────────┘  └───────┬────────┘  └───────┬────────┘        │   │
│  │          │                   │                   │                  │   │
│  │          ▼                   ▼                   ▼                  │   │
│  │  ┌───────────────────────────────────────────────────────────────┐ │   │
│  │  │                   Internal Data Structures                     │ │   │
│  │  │   • Device Context    • Job Queue    • Memory Allocator       │ │   │
│  │  └───────────────────────────────────────────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                         │                                   │
│                                         ▼                                   │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │              Linux Kernel Subsystems                                 │   │
│  │   • PCI/Platform    • DMA Engine    • Memory Mgmt    • Scheduler   │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                         │                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                           HARDWARE                                           │
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    AI Accelerator Hardware                           │   │
│  │                                                                      │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │   │
│  │  │   Control    │  │   Compute    │  │   Device     │              │   │
│  │  │   Registers  │  │   Engines    │  │   Memory     │              │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘              │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Module Descriptions

### ai_accel.c - Core Driver Module

**Responsibilities:**
- Device registration and probe/remove handlers
- Character device operations (open, close, read, write)
- Module initialization and cleanup
- Power management (suspend/resume)
- Sysfs interface for device attributes

**Key Data Structures:**
```c
struct ai_accel_device {
    struct device *dev;          /* Parent device */
    struct cdev cdev;            /* Character device */
    void __iomem *regs;          /* Memory-mapped registers */
    size_t mem_size;             /* Device memory size */
    size_t mem_used;             /* Memory in use */
    struct list_head job_queue;  /* Pending jobs */
    spinlock_t lock;             /* Device lock */
};
```

### ai_ioctl.c - IOCTL Handler Module

**Responsibilities:**
- Process all ioctl commands from userspace
- Validate user inputs
- Coordinate with other modules
- Copy data between user and kernel space

**Supported IOCTLs:**

| IOCTL | Description |
|-------|-------------|
| `AI_ACCEL_IOC_GET_INFO` | Get device information |
| `AI_ACCEL_IOC_ALLOC_MEM` | Allocate device memory |
| `AI_ACCEL_IOC_FREE_MEM` | Free device memory |
| `AI_ACCEL_IOC_SUBMIT_INFERENCE` | Submit inference job |
| `AI_ACCEL_IOC_GET_STATS` | Get device statistics |
| `AI_ACCEL_IOC_SET_POWER` | Set power mode |
| `AI_ACCEL_IOC_WAIT` | Wait for job completion |

### ai_dma.c - DMA Operations Module

**Responsibilities:**
- Initialize and manage DMA channels
- Allocate DMA-capable memory buffers
- Map user buffers for DMA transfers
- Execute synchronous and asynchronous transfers
- Handle scatter-gather operations

**Key Functions:**
```c
int ai_dma_init(struct device *dev);
void ai_dma_exit(void);
struct ai_dma_buffer *ai_dma_alloc_buffer(struct device *dev, size_t size, 
                                           enum dma_data_direction dir);
void ai_dma_free_buffer(struct device *dev, struct ai_dma_buffer *buf);
int ai_dma_transfer_sync(struct device *dev, dma_addr_t dst, dma_addr_t src,
                          size_t size, unsigned int timeout_ms);
```

## Data Flow

### Inference Request Flow

```
1. Application calls ai_run_inference()
           │
           ▼
2. libaidrv prepares ai_accel_inference struct
           │
           ▼
3. ioctl(AI_ACCEL_IOC_SUBMIT_INFERENCE)
           │
           ▼
4. ai_ioctl_submit_inference() validates & copies input
           │
           ▼
5. DMA transfers input data to device memory
           │
           ▼
6. Hardware executes inference
           │
           ▼
7. DMA transfers output data back to host
           │
           ▼
8. ioctl returns with results
           │
           ▼
9. Application receives output
```

### Memory Management Flow

```
Allocation:                          Deallocation:
                                     
ai_alloc_buffer()                    ai_free_buffer()
      │                                    │
      ▼                                    ▼
ALLOC_MEM ioctl                      FREE_MEM ioctl
      │                                    │
      ▼                                    ▼
dma_alloc_coherent()                 dma_free_coherent()
      │                                    │
      ▼                                    ▼
Return handle to user                Release resources
```

## Synchronization

### Lock Hierarchy

1. **Global `channel_lock`** - Protects DMA channel allocation
2. **Per-device `device->lock`** - Protects device state
3. **Per-file `ctx->lock`** - Protects per-process context

### Concurrency Model

- Multiple processes can open the device simultaneously
- Job submission is serialized per-device
- DMA channels are pooled and shared
- Memory allocations are tracked per-process

## Error Handling

| Error Code | Description | Recovery |
|------------|-------------|----------|
| `-ENOMEM` | Out of memory | Free resources and retry |
| `-EBUSY` | Device busy | Wait and retry |
| `-ETIMEDOUT` | Operation timeout | Check device status |
| `-EFAULT` | Bad user address | Fix application bug |
| `-EINVAL` | Invalid parameter | Check input values |

## Configuration

### Kconfig Options

- `AI_ACCELERATOR` - Enable driver support
- `AI_ACCEL_DMA` - Enable DMA support
- `AI_ACCEL_DEBUG` - Enable debug logging
- `AI_ACCEL_MAX_DEVICES` - Max devices (default: 4)
- `AI_ACCEL_POWER_MANAGEMENT` - Enable power management

### Module Parameters

```bash
# Set max inference queue depth
modprobe ai_accel queue_depth=128

# Enable verbose logging
modprobe ai_accel debug=1
```

## Performance Considerations

### DMA Optimization
- Use scatter-gather for large, non-contiguous transfers
- Pin user memory to avoid page faults during DMA
- Use double-buffering for streaming workloads

### Memory Allocation
- Prefer large contiguous allocations
- Use memory pools for frequent small allocations
- Align allocations to cache line boundaries

### Synchronization
- Use spinlocks for short critical sections
- Use mutex for longer operations
- Avoid holding locks during DMA transfers
