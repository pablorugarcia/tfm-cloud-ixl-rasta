
#ifndef CLOUD_IXL_SCILS_ADAPTER_H
#define CLOUD_IXL_SCILS_ADAPTER_H
 
#define CLOUD_IXL_SIGNAL_VECTOR_SIZE 18U

#include <stdbool.h>
#include <stdint.h>
#include <cloud_ixl_types.h>
#include <sci.h>
#include <scils.h>


bool cloud_ixl_build_signal_vector(SignalAspect aspect, uint8_t vector[CLOUD_IXL_SIGNAL_VECTOR_SIZE]);

sci_telegram *cloud_ixl_create_signal_aspect_telegram(char *sender, char *receiver, SignalAspect aspect);

typedef enum{
    CLOUD_IXL_SCILS_BUILD_ERROR,
    CLOUD_IXL_SCILS_SEND_ERROR,
    SUCCES_SCILS,
} CloudIxlScilsSendResult;

CloudIxlScilsSendResult cloud_ixl_scils_send_signal_aspect(scils_t *scils, char *receiver, SignalAspect aspect);

#endif