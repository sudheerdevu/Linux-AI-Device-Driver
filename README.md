# Linux AI Device Driver

> Educational kernel module demonstrating driver fundamentals for AI accelerator hardware.

## 🎯 Purpose

This project provides a **educational reference** for understanding:
- Linux kernel module development
- Hardware abstraction for AI accelerators
- DMA buffer management for inference
- IOCTL interfaces for userspace communication
- Memory mapping for zero-copy data transfer

## ⚠️ Disclaimer

This is a **learning resource**, not production code. The driver demonstrates concepts applicable to:
- NPU (Neural Processing Unit) drivers
- GPU compute interfaces
- Custom AI accelerator development

## 📁 Project Structure

```
Linux-AI-Device-Driver/
├── README.md              # This file
├── driver/
│   ├── ai_accel.c         # Main kernel module
│   ├── ai_accel.h         # Driver header
│   ├── ai_dma.c           # DMA operations
│   ├── ai_ioctl.c         # IOCTL handlers
│   ├── Makefile           # Kernel build
│   └── Kconfig            # Build configuration
├── userspace/
│   ├── libaidrv.c         # Userspace library
│   ├── libaidrv.h         # Library header
│   └── test_driver.c      # Test application
├── include/
│   └── uapi/
│       └── ai_accel.h     # User/kernel interface
└── docs/
    ├── architecture.md
    └── api-reference.md
```

## 🔧 Key Concepts Demonstrated

### 1. Character Device Interface
```c
static const struct file_operations ai_fops = {
    .owner          = THIS_MODULE,
    .open           = ai_open,
    .release        = ai_release,
    .read           = ai_read,
    .write          = ai_write,
    .unlocked_ioctl = ai_ioctl,
    .mmap           = ai_mmap,
};
```

### 2. DMA Buffer Management
```c
// Allocate coherent DMA buffer
dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);

// Map user pages for DMA
get_user_pages_fast(addr, nr_pages, FOLL_WRITE, pages);
dma_map_sg(dev, sg, nents, DMA_TO_DEVICE);
```

### 3. IOCTL Interface
```c
#define AI_IOC_MAGIC 'A'
#define AI_IOC_SUBMIT_INFERENCE  _IOW(AI_IOC_MAGIC, 1, struct inference_request)
#define AI_IOC_WAIT_COMPLETION   _IOR(AI_IOC_MAGIC, 2, struct inference_result)
#define AI_IOC_GET_CAPS          _IOR(AI_IOC_MAGIC, 3, struct device_caps)
```

### 4. Memory-Mapped Registers
```c
// Map device registers
void __iomem *regs = ioremap(pci_resource_start(pdev, 0), 
                             pci_resource_len(pdev, 0));

// Access registers
writel(value, regs + REG_OFFSET);
value = readl(regs + REG_OFFSET);
```

## 📚 Learning Resources

1. [Linux Device Drivers, 3rd Edition](https://lwn.net/Kernel/LDD3/)
2. [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/)
3. [DMA API Guide](https://www.kernel.org/doc/html/latest/core-api/dma-api.html)

## 🏗️ Building

```bash
# Build kernel module
cd driver
make

# Build userspace test
cd ../userspace
make

# Load module (simulation mode)
sudo insmod ai_accel.ko simulate=1

# Run test
./test_driver
```

## 📖 Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    User Space                                │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐      │
│  │ Application │    │  Runtime    │    │   Library   │      │
│  └──────┬──────┘    └──────┬──────┘    └──────┬──────┘      │
│         │                  │                  │              │
│         └──────────────────┴──────────────────┘              │
│                            │                                 │
│         ioctl() / mmap() / read() / write()                  │
├─────────────────────────────┼───────────────────────────────┤
│                    Kernel Space                              │
│                            │                                 │
│         ┌──────────────────▼──────────────────┐              │
│         │      AI Accelerator Driver          │              │
│         │   (ai_accel.ko)                     │              │
│         └──────────────────┬──────────────────┘              │
│                            │                                 │
│         ┌──────────────────▼──────────────────┐              │
│         │       DMA / Memory Manager          │              │
│         └──────────────────┬──────────────────┘              │
│                            │                                 │
├─────────────────────────────┼───────────────────────────────┤
│                    Hardware                                  │
│         ┌──────────────────▼──────────────────┐              │
│         │      AI Accelerator Hardware        │              │
│         │   (NPU / GPU / Custom ASIC)         │              │
│         └─────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────┘
```

## License

GPL-2.0 (kernel module requirement)
