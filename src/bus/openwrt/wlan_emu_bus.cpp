#include "wlan_emu.h"
#include "wlan_emu_bus.h"
#include "wlan_emu_bus_common.h"
#include "wifi_util.h"
#include <stdlib.h>
#include <string.h>

static bus_error_t bus_open(bus_handle_t *handle, char *component_name)
{
    return bus_error_success;
}

static bus_error_t bus_close(bus_handle_t *handle)
{
    return bus_error_success;
}

bus_error_t bus_data_get(bus_handle_t *handle, char const *name, raw_data_t *data)
{
    return bus_error_success;
}

void bus_data_free(raw_data_t *data)
{
    if ((data->raw_data.bytes) &&
        (data->data_type == bus_data_type_bytes || data->data_type == bus_data_type_string)) {
        free(data->raw_data.bytes);
    }
}

static bus_error_t bus_set(bus_handle_t *handle, char const *name, raw_data_t *data)
{
    return bus_error_success;
}

bus_error_t bus_reg_data_elements(bus_handle_t *handle, bus_data_element_t *data_element,
    uint32_t num_of_element)
{
    return bus_error_success;
}

static bus_error_t bus_event_publish(bus_handle_t *handle, char const *name, raw_data_t *data)
{
    return bus_error_success;
}

static bus_error_t bus_raw_event_publish(bus_handle_t *handle, char *name, void *data,
    unsigned int size)
{
    return bus_error_success;
}

static bus_error_t bus_set_string(bus_handle_t *handle, char const *param_name,
    char const *param_str)
{
    return bus_error_success;
}

bus_error_t bus_event_subscribe(bus_handle_t *handle, char const *event_name, void *cb,
    void *userData, int timeout)
{
    return bus_error_success;
}

bus_error_t bus_event_subscribe_ex(bus_handle_t *handle, bus_event_sub_t *l_sub_info_map,
    int num_sub, int timeout)
{
    return bus_error_success;
}

bus_error_t bus_event_subscribe_ex_async(bus_handle_t *handle, bus_event_sub_t *l_sub_info_map,
    int num_sub, void *l_sub_handler, int timeout)
{
    return bus_error_success;
}

bus_error_t bus_method_invoke(bus_handle_t *handle, void *paramName, char *event,
    raw_data_t *input_data, raw_data_t *output_data, bool input_bus_data)
{
    return bus_error_success;
}

static bus_error_t bus_get_trace_context(bus_handle_t *handle, char *traceParent,
    uint32_t traceParentLength, char *traceState, uint32_t traceStateLength)
{
    return bus_error_success;
}

static bus_error_t bus_reg_table_row(bus_handle_t *handle, char const *name,
    uint32_t row_index, char const *alias)
{
    return bus_error_success;
}


static bus_error_t bus_unreg_table_row(bus_handle_t *handle, char const *name)
{
    return bus_error_success;
}

static bus_error_t bus_remove_table_row(bus_handle_t *handle, char const *name)
{
    return bus_error_success;
}

static bus_error_t bus_event_unsubscribe(bus_handle_t *handle, char const *event_name)
{
    return bus_error_success;
}

bus_error_t bus_unreg_data_elements(bus_handle_t *handle, uint32_t num_of_element,
    bus_data_element_t *data_element)
{
    return bus_error_success;
}

bus_error_t wlan_emu_bus_t::bus_init()
{
    this->desc.bus_open_fn = bus_open;
    this->desc.bus_close_fn = bus_close;
    this->desc.bus_data_get_fn = bus_data_get;
    this->desc.bus_data_free_fn = bus_data_free;
    this->desc.bus_set_fn = bus_set;
    this->desc.bus_reg_data_element_fn = bus_reg_data_elements;
    this->desc.bus_event_publish_fn = bus_event_publish;
    this->desc.bus_raw_event_publish_fn = bus_raw_event_publish;
    this->desc.bus_set_string_fn = bus_set_string;
    this->desc.bus_event_subs_fn = bus_event_subscribe;
    this->desc.bus_event_subs_ex_fn = bus_event_subscribe_ex;
    this->desc.bus_event_subs_ex_async_fn = bus_event_subscribe_ex_async;
    this->desc.bus_method_invoke_fn = bus_method_invoke;
    this->desc.bus_get_trace_context_fn = bus_get_trace_context;
    this->desc.bus_reg_table_row_fn = bus_reg_table_row;
    this->desc.bus_unreg_table_row_fn = bus_unreg_table_row;
    this->desc.bus_remove_table_row_fn = bus_remove_table_row;
    this->desc.bus_event_unsubs_fn = bus_event_unsubscribe;
    this->desc.bus_unreg_data_elements_fn = bus_unreg_data_elements;

    return bus_error_success;
}

wlan_emu_bus_t::wlan_emu_bus_t()
{
}

wlan_emu_bus_t::~wlan_emu_bus_t()
{
}