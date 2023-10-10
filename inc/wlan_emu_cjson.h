#include <cjson/cJSON.h>

#define decode_param_string(json, key, value) \
{   \
    value = cJSON_GetObjectItem(json, key);     \
    if ((value == NULL) || (cJSON_IsString(value) == false) ||  \
            (value->valuestring == NULL) || (strcmp(value->valuestring, "") == 0)) {    \
        printf("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);   \
        return RETURN_ERR;  \
    }   \
}   \

#define decode_param_allow_empty_string(json, key, value) \
{   \
    value = cJSON_GetObjectItem(json, key);     \
    if ((value == NULL) || (cJSON_IsString(value) == false) ||  \
            (value->valuestring == NULL) ) {    \
        printf("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);   \
        return RETURN_ERR;  \
    }   \
}   \

#define decode_param_integer(json, key, value) \
{   \
    value = cJSON_GetObjectItem(json, key);     \
    if ((value == NULL) || (cJSON_IsNumber(value) == false)) {  \
        printf("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);   \
        return RETURN_ERR;  \
    }   \
}   \

#define decode_param_bool(json, key, value) \
{   \
    value = cJSON_GetObjectItem(json, key);     \
    if ((value == NULL) || (cJSON_IsBool(value) == false)) {    \
        printf("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);   \
        return RETURN_ERR;  \
    }   \
}   \

#define decode_param_array(json, key, value) \
{   \
    value = cJSON_GetObjectItem(json, key);     \
    if ((value == NULL) || (cJSON_IsArray(value) == false)) {   \
        printf("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);   \
        return RETURN_ERR;  \
    }   \
}   \

#define decode_param_object(json, key, value) \
{   \
    value = cJSON_GetObjectItem(json, key);     \
    if ((value == NULL) || (cJSON_IsObject(value) == false)) {  \
        printf("%s:%d: Validation failed for key:%s\n", __func__, __LINE__, key);   \
        return RETURN_ERR;  \
    }   \
}   \

