#include <stdio.h>

#include <rasta_new.h>
#include <rastautil.h>
#include <scils.h>

#define CONFIG_PATH "config/rasta_server_local.cfg"
#define IXL_RASTA_ID 0x62UL

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
    printf(
        "OC: RaSTA handshake with 0x%08lX completed\n",
        (unsigned long)result->connection.remote_id
    );
}

int main(void)
{
    struct rasta_handle handle;

    sr_init_handle(&handle, CONFIG_PATH);

    handle.notifications.on_receive = on_rasta_receive;
    handle.notifications.on_handshake_complete =
        on_rasta_handshake;

    oc_scils = scils_init(&handle, "LS_OC");
    scils_register_sci_name(
        oc_scils,
        "IXL_CENTRAL",
        IXL_RASTA_ID
    );

    printf("OC simulator listening. Press Enter to stop.\n");
    getchar();

    scils_cleanup(oc_scils);
    sr_cleanup(&handle);

    return 0;
}