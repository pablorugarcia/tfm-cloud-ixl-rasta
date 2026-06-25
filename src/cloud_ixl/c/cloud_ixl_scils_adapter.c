#include <stdbool.h>
#include <stdint.h>
#include <cloud_ixl_types.h>
#include <cloud_ixl_scils_adapter.h>
#include <sci_telegram_factory.h>
#include <scils_telegram_factory.h>
#include <scils.h>
#include <rmemory.h>
#include <sci_ls_icd.h>

#include <string.h>

bool cloud_ixl_build_signal_vector(SignalAspect aspect, uint8_t vector[SCI_LS_ICD_SIGNAL_VECTOR_SIZE]){
    static const uint8_t signal_vectors[APAGADA + 1][SCI_LS_ICD_SIGNAL_VECTOR_SIZE] = {
        [VIA_LIBRE] = {
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x01, 0xFE, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        },
        [PARADA] = {
            0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x01, 0xFE, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        },
        [ANUNCIO_PARADA] = {
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x01, 0xFE, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        },
        [VIA_LIBRE_CONDICIONAL] = {
            0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x01, 0xFE, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        },
        [ANUNCIO_PRECAUCION] = {
            0x05, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x01, 0xFE, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        },
        [REBASE] = {
            0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x01, 0x01, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        },
        [PARADA_SELECTIVA_N2] = {
            0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x01, 0x02, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        },
        [REBASE_AUTORIZADO] = {
            0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x01, 0x03, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        },
        [PARADA_SELECTIVA_N1] = {
            0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x01, 0x04, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        },
        [APAGADA] = {
            0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xFE, 0xFE, 0x0F, 0xFE, 0xFE, 0xFE,
            0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE
        }
    };

    if(vector == NULL){
        return false;
    }

    if (aspect < VIA_LIBRE || aspect > APAGADA) {
        return false;
    }

    memcpy(vector, signal_vectors[aspect], SCI_LS_ICD_SIGNAL_VECTOR_SIZE);
    return true;
}


sci_telegram *cloud_ixl_create_signal_aspect_telegram(char *sender, char *receiver, SignalAspect aspect){
    uint8_t vector[SCI_LS_ICD_SIGNAL_VECTOR_SIZE];

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
    if (telegram == NULL) {
        return NULL;
    }

    telegram->payload.used_bytes = SCI_LS_ICD_SIGNAL_VECTOR_SIZE;

    memcpy(telegram->payload.data, vector, SCI_LS_ICD_SIGNAL_VECTOR_SIZE);

    return telegram;

}

CloudIxlScilsSendResult cloud_ixl_scils_send_signal_aspect(scils_t *scils, char *receiver, SignalAspect aspect){
    
    if (scils == NULL || receiver == NULL) {
        return CLOUD_IXL_SCILS_BUILD_ERROR;
    }

    sci_telegram *telegram = cloud_ixl_create_signal_aspect_telegram(scils->sciName, receiver, aspect);

    if (telegram == NULL) {
        return CLOUD_IXL_SCILS_BUILD_ERROR;
    }

    sci_return_code code = scils_send_telegram(scils, telegram);

    rfree(telegram);

    if (code != SUCCESS) {
        return CLOUD_IXL_SCILS_SEND_ERROR;
    }
    return SUCCESS_SCILS;

}
