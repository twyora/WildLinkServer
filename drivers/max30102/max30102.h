#ifndef __MAX30102_H__
#define __MAX30102_H__

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    MAX30102_INTERRUPT_STATUS_A_FIFO_FULL = 0x80, /**< fifo almost full flag */
    MAX30102_INTERRUPT_STATUS_PPG_RDY = 0x40,     /**< new fifo data ready */
    MAX30102_INTERRUPT_STATUS_ALC_OVF =
        0x20, /**< ambient light cancellation overflow */
    MAX30102_INTERRUPT_STATUS_PWR_RDY = 0x01, /**< power ready flag */
    MAX30102_INTERRUPT_STATUS_DIE_TEMP_RDY =
        0x02, /**< internal temperature ready flag */
} max30102_interrupt_status_t;

typedef struct max30102_handle_s {
    uint8_t (*i2c_init)(void);
    uint8_t (*i2c_deinit)(void);
    uint8_t (*i2c_read)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
    uint8_t (*i2c_write)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
    void (*irq_callback)(struct max30102_handle_s *handle,
                         max30102_interrupt_status_t status);
    void (*delay_ms)(uint32_t ms);
    void (*debug_print)(const char *const fmt, ...);

    bool _initialized;
} max30102_handle_t;

#define MAX30102_I2C_ADDR_7BIT 0x57

#define MAX30102_REG_INTERRUPT_STATUS_1 0x00 /**< interrupt status 1 register */
#define MAX30102_REG_INTERRUPT_STATUS_2 0x01 /**< interrupt status 2 register */
#define MAX30102_REG_INTERRUPT_ENABLE_1 0x02 /**< interrupt enable 1 register */
#define MAX30102_REG_INTERRUPT_ENABLE_2 0x03 /**< interrupt enable 2 register */
#define MAX30102_REG_FIFO_WRITE_POINTER 0x04 /**< fifo write pointer register */
#define MAX30102_REG_OVERFLOW_COUNTER 0x05   /**< overflow counter register */
#define MAX30102_REG_FIFO_READ_POINTER 0x06  /**< fifo read pointer register */
#define MAX30102_REG_FIFO_DATA_REGISTER 0x07 /**< fifo data register */
#define MAX30102_REG_FIFO_CONFIG 0x08        /**< fifo config register */
#define MAX30102_REG_MODE_CONFIG 0x09        /**< mode config register */
#define MAX30102_REG_SPO2_CONFIG 0x0A        /**< spo2 config register */
#define MAX30102_REG_LED_PULSE_1 0x0C        /**< led pulse amplitude 1 register */
#define MAX30102_REG_LED_PULSE_2 0x0D        /**< led pulse amplitude 2 register */
#define MAX30102_REG_MULTI_LED_MODE_CONTROL_1                                          \
    0x11 /**< multi led mode control 1 register */
#define MAX30102_REG_MULTI_LED_MODE_CONTROL_2                                          \
    0x12                                    /**< multi led mode control 2 register */
#define MAX30102_REG_DIE_TEMP_INTEGER 0x1F  /**< die temperature integer register */
#define MAX30102_REG_DIE_TEMP_FRACTION 0x20 /**< die temperature fraction register */
#define MAX30102_REG_DIE_TEMP_CONFIG 0x21   /**< die temperature config register */
#define MAX30102_REG_REVISION_ID 0xFE       /**< revision id register */
#define MAX30102_REG_PART_ID 0xFF           /**< part id register */

#define MAX30102_LINK_INIT(pHANDLE) memset(pHANDLE, 0, sizeof(*(pHANDLE)))
#define MAX30102_LINK_I2C_INIT(pHANDLE, FUNC) ((pHANDLE)->i2c_init = (FUNC))
#define MAX30102_LINK_I2C_DEINIT(pHANDLE, FUNC) ((pHANDLE)->i2c_deinit = (FUNC))
#define MAX30102_LINK_I2C_READ(pHANDLE, FUNC) ((pHANDLE)->i2c_read = (FUNC))
#define MAX30102_LINK_I2C_WRITE(pHANDLE, FUNC) ((pHANDLE)->i2c_write = (FUNC))
#define MAX30102_LINK_IRQ_CALLBACK(pHANDLE, FUNC) ((pHANDLE)->irq_callback = (FUNC))
#define MAX30102_LINK_DELAY_MS(pHANDLE, FUNC) ((pHANDLE)->delay_ms = (FUNC))
#define MAX30102_LINK_DEBUG_PRINT(pHANDLE, FUNC) ((pHANDLE)->debug_print = (FUNC))

#define MAX30102_ERR_NONE 0
#define MAX30102_ERR_FAILED 1
#define MAX30102_ERR_HANDLE_IS_NULL 2
#define MAX30102_ERR_MEMBER_FUNC_IS_NULL 3
#define MAX30102_ERR_NOT_INITIALIZED 4
#define MAX30102_ERR_INVALID_PARAMS 5

uint8_t max30102_init(max30102_handle_t *handle);
void max30102_deinit(max30102_handle_t *handle);

uint8_t max30102_read_reg(max30102_handle_t *handle, uint8_t reg_addr, uint8_t *buf);
uint8_t max30102_write_reg(max30102_handle_t *handle, uint8_t reg_addr, uint8_t data);
uint8_t max30102_read_fifo(max30102_handle_t *handle, uint32_t *ir_led_data,
                           uint32_t *red_led_data);

uint8_t max30102_reset(max30102_handle_t *handle);

uint8_t max30102_irq_handler(max30102_handle_t *handle);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif //__MAX30102_H__