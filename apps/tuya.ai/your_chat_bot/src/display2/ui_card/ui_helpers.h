#ifndef _UI_CARD_HELPERS_H
#define _UI_CARD_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

void _ui_screen_change(lv_obj_t **target, lv_screen_load_anim_t fademode,
                       int spd, int delay, void (*target_init)(void));

#ifdef __cplusplus
}
#endif

#endif
