#include <stdio.h>
#include <rmemory.h>
#include <sci_ls_icd.h>
#include <rasta_new.h>
#include <rastautil.h>
#include <scils.h>

#define OC_SCI_NAME "LS_OC"
#define IXL_SCI_NAME "IXL_CENTRAL"
#define SUPPORTED_PDI_VERSION 0x05U

#define CONFIG_PATH "config/rasta_server_local.cfg"
#define IXL_RASTA_ID 0x62UL

typedef enum {
    OC_PDI_WAIT_VERSION_REQUEST,
    OC_PDI_WAIT_STATUS_REQUEST,
    OC_PDI_ESTABLISHED,
    OC_PDI_FAILED
} OcPdiState;

static OcPdiState oc_pdi_state =
    OC_PDI_WAIT_VERSION_REQUEST;

static scils_t *oc_scils;

static void on_rasta_receive(
    struct rasta_notification_result *result)
{
    rastaApplicationMessage message =
        sr_get_received_data(
            result->handle,
            &result->connection
        );

    scils_on_rasta_receive(oc_scils, message);
}

static void on_rasta_handshake(
    struct rasta_notification_result *result)
{
    oc_pdi_state = OC_PDI_WAIT_VERSION_REQUEST;
    printf(
        "OC: RaSTA handshake with 0x%08lX completed\n",
        (unsigned long)result->connection.remote_id
    );
}

static void on_version_request(
    scils_t *ls,
    char *sender,
    unsigned char requested_version)
{
    (void)sender;
    if (oc_pdi_state != OC_PDI_WAIT_VERSION_REQUEST) {
        printf("OC: version request received in invalid state\n");
        oc_pdi_state = OC_PDI_FAILED;
        return;
    }
    sci_version_check_result comparison_result;

    if (requested_version == SUPPORTED_PDI_VERSION) {
        comparison_result =
            SCI_VERSION_CHECK_RESULT_VERSIONS_ARE_EQUAL;
    } else {
        comparison_result =
            SCI_VERSION_CHECK_RESULT_VERSIONS_ARE_NOT_EQUAL;
    }

    printf(
        "OC: version requested=0x%02X, supported=0x%02X\n",
        requested_version,
        SUPPORTED_PDI_VERSION
    );

    sci_telegram *response =
        sci_ls_icd_create_version_response(
            OC_SCI_NAME,
            IXL_SCI_NAME,
            requested_version,
            SUPPORTED_PDI_VERSION,
            comparison_result
        );

    if (response == NULL) {
        printf("OC: version response could not be built\n");
        return;
    }

    sci_return_code send_code =
        scils_send_telegram(ls, response);

    rfree(response);

    if (send_code != SUCCESS) {
        printf("OC: version response could not be sent\n");
        return;
    }

    printf("OC: version response sent\n");

    if (comparison_result !=
        SCI_VERSION_CHECK_RESULT_VERSIONS_ARE_EQUAL) {
        printf("OC: requested PDI version is incompatible\n");
        oc_pdi_state = OC_PDI_FAILED;
        return;
    }

    oc_pdi_state = OC_PDI_WAIT_STATUS_REQUEST;
}

static void on_status_request(
    scils_t *ls,
    char *sender)
{
    (void)sender;

    if (oc_pdi_state != OC_PDI_WAIT_STATUS_REQUEST) {
        printf("OC: status request received in invalid state\n");
        oc_pdi_state = OC_PDI_FAILED;
        return;
    }

    sci_return_code send_code =
        scils_send_status_begin(ls, IXL_SCI_NAME);

    if (send_code != SUCCESS) {
        printf("OC: Status Begin could not be sent\n");
        oc_pdi_state = OC_PDI_FAILED;
        return;
    }

    printf("OC: Status Begin sent\n");

    send_code =
        scils_send_status_finish(ls, IXL_SCI_NAME);

    if (send_code != SUCCESS) {
        printf("OC: Status Finish could not be sent\n");
        oc_pdi_state = OC_PDI_FAILED;
        return;
    }

    oc_pdi_state = OC_PDI_ESTABLISHED;
    printf("OC: PDI established\n");
}

int main(void){
    struct rasta_handle handle;

    sr_init_handle(&handle, CONFIG_PATH);

    handle.notifications.on_receive = on_rasta_receive;
    handle.notifications.on_handshake_complete =
        on_rasta_handshake;

    oc_scils = scils_init(&handle, OC_SCI_NAME);
    oc_scils->notifications.on_version_request_received = on_version_request;
    oc_scils->notifications.on_status_request_received = on_status_request;
    scils_register_sci_name(oc_scils, IXL_SCI_NAME, IXL_RASTA_ID);

    printf("OC simulator listening. Press Enter to stop.\n");
    getchar();

    scils_cleanup(oc_scils);
    sr_cleanup(&handle);

    return 0;
}