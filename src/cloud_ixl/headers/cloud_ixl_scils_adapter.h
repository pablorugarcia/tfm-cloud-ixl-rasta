
#ifndef CLOUD_IXL_SCILS_ADAPTER_H
#define CLOUD_IXL_SCILS_ADAPTER_H
 
#define CLOUD_IXL_SIGNAL_VECTOR_SIZE 18U

#include <stdbool.h>
#include <stdint.h>
#include <cloud_ixl_types.h>
#include <sci.h>

bool cloud_ixl_build_signal_vector(SignalAspect aspect, uint8_t vector[CLOUD_IXL_SIGNAL_VECTOR_SIZE]);

sci_telegram *cloud_ixl_create_signal_aspect_telegram(char *sender, char *receiver, SignalAspect aspect);

#endif