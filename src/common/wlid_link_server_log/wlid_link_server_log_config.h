#pragma once
#ifndef __WLID_LINK_SERVER_LOG_CONFIG_H__
#define __WLID_LINK_SERVER_LOG_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "wlid_link_server_log_levels.h"

#define WLID_LINK_SERVER_CURRENT_LOG_LEVEL WLID_LINK_SERVER_LOG_LEVEL_DEBUG
#define WLID_LINK_SERVER_LOG_PRINT(fmt, ...) osal_printk((fmt), ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __WLID_LINK_SERVER_LOG_CONFIG_H__