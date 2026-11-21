#ifndef HEALTH_AUDIT_H
#define HEALTH_AUDIT_H

#include "gba.h"

/* Hardware Component Health Status */
typedef enum {
    HEALTH_STATUS_PASS = 0,
    HEALTH_STATUS_WARN,
    HEALTH_STATUS_FAIL,
    HEALTH_STATUS_STANDBY,
    HEALTH_STATUS_NA
} HealthStatus;

/* Consolidated Health Audit Report */
typedef struct {
    const char* model_name;
    bool is_nds;
    u16 waitcnt;

    HealthStatus status_cpu;
    HealthStatus status_iwram;
    HealthStatus status_ewram;
    HealthStatus status_vram;
    HealthStatus status_sound;
    HealthStatus status_sio;
    HealthStatus status_sram;
    HealthStatus status_rtc;
    HealthStatus status_keys;

    int stuck_keys_mask;
    int total_score;        /* 0 - 100% */
    const char* grade_str;  /* e.g. "GRADE A+ (MINT / CERTIFIED)" */
    u16 grade_color;
} HealthAuditReport;

/* Run automated hardware audit sweep */
void health_audit_run(HealthAuditReport* report);

/* Interactive Health Audit Dashboard Screen */
void run_health_audit_screen(void);

#endif /* HEALTH_AUDIT_H */
