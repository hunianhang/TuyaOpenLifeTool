#ifndef _UI_CARD_CHAT_H
#define _UI_CARD_CHAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

extern lv_obj_t *ui_chat;

void ui_chat_screen_init(void);
void ui_chat_screen_destroy(void);
void ui_chat_append_user_msg(const char *msg);
void ui_chat_append_ai_msg(const char *msg);
void ui_chat_set_status(const char *status);

#ifdef __cplusplus
}
#endif

#endif
