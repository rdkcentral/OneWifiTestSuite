/**
 * Copyright 2025 Comcast Cable Communications Management, LLC
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

#include "wlan_emu_ui_mgr.h"
#include "wlan_emu_log.h"

int wlan_emu_ui_ssl_config::get_mtls_configuration()
{
    /* Generate proper ssl key with openssl and replace with dummy_ssl_key */
    /* The dummy_ssl_cert_path should be replaced by proper ssl certificate path
       where its installed */
    /* These ssl_key and ssl_cert are used for http_get and http_post operations */

    snprintf(ssl_key, sizeof(ssl_key), "%s", "dummy_ssl_key");
    snprintf(ssl_cert, sizeof(ssl_cert), "%s", "dummy_ssl_cert_path");

    return RETURN_OK;
}

wlan_emu_ui_ssl_config::wlan_emu_ui_ssl_config()
{
    if (get_mtls_configuration() == RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: SSL configuration retrieved successfully\n",
            __func__, __LINE__);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Failed to retrieve SSL configuration\n",
            __func__, __LINE__);
        return;
    }
    set_ssl_cert(ssl_cert);
    set_ssl_key(ssl_key);
}

wlan_emu_ui_ssl_config::~wlan_emu_ui_ssl_config() {

}
