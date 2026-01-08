#include "wlan_emu_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

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
    va_list list;
    FILE *log_fp = NULL;
    char *dbg_file = "/nvram/cciDbg";
    char buff[256] = { 0 };

    switch (level) {
    case wlan_emu_log_level_dbg:
        if ((access(dbg_file, R_OK)) != 0) {
            return;
        }
        break;

    case wlan_emu_log_level_info:
    case wlan_emu_log_level_err:
        break;
    default:
        return;
    }

    log_fp = fopen(CCI_LOG_FILE, "a+");
    if (log_fp == NULL) {
        return;
    }

    get_formatted_time(&buff[strlen(buff)]);

    static const char *level_marker[wlan_emu_log_level_max] = {
        [wlan_emu_log_level_dbg] = "<D>",
        [wlan_emu_log_level_info] = "<I>",
        [wlan_emu_log_level_err] = "<E>",
    };
    if (level < wlan_emu_log_level_max)
        snprintf(&buff[strlen(buff)], sizeof(buff) - strlen(buff), "%s ", level_marker[level]);

    fprintf(log_fp, "%s", buff);

    va_start(list, format);
    vfprintf(log_fp, format, list);
    va_end(list);

    fflush(log_fp);
    fclose(log_fp);
    return;
}
