#pragma once
#ifndef __MAX30102_TASK_H__
#define __MAX30102_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include <stdbool.h>
#include <stdint.h>

#include "errcode.h"
#include "osal_task.h"

extern osal_task *g_max30102_task_handle;

void max30102_read_heart_rate_and_oxygen_saturation(uint8_t *out_spo2_val,
                                                    bool *out_spo2_valid,
                                                    uint8_t *out_heart_rate,
                                                    bool *out_heart_rate_valid);
errcode_t max30102_task_entry(void);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__MAX30102_TASK_H__