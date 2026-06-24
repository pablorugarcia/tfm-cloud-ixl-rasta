#ifndef SCI_LS_ICD_H
#define SCI_LS_ICD_H

#include <sci.h>

#define SCI_LS_ICD_VERSION_RESPONSE_PAYLOAD_SIZE 3U

typedef struct {
    unsigned char requested_version;
    unsigned char supported_version;
    sci_version_check_result result;
} sci_ls_icd_version_response;

typedef enum {
    SCI_LS_ICD_PARSE_SUCCESS = 0,
    SCI_LS_ICD_PARSE_INVALID_ARGUMENT,
    SCI_LS_ICD_PARSE_INVALID_PROTOCOL,
    SCI_LS_ICD_PARSE_INVALID_MESSAGE_TYPE,
    SCI_LS_ICD_PARSE_INVALID_PAYLOAD_LENGTH,
    SCI_LS_ICD_PARSE_INVALID_RESULT
} sci_ls_icd_parse_result;

/*
 * Project-specific SCI-LS Version Response (0x0025):
 * [requested version][supported version][comparison result].
 */
sci_telegram *sci_ls_icd_create_version_response(
    char *sender,
    char *receiver,
    unsigned char requested_version,
    unsigned char supported_version,
    sci_version_check_result result
);

sci_ls_icd_parse_result sci_ls_icd_parse_version_response(
    sci_telegram *telegram,
    sci_ls_icd_version_response *response
);

#endif
