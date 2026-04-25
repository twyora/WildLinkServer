#include <inttypes.h>

#include "max30102.h"

uint8_t max30102_init(max30102_handle_t *handle) {
    uint8_t ret;

    if (handle == NULL) {
        ret = MAX30102_ERR_HANDLE_IS_NULL;
        goto error;
    }
    else if (handle->i2c_init == NULL) {
        ret = MAX30102_ERR_MEMBER_FUNC_IS_NULL;
        goto error;
    }
    else if (handle->i2c_deinit == NULL) {
        ret = MAX30102_ERR_MEMBER_FUNC_IS_NULL;
        goto error;
    }
    else if (handle->i2c_read == NULL) {
        ret = MAX30102_ERR_MEMBER_FUNC_IS_NULL;
        goto error;
    }
    else if (handle->i2c_write == NULL) {
        ret = MAX30102_ERR_MEMBER_FUNC_IS_NULL;
        goto error;
    }
    else if (handle->irq_callback == NULL) {
        ret = MAX30102_ERR_MEMBER_FUNC_IS_NULL;
        goto error;
    }
    else if (handle->delay_ms == NULL) {
        ret = MAX30102_ERR_MEMBER_FUNC_IS_NULL;
        goto error;
    }
    else if (handle->debug_print == NULL) {
        ret = MAX30102_ERR_MEMBER_FUNC_IS_NULL;
        goto error;
    }

    ret = handle->i2c_init();
    if (ret != MAX30102_ERR_NONE) {
        goto error;
    }

    handle->_initialized = true;

    return MAX30102_ERR_NONE;
error:
    return ret;
}

void max30102_deinit(max30102_handle_t *handle) {
    if (handle == NULL) {
        return;
    }
    else if (!(handle->_initialized)) {
        return;
    }

    handle->i2c_deinit();
    handle->_initialized = false;
}

uint8_t max30102_read_reg(max30102_handle_t *handle, uint8_t reg_addr, uint8_t *buf) {
    if (handle == NULL) {
        return MAX30102_ERR_HANDLE_IS_NULL;
    }
    else if (!(handle->_initialized)) {
        return MAX30102_ERR_NOT_INITIALIZED;
    }
    else if (buf == NULL) {
        return MAX30102_ERR_INVALID_PARAMS;
    }

    uint8_t ret;
    ret = handle->i2c_read(MAX30102_I2C_ADDR_7BIT, reg_addr, buf, sizeof(*buf));

    return ret;
}

uint8_t max30102_write_reg(max30102_handle_t *handle, uint8_t reg_addr, uint8_t data) {
    if (handle == NULL) {
        return MAX30102_ERR_HANDLE_IS_NULL;
    }
    else if (!(handle->_initialized)) {
        return MAX30102_ERR_NOT_INITIALIZED;
    }

    uint8_t ret;
    ret = handle->i2c_write(MAX30102_I2C_ADDR_7BIT, reg_addr, &data, sizeof(data));

    return ret;
}

uint8_t max30102_read_fifo(max30102_handle_t *handle, uint32_t *ir_led_data,
                           uint32_t *red_led_data) {
    if (handle == NULL) {
        return MAX30102_ERR_HANDLE_IS_NULL;
    }
    else if (!(handle->_initialized)) {
        return MAX30102_ERR_NOT_INITIALIZED;
    }
    else if (ir_led_data == NULL || red_led_data == NULL) {
        return MAX30102_ERR_INVALID_PARAMS;
    }

    uint8_t ret;
    uint8_t fifo_buf[6] = {0};

    ret = handle->i2c_read(MAX30102_I2C_ADDR_7BIT, MAX30102_REG_FIFO_DATA_REGISTER,
                           fifo_buf, sizeof(fifo_buf));
    if (ret != MAX30102_ERR_NONE) {
        goto error;
    }

    *red_led_data = (fifo_buf[0] << 16) | (fifo_buf[1] << 8) | (fifo_buf[2]);
    *ir_led_data = (fifo_buf[3] << 16) | (fifo_buf[4] << 8) | (fifo_buf[5]);

    *red_led_data &= 0x03FFFF; // Mask MSB [23:18]
    *ir_led_data &= 0x03FFFF;  // Mask MSB [23:18]

    return MAX30102_ERR_NONE;
error:
    return ret;
}

uint8_t max30102_reset(max30102_handle_t *handle) {
    if (handle == NULL) {
        return MAX30102_ERR_HANDLE_IS_NULL;
    }
    else if (!(handle->_initialized)) {
        return MAX30102_ERR_NOT_INITIALIZED;
    }

    uint8_t ret;
    uint8_t temp;

    temp = 0x80;
    ret = handle->i2c_write(MAX30102_I2C_ADDR_7BIT, MAX30102_REG_MODE_CONFIG, &temp,
                            sizeof(temp));
    if (ret != MAX30102_ERR_NONE) {
        goto error;
    }

    temp = 0x40;
    ret = handle->i2c_write(MAX30102_I2C_ADDR_7BIT, MAX30102_REG_MODE_CONFIG, &temp,
                            sizeof(temp));
    if (ret != MAX30102_ERR_NONE) {
        goto error;
    }

    return MAX30102_ERR_NONE;
error:
    return ret;
}

uint8_t max30102_irq_handler(max30102_handle_t *handle) {
    if (handle == NULL) {
        return MAX30102_ERR_HANDLE_IS_NULL;
    }
    else if (!(handle->_initialized)) {
        return MAX30102_ERR_NOT_INITIALIZED;
    }

    uint8_t ret;
    uint8_t int_status_x;

    // int status register 1
    ret = handle->i2c_read(MAX30102_I2C_ADDR_7BIT, MAX30102_REG_INTERRUPT_STATUS_1,
                           &int_status_x, sizeof(int_status_x));
    if (ret != MAX30102_ERR_NONE) {
        handle->debug_print("failed to read int status register 1,ret = %" PRIu8 "\r\n",
                            ret);
        goto error;
    }

    if (int_status_x & MAX30102_INTERRUPT_STATUS_A_FIFO_FULL) {
        handle->irq_callback(handle, MAX30102_INTERRUPT_STATUS_A_FIFO_FULL);
    }
    if (int_status_x & MAX30102_INTERRUPT_STATUS_PPG_RDY) {
        handle->irq_callback(handle, MAX30102_INTERRUPT_STATUS_PPG_RDY);
    }
    if (int_status_x & MAX30102_INTERRUPT_STATUS_ALC_OVF) {
        handle->irq_callback(handle, MAX30102_INTERRUPT_STATUS_ALC_OVF);
    }

    // int status register 2
    ret = handle->i2c_read(MAX30102_I2C_ADDR_7BIT, MAX30102_REG_INTERRUPT_STATUS_2,
                           &int_status_x, sizeof(int_status_x));
    if (ret != MAX30102_ERR_NONE) {
        handle->debug_print("failed to read int status register 2,ret = %" PRIu8 "\r\n",
                            ret);
        goto error;
    }

    if (int_status_x & MAX30102_INTERRUPT_STATUS_DIE_TEMP_RDY) {
        handle->irq_callback(handle, MAX30102_INTERRUPT_STATUS_DIE_TEMP_RDY);
    }

    return MAX30102_ERR_NONE;
error:
    return ret;
}
