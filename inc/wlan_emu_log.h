#ifndef WLAN_EMU_LOG
#define WLAN_EMU_LOG

typedef enum {
    wlan_emu_log_level_dbg,
    wlan_emu_log_level_info,
    wlan_emu_log_level_err,
    wlan_emu_log_level_max,
} wlan_emu_log_level_t;

void wlan_emu_print(wlan_emu_log_level_t level, const char *format, ...);

#endif // WLAN_EMU_LOG
