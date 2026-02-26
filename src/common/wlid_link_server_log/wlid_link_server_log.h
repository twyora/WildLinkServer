#pragma once
#ifndef __WLID_LINK_SERVER_LOG_H__
#define __WLID_LINK_SERVER_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include "wlid_link_server_log_levels.h"

#ifndef WLID_LINK_SERVER_IGNORE_USER_CONFIG
#include "wlid_link_server_log_config.h"
#endif // WLID_LINK_SERVER_IGNORE_USER_CONFIG

#ifndef WLID_LINK_SERVER_LOG_PRINT
#define WLID_LINK_SERVER_LOG_PRINT(fmt, ...)
#warning "WLID_LINK_SERVER_LOG_PRINT not defined! Log will not be printed!"
#endif // WLID_LINK_SERVER_LOG_PRINT

#ifndef WLID_LINK_SERVER_CURRENT_LOG_LEVEL
#define WLID_LINK_SERVER_CURRENT_LOG_LEVEL WLID_LINK_SERVER_LOG_LEVEL_NONE
#endif // WLID_LINK_SERVER_CURRENT_LOG_LEVEL

#if WLID_LINK_SERVER_CURRENT_LOG_LEVEL == WLID_LINK_SERVER_LOG_LEVEL_NONE
#define WLID_LINK_SERVER_LOG_PRINT_DEBUG(fmt, ...)
#define WLID_LINK_SERVER_LOG_DEBUG(fmt, ...)
#define WLID_LINK_SERVER_LOG__PRINT_INFO(fmt, ...)
#define WLID_LINK_SERVER_LOG_INFO(fmt, ...)
#define WLID_LINK_SERVER_LOG_PRINT_WARN(fmt, ...)
#define WLID_LINK_SERVER_LOG_WARN(fmt, ...)
#define WLID_LINK_SERVER_LOG_PRINT_ERROR(fmt, ...)
#define WLID_LINK_SERVER_LOG_ERROR(fmt, ...)
#else
#if WLID_LINK_SERVER_CURRENT_LOG_LEVEL <= WLID_LINK_SERVER_LOG_LEVEL_DEBUG
#define WLID_LINK_SERVER_LOG_PRINT_DEBUG(fmt, ...)                                     \
    WLID_LINK_SERVER_LOG_PRINT((fmt), ##__VA_ARGS__)
#define WLID_LINK_SERVER_LOG_DEBUG(fmt, ...)                                           \
    WLID_LINK_SERVER_LOG_PRINT_DEBUG(("[WLS DEBUG] %s:%d: " fmt), __func__, __LINE__,  \
                                     ##__VA_ARGS__)
#else
#define WLID_LINK_SERVER_LOG_PRINT_DEBUG(fmt, ...)
#define WLID_LINK_SERVER_LOG_DEBUG(fmt, ...)
#endif // WLID_LINK_SERVER_CURRENT_LOG_LEVEL <= WLID_LINK_SERVER_LOG_LEVEL_DEBUG

#if WLID_LINK_SERVER_CURRENT_LOG_LEVEL <= WLID_LINK_SERVER_LOG_LEVEL_INFO
#define WLID_LINK_SERVER_LOG__PRINT_INFO(fmt, ...)                                     \
    WLID_LINK_SERVER_LOG_PRINT((fmt), ##__VA_ARGS__)
#define WLID_LINK_SERVER_LOG_INFO(fmt, ...)                                            \
    WLID_LINK_SERVER_LOG__PRINT_INFO(("[WLS INFO] %s:%d: " fmt), __func__, __LINE__,   \
                                     ##__VA_ARGS__)
#else
#define WLID_LINK_SERVER_LOG__PRINT_INFO(fmt, ...)
#define WLID_LINK_SERVER_LOG_INFO(fmt, ...)
#endif // WLID_LINK_SERVER_CURRENT_LOG_LEVEL <= WLID_LINK_SERVER_LOG_LEVEL_INFO

#if WLID_LINK_SERVER_CURRENT_LOG_LEVEL <= WLID_LINK_SERVER_LOG_LEVEL_WARN
#define WLID_LINK_SERVER_LOG_PRINT_WARN(fmt, ...)                                      \
    WLID_LINK_SERVER_LOG_PRINT((fmt), ##__VA_ARGS__)
#define WLID_LINK_SERVER_LOG_WARN(fmt, ...)                                            \
    WLID_LINK_SERVER_LOG_PRINT_WARN(("[WLS WARN] %s:%d: " fmt), __func__, __LINE__,    \
                                    ##__VA_ARGS__)
#else
#define WLID_LINK_SERVER_LOG_PRINT_WARN(fmt, ...)
#define WLID_LINK_SERVER_LOG_WARN(fmt, ...)
#endif // WLID_LINK_SERVER_CURRENT_LOG_LEVEL <= WLID_LINK_SERVER_LOG_LEVEL_WARN

#if WLID_LINK_SERVER_CURRENT_LOG_LEVEL <= WLID_LINK_SERVER_LOG_LEVEL_ERROR
#define WLID_LINK_SERVER_LOG_PRINT_ERROR(fmt, ...)                                     \
    WLID_LINK_SERVER_LOG_PRINT((fmt), ##__VA_ARGS__)
#define WLID_LINK_SERVER_LOG_ERROR(fmt, ...)                                           \
    WLID_LINK_SERVER_LOG_PRINT_ERROR(("[WLS ERROR] %s:%d: " fmt), __func__, __LINE__,  \
                                     ##__VA_ARGS__)
#else
#define WLID_LINK_SERVER_LOG_PRINT_ERROR(fmt, ...)
#define WLID_LINK_SERVER_LOG_ERROR(fmt, ...)
#endif // WLID_LINK_SERVER_CURRENT_LOG_LEVEL <= WLID_LINK_SERVER_LOG_LEVEL_ERROR

#endif // WLID_LINK_SERVER_CURRENT_LOG_LEVEL != WLID_LINK_SERVER_LOG_LEVEL_NONE

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // __WLID_LINK_SERVER_LOG_H__