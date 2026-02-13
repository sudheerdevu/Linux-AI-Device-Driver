/*
 * Unit tests for AI Accelerator userspace library
 * Compile: gcc -I../include test_libaidrv.c ../userspace/libaidrv.c -o test_libaidrv
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../userspace/libaidrv.h"

#define TEST_PASS() printf("[PASS] %s\n", __func__)
#define TEST_FAIL(msg) do { printf("[FAIL] %s: %s\n", __func__, msg); return 1; } while(0)

/* Test context initialization without device */
int test_context_init_no_device(void)
{
    struct aidrv_context ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    /* Should fail gracefully without device */
    int ret = aidrv_init(&ctx, "/dev/nonexistent_device");
    if (ret == 0) {
        aidrv_cleanup(&ctx);
        TEST_FAIL("Should fail on nonexistent device");
    }
    
    TEST_PASS();
    return 0;
}

/* Test buffer structure */
int test_buffer_struct(void)
{
    struct aidrv_buffer buf;
    memset(&buf, 0, sizeof(buf));
    
    buf.size = 1024;
    buf.data = malloc(buf.size);
    
    if (!buf.data) {
        TEST_FAIL("Memory allocation failed");
    }
    
    /* Fill with test pattern */
    memset(buf.data, 0xAA, buf.size);
    
    /* Verify pattern */
    unsigned char *p = (unsigned char *)buf.data;
    for (size_t i = 0; i < buf.size; i++) {
        if (p[i] != 0xAA) {
            free(buf.data);
            TEST_FAIL("Buffer pattern mismatch");
        }
    }
    
    free(buf.data);
    TEST_PASS();
    return 0;
}

/* Test job structure initialization */
int test_job_struct(void)
{
    struct aidrv_job job;
    memset(&job, 0, sizeof(job));
    
    job.input_count = 2;
    job.output_count = 1;
    job.priority = 0;
    job.flags = 0;
    
    if (job.input_count != 2 || job.output_count != 1) {
        TEST_FAIL("Job structure initialization failed");
    }
    
    TEST_PASS();
    return 0;
}

/* Test error code definitions */
int test_error_codes(void)
{
    /* Verify error codes are defined and distinct */
    if (AIDRV_SUCCESS == AIDRV_ERROR_DEVICE) {
        TEST_FAIL("Error codes not distinct");
    }
    
    TEST_PASS();
    return 0;
}

int main(void)
{
    int failures = 0;
    
    printf("=== AI Driver Library Unit Tests ===\n\n");
    
    failures += test_context_init_no_device();
    failures += test_buffer_struct();
    failures += test_job_struct();
    failures += test_error_codes();
    
    printf("\n=== Results ===\n");
    if (failures == 0) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("%d test(s) failed\n", failures);
        return 1;
    }
}
