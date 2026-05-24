#include "app_alarm.h"
#include "tal_api.h"
#include "tal_time_service.h"
#include "cJSON.h"
#include "app_chat_bot.h"

#if defined(ENABLE_CHAT_DISPLAY2) && (ENABLE_CHAT_DISPLAY2 == 1)
#include "app_display.h"
#endif

#define KV_KEY "app_alarm_v1"

typedef struct {
    int  hour;
    int  minute;
    bool enabled;
    bool triggered_today;
} alarm_state_t;

static alarm_state_t sg_alarm      = {7, 0, false, false};
static TIMER_ID      sg_check_tmr  = NULL;

static void __save(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddNumberToObject(root, "h", sg_alarm.hour);
    cJSON_AddNumberToObject(root, "m", sg_alarm.minute);
    cJSON_AddBoolToObject(root,   "e", sg_alarm.enabled);
    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!str) return;
    tal_kv_set(KV_KEY, (uint8_t *)str, strlen(str));
    tal_free(str);
}

static void __load(void)
{
    uint8_t *buf = NULL;
    size_t   len = 0;
    if (OPRT_OK != tal_kv_get(KV_KEY, &buf, &len)) return;
    cJSON *root = cJSON_ParseWithLength((char *)buf, len);
    tal_kv_free(buf);
    if (!root) return;
    cJSON *h = cJSON_GetObjectItem(root, "h");
    cJSON *m = cJSON_GetObjectItem(root, "m");
    cJSON *e = cJSON_GetObjectItem(root, "e");
    if (cJSON_IsNumber(h)) sg_alarm.hour    = (int)h->valuedouble;
    if (cJSON_IsNumber(m)) sg_alarm.minute  = (int)m->valuedouble;
    if (cJSON_IsBool(e))   sg_alarm.enabled = cJSON_IsTrue(e);
    cJSON_Delete(root);
    PR_DEBUG("[alarm] loaded: %02d:%02d enabled=%d", sg_alarm.hour, sg_alarm.minute, sg_alarm.enabled);
}

static void __check_timer_cb(TIMER_ID id, void *arg)
{
    if (!sg_alarm.enabled) return;
    if (OPRT_OK != tal_time_check_time_sync()) return;

    POSIX_TM_S tm = {0};
    tal_time_get_local_time_custom(0, &tm);

    if (tm.tm_hour == sg_alarm.hour && tm.tm_min == sg_alarm.minute) {
        if (!sg_alarm.triggered_today) {
            sg_alarm.triggered_today = true;
            ai_audio_player_play_alert(AI_AUDIO_ALERT_WAKEUP);
#if defined(ENABLE_CHAT_DISPLAY2) && (ENABLE_CHAT_DISPLAY2 == 1)
            char msg[32];
            snprintf(msg, sizeof(msg), "Alarm! %02d:%02d", sg_alarm.hour, sg_alarm.minute);
            app_display_send_msg(TY_DISPLAY_TP_SYSTEM_MSG, (uint8_t *)msg, strlen(msg));
#endif
            PR_NOTICE("[alarm] fired at %02d:%02d", sg_alarm.hour, sg_alarm.minute);
        }
    } else {
        sg_alarm.triggered_today = false;
    }
}

OPERATE_RET app_alarm_set(uint8_t hour, uint8_t minute, bool enabled)
{
    if (hour > 23 || minute > 59) return OPRT_INVALID_PARM;
    sg_alarm.hour            = hour;
    sg_alarm.minute          = minute;
    sg_alarm.enabled         = enabled;
    sg_alarm.triggered_today = false;
    __save();
    PR_DEBUG("[alarm] set %02d:%02d enabled=%d", hour, minute, enabled);
    return OPRT_OK;
}

void app_alarm_upload(void)
{
#if defined(ENABLE_CHAT_DISPLAY2) && (ENABLE_CHAT_DISPLAY2 == 1)
    if (!sg_alarm.enabled) return;
    char msg[36];
    snprintf(msg, sizeof(msg), "Alarm set %02d:%02d", sg_alarm.hour, sg_alarm.minute);
    app_display_send_msg(TY_DISPLAY_TP_SYSTEM_MSG, (uint8_t *)msg, strlen(msg));
#endif
}

OPERATE_RET app_alarm_init(void)
{
    memset(&sg_alarm, 0, sizeof(sg_alarm));
    sg_alarm.hour    = 7;
    sg_alarm.enabled = false;
    __load();

    OPERATE_RET rt = tal_sw_timer_create(__check_timer_cb, NULL, &sg_check_tmr);
    if (rt != OPRT_OK) return rt;
    return tal_sw_timer_start(sg_check_tmr, 60 * 1000, TAL_TIMER_CYCLE);
}
