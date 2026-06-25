#ifndef SCI_LS_ICD_H
#define SCI_LS_ICD_H

#include <sci.h>

#define SCI_LS_ICD_VERSION_RESPONSE_PAYLOAD_SIZE 3U

#define SCI_LS_ICD_LUMINOSITY_STATUS_PAYLOAD_SIZE 1U

#define SCI_LS_ICD_SIGNAL_VECTOR_SIZE 18U
#define SCI_LS_ICD_EXECUTION_ERROR_PAYLOAD_SIZE \
    (1U + SCI_LS_ICD_SIGNAL_VECTOR_SIZE)

/*
 * The underlying SCI helper stores message types in host order before
 * encoding, so 0x0700 is transmitted on the wire as ICD message 0x0007.
 */
#define SCI_LS_ICD_MESSAGE_TYPE_EXECUTION_ERROR 0x0700U

#define SCI_LS_ICD_EXECUTION_ERROR_LAMP_FAILURE 0x01U
#define SCI_LS_ICD_EXECUTION_ERROR_UNKNOWN_SIGNAL_VECTOR 0x02U
#define SCI_LS_ICD_EXECUTION_ERROR_INVALID_HEADER_RECEIVER_OR_TYPE 0x03U
#define SCI_LS_ICD_EXECUTION_ERROR_INVALID_LUMINOSITY 0x04U

#define SCI_LS_ICD_LUMINOSITY_DAY 0x01U
#define SCI_LS_ICD_LUMINOSITY_NIGHT 0x02U

typedef struct {
    unsigned char bytes[SCI_LS_ICD_SIGNAL_VECTOR_SIZE];
} sci_ls_icd_signal_vector;

typedef struct {
    unsigned char requested_version;
    unsigned char supported_version;
    sci_version_check_result result;
} sci_ls_icd_version_response;

typedef struct {
    unsigned char error_code;
    sci_ls_icd_signal_vector current_signal_vector;
} sci_ls_icd_execution_error;

typedef enum {
    SCI_LS_ICD_PARSE_SUCCESS = 0,
    SCI_LS_ICD_PARSE_INVALID_ARGUMENT,
    SCI_LS_ICD_PARSE_INVALID_PROTOCOL,
    SCI_LS_ICD_PARSE_INVALID_MESSAGE_TYPE,
    SCI_LS_ICD_PARSE_INVALID_PAYLOAD_LENGTH,
    SCI_LS_ICD_PARSE_INVALID_RESULT
} sci_ls_icd_parse_result;

sci_ls_icd_parse_result
sci_ls_icd_parse_signal_aspect_status(
    sci_telegram *telegram,
    sci_ls_icd_signal_vector *vector
);

sci_ls_icd_parse_result sci_ls_icd_parse_luminosity_status(
    sci_telegram *telegram,
    unsigned char *luminosity
);

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

sci_ls_icd_parse_result sci_ls_icd_parse_execution_error(
    sci_telegram *telegram,
    sci_ls_icd_execution_error *error
);

#endif
