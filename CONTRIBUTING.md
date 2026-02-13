# Contributing to Linux AI Device Driver

Thank you for your interest in contributing!

## Development Setup

### Prerequisites
- Linux kernel headers (5.4+)
- GCC with kernel module support
- Make

### Building
```bash
cd driver
make
```

## Code Style

Follow the Linux kernel coding style:
- Use tabs for indentation (8 spaces)
- Line length: 80 characters max
- Use kernel naming conventions

```bash
# Check style
./scripts/checkpatch.pl --file driver/*.c
```

## Testing

Before submitting:
1. Build the module without warnings
2. Load/unload the module successfully
3. Run userspace tests

```bash
# Load module
sudo insmod driver/ai_accel.ko

# Run tests  
cd userspace && make test

# Unload
sudo rmmod ai_accel
```

## Pull Request Process

1. Fork the repository
2. Create a feature branch
3. Make changes with clear commit messages
4. Test thoroughly
5. Submit PR with description

## Reporting Issues

Include:
- Kernel version (`uname -r`)
- dmesg output
- Steps to reproduce
- Hardware information

## License

Contributions are licensed under MIT License.
