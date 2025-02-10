#include "wlan_common_utils.h"
#include "wlan_emu_log.h"
#include <string.h>

#define ENUM_STRING_CONV_LOOKUP(enum_array, enum_data)                                            \
    for (int i = 0; i < sizeof(enum_array) / sizeof(enum_array[0]); i++) {                        \
        if (enum_array[i].enum_data == enum_data) {                                               \
            return enum_array[i].string_data;                                                     \
        }                                                                                         \
    }                                                                                             \
    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: invalid enum_data : %d\n", __func__, __LINE__, \
        enum_data);                                                                               \
    return nullptr;

#define STRING_ENUM_CONV_LOOKUP(enum_array, string_data)                                       \
    for (int i = 0; i < sizeof(enum_array) / sizeof(enum_array[0]); i++) {                     \
        if (strcmp(enum_array[i].string_data, string_data) == 0) {                             \
            return enum_array[i].enum_data;                                                    \
        }                                                                                      \
    }                                                                                          \
    wlan_emu_print(wlan_emu_log_level_err, "%s:%d: invalid string : %s\n", __func__, __LINE__, \
        string_data);                                                                          \
    return -1;

const char *wlan_common_utils::get_mfp_string(int enum_data)
{
    ENUM_STRING_CONV_LOOKUP(mfp_config, enum_data)
}

int wlan_common_utils::get_mfp_enum(const char *string_data)
{
    STRING_ENUM_CONV_LOOKUP(mfp_config, string_data)
}

const char *wlan_common_utils::get_sta_type_string(int enum_data)
{
    ENUM_STRING_CONV_LOOKUP(client_model, enum_data)
}

int wlan_common_utils::get_sta_type_enum(const char *string_data)
{
    STRING_ENUM_CONV_LOOKUP(client_model, string_data)
}

const char *wlan_common_utils::get_secu_mode_string(int enum_data)
{
    ENUM_STRING_CONV_LOOKUP(secu_mode, enum_data)
}

int wlan_common_utils::get_secu_mode_enum(const char *string_data)
{
    STRING_ENUM_CONV_LOOKUP(secu_mode, string_data)
}

const char *wlan_common_utils::get_encryption_string(int enum_data)
{
    ENUM_STRING_CONV_LOOKUP(encryption, enum_data)
}

int wlan_common_utils::get_encryption_enum(const char *string_data)
{
    STRING_ENUM_CONV_LOOKUP(encryption, string_data)
}

int wlan_common_utils::decode_pcap_frame_type(char *frame_type_str,
    frame_capture_request_t *frame_capture_req)
{
    if (strcmp(frame_type_str, "Beacon") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_cfg80211;
        frame_capture_req->cfg80211_ops |= 1 << wlan_emu_cfg80211_ops_type_start_ap;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "ProbeResponse") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_prb_resp;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "ProbeRequest") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_prb_req;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "AssociationResponse") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_assoc_resp;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "AssociationRequest") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_assoc_req;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "Authentication") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_auth;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "Deauthentication") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_deauth;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "Disassociation") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_disassoc;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "Eapol") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_eapol;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "ReassociationRequest") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_reassoc_req;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "ReassociationResponse") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_reassoc_resp;
        return RETURN_OK;
    } else if (strcmp(frame_type_str, "Action") == 0) {
        frame_capture_req->msg_type |= 1 << wlan_emu_msg_type_frm80211;
        frame_capture_req->frm80211_ops |= 1 << wlan_emu_frm80211_ops_type_action;
        return RETURN_OK;
    }

    return RETURN_ERR;
}

int wlan_common_utils::encode_pcap_frame_type(cJSON *fc_arr,
    frame_capture_request_t frame_capture_req)
{
    cJSON *frame_obj;
    if (frame_capture_req.msg_type & 1 << wlan_emu_msg_type_cfg80211) {
        if (frame_capture_req.cfg80211_ops & (1 << wlan_emu_cfg80211_ops_type_start_ap)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "Beacon");
        }
    }
    if (frame_capture_req.msg_type & 1 << wlan_emu_msg_type_frm80211) {

        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_prb_resp)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "ProbeResponse");
        }
        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_prb_req)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "ProbeRequest");
        }
        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_assoc_resp)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "AssociationResponse");
        }
        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_assoc_req)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "AssociationRequest");
        }
        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_auth)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "Authentication");
        }
        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_deauth)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "Deauthentication");
        }
        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_disassoc)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "Disassociation");

        }
        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_eapol)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "Eapol");
        }
        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_reassoc_req)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "ReassociationRequest");
        }
        if (frame_capture_req.frm80211_ops &
            (1 << wlan_emu_frm80211_ops_type_reassoc_resp)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "ReassociationResponse");
        }
        if (frame_capture_req.frm80211_ops & (1 << wlan_emu_frm80211_ops_type_action)) {
            frame_obj = cJSON_CreateObject();
            if (frame_obj == NULL) {
                return RETURN_ERR;
            }
            cJSON_AddItemToArray(fc_arr, frame_obj);
            cJSON_AddStringToObject(frame_obj, "FrameType", "Action");
        }
    }

    return RETURN_OK;
}
