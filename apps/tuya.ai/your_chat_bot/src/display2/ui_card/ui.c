#include "ui.h"

void ui_init(void)
{
    ui_home_screen_init();
    lv_screen_load(ui_home);
}

void ui_destroy(void)
{
    ui_home_screen_destroy();
    ui_chat_screen_destroy();
}
