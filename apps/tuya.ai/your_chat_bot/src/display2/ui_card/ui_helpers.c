#include "ui_helpers.h"

void _ui_screen_change(lv_obj_t **target, lv_screen_load_anim_t fademode,
                       int spd, int delay, void (*target_init)(void))
{
    if (*target == NULL) {
        target_init();
    }
    lv_screen_load_anim(*target, fademode, spd, delay, false);
}
