#include <inttypes.h>
#include <stddef.h>

#include "app_init.h"
#include "common_def.h"
#include "gpio.h"
#include "osal_debug.h"
#include "osal_mutex.h"
#include "osal_task.h"
#include "pinctrl.h"
#include "pinctrl_porting.h"
#include "soc_osal.h"

#include "common/wlid_link_server_log/wlid_link_server_log.h"
#include "sle_server_task.h"
#include "soft_i2c.h"

#if defined(CONFIG_MAX30102_TASK_ENABLED) && (CONFIG_MAX30102_TASK_ENABLED == 1)
#include "max30102_task.h"
#endif // CONFIG_MAX30102_TASK_ENABLED) && (CONFIG_MAX30102_TASK_ENABLED == 1)

#if CONFIG_MAX30205_TASK_ENABLED
#include "max30205_task.h"
#endif // CONFIG_MAX30205_TASK_ENABLED

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#endif // STRINGIFY

osal_task *g_app_init_task_handle;
uint8_t g_server_id = 0;
uint16_t g_service_handle = 0;
uint16_t g_property_handle = 0;

static osal_mutex g_soft_i2c_mutex;
soft_i2c_handle_t g_soft_i2c_handle;
static uint8_t soft_i2c_interface_pin_init(void);
static void soft_i2c_interface_pin_deinit(void);
static uint8_t soft_i2c_interface_scl_write(uint8_t val);
static uint8_t soft_i2c_interface_scl_read(void);
static uint8_t soft_i2c_interface_sda_write(uint8_t val);
static uint8_t soft_i2c_interface_sda_read(void);
static uint8_t soft_i2c_interface_mutex_acquire(void);
static void soft_i2c_interface_mutex_release(void);

static int app_init_task(void *args) {
    unused(args);

    osal_printk("start\r\n");
    osal_msleep(1000);

    // soft i2c init
    if (osal_mutex_init(&g_soft_i2c_mutex) != OSAL_SUCCESS) {
        WLID_LINK_SERVER_LOG_ERROR("soft i2c mutex init failed\r\n");
        goto exit;
    }
    SOFT_I2C_INIT(&g_soft_i2c_handle);
    SOFT_I2C_LINK_PIN_INIT(&g_soft_i2c_handle, soft_i2c_interface_pin_init);
    SOFT_I2C_LINK_PIN_DEINIT(&g_soft_i2c_handle, soft_i2c_interface_pin_deinit);
    SOFT_I2C_LINK_SCL_WRITE(&g_soft_i2c_handle, soft_i2c_interface_scl_write);
    SOFT_I2C_LINK_SCL_READ(&g_soft_i2c_handle, soft_i2c_interface_scl_read);
    SOFT_I2C_LINK_SDA_WRITE(&g_soft_i2c_handle, soft_i2c_interface_sda_write);
    SOFT_I2C_LINK_SDA_READ(&g_soft_i2c_handle, soft_i2c_interface_sda_read);
    SOFT_I2C_LINK_MUTEX_ACQUIRE(&g_soft_i2c_handle, soft_i2c_interface_mutex_acquire);
    SOFT_I2C_LINK_MUTEX_RELEASE(&g_soft_i2c_handle, soft_i2c_interface_mutex_release);
    {
        uint8_t soft_i2c_ret;
        soft_i2c_ret = soft_i2c_init(&g_soft_i2c_handle);
        if (soft_i2c_ret != SOFT_I2C_ERR_NONE) {
            WLID_LINK_SERVER_LOG_ERROR("soft i2c init failed, ret = %#" PRIu8 "\r\n",
                                       soft_i2c_ret);
            goto exit;
        }
    }

    errcode_t ret;

    ret = sle_server_task_entry();
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("sle server task entry exit, ret = %#" PRIx32 "\r\n",
                                   ret);
        goto exit;
    }

#if CONFIG_MAX30102_TASK_ENABLED == 1
    ret = max30102_task_entry();
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("max30102 task entry error, ret = %#" PRIx32 "\r\n",
                                   ret);
        goto exit;
    }
#endif // CONFIG_MAX30102_TASK_ENABLED == 1

#if CONFIG_MAX30205_TASK_ENABLED == 1
    ret = max30205_task_entry();
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("max30205 task entry error, ret = %#" PRIx32 "\r\n",
                                   ret);
        goto exit;
    }
#endif // CONFIG_MAX30205_TASK_ENABLED

exit:
    osal_kthread_destroy(g_app_init_task_handle, 0);
    return 0;
}

static void app_entry(void) {
    osal_kthread_lock();

    g_app_init_task_handle = osal_kthread_create(
        app_init_task, NULL, STRINGIFY(app_init_task), CONFIG_APP_INIT_TASK_STACK_SIZE);
    osal_kthread_set_priority(g_app_init_task_handle, CONFIG_APP_INIT_TASK_PRIORITY);

    osal_kthread_unlock();
}

app_run(app_entry);

static uint8_t soft_i2c_interface_pin_init(void) {
#if defined(CONFIG_SOFT_I2C_SDA_PIN)
#if CONFIG_SOFT_I2C_SDA_PIN == 4
    uapi_pin_set_mode(CONFIG_SOFT_I2C_SDA_PIN, PIN_MODE_2); // gpio
#elif CONFIG_SOFT_I2C_SDA_PIN == 5
    uapi_pin_set_mode(CONFIG_SOFT_I2C_SDA_PIN, PIN_MODE_4); // gpio
#else
    uapi_pin_set_mode(CONFIG_SOFT_I2C_SDA_PIN, HAL_PIO_FUNC_GPIO);
#endif // CONFIG_SOFT_I2C_SDA_PIN==4
#endif // defined(CONFIG_SOFT_I2C_SDA_PIN)
    uapi_pin_set_pull(CONFIG_SOFT_I2C_SDA_PIN, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(CONFIG_SOFT_I2C_SDA_PIN, GPIO_DIRECTION_OUTPUT);

#if defined(CONFIG_SOFT_I2C_SCL_PIN)
#if CONFIG_SOFT_I2C_SCL_PIN == 4
    uapi_pin_set_mode(CONFIG_SOFT_I2C_SCL_PIN, PIN_MODE_2);
#elif CONFIG_SOFT_I2C_SCL_PIN == 5
    uapi_pin_set_mode(CONFIG_SOFT_I2C_SCL_PIN, PIN_MODE_4);
#else
    uapi_pin_set_mode(CONFIG_SOFT_I2C_SCL_PIN, HAL_PIO_FUNC_GPIO);
#endif // CONFIG_SOFT_I2C_SCL_PIN==4
#endif // defined(CONFIG_SOFT_I2C_SCL_PIN)
    uapi_pin_set_pull(CONFIG_SOFT_I2C_SCL_PIN, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(CONFIG_SOFT_I2C_SCL_PIN, GPIO_DIRECTION_OUTPUT);

    return SOFT_I2C_ERR_NONE;
}

static void soft_i2c_interface_pin_deinit(void) {
    return;
}

static uint8_t soft_i2c_interface_scl_write(uint8_t val) {
    errcode_t ret;

    uapi_gpio_set_dir(CONFIG_SOFT_I2C_SCL_PIN, GPIO_DIRECTION_OUTPUT);
    ret = uapi_gpio_set_val(CONFIG_SOFT_I2C_SCL_PIN,
                            val ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
    if (ret != ERRCODE_SUCC) {
        return SOFT_I2C_ERR_FAILED;
    }

    return SOFT_I2C_ERR_NONE;
}

static uint8_t soft_i2c_interface_scl_read(void) {
    uapi_gpio_set_dir(CONFIG_SOFT_I2C_SCL_PIN, GPIO_DIRECTION_INPUT);
    return uapi_gpio_get_val(CONFIG_SOFT_I2C_SCL_PIN) ? 1 : 0;
}

static uint8_t soft_i2c_interface_sda_write(uint8_t val) {
    errcode_t ret;

    uapi_gpio_set_dir(CONFIG_SOFT_I2C_SDA_PIN, GPIO_DIRECTION_OUTPUT);
    ret = uapi_gpio_set_val(CONFIG_SOFT_I2C_SDA_PIN,
                            val ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
    if (ret != ERRCODE_SUCC) {
        return SOFT_I2C_ERR_FAILED;
    }

    return SOFT_I2C_ERR_NONE;
}

static uint8_t soft_i2c_interface_sda_read(void) {
    uapi_gpio_set_dir(CONFIG_SOFT_I2C_SDA_PIN, GPIO_DIRECTION_INPUT);
    return uapi_gpio_get_val(CONFIG_SOFT_I2C_SDA_PIN) ? 1 : 0;
}

static uint8_t soft_i2c_interface_mutex_acquire(void) {
    int ret;

    ret = osal_mutex_lock_timeout(&g_soft_i2c_mutex, 5000);
    if (ret != OSAL_SUCCESS) {
        return SOFT_I2C_ERR_MUTEX_ACQUIRE;
    }

    return SOFT_I2C_ERR_NONE;
}

static void soft_i2c_interface_mutex_release(void) {
    osal_mutex_unlock(&g_soft_i2c_mutex);
}
