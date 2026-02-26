#include <inttypes.h>
#include <stdarg.h>

#include "common_def.h"
#include "soc_osal.h"

#include "../../common/wlid_link_server_log/wlid_link_server_log.h"
#include "max30205.h"
#include "soft_i2c.h"

#include "max30205_task.h"

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#endif // STRINGIFY

osal_task *g_max30205_task_handle;
extern soft_i2c_handle_t g_soft_i2c_handle;

static float temperature = 0;

static uint8_t max30205_interface_iic_init(void);
static uint8_t max30205_interface_iic_deinit(void);
static uint8_t max30205_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf,
                                           uint16_t len);
static uint8_t max30205_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf,
                                            uint16_t len);
static void max30205_interface_delay_ms(uint32_t ms);
static void max30205_interface_debug_print(const char *const fmt, ...);

static int max30205_task(void *args);

float max30205_task_read_temperature(void) {
    return temperature;
}

errcode_t max30205_task_entry(void) {
    g_max30205_task_handle = osal_kthread_create(
        max30205_task, NULL, STRINGIFY(max30205_task), CONFIG_MAX30205_TASK_STACK_SIZE);
    if (g_max30205_task_handle == NULL) {
        WLID_LINK_SERVER_LOG_ERROR("failed to create max30205 task\r\n");
        goto failed;
    }

    if (osal_kthread_set_priority(g_max30205_task_handle, CONFIG_MAX30205_TASK_PRIORITY)
        != OSAL_SUCCESS)
    {
        WLID_LINK_SERVER_LOG_ERROR("failed to set max30205 task priority\r\n");
        goto failed;
    }

    return ERRCODE_SUCC;
failed:
    return ERRCODE_FAIL;
}

static uint8_t max30205_interface_iic_init(void) {
    return 0;
}

static uint8_t max30205_interface_iic_deinit(void) {
    return 0;
}

static uint8_t max30205_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf,
                                           uint16_t len) {
    return soft_i2c_mem_read(&g_soft_i2c_handle, addr >> 1, SOFT_I2C_ADDR_7BIT, &reg,
                             sizeof(reg), buf, len);
}

static uint8_t max30205_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf,
                                            uint16_t len) {
    return soft_i2c_mem_write(&g_soft_i2c_handle, addr >> 1, SOFT_I2C_ADDR_7BIT, &reg,
                              sizeof(reg), buf, len);
}

static void max30205_interface_delay_ms(uint32_t ms) {
    osal_msleep(ms);
}

static void max30205_interface_debug_print(const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    osal_vprintk(fmt, args);
    va_end(args);
}

static int max30205_task(void *args) {
    unused(args);
    osal_msleep(1000);

    max30205_handle_t max30205_handle;

    DRIVER_MAX30205_LINK_INIT(&max30205_handle, max30205_handle_t);
    DRIVER_MAX30205_LINK_IIC_INIT(&max30205_handle, max30205_interface_iic_init);
    DRIVER_MAX30205_LINK_IIC_DEINIT(&max30205_handle, max30205_interface_iic_deinit);
    DRIVER_MAX30205_LINK_IIC_READ(&max30205_handle, max30205_interface_iic_read);
    DRIVER_MAX30205_LINK_IIC_WRITE(&max30205_handle, max30205_interface_iic_write);
    DRIVER_MAX30205_LINK_DELAY_MS(&max30205_handle, max30205_interface_delay_ms);
    DRIVER_MAX30205_LINK_DEBUG_PRINT(&max30205_handle, max30205_interface_debug_print);

    uint8_t ret;
    ret = max30205_set_addr_pin(&max30205_handle, MAX30205_ADDRESS_0);
    WLID_LINK_SERVER_LOG_INFO("ret = %" PRIu8 "\r\n", ret);
    ret = max30205_init(&max30205_handle);
    WLID_LINK_SERVER_LOG_INFO("ret = %" PRIu8 "\r\n", ret);
    ret = max30205_set_data_format(&max30205_handle, MAX30205_DATA_FORMAT_NORMAL);
    WLID_LINK_SERVER_LOG_INFO("ret = %" PRIu8 "\r\n", ret);

    int16_t temp_raw = 0;
    for (;;) {
        osal_msleep(3000);
        max30205_single_read(&max30205_handle, &temp_raw, &temperature);
        WLID_LINK_SERVER_LOG_INFO("temperature raw = %#04" PRIx16
                                  ", temperature * 100 = %" PRId32 "\r\n",
                                  temp_raw, (int)((temperature + 64) * 100));
        max30205_single_read(&max30205_handle, &temp_raw, &temperature);
    }

    return 0;
}