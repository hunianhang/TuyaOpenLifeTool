#ifndef __APP_ALARM_H__
#define __APP_ALARM_H__

#include "tuya_cloud_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

OPERATE_RET app_alarm_init(void);
OPERATE_RET app_alarm_set(uint8_t hour, uint8_t minute, bool enabled);
void        app_alarm_upload(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_ALARM_H__ */
