#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
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
#define PDI_ESTABLISHMENT_TIMEOUT_SECONDS 10

#define CONFIG_PATH "config/rasta_client1_local.cfg"
#define OC_RASTA_ID 0x61UL
#define DEFAULT_SIGNAL_LUMINOSITY SCILS_BRIGHTNESS_DAY

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

static bool is_pdi_establishment_terminal(CloudIxlPdiState state)
{
    return state == PDI_ESTABLISHED || state == PDI_FAILED;
}

static bool is_luminosity_status_allowed(CloudIxlPdiState state)
{
    return state == PDI_RECEIVING_INITIAL_STATUS ||
           state == PDI_ESTABLISHED ||
           state == PDI_WAIT_CONFIRMATION ||
           state == PDI_COMMAND_CONFIRMED;
}

static void set_pdi_state_locked(CloudIxlPdiState state)
{
    pdi_state = state;
    pthread_cond_broadcast(&confirmation_condition);
}

static void set_pdi_state(CloudIxlPdiState state)
{
    pthread_mutex_lock(&confirmation_lock);
    set_pdi_state_locked(state);
    pthread_mutex_unlock(&confirmation_lock);
}

static bool require_pdi_state(CloudIxlPdiState expected)
{
    bool valid;

    pthread_mutex_lock(&confirmation_lock);
    valid = pdi_state == expected;
    if (!valid) {
        set_pdi_state_locked(PDI_FAILED);
    }
    pthread_mutex_unlock(&confirmation_lock);

    return valid;
}

static bool transition_pdi_state(
    CloudIxlPdiState expected,
    CloudIxlPdiState next)
{
    bool transitioned;

    pthread_mutex_lock(&confirmation_lock);
    transitioned = pdi_state == expected;
    set_pdi_state_locked(transitioned ? next : PDI_FAILED);
    pthread_mutex_unlock(&confirmation_lock);

    return transitioned;
}

static CloudIxlPdiState wait_for_pdi_establishment(void)
{
    time_t now = time(NULL);
    struct timespec deadline;
    CloudIxlPdiState state;

    if (now == (time_t)-1) {
        set_pdi_state(PDI_FAILED);
        return PDI_FAILED;
    }

    deadline.tv_sec = now + PDI_ESTABLISHMENT_TIMEOUT_SECONDS;
    deadline.tv_nsec = 0;

    pthread_mutex_lock(&confirmation_lock);
    while (!is_pdi_establishment_terminal(pdi_state)) {
        int wait_result =
            pthread_cond_timedwait(
                &confirmation_condition,
                &confirmation_lock,
                &deadline
            );

        if (wait_result == ETIMEDOUT) {
            printf(
                "PDI: establishment timeout after %d seconds\n",
                PDI_ESTABLISHMENT_TIMEOUT_SECONDS
            );
            set_pdi_state_locked(PDI_FAILED);
            break;
        }

        if (wait_result != 0) {
            printf("PDI: establishment wait failed: %d\n", wait_result);
            set_pdi_state_locked(PDI_FAILED);
            break;
        }
    }

    state = pdi_state;
    pthread_mutex_unlock(&confirmation_lock);

    return state;
}

static void fail_pending_signal_confirmation_locked(void)
{
    if (!command_confirmation_done) {
        command_confirmation_done = true;
        command_confirmation_ok = false;
    }

    set_pdi_state_locked(PDI_FAILED);
}

static void fail_pending_signal_confirmation(void)
{
    pthread_mutex_lock(&confirmation_lock);

    fail_pending_signal_confirmation_locked();

    pthread_mutex_unlock(&confirmation_lock);
}

static void finish_signal_confirmation(bool matched)
{
    pthread_mutex_lock(&confirmation_lock);

    command_confirmation_done = true;
    command_confirmation_ok = matched;
    set_pdi_state_locked(
        matched ? PDI_COMMAND_CONFIRMED : PDI_COMMAND_MISMATCH
    );

    pthread_mutex_unlock(&confirmation_lock);
}

void on_rasta_handshake(struct rasta_notification_result *result){
    if (result->connection.remote_id != OC_RASTA_ID) {
        printf("Handshake with an unnespected RaSTA node\n");
        set_pdi_state(PDI_FAILED);
        return;
    }

    printf("Completed RaSTA handshake with LS_OC\n");

    set_pdi_state(PDI_WAIT_VERSION_RESPONSE);

    sci_return_code code = scils_send_version_request(scils, "LS_OC", PDI_VERSION);

    if (code != SUCCESS) {
        printf("PDI version request couldn't be sent\n");
        set_pdi_state(PDI_FAILED);
        return;
    }
}

static void handle_icd_version_response(
    scils_t *ls,
    const sci_ls_icd_version_response *response
);

static void handle_icd_execution_error(
    const sci_ls_icd_execution_error *error
);

static void on_brightness_status(
    scils_t *ls,
    char *sender,
    scils_brightness brightness
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
    bool waiting_for_signal_confirmation =
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
            set_pdi_state(PDI_FAILED);
            return;
        }

        handle_icd_version_response(scils, &response);
        return;
    }

    if (telegram->protocol_type == SCI_PROTOCOL_LS &&
        sci_get_message_type(telegram) ==
            SCILS_MESSAGE_TYPE_SIGNAL_BRIGHTNESS_STATUS) {

        unsigned char luminosity;
        sci_ls_icd_parse_result parse_result =
            sci_ls_icd_parse_luminosity_status(
                telegram,
                &luminosity
            );

        if (parse_result != SCI_LS_ICD_PARSE_SUCCESS) {
            rfree(telegram);
            printf(
                "PDI: invalid luminosity status telegram: %d\n",
                (int)parse_result
            );
            fail_pending_signal_confirmation();
            return;
        }

        on_brightness_status(
            scils,
            telegram->sender,
            (scils_brightness)luminosity
        );
        rfree(telegram);
        return;
    }

    if (telegram->protocol_type == SCI_PROTOCOL_LS &&
        sci_get_message_type(telegram) ==
            SCI_LS_ICD_MESSAGE_TYPE_EXECUTION_ERROR) {

        sci_ls_icd_execution_error error;
        sci_ls_icd_parse_result parse_result =
            sci_ls_icd_parse_execution_error(
                telegram,
                &error
            );

        rfree(telegram);

        if (parse_result != SCI_LS_ICD_PARSE_SUCCESS) {
            printf(
                "PDI: invalid execution error telegram: %d\n",
                (int)parse_result
            );
            fail_pending_signal_confirmation();
            return;
        }

        handle_icd_execution_error(&error);
        return;
    }

    if (telegram->protocol_type == SCI_PROTOCOL_LS &&
        sci_get_message_type(telegram) ==
            SCILS_MESSAGE_TYPE_SIGNAL_ASPECT_STATUS &&
        waiting_for_signal_confirmation) {

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
            pthread_mutex_lock(&confirmation_lock);
            if (pdi_state == PDI_WAIT_CONFIRMATION) {
                fail_pending_signal_confirmation_locked();
            } else {
                set_pdi_state_locked(PDI_DISCONNECTED);
            }
            pthread_mutex_unlock(&confirmation_lock);
            break;

        case RASTA_CONNECTION_DOWN:
            printf("DOWN\n");
            pthread_mutex_lock(&confirmation_lock);
            if (pdi_state == PDI_WAIT_CONFIRMATION) {
                fail_pending_signal_confirmation_locked();
            } else {
                set_pdi_state_locked(PDI_WAIT_RASTA_HANDSHAKE);
            }
            pthread_mutex_unlock(&confirmation_lock);
            break;

        case RASTA_CONNECTION_START:
            printf("START\n");
            pthread_mutex_lock(&confirmation_lock);
            if (pdi_state == PDI_WAIT_CONFIRMATION) {
                fail_pending_signal_confirmation_locked();
            } else {
                set_pdi_state_locked(PDI_WAIT_RASTA_HANDSHAKE);
            }
            pthread_mutex_unlock(&confirmation_lock);
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

static const char *execution_error_code_to_string(unsigned char error_code)
{
    switch (error_code) {
        case SCI_LS_ICD_EXECUTION_ERROR_LAMP_FAILURE:
            return "lamp failure";

        case SCI_LS_ICD_EXECUTION_ERROR_UNKNOWN_SIGNAL_VECTOR:
            return "unknown or incomplete signal vector";

        case SCI_LS_ICD_EXECUTION_ERROR_INVALID_HEADER_RECEIVER_OR_TYPE:
            return "invalid header, receiver or message type";

        case SCI_LS_ICD_EXECUTION_ERROR_INVALID_LUMINOSITY:
            return "invalid luminosity";

        default:
            return "unknown execution error";
    }
}

static void print_signal_vector(const sci_ls_icd_signal_vector *vector)
{
    printf("PDI: current signal vector:");
    for (size_t i = 0; i < SCI_LS_ICD_SIGNAL_VECTOR_SIZE; i++) {
        printf(" %02X", (unsigned int)vector->bytes[i]);
    }
    printf("\n");
}

static void handle_icd_execution_error(
    const sci_ls_icd_execution_error *error)
{
    printf(
        "PDI: execution error received: 0x%02X (%s)\n",
        (unsigned int)error->error_code,
        execution_error_code_to_string(error->error_code)
    );
    print_signal_vector(&error->current_signal_vector);

    fail_pending_signal_confirmation();
}

static void handle_icd_version_response(
    scils_t *ls,
    const sci_ls_icd_version_response *response)
{
    printf("PDI: ICD version response received\n");

    if (!require_pdi_state(PDI_WAIT_VERSION_RESPONSE)) {
        printf("PDI: version response received in invalid state\n");
        return;
    }

    if (response->requested_version != PDI_VERSION ||
        response->supported_version != PDI_VERSION ||
        response->result !=
            SCI_VERSION_CHECK_RESULT_VERSIONS_ARE_EQUAL) {
        printf("PDI: incompatible PDI version\n");
        set_pdi_state(PDI_FAILED);
        return;
    }

    set_pdi_state(PDI_WAIT_INITIALISATION_START);

    sci_return_code send_code =
        scils_send_status_request(ls, "LS_OC");

    if (send_code != SUCCESS) {
        printf("PDI: status request could not be sent\n");
        set_pdi_state(PDI_FAILED);
        return;
    }
}

static void on_initialisation_start(scils_t *ls, char *sender){
    (void)ls;
    (void)sender;

    printf("PDI: initial status transfer started\n");

    if (!transition_pdi_state(
            PDI_WAIT_INITIALISATION_START,
            PDI_RECEIVING_INITIAL_STATUS
        )) {
        printf("PDI: status begin received in an invalid state\n");
        return;
    }
}

static void on_aspect_status(scils_t *ls, char *sender, scils_signal_aspect aspect){
    (void)ls;
    (void)sender;

    if (!require_pdi_state(PDI_RECEIVING_INITIAL_STATUS)) {
        printf("PDI: aspect status received in an invalid state\n");
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

    bool valid_state;
    pthread_mutex_lock(&confirmation_lock);

    valid_state = is_luminosity_status_allowed(pdi_state);

    if (!valid_state) {
        set_pdi_state_locked(PDI_FAILED);
    }

    pthread_mutex_unlock(&confirmation_lock);

    if (!valid_state) {
        printf("PDI: brightness status received in an invalid state\n");
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

    if (!transition_pdi_state(
            PDI_RECEIVING_INITIAL_STATUS,
            PDI_ESTABLISHED
        )) {
        printf("PDI: status finish received in an invalid state\n");
        return;
    }

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
    
    set_pdi_state(PDI_WAIT_RASTA_HANDSHAKE);
    sr_connect(&h, OC_RASTA_ID, channels);
    printf("Waiting for PDI establishment...\n");

    CloudIxlPdiState established_state = wait_for_pdi_establishment();

    if (established_state != PDI_ESTABLISHED) {
        printf(
            "Cannot send commands: PDI is not established (state=%d)\n",
            (int)established_state
        );

        sr_cleanup(&h);
        return 1;
    }

    cloud_ixl_state_init(&state);

    CloudIxlScilsSendResult luminosity_result =
        cloud_ixl_scils_send_luminosity(
            scils,
            receiver,
            DEFAULT_SIGNAL_LUMINOSITY
        );

    switch (luminosity_result) {
        case SUCCESS_SCILS:
            printf("SCI-LS luminosity command sent\n");
            break;

        case CLOUD_IXL_SCILS_BUILD_ERROR:
            printf("SCI-LS luminosity telegram could not be built\n");
            sr_cleanup(&h);
            return 1;

        case CLOUD_IXL_SCILS_SEND_ERROR:
            printf("SCI-LS luminosity telegram could not be sent through RaSTA\n");
            sr_cleanup(&h);
            return 1;

        default:
            printf("Unknown SCI-LS luminosity sending result\n");
            sr_cleanup(&h);
            return 1;
    }

    int exit_code = 0;

    while(exit_code == 0){
        RouteRequest r_request = receive_route_request();
        RouteDecision decision;
        SignalAspect aspect;
        sci_telegram * telegram;
        struct RastaByteArray encoded_telegram;
        CloudIxlScilsSendResult result;
        bool release_requested;

        if(r_request.command == ROUTE_COMMAND_QUIT){
            printf("No more route commands.\n");
            break;
        }

        release_requested = r_request.command == ROUTE_COMMAND_RELEASE;
        if(release_requested){
            decision = STOP;
        } else {
            decision = request_route_decision(&state, r_request.route_id);
        }

        aspect = ls_request_command(decision);
        printf("Decision: %s\n", decision_name_to_string(decision));

        if (!cloud_ixl_build_signal_vector(aspect, expected_signal_vector.bytes)) {
            printf("SCI-LS expected signal vector could not be built\n");
            exit_code = 1;
            break;
        }

        telegram = cloud_ixl_create_signal_aspect_telegram(sender, receiver, aspect);
        if (telegram == NULL) {
            printf("There was an error in the building of the telegram: NULL\n");
            exit_code = 1;
            break;
        }
        encoded_telegram = sci_encode_telegram(telegram);

        pthread_mutex_lock(&confirmation_lock);
        command_confirmation_done = false;
        command_confirmation_ok = false;
        set_pdi_state_locked(PDI_WAIT_CONFIRMATION);
        pthread_mutex_unlock(&confirmation_lock);
        
        result = cloud_ixl_scils_send_signal_aspect(scils, "LS_OC", aspect);

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
            rfree(telegram);
            freeRastaByteArray(&encoded_telegram);
            break;
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
        } else if(release_requested){
            (void)release_route(&state, r_request.route_id);
        }
        pthread_mutex_unlock(&confirmation_lock);

        rfree(telegram);
        freeRastaByteArray(&encoded_telegram);
    }

    sr_cleanup(&h);
    return exit_code;
}
