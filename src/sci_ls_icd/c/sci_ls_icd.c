#include <stdbool.h>
#include <string.h>

#include <sci.h>
#include <sci_telegram_factory.h>
#include <scils_telegram_factory.h>
#include <sci_ls_icd.h>

static bool is_valid_sci_name(const char *name)
{
    return name != NULL &&
           name[0] != '\0' &&
           strlen(name) <= SCI_NAME_LENGTH;
}

static bool is_icd_version_result(sci_version_check_result result)
{
    return result == SCI_VERSION_CHECK_RESULT_VERSIONS_ARE_NOT_EQUAL ||
           result == SCI_VERSION_CHECK_RESULT_VERSIONS_ARE_EQUAL;
}

static bool is_icd_luminosity(unsigned char luminosity)
{
    return luminosity == SCI_LS_ICD_LUMINOSITY_DAY ||
           luminosity == SCI_LS_ICD_LUMINOSITY_NIGHT;
}

static bool is_icd_execution_error_code(unsigned char error_code)
{
    return error_code == SCI_LS_ICD_EXECUTION_ERROR_LAMP_FAILURE ||
           error_code == SCI_LS_ICD_EXECUTION_ERROR_UNKNOWN_SIGNAL_VECTOR ||
           error_code ==
               SCI_LS_ICD_EXECUTION_ERROR_INVALID_HEADER_RECEIVER_OR_TYPE ||
           error_code == SCI_LS_ICD_EXECUTION_ERROR_INVALID_LUMINOSITY;
}

sci_ls_icd_parse_result sci_ls_icd_parse_signal_aspect_status(sci_telegram *telegram, sci_ls_icd_signal_vector *vector){
    if(telegram == NULL || vector == NULL){
        return SCI_LS_ICD_PARSE_INVALID_ARGUMENT;
    }

    if(telegram->protocol_type != SCI_PROTOCOL_LS){
        return SCI_LS_ICD_PARSE_INVALID_PROTOCOL;
    }

    if(sci_get_message_type(telegram) != SCILS_MESSAGE_TYPE_SIGNAL_ASPECT_STATUS){
        return SCI_LS_ICD_PARSE_INVALID_MESSAGE_TYPE;
    }

    if(telegram->payload.used_bytes != SCI_LS_ICD_SIGNAL_VECTOR_SIZE){
        return SCI_LS_ICD_PARSE_INVALID_PAYLOAD_LENGTH;
    }
    memcpy(vector->bytes, telegram->payload.data, SCI_LS_ICD_SIGNAL_VECTOR_SIZE);

    return SCI_LS_ICD_PARSE_SUCCESS;
}

sci_ls_icd_parse_result sci_ls_icd_parse_luminosity_status(
    sci_telegram *telegram,
    unsigned char *luminosity)
{
    if (telegram == NULL || luminosity == NULL) {
        return SCI_LS_ICD_PARSE_INVALID_ARGUMENT;
    }

    if (telegram->protocol_type != SCI_PROTOCOL_LS) {
        return SCI_LS_ICD_PARSE_INVALID_PROTOCOL;
    }

    if (sci_get_message_type(telegram) !=
        SCILS_MESSAGE_TYPE_SIGNAL_BRIGHTNESS_STATUS) {
        return SCI_LS_ICD_PARSE_INVALID_MESSAGE_TYPE;
    }

    if (telegram->payload.used_bytes !=
        SCI_LS_ICD_LUMINOSITY_STATUS_PAYLOAD_SIZE) {
        return SCI_LS_ICD_PARSE_INVALID_PAYLOAD_LENGTH;
    }

    if (!is_icd_luminosity(telegram->payload.data[0])) {
        return SCI_LS_ICD_PARSE_INVALID_RESULT;
    }

    *luminosity = telegram->payload.data[0];

    return SCI_LS_ICD_PARSE_SUCCESS;
}

sci_ls_icd_parse_result sci_ls_icd_parse_execution_error(
    sci_telegram *telegram,
    sci_ls_icd_execution_error *error)
{
    if (telegram == NULL || error == NULL) {
        return SCI_LS_ICD_PARSE_INVALID_ARGUMENT;
    }

    if (telegram->protocol_type != SCI_PROTOCOL_LS) {
        return SCI_LS_ICD_PARSE_INVALID_PROTOCOL;
    }

    if (sci_get_message_type(telegram) !=
        SCI_LS_ICD_MESSAGE_TYPE_EXECUTION_ERROR) {
        return SCI_LS_ICD_PARSE_INVALID_MESSAGE_TYPE;
    }

    if (telegram->payload.used_bytes !=
        SCI_LS_ICD_EXECUTION_ERROR_PAYLOAD_SIZE) {
        return SCI_LS_ICD_PARSE_INVALID_PAYLOAD_LENGTH;
    }

    if (!is_icd_execution_error_code(telegram->payload.data[0])) {
        return SCI_LS_ICD_PARSE_INVALID_RESULT;
    }

    error->error_code = telegram->payload.data[0];
    memcpy(
        error->current_signal_vector.bytes,
        &telegram->payload.data[1],
        SCI_LS_ICD_SIGNAL_VECTOR_SIZE
    );

    return SCI_LS_ICD_PARSE_SUCCESS;
}

sci_telegram *sci_ls_icd_create_version_response(
    char *sender,
    char *receiver,
    unsigned char requested_version,
    unsigned char supported_version,
    sci_version_check_result result)
{
    if (!is_valid_sci_name(sender) ||
        !is_valid_sci_name(receiver) ||
        !is_icd_version_result(result)) {
        return NULL;
    }

    sci_telegram *telegram = sci_create_base_telegram(
        SCI_PROTOCOL_LS,
        sender,
        receiver,
        SCI_MESSAGE_TYPE_VERSION_RESPONSE
    );

    if (telegram == NULL) {
        return NULL;
    }

    telegram->payload.used_bytes =
        SCI_LS_ICD_VERSION_RESPONSE_PAYLOAD_SIZE;
    telegram->payload.data[0] = requested_version;
    telegram->payload.data[1] = supported_version;
    telegram->payload.data[2] = (unsigned char)result;

    return telegram;
}

sci_ls_icd_parse_result sci_ls_icd_parse_version_response(
    sci_telegram *telegram,
    sci_ls_icd_version_response *response)
{
    if (telegram == NULL || response == NULL) {
        return SCI_LS_ICD_PARSE_INVALID_ARGUMENT;
    }

    if (telegram->protocol_type != SCI_PROTOCOL_LS) {
        return SCI_LS_ICD_PARSE_INVALID_PROTOCOL;
    }

    if (sci_get_message_type(telegram) !=
        SCI_MESSAGE_TYPE_VERSION_RESPONSE) {
        return SCI_LS_ICD_PARSE_INVALID_MESSAGE_TYPE;
    }

    if (telegram->payload.used_bytes !=
        SCI_LS_ICD_VERSION_RESPONSE_PAYLOAD_SIZE) {
        return SCI_LS_ICD_PARSE_INVALID_PAYLOAD_LENGTH;
    }

    sci_version_check_result result =
        (sci_version_check_result)telegram->payload.data[2];

    if (!is_icd_version_result(result)) {
        return SCI_LS_ICD_PARSE_INVALID_RESULT;
    }

    response->requested_version = telegram->payload.data[0];
    response->supported_version = telegram->payload.data[1];
    response->result = result;

    return SCI_LS_ICD_PARSE_SUCCESS;
}
