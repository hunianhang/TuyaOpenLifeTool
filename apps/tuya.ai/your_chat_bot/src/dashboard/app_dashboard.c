#include "app_dashboard.h"
#include "tal_api.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

#if defined(ENABLE_CHAT_DISPLAY2) && (ENABLE_CHAT_DISPLAY2 == 1)
#include "app_display.h"
#endif

#define KV_KEY_WEATHER "dash_weather_v1"
#define KV_KEY_TODO    "dash_todo_v1"
#define KV_KEY_MED     "dash_med_v1"

/* ── In-memory state ─────────────────────────────────────── */
static DASH_WEATHER_T  sg_weather  = {0};

static DASH_TODO_T     sg_todos[DASH_TODO_MAX];
static int             sg_todo_count   = 0;
static int             sg_todo_next_id = 1;

static DASH_MEDICINE_T sg_meds[DASH_MED_MAX];
static int             sg_med_count   = 0;
static int             sg_med_next_id = 1;

/* ── Helper: trigger UI refresh ──────────────────────────── */
static void __notify_refresh(void)
{
#if defined(ENABLE_CHAT_DISPLAY2) && (ENABLE_CHAT_DISPLAY2 == 1)
    app_display_send_msg(TY_DISPLAY_TP_SYSTEM_MSG, (uint8_t *)"", 0);
#endif
}

/* ── Weather persistence ─────────────────────────────────── */
static void __weather_save(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddStringToObject(root, "city",      sg_weather.city);
    cJSON_AddStringToObject(root, "cond",      sg_weather.condition);
    cJSON_AddNumberToObject(root, "temp",      sg_weather.temp_c);
    cJSON_AddNumberToObject(root, "hum",       sg_weather.humidity);
    cJSON_AddBoolToObject(root,   "valid",     sg_weather.valid);
    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!str) return;
    tal_kv_set(KV_KEY_WEATHER, (uint8_t *)str, strlen(str));
    tal_free(str);
}

static void __weather_load(void)
{
    uint8_t *buf = NULL;
    size_t   len = 0;
    if (OPRT_OK != tal_kv_get(KV_KEY_WEATHER, &buf, &len)) return;
    cJSON *root = cJSON_ParseWithLength((char *)buf, len);
    tal_kv_free(buf);
    if (!root) return;
    cJSON *city  = cJSON_GetObjectItem(root, "city");
    cJSON *cond  = cJSON_GetObjectItem(root, "cond");
    cJSON *temp  = cJSON_GetObjectItem(root, "temp");
    cJSON *hum   = cJSON_GetObjectItem(root, "hum");
    cJSON *valid = cJSON_GetObjectItem(root, "valid");
    if (cJSON_IsString(city))  snprintf(sg_weather.city,      sizeof(sg_weather.city),      "%s", city->valuestring);
    if (cJSON_IsString(cond))  snprintf(sg_weather.condition, sizeof(sg_weather.condition), "%s", cond->valuestring);
    if (cJSON_IsNumber(temp))  sg_weather.temp_c   = (int)temp->valuedouble;
    if (cJSON_IsNumber(hum))   sg_weather.humidity = (int)hum->valuedouble;
    if (cJSON_IsBool(valid))   sg_weather.valid    = cJSON_IsTrue(valid);
    cJSON_Delete(root);
}

/* ── Todo persistence ────────────────────────────────────── */
static void __todo_save(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddNumberToObject(root, "next_id", sg_todo_next_id);
    cJSON *arr = cJSON_AddArrayToObject(root, "items");
    for (int i = 0; i < sg_todo_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id",   sg_todos[i].id);
        cJSON_AddBoolToObject(item,   "done", sg_todos[i].done);
        cJSON_AddStringToObject(item, "text", sg_todos[i].text);
        cJSON_AddItemToArray(arr, item);
    }
    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!str) return;
    tal_kv_set(KV_KEY_TODO, (uint8_t *)str, strlen(str));
    tal_free(str);
}

static void __todo_load(void)
{
    uint8_t *buf = NULL;
    size_t   len = 0;
    if (OPRT_OK != tal_kv_get(KV_KEY_TODO, &buf, &len)) return;
    cJSON *root = cJSON_ParseWithLength((char *)buf, len);
    tal_kv_free(buf);
    if (!root) return;
    cJSON *nid = cJSON_GetObjectItem(root, "next_id");
    if (cJSON_IsNumber(nid)) sg_todo_next_id = (int)nid->valuedouble;
    cJSON *arr = cJSON_GetObjectItem(root, "items");
    int cnt = cJSON_GetArraySize(arr);
    sg_todo_count = 0;
    for (int i = 0; i < cnt && sg_todo_count < DASH_TODO_MAX; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *id   = cJSON_GetObjectItem(item, "id");
        cJSON *done = cJSON_GetObjectItem(item, "done");
        cJSON *text = cJSON_GetObjectItem(item, "text");
        if (!cJSON_IsNumber(id) || !cJSON_IsString(text)) continue;
        sg_todos[sg_todo_count].id   = (int)id->valuedouble;
        sg_todos[sg_todo_count].done = cJSON_IsTrue(done);
        snprintf(sg_todos[sg_todo_count].text, sizeof(sg_todos[0].text), "%s", text->valuestring);
        sg_todo_count++;
    }
    cJSON_Delete(root);
    PR_DEBUG("[dashboard] loaded %d todos", sg_todo_count);
}

/* ── Medicine persistence ────────────────────────────────── */
static void __med_save(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddNumberToObject(root, "next_id", sg_med_next_id);
    cJSON *arr = cJSON_AddArrayToObject(root, "items");
    for (int i = 0; i < sg_med_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "id",     sg_meds[i].id);
        cJSON_AddNumberToObject(item, "hour",   sg_meds[i].hour);
        cJSON_AddNumberToObject(item, "minute", sg_meds[i].minute);
        cJSON_AddStringToObject(item, "name",   sg_meds[i].name);
        cJSON_AddStringToObject(item, "dosage", sg_meds[i].dosage);
        cJSON_AddItemToArray(arr, item);
    }
    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!str) return;
    tal_kv_set(KV_KEY_MED, (uint8_t *)str, strlen(str));
    tal_free(str);
}

static void __med_load(void)
{
    uint8_t *buf = NULL;
    size_t   len = 0;
    if (OPRT_OK != tal_kv_get(KV_KEY_MED, &buf, &len)) return;
    cJSON *root = cJSON_ParseWithLength((char *)buf, len);
    tal_kv_free(buf);
    if (!root) return;
    cJSON *nid = cJSON_GetObjectItem(root, "next_id");
    if (cJSON_IsNumber(nid)) sg_med_next_id = (int)nid->valuedouble;
    cJSON *arr = cJSON_GetObjectItem(root, "items");
    int cnt = cJSON_GetArraySize(arr);
    sg_med_count = 0;
    for (int i = 0; i < cnt && sg_med_count < DASH_MED_MAX; i++) {
        cJSON *item   = cJSON_GetArrayItem(arr, i);
        cJSON *id     = cJSON_GetObjectItem(item, "id");
        cJSON *hour   = cJSON_GetObjectItem(item, "hour");
        cJSON *minute = cJSON_GetObjectItem(item, "minute");
        cJSON *name   = cJSON_GetObjectItem(item, "name");
        cJSON *dosage = cJSON_GetObjectItem(item, "dosage");
        if (!cJSON_IsNumber(id) || !cJSON_IsString(name)) continue;
        sg_meds[sg_med_count].id     = (int)id->valuedouble;
        sg_meds[sg_med_count].hour   = cJSON_IsNumber(hour)   ? (uint32_t)hour->valuedouble   : 8;
        sg_meds[sg_med_count].minute = cJSON_IsNumber(minute) ? (uint32_t)minute->valuedouble : 0;
        snprintf(sg_meds[sg_med_count].name,   sizeof(sg_meds[0].name),   "%s", name->valuestring);
        snprintf(sg_meds[sg_med_count].dosage, sizeof(sg_meds[0].dosage), "%s",
                 cJSON_IsString(dosage) ? dosage->valuestring : "1");
        sg_med_count++;
    }
    cJSON_Delete(root);
    PR_DEBUG("[dashboard] loaded %d medicines", sg_med_count);
}

/* ── Public API ──────────────────────────────────────────── */

OPERATE_RET app_dashboard_init(void)
{
    memset(&sg_weather, 0, sizeof(sg_weather));
    memset(sg_todos,    0, sizeof(sg_todos));
    memset(sg_meds,     0, sizeof(sg_meds));
    sg_todo_count   = 0;
    sg_todo_next_id = 1;
    sg_med_count    = 0;
    sg_med_next_id  = 1;

    __weather_load();
    __todo_load();
    __med_load();
    return OPRT_OK;
}

/* Weather */
OPERATE_RET app_dashboard_weather_set(const char *city, const char *condition, int temp_c, int humidity)
{
    if (!city || !condition) return OPRT_INVALID_PARM;
    snprintf(sg_weather.city,      sizeof(sg_weather.city),      "%s", city);
    snprintf(sg_weather.condition, sizeof(sg_weather.condition), "%s", condition);
    sg_weather.temp_c   = temp_c;
    sg_weather.humidity = humidity;
    sg_weather.valid    = true;
    __weather_save();
    __notify_refresh();
    return OPRT_OK;
}

const DASH_WEATHER_T *app_dashboard_weather_get(void)
{
    return &sg_weather;
}

/* Todo */
int app_dashboard_todo_count(void) { return sg_todo_count; }

const DASH_TODO_T *app_dashboard_todo_get(int idx)
{
    if (idx < 0 || idx >= sg_todo_count) return NULL;
    return &sg_todos[idx];
}

OPERATE_RET app_dashboard_todo_add(const char *text, int *id_out)
{
    if (!text || sg_todo_count >= DASH_TODO_MAX) return OPRT_COM_ERROR;
    DASH_TODO_T *t = &sg_todos[sg_todo_count];
    t->id   = sg_todo_next_id++;
    t->done = false;
    snprintf(t->text, sizeof(t->text), "%s", text);
    sg_todo_count++;
    if (id_out) *id_out = t->id;
    __todo_save();
    __notify_refresh();
    return OPRT_OK;
}

OPERATE_RET app_dashboard_todo_complete(int id)
{
    for (int i = 0; i < sg_todo_count; i++) {
        if (sg_todos[i].id == id) {
            sg_todos[i].done = true;
            __todo_save();
            __notify_refresh();
            return OPRT_OK;
        }
    }
    return OPRT_COM_ERROR;
}

OPERATE_RET app_dashboard_todo_delete(int id)
{
    for (int i = 0; i < sg_todo_count; i++) {
        if (sg_todos[i].id == id) {
            for (int j = i; j < sg_todo_count - 1; j++) sg_todos[j] = sg_todos[j + 1];
            sg_todo_count--;
            __todo_save();
            __notify_refresh();
            return OPRT_OK;
        }
    }
    return OPRT_COM_ERROR;
}

/* Medicine */
int app_dashboard_medicine_count(void) { return sg_med_count; }

const DASH_MEDICINE_T *app_dashboard_medicine_get(int idx)
{
    if (idx < 0 || idx >= sg_med_count) return NULL;
    return &sg_meds[idx];
}

OPERATE_RET app_dashboard_medicine_add(const char *name, const char *dosage,
                                       uint8_t hour, uint8_t minute, int *id_out)
{
    if (!name || sg_med_count >= DASH_MED_MAX) return OPRT_COM_ERROR;
    DASH_MEDICINE_T *m = &sg_meds[sg_med_count];
    m->id     = sg_med_next_id++;
    m->hour   = hour;
    m->minute = minute;
    snprintf(m->name,   sizeof(m->name),   "%s", name);
    snprintf(m->dosage, sizeof(m->dosage), "%s", dosage ? dosage : "1");
    sg_med_count++;
    if (id_out) *id_out = m->id;
    __med_save();
    __notify_refresh();
    return OPRT_OK;
}

OPERATE_RET app_dashboard_medicine_delete(int id)
{
    for (int i = 0; i < sg_med_count; i++) {
        if (sg_meds[i].id == id) {
            for (int j = i; j < sg_med_count - 1; j++) sg_meds[j] = sg_meds[j + 1];
            sg_med_count--;
            __med_save();
            __notify_refresh();
            return OPRT_OK;
        }
    }
    return OPRT_COM_ERROR;
}
