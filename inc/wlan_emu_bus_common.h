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

#ifndef WLAN_EMU_BUS_COMMON_H
#define WLAN_EMU_BUS_COMMON_H

#include <ccsp/bus_common.h>
#include <ccsp/bus.h>

typedef bus_error_t (*wifi_bus_event_unsubs_t)(bus_handle_t *handle, char const *name);
typedef bus_error_t (*wifi_bus_unreg_data_elements_t)(bus_handle_t *handle, uint32_t num_of_element,
    bus_data_element_t *data_element);

typedef struct {
    wifi_bus_open_t bus_open_fn;
    wifi_bus_close_t bus_close_fn;
    wifi_bus_data_get_t bus_data_get_fn;
    wifi_bus_data_free_t bus_data_free_fn;
    wifi_bus_set_t bus_set_fn;
    wifi_bus_reg_elements_t bus_reg_data_element_fn;
    wifi_bus_event_publish_t bus_event_publish_fn;
    wifi_bus_raw_event_publish_t bus_raw_event_publish_fn;
    wifi_bus_set_str_t bus_set_string_fn;
    wifi_bus_event_subs_t bus_event_subs_fn;
    wifi_bus_event_subscribe_ex_t bus_event_subs_ex_fn;
    wifi_bus_event_subscribe_ex_async_t bus_event_subs_ex_async_fn;
    wifi_bus_method_invoke_t bus_method_invoke_fn;
    wifi_bus_get_trace_context_t bus_get_trace_context_fn;
    wifi_bus_reg_table_row_t bus_reg_table_row_fn;
    wifi_bus_unreg_table_row_t bus_unreg_table_row_fn;
    wifi_bus_remove_table_row_t bus_remove_table_row_fn;
    wifi_bus_event_unsubs_t bus_event_unsubs_fn;
    wifi_bus_unreg_data_elements_t bus_unreg_data_elements_fn;
} wifi_emu_bus_desc_t;

void init_bus_mux_root(void);
void deinit_bus_all_mux_param(void);
#endif // WLAN_EMU_BUS_COMMON_H
