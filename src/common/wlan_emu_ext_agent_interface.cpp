#include "wlan_emu_ext_agent_interface.h"
#include "wlan_emu_ext_sta_mgr.h"
#include "wlan_emu_log.h"
#include "wlan_emu_tests.h"
#include "wlan_emu_cjson.h"
#include "wlan_emu_ui_mgr.h"
#include "wlan_emu_err_code.h"
#include <string.h>
#include <memory>
#include <fstream>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sstream>

#define NUM_HOST_ENTERIES "Device.Hosts.HostNumberOfEntries"
#define Layer1interface "Device.Hosts.Host.%u.Layer1Interface"
#define HOST_IP "Device.Hosts.Host.%u.IPAddress"
#define HOST_MAC "Device.Hosts.Host.%u.PhysAddress"
#define HOST_ACTIVE "Device.Hosts.Host.%u.Active"
#define HOST_NAME "Device.Hosts.Host.%u.HostName"

static const char *frame_types[] = { "Beacon", "ProbeRequest", "ProbeResponse",
    "AssociationRequest", "AssociationResponse", "Authentication", "Deauthentication",
    "Disassociation", "ReassociationRequest", "ReassociationResponse", "Action", "Eapol" };

static const int frame_ops[] = { wlan_emu_cfg80211_ops_type_start_ap,
    wlan_emu_frm80211_ops_type_prb_req, wlan_emu_frm80211_ops_type_prb_resp,
    wlan_emu_frm80211_ops_type_assoc_req, wlan_emu_frm80211_ops_type_assoc_resp,
    wlan_emu_frm80211_ops_type_auth, wlan_emu_frm80211_ops_type_deauth,
    wlan_emu_frm80211_ops_type_disassoc, wlan_emu_frm80211_ops_type_reassoc_req,
    wlan_emu_frm80211_ops_type_reassoc_resp, wlan_emu_frm80211_ops_type_action,
    wlan_emu_frm80211_ops_type_eapol };

/*This function is used by external agent*/
int wlan_emu_ext_agent_interface_t::agent_init()
{
    char tmp_agent_mac[MAC_ADDRESS_LENGTH] = { 0x11, 0x11, 0x11, 0x11, 0x11, 0x01 };

    client_info = queue_create();
    if (client_info == NULL) {
        wlan_emu_print(wlan_emu_log_level_info,
            "%s:%d: queue_initialization failed for client_info \n", __func__, __LINE__);
        return RETURN_ERR;
    }
    agent_hostname = "Raspberry";
    memcpy(agent_mac, tmp_agent_mac, sizeof(agent_mac));

    return RETURN_OK;
}

/*This function is used by external agent*/
void wlan_emu_ext_agent_interface_t::agent_exit()
{
    queue_destroy(client_info);
}

int wlan_emu_ext_agent_interface_t::encode_external_agent_capability(std::string &cap_json_string)
{
    cJSON *json = NULL, *cap_arr = NULL;
    char *str = NULL, mac_str[32] = { 0 };
    sta_info_t *sta_info = NULL;

    json = cJSON_CreateObject();
    if (json == NULL) {
        return RETURN_ERR;
    }

    cJSON_AddStringToObject(json, "Version", version);
    cJSON_AddStringToObject(json, "SubDocName", "Agent Capability");

    uint8_mac_to_string_mac(agent_mac, mac_str);
    cJSON_AddStringToObject(json, "ExternalAgentMACAddress", mac_str);
    cJSON_AddStringToObject(json, "ExternalAgentHostName", agent_hostname.c_str());
    cJSON_AddStringToObject(json, "ExternalAgentIPAddress", agent_ip_address);
    cJSON_AddNumberToObject(json, "ExternalClientCount", total_supported_clients);

    cap_arr = cJSON_CreateArray();
    if (cap_arr == NULL) {
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    cJSON_AddItemToObject(json, "ExternalClientCapability", cap_arr);

    for (int count = 0; count < total_supported_clients; count++) {
        cJSON *client_obj = cJSON_CreateObject();
        if (client_obj == NULL) {
            cJSON_Delete(json);
            return RETURN_ERR;
        }
        sta_info = (sta_info_t *)queue_peek(client_info, count);
        if (sta_info != NULL) {
            uint8_mac_to_string_mac(sta_info->mac, mac_str);
            cJSON_AddStringToObject(client_obj, "ExternalClientMAC", mac_str);
        }
        cJSON_AddItemToArray(cap_arr, client_obj);
    }

    str = cJSON_Print(json);
    if (str == NULL) {
        cJSON_Delete(json);
        return RETURN_ERR;
    }

    cap_json_string.append(str);

    cJSON_free(str);
    cJSON_Delete(json);
    return RETURN_OK;
}

static int add_frame_type(test_step_params_t *step, cJSON *station_arr)
{
    for (int i = 0; i < sizeof(frame_types) / sizeof(frame_types[0]); i++) {
        if (step->frame_request.frm80211_ops & frame_ops[i]) {
            cJSON *frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(station_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", frame_types[i]);
        }
    }
    return RETURN_OK;
}

static int add_station_proto_type(test_step_params_t *step, cJSON *station_arr)
{
    station_prototype_pcaps_t *tmp_station_prototype_pcap;

    for (int i = 0; i < sizeof(frame_types) / sizeof(frame_types[0]); i++) {
        if (step->frame_request.frm80211_ops & frame_ops[i]) {
            cJSON *prototype_obj = cJSON_CreateObject();
            if (prototype_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(station_arr, prototype_obj);
            cJSON_AddStringToObject(prototype_obj, "FrameType", frame_types[i]);

            tmp_station_prototype_pcap = (station_prototype_pcaps_t *)queue_peek(
                step->u.sta_test->station_prototype->fc_prototype_q, i);
            cJSON_AddStringToObject(prototype_obj, "PcapLocation",
                tmp_station_prototype_pcap->file_location);
        }
    }
    return RETURN_OK;
}

int wlan_emu_ext_agent_interface_t::decode_external_agent_capability(const std::string &response,
    wlan_emu_ext_agent_interface_t *agent_details)
{
    cJSON *root_json = NULL;
    cJSON *param = NULL;
    cJSON *entry = NULL;
    cJSON *sub_entry = NULL;
    sta_info_t *external_client_info = NULL;

    if (response.empty()) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Capability file is empty\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    root_json = cJSON_Parse(response.c_str());
    if (root_json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Json file is empty\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    decode_param_string(root_json, "SubDocName", param);
    copy_string(agent_details->subdoc_name, param->valuestring, sizeof(subdoc_name) - 1);

    decode_param_string(root_json, "ExternalAgentHostName", param);
    agent_details->agent_hostname = param->valuestring;

    decode_param_integer(root_json, "ExternalClientCount", param);
    agent_details->total_supported_clients = param->valuedouble;

    entry = cJSON_GetObjectItem(root_json, "ExternalClientCapability");

    if (cJSON_IsArray(entry) == false) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: Invalid number of client capability\n",
            __func__, __LINE__);
        cJSON_Delete(root_json);
        return RETURN_ERR;
    }

    agent_details->client_info = queue_create();
    cJSON_ArrayForEach(sub_entry, entry) {
        external_client_info = new (std::nothrow)
            sta_info_t; // will free later based on where the data is used
        external_client_info->status = sta_state_free;
        if (external_client_info == nullptr) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d failed to allocate memory\n", __func__,
                __LINE__);
            cJSON_Delete(root_json);
            return RETURN_ERR;
        }
        if (decode_param_string_fn(sub_entry, "ExternalClientMAC", param) == RETURN_ERR) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to decode ExternalClientMAC\n",
                __func__, __LINE__);
            delete (external_client_info);
            return RETURN_ERR;
        }
        string_mac_to_uint8_mac(external_client_info->mac, param->valuestring);
        queue_push(agent_details->client_info, external_client_info);
    }

    cJSON_Delete(root_json);
    return RETURN_OK;
}

static int get_external_agent_details(uint instance_number, bus_handle_t *handle,
    wlan_emu_ext_agent_interface_t *agent_details, wlan_emu_bus_t *bus_mgr)
{
    bus_error_t rc = bus_error_success;
    char param_name[64] = { 0 };
    raw_data_t bus_data;
    char *layer1interace;
    bool active_status = false;
    char agent_host_name[] = "raspberrypi_ext";

    memset(param_name, 0, sizeof(param_name));
    memset(&bus_data, 0, sizeof(bus_data));
    snprintf(param_name, sizeof(param_name), Layer1interface, instance_number);
    rc = bus_mgr->desc.bus_data_get_fn(handle, param_name, &bus_data);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d bus_get failed for [%s] with error [%d]\n",
            __func__, __LINE__, param_name, rc);
        return RETURN_ERR;
    }

    if (bus_data.data_type != bus_data_type_string) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid data type for [%s]\n", __func__,
            __LINE__, param_name);
        return RETURN_ERR;
    }
    layer1interace = new char[bus_data.raw_data_len + 1];
    copy_string(layer1interace, (char *)bus_data.raw_data.bytes, bus_data.raw_data_len + 1);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d Layer1interface : %s\n", __func__, __LINE__,
        layer1interace);
    free(bus_data.raw_data.bytes);

    memset(param_name, 0, sizeof(param_name));
    memset(&bus_data, 0, sizeof(bus_data));

    snprintf(param_name, sizeof(param_name), HOST_ACTIVE, instance_number);
    rc = bus_mgr->desc.bus_data_get_fn(handle, param_name, &bus_data);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d bus_get failed for [%s] with error [%d]\n",
            __func__, __LINE__, param_name, rc);
        return RETURN_ERR;
    }
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d Active status : %d\n", __func__, __LINE__,
        bus_data.raw_data.b);
    active_status = bus_data.raw_data.b;

    if (strstr(layer1interace, "Ethernet") == NULL || active_status == false) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d Not an active Ethernet interface\n", __func__,
            __LINE__);
        delete[] layer1interace;
        return RETURN_OK;
    }

    memset(&bus_data, 0, sizeof(bus_data));
    memset(param_name, 0, sizeof(param_name));

    snprintf(param_name, sizeof(param_name), HOST_NAME, instance_number);
    rc = bus_mgr->desc.bus_data_get_fn(handle, param_name, &bus_data);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d bus_get failed for [%s] with error [%d]\n",
            __func__, __LINE__, param_name, rc);
        delete[] layer1interace;
        return RETURN_ERR;
    }

    if (bus_data.data_type != bus_data_type_string) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid data type for [%s]\n", __func__,
            __LINE__, param_name);
        delete[] layer1interace;
        return RETURN_ERR;
    }

    if (strncmp((char *)bus_data.raw_data.bytes, agent_host_name, strlen(agent_host_name)) != 0) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d ignoring host_name : %s\n", __func__,
            __LINE__, (char *)bus_data.raw_data.bytes);
        delete[] layer1interace;
        return RETURN_ERR;
    }

    free(bus_data.raw_data.bytes);
    memset(&bus_data, 0, sizeof(bus_data));
    memset(param_name, 0, sizeof(param_name));

    snprintf(param_name, sizeof(param_name), HOST_MAC, instance_number);
    rc = bus_mgr->desc.bus_data_get_fn(handle, param_name, &bus_data);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d bus_get failed for [%s] with error [%d]\n",
            __func__, __LINE__, param_name, rc);
        delete[] layer1interace;
        return RETURN_ERR;
    }

    if (bus_data.data_type != bus_data_type_string) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid data type for [%s]\n", __func__,
            __LINE__, param_name);
        delete[] layer1interace;
        return RETURN_ERR;
    }
    string_mac_to_uint8_mac(agent_details->agent_mac, (char *)bus_data.raw_data.bytes);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d Agent MAC : %s\n", __func__, __LINE__,
        (char *)bus_data.raw_data.bytes);
    free(bus_data.raw_data.bytes);

    memset(&bus_data, 0, sizeof(bus_data));
    memset(param_name, 0, sizeof(param_name));

    snprintf(param_name, sizeof(param_name), HOST_IP, instance_number);
    rc = bus_mgr->desc.bus_data_get_fn(handle, param_name, &bus_data);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d bus_get failed for [%s] with error [%d]\n",
            __func__, __LINE__, param_name, rc);
        delete[] layer1interace;
        return RETURN_ERR;
    }

    if (bus_data.data_type != bus_data_type_string) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Invalid data type for [%s]\n", __func__,
            __LINE__, param_name);
        free(bus_data.raw_data.bytes);
        delete[] layer1interace;
        return RETURN_ERR;
    }

    copy_string(agent_details->agent_ip_address, (char *)bus_data.raw_data.bytes,
        sizeof(agent_details->agent_ip_address) - 1);
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d Agent IP : %s\n", __func__, __LINE__,
        agent_details->agent_ip_address);
    free(bus_data.raw_data.bytes);
    delete[] layer1interace;

    return RETURN_OK;
}

#define ARP_TABLE "/proc/net/arp"

static bool check_hostname(const std::string &ip)
{
    struct sockaddr_in sa {};
    char agent_host_name[] = "raspberrypi_ext";
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);

    struct hostent *host = gethostbyaddr(&sa.sin_addr, sizeof(sa.sin_addr), AF_INET);
    if (host) {
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d  hostname is %s\n", __func__, __LINE__,
            host->h_name);
    } else {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d  Unable to fetch hostname\n", __func__,
            __LINE__);
        return false;
    }
    if (strncmp(host->h_name, agent_host_name, strlen(agent_host_name)) == 0) {
        return true;
    }

    return false;
}

int wlan_emu_ext_agent_interface_t::get_external_agent_info(hash_map_t *ext_agent_map,
    bus_handle_t *handle, wlan_emu_bus_t *bus_mgr)
{
#if 1
    raw_data_t bus_data;
    bus_error_t rc = bus_error_success;

    memset(&bus_data, 0, sizeof(bus_data));
    rc = bus_mgr->desc.bus_data_get_fn(handle, NUM_HOST_ENTERIES, &bus_data);
    if (rc != bus_error_success) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d bus_get failed for [%s] with error [%d]\n",
            __func__, __LINE__, NUM_HOST_ENTERIES, rc);
        return RETURN_ERR;
    }
    this->external_agent_count = bus_data.raw_data.u32;

    for (int count = 1; count <= external_agent_count; count++) {
        memset(&bus_data, 0, sizeof(bus_data));
        wlan_emu_ext_agent_interface_t *agent_details = new wlan_emu_ext_agent_interface_t();

        if (get_external_agent_details(count, handle, agent_details, bus_mgr) == RETURN_OK) {
            if (strlen(agent_details->agent_ip_address) != 0) {
                char mac_str[32] = { 0 };
                uint8_mac_to_string_mac(agent_details->agent_mac, mac_str);
                wlan_emu_print(wlan_emu_log_level_err, "%s:%d Added agent mac : %s\n", __func__,
                    __LINE__, mac_str);
                hash_map_put(ext_agent_map, strdup(mac_str), agent_details);
            } else {
                delete (agent_details);
            }
        } else {
            delete (agent_details);
        }

        /*
                if (get_external_agent_details(count, handle, agent_details, bus_mgr) != RETURN_OK)
           { wlan_emu_print(wlan_emu_log_level_err, "%s:%d get_external_agent_details failed\n",
                        __func__, __LINE__);
                    delete (agent_details);
                    return RETURN_ERR;
                } else if (strlen(agent_details->agent_ip_address) != 0) {
                    char mac_str[32] = { 0 };
                    uint8_mac_to_string_mac(agent_details->agent_mac, mac_str);
                    hash_map_put(ext_agent_map, strdup(mac_str), agent_details);
                }
        */
    }
#else
    std::ifstream arp_file(ARP_TABLE);
    if (!arp_file) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d Failed to open ARP table\n", __func__,
            __LINE__);
        return -1;
    }
    char mac_str[32] = { 0 };
    std::string line, ip, mac;
    std::getline(arp_file, line);
    unsigned int count_ext = 0;
    while (std::getline(arp_file, line)) {
        std::istringstream iss(line);
        if (iss >> ip >> std::ws >> std::ws >> std::ws >> mac) {
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d IP is %s, mac is %s\n", __func__,
                __LINE__, ip.c_str(), mac.c_str());
            if (check_hostname(ip)) {
                wlan_emu_print(wlan_emu_log_level_dbg,
                    "%s:%d IP is %s, mac is %s pushing to queue \n", __func__, __LINE__, ip.c_str(),
                    mac.c_str());
                wlan_emu_ext_agent_interface_t *agent_details =
                    new wlan_emu_ext_agent_interface_t();
                strncpy(agent_details->agent_ip_address, ip.c_str(),
                    sizeof(agent_details->agent_ip_address) - 1);
                string_mac_to_uint8_mac(agent_details->agent_mac, (char *)mac.c_str());
                uint8_mac_to_string_mac(agent_details->agent_mac, mac_str);
                hash_map_put(ext_agent_map, strdup(mac_str), agent_details);
                count_ext++;
            }
        }
    }
    arp_file.close();

    this->external_agent_count = count_ext;
    wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d count is %d\n", __func__, __LINE__,
        this->external_agent_count);
#endif
    return RETURN_OK;
}

int wlan_emu_ext_agent_interface_t::get_external_agent_capabilities(hash_map_t *ext_agent_map)
{
    std::string response;
    char capability_url[128] = { 0 };
    char mac_str[32] = { 0 };
    long status_code = 0;

    wlan_emu_ext_agent_interface_t *agent_details =
        (wlan_emu_ext_agent_interface_t *)hash_map_get_first(ext_agent_map);
    if (agent_details == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: No external agent found\n", __func__,
            __LINE__);
        return RETURN_OK;
    }

    while (agent_details != NULL) {
        snprintf(capability_url, sizeof(capability_url), "%s:%d/%s",
            agent_details->agent_ip_address, 1234, "Capability");

        if (http_get(capability_url, response, status_code, m_ui_mgr->cci_error_code) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: http_get failed for %s\n", __func__,
                __LINE__, capability_url);
            agent_details = (wlan_emu_ext_agent_interface_t *)hash_map_get_next(ext_agent_map,
                agent_details);
            continue;
        }

        if (decode_external_agent_capability(response, agent_details) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: decode_config_file failed for %s\n",
                __func__, __LINE__, response.c_str());
            uint8_mac_to_string_mac(agent_details->agent_mac, mac_str);
            agent_details = (wlan_emu_ext_agent_interface_t *)hash_map_get_next(ext_agent_map,
                agent_details);
            continue;
        }
        agent_details = (wlan_emu_ext_agent_interface_t *)hash_map_get_next(ext_agent_map,
            agent_details);
    }
    return RETURN_OK;
}

int wlan_emu_ext_agent_interface_t::get_external_agent_test_status(ext_agent_status_resp_t &status)
{
    long status_code;
    std::string response;
    std::string url;

    url = agent_proto + std::string(agent_ip_address) + agent_port + status_endpoint;

    if (http_get(url, response, status_code, m_ui_mgr->cci_error_code) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to send get status request\n",
            __func__, __LINE__);
        return RETURN_ERR;
    }

    if (status_code != http_status_code_ok) {
        m_ui_mgr->cci_error_code = EEXTAGENT;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: http status code: %d\n", __func__, __LINE__,
            status_code);
        return RETURN_ERR;
    }

    if (decode_external_agent_test_status(response, status) != RETURN_OK) {
        m_ui_mgr->cci_error_code = EEXTAGENT;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: failed to decode status\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int wlan_emu_ext_agent_interface_t::send_external_agent_command(const std::string &command)
{
    long status_code;
    std::string url;

    url = agent_proto + std::string(agent_ip_address) + agent_port + command_endpoint;
    if (http_post(url, command, status_code, m_ui_mgr->cci_error_code) != RETURN_OK) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to send %s command\n", __func__,
            __LINE__, command.c_str());
        return RETURN_ERR;
    }

    if (status_code != http_status_code_ok) {
        m_ui_mgr->cci_error_code = EEXTAGENT;
        wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: http status code: %d\n", __func__, __LINE__,
            status_code);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int wlan_emu_ext_agent_interface_t::send_external_agent_start_command()
{
    return send_external_agent_command("start");
}

int wlan_emu_ext_agent_interface_t::send_external_agent_stop_command()
{
    return send_external_agent_command("stop");
}

int wlan_emu_ext_agent_interface_t::encode_external_agent_test_status(
    const ext_agent_status_resp_t &status, std::string &json_str)
{
    char *str;
    cJSON *json, *step_arr, *step_elem, *file_arr;

    json = cJSON_CreateObject();
    if (json == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to create json object\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }
    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json_wrapper(json, cJSON_Delete);

    encode_param_string(json, "ExternalAgentName", status.agent_name.c_str());
    encode_param_integer(json, "ExternalAgentStepCount", status.step_count);
    encode_param_string(json, "ExternalAgentStatus", agent_state_as_string(status.state).c_str());

    encode_param_array(json, "ExternalAgentStepStatus", step_arr);
    for (const ext_agent_step_status_t &step : status.steps) {
        encode_param_array_item_object(step_arr, step_elem);
        encode_param_integer(step_elem, "StepNumber", step.step_number);
        encode_param_string(step_elem, "StepStatus", step_state_as_string(step.state).c_str());
        encode_param_array(step_elem, "ResultFiles", file_arr);
        for (const std::string &file : step.result_files) {
            encode_param_array_item_string(file_arr, file.c_str());
        }
    }

    str = cJSON_Print(json);
    if (str == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to create json str\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    json_str = str;

    cJSON_free(str);

    return RETURN_OK;
}

int wlan_emu_ext_agent_interface_t::decode_external_agent_test_status(const std::string &json_str,
    ext_agent_status_resp_t &status)
{
    cJSON *json_root, *param, *step_arr, *step_param, *step_elem, *file_arr, *file_elem;

    json_root = cJSON_Parse(json_str.c_str());
    if (json_root == NULL) {
        wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to parse json\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json_wrapper(json_root, cJSON_Delete);

    decode_param_string(json_root, "ExternalAgentHostName", param);
    status.agent_name = std::string(param->valuestring);
    decode_param_integer(json_root, "ExternalAgentStepCount", param);
    status.step_count = param->valueint;
    decode_param_string(json_root, "ExternalAgentStatus", param);
    status.state = agent_state_as_enum(param->valuestring);

    decode_param_array(json_root, "ExternalAgentStepStatus", step_arr);
    cJSON_ArrayForEach(step_elem, step_arr) {
        status.steps.emplace_back();
        ext_agent_step_status_t &step_status = status.steps.back();
        decode_param_integer(step_elem, "StepNumber", step_param);
        step_status.step_number = step_param->valueint;
        decode_param_string(step_elem, "StepStatus", step_param);
        step_status.state = step_state_as_enum(step_param->valuestring);

        if (step_status.state == wlan_emu_tests_state_cmd_results) {
            decode_param_array(step_elem, "ResultFiles", file_arr);
            cJSON_ArrayForEach(file_elem, file_arr) {
                validate_param_string(file_elem);
                step_status.result_files.emplace_back(file_elem->valuestring);
            }
        }
    }

    return RETURN_OK;
}

int wlan_emu_ext_agent_interface_t::download_external_agent_result_files(
    const std::vector<std::string> &files)
{
    std::string agent_url = agent_proto + std::string(agent_ip_address) + agent_port +
        std::string("/Results/");

    for (const std::string &file_name : files) {
        std::string url = agent_url + file_name;
        std::string file_path = file_name;
        long status_code;

        if (http_get_file(url, file_path, status_code, m_ui_mgr->cci_error_code) != RETURN_OK) {
            wlan_emu_print(wlan_emu_log_level_err, "%s:%d: failed to download %s\n", __func__,
                __LINE__, file_name.c_str());
            return RETURN_ERR;
        }

        if (status_code != http_status_code_ok) {
            m_ui_mgr->cci_error_code = EEXTAGENT;
            wlan_emu_print(wlan_emu_log_level_dbg, "%s:%d: http status code: %d\n", __func__,
                __LINE__, status_code);
            return RETURN_ERR;
        }
    }

    return RETURN_OK;
}

wlan_emu_ext_agent_interface_t::wlan_emu_ext_agent_interface_t()
{
    this->client_info = NULL;
}

wlan_emu_ext_agent_interface_t::~wlan_emu_ext_agent_interface_t()
{
}
