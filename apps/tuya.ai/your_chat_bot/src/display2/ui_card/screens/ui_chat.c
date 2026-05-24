/**
 * @file ui_chat.c
 * @brief Chat screen with message bubbles and back navigation.
 */

#include "../ui.h"
#include "app_ui_helper.h"
#include <string.h>

#define C_BG        lv_color_hex(0x141B27)
#define C_TOPBAR    lv_color_hex(0x0E1420)
#define C_CARD      lv_color_hex(0x1C2537)
#define C_BUBBLE_U  lv_color_hex(0x1A4A80)   /* user bubble */
#define C_BUBBLE_AI lv_color_hex(0x1C2537)   /* AI bubble */
#define C_TEXT      lv_color_hex(0xE8EDF2)
#define C_TEXT_DIM  lv_color_hex(0x7F8FA6)
#define C_ACCENT    lv_color_hex(0x6C3483)

lv_obj_t *ui_chat = NULL;

static lv_obj_t *s_msg_area   = NULL;
static lv_obj_t *s_status_lbl = NULL;

/* ── Scroll to bottom after new message ──────────────────── */
static void __scroll_bottom_cb(lv_timer_t *t)
{
    (void)t;
    if (s_msg_area) {
        lv_obj_scroll_to_y(s_msg_area, LV_COORD_MAX, LV_ANIM_ON);
    }
}

/* ── Navigation ──────────────────────────────────────────── */
static void __back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        _ui_screen_change(&ui_home, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 280, 0, &ui_home_screen_init);
    }
}

/* ── Screen events ───────────────────────────────────────── */
static void __chat_loaded_cb(lv_event_t *e)
{
    (void)e;
    app_ui_network_status_change_subscribe();
}

static void __chat_unloaded_cb(lv_event_t *e)
{
    (void)e;
    app_ui_network_status_change_unsubscribe();
}

/* ── Screen init ─────────────────────────────────────────── */
void ui_chat_screen_init(void)
{
    ui_chat = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_chat, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_chat, C_BG, 0);
    lv_obj_set_style_bg_opa(ui_chat, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(ui_chat, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_chat, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(ui_chat, 0, 0);
    lv_obj_add_event_cb(ui_chat, __chat_loaded_cb,   LV_EVENT_SCREEN_LOADED,   NULL);
    lv_obj_add_event_cb(ui_chat, __chat_unloaded_cb, LV_EVENT_SCREEN_UNLOADED, NULL);

    /* ── Top bar: back | title | status ── */
    lv_obj_t *topbar = lv_obj_create(ui_chat);
    lv_obj_remove_style_all(topbar);
    lv_obj_set_width(topbar, lv_pct(100));
    lv_obj_set_height(topbar, 44);
    lv_obj_set_style_bg_color(topbar, C_TOPBAR, 0);
    lv_obj_set_style_bg_opa(topbar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(topbar, 8, 0);
    lv_obj_set_style_pad_ver(topbar, 0, 0);
    lv_obj_set_flex_flow(topbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    /* back button */
    lv_obj_t *back_btn = lv_obj_create(topbar);
    lv_obj_remove_style_all(back_btn);
    lv_obj_set_width(back_btn, 60);
    lv_obj_set_height(back_btn, lv_pct(100));
    lv_obj_set_style_bg_color(back_btn, C_ACCENT, 0);
    lv_obj_set_style_bg_opa(back_btn, 60, 0);
    lv_obj_set_style_radius(back_btn, 6, 0);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(back_btn, __back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(back_lbl);

    /* title */
    lv_obj_t *title = lv_label_create(topbar);
    lv_label_set_text(title, "AI Chat");
    lv_obj_set_flex_grow(title, 1);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(title, C_TEXT, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);

    /* status (right) */
    s_status_lbl = lv_label_create(topbar);
    lv_label_set_text(s_status_lbl, "");
    lv_obj_set_width(s_status_lbl, 70);
    lv_obj_set_style_text_align(s_status_lbl, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(s_status_lbl, C_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_14, 0);

    /* ── Scrollable message area ── */
    s_msg_area = lv_obj_create(ui_chat);
    lv_obj_remove_style_all(s_msg_area);
    lv_obj_set_width(s_msg_area, lv_pct(100));
    lv_obj_set_flex_grow(s_msg_area, 1);
    lv_obj_set_style_bg_color(s_msg_area, C_BG, 0);
    lv_obj_set_style_bg_opa(s_msg_area, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_msg_area, 8, 0);
    lv_obj_set_style_pad_row(s_msg_area, 6, 0);
    lv_obj_set_flex_flow(s_msg_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_msg_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(s_msg_area, LV_DIR_VER);
}

void ui_chat_screen_destroy(void)
{
    s_msg_area = s_status_lbl = NULL;
    if (ui_chat) { lv_obj_del(ui_chat); ui_chat = NULL; }
}

/* ── Message bubble helper ───────────────────────────────── */
static void __append_bubble(const char *text, bool is_user)
{
    if (!s_msg_area || !text) return;

    /* row: aligns bubble to left (AI) or right (user) */
    lv_obj_t *row = lv_obj_create(s_msg_area);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    if (is_user) {
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    } else {
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    }

    lv_obj_t *bubble = lv_obj_create(row);
    lv_obj_remove_style_all(bubble);
    lv_obj_set_width(bubble, lv_pct(82));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(bubble, is_user ? C_BUBBLE_U : C_BUBBLE_AI, 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bubble, 8, 0);
    lv_obj_set_style_pad_all(bubble, 8, 0);
    lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(bubble);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, lv_pct(100));
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    lv_timer_t *st = lv_timer_create(__scroll_bottom_cb, 80, NULL);
    lv_timer_set_repeat_count(st, 1);
}

void ui_chat_append_user_msg(const char *msg)
{
    __append_bubble(msg, true);
}

void ui_chat_append_ai_msg(const char *msg)
{
    __append_bubble(msg, false);
}

void ui_chat_set_status(const char *status)
{
    if (s_status_lbl && status) {
        lv_label_set_text(s_status_lbl, status);
    }
}
