#include <scils.h>

#include <memory.h>
#include <rastafactory.h>
#include <rasta_new.h>
#include <rmemory.h>
#include <sci.h>
#include <sci_telegram_factory.h>

static sci_return_code send_created_telegram(
    scils_t *ls,
    sci_telegram *telegram)
{
    sci_return_code result = scils_send_telegram(ls, telegram);
    rfree(telegram);
    return result;
}

sci_return_code scils_send_telegram(
    scils_t *ls,
    sci_telegram *telegram)
{
    struct RastaByteArray data = sci_encode_telegram(telegram);
    char *sci_name = sci_get_name_string(telegram->receiver);
    unsigned long rasta_id;
    int lookup_result = hashmap_get(
        ls->sciNamesToRastaIds,
        sci_name,
        (void **)&rasta_id
    );

    rfree(sci_name);

    if (lookup_result == MAP_MISSING) {
        return UNKNOWN_SCI_NAME;
    }

    struct RastaMessageData message_data;
    allocateRastaMessageData(&message_data, 1);
    message_data.data_array[0] = data;

    sr_send(ls->rasta_handle, rasta_id, message_data);

    freeRastaMessageData(&message_data);
    return SUCCESS;
}

scils_t *scils_init(
    struct rasta_handle *handle,
    char *sci_name)
{
    scils_t *scils = rmalloc(sizeof(scils_t));

    scils->rasta_handle = handle;
    scils->sciName = rmalloc((unsigned int)strlen(sci_name) + 1U);
    strcpy(scils->sciName, sci_name);
    scils->sciNamesToRastaIds = hashmap_new();

    scils->notifications.on_version_request_received = NULL;
    scils->notifications.on_status_request_received = NULL;
    scils->notifications.on_status_begin_received = NULL;
    scils->notifications.on_status_finish_received = NULL;

    return scils;
}

void scils_cleanup(scils_t *ls)
{
    hashmap_free(ls->sciNamesToRastaIds);
    rfree(ls->sciName);
    rfree(ls);
}

sci_return_code scils_send_version_request(
    scils_t *ls,
    char *receiver,
    unsigned char requested_version)
{
    sci_telegram *telegram = sci_create_version_request(
        SCI_PROTOCOL_LS,
        ls->sciName,
        receiver,
        requested_version
    );

    return send_created_telegram(ls, telegram);
}

sci_return_code scils_send_status_request(
    scils_t *ls,
    char *receiver)
{
    sci_telegram *telegram = sci_create_status_request(
        SCI_PROTOCOL_LS,
        ls->sciName,
        receiver
    );

    return send_created_telegram(ls, telegram);
}

sci_return_code scils_send_status_begin(
    scils_t *ls,
    char *receiver)
{
    sci_telegram *telegram = sci_create_status_begin(
        SCI_PROTOCOL_LS,
        ls->sciName,
        receiver
    );

    return send_created_telegram(ls, telegram);
}

sci_return_code scils_send_status_finish(
    scils_t *ls,
    char *receiver)
{
    sci_telegram *telegram = sci_create_status_finish(
        SCI_PROTOCOL_LS,
        ls->sciName,
        receiver
    );

    return send_created_telegram(ls, telegram);
}

static void handle_version_request(
    scils_t *ls,
    sci_telegram *telegram)
{
    unsigned char requested_version;
    sci_parse_result parse_result = sci_parse_version_request_payload(
        telegram,
        &requested_version
    );

    if (parse_result == SCI_PARSE_SUCCESS &&
        ls->notifications.on_version_request_received != NULL) {
        ls->notifications.on_version_request_received(
            ls,
            telegram->sender,
            requested_version
        );
    }
}

void scils_on_rasta_receive(
    scils_t *ls,
    rastaApplicationMessage message)
{
    sci_telegram *telegram = sci_decode_telegram(message.appMessage);

    if (telegram == NULL) {
        return;
    }

    if (telegram->protocol_type != SCI_PROTOCOL_LS) {
        rfree(telegram);
        return;
    }

    switch (sci_get_message_type(telegram)) {
        case SCI_MESSAGE_TYPE_VERSION_REQUEST:
            handle_version_request(ls, telegram);
            break;

        case SCI_MESSAGE_TYPE_STATUS_REQUEST:
            if (ls->notifications.on_status_request_received != NULL) {
                ls->notifications.on_status_request_received(
                    ls,
                    telegram->sender
                );
            }
            break;

        case SCI_MESSAGE_TYPE_STATUS_BEGIN:
            if (ls->notifications.on_status_begin_received != NULL) {
                ls->notifications.on_status_begin_received(
                    ls,
                    telegram->sender
                );
            }
            break;

        case SCI_MESSAGE_TYPE_STATUS_FINISH:
            if (ls->notifications.on_status_finish_received != NULL) {
                ls->notifications.on_status_finish_received(
                    ls,
                    telegram->sender
                );
            }
            break;

        default:
            break;
    }

    rfree(telegram);
}

void scils_register_sci_name(
    scils_t *ls,
    char *sci_name,
    unsigned long rasta_id)
{
    sci_telegram telegram;
    unsigned long existing_rasta_id;

    sci_set_sender(&telegram, sci_name);

    char *key = sci_get_name_string(telegram.sender);
    int lookup_result = hashmap_get(
        ls->sciNamesToRastaIds,
        key,
        (void **)&existing_rasta_id
    );

    if (lookup_result == MAP_MISSING) {
        int put_result = hashmap_put(
            ls->sciNamesToRastaIds,
            key,
            (any_t)rasta_id
        );

        if (put_result != MAP_OK) {
            rfree(key);
        }
    }
}
