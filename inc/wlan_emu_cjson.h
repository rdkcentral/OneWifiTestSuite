#include <cjson/cJSON.h>

#define decode_param_string(json, key, value)                                                      \
    {                                                                                              \
        value = cJSON_GetObjectItem(json, key);                                                    \
        if ((value == NULL) || (cJSON_IsString(value) == false) || (value->valuestring == NULL) || \
            (strcmp(value->valuestring, "") == 0)) {                                               \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Validation failed for key:%s\n",        \
                __func__, __LINE__, key);                                                          \
            return RETURN_ERR;                                                                     \
        }                                                                                          \
    }

#define decode_param_allow_empty_string(json, key, value)                                          \
    {                                                                                              \
        value = cJSON_GetObjectItem(json, key);                                                    \
        if ((value == NULL) || (cJSON_IsString(value) == false) || (value->valuestring == NULL)) { \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Validation failed for key:%s\n",        \
                __func__, __LINE__, key);                                                          \
            return RETURN_ERR;                                                                     \
        }                                                                                          \
    }

#define decode_param_integer(json, key, value)                                              \
    {                                                                                       \
        value = cJSON_GetObjectItem(json, key);                                             \
        if ((value == NULL) || (cJSON_IsNumber(value) == false)) {                          \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Validation failed for key:%s\n", \
                __func__, __LINE__, key);                                                   \
            return RETURN_ERR;                                                              \
        }                                                                                   \
    }

#define decode_param_bool(json, key, value)                                                 \
    {                                                                                       \
        value = cJSON_GetObjectItem(json, key);                                             \
        if ((value == NULL) || (cJSON_IsBool(value) == false)) {                            \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Validation failed for key:%s\n", \
                __func__, __LINE__, key);                                                   \
            return RETURN_ERR;                                                              \
        }                                                                                   \
    }

#define decode_param_array(json, key, value)                                                \
    {                                                                                       \
        value = cJSON_GetObjectItem(json, key);                                             \
        if ((value == NULL) || (cJSON_IsArray(value) == false)) {                           \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Validation failed for key:%s\n", \
                __func__, __LINE__, key);                                                   \
            return RETURN_ERR;                                                              \
        }                                                                                   \
    }

#define decode_param_object(json, key, value)                                               \
    {                                                                                       \
        value = cJSON_GetObjectItem(json, key);                                             \
        if ((value == NULL) || (cJSON_IsObject(value) == false)) {                          \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Validation failed for key:%s\n", \
                __func__, __LINE__, key);                                                   \
            return RETURN_ERR;                                                              \
        }                                                                                   \
    }
