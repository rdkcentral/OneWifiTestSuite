/**
 * Copyright 2026 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

#define encode_param_string(json, key, value)                                              \
    {                                                                                      \
        if (cJSON_AddStringToObject(json, key, value) == NULL) {                           \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to add %s for key:%s\n", \
                __func__, __LINE__, value, key);                                           \
            return RETURN_ERR;                                                             \
        }                                                                                  \
    }

#define encode_param_integer(json, key, value)                                             \
    {                                                                                      \
        if (cJSON_AddNumberToObject(json, key, value) == NULL) {                           \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to add %d for key:%s\n", \
                __func__, __LINE__, value, key);                                           \
            return RETURN_ERR;                                                             \
        }                                                                                  \
    }

#define encode_param_array(json, key, param)                                                    \
    {                                                                                           \
        if ((param = cJSON_CreateArray()) == NULL) {                                            \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to create array\n", __func__, \
                __LINE__);                                                                      \
            return RETURN_ERR;                                                                  \
        }                                                                                       \
        if (cJSON_AddItemToObject(json, key, param) == false) {                                 \
            cJSON_Delete(param);                                                                \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to add array for key:%s\n",   \
                __func__, __LINE__, key);                                                       \
            return RETURN_ERR;                                                                  \
        }                                                                                       \
    }

#define encode_param_array_item_object(json, param)                                               \
    {                                                                                             \
        param = cJSON_CreateObject();                                                             \
        if (param == NULL) {                                                                      \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to create object\n", __func__,  \
                __LINE__);                                                                        \
            return RETURN_ERR;                                                                    \
        }                                                                                         \
        if (cJSON_AddItemToArray(json, param) == false) {                                         \
            cJSON_Delete(param);                                                                  \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to add array item\n", __func__, \
                __LINE__);                                                                        \
            return RETURN_ERR;                                                                    \
        }                                                                                         \
    }

#define encode_param_array_item_string(json, value)                                               \
    {                                                                                             \
        cJSON *param = cJSON_CreateString(value);                                                 \
        if (param == NULL) {                                                                      \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to create object\n", __func__,  \
                __LINE__);                                                                        \
            return RETURN_ERR;                                                                    \
        }                                                                                         \
        if (cJSON_AddItemToArray(json, param) == false) {                                         \
            cJSON_Delete(param);                                                                  \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to add array item\n", __func__, \
                __LINE__);                                                                        \
            return RETURN_ERR;                                                                    \
        }                                                                                         \
    }

#define validate_param_string(param)                                                               \
    {                                                                                              \
        if ((param == NULL) || (cJSON_IsString(param) == false) || (param->valuestring == NULL)) { \
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Validation failed for string param\n",  \
                __func__, __LINE__);                                                               \
            return RETURN_ERR;                                                                     \
        }                                                                                          \
    }
