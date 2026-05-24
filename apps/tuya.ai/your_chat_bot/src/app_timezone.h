#ifndef __APP_TIMEZONE_H__
#define __APP_TIMEZONE_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Compute DST windows for the current year (+next) and populate
 * tal_time_set_sum_zone_tbl().  Safe to call at any time after the
 * system clock and timezone are both set. */
void app_timezone_dst_apply(void);

/* Subscribe to "app.time.sync" for periodic resyncs.  Call once at
 * startup before the first time-sync event fires. */
void app_timezone_dst_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_TIMEZONE_H__ */
