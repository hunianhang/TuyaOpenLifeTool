/**
 * @file app_mcp.c
 * @brief app_mcp module is used to
 * @version 0.1
 * @copyright Copyright (c) 2021-2025 Tuya Inc. All Rights Reserved.
 */

#include "app_mcp.h"
#include "tuya_ai_agent.h"
#include "wukong_ai_mcp_server.h"

#if defined(ENABLE_EX_MODULE_CAMERA) && (ENABLE_EX_MODULE_CAMERA == 1)
#include "app_camera.h"
#endif

#if defined(ENABLE_CHAT_DISPLAY2) && (ENABLE_CHAT_DISPLAY2 == 1)
#include "app_display.h"
#include "lang_config.h"
#endif

#if defined(ENABLE_ALARM) && (ENABLE_ALARM == 1)
#include "app_alarm.h"
#endif

#if defined(ENABLE_DASHBOARD) && (ENABLE_DASHBOARD == 1)
#include "app_dashboard.h"
#endif

#include "tal_api.h"
#include "cJSON.h"

#include "ai_audio.h"
#include "wukong_ai_mcp_server.h"
#include <stdint.h>

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
********************function declaration********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/
static OPERATE_RET __get_device_info(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val,
                                     void *user_data)
{
    OPERATE_RET rt = OPRT_OK;

    cJSON *json = cJSON_CreateObject();
    TUYA_CHECK_NULL_RETURN(json, OPRT_CR_CJSON_ERR);

    cJSON_AddStringToObject(json, "model", PROJECT_NAME);
    cJSON_AddStringToObject(json, "firmwareVersion", PROJECT_VERSION);

    wukong_mcp_return_value_set_json(ret_val, json);

    return rt;
}

// __set_volume
static OPERATE_RET __set_volume(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    int volume = 50; // default volume

    // Parse properties to get volume
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *prop = properties->properties[i];
        if (strcmp(prop->name, "volume") == 0 && prop->type == MCP_PROPERTY_TYPE_INTEGER) {
            volume = prop->default_val.int_val;
            break;
        }
    }

    // FIXME: Implement actual volume setting logic here
    ai_audio_set_volume(volume);
    PR_DEBUG("MCP set volume to %d", volume);

#if defined(ENABLE_CHAT_DISPLAY2) && (ENABLE_CHAT_DISPLAY2 == 1)
    char volume_msg[36] = {0};
    snprintf(volume_msg, sizeof(volume_msg), "%s %d (MCP)", SYSTEM_MSG_VOLUME, volume);
    app_display_send_msg(TY_DISPLAY_TP_SYSTEM_MSG, (uint8_t *)volume_msg, strlen(volume_msg));
#endif

    // Set return value
    wukong_mcp_return_value_set_bool(ret_val, TRUE);

    return OPRT_OK;
}

#if defined(ENABLE_EX_MODULE_CAMERA) && (ENABLE_EX_MODULE_CAMERA == 1)
static OPERATE_RET __take_photo(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t *image_data     = NULL;
    uint32_t image_data_len = 0;

    TUYA_CALL_ERR_RETURN(app_camera_jpeg_capture(&image_data, &image_data_len, 3 * 1000));
    rt = wukong_mcp_return_value_set_image(ret_val, MCP_IMAGE_MIME_TYPE_JPEG, image_data, image_data_len);

    return rt;
}
#endif

#if defined(ENABLE_ALARM) && (ENABLE_ALARM == 1)
static OPERATE_RET __set_alarm(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    int    hour    = 7;
    int    minute  = 0;
    BOOL_T enabled = TRUE;
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *p = properties->properties[i];
        if (strcmp(p->name, "hour") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            hour = p->default_val.int_val;
        else if (strcmp(p->name, "minute") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            minute = p->default_val.int_val;
        else if (strcmp(p->name, "enabled") == 0 && p->type == MCP_PROPERTY_TYPE_BOOLEAN)
            enabled = p->default_val.bool_val;
    }
    OPERATE_RET rt = app_alarm_set((uint8_t)hour, (uint8_t)minute, enabled != FALSE);
    if (rt == OPRT_OK) app_alarm_upload();
    wukong_mcp_return_value_set_bool(ret_val, rt == OPRT_OK ? TRUE : FALSE);
    return OPRT_OK;
}

static OPERATE_RET __set_alarm_in_minutes(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    int minutes = 5;
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *p = properties->properties[i];
        if (strcmp(p->name, "minutes") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            minutes = p->default_val.int_val;
    }
    OPERATE_RET rt = app_alarm_set_in_minutes((uint32_t)minutes);
    if (rt == OPRT_OK) app_alarm_upload();
    wukong_mcp_return_value_set_bool(ret_val, rt == OPRT_OK ? TRUE : FALSE);
    return OPRT_OK;
}
#endif

#if defined(ENABLE_DASHBOARD) && (ENABLE_DASHBOARD == 1)
static OPERATE_RET __dashboard_weather_set(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    const char *city = "Unknown", *condition = "Unknown";
    int temp = 20, humidity = 50;
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *p = properties->properties[i];
        if (strcmp(p->name, "city") == 0 && p->type == MCP_PROPERTY_TYPE_STRING)
            city = p->default_val.str_val;
        else if (strcmp(p->name, "condition") == 0 && p->type == MCP_PROPERTY_TYPE_STRING)
            condition = p->default_val.str_val;
        else if (strcmp(p->name, "temp") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            temp = p->default_val.int_val;
        else if (strcmp(p->name, "humidity") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            humidity = p->default_val.int_val;
    }
    OPERATE_RET rt = app_dashboard_weather_set(city, condition, temp, humidity);
    wukong_mcp_return_value_set_bool(ret_val, rt == OPRT_OK ? TRUE : FALSE);
    return OPRT_OK;
}

static OPERATE_RET __dashboard_todo_add(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    const char *text = NULL;
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *p = properties->properties[i];
        if (strcmp(p->name, "text") == 0 && p->type == MCP_PROPERTY_TYPE_STRING)
            text = p->default_val.str_val;
    }
    if (!text) { wukong_mcp_return_value_set_bool(ret_val, FALSE); return OPRT_OK; }
    int id = 0;
    OPERATE_RET rt = app_dashboard_todo_add(text, &id);
    ty_cJSON *json = ty_cJSON_CreateObject();
    if (json) {
        ty_cJSON_AddBoolToObject(json, "success", rt == OPRT_OK ? TRUE : FALSE);
        ty_cJSON_AddNumberToObject(json, "id", id);
        wukong_mcp_return_value_set_json(ret_val, json);
    }
    return OPRT_OK;
}

static OPERATE_RET __dashboard_todo_complete(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    int id = -1;
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *p = properties->properties[i];
        if (strcmp(p->name, "id") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            id = p->default_val.int_val;
    }
    OPERATE_RET rt = (id >= 0) ? app_dashboard_todo_complete(id) : OPRT_COM_ERROR;
    wukong_mcp_return_value_set_bool(ret_val, rt == OPRT_OK ? TRUE : FALSE);
    return OPRT_OK;
}

static OPERATE_RET __dashboard_todo_delete(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    int id = -1;
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *p = properties->properties[i];
        if (strcmp(p->name, "id") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            id = p->default_val.int_val;
    }
    OPERATE_RET rt = (id >= 0) ? app_dashboard_todo_delete(id) : OPRT_COM_ERROR;
    wukong_mcp_return_value_set_bool(ret_val, rt == OPRT_OK ? TRUE : FALSE);
    return OPRT_OK;
}

static OPERATE_RET __dashboard_medicine_add(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    const char *name = NULL, *dosage = "1";
    int hour = 8, minute = 0;
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *p = properties->properties[i];
        if (strcmp(p->name, "name") == 0 && p->type == MCP_PROPERTY_TYPE_STRING)
            name = p->default_val.str_val;
        else if (strcmp(p->name, "dosage") == 0 && p->type == MCP_PROPERTY_TYPE_STRING)
            dosage = p->default_val.str_val;
        else if (strcmp(p->name, "hour") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            hour = p->default_val.int_val;
        else if (strcmp(p->name, "minute") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            minute = p->default_val.int_val;
    }
    if (!name) { wukong_mcp_return_value_set_bool(ret_val, FALSE); return OPRT_OK; }
    int id = 0;
    OPERATE_RET rt = app_dashboard_medicine_add(name, dosage, (uint8_t)hour, (uint8_t)minute, &id);
    ty_cJSON *json = ty_cJSON_CreateObject();
    if (json) {
        ty_cJSON_AddBoolToObject(json, "success", rt == OPRT_OK ? TRUE : FALSE);
        ty_cJSON_AddNumberToObject(json, "id", id);
        wukong_mcp_return_value_set_json(ret_val, json);
    }
    return OPRT_OK;
}

static OPERATE_RET __dashboard_medicine_delete(const MCP_PROPERTY_LIST_T *properties, MCP_RETURN_VALUE_T *ret_val, void *user_data)
{
    int id = -1;
    for (int i = 0; i < properties->count; i++) {
        MCP_PROPERTY_T *p = properties->properties[i];
        if (strcmp(p->name, "id") == 0 && p->type == MCP_PROPERTY_TYPE_INTEGER)
            id = p->default_val.int_val;
    }
    OPERATE_RET rt = (id >= 0) ? app_dashboard_medicine_delete(id) : OPRT_COM_ERROR;
    wukong_mcp_return_value_set_bool(ret_val, rt == OPRT_OK ? TRUE : FALSE);
    return OPRT_OK;
}
#endif /* ENABLE_DASHBOARD */

static OPERATE_RET __app_mcp_init(void *data)
{
    OPERATE_RET rt = OPRT_OK;

    wukong_mcp_server_init("Tuya MCP Server", "1.0");
    tuya_ai_agent_mcp_set_cb(wukong_mcp_server_parse_message, NULL);

    // device.info.get tool
    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("device.info.get",
                                           "Get device information such as model, and firmware version.",
                                           __get_device_info, NULL),
                       __ERR);

#if defined(ENABLE_EX_MODULE_CAMERA) && (ENABLE_EX_MODULE_CAMERA == 1)
    // device.camera.take_photo tool
    TUYA_CALL_ERR_GOTO(
        WUKONG_MCP_TOOL_ADD("device.camera.take_photo",
                            "Activates the device's camera to capture one or more photos.\n"
                            "Parameters:\n"
                            "- count (int): Number of photos to capture (1-10).\n"
                            "Response:\n"
                            "- Returns the captured photos encoded in Base64 format.",
                            __take_photo, NULL, MCP_PROP_STR("question", "The question prompting the photo capture."),
                            MCP_PROP_INT_DEF_RANGE("count", "Number of photos to capture (1-10).", 1, 1, 10)),
        __ERR);
#endif

    // device.audio.volume_set
    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("device.audio.volume_set",
                                           "Sets the device's volume level.\n"
                                           "Parameters:\n"
                                           "- volume (int): The volume level to set (0-100).\n"
                                           "Response:\n"
                                           "- Returns true if the volume was set successfully.",
                                           __set_volume, NULL,
                                           MCP_PROP_INT_RANGE("volume", "The volume level to set (0-100).", 0, 100)),
                       __ERR);

#if defined(ENABLE_ALARM) && (ENABLE_ALARM == 1)
    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("device.alarm.set",
                                           "Sets the daily wake-up alarm.\n"
                                           "Parameters: hour (0-23), minute (0-59), enabled (bool).",
                                           __set_alarm, NULL,
                                           MCP_PROP_INT_RANGE("hour", "Hour (0-23).", 0, 23),
                                           MCP_PROP_INT_RANGE("minute", "Minute (0-59).", 0, 59),
                                           MCP_PROP_BOOL_DEF("enabled", "Enable or disable.", TRUE)),
                       __ERR);
    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("device.alarm.set_in_minutes",
                                           "Sets an alarm to ring in N minutes from now.\n"
                                           "The alarm rings continuously until the STOP button is pressed.\n"
                                           "Parameter: minutes (1-1440).",
                                           __set_alarm_in_minutes, NULL,
                                           MCP_PROP_INT_RANGE("minutes", "Minutes from now (1-1440).", 1, 1440)),
                       __ERR);
#endif

#if defined(ENABLE_DASHBOARD) && (ENABLE_DASHBOARD == 1)
    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("dashboard.weather.set",
                                           "Set weather on dashboard. Parameters: city, condition, temp (°C), humidity (%).",
                                           __dashboard_weather_set, NULL,
                                           MCP_PROP_STR("city", "City name."),
                                           MCP_PROP_STR("condition", "Weather condition."),
                                           MCP_PROP_INT_RANGE("temp", "Temperature in Celsius.", -50, 60),
                                           MCP_PROP_INT_RANGE("humidity", "Humidity 0-100.", 0, 100)),
                       __ERR);

    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("dashboard.todo.add",
                                           "Add a todo item. Returns {success, id}.",
                                           __dashboard_todo_add, NULL,
                                           MCP_PROP_STR("text", "Todo text.")),
                       __ERR);

    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("dashboard.todo.complete",
                                           "Mark a todo item as done.",
                                           __dashboard_todo_complete, NULL,
                                           MCP_PROP_INT_RANGE("id", "Item ID.", 0, 9999)),
                       __ERR);

    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("dashboard.todo.delete",
                                           "Delete a todo item.",
                                           __dashboard_todo_delete, NULL,
                                           MCP_PROP_INT_RANGE("id", "Item ID.", 0, 9999)),
                       __ERR);

    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("dashboard.medicine.add",
                                           "Add a medicine reminder. Returns {success, id}.",
                                           __dashboard_medicine_add, NULL,
                                           MCP_PROP_STR("name", "Medicine name."),
                                           MCP_PROP_STR("dosage", "Dosage, e.g. '1 tablet'."),
                                           MCP_PROP_INT_RANGE("hour", "Hour (0-23).", 0, 23),
                                           MCP_PROP_INT_RANGE("minute", "Minute (0-59).", 0, 59)),
                       __ERR);

    TUYA_CALL_ERR_GOTO(WUKONG_MCP_TOOL_ADD("dashboard.medicine.delete",
                                           "Delete a medicine reminder.",
                                           __dashboard_medicine_delete, NULL,
                                           MCP_PROP_INT_RANGE("id", "Reminder ID.", 0, 9999)),
                       __ERR);
#endif /* ENABLE_DASHBOARD */

    PR_DEBUG("app_mcp_init success");
    return rt;

__ERR:
    app_mcp_deinit();
    return rt;
}

OPERATE_RET app_mcp_init(void)
{
    return tal_event_subscribe(EVENT_MQTT_CONNECTED, "app_mcp_init", __app_mcp_init, SUBSCRIBE_TYPE_ONETIME);
}

OPERATE_RET app_mcp_deinit(void)
{
    wukong_mcp_server_destroy();
    PR_DEBUG("APP MCP deinit success");
    return OPRT_OK;
}
