#ifndef LST_SIMULATOR_SCILS_H
#define LST_SIMULATOR_SCILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hashmap.h>
#include <rasta_new.h>
#include <sci.h>

typedef struct scils_t scils_t;

typedef void (*scils_on_version_request_received_ptr)(
    scils_t *,
    char *,
    unsigned char
);
typedef void (*scils_on_status_request_received_ptr)(scils_t *, char *);
typedef void (*scils_on_status_begin_received_ptr)(scils_t *, char *);
typedef void (*scils_on_status_finish_received_ptr)(scils_t *, char *);

typedef struct {
    scils_on_version_request_received_ptr on_version_request_received;
    scils_on_status_request_received_ptr on_status_request_received;
    scils_on_status_begin_received_ptr on_status_begin_received;
    scils_on_status_finish_received_ptr on_status_finish_received;
} scils_notification_ptr;

struct scils_t {
    char *sciName;
    struct rasta_handle *rasta_handle;
    map_t sciNamesToRastaIds;
    scils_notification_ptr notifications;
};

scils_t *scils_init(
    struct rasta_handle *handle,
    char *sci_name
);

void scils_cleanup(scils_t *ls);

sci_return_code scils_send_version_request(
    scils_t *ls,
    char *receiver,
    unsigned char requested_version
);

sci_return_code scils_send_status_request(
    scils_t *ls,
    char *receiver
);

sci_return_code scils_send_status_begin(
    scils_t *ls,
    char *receiver
);

sci_return_code scils_send_status_finish(
    scils_t *ls,
    char *receiver
);

void scils_on_rasta_receive(
    scils_t *ls,
    rastaApplicationMessage message
);

void scils_register_sci_name(
    scils_t *ls,
    char *sci_name,
    unsigned long rasta_id
);

sci_return_code scils_send_telegram(
    scils_t *ls,
    sci_telegram *telegram
);

#ifdef __cplusplus
}
#endif

#endif
