#ifndef __MAX30205_TASK_H__
#pragma once
#define __MAX30205_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "errcode.h"
#include "osal_task.h"

extern osal_task *g_max30205_task_handle;

float max30205_task_read_temperature(void);
errcode_t max30205_task_entry(void);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__MAX30205_TASK_H__