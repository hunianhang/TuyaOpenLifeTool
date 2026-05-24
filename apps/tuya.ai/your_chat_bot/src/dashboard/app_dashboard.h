#ifndef __APP_DASHBOARD_H__
#define __APP_DASHBOARD_H__

#include "tuya_cloud_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DASH_TODO_MAX  20
#define DASH_MED_MAX   20

typedef struct {
    bool valid;
    char city[32];
    char condition[32];
    int  temp_c;
    int  humidity;
} DASH_WEATHER_T;

typedef struct {
    int  id;
    bool done;
    char text[128];
} DASH_TODO_T;

typedef struct {
    int      id;
    uint32_t hour;
    uint32_t minute;
    char     name[64];
    char     dosage[32];
} DASH_MEDICINE_T;

OPERATE_RET app_dashboard_init(void);

OPERATE_RET             app_dashboard_weather_set(const char *city, const char *condition, int temp_c, int humidity);
const DASH_WEATHER_T   *app_dashboard_weather_get(void);

int                     app_dashboard_todo_count(void);
const DASH_TODO_T      *app_dashboard_todo_get(int idx);
OPERATE_RET             app_dashboard_todo_add(const char *text, int *id_out);
OPERATE_RET             app_dashboard_todo_complete(int id);
OPERATE_RET             app_dashboard_todo_delete(int id);

int                     app_dashboard_medicine_count(void);
const DASH_MEDICINE_T  *app_dashboard_medicine_get(int idx);
OPERATE_RET             app_dashboard_medicine_add(const char *name, const char *dosage,
                                                   uint8_t hour, uint8_t minute, int *id_out);
OPERATE_RET             app_dashboard_medicine_delete(int id);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DASHBOARD_H__ */
