#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include "wlan_emu_log.h"

char *get_formatted_time(char *time)
{
    struct tm *tm_info;
    struct timeval tv_now;
    char tmp[512];

    gettimeofday(&tv_now, NULL);
    tm_info = localtime(&tv_now.tv_sec);

    strftime(tmp, sizeof(tmp), "%y%m%d-%T", tm_info);

    snprintf(time, sizeof(tmp), "%s.%06lu", tmp, tv_now.tv_usec);

    return time;
}

void wlan_emu_print(wlan_emu_log_level_t level, const char *format, ...)
{
    char buff[1024] = {0};
    va_list list;
    FILE *log_fp = NULL;
    char *dbg_file = "/nvram/cciDbg";
    char *log_file = "/tmp/cciLog";


    log_fp = fopen(log_file, "a+");
    if (log_fp == NULL) {
        return;
    }

    switch (level) {
        case wlan_emu_log_level_dbg:
            if ((access(dbg_file, R_OK)) == 0) {
                va_start(list, format);
                vfprintf(log_fp, format, list);
                va_end(list);
            }
        break;

        case wlan_emu_log_level_info:
        case wlan_emu_log_level_err:
            va_start(list, format);
            vfprintf(log_fp, format, list);
            va_end(list);
        break;
        default:
            return;
    }

    fflush(log_fp);
    fclose(log_fp);
    return;
}

