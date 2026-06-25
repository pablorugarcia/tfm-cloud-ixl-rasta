#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <cloud_ixl_routes.h>
#include <cloud_ixl_state.h>
#include <cloud_ixl_types.h>
#include <cloud_ixl_input.h>
#include <cloud_ixl_logic.h>
#include <cloud_ixl_format.h>
#include <cloud_ixl_output.h>
#include <cloud_ixl_scils_adapter.h>
#include <rmemory.h>
#include <sci.h>
#include <rastautil.h>
#include <rasta_new.h>
#include <sci_ls_icd.h>
#include <sci_telegram_factory.h>

#define PDI_VERSION 0x05U

#define CONFIG_PATH "config/rasta_client1_local.cfg"
#define OC_RASTA_ID 0x61UL

typedef enum {
    PDI_DISCONNECTED, /*there is no conection*/
    PDI_WAIT_RASTA_HANDSHAKE, /*sr_connect() was called but RaSTA is not UP yet*/
    PDI_WAIT_VERSION_RESPONSE, /*rasta is up and the 0x0024 message was sent*/
    PDI_WAIT_INITIALISATION_START, /*version was accepted and 0x0021 was sent*/
    PDI_RECEIVING_INITIAL_STATUS, /*0x0022 was received and its waiting initial states*/
    PDI_ESTABLISHED, /*0x0023 was received and now orders can be sent*/
    PDI_WAIT_CONFIRMATION,
    PDI_COMMAND_CONFIRMED,
    PDI_COMMAND_MISMATCH,
    PDI_FAILED /*there was incompatibility, timeout or protocol error*/
} CloudIxlPdiState;

static CloudIxlPdiState pdi_state = PDI_DISCONNECTED;
static pthread_mutex_t confirmation_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t confirmation_condition = PTHREAD_COND_INITIALIZER;
static sci_ls_icd_signal_vector expected_signal_vector;
static bool command_confirmation_done = false;
static bool command_confirmation_ok = false;


scils_t * scils;

static void fail_pending_signal_confirmation(void)
{
    pthread_mutex_lock(&confirmation_lock);

    pdi_state = PDI_FAILED;

    if (!command_confirmation_done) {
        command_confirmation_done = true;
        command_confirmation_ok = false;
        pthread_cond_signal(&confirmation_condition);
    }

    pthread_mutex_unlock(&confirmation_lock);
}

static void finish_signal_confirmation(bool matched)
{
    pthread_mutex_lock(&confirmation_lock);

    command_confirmation_done = true;
    command_confirmation_ok = matched;
    pdi_state = matched ? PDI_COMMAND_CONFIRMED : PDI_COMMAND_MISMATCH;

    pthread_cond_signal(&confirmation_condition);
    pthread_mutex_unlock(&confirmation_lock);
}

void on_rasta_handshake(struct rasta_notification_result *result){
    pdi_state = PDI_WAIT_VERSION_RESPONSE;

    if (result->connection.remote_id != OC_RASTA_ID) {
        printf("Handshake with an unnespected RaSTA node\n");
        pdi_state = PDI_FAILED;
        return;
    }

    printf("Completed RaSTA handshake with LS_OC\n");

    sci_return_code code = scils_send_version_request(scils, "LS_OC", PDI_VERSION);

    if (code != SUCCESS) {
        printf("PDI version request couldn't be sent\n");
        pdi_state = PDI_FAILED;
        return;
    }

    pdi_state = PDI_WAIT_VERSION_RESPONSE;
}

static void handle_icd_version_response(
    scils_t *ls,
    const sci_ls_icd_version_response *response
);

void on_rasta_receive(struct rasta_notification_result *result)
{
    rastaApplicationMessage message =
        sr_get_received_data(
            result->handle,
            &result->connection
        );

    sci_telegram *telegram =
        sci_decode_telegram(message.appMessage);

    if (telegram == NULL) {
        printf("PDI: received data is not a valid SCI telegram\n");
        return;
    }

    pthread_mutex_lock(&confirmation_lock);
    bool waiting_for_confirmation =
        pdi_state == PDI_WAIT_CONFIRMATION;
    pthread_mutex_unlock(&confirmation_lock);

    if (telegram->protocol_type == SCI_PROTOCOL_LS && sci_get_message_type(telegram) == SCI_MESSAGE_TYPE_VERSION_RESPONSE) {

        sci_ls_icd_version_response response;

        sci_ls_icd_parse_result parse_result =
            sci_ls_icd_parse_version_response(
                telegram,
                &response
            );

        rfree(telegram);

        if (parse_result != SCI_LS_ICD_PARSE_SUCCESS) {
            printf(
                "PDI: invalid ICD version response: %d\n",
                (int)parse_result
            );
            pdi_state = PDI_FAILED;
            return;
        }

        handle_icd_version_response(scils, &response);
        return;
    }

    if (telegram->protocol_type == SCI_PROTOCOL_LS &&
        sci_get_message_type(telegram) ==
            SCILS_MESSAGE_TYPE_SIGNAL_ASPECT_STATUS &&
        waiting_for_confirmation) {

        sci_ls_icd_signal_vector reported_vector;
        sci_ls_icd_parse_result parse_result =
            sci_ls_icd_parse_signal_aspect_status(
                telegram,
                &reported_vector
            );
        rfree(telegram);

        if (parse_result != SCI_LS_ICD_PARSE_SUCCESS) {
            printf(
                "PDI: invalid parse signal aspect status: %d\n",
                (int)parse_result
            );
            fail_pending_signal_confirmation();
            return;
        }

        bool matched =
            memcmp(
                expected_signal_vector.bytes,
                reported_vector.bytes,
                SCI_LS_ICD_SIGNAL_VECTOR_SIZE
            ) == 0;

        if (!matched) {
            printf("Command and message do not match\n");
        } else {
            printf("Command and message match\n");
        }

        finish_signal_confirmation(matched);

        return;
    }

    rfree(telegram);

    scils_on_rasta_receive(scils, message);
}

static void on_connection_change(struct rasta_notification_result *result){
    printf("Cambio de conexión RaSTA con 0x%08lX: ", (unsigned long)result->connection.remote_id);

    switch (result->connection.current_state) {
        case RASTA_CONNECTION_CLOSED:
            printf("CLOSED\n");
            if (pdi_state == PDI_WAIT_CONFIRMATION) {
                fail_pending_signal_confirmation();
            } else {
                pdi_state = PDI_DISCONNECTED;
            }
            break;

        case RASTA_CONNECTION_DOWN:
            printf("DOWN\n");
            if (pdi_state == PDI_WAIT_CONFIRMATION) {
                fail_pending_signal_confirmation();
            } else {
                pdi_state = PDI_WAIT_RASTA_HANDSHAKE;
            }
            break;

        case RASTA_CONNECTION_START:
            printf("START\n");
            if (pdi_state == PDI_WAIT_CONFIRMATION) {
                fail_pending_signal_confirmation();
            } else {
                pdi_state = PDI_WAIT_RASTA_HANDSHAKE;
            }
            break;

        case RASTA_CONNECTION_UP:
            printf("UP\n");
            break;

        case RASTA_CONNECTION_RETRREQ:
            printf("RETRREQ\n");
            break;

        case RASTA_CONNECTION_RETRRUN:
            printf("RETRRUN\n");
            break;

        default:
            printf("UNKNOWN\n");
            break;
    }
}

static void on_timeout(struct rasta_notification_result *result){
    printf("Timeout RaSTA con el nodo 0x%08lX\n", (unsigned long)result->connection.remote_id);

    fail_pending_signal_confirmation();
}

static void handle_icd_version_response(
    scils_t *ls,
    const sci_ls_icd_version_response *response)
{
    printf("PDI: ICD version response received\n");

    if (pdi_state != PDI_WAIT_VERSION_RESPONSE) {
        printf("PDI: version response received in invalid state\n");
        pdi_state = PDI_FAILED;
        return;
    }

    if (response->requested_version != PDI_VERSION ||
        response->supported_version != PDI_VERSION ||
        response->result !=
            SCI_VERSION_CHECK_RESULT_VERSIONS_ARE_EQUAL) {
        printf("PDI: incompatible PDI version\n");
        pdi_state = PDI_FAILED;
        return;
    }

    sci_return_code send_code =
        scils_send_status_request(ls, "LS_OC");

    if (send_code != SUCCESS) {
        printf("PDI: status request could not be sent\n");
        pdi_state = PDI_FAILED;
        return;
    }

    pdi_state = PDI_WAIT_INITIALISATION_START;
}

static void on_initialisation_start(scils_t *ls, char *sender){
    (void)ls;
    (void)sender;

    printf("PDI: initial status transfer started\n");

    if (pdi_state != PDI_WAIT_INITIALISATION_START) {
        printf("PDI: status begin received in an invalid state\n");
        pdi_state = PDI_FAILED;
        return;
    }

    pdi_state = PDI_RECEIVING_INITIAL_STATUS;
}

static void on_aspect_status(scils_t *ls, char *sender, scils_signal_aspect aspect){
    (void)ls;
    (void)sender;

    if (pdi_state != PDI_RECEIVING_INITIAL_STATUS) {
        printf("PDI: aspect status received in an invalid state\n");
        pdi_state = PDI_FAILED;
        return;
    }

    printf("PDI: signal aspect status received, main=0x%02X\n", (unsigned int)aspect.main);
}

static void on_brightness_status(
    scils_t *ls,
    char *sender,
    scils_brightness brightness)
{
    (void)ls;
    (void)sender;

    if (pdi_state != PDI_RECEIVING_INITIAL_STATUS) {
        printf("PDI: brightness status received in an invalid state\n");
        pdi_state = PDI_FAILED;
        return;
    }

    printf(
        "PDI: brightness status received: 0x%02X\n",
        (unsigned int)brightness
    );
}

static void on_initialisation_completed(scils_t *ls, char *sender)
{
    (void)ls;
    (void)sender;

    if (pdi_state != PDI_RECEIVING_INITIAL_STATUS) {
        printf("PDI: status finish received in an invalid state\n");
        pdi_state = PDI_FAILED;
        return;
    }

    pdi_state = PDI_ESTABLISHED;
    printf("PDI: initialisation completed\n");
}


int main(void){
    IXL_state state;
    char sender[] = "IXL_CENTRAL";
    char receiver[] = "LS_OC";
    struct rasta_handle h;
    struct RastaIPData channels[2] = {0};

    strcpy(channels[0].ip, "127.0.0.1");
    channels[0].port = 8888;

    strcpy(channels[1].ip, "127.0.0.1");
    channels[1].port = 8889;

    sr_init_handle(&h, CONFIG_PATH);
    h.notifications.on_receive = on_rasta_receive;
    h.notifications.on_handshake_complete = on_rasta_handshake;
    printf("Initialising RaSTA connection with the OC...\n");
    h.notifications.on_connection_state_change = on_connection_change;
    h.notifications.on_heartbeat_timeout = on_timeout;
    scils = scils_init(&h, "IXL_CENTRAL");
    scils_register_sci_name(scils, "LS_OC", 0x61);

    scils->notifications.on_status_begin_received = on_initialisation_start;
    scils->notifications.on_signal_aspect_status_received = on_aspect_status;
    scils->notifications.on_brightness_status_received = on_brightness_status;
    scils->notifications.on_status_finish_received = on_initialisation_completed;
    
    pdi_state = PDI_WAIT_RASTA_HANDSHAKE;
    sr_connect(&h, OC_RASTA_ID, channels);
    printf("Press enter to continue. \n");    º
    getchar();

    if (pdi_state != PDI_ESTABLISHED) {
    printf(
        "Cannot send commands: PDI is not established (state=%d)\n",
        (int)pdi_state
    );

    sr_cleanup(&h);
    return 1;
    }

    cloud_ixl_state_init(&state);
    RouteRequest r_request = receive_route_request();
    RouteDecision decision = request_route_decision(&state, r_request.route_id);
    SignalAspect aspect = ls_request_command(decision); 
    printf("Decision: %s\n", decision_name_to_string(decision));

    if (!cloud_ixl_build_signal_vector(aspect, expected_signal_vector.bytes)) {
        printf("SCI-LS expected signal vector could not be built\n");
        sr_cleanup(&h);
        return 1;
    }

    sci_telegram * telegram = cloud_ixl_create_signal_aspect_telegram(sender, receiver, aspect);
    if (telegram == NULL) {
        printf("There was an error in the building of the telegram: NULL\n");
        sr_cleanup(&h);
        return 1;
    }
    struct RastaByteArray encoded_telegram = sci_encode_telegram(telegram);

    pthread_mutex_lock(&confirmation_lock);
    command_confirmation_done = false;
    command_confirmation_ok = false;
    pdi_state = PDI_WAIT_CONFIRMATION;
    pthread_mutex_unlock(&confirmation_lock);
    
    CloudIxlScilsSendResult result = cloud_ixl_scils_send_signal_aspect(scils, "LS_OC", aspect);
    int exit_code = 0;

    switch (result) {
        case SUCCESS_SCILS:
            printf("SCI-LS signal aspect command sent\n");
            break;

        case CLOUD_IXL_SCILS_BUILD_ERROR:
            printf("SCI-LS telegram could not be built\n");
            exit_code = 1;
            break;

        case CLOUD_IXL_SCILS_SEND_ERROR:
            printf("SCI-LS telegram could not be sent through RaSTA\n");
            exit_code = 1;
            break;

        default:
            printf("Unknown SCI-LS sending result\n");
            exit_code = 1;
            break;
    }
    if(result != SUCCESS_SCILS){
        fail_pending_signal_confirmation();
        sr_cleanup(&h);
        rfree(telegram);
        freeRastaByteArray(&encoded_telegram);
        return exit_code;
    }

    pthread_mutex_lock(&confirmation_lock);
    while (!command_confirmation_done &&
           pdi_state == PDI_WAIT_CONFIRMATION) {
        pthread_cond_wait(
            &confirmation_condition,
            &confirmation_lock
        );
    }

    if (!command_confirmation_ok) {
        exit_code = 1;
    }
    pthread_mutex_unlock(&confirmation_lock);

    sr_cleanup(&h);
    rfree(telegram);
    freeRastaByteArray(&encoded_telegram);
    return exit_code;
}
