#include <rasta_red_multiplexer.h>
#include <rastaredundancy_new.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#ifndef PIKEOS_TOOLCHAIN
#include <syscall.h>
#else
#include <lwip/sockets.h> // lwip sockets
#include <time.h>
#include <vm.h>
#endif // PIKEOS_TOOLCHAIN
#include "rmemory.h"
#include "udp.h"
#include "rastautil.h"

static void free_received_packet(struct RastaRedundancyPacket *packet) {
    if (packet == NULL) {
        return;
    }

    freeRastaByteArray(&packet->data.data);
    freeRastaByteArray(&packet->data.checksum);
    free(packet);
}

static rasta_redundancy_channel *get_channel_unlocked(
    redundancy_mux *mux, unsigned long id
) {
    for (unsigned int i = 0; i < mux->channel_count; ++i) {
        if (mux->connected_channels[i]->associated_id == id) {
            return mux->connected_channels[i];
        }
    }

    return NULL;
}

static int append_channel_unlocked(
    redundancy_mux *mux, rasta_redundancy_channel *channel
) {
    rasta_redundancy_channel **resized = rrealloc(
        mux->connected_channels,
        (mux->channel_count + 1U) * sizeof(*mux->connected_channels)
    );
    if (resized == NULL) {
        return 0;
    }

    mux->connected_channels = resized;
    mux->connected_channels[mux->channel_count] = channel;
    mux->channel_count++;
    return 1;
}

static int retire_channel_unlocked(
    redundancy_mux *mux, rasta_redundancy_channel *channel
) {
    rasta_redundancy_channel **resized = rrealloc(
        mux->retired_channels,
        (mux->retired_channel_count + 1U) * sizeof(*mux->retired_channels)
    );
    if (resized == NULL) {
        return 0;
    }

    mux->retired_channels = resized;
    mux->retired_channels[mux->retired_channel_count] = channel;
    mux->retired_channel_count++;
    return 1;
}

static void destroy_channel(rasta_redundancy_channel *channel) {
    if (channel == NULL) {
        return;
    }

    rasta_red_cleanup(channel);
    rfree(channel);
}

/* --- Notifications --- */

/**
 * wrapper for parameter in the diagnose notification thread handler
 */
struct diagnose_notification_parameter_wrapper {
    /**
     * the used redundancy multiplexer
     */
    redundancy_mux * mux;

    /**
     * value of N_diagnose
     */
    int n_diagnose;

    /**
     * current value of N_missed
     */
    int n_missed;

    /**
     * current value of T_drift
     */
    unsigned long t_drift;

    /**
     * current value of T_drift2
     */
    unsigned long t_drift2;

    /**
     * associated id of the redundancy channel this notification origins from
     */
    unsigned long channel_id;
};

/**
 * wrapper for parameter in the onNewNotification notification thread handler
 */
struct new_connection_notification_parameter_wrapper{
    /**
     * the used redundancy multiplexer
     */
    redundancy_mux * mux;

    /**
     * the id of the new redundancy channel
     */
    unsigned long id;
};

/**
 * the is the function that handles the call of the onDiagnosticsAvailable notification pointer.
 * this runs on a separate thread
 * @param connection the connection that will be used
 * @return unused
 */
void* red_on_new_connection_caller(void * wrapper){
    struct new_connection_notification_parameter_wrapper * w = (struct new_connection_notification_parameter_wrapper * )wrapper;

    //pthread_mutex_lock(&w->mux->lock);
    logger_log(w->mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy onNewConnection caller", "calling onNewConnection function");
    (*w->mux->notifications.on_new_connection)(w->mux, w->id);

    w->mux->notifications_running = (unsigned short)(w->mux->notifications_running -1);
    //pthread_mutex_unlock(&w->mux->lock);

    // free the memory of the wrapper;
    rfree(wrapper);
    return NULL;
}

/**
 * fires the onDiagnoseAvailable event.
 * This implementation will take care if the function pointer is NULL and start a thread to call the notification
 * @param mux the redundancy multiplexer that is used
 * @param id the id of the new redundacy channel
 */
void red_call_on_new_connection(redundancy_mux * mux, unsigned long id){
    //pthread_mutex_lock(&mux->lock);
    if (mux->notifications.on_new_connection == NULL){
        // notification not set, do nothing
        return;
    }

    mux->notifications_running++;
    //pthread_mutex_unlock(&mux->lock);

    pthread_t caller_thread;
    struct new_connection_notification_parameter_wrapper* wrapper =
            rmalloc(sizeof(struct new_connection_notification_parameter_wrapper));
    wrapper->mux = mux;
    wrapper->id = id;

    if (pthread_create(&caller_thread, NULL, red_on_new_connection_caller, wrapper)){
        logger_log(mux->logger, LOG_LEVEL_ERROR, "RaSTA Redundancy call onNewConnection", "error while creating thread");
        exit(1);
    }

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy call onNewConnection", "called onNewConnection");
}


/**
 * the is the function that handles the call of the onDiagnosticsAvailable notification pointer.
 * this runs on a separate thread
 * @param wrapper a wrapper that contains the mux and the diagnose data
 * @return unused
 */
void* red_on_diagnostic_caller(void * wrapper){
    struct diagnose_notification_parameter_wrapper * w = (struct diagnose_notification_parameter_wrapper * )wrapper;

    logger_log(w->mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy onDiagnostics caller", "calling onDiagnostics function");
    (*w->mux->notifications.on_diagnostics_available)(w->mux, w->n_diagnose, w->n_missed, w->t_drift, w->t_drift2, w->channel_id);

    w->mux->notifications_running = (unsigned short)(w->mux->notifications_running -1);

    // free the memory of the wrapper;
    rfree(wrapper);
    return NULL;
}

/**
 * fires the onDiagnoseAvailable event.
 * This implementation will take care if the function pointer is NULL and start a thread to call the notification
 * @param mux the redundancy multiplexer that is used
 * @param n_diagnose the value of N_Diagnose
 * @param n_missed the current value of N_missed
 * @param t_drift the current value of T_drift
 * @param t_drift2 the current value of T_drift2
 * @param id the id of the redundancy channel
 */
void red_call_on_diagnostic(redundancy_mux * mux, int n_diagnose,
                          int n_missed, unsigned long t_drift, unsigned long t_drift2, unsigned long id){
    pthread_mutex_lock(&mux->lock);
    if (mux->notifications.on_diagnostics_available == NULL){
        // notification not set, do nothing
        pthread_mutex_unlock(&mux->lock);
        return;
    }

    mux->notifications_running++;
    pthread_mutex_unlock(&mux->lock);

    pthread_t caller_thread;
    struct diagnose_notification_parameter_wrapper* wrapper = rmalloc(sizeof(struct diagnose_notification_parameter_wrapper));
    wrapper->mux = mux;
    wrapper->n_diagnose = n_diagnose;
    wrapper->n_missed = n_missed;
    wrapper->t_drift = t_drift;
    wrapper->t_drift2 = t_drift2;
    wrapper->channel_id = id;

    if (pthread_create(&caller_thread, NULL, red_on_diagnostic_caller, wrapper)){
        logger_log(mux->logger, LOG_LEVEL_ERROR, "RaSTA Redundancy call onDiagnostics", "error while creating thread");
        exit(1);
    }

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA Redundancy call onDiagnostics", "called onDiagnostics");
}



/* --------------------- */

/* ----- Thread handlers ----- */

/**
 * wrapper for parameter in the receive thread handler
 */
struct receive_thread_parameter_wrapper{
    redundancy_mux * mux;
    int channel_index;
};

/**
 * received data on a UDP socket
 * @param mux the multiplexer that is used
 * @param channel_id the index of the udp socket
 */
void receive_packet(redundancy_mux * mux, int channel_id){
#ifndef PIKEOS_TOOLCHAIN
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "Receive called");
#endif // PIKEOS_TOOLCHAIN

    // receive a packet
    unsigned char * buffer = rmalloc(sizeof(unsigned char) * MAX_DEFER_QUEUE_MSG_SIZE);

    // the sender of the received packet
    struct sockaddr_in sender;

    pthread_mutex_lock(&mux->lock);
    int fd = mux->udp_socket_fds[channel_id];
    pthread_mutex_unlock(&mux->lock);
#ifndef PIKEOS_TOOLCHAIN
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "channel %d waiting for data on fd %d...", channel_id, fd);
#endif // PIKEOS_TOOLCHAIN

    // wait for pdu
    size_t len = udp_receive(fd, buffer, MAX_DEFER_QUEUE_MSG_SIZE, &sender);
    if (len == 0) {
        rfree(buffer);
        return;
    }
#ifndef PIKEOS_TOOLCHAIN
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "channel %d received data on upd", channel_id);
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "channel %d received data len = %d", channel_id, len);
#endif // PIKEOS_TOOLCHAIN

    struct RastaByteArray* incomingData = malloc(sizeof(struct RastaByteArray));
    incomingData->length = (unsigned int)len;
    incomingData->bytes = buffer;

    rasta_hashing_context_t test;
    struct crc_options options;

    pthread_mutex_lock(&mux->lock);
    test.hash_length = mux->sr_hashing_context.hash_length;
    test.algorithm = mux->sr_hashing_context.algorithm;
    allocateRastaByteArray(&test.key, mux->sr_hashing_context.key.length);
    rmemcpy(test.key.bytes, mux->sr_hashing_context.key.bytes, mux->sr_hashing_context.key.length);
    rmemcpy(&options, &mux->config.redundancy.crc_type, sizeof(mux->config.redundancy.crc_type));
    pthread_mutex_unlock(&mux->lock);
    struct RastaRedundancyPacket *receivedPacket = malloc(sizeof(struct RastaRedundancyPacket));
    bytesToRastaRedundancyPacket(receivedPacket, incomingData,&options, &test);

    freeRastaByteArray(&test.key);
    rasta_transport_channel connected_channel;
    connected_channel.ip_address = rmalloc(IPV4_STR_LEN);
    sockaddr_to_host(sender, connected_channel.ip_address);
    connected_channel.port = ntohs(sender.sin_port);

    // find assiociated redundancy channel
    pthread_mutex_lock(&mux->lock);
    for (unsigned int i = 0; i < mux->channel_count; ++i) {
        if (receivedPacket->data.sender_id == mux->connected_channels[i]->associated_id){
            // found redundancy channel with associated id
            // need to check if redundancy channel already knows ip & port of sender
            rasta_redundancy_channel* channel = mux->connected_channels[i];
            pthread_mutex_lock(&channel->channel_lock);
            if (channel->connected_channel_count < mux->port_count){
                // not all remote transport channel endpoints discovered

                int is_channel_saved= 0;

                for (unsigned int j = 0; j < channel->connected_channel_count; ++j) {
                    if (channel->connected_channels[j].port == connected_channel.port &&
                        strcmp(connected_channel.ip_address, channel->connected_channels[j].ip_address) == 0){
                        // channel is already saved
                        is_channel_saved = 1;
                    }
                }

                if (!is_channel_saved){
                    // channel wasn't saved yet -> add to list
                    channel->connected_channels[channel->connected_channel_count].ip_address = connected_channel.ip_address;
                    channel->connected_channels[channel->connected_channel_count].port = connected_channel.port;
                    channel->connected_channel_count++;

#ifndef PIKEOS_TOOLCHAIN
                    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive", "channel %d discovered client transport channel %s:%d for connection to 0x%lX",
                               channel_id, connected_channel.ip_address, connected_channel.port, channel->associated_id);
#endif // PIKEOS_TOOLCHAIN
                } else {
                    // temp channel no longer needed -> free memory
                    rfree(connected_channel.ip_address);
                }
            } else {
                rfree(connected_channel.ip_address);
            }
            pthread_mutex_unlock(&channel->channel_lock);

            rasta_red_f_receive(channel, receivedPacket, channel_id);

            pthread_mutex_unlock(&mux->lock);
            rfree(buffer);
            free_received_packet(receivedPacket);
            free(incomingData);
            return;
        }
    }

    // no associated channel found -> received message from new partner
#ifndef PIKEOS_TOOLCHAIN
    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux receive", "received pdu from unknown entity with id=0x%lX", receivedPacket->data.sender_id);
#endif // PIKEOS_TOOLCHAIN
    rasta_redundancy_channel *new_channel = rmalloc(sizeof(*new_channel));
    if (new_channel == NULL) {
        pthread_mutex_unlock(&mux->lock);
        rfree(connected_channel.ip_address);
        free_received_packet(receivedPacket);
        free(incomingData);
        rfree(buffer);
        return;
    }

    rasta_red_init(new_channel, mux->logger, &mux->config, mux->port_count,
                   receivedPacket->data.sender_id);
    new_channel->associated_id = receivedPacket->data.sender_id;
    // add transport channel to redundancy channel
    new_channel->connected_channels[0].ip_address = connected_channel.ip_address;
    new_channel->connected_channels[0].port= connected_channel.port;
    new_channel->connected_channel_count++;

    new_channel->is_open = 1;

    if (!append_channel_unlocked(mux, new_channel)) {
        pthread_mutex_unlock(&mux->lock);
        destroy_channel(new_channel);
        free_received_packet(receivedPacket);
        free(incomingData);
        rfree(buffer);
        return;
    }

    // fire new redundancy channel notification
    red_call_on_new_connection(mux, new_channel->associated_id);

    // call receive function of new channel
    rasta_red_f_receive(new_channel, receivedPacket, channel_id);
    pthread_mutex_unlock(&mux->lock);

    // free receive buffer
    // TODO changed from previous version
    free_received_packet(receivedPacket);
    free(incomingData);
    rfree(buffer);
}

/**
 * handler for receiving data on a specific port
 * @param arg_wrapper wrapper to pass multiplexer and port index
 * @return unused
 */
void * channel_receive_handler(void * arg_wrapper){
    struct receive_thread_parameter_wrapper * args = (struct receive_thread_parameter_wrapper*)arg_wrapper;

#ifndef PIKEOS_TOOLCHAIN
    logger_log(args->mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive thread", "Thread %d running",
               args->channel_index);
#endif // PIKEOS_TOOLCHAIN

    // set diagnose window start time
    //args->mux->channels[args->channel_index].diagnostics_data.start_time = current_ts();

    // receive data until the multiplexer is closed
    pthread_mutex_lock(&args->mux->lock);
    int open = args->mux->is_open;
    pthread_mutex_unlock(&args->mux->lock);

    while (open){

        pthread_mutex_lock(&args->mux->lock);
        unsigned int mux_channel_count = args->mux->channel_count;
        pthread_mutex_unlock(&args->mux->lock);

        for (unsigned int i = 0; i < mux_channel_count; ++i) {
            pthread_mutex_lock(&args->mux->lock);
            if (i >= args->mux->channel_count) {
                pthread_mutex_unlock(&args->mux->lock);
                break;
            }
            rasta_redundancy_channel *current = args->mux->connected_channels[i];
            unsigned int n_diagnose = args->mux->config.redundancy.n_diagnose;
            pthread_mutex_unlock(&args->mux->lock);

            int notify = 0;
            int n_missed = 0;
            unsigned long t_drift = 0;
            unsigned long t_drift2 = 0;
            unsigned long associated_id = 0;

            pthread_mutex_lock(&current->channel_lock);
            unsigned long channel_diag_start_time =
                current->connected_channels[args->channel_index].diagnostics_data.start_time;

            if (current_ts() - channel_diag_start_time >= n_diagnose) {
                // increase n_missed by amount of messages that are not received

                // amount of missed packets
                unsigned int received_packets =
                    current->connected_channels[args->channel_index].diagnostics_data.received_packets;
                unsigned int buffered_packets = current->diagnostics_packet_buffer.count;
                unsigned int missed_count = buffered_packets > received_packets
                    ? buffered_packets - received_packets
                    : 0;

                // increase n_missed
                current->connected_channels[args->channel_index].diagnostics_data.n_missed +=
                    (int)missed_count;

                n_missed = current->connected_channels[args->channel_index].diagnostics_data.n_missed;
                t_drift = current->connected_channels[args->channel_index].diagnostics_data.t_drift;
                t_drift2 = current->connected_channels[args->channel_index].diagnostics_data.t_drift2;
                associated_id = current->associated_id;
                notify = 1;

                // reset values
                current->connected_channels[args->channel_index].diagnostics_data.n_missed = 0;
                current->connected_channels[args->channel_index].diagnostics_data.received_packets = 0;
                current->connected_channels[args->channel_index].diagnostics_data.t_drift = 0;
                current->connected_channels[args->channel_index].diagnostics_data.t_drift2 = 0;
                current->connected_channels[args->channel_index].diagnostics_data.start_time = current_ts();

                deferqueue_clear(&current->diagnostics_packet_buffer);
            }
            pthread_mutex_unlock(&current->channel_lock);

            if (notify) {
                red_call_on_diagnostic(args->mux, (int)n_diagnose, n_missed,
                                       t_drift, t_drift2, associated_id);
            }
            usleep(5000);

            // channel count might have changed due to removal of channels
            pthread_mutex_lock(&args->mux->lock);
            mux_channel_count = args->mux->channel_count;
            pthread_mutex_unlock(&args->mux->lock);
        }
        receive_packet(args->mux, args->channel_index);
#ifndef PIKEOS_TOOLCHAIN
        logger_log(args->mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive thread", "Thread %d calling receive",
                   args->channel_index);
#endif
        receive_packet(args->mux, args->channel_index);
#ifndef PIKEOS_TOOLCHAIN
        logger_log(args->mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive thread", "Thread %d receive done",
                   args->channel_index);
#endif // PIKEOS_TOOLCHAIN

        pthread_mutex_lock(&args->mux->lock);
        open = args->mux->is_open;
        pthread_mutex_unlock(&args->mux->lock);

        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        //nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }

#ifndef PIKEOS_TOOLCHAIN
    logger_log(args->mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux receive thread", "Thread %d stopped",
               args->channel_index);
#endif // PIKEOS_TOOLCHAIn

    // free memory of arg wrapper that has been allocated in rasta_redundancy_open
    rfree(arg_wrapper);
    return NULL;
}

/**
 * handler for multiplexing timeouts of the known redundancy channels
 * @param mux the multiplexer that is used
 * @return unused
 */
void * channel_timeout_handler(void* mux){
    redundancy_mux * mx = (redundancy_mux *) mux;

    logger_log(mx->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux timeout thread", "Timeout thread running");

    pthread_mutex_lock(&mx->lock);
    int open = mx->is_open;
    pthread_mutex_unlock(&mx->lock);
    while (open){
        // handle timeouts for each connected client
        unsigned long sleep_time = 100000;
        pthread_mutex_lock(&mx->lock);
        unsigned int mux_channel_count = mx->channel_count;
        pthread_mutex_unlock(&mx->lock);

        for (unsigned int i = 0; i < mux_channel_count; ++i) {
            pthread_mutex_lock(&mx->lock);
            if (i >= mx->channel_count) {
                pthread_mutex_unlock(&mx->lock);
                break;
            }
            rasta_redundancy_channel *current_channel = mx->connected_channels[i];
            pthread_mutex_unlock(&mx->lock);

            // get channel information
            pthread_mutex_lock(&current_channel->channel_lock);
            unsigned int channel_defer_q_count = current_channel->defer_q.count;
            unsigned int channel_t_seq = current_channel->configuration_parameters.t_seq;
            unsigned long channel_oldest_ts = channel_defer_q_count > 0
                ? current_channel->defer_q.elements[0].received_timestamp
                : 0;
            pthread_mutex_unlock(&current_channel->channel_lock);

            if(channel_defer_q_count == 0){
                // skip if queue is empty
                continue;
            }
            unsigned long current_time = current_ts();

            // defer queue is sorted, so the first element is always the oldest
            if ((current_time - channel_oldest_ts) > channel_t_seq){
                // timeout, send to next layer

                logger_log(mx->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux timeout thread", "timout detected for connection %d. calling f_deferTmo", i);
                rasta_red_f_deferTmo(current_channel);
            } else{
                sleep_time = 1000* (channel_t_seq - (current_time - channel_oldest_ts));
            }
        }
        pthread_mutex_lock(&mx->lock);
        open = mx->is_open;
        pthread_mutex_unlock(&mx->lock);
        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        //nanosleep((const struct timespec[]){{0, 0L}}, NULL);
        usleep((useconds_t)sleep_time);
    }

    logger_log(mx->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux timeout thread", "Timeout thread stopped");
    return NULL;
}

/* ----------------------------*/

redundancy_mux redundancy_mux_init_(struct logger_t *logger, struct RastaConfigInfo config){
    redundancy_mux mux;

    mux.logger = logger;
    //mux.listen_ports = listen_ports;
    mux.port_count = config.redundancy.connections.count;
    mux.config = config;

    mux.is_open = 0;
    mux.notifications_running = 0;

    // initialize the multiplexer mutex
    pthread_mutex_init(&mux.lock, NULL);

    logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "init memory for %d listen ports", mux.port_count);

    // init and bind udp sockets + threads array
    mux.udp_socket_fds = rmalloc(mux.port_count * sizeof(int));
    mux.transport_receive_threads = rmalloc(mux.port_count * sizeof(pthread_t));


    // channels are allocated individually so their mutexes keep a stable address
    mux.connected_channels = NULL;
    mux.channel_count = 0;
    mux.retired_channels = NULL;
    mux.retired_channel_count = 0;

    // init notifications to NULL
    mux.notifications.on_diagnostics_available = NULL;
    mux.notifications.on_new_connection = NULL;

    // load ports that are specified in config
    if (mux.config.redundancy.connections.count > 0){
        logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "loading listen from config");

        mux.listen_ports = rmalloc(sizeof(uint16_t) * mux.config.redundancy.connections.count);
        for (unsigned int j = 0; j < mux.config.redundancy.connections.count; ++j) {
            // init socket
            mux.udp_socket_fds[j] = udp_init();

            // bind socket to device and port
            udp_bind_device(mux.udp_socket_fds[j], (uint16_t) mux.config.redundancy.connections.data[j].port,
                            mux.config.redundancy.connections.data[j].ip);

            mux.listen_ports[j] = (uint16_t )mux.config.redundancy.connections.data[j].port;
        }
    }

    // init hashing context
    mux.sr_hashing_context.hash_length = config.sending.md4_type;
    mux.sr_hashing_context.algorithm = config.sending.sr_hash_algorithm;

    if (mux.sr_hashing_context.algorithm == RASTA_ALGO_MD4){
        // use MD4 IV as key
        rasta_md4_set_key(&mux.sr_hashing_context, config.sending.md4_a, config.sending.md4_b,
                          config.sending.md4_c, config.sending.md4_d);
    } else {
        // use the sr_hash_key
        allocateRastaByteArray(&mux.sr_hashing_context.key, sizeof(unsigned int));

        // convert unsigned in to byte array
        mux.sr_hashing_context.key.bytes[0] = (config.sending.sr_hash_key >> 24) & 0xFF;
        mux.sr_hashing_context.key.bytes[1] = (config.sending.sr_hash_key >> 16) & 0xFF;
        mux.sr_hashing_context.key.bytes[2] = (config.sending.sr_hash_key >> 8) & 0xFF;
        mux.sr_hashing_context.key.bytes[3] = (config.sending.sr_hash_key) & 0xFF;
    }

    logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "initialization done");
    return mux;
}

redundancy_mux redundancy_mux_init(struct logger_t *logger, uint16_t * listen_ports, unsigned int port_count, struct RastaConfigInfo config){
    redundancy_mux mux;

    mux.logger = logger;
    mux.listen_ports = listen_ports;
    mux.port_count = port_count;
    mux.config = config;

    mux.is_open = 0;
    mux.notifications_running = 0;

    // initialize the multiplexer mutex
    pthread_mutex_init(&mux.lock, NULL);

    logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "init memory for %d listen ports", port_count);

    // init and bind udp sockets + threads array
    mux.udp_socket_fds = rmalloc(port_count * sizeof(int));
    mux.transport_receive_threads = rmalloc(port_count * sizeof(pthread_t));

    // set up udp sockets
    for (unsigned int i = 0; i < port_count; ++i) {
        logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "setting up udp socket %d/%d", i+1,port_count);
        mux.udp_socket_fds[i] = udp_init();
        udp_bind(mux.udp_socket_fds[i], listen_ports[i]);
    }

    // channels are allocated individually so their mutexes keep a stable address
    mux.connected_channels = NULL;
    mux.channel_count = 0;
    mux.retired_channels = NULL;
    mux.retired_channel_count = 0;

    // init notifications to NULL
    mux.notifications.on_diagnostics_available = NULL;
    mux.notifications.on_new_connection = NULL;

    // load channel that is specified in config
    if (mux.config.redundancy.connections.count > 0){
        logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "loading redundancy channel from config");
        rasta_redundancy_channel *new_channel = rmalloc(sizeof(*new_channel));
        if (new_channel == NULL) {
            logger_log(mux.logger, LOG_LEVEL_ERROR, "RaSTA RedMux init",
                       "could not allocate configured redundancy channel");
            return mux;
        }
        rasta_red_init(new_channel, mux.logger, &mux.config, mux.port_count,
                       mux.config.general.rasta_id);
        new_channel->associated_id = 0x0;

        for (unsigned int j = 0; j < mux.config.redundancy.connections.count; ++j) {
            logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "setting up transport channel %d/%d",
                       j+1, mux.config.redundancy.connections.count);
            logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "transport channel: ip=%s, port=%d",
                       mux.config.redundancy.connections.data[j].ip, mux.config.redundancy.connections.data[j].port);
            // no associated channel found -> received message from new partner
            // add transport channel to redundancy channel
            rasta_red_add_transport_channel(new_channel, mux.config.redundancy.connections.data[j].ip,
                                            (uint16_t )mux.config.redundancy.connections.data[j].port);
        }

        if (!append_channel_unlocked(&mux, new_channel)) {
            destroy_channel(new_channel);
            logger_log(mux.logger, LOG_LEVEL_ERROR, "RaSTA RedMux init",
                       "could not store configured redundancy channel");
        }
    }

    logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "initialization done");
    return mux;
}

redundancy_mux redundancy_mux_init_with_devices(struct logger_t *logger, struct RastaIPData * listen_ports, unsigned int port_count, struct RastaConfigInfo config){
    redundancy_mux mux;

    mux.logger = logger;
    //mux.listen_ports = listen_ports; TODO FIX THIS!
    mux.port_count = port_count;
    mux.config = config;

    mux.is_open = 0;
    mux.notifications_running = 0;

    // initialize the multiplexer mutex
    pthread_mutex_init(&mux.lock, NULL);

    logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "init memory for %d listen ports", port_count);

    // init and bind udp sockets + threads array
    mux.udp_socket_fds = rmalloc(port_count * sizeof(int));
    mux.transport_receive_threads = rmalloc(port_count * sizeof(pthread_t));

    // set up udp sockets
    for (unsigned int i = 0; i < port_count; ++i) {
        logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "setting up udp socket %d/%d", i+1,port_count);
        mux.udp_socket_fds[i] = udp_init();
        udp_bind_device(mux.udp_socket_fds[i], (uint16_t) listen_ports[i].port, listen_ports[i].ip);
    }

    // channels are allocated individually so their mutexes keep a stable address
    mux.connected_channels = NULL;
    mux.channel_count = 0;
    mux.retired_channels = NULL;
    mux.retired_channel_count = 0;

    // init notifications to NULL
    mux.notifications.on_diagnostics_available = NULL;
    mux.notifications.on_new_connection = NULL;

    // load channel that is specified in config
    if (mux.config.redundancy.connections.count > 0){
        logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "loading redundancy channel from config");
        rasta_redundancy_channel *new_channel = rmalloc(sizeof(*new_channel));
        if (new_channel == NULL) {
            logger_log(mux.logger, LOG_LEVEL_ERROR, "RaSTA RedMux init",
                       "could not allocate configured redundancy channel");
            return mux;
        }
        rasta_red_init(new_channel, mux.logger, &mux.config, mux.port_count,
                       mux.config.general.rasta_id);
        new_channel->associated_id = 0x0;

        for (unsigned int j = 0; j < mux.config.redundancy.connections.count; ++j) {
            logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "setting up transport channel %d/%d",
                       j+1, mux.config.redundancy.connections.count);
            logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "transport channel: ip=%s, port=%d",
                       mux.config.redundancy.connections.data[j].ip, mux.config.redundancy.connections.data[j].port);
            // no associated channel found -> received message from new partner
            // add transport channel to redundancy channel
            rasta_red_add_transport_channel(new_channel, mux.config.redundancy.connections.data[j].ip,
                                            (uint16_t )mux.config.redundancy.connections.data[j].port);
        }

        if (!append_channel_unlocked(&mux, new_channel)) {
            destroy_channel(new_channel);
            logger_log(mux.logger, LOG_LEVEL_ERROR, "RaSTA RedMux init",
                       "could not store configured redundancy channel");
        }
    }

    logger_log(mux.logger, LOG_LEVEL_DEBUG, "RaSTA RedMux init", "initialization done");
    return mux;
}

void redundancy_mux_open(redundancy_mux * mux){
    if (mux->is_open){
        // already open
        return;
    }

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux open", "channels : %d", mux->port_count);

    // set open to true here to let threads run as soon as they are created
    mux->is_open = 1;

    // start receive threads for transport channels
    for (unsigned int i = 0; i < mux->port_count; ++i) {
        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux open",
                   "opening transport channel %d/%d", i+1, mux->port_count);

        struct receive_thread_parameter_wrapper * wrapper = rmalloc(sizeof(struct receive_thread_parameter_wrapper));
        wrapper->mux= mux;
        wrapper->channel_index= i;

        pthread_t recv_thread;

        // start the received thread for the port
        if (pthread_create(&recv_thread, NULL, channel_receive_handler, wrapper)){
            logger_log(mux->logger, LOG_LEVEL_ERROR, "RaSTA RedMux open", "error while creating thread");
            exit(1);
        }

        // set the thread in list so close can join it
        mux->transport_receive_threads[i] = recv_thread;

        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux open",
                   "listen thread for port %d started", mux->listen_ports[wrapper->channel_index]);
    }

    // start timeout thread
    pthread_t tmo_thread;
    if (pthread_create(&tmo_thread, NULL, channel_timeout_handler, mux)){
        logger_log(mux->logger, LOG_LEVEL_ERROR, "RaSTA RedMux open", "error while creating timeout thread");
        exit(1);
    }
    mux->timeout_thread = tmo_thread;


    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux open", "redundancy multiplexer is ready");
}

void redundancy_mux_close(redundancy_mux * mux){
    // set flag to 0, will cause the threads to stop and cleanup before exiting
    pthread_mutex_lock(&mux->lock);
    mux->is_open = 0;
    pthread_mutex_unlock(&mux->lock);

    // close the sockets of the transport channels
    for (unsigned int i = 0; i < mux->port_count; ++i) {
        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux close", "closing udp socket %d/%d", i+1, mux->port_count);
        udp_close(mux->udp_socket_fds[i]);
    }

    // Wait until no worker can access channels or thread data before freeing it.
    for (unsigned int i = 0; i < mux->port_count; ++i) {
        pthread_join(mux->transport_receive_threads[i], NULL);
    }
    pthread_join(mux->timeout_thread, NULL);

    // free arrays
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux close", "freeing thread data");
    rfree(mux->udp_socket_fds);
    rfree(mux->transport_receive_threads);
    mux->udp_socket_fds = NULL;
    mux->transport_receive_threads = NULL;
    mux->port_count = 0;

    // close the redundancy channels
    for (unsigned int j = 0; j < mux->channel_count; ++j) {
        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux close", "cleanup connected channel %d/%d", j+1, mux->channel_count);
        destroy_channel(mux->connected_channels[j]);
    }
    for (unsigned int j = 0; j < mux->retired_channel_count; ++j) {
        destroy_channel(mux->retired_channels[j]);
    }
    rfree(mux->connected_channels);
    rfree(mux->retired_channels);
    mux->connected_channels = NULL;
    mux->retired_channels = NULL;
    mux->channel_count = 0;
    mux->retired_channel_count = 0;

    freeRastaByteArray(&mux->sr_hashing_context.key);

    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux close", "redundancy multiplexer closed");
}

rasta_redundancy_channel * redundancy_mux_get_channel(redundancy_mux * mux, unsigned long id){
    pthread_mutex_lock(&mux->lock);
    rasta_redundancy_channel *channel = get_channel_unlocked(mux, id);
    pthread_mutex_unlock(&mux->lock);
    return channel;
}

void redundancy_mux_set_config_id(redundancy_mux * mux, unsigned long id){
    pthread_mutex_lock(&mux->lock);
    // only set if a channel is available
    if (mux->channel_count > 0){
        mux->connected_channels[0]->associated_id = id;
    }
    pthread_mutex_unlock(&mux->lock);
}

void redundancy_mux_send(redundancy_mux * mux, struct RastaPacket data){
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "sending a data packet to id 0x%lX", data.receiver_id);

    // get the channel to the remote entity by the data's received_id
    rasta_redundancy_channel * receiver = redundancy_mux_get_channel(mux, data.receiver_id);

    if (receiver == NULL){
        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "redundancy channel with id=0x%lX unknown", data.receiver_id);
        // not receiver found
        return;
    }

    pthread_mutex_lock(&receiver->channel_lock);
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "current seq_tx=%d", receiver->seq_tx);

    // create packet to send and convert to byte array
    struct RastaRedundancyPacket packet = createRedundancyPacket(receiver->seq_tx, data, mux->config.redundancy.crc_type);
    struct RastaByteArray data_to_send = rastaRedundancyPacketToBytes(packet, &receiver->hashing_context);

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "redundancy packet created");

    // increase seq_tx
    receiver->seq_tx = receiver->seq_tx +1;

    // send on every transport channels
    rasta_transport_channel channel;
    for (unsigned int i = 0; i < receiver->connected_channel_count; ++i) {
        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "Sending on transport channel %d/%d",
                   i+1, receiver->connected_channel_count);

        channel = receiver->connected_channels[i];

        // send using the channel specific udp socket
        udp_send(mux->udp_socket_fds[i], data_to_send.bytes, data_to_send.length, channel.ip_address, channel.port);

        logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux send", "Sent data over channel %s:%d",
                   channel.ip_address, channel.port);
    }

    freeRastaByteArray(&data_to_send);
    pthread_mutex_unlock(&receiver->channel_lock);

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA Red send", "Data sent over all transport channels");
}

struct RastaPacket redundancy_mux_retrieve(redundancy_mux * mux, unsigned long id){
    // get the channel by id
    rasta_redundancy_channel * target = redundancy_mux_get_channel(mux, id);

    if (target == NULL){
        logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux retrieve", "entity with id 0x%lX not connected, waiting...", id);
    }

    // if ID is unknown, wait until available
    while (target == NULL){
        target = redundancy_mux_get_channel(mux, id);

        usleep(100000);
        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        //nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }

    struct RastaByteArray * element;

    // busy wait for data in FIFO
    while (fifo_get_size(target->fifo_recv) == 0){
        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        usleep(10000);
        //nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }

    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux retrieve", "Found element in queue");

    element = fifo_pop(target->fifo_recv);

    struct RastaPacket packet;
    bytesToRastaPacket(&packet, element, &target->hashing_context);

    freeRastaByteArray(element);
    rfree(element);

    return packet;
}

void redundancy_mux_wait_for_notifications(redundancy_mux * mux){
    if (mux->notifications_running == 0){
        logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "all notification threads finished");
        return;
    }
    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "waiting for %d notification thread(s) to finish", mux->notifications_running);
    while (mux->notifications_running > 0){
        // busy wait
        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }
    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "all notification threads finished");
}

void redundancy_mux_wait_for_entity(redundancy_mux * mux, unsigned long id){
    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "waiting for entity with id=0x%lX", id);
    rasta_redundancy_channel * target = NULL;
    while (target == NULL){
        target = redundancy_mux_get_channel(mux, id);
        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }
    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux wait", "entity with id=0x%lX available", id);
}

void redundancy_mux_add_channel(redundancy_mux * mux, unsigned long id, struct RastaIPData * transport_channels){
    rasta_redundancy_channel *channel = rmalloc(sizeof(*channel));
    if (channel == NULL) {
        return;
    }
    rasta_red_init(channel, mux->logger, &mux->config, mux->port_count, id);

    // add transport channels
    for (unsigned int i = 0; i < mux->port_count; ++i) {
        rasta_red_add_transport_channel(channel, transport_channels[i].ip,
                                        (uint16_t)transport_channels[i].port);
    }

    pthread_mutex_lock(&mux->lock);
    int appended = append_channel_unlocked(mux, channel);
    pthread_mutex_unlock(&mux->lock);

    if (!appended) {
        destroy_channel(channel);
        return;
    }

    logger_log(mux->logger, LOG_LEVEL_INFO, "RaSTA RedMux add channel", "added new redundancy channel for ID=0x%X", id);
}

void redundancy_mux_remove_channel(redundancy_mux * mux, unsigned long channel_id){
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux remove channel", "removing channel with ID=0x%X", channel_id);

    pthread_mutex_lock(&mux->lock);
    unsigned int index = mux->channel_count;
    for (unsigned int i = 0; i < mux->channel_count; ++i) {
        if (mux->connected_channels[i]->associated_id == channel_id) {
            index = i;
            break;
        }
    }

    if (index == mux->channel_count) {
        pthread_mutex_unlock(&mux->lock);
        return;
    }

    rasta_redundancy_channel *channel = mux->connected_channels[index];
    if (!retire_channel_unlocked(mux, channel)) {
        pthread_mutex_unlock(&mux->lock);
        return;
    }

    for (unsigned int i = index + 1U; i < mux->channel_count; ++i) {
        mux->connected_channels[i - 1U] = mux->connected_channels[i];
    }
    mux->channel_count--;
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux remove channel", "%d channels left", mux->channel_count);
    pthread_mutex_unlock(&mux->lock);
}

/**
 * gets the amount of messages in the receive queue of the connected channel with index @p redundancy_channel_index
 * @param mux the multiplexer that is used
 * @param redundancy_channel_index the index of the redundancy channel inside the mux connected_channels array
 * @return amount of messages in the queue
 */
unsigned int get_queue_msg_count(redundancy_mux * mux, int redundancy_channel_index){
    pthread_mutex_lock(&mux->lock);
    if (redundancy_channel_index < 0 ||
        (unsigned int)redundancy_channel_index >= mux->channel_count){
        // channel does not exist anymore
        pthread_mutex_unlock(&mux->lock);
        return 0;
    }

    rasta_redundancy_channel *channel = mux->connected_channels[redundancy_channel_index];
    pthread_mutex_unlock(&mux->lock);

    pthread_mutex_lock(&channel->channel_lock);
    if (channel->fifo_recv == NULL){
        pthread_mutex_unlock(&channel->channel_lock);
        return 0;
    }
    unsigned int size = fifo_get_size(channel->fifo_recv);
    pthread_mutex_unlock(&channel->channel_lock);

    return size;
}

struct RastaPacket redundancy_mux_retrieve_all(redundancy_mux * mux){
    logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux retrieve all", "waiting for message on any connection");
    unsigned int current_index = 0;
    while (1){
        if (mux->channel_count == 0){
            // if no channel are connected yet, do noting until a channel is available
            usleep(100000);
            continue;
        }

        if (current_index == mux->channel_count){
            current_index = 0;
        }

        if (get_queue_msg_count(mux, (int)current_index) > 0){
            logger_log(mux->logger, LOG_LEVEL_DEBUG, "RaSTA RedMux retrieve all", "channel with index %d has messages", current_index);
            pthread_mutex_lock(&mux->lock);
            if (current_index >= mux->channel_count) {
                pthread_mutex_unlock(&mux->lock);
                continue;
            }
            unsigned long associated_id =
                mux->connected_channels[current_index]->associated_id;
            pthread_mutex_unlock(&mux->lock);
            return redundancy_mux_retrieve(mux, associated_id);
        }

        current_index++;
        // to avoid to much CPU utilization, force context switch by sleeping for 0ns
        nanosleep((const struct timespec[]){{0, 0L}}, NULL);
    }
}
