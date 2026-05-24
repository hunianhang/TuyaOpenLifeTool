#ifndef _UI_CARD_H
#define _UI_CARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"
#include "ui_helpers.h"
#include "screens/ui_home.h"
#include "screens/ui_chat.h"

void ui_init(void);
void ui_destroy(void);

#ifdef __cplusplus
}
#endif

#endif
