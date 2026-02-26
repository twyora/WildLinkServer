#include <inttypes.h>

#include "common_def.h"
#include "gpio.h"
#include "osal_event.h"
#include "pinctrl.h"
#include "pinctrl_porting.h"
#include "securec.h"
#include "soc_osal.h"

#include "max30102.h"
#include "max30102_algorithm.h"
#include "soft_i2c.h"
#include "wlid_link_server_log.h"

#include "max30102_task.h"

#define MAX30102_EVENT_FIFO_A_FULL 0x01u        // FIFO Almost Full
#define MAX30102_EVENT_SAMPLE_BUFFER_FULL 0x02u // Sample Buffer Full
#define MAX30102_EVENT_INT_TRIGGERED 0x04u      // Interrupt Triggered
#define MAX30102_EVENT_ALL                                                             \
    (MAX30102_EVENT_FIFO_A_FULL | MAX30102_EVENT_SAMPLE_BUFFER_FULL                    \
     | MAX30102_EVENT_INT_TRIGGERED)

#define MAX30102_SAMPLE_DATA_BUFFER_SIZE 500
#define MAX30102_SLIDING_WINDOW_KEEP_COUNT 400

#if MAX30102_SAMPLE_DATA_BUFFER_SIZE < MAX30102_SLIDING_WINDOW_KEEP_COUNT
#error                                                                                 \
    "MAX30102_SAMPLE_DATA_BUFFER_SIZE must be greater than MAX30102_SLIDING_WINDOW_KEEP_COUNT"
#endif // MAX30102_SAMPLE_DATA_BUFFER_SIZE < MAX30102_SLIDING_WINDOW_KEEP_COUNT

#define MAX30102_SAMPLE_DATA_PREPROCESS_ENABLED 1

#if MAX30102_SAMPLE_DATA_PREPROCESS_ENABLED
#define MAX30102_SAMPLE_DATA_BUFFER_SIZE 500
#define MAX30102_SLIDING_WINDOW_KEEP_COUNT 400
#define MAX30102_FILTER_ORDER 5
#define MAX30102_LOWPASS_CUTOFF_HZ 5
#define MAX30102_SAMPLING_RATE 100
#define MAX30102_HEART_RATE_MIN 40
#define MAX30102_HEART_RATE_MAX 200
#define MAX30102_SPO2_MIN 80
#define MAX30102_SPO2_MAX 100
#define MAX30102_OUTPUT_SMOOTH_COUNT 5
#define MAX30102_SIGNAL_QUALITY_THRESHOLD 50
#endif // MAX30102_SAMPLE_DATA_PREPROCESS_ENABLED

#if MAX30102_SAMPLE_DATA_PREPROCESS_ENABLED

/**
 * @brief 五阶平滑滤波
 */
static void signal_preprocess_5th_order_smooth(int32_t *signal, uint32_t length) {
    if (length < 5) {
        return;
    }

    int32_t sum;
    // 从后向前处理，避免数据覆盖
    for (uint32_t i = length - 3; i >= 2; i--) {
        sum = signal[i - 2] + signal[i - 1] + signal[i] + signal[i + 1] + signal[i + 2];
        signal[i] = sum / 5;
    }
}

/**
 * @brief 低通滤波
 */
static void signal_preprocess_lowpass_filter(int32_t *signal, uint32_t length,
                                             int32_t sampling_rate, int32_t cutoff_hz) {
    if (length < 2) {
        return;
    }

    // 计算滤波系数
    int32_t alpha = (2 * 314 * cutoff_hz * 1024) / (sampling_rate * 1000);
    if (alpha > 1024)
        alpha = 1024;
    if (alpha < 1)
        alpha = 1;

    int32_t y_prev = signal[0];

    for (uint32_t i = 1; i < length; i++) {
        int32_t y = alpha * signal[i] + (1024 - alpha) * y_prev;
        signal[i] = y / 1024;
        y_prev = signal[i];
    }
}

static void signal_preprocess_remove_baseline(int32_t *signal, uint32_t length) {
    if (length < 2) {
        return;
    }

    int32_t hp_alpha = 32;

    int32_t x_prev = signal[0];
    signal[0] = 0;
    int32_t y_prev = 0;

    for (uint32_t i = 0; i < length; i++) {
        int32_t diff = signal[i] - x_prev;
        y_prev = (hp_alpha * diff + (1024 - hp_alpha) * y_prev) / 1024;
        signal[i] = y_prev;
        x_prev = signal[i];
    }
}

/**
 * @brief 信号质量评估（不修改数据）
 */
static int32_t signal_quality_assessment(int32_t *ir_signal, int32_t *red_signal,
                                         uint32_t length) {
    uint32_t i;
    int32_t ir_mean = 0;
    int32_t ir_variance = 0;
    int32_t peak_count = 0;
    int32_t quality_score = 0;
    int32_t threshold;

    // 计算均值
    for (i = 0; i < length; i++) {
        ir_mean += ir_signal[i];
    }
    ir_mean = ir_mean / length;

    // 计算方差
    for (i = 0; i < length; i++) {
        int32_t diff = ir_signal[i] - ir_mean;
        ir_variance += (diff * diff) / length;
    }

    // 检测波峰
    threshold = ir_mean + (ir_variance / 100);
    for (i = 1; i < length - 1; i++) {
        if (ir_signal[i] > ir_signal[i - 1] && ir_signal[i] > ir_signal[i + 1]
            && ir_signal[i] > threshold)
        {
            peak_count++;
        }
    }

    // 评分
    if (peak_count >= 3 && peak_count <= 17) {
        quality_score += 40;
    }
    else if (peak_count >= 2 && peak_count <= 20) {
        quality_score += 20;
    }

    if (ir_variance > 1000 && ir_variance < 1000000) {
        quality_score += 30;
    }
    else if (ir_variance > 500 && ir_variance < 2000000) {
        quality_score += 15;
    }

    quality_score += 30;
    if (quality_score > 100)
        quality_score = 100;

    return quality_score;
}

#endif // MAX30102_SAMPLE_DATA_PREPROCESS_ENABLED

osal_task *g_max30102_task_handle;
static osal_event g_max30102_event;

extern soft_i2c_handle_t g_soft_i2c_handle;

static max30102_handle_t g_max30102_handle;

typedef struct {
    uint32_t ir_buf[MAX30102_SAMPLE_DATA_BUFFER_SIZE];
    uint32_t red_buf[MAX30102_SAMPLE_DATA_BUFFER_SIZE];
} max30102_sensor_buffers;

static max30102_sensor_buffers g_max30102_buffers[2];

static int32_t g_spo2_val;   // SPO2 value
static int8_t g_spo2_valid;  // indicator to show if the SP02 calculation is valid
static int32_t g_heart_rate; // heart rate value
static int8_t
    g_heart_rate_valid; // indicator to show if the heart rate calculation is valid

static uint8_t g_valid_hr_read_buf[MAX30102_OUTPUT_SMOOTH_COUNT] = {0};
static uint8_t g_hr_buf_wr_idx = 0;
static uint8_t g_hr_valid_count = 0;

static void max30102_int_triggered(pin_t pin, uintptr_t param);

static uint8_t max30102_interface_i2c_init(void);
static uint8_t max30102_interface_i2c_deinit(void);
static uint8_t max30102_interface_i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf,
                                           uint16_t len);
static uint8_t max30102_interface_i2c_write(uint8_t addr, uint8_t reg, uint8_t *buf,
                                            uint16_t len);
static void max30102_interface_irq_callback(max30102_handle_t *handle,
                                            max30102_interrupt_status_t status);
static void max30102_interface_delay_ms(uint32_t ms);
static void max30102_interface_debug_print(const char *const fmt, ...);

static int max30102_task(void *args);

void max30102_read_heart_rate_and_oxygen_saturation(uint8_t *out_spo2_val,
                                                    bool *out_spo2_valid,
                                                    uint8_t *out_heart_rate,
                                                    bool *out_heart_rate_valid) {
    if (out_spo2_val != NULL) {
        *out_spo2_val = g_spo2_val;
    }
    if (out_spo2_valid != NULL) {
        *out_spo2_valid = g_spo2_valid;
    }
    if (out_heart_rate != NULL) {
        uint16_t hr_sum = 0;
        for (uint8_t i = 0; i < g_hr_valid_count; i++) {
            hr_sum += g_valid_hr_read_buf[i];
        }
        *out_heart_rate = (uint8_t)(hr_sum / g_hr_valid_count);
    }
    if (out_heart_rate_valid != NULL) {
        *out_heart_rate_valid = g_heart_rate_valid;
    }
}

errcode_t max30102_task_entry(void) {
    int ret;

    ret = osal_event_init(&g_max30102_event);
    if (ret != OSAL_SUCCESS) {
        WLID_LINK_SERVER_LOG_ERROR("failed to init max30102 event\r\n");
        goto _error_return;
    }

    g_max30102_task_handle = osal_kthread_create(max30102_task, NULL, "max30102_task",
                                                 CONFIG_MAX30102_TASK_STACK_SIZE);
    if (g_max30102_task_handle == NULL) {
        WLID_LINK_SERVER_LOG_ERROR("failed to create max30102 task\r\n");
        goto _event_clean;
    }

    if (osal_kthread_set_priority(g_max30102_task_handle, CONFIG_MAX30102_TASK_PRIORITY)
        != OSAL_SUCCESS)
    {
        WLID_LINK_SERVER_LOG_ERROR("failed to set max30102 task priority\r\n");
        goto _kthread_clean;
    }

    return ERRCODE_SUCC;
_kthread_clean:
    osal_kthread_destroy(g_max30102_task_handle, 0);
_event_clean:
    osal_event_destroy(&g_max30102_event);
_error_return:
    return ERRCODE_FAIL;
}

static int max30102_task(void *args) {
    unused(args);
    WLID_LINK_SERVER_LOG_INFO("start\r\n");
    osal_msleep(1000);

    uint8_t ret;

#if CONFIG_MAX30102_INT_PIN == 4
    uapi_pin_set_mode(CONFIG_MAX30102_INT_PIN, PIN_MODE_2);
#elif CONFIG_MAX30102_INT_PIN == 5
    uapi_pin_set_mode(CONFIG_MAX30102_INT_PIN, PIN_MODE_4);
#endif // CONFIG_MAX30102_INT_PIN==4
    uapi_pin_set_mode(CONFIG_MAX30102_INT_PIN, HAL_PIO_FUNC_GPIO);
    uapi_pin_set_pull(CONFIG_MAX30102_INT_PIN, PIN_PULL_TYPE_UP);
    uapi_gpio_set_dir(CONFIG_MAX30102_INT_PIN, GPIO_DIRECTION_INPUT);
    uapi_gpio_register_isr_func(CONFIG_MAX30102_INT_PIN, GPIO_INTERRUPT_FALLING_EDGE,
                                max30102_int_triggered);

    // initialize max30102 handler
    MAX30102_LINK_INIT(&g_max30102_handle);
    MAX30102_LINK_I2C_INIT(&g_max30102_handle, max30102_interface_i2c_init);
    MAX30102_LINK_I2C_DEINIT(&g_max30102_handle, max30102_interface_i2c_deinit);
    MAX30102_LINK_I2C_READ(&g_max30102_handle, max30102_interface_i2c_read);
    MAX30102_LINK_I2C_WRITE(&g_max30102_handle, max30102_interface_i2c_write);
    MAX30102_LINK_IRQ_CALLBACK(&g_max30102_handle, max30102_interface_irq_callback);
    MAX30102_LINK_DELAY_MS(&g_max30102_handle, max30102_interface_delay_ms);
    MAX30102_LINK_DEBUG_PRINT(&g_max30102_handle, max30102_interface_debug_print);

    // initialize max30102 handler
    ret = max30102_init(&g_max30102_handle);
    if (ret != MAX30102_ERR_NONE) {
        WLID_LINK_SERVER_LOG_ERROR("init failed, ret = %" PRIu8 "\r\n", ret);
    }

    // reset max30102
    ret = max30102_reset(&g_max30102_handle);
    if (ret != MAX30102_ERR_NONE) {
        WLID_LINK_SERVER_LOG_ERROR("reset failed, ret = %" PRIu8 "\r\n", ret);
    }
    osal_msleep(100);

    // configure max30102 registers
    const uint8_t reg_configs[][2] = {
        {MAX30102_REG_INTERRUPT_ENABLE_1, 0x80}, // only FIFO Almost Full int enabled
        {MAX30102_REG_FIFO_WRITE_POINTER, 0x00},
        {MAX30102_REG_OVERFLOW_COUNTER, 0x00},
        {MAX30102_REG_FIFO_READ_POINTER, 0x00},
        {MAX30102_REG_FIFO_CONFIG,
         0x1c}, // sample avg = 4,fifo rollover = 1, fifo almost full = 20
        {MAX30102_REG_MODE_CONFIG,
         0x03}, // 0x02 for Red only, 0x03 for SpO2 mode 0x07 multimode LED
        {MAX30102_REG_SPO2_CONFIG, 0x67}, // SPO2_ADC range = 4096nA, SPO2 sample
                                          // rate(100 Hz), LED pulseWidth (400uS)
        {MAX30102_REG_LED_PULSE_1, 0x3f}, // Choose value for ~ 7mA for LED1
        {MAX30102_REG_LED_PULSE_2, 0x3f}  // Choose value for ~ 7mA for LED2
    };

    for (size_t i = 0; i < array_size(reg_configs); i++) {
        ret = max30102_write_reg(&g_max30102_handle, reg_configs[i][0],
                                 reg_configs[i][1]);
        if (ret != MAX30102_ERR_NONE) {
            WLID_LINK_SERVER_LOG_ERROR("write reg failed, ret = %" PRIu8 "\r\n", ret);
        }
    }

    max30102_sensor_buffers *p_fifo_buf = &g_max30102_buffers[0];
    max30102_sensor_buffers *p_cal_buf = &g_max30102_buffers[1];
    uint16_t fifo_buf_wr_idx = MAX30102_SLIDING_WINDOW_KEEP_COUNT;

    for (;;) {
        osal_msleep(1);

        const int max30102_event =
            osal_event_read(&g_max30102_event, MAX30102_EVENT_ALL, OSAL_EVENT_FOREVER,
                            OSAL_WAITMODE_OR | OSAL_WAITMODE_CLR);
        if (max30102_event == OSAL_FAILURE) {
            WLID_LINK_SERVER_LOG_ERROR("failed to read max30102 event\r\n");
            continue;
        }

        if (max30102_event & MAX30102_EVENT_FIFO_A_FULL) {
            uint8_t max30102_wr_ptr;
            ret = max30102_read_reg(&g_max30102_handle, MAX30102_REG_FIFO_WRITE_POINTER,
                                    &max30102_wr_ptr);
            if (ret != MAX30102_ERR_NONE) {
                WLID_LINK_SERVER_LOG_ERROR(
                    "read fifo wr ptr failed, ret = %" PRIu8 "\r\n", ret);
                continue;
            }

            uint8_t max30102_rd_ptr;
            ret = max30102_read_reg(&g_max30102_handle, MAX30102_REG_FIFO_READ_POINTER,
                                    &max30102_rd_ptr);
            if (ret != MAX30102_ERR_NONE) {
                WLID_LINK_SERVER_LOG_ERROR(
                    "read fifo rd ptr failed, ret = %" PRIu8 "\r\n", ret);
                continue;
            }

            // check how much data in the fifo
            uint8_t max30102_fifo_available_cnt;
            if (max30102_wr_ptr > max30102_rd_ptr) {
                max30102_fifo_available_cnt = max30102_wr_ptr - max30102_rd_ptr;
            }
            else {
                max30102_fifo_available_cnt = 32 - max30102_rd_ptr + max30102_wr_ptr;
            }

            // The amount of data read should not exceed the remaining space in the
            // buffer
            if (fifo_buf_wr_idx + max30102_fifo_available_cnt
                > MAX30102_SAMPLE_DATA_BUFFER_SIZE)
            {
                max30102_fifo_available_cnt =
                    MAX30102_SAMPLE_DATA_BUFFER_SIZE - fifo_buf_wr_idx;
            }

            for (uint16_t i = 0; i < max30102_fifo_available_cnt; i++) {
                ret = max30102_read_fifo(&g_max30102_handle,
                                         &(p_fifo_buf->ir_buf[fifo_buf_wr_idx]),
                                         &(p_fifo_buf->red_buf[fifo_buf_wr_idx]));
                if (ret != MAX30102_ERR_NONE) {
                    WLID_LINK_SERVER_LOG_ERROR("read fifo failed, ret = %" PRIu8 "\r\n",
                                               ret);
                    break;
                }
                fifo_buf_wr_idx++;
            }

            if (fifo_buf_wr_idx >= MAX30102_SAMPLE_DATA_BUFFER_SIZE) {
                max30102_sensor_buffers *swap_temp = p_fifo_buf;
                p_fifo_buf = p_cal_buf;
                p_cal_buf = swap_temp;

                memcpy(p_fifo_buf->ir_buf,
                       p_cal_buf->ir_buf
                           + (MAX30102_SAMPLE_DATA_BUFFER_SIZE
                              - MAX30102_SLIDING_WINDOW_KEEP_COUNT),
                       MAX30102_SLIDING_WINDOW_KEEP_COUNT
                           * sizeof(p_cal_buf->ir_buf[0]));
                memcpy(p_fifo_buf->red_buf,
                       p_cal_buf->red_buf
                           + (MAX30102_SAMPLE_DATA_BUFFER_SIZE
                              - MAX30102_SLIDING_WINDOW_KEEP_COUNT),
                       MAX30102_SLIDING_WINDOW_KEEP_COUNT
                           * sizeof(p_cal_buf->red_buf[0]));

                fifo_buf_wr_idx = MAX30102_SLIDING_WINDOW_KEEP_COUNT;

                osal_event_write(&g_max30102_event, MAX30102_EVENT_SAMPLE_BUFFER_FULL);
            }
        }

        if (max30102_event & MAX30102_EVENT_SAMPLE_BUFFER_FULL) {
#if MAX30102_SAMPLE_DATA_PREPROCESS_ENABLED

            // 5th order smooth
            signal_preprocess_5th_order_smooth(((int32_t *)p_cal_buf->ir_buf),
                                               MAX30102_SAMPLE_DATA_BUFFER_SIZE);
            signal_preprocess_5th_order_smooth(((int32_t *)p_cal_buf->red_buf),
                                               MAX30102_SAMPLE_DATA_BUFFER_SIZE);
            // lowpass filter
            signal_preprocess_lowpass_filter(
                ((int32_t *)p_cal_buf->ir_buf), MAX30102_SAMPLE_DATA_BUFFER_SIZE,
                MAX30102_SAMPLING_RATE, MAX30102_LOWPASS_CUTOFF_HZ);
            signal_preprocess_lowpass_filter(
                ((int32_t *)p_cal_buf->red_buf), MAX30102_SAMPLE_DATA_BUFFER_SIZE,
                MAX30102_SAMPLING_RATE, MAX30102_LOWPASS_CUTOFF_HZ);

            int32_t signal_quality = signal_quality_assessment(
                ((int32_t *)p_cal_buf->ir_buf), ((int32_t *)p_cal_buf->red_buf),
                MAX30102_SAMPLE_DATA_BUFFER_SIZE);
            WLID_LINK_SERVER_LOG_DEBUG("signal quality: %" PRIi32 "\r\n",
                                       signal_quality);

            // remove baseline
            // signal_preprocess_remove_baseline(((int32_t *)p_cal_buf->ir_buf),
            //                                   MAX30102_SAMPLE_DATA_BUFFER_SIZE);
            // signal_preprocess_remove_baseline(((int32_t *)p_cal_buf->red_buf),
            //                                   MAX30102_SAMPLE_DATA_BUFFER_SIZE);

#endif // MAX30102_SAMPLE_DATA_PREPROCESS_ENABLED

            maxim_heart_rate_and_oxygen_saturation(
                &(p_cal_buf->ir_buf[0]), array_size(p_cal_buf->ir_buf),
                &(p_cal_buf->red_buf[0]), &g_spo2_val, &g_spo2_valid, &g_heart_rate,
                &g_heart_rate_valid);
            WLID_LINK_SERVER_LOG_DEBUG(
                "heart rate: %" PRIi32 ", heart valid: %" PRIi8 ", spo2 val: %" PRIi32
                ", spo2 valid: %" PRIi8 "\r\n",
                g_heart_rate, g_heart_rate_valid, g_spo2_val, g_spo2_valid);

            if (signal_quality >= 60) {
                if (g_heart_rate < 0 || g_heart_rate > 150) {
                    g_heart_rate_valid = 0;
                }
                if (g_spo2_val < 0 || g_spo2_val > 100) {
                    g_spo2_valid = 0;
                }
            }
            else {
                g_heart_rate_valid = 0;
                g_spo2_valid = 0;
            }

            if (g_heart_rate_valid) {
                if (g_hr_valid_count > 0) {
                    float hr_suppression_factor = 0.9f;
                    if (g_heart_rate >= 70 && g_heart_rate <= 90) {
                        hr_suppression_factor *= 0.3f;
                    }
                    else if ((g_heart_rate >= 60 && g_heart_rate < 70)
                             || (g_heart_rate > 90 && g_heart_rate <= 120))
                    {
                        hr_suppression_factor *= 0.75f;
                    }
                    else {
                        hr_suppression_factor *= 0.95f;
                    }

                    const uint8_t last_valid_hr_rd_idx =
                        (g_hr_buf_wr_idx + array_size(g_valid_hr_read_buf) - 1)
                        % array_size(g_valid_hr_read_buf);
                    const int32_t last_valid_hr =
                        g_valid_hr_read_buf[last_valid_hr_rd_idx];

                    g_heart_rate = (last_valid_hr * hr_suppression_factor
                                    + g_heart_rate * (1.0f - hr_suppression_factor));

                    WLID_LINK_SERVER_LOG_DEBUG("smoothed heart rate = %" PRIi32 "\r\n",
                                               g_heart_rate);
                }

                g_valid_hr_read_buf[g_hr_buf_wr_idx++] = (uint8_t)g_heart_rate;
                g_hr_buf_wr_idx %= array_size(g_valid_hr_read_buf);
                if (g_hr_valid_count < array_size(g_valid_hr_read_buf)) {
                    g_hr_valid_count++;
                }
            }
        }

        if (max30102_event & MAX30102_EVENT_INT_TRIGGERED) {
            max30102_irq_handler(&g_max30102_handle);
        }
    }
    osal_kthread_destroy(g_max30102_task_handle, 0);
    return 0;
}

static void max30102_int_triggered(pin_t pin, uintptr_t param) {
    unused(pin);
    unused(param);

    osal_event_write(&g_max30102_event, MAX30102_EVENT_INT_TRIGGERED);
}

static uint8_t max30102_interface_i2c_init(void) {
    return MAX30102_ERR_NONE;
}

static uint8_t max30102_interface_i2c_deinit(void) {
    return MAX30102_ERR_NONE;
}

static uint8_t max30102_interface_i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf,
                                           uint16_t len) {
    uint8_t ret;
    ret = soft_i2c_mem_read(&g_soft_i2c_handle, addr, SOFT_I2C_ADDR_7BIT, &reg,
                            sizeof(reg), buf, len);
    if (ret != SOFT_I2C_ERR_NONE) {
        WLID_LINK_SERVER_LOG_ERROR("ret = %" PRIu8 "\r\n", ret);
        return MAX30102_ERR_FAILED;
    }

    return MAX30102_ERR_NONE;
}

static uint8_t max30102_interface_i2c_write(uint8_t addr, uint8_t reg, uint8_t *buf,
                                            uint16_t len) {
    uint8_t ret;
    ret = soft_i2c_mem_write(&g_soft_i2c_handle, addr, SOFT_I2C_ADDR_7BIT, &reg,
                             sizeof(reg), buf, len);
    if (ret != SOFT_I2C_ERR_NONE) {
        WLID_LINK_SERVER_LOG_ERROR("ret = %" PRIu8 "\r\n", ret);
        return MAX30102_ERR_FAILED;
    }

    return MAX30102_ERR_NONE;
}

static void max30102_interface_irq_callback(max30102_handle_t *handle,
                                            max30102_interrupt_status_t status) {
    unused(handle);

    switch (status) {
    case MAX30102_INTERRUPT_STATUS_A_FIFO_FULL: {
        osal_event_write(&g_max30102_event, MAX30102_EVENT_FIFO_A_FULL);
        break;
    }

    case MAX30102_INTERRUPT_STATUS_PPG_RDY:
    case MAX30102_INTERRUPT_STATUS_ALC_OVF:
    case MAX30102_INTERRUPT_STATUS_PWR_RDY:
    case MAX30102_INTERRUPT_STATUS_DIE_TEMP_RDY:
    default: {
        WLID_LINK_SERVER_LOG_WARN("int status = %d\r\n", status);
        break;
    }
    }
}

static void max30102_interface_delay_ms(uint32_t ms) {
    osal_msleep(ms);
}

static void max30102_interface_debug_print(const char *const fmt, ...) {
    va_list args;
    va_start(args, fmt);
    osal_vprintk(fmt, args);
    va_end(args);
}