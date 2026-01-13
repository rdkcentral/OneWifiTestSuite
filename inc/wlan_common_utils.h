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

#ifndef WLAN_COMMON_UTILS_H
#define WLAN_COMMON_UTILS_H
#include "wlan_emu_common.h"
#include "wifi_hal.h"

typedef struct {
    int enum_data;
    const char *string_data;
} enum_string_conv_t;

static enum_string_conv_t mfp_config[] = {
    { wifi_mfp_cfg_disabled, "Disabled" },
    { wifi_mfp_cfg_optional, "Optional" },
    { wifi_mfp_cfg_required, "Required" }
};

static enum_string_conv_t client_model[] = {
    { sta_model_type_iphone, "Iphone" },
    { sta_model_type_pixel,  "Pixel"  }
};

static enum_string_conv_t secu_mode[] = {
    { wifi_security_mode_none,                "None"                     },
    { wifi_security_mode_wep_64,              "WEP64"                    },
    { wifi_security_mode_wep_128,             "WEP128"                   },
    { wifi_security_mode_wpa_personal,        "WPA-Personal"             },
    { wifi_security_mode_wpa2_personal,       "WPA2-Personal"            },
    { wifi_security_mode_wpa_wpa2_personal,   "WPA-WPA2-Personal"        },
    { wifi_security_mode_wpa_enterprise,      "WPA-Enterprise"           },
    { wifi_security_mode_wpa2_enterprise,     "WPA2-Enterprise"          },
    { wifi_security_mode_wpa_wpa2_enterprise, "WPA-WPA2-Enterprise"      },
    { wifi_security_mode_wpa3_personal,       "WPA3-Personal"            },
    { wifi_security_mode_wpa3_transition,     "WPA3-Personal-Transition" },
    { wifi_security_mode_wpa3_enterprise,     "WPA3-Enterprise"          },
    { wifi_security_mode_enhanced_open,       "Enhanced-Open"            }
};

static enum_string_conv_t encryption[] = {
    { wifi_encryption_none,        "None"        },
    { wifi_encryption_tkip,        "TKIP"        },
    { wifi_encryption_aes,         "AES"         },
    { wifi_encryption_aes_tkip,    "AES+TKIP"    },
    { wifi_encryption_aes_gcmp256, "AES+GCMP256" },
    { wifi_encryption_gcmp256,     "GCMP256"     }
};

class wlan_common_utils {
public:
    static const char *get_mfp_string(int enum_data);
    static int get_mfp_enum(const char *string_data);
    static const char *get_sta_type_string(int enum_data);
    static int get_sta_type_enum(const char *string_data);
    static const char *get_secu_mode_string(int enum_data);
    static int get_secu_mode_enum(const char *string_data);
    static const char *get_encryption_string(int enum_data);
    static int get_encryption_enum(const char *string_data);
    static int decode_pcap_frame_type(char *frame_type_str,
        frame_capture_request_t *frame_capture_req);
    static int encode_pcap_frame_type(cJSON *fc_arr, frame_capture_request_t frame_capture_req);
};
#endif //WLAN_COMMON_UTILS_H
