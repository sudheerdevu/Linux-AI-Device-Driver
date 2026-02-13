#!/bin/bash
# Test suite for Linux AI Device Driver userspace library

set -e

echo "=== Linux AI Device Driver Test Suite ==="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

PASSED=0
FAILED=0

pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((PASSED++))
}

fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((FAILED++))
}

# Test 1: Check library header exists
echo "Test 1: Checking library header..."
if [ -f "../userspace/libaidrv.h" ]; then
    pass "libaidrv.h exists"
else
    fail "libaidrv.h not found"
fi

# Test 2: Check library source exists
echo "Test 2: Checking library source..."
if [ -f "../userspace/libaidrv.c" ]; then
    pass "libaidrv.c exists"
else
    fail "libaidrv.c not found"
fi

# Test 3: Check driver source exists
echo "Test 3: Checking driver sources..."
if [ -f "../driver/ai_accel.c" ] && [ -f "../driver/ai_dma.c" ] && [ -f "../driver/ai_ioctl.c" ]; then
    pass "All driver sources exist"
else
    fail "Missing driver sources"
fi

# Test 4: Check Makefile exists
echo "Test 4: Checking build system..."
if [ -f "../driver/Makefile" ]; then
    pass "Driver Makefile exists"
else
    fail "Driver Makefile not found"
fi

# Test 5: Check header definitions
echo "Test 5: Checking UAPI header definitions..."
if grep -q "AI_ACCEL_IOC_SUBMIT" ../include/uapi/ai_accel.h 2>/dev/null; then
    pass "IOCTL definitions found"
else
    fail "IOCTL definitions missing"
fi

# Test 6: Check Kconfig
echo "Test 6: Checking kernel config..."
if [ -f "../driver/Kconfig" ]; then
    pass "Kconfig exists"
else
    fail "Kconfig not found"
fi

# Test 7: Syntax check C files (if gcc available)
echo "Test 7: Syntax checking C files..."
if command -v gcc &> /dev/null; then
    if gcc -fsyntax-only -I../include ../userspace/libaidrv.c 2>/dev/null; then
        pass "libaidrv.c syntax OK"
    else
        fail "libaidrv.c has syntax errors"
    fi
else
    echo "  [SKIP] gcc not available"
fi

echo ""
echo "=== Test Results ==="
echo -e "Passed: ${GREEN}${PASSED}${NC}"
echo -e "Failed: ${RED}${FAILED}${NC}"

if [ $FAILED -gt 0 ]; then
    exit 1
fi
