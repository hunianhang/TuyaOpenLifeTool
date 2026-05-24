/**
 * @file ui_home.c
 * @brief Card-style dashboard home screen for 320x480 portrait displays.
 *        Layout: status bar → date → weather card → todo card → medicine card → chat button
 */

#include "../ui.h"
#include "app_ui_helper.h"
#include <stdio.h>
#include <string.h>

#if defined(ENABLE_DASHBOARD) && (ENABLE_DASHBOARD == 1)
#include "app_dashboard.h"
#endif

#if defined(ENABLE_ALARM) && (ENABLE_ALARM == 1)
#include "app_alarm.h"
#endif

/* ── Color palette ───────────────────────────────────────── */
#define C_BG        lv_color_hex(0x141B27)
#define C_CARD      lv_color_hex(0x1C2537)
#define C_TOPBAR    lv_color_hex(0x0E1420)
#define C_TEXT      lv_color_hex(0xE8EDF2)
#define C_TEXT_DIM  lv_color_hex(0x7F8FA6)
#define C_WEATHER   lv_color_hex(0x2471A3)
#define C_TODO      lv_color_hex(0x1E8449)
#define C_MED       lv_color_hex(0xCA6F1E)
#define C_CHAT_BTN  lv_color_hex(0x6C3483)
#define C_NOTIFY    lv_color_hex(0xE74C3C)

/* ── Widget handles ──────────────────────────────────────── */
lv_obj_t *ui_home = NULL;

static lv_obj_t *s_status_lbl  = NULL;
static lv_obj_t *s_time_lbl    = NULL;
static lv_obj_t *s_date_lbl    = NULL;
static lv_obj_t *s_notify_lbl  = NULL;
static lv_obj_t *s_weather_lbl = NULL;
static lv_obj_t *s_todo_lbl    = NULL;
static lv_obj_t *s_med_lbl     = NULL;

static lv_timer_t *s_clock_tmr     = NULL;
static lv_timer_t *s_notify_tmr    = NULL;
static lv_obj_t   *s_alarm_overlay = NULL;

/* ── Helpers ─────────────────────────────────────────────── */
static const char *__month_abbr(uint32_t m)
{
    static const char *t[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    return (m >= 1 && m <= 12) ? t[m - 1] : "?";
}

static const char *__weekday(uint32_t y, uint32_t m, uint32_t d)
{
    static const char *t[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    if (m < 3) { m += 12; y--; }
    int k = (int)(y % 100), j = (int)(y / 100);
    int h = (int)(d + 13*(m+1)/5 + k + k/4 + j/4 + 5*j) % 7;
    return t[((h + 5) % 7)];
}

/* ── Timers ──────────────────────────────────────────────── */
static void __clock_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_time_lbl) return;
    uint32_t h = 0, m = 0;
    app_ui_get_time(&h, &m);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02u:%02u", h, m);
    lv_label_set_text(s_time_lbl, buf);
}

static void __notify_clear_cb(lv_timer_t *t)
{
    (void)t;
    if (s_notify_lbl) lv_label_set_text(s_notify_lbl, "");
    s_notify_tmr = NULL;
}

/* ── Card builder ────────────────────────────────────────── */
static lv_obj_t *__make_card(lv_obj_t *parent, lv_color_t accent, const char *title)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_set_style_pad_top(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* top accent bar */
    lv_obj_t *bar = lv_obj_create(card);
    lv_obj_remove_style_all(bar);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 3);
    lv_obj_set_style_bg_color(bar, accent, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 2, 0);

    /* title */
    lv_obj_t *ttl = lv_label_create(card);
    lv_label_set_text(ttl, title);
    lv_obj_set_style_text_color(ttl, accent, 0);
    lv_obj_set_style_text_font(ttl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_top(ttl, 3, 0);

    /* content label (returned for caller to populate) */
    lv_obj_t *content = lv_label_create(card);
    lv_label_set_long_mode(content, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_style_text_color(content, C_TEXT, 0);
    lv_obj_set_style_text_font(content, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_top(content, 4, 0);

    return content;
}

/* ── Dashboard data refresh ──────────────────────────────── */
static void __refresh_weather(void)
{
    if (!s_weather_lbl) return;
#if defined(ENABLE_DASHBOARD) && (ENABLE_DASHBOARD == 1)
    const DASH_WEATHER_T *w = app_dashboard_weather_get();
    if (w && w->valid) {
        char buf[80];
        snprintf(buf, sizeof(buf), "%s\n%s   %d" "\xc2\xb0" "C   Humidity %d%%",
                 w->city, w->condition, w->temp_c, w->humidity);
        lv_label_set_text(s_weather_lbl, buf);
    } else {
        lv_label_set_text(s_weather_lbl, "No weather data\nAsk AI: \"What's the weather in...\"");
    }
#else
    lv_label_set_text(s_weather_lbl, "Dashboard not enabled");
#endif
}

static void __refresh_todo(void)
{
    if (!s_todo_lbl) return;
#if defined(ENABLE_DASHBOARD) && (ENABLE_DASHBOARD == 1)
    int cnt = app_dashboard_todo_count();
    if (cnt == 0) {
        lv_label_set_text(s_todo_lbl, "No items.\nAsk AI: \"Add todo: ...\"");
        return;
    }
    char buf[320] = {0};
    int  pos = 0;
    for (int i = 0; i < cnt && pos < (int)sizeof(buf) - 40; i++) {
        const DASH_TODO_T *item = app_dashboard_todo_get(i);
        if (!item) continue;
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "%s %d. %s\n",
                        item->done ? "[x]" : "[ ]",
                        item->id, item->text);
    }
    /* trim trailing newline */
    if (pos > 0 && buf[pos - 1] == '\n') buf[pos - 1] = '\0';
    lv_label_set_text(s_todo_lbl, buf);
#else
    lv_label_set_text(s_todo_lbl, "Dashboard not enabled");
#endif
}

static void __refresh_medicine(void)
{
    if (!s_med_lbl) return;
#if defined(ENABLE_DASHBOARD) && (ENABLE_DASHBOARD == 1)
    int cnt = app_dashboard_medicine_count();
    if (cnt == 0) {
        lv_label_set_text(s_med_lbl, "No reminders.\nAsk AI: \"Add medicine: ...\"");
        return;
    }
    char buf[256] = {0};
    int  pos = 0;
    for (int i = 0; i < cnt && pos < (int)sizeof(buf) - 48; i++) {
        const DASH_MEDICINE_T *m = app_dashboard_medicine_get(i);
        if (!m) continue;
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "%02u:%02u  %s  %s\n",
                        m->hour, m->minute, m->name, m->dosage);
    }
    if (pos > 0 && buf[pos - 1] == '\n') buf[pos - 1] = '\0';
    lv_label_set_text(s_med_lbl, buf);
#else
    lv_label_set_text(s_med_lbl, "Dashboard not enabled");
#endif
}

/* ── Screen event callbacks ──────────────────────────────── */
static void __home_loaded_cb(lv_event_t *e)
{
    (void)e;
    /* refresh time */
    __clock_cb(NULL);

    /* refresh date */
    uint32_t y = 0, mo = 0, d = 0;
    app_ui_get_date(&y, &mo, &d);
    if (y > 0 && s_date_lbl) {
        char buf[28];
        snprintf(buf, sizeof(buf), "%s, %s %u %u",
                 __weekday(y, mo, d), __month_abbr(mo), d, y);
        lv_label_set_text(s_date_lbl, buf);
    }

    /* start 30s clock timer */
    if (!s_clock_tmr) {
        s_clock_tmr = lv_timer_create(__clock_cb, 30000, NULL);
    }

    app_ui_network_status_change_subscribe();
    __refresh_weather();
    __refresh_todo();
    __refresh_medicine();
}

static void __home_unloaded_cb(lv_event_t *e)
{
    (void)e;
    if (s_clock_tmr) {
        lv_timer_del(s_clock_tmr);
        s_clock_tmr = NULL;
    }
    app_ui_network_status_change_unsubscribe();
}

/* ── Alarm stop overlay ──────────────────────────────────── */
static void __alarm_stop_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
#if defined(ENABLE_ALARM) && (ENABLE_ALARM == 1)
    app_alarm_stop();
#endif
}

static lv_obj_t *__make_alarm_overlay(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, lv_pct(100), 60);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xC0392B), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, __alarm_stop_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, LV_SYMBOL_BELL "  STOP ALARM");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl);

    return btn;
}

/* ── Chat button ─────────────────────────────────────────── */
static void __chat_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        _ui_screen_change(&ui_chat, LV_SCR_LOAD_ANIM_MOVE_LEFT, 280, 0, &ui_chat_screen_init);
    }
}

/* ── Screen init ─────────────────────────────────────────── */
void ui_home_screen_init(void)
{
    ui_home = lv_obj_create(NULL);
    lv_obj_remove_flag(ui_home, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_home, C_BG, 0);
    lv_obj_set_style_bg_opa(ui_home, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(ui_home, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_home, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(ui_home, 0, 0);
    lv_obj_add_event_cb(ui_home, __home_loaded_cb,   LV_EVENT_SCREEN_LOADED,   NULL);
    lv_obj_add_event_cb(ui_home, __home_unloaded_cb, LV_EVENT_SCREEN_UNLOADED, NULL);

    /* ── Top bar: wifi | status | time ── */
    lv_obj_t *topbar = lv_obj_create(ui_home);
    lv_obj_remove_style_all(topbar);
    lv_obj_set_width(topbar, lv_pct(100));
    lv_obj_set_height(topbar, 36);
    lv_obj_set_style_bg_color(topbar, C_TOPBAR, 0);
    lv_obj_set_style_bg_opa(topbar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(topbar, 10, 0);
    lv_obj_set_style_pad_ver(topbar, 0, 0);
    lv_obj_set_flex_flow(topbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(topbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *wifi_icon = lv_label_create(topbar);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_icon, C_TODO, 0);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_14, 0);

    s_status_lbl = lv_label_create(topbar);
    lv_label_set_text(s_status_lbl, "STANDBY");
    lv_obj_set_style_text_color(s_status_lbl, C_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_flex_grow(s_status_lbl, 1);
    lv_obj_set_style_text_align(s_status_lbl, LV_TEXT_ALIGN_CENTER, 0);

    s_time_lbl = lv_label_create(topbar);
    lv_label_set_text(s_time_lbl, "--:--");
    lv_obj_set_style_text_color(s_time_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(s_time_lbl, &lv_font_montserrat_16, 0);

    /* ── Date + notification bar ── */
    lv_obj_t *datebar = lv_obj_create(ui_home);
    lv_obj_remove_style_all(datebar);
    lv_obj_set_width(datebar, lv_pct(100));
    lv_obj_set_height(datebar, 26);
    lv_obj_set_style_bg_color(datebar, C_TOPBAR, 0);
    lv_obj_set_style_bg_opa(datebar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(datebar, 10, 0);
    lv_obj_set_style_pad_ver(datebar, 0, 0);
    lv_obj_set_flex_flow(datebar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(datebar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(datebar, LV_OBJ_FLAG_SCROLLABLE);

    s_date_lbl = lv_label_create(datebar);
    lv_label_set_text(s_date_lbl, "Syncing...");
    lv_obj_set_style_text_color(s_date_lbl, C_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_date_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_flex_grow(s_date_lbl, 1);

    s_notify_lbl = lv_label_create(datebar);
    lv_label_set_text(s_notify_lbl, "");
    lv_obj_set_style_text_color(s_notify_lbl, C_NOTIFY, 0);
    lv_obj_set_style_text_font(s_notify_lbl, &lv_font_montserrat_14, 0);

    /* ── Scrollable card area ── */
    lv_obj_t *scroll = lv_obj_create(ui_home);
    lv_obj_remove_style_all(scroll);
    lv_obj_set_width(scroll, lv_pct(100));
    lv_obj_set_flex_grow(scroll, 1);
    lv_obj_set_style_bg_color(scroll, C_BG, 0);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scroll, 8, 0);
    lv_obj_set_style_pad_row(scroll, 8, 0);
    lv_obj_set_flex_flow(scroll, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);

    s_weather_lbl = __make_card(scroll, C_WEATHER, LV_SYMBOL_GPS "  WEATHER");
    s_todo_lbl    = __make_card(scroll, C_TODO,    LV_SYMBOL_OK  "  TODO");
    s_med_lbl     = __make_card(scroll, C_MED,     LV_SYMBOL_BELL "  MEDICINE");

    /* ── Chat button (bottom) ── */
    lv_obj_t *chat_btn = lv_obj_create(ui_home);
    lv_obj_remove_style_all(chat_btn);
    lv_obj_set_width(chat_btn, lv_pct(100));
    lv_obj_set_height(chat_btn, 44);
    lv_obj_set_style_bg_color(chat_btn, C_CHAT_BTN, 0);
    lv_obj_set_style_bg_opa(chat_btn, LV_OPA_COVER, 0);
    lv_obj_add_flag(chat_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(chat_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(chat_btn, __chat_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *chat_lbl = lv_label_create(chat_btn);
    lv_label_set_text(chat_lbl, LV_SYMBOL_AUDIO "  AI Chat");
    lv_obj_set_style_text_color(chat_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(chat_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(chat_lbl);

    /* Alarm stop button — hidden by default, floats above the chat button */
    s_alarm_overlay = __make_alarm_overlay(ui_home);
}

void ui_home_screen_destroy(void)
{
    if (s_clock_tmr)  { lv_timer_del(s_clock_tmr);  s_clock_tmr  = NULL; }
    if (s_notify_tmr) { lv_timer_del(s_notify_tmr); s_notify_tmr = NULL; }
    s_status_lbl = s_time_lbl = s_date_lbl = s_notify_lbl = NULL;
    s_weather_lbl = s_todo_lbl = s_med_lbl = NULL;
    s_alarm_overlay = NULL;
    if (ui_home) { lv_obj_del(ui_home); ui_home = NULL; }
}

/* ── Public API called by app_display.c ──────────────────── */

void ui_set_emotion(const char *emotion)
{
    (void)emotion; /* status-text based; no emoji widget */
}

void ui_set_device_status(const char *status)
{
    if (s_status_lbl && status) {
        lv_label_set_text(s_status_lbl, status);
    }
    if (ui_chat) {
        ui_chat_set_status(status);
    }
}

void ui_set_user_msg(const char *msg)
{
    if (!msg) return;
    _ui_screen_change(&ui_chat, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, &ui_chat_screen_init);
    ui_chat_append_user_msg(msg);
}

void ui_set_assistant_msg(const char *msg)
{
    if (!msg) return;
    _ui_screen_change(&ui_chat, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, &ui_chat_screen_init);
    ui_chat_append_ai_msg(msg);
}

void ui_set_system_msg(const char *msg)
{
    if (!s_notify_lbl || !msg) return;
    lv_label_set_text(s_notify_lbl, msg);
    if (s_notify_tmr) {
        lv_timer_reset(s_notify_tmr);
    } else {
        s_notify_tmr = lv_timer_create(__notify_clear_cb, 3000, NULL);
        lv_timer_set_repeat_count(s_notify_tmr, 1);
    }
}

void ui_dashboard_refresh(void)
{
    if (!ui_home || lv_scr_act() != ui_home) return;
    __refresh_weather();
    __refresh_todo();
    __refresh_medicine();
}

void ui_alarm_stop_show(bool show)
{
    if (!s_alarm_overlay) return;
    if (show) {
        lv_obj_remove_flag(s_alarm_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_alarm_overlay);
    } else {
        lv_obj_add_flag(s_alarm_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}
