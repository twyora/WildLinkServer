#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "common_def.h"
#include "osal_debug.h"
#include "osal_event.h"
#include "osal_task.h"
#include "sle_connection_manager.h"
#include "sle_device_discovery.h"
#include "sle_ssap_server.h"
#include "soc_osal.h"

#if CONFIG_MAX30102_TASK_ENABLED
#include "max30102_task.h"
#endif // CONFIG_MAX30102_TASK_ENABLED

#if CONFIG_MAX30205_TASK_ENABLED
#include "max30205_task.h"
#endif // CONFIG_MAX30205_TASK_ENABLED

#include "wlid_link_server_log.h"

#include "sle_server_task.h"

#ifndef STRINGIFY
#define STRINGIFY(x) #x
#endif

#define SLE_SERVER_TASK_ADV_HANDLE 1

#define SLE_SERVER_EVENT_CLIENT_CONNECTED 0x01u

static const uint8_t g_sle_uuid_base[SLE_UUID_LEN] = {
    0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
    0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t g_sle_server_addr[] = {
    CONFIG_SLE_SERVER_ADDR_LOW_BYTE, 0x02, 0x03, 0x04, 0x05, 0x06};
static uint16_t g_sle_server_conn_id = 0;
static uint8_t g_sle_server_id = 0;
static uint16_t g_sle_server_start_handle = 0;
static uint16_t g_sle_server_end_handle = 0;
static uint16_t g_sle_server_max30102_handle = 0;
static uint16_t g_sle_server_max30205_handle = 0;

osal_task *g_sle_server_task_handle = NULL;
static osal_event g_sle_server_event;

// announce
static void sle_server_task_announce_enable_callback(uint32_t announce_id,
                                                     errcode_t status);
static void sle_server_task_announce_disable_callback(uint32_t announce_id,
                                                      errcode_t status);
static void sle_server_task_announce_terminal_callback(uint32_t announce_id);

// connection
static void sle_server_task_connect_state_changed_callback(
    uint16_t conn_id, const sle_addr_t *addr, sle_acb_state_t conn_state,
    sle_pair_state_t pair_state, sle_disc_reason_t disc_reason);
static void sle_server_task_pair_complete_callback(uint16_t conn_id,
                                                   const sle_addr_t *addr,
                                                   errcode_t status);

// ssaps
static void sle_server_task_ssapc_mtu_changed_callback(uint8_t server_id,
                                                       uint16_t conn_id,
                                                       ssap_exchange_info_t *info,
                                                       errcode_t status);
static void sle_server_task_read_request_callback(uint8_t server_id, uint16_t conn_id,
                                                  ssaps_req_read_cb_t *read_cb_para,
                                                  errcode_t status);
static void sle_server_task_write_request_callback(uint8_t server_id, uint16_t conn_id,
                                                   ssaps_req_write_cb_t *write_cb_para,
                                                   errcode_t status);

static sle_uuid_t *sle_server_task_set_uuid(sle_uuid_t *buf, uint16_t uuid);
static errcode_t sle_server_task_add_property(uint8_t server_id,
                                              uint16_t service_handle,
                                              uint16_t property_uuid,
                                              uint16_t *out_property_handle);

static errcode_t sle_server_task_send_report_by_uuid(uint16_t uuid, void *data,
                                                     uint16_t data_len);
static errcode_t sle_server_task_send_report_by_handle(uint16_t handle, void *data,
                                                       uint16_t data_len);

static int sle_server_task(void *args) {
    unused(args);

    WLID_LINK_SERVER_LOG_INFO("in\r\n");
    osal_msleep(1000);

    errcode_t ret;

    sle_announce_seek_callbacks_t seek_cbks = {0};
    sle_connection_callbacks_t conn_cbks = {0};
    ssaps_callbacks_t ssaps_cbks = {0};
    sle_uuid_t sle_uuid = {0};
    sle_announce_param_t param = {0};
    sle_announce_data_t data = {0};
    uint8_t announce_data[] = {0x01, 0x02, 0x01, 0x02, 0x02, 0x00};
    uint8_t seek_data[] = {0x0c, 0x02, 0x0a, 0x10, 0x0b, 0x73, 0x6c, 0x65, 0x5f, 0x75,
                           0x61, 0x72, 0x74, 0x5f, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72};

    ret = enable_sle();
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("failed to enable sle, ret = %08" PRIx32 "\r\n",
                                   ret);
        goto _error_exit;
    }

    seek_cbks.announce_enable_cb = sle_server_task_announce_enable_callback;
    seek_cbks.announce_disable_cb = sle_server_task_announce_disable_callback;
    seek_cbks.announce_terminal_cb = sle_server_task_announce_terminal_callback;
    ret = sle_announce_seek_register_callbacks(&seek_cbks);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR(
            "failed to register seek callbacks, ret = %08" PRIx32 "\r\n", ret);
        goto _error_exit;
    }

    conn_cbks.connect_state_changed_cb = sle_server_task_connect_state_changed_callback;
    conn_cbks.pair_complete_cb = sle_server_task_pair_complete_callback;
    ret = sle_connection_register_callbacks(&conn_cbks);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR(
            "failed to register connection callbacks, ret = %08" PRIx32 "\r\n", ret);
        goto _error_exit;
    }

    ssaps_cbks.mtu_changed_cb = sle_server_task_ssapc_mtu_changed_callback;
    ssaps_cbks.read_request_cb = sle_server_task_read_request_callback;
    ssaps_cbks.write_request_cb = sle_server_task_write_request_callback;
    ret = ssaps_register_callbacks(&ssaps_cbks);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR(
            "failed to register ssaps callbacks, ret = %08" PRIx32 "\r\n", ret);
        goto _error_exit;
    }

    // add server
    sle_server_task_set_uuid(&sle_uuid, CONFIG_SLE_SERVER_APP_UUID);
    ret = ssaps_register_server(&sle_uuid, &g_sle_server_id);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("failed to register server, ret = %08" PRIx32 "\r\n",
                                   ret);
        goto _error_exit;
    }

    // add service
    sle_server_task_set_uuid(&sle_uuid, CONFIG_SLE_SERVER_SERVICE_UUID);
    ret = ssaps_add_service_sync(g_sle_server_id, &sle_uuid, true,
                                 &g_sle_server_start_handle);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("failed to add service, ret = %08" PRIx32 "\r\n",
                                   ret);
        goto _error_exit;
    }

    ret = sle_server_task_add_property(g_sle_server_id, g_sle_server_start_handle,
                                       CONFIG_SLE_SERVER_MAX30102_UUID,
                                       &g_sle_server_max30102_handle);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("failed to add property, ret = %08" PRIx32 "\r\n",
                                   ret);
        goto _error_exit;
    }

    ret = sle_server_task_add_property(g_sle_server_id, g_sle_server_start_handle,
                                       CONFIG_SLE_SERVER_MAX30205_UUID,
                                       &g_sle_server_max30205_handle);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("failed to add property, ret = %08" PRIx32 "\r\n",
                                   ret);
        goto _error_exit;
    }

    ret = ssaps_start_service(g_sle_server_id, g_sle_server_start_handle);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("failed to start service, ret = %08" PRIx32 "\r\n",
                                   ret);
        goto _error_exit;
    }

    param.announce_mode = SLE_ANNOUNCE_MODE_CONNECTABLE_SCANABLE;
    param.announce_handle = SLE_SERVER_TASK_ADV_HANDLE;
    param.announce_gt_role = SLE_ANNOUNCE_ROLE_T_CAN_NEGO;
    param.announce_level = SLE_ANNOUNCE_LEVEL_NORMAL;
    param.announce_channel_map = 0x07;
    param.announce_interval_min = 0xC8;     // 25ms
    param.announce_interval_max = 0xC8;     // 25ms
    param.conn_interval_min = 0x64;         // 12.5ms
    param.conn_interval_max = 0x64;         // 12.5ms
    param.conn_max_latency = 0x01F3;        // 4990ms, unit: 10ms
    param.conn_supervision_timeout = 0x1F4; // 5000ms, unit: 10ms
    param.announce_tx_power = 18;
    param.own_addr.type = SLE_ADDRESS_TYPE_PUBLIC;
    memcpy(param.own_addr.addr, g_sle_server_addr, sizeof(param.own_addr.addr));
    ret = sle_set_announce_param(param.announce_handle, &param);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR(
            "failed to set announce param, ret = %08" PRIx32 "\r\n", ret);
        goto _error_exit;
    }

    data.announce_data = announce_data;
    data.announce_data_len = sizeof(announce_data);
    data.seek_rsp_data = seek_data;
    data.seek_rsp_data_len = sizeof(seek_data);
    ret = sle_set_announce_data(SLE_SERVER_TASK_ADV_HANDLE, &data);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR(
            "failed to set announce data, ret = %08" PRIx32 "\r\n", ret);
        goto _error_exit;
    }

    ret = sle_start_announce(SLE_SERVER_TASK_ADV_HANDLE);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("failed to start announce, ret = %08" PRIx32 "\r\n",
                                   ret);
        goto _error_exit;
    }

    for (;;) {
        osal_msleep(1);

        const int event =
            osal_event_read(&g_sle_server_event, SLE_SERVER_EVENT_CLIENT_CONNECTED,
                            OSAL_WAIT_FOREVER, OSAL_WAITMODE_OR);

        if (event == OSAL_FAILURE) {
            WLID_LINK_SERVER_LOG_ERROR("failed to read event\r\n");
            continue;
        }

        if (event & SLE_SERVER_EVENT_CLIENT_CONNECTED) {
#if CONFIG_MAX30102_TASK_ENABLED
            uint8_t max30102_data[4] = {0};
            // max30102
            max30102_read_heart_rate_and_oxygen_saturation(
                /*out_spo2_val = */ &max30102_data[0],
                /*out_spo2_valid = */ (bool *)&max30102_data[1],
                /*out_heart_rate_val = */ &max30102_data[2],
                /*out_heart_rate_valid = */ (bool *)&max30102_data[3]);
            sle_server_task_send_report_by_uuid(CONFIG_SLE_SERVER_MAX30102_UUID,
                                                &max30102_data, sizeof(max30102_data));
            WLID_LINK_SERVER_LOG_DEBUG(
                "send max30102 data = %#02x, %#02x, %#02x, %#02x\r\n", max30102_data[0],
                max30102_data[1], max30102_data[2], max30102_data[3]);
            osal_msleep(500);
#endif // CONFIG_MAX30102_TASK_ENABLED

#if CONFIG_MAX30205_TASK_ENABLED
            float max30205_temp = max30205_task_read_temperature();
            sle_server_task_send_report_by_uuid(CONFIG_SLE_SERVER_MAX30205_UUID,
                                                &max30205_temp, sizeof(max30205_temp));
            WLID_LINK_SERVER_LOG_DEBUG("send max30205 temp = %d.%02d\r\n",
                                       (int)max30205_temp, (int)(max30205_temp * 100));
            osal_msleep(500);
#endif // CONFIG_MAX30205_TASK_ENABLED
        }

        osal_msleep(1);
    }

_error_exit:
    WLID_LINK_SERVER_LOG_ERROR("error code = %08" PRIx32 "\r\n", ret);
    osal_kthread_destroy(g_sle_server_task_handle, 0);
    return 0;
}

errcode_t sle_server_task_entry(void) {
    errcode_t ret;

    ret = osal_event_init(&g_sle_server_event);
    if (ret != OSAL_SUCCESS) {
        WLID_LINK_SERVER_LOG_ERROR("failed to create sle server event\r\n");
        goto _error_return;
    }

    g_sle_server_task_handle =
        osal_kthread_create(sle_server_task, NULL, STRINGIFY(sle_server_task),
                            CONFIG_SLE_SERVER_TASK_STACK_SIZE);
    if (g_sle_server_task_handle == NULL) {
        WLID_LINK_SERVER_LOG_ERROR("failed to create sle server task\r\n");
        goto _clean_event;
    }

    if (osal_kthread_set_priority(g_sle_server_task_handle,
                                  CONFIG_SLE_SERVER_TASK_PRIORITY)
        != OSAL_SUCCESS)
    {
        WLID_LINK_SERVER_LOG_ERROR("failed to set sle server task priority\r\n");
        goto _clean_kthread;
    }

    return ERRCODE_SUCC;
_clean_kthread:
    osal_kthread_destroy(g_sle_server_task_handle, 0);
_clean_event:
    osal_event_destroy(&g_sle_server_event);
_error_return:
    return ERRCODE_FAIL;
}

static void sle_server_task_announce_enable_callback(uint32_t announce_id,
                                                     errcode_t status) {
    WLID_LINK_SERVER_LOG_DEBUG("announce id = %" PRIu32 "\r\n", announce_id);
    if (status != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("status = %08" PRIx32 "\r\n", status);
    }
}

static void sle_server_task_announce_disable_callback(uint32_t announce_id,
                                                      errcode_t status) {
    WLID_LINK_SERVER_LOG_DEBUG("announce id = %" PRIu32 "\r\n", announce_id);
    if (status != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("status = %08" PRIx32 "\r\n", status);
    }
}

static void sle_server_task_announce_terminal_callback(uint32_t announce_id) {
    WLID_LINK_SERVER_LOG_DEBUG("announce id = %" PRIu32 "\r\n", announce_id);
}

static void sle_server_task_connect_state_changed_callback(
    uint16_t conn_id, const sle_addr_t *addr, sle_acb_state_t conn_state,
    sle_pair_state_t pair_state, sle_disc_reason_t disc_reason) {
    WLID_LINK_SERVER_LOG_DEBUG(
        "conn id = %" PRIx16 ", conn state = %" PRId32 ", pair state = %" PRId32
        ", disc reason = %" PRId32 ", sle addr type = %" PRIu8 ", sle addr = ",
        conn_id, conn_state, pair_state, disc_reason, addr->type);
    for (uint8_t i = 0; i < sizeof(addr->addr) / sizeof(addr->addr[0]); i++) {
        WLID_LINK_SERVER_LOG_PRINT_DEBUG("%#02" PRIx8 " ", addr->addr[i]);
    }
    WLID_LINK_SERVER_LOG_PRINT_DEBUG("\r\n");

    if (conn_state == SLE_ACB_STATE_CONNECTED) {
        g_sle_server_conn_id = conn_id;
        osal_event_write(&g_sle_server_event, SLE_SERVER_EVENT_CLIENT_CONNECTED);
    }
    else if (conn_state == SLE_ACB_STATE_DISCONNECTED) {
        g_sle_server_conn_id = 0;
        osal_event_clear(&g_sle_server_event, SLE_SERVER_EVENT_CLIENT_CONNECTED);
        errcode_t start_adv_ret = sle_start_announce(SLE_SERVER_TASK_ADV_HANDLE);
        if (start_adv_ret != ERRCODE_SUCC) {
            WLID_LINK_SERVER_LOG_ERROR(
                "failed to restart announce, ret = %#" PRIx32 "\r\n", start_adv_ret);
        }
    }

    unused(addr);
    unused(pair_state);
    unused(disc_reason);
}

static void sle_server_task_pair_complete_callback(uint16_t conn_id,
                                                   const sle_addr_t *addr,
                                                   errcode_t status) {
    WLID_LINK_SERVER_LOG_DEBUG(
        "conn id = %" PRIx16 ", sle addr type = %" PRIu8 ", sle addr = ", conn_id,
        addr->type);
    for (uint8_t i = 0; i < sizeof(addr->addr) / sizeof(addr->addr[0]); i++) {
        WLID_LINK_SERVER_LOG_PRINT_DEBUG("%#02" PRIx8 " ", addr->addr[i]);
    }
    WLID_LINK_SERVER_LOG_PRINT_DEBUG("\r\n");

    if (status != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("status = %08" PRIx32 "\r\n", status);
    }

    ssap_exchange_info_t parameter = {0};
    parameter.mtu_size = 520;
    parameter.version = 1;
    errcode_t ssaps_set_info_ret = ssaps_set_info(g_sle_server_id, &parameter);
    if (ssaps_set_info_ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("failed to set ssap info, ret = %#" PRIx32 "\r\n",
                                   ssaps_set_info_ret);
    }

    unused(conn_id);
    unused(addr);
}

static void sle_server_task_ssapc_mtu_changed_callback(uint8_t server_id,
                                                       uint16_t conn_id,
                                                       ssap_exchange_info_t *info,
                                                       errcode_t status) {
    WLID_LINK_SERVER_LOG_DEBUG("server id = %" PRIu8 ", conn id = %" PRIu16
                               ", mtu: size = %" PRIu32 ", version = %" PRIu16 "\r\n",
                               server_id, conn_id, info->mtu_size, info->version);

    if (status != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("status = %08" PRIx32 "\r\n", status);
    }

    unused(server_id);
    unused(conn_id);
    unused(info);
}

static void sle_server_task_read_request_callback(uint8_t server_id, uint16_t conn_id,
                                                  ssaps_req_read_cb_t *read_cb_para,
                                                  errcode_t status) {
    WLID_LINK_SERVER_LOG_DEBUG(
        "server id = %" PRIu8 ", conn id = %" PRIu16 ", read request: id = %" PRIu16
        ", handle = %" PRIu16 ", type = %" PRIu8 ", need resp = %" PRIu8
        ", need authorize = %" PRIu8,
        server_id, conn_id, read_cb_para->request_id, read_cb_para->handle,
        read_cb_para->type, read_cb_para->need_rsp, read_cb_para->need_authorize);

    if (status != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("status = %08" PRIx32 "\r\n", status);
    }

    unused(server_id);
    unused(conn_id);
    unused(read_cb_para);
}

static void sle_server_task_write_request_callback(uint8_t server_id, uint16_t conn_id,
                                                   ssaps_req_write_cb_t *write_cb_para,
                                                   errcode_t status) {
    WLID_LINK_SERVER_LOG_DEBUG(
        "server id = %" PRIu8 ", conn id = %" PRIu16 ", write request: id = %" PRIu16
        ", handle = %" PRIu16 ", type = %" PRIu8 "need resp = %" PRIu8
        ", need authorize = %" PRIu8 " , length = %" PRIu16 ", data = ",
        server_id, conn_id, write_cb_para->request_id, write_cb_para->handle,
        write_cb_para->type, write_cb_para->need_rsp, write_cb_para->need_authorize,
        write_cb_para->length);

    for (uint16_t i = 0; i < write_cb_para->length; i++) {
        WLID_LINK_SERVER_LOG_PRINT_DEBUG("%#02" PRIx8 " ", write_cb_para->value[i]);
    }
    WLID_LINK_SERVER_LOG_PRINT_DEBUG("\r\n");

    if (status != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("status = %08" PRIx32 "\r\n", status);
    }

    unused(server_id);
    unused(conn_id);
    unused(write_cb_para);
}

static sle_uuid_t *sle_server_task_set_uuid(sle_uuid_t *buf, uint16_t uuid) {
    static const uint8_t sle_uuid_base[SLE_UUID_LEN] = {
        0x37, 0xBE, 0xA8, 0x80, 0xFC, 0x70, 0x11, 0xEA,
        0xB7, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    memcpy(buf->uuid, sle_uuid_base, sizeof(buf->uuid) - sizeof(uuid));
    memcpy(&(buf->uuid[sizeof(buf->uuid) - sizeof(uuid)]), &uuid, sizeof(uuid));
    buf->len = sizeof(uuid);

    return buf;
}

static errcode_t sle_server_task_add_property(uint8_t server_id,
                                              uint16_t service_handle,
                                              uint16_t property_uuid,
                                              uint16_t *out_property_handle) {
    if (out_property_handle == NULL) {
        return ERRCODE_INVALID_PARAM;
    }

    errcode_t ret;

    ssaps_property_info_t property = {0};
    ssaps_desc_info_t descriptor = {0};
    uint8_t property_value[8] = {0};
    uint8_t ntf_value[] = {0x01, 0x0};

    property.permissions = (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE);
    property.operate_indication =
        SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_NOTIFY;
    property.value = property_value;
    property.value_len = sizeof(property_value);
    sle_server_task_set_uuid(&property.uuid, property_uuid);

    ret = ssaps_add_property_sync(server_id, service_handle, &property,
                                  out_property_handle);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("failed to add property, uuid = %04" PRIx16 "\r\n",
                                   property_uuid);
        goto _error_exit;
    }

    if (g_sle_server_end_handle < *out_property_handle) {
        g_sle_server_end_handle = *out_property_handle;
    }

    descriptor.permissions = (SSAP_PERMISSION_READ | SSAP_PERMISSION_WRITE);
    descriptor.type = SSAP_DESCRIPTOR_USER_DESCRIPTION;
    descriptor.operate_indication =
        SSAP_OPERATE_INDICATION_BIT_READ | SSAP_OPERATE_INDICATION_BIT_WRITE;
    descriptor.value = ntf_value;
    descriptor.value_len = sizeof(ntf_value);

    ret = ssaps_add_descriptor_sync(server_id, service_handle, *out_property_handle,
                                    &descriptor);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR(
            "failed to add descriptor, property uuid = %04" PRIx16 "\r\n",
            property_uuid);
        goto _error_exit;
    }

    return ERRCODE_SUCC;
_error_exit:
    return ret;
}

static errcode_t sle_server_task_send_report_by_uuid(uint16_t uuid, void *data,
                                                     uint16_t data_len) {
    errcode_t ret;

    ssaps_ntf_ind_by_uuid_t param = {0};
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.start_handle = g_sle_server_start_handle;
    param.end_handle = g_sle_server_end_handle;
    sle_server_task_set_uuid(&param.uuid, uuid);
    param.value_len = data_len;
    param.value = (uint8_t *)data;

    ret = ssaps_notify_indicate_by_uuid(g_sle_server_id, g_sle_server_conn_id, &param);
    if (ret != ERRCODE_SUCC) {
        WLID_LINK_SERVER_LOG_ERROR("notify error\r\n");
        goto _error_exit;
    }

    return ERRCODE_SUCC;
_error_exit:
    WLID_LINK_SERVER_LOG_ERROR("ret = %08" PRIx32 "\r\n", ret);
    return ret;
}

static errcode_t sle_server_task_send_report_by_handle(uint16_t handle, void *data,
                                                       uint16_t data_len) {
    ssaps_ntf_ind_t param = {0};
    param.handle = handle;
    param.type = SSAP_PROPERTY_TYPE_VALUE;
    param.value = (uint8_t *)data;
    param.value_len = data_len;
    return ssaps_notify_indicate(g_sle_server_id, g_sle_server_conn_id, &param);
}