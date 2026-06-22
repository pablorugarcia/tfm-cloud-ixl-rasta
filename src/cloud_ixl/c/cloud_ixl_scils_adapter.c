#include <stdbool.h>
#include <stdint.h>
#include <cloud_ixl_types.h>
#include <cloud_ixl_scils_adapter.h>
#include <sci_telegram_factory.h>
#include <scils_telegram_factory.h>

#include <stddef.h>
#include <string.h>
bool cloud_ixl_build_signal_vector(SignalAspect aspect, uint8_t vector[CLOUD_IXL_SIGNAL_VECTOR_SIZE]){
    if(vector == NULL){
        return false;
    }

    /* TODO: Centralize national bytes 9-17; byte 9 currently encodes WHITE/BLUE per the project ICD. */
    
    switch (aspect)
    {
    case GREEN:
        vector[0] = 0x01;
        vector[1] = 0x00;
        vector[2] = 0x00;
        vector[3] = 0x00;
        vector[4] = 0x00;
        vector[5] = 0x00;
        vector[6] = 0xFE;
        vector[7] = 0xFE;
        vector[8] = 0x01;
        vector[9] = 0xFE;
        vector[10] = 0xFE;
        vector[11] = 0xFE;
        vector[12] = 0xFE;
        vector[13] = 0xFE;
        vector[14] = 0xFE;
        vector[15] = 0xFE;
        vector[16] = 0xFE;
        vector[17] = 0xFE;
        return true;
    case RED:
        vector[0] = 0x02;
        vector[1] = 0x00;
        vector[2] = 0x00;
        vector[3] = 0x00;
        vector[4] = 0x00;
        vector[5] = 0x00;
        vector[6] = 0xFE;
        vector[7] = 0xFE;
        vector[8] = 0x01;
        vector[9] = 0xFE;
        vector[10] = 0xFE;
        vector[11] = 0xFE;
        vector[12] = 0xFE;
        vector[13] = 0xFE;
        vector[14] = 0xFE;
        vector[15] = 0xFE;
        vector[16] = 0xFE;
        vector[17] = 0xFE;
        return true;

    case YELLOW:
        vector[0] = 0x03;
        vector[1] = 0x00;
        vector[2] = 0x00;
        vector[3] = 0x00;
        vector[4] = 0x00;
        vector[5] = 0x00;
        vector[6] = 0xFE;
        vector[7] = 0xFE;
        vector[8] = 0x01;
        vector[9] = 0xFE;
        vector[10] = 0xFE;
        vector[11] = 0xFE;
        vector[12] = 0xFE;
        vector[13] = 0xFE;
        vector[14] = 0xFE;
        vector[15] = 0xFE;
        vector[16] = 0xFE;
        vector[17] = 0xFE;
        return true;
    case GREEN_FLASHING:
        vector[0] = 0x04;
        vector[1] = 0x00;
        vector[2] = 0x00;
        vector[3] = 0x00;
        vector[4] = 0x00;
        vector[5] = 0x00;
        vector[6] = 0xFE;
        vector[7] = 0xFE;
        vector[8] = 0x01;
        vector[9] = 0xFE;
        vector[10] = 0xFE;
        vector[11] = 0xFE;
        vector[12] = 0xFE;
        vector[13] = 0xFE;
        vector[14] = 0xFE;
        vector[15] = 0xFE;
        vector[16] = 0xFE;
        vector[17] = 0xFE;
        return true;
    case WHITE:
        vector[0] = 0xFE;
        vector[1] = 0x00;
        vector[2] = 0x00;
        vector[3] = 0x00;
        vector[4] = 0x00;
        vector[5] = 0x00;
        vector[6] = 0xFE;
        vector[7] = 0xFE;
        vector[8] = 0x01;
        vector[9] = 0x01;
        vector[10] = 0xFE;
        vector[11] = 0xFE;
        vector[12] = 0xFE;
        vector[13] = 0xFE;
        vector[14] = 0xFE;
        vector[15] = 0xFE;
        vector[16] = 0xFE;
        vector[17] = 0xFE;
        return true;
    case BLUE:
        vector[0] = 0xFE;
        vector[1] = 0x00;
        vector[2] = 0x00;
        vector[3] = 0x00;
        vector[4] = 0x00;
        vector[5] = 0x00;
        vector[6] = 0xFE;
        vector[7] = 0xFE;
        vector[8] = 0x01;
        vector[9] = 0x02;
        vector[10] = 0xFE;
        vector[11] = 0xFE;
        vector[12] = 0xFE;
        vector[13] = 0xFE;
        vector[14] = 0xFE;
        vector[15] = 0xFE;
        vector[16] = 0xFE;
        vector[17] = 0xFE;
        return true;
    
    default:
        return false;
    }
}


sci_telegram *cloud_ixl_create_signal_aspect_telegram(char *sender, char *receiver, SignalAspect aspect){
    uint8_t vector[CLOUD_IXL_SIGNAL_VECTOR_SIZE];

    if (!cloud_ixl_build_signal_vector(aspect, vector)) {
    return NULL;
    }
    if (sender == NULL || receiver == NULL){
        return NULL;
    }
    if (strlen(sender) > SCI_NAME_LENGTH || strlen(receiver) > SCI_NAME_LENGTH){
        return NULL;
    }
    if (sender[0] == '\0' || receiver[0] == '\0') {
    return NULL;
    }

    sci_telegram * telegram = sci_create_base_telegram(SCI_PROTOCOL_LS, sender, receiver, SCILS_MESSAGE_TYPE_SHOW_SIGNAL_ASPECT);
    cloud_ixl_build_signal_vector(aspect, vector);
    telegram->payload.used_bytes = CLOUD_IXL_SIGNAL_VECTOR_SIZE;

    for (size_t i = 0; i < CLOUD_IXL_SIGNAL_VECTOR_SIZE; i++)
    {
        telegram->payload.data[i] = vector[i];
    }

    return telegram;

}