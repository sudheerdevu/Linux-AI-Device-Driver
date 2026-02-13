/* SPDX-License-Identifier: GPL-2.0 */
/*
 * AI Accelerator Driver - Internal Header
 */

#ifndef _AI_ACCEL_H_
#define _AI_ACCEL_H_

#include <linux/types.h>

/* Forward declarations */
struct ai_device;
struct ai_buffer;
struct ai_model;

/* Driver configuration */
#define AI_MAX_BUFFERS      1024
#define AI_MAX_MODELS       64
#define AI_MAX_PENDING      256

/* Internal structures and functions would go here */

#endif /* _AI_ACCEL_H_ */
