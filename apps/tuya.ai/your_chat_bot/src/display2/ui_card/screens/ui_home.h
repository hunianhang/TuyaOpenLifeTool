#ifndef _UI_CARD_HOME_H
#define _UI_CARD_HOME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

extern lv_obj_t *ui_home;

void ui_home_screen_init(void);
void ui_home_screen_destroy(void);

/* Called by app_display.c message dispatcher */
void ui_set_emotion(const char *emotion);
void ui_set_device_status(const char *status);
void ui_set_user_msg(const char *msg);
void ui_set_assistant_msg(const char *msg);
void ui_set_system_msg(const char *msg);
void ui_dashboard_refresh(void);
void ui_alarm_stop_show(bool show);

#ifdef __cplusplus
}
#endif

#endif
