#include "app_timezone.h"
#include "tal_time_service.h"
#include "tal_event.h"
#include "tal_log.h"
#include <string.h>
#include <stdint.h>

#define SECS_PER_HOUR 3600

/* Day-of-week: 0=Sunday … 6=Saturday (Tomohiko Sakamoto's algorithm) */
static int __dow(int y, int m, int d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static int __days_in_month(int y, int m)
{
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int d = days[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0))
        d = 29;
    return d;
}

/* Unix timestamp (UTC) of midnight on y-m-d */
static TIME_T __day_epoch(int y, int m, int d)
{
    TIME_T days = 0;
    for (int yr = 1970; yr < y; yr++)
        days += ((yr % 4 == 0 && yr % 100 != 0) || yr % 400 == 0) ? 366 : 365;
    for (int mo = 1; mo < m; mo++)
        days += (TIME_T)__days_in_month(y, mo);
    days += (TIME_T)(d - 1);
    return days * 86400;
}

/* Day of the nth occurrence of weekday (0=Sun) in month m.
 * nth=1 → first occurrence, nth=2 → second, nth=-1 → last. */
static int __nth_weekday(int y, int m, int weekday, int nth)
{
    if (nth > 0) {
        int off = (weekday - __dow(y, m, 1) + 7) % 7;
        return 1 + off + (nth - 1) * 7;
    }
    /* last occurrence */
    int last = __days_in_month(y, m);
    int off  = (__dow(y, m, last) - weekday + 7) % 7;
    return last - off;
}

/*
 * US DST (since 2007):
 *   Start: 2nd Sunday of March  at 02:00 local standard time
 *   End:   1st Sunday of November at 02:00 local daylight time
 *
 *   In UTC:
 *     start = midnight(2nd Sun Mar) + 2h - tz_std
 *     end   = midnight(1st Sun Nov) + 2h - (tz_std + 1h) = midnight + 1h - tz_std
 */
static void __apply_us_dst(int year, int tz_sec, SUM_ZONE_S *zones, uint32_t *cnt)
{
    for (int y = year; y <= year + 1; y++) {
        int start_day = __nth_weekday(y, 3,  0, 2);
        int end_day   = __nth_weekday(y, 11, 0, 1);

        int start_off = 2 * SECS_PER_HOUR - tz_sec;
        int end_off   = 1 * SECS_PER_HOUR - tz_sec;

        zones[*cnt].posix_min = __day_epoch(y, 3,  start_day) + (TIME_T)start_off;
        zones[*cnt].posix_max = __day_epoch(y, 11, end_day)   + (TIME_T)end_off;
        (*cnt)++;
    }
}

/*
 * EU DST:
 *   Start: Last Sunday of March   at 01:00 UTC
 *   End:   Last Sunday of October at 01:00 UTC
 */
static void __apply_eu_dst(int year, SUM_ZONE_S *zones, uint32_t *cnt)
{
    for (int y = year; y <= year + 1; y++) {
        int start_day = __nth_weekday(y, 3,  0, -1);
        int end_day   = __nth_weekday(y, 10, 0, -1);

        zones[*cnt].posix_min = __day_epoch(y, 3,  start_day) + (TIME_T)SECS_PER_HOUR;
        zones[*cnt].posix_max = __day_epoch(y, 10, end_day)   + (TIME_T)SECS_PER_HOUR;
        (*cnt)++;
    }
}

void app_timezone_dst_apply(void)
{
    if (OPRT_OK != tal_time_check_time_sync()) {
        PR_DEBUG("[timezone] time not synced yet, skip DST");
        return;
    }

    int tz_sec = 0;
    tal_time_get_time_zone_seconds(&tz_sec);

    /* Get year from UTC to avoid chicken-and-egg with DST */
    TIME_T posix = tal_time_get_posix();
    int year = 1970;
    {
        TIME_T rem = posix;
        for (int y = 1970; ; y++) {
            TIME_T yr_secs = (TIME_T)(((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) ? 366 : 365) * 86400;
            if (rem < yr_secs) { year = y; break; }
            rem -= yr_secs;
        }
    }

    SUM_ZONE_S zones[4];
    uint32_t   cnt = 0;
    memset(zones, 0, sizeof(zones));

    if (tz_sec >= -8 * SECS_PER_HOUR && tz_sec <= -4 * SECS_PER_HOUR) {
        /* US: PST (UTC-8) / MST (UTC-7) / CST (UTC-6) / EST (UTC-5) */
        __apply_us_dst(year, tz_sec, zones, &cnt);
        PR_INFO("[timezone] US DST applied: tz=%ds year=%d-%d", tz_sec, year, year + 1);
    } else if (tz_sec >= 0 && tz_sec <= 2 * SECS_PER_HOUR) {
        /* EU: WET (UTC+0) / CET (UTC+1) / EET (UTC+2) */
        __apply_eu_dst(year, zones, &cnt);
        PR_INFO("[timezone] EU DST applied: tz=%ds year=%d-%d", tz_sec, year, year + 1);
    } else {
        PR_INFO("[timezone] No DST rules for tz=%ds", tz_sec);
    }

    if (cnt > 0) {
        tal_time_set_sum_zone_tbl(zones, cnt);
    }
}

static int __time_sync_cb(void *data)
{
    app_timezone_dst_apply();
    return 0;
}

void app_timezone_dst_init(void)
{
    /* Periodic resyncs: "app.time.sync" fires from TUYA_EVENT_TIMESTAMP_SYNC.
     * On first boot the timezone is not yet set at that point; the MQTT
     * connected handler in tuya_main.c calls app_timezone_dst_apply() once
     * activation is complete to cover that case. */
    tal_event_subscribe("app.time.sync", "app_timezone", __time_sync_cb, SUBSCRIBE_TYPE_NORMAL);
}
