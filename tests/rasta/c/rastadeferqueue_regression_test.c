#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "rastadeferqueue.h"
#include "rasta_red_multiplexer.h"
#include "rastautil.h"
#include "rmemory.h"
#include "udp.h"

static void init_packet(struct RastaRedundancyPacket *packet,
                        uint32_t sequence_number,
                        unsigned char data_value,
                        unsigned char checksum_value) {
    memset(packet, 0, sizeof(*packet));
    packet->sequence_number = sequence_number;
    allocateRastaByteArray(&packet->data.data, 4);
    allocateRastaByteArray(&packet->data.checksum, 3);
    assert(packet->data.data.bytes != NULL);
    assert(packet->data.checksum.bytes != NULL);
    memset(packet->data.data.bytes, data_value, packet->data.data.length);
    memset(packet->data.checksum.bytes, checksum_value,
           packet->data.checksum.length);
}

static void free_original(struct RastaRedundancyPacket *packet) {
    freeRastaByteArray(&packet->data.data);
    freeRastaByteArray(&packet->data.checksum);
}

static struct RastaRedundancyPacket *find_packet(struct defer_queue *queue,
                                                 uint32_t sequence_number) {
    for (unsigned int i = 0; i < queue->count; ++i) {
        if (queue->elements[i].packet != NULL &&
            queue->elements[i].packet->sequence_number == sequence_number) {
            return queue->elements[i].packet;
        }
    }
    return NULL;
}

static void test_deep_copy_and_lifecycle(void) {
    struct defer_queue queue;
    struct RastaRedundancyPacket first;
    struct RastaRedundancyPacket second;
    struct RastaRedundancyPacket borrowed;

    defer_queue_init(&queue, 3);
    init_packet(&first, 10, 0x11, 0xA1);
    init_packet(&second, 20, 0x22, 0xA2);

    deferqueue_add(&queue, &first, 200);
    deferqueue_add(&queue, &second, 100);

    struct RastaRedundancyPacket *first_copy = find_packet(&queue, 10);
    struct RastaRedundancyPacket *second_copy = find_packet(&queue, 20);
    assert(queue.count == 2);
    assert(first_copy != NULL && second_copy != NULL);
    assert(first_copy != &first && second_copy != &second);
    assert(first_copy != second_copy);
    assert(first_copy->data.data.bytes != first.data.data.bytes);
    assert(first_copy->data.checksum.bytes != first.data.checksum.bytes);
    assert(second_copy->data.data.bytes != second.data.data.bytes);
    assert(second_copy->data.checksum.bytes != second.data.checksum.bytes);

    first.data.data.bytes[0] = 0xFF;
    first.data.checksum.bytes[0] = 0xFF;
    assert(first_copy->data.data.bytes[0] == 0x11);
    assert(first_copy->data.checksum.bytes[0] == 0xA1);

    free_original(&first);
    free_original(&second);

    deferqueue_get(&borrowed, &queue, 20);
    assert(borrowed.sequence_number == 20);
    assert(borrowed.data.data.bytes[0] == 0x22);
    assert(borrowed.data.checksum.bytes[0] == 0xA2);

    deferqueue_remove(&queue, 20);
    assert(queue.count == 1);
    assert(deferqueue_contains(&queue, 10));
    assert(!deferqueue_contains(&queue, 20));

    deferqueue_get(&borrowed, &queue, 99);
    assert(borrowed.sequence_number == 0);
    assert(borrowed.data.data.bytes == NULL);
    assert(borrowed.data.checksum.bytes == NULL);

    deferqueue_clear(&queue);
    assert(queue.count == 0);
    deferqueue_destroy(&queue);
    assert(queue.elements == NULL);
}

static void test_duplicate_and_maximum_sequence(void) {
    struct defer_queue queue;
    struct RastaRedundancyPacket first;
    struct RastaRedundancyPacket duplicate;
    struct RastaRedundancyPacket maximum;

    defer_queue_init(&queue, 2);
    init_packet(&first, 5, 0x15, 0xA5);
    init_packet(&duplicate, 5, 0x25, 0xB5);

    deferqueue_add(&queue, &first, 1);
    deferqueue_add(&queue, &duplicate, 2);
    assert(queue.count == 1);

    free_original(&first);
    free_original(&duplicate);
    deferqueue_clear(&queue);
    assert(deferqueue_smallest_seqnr(&queue) == -1);

    init_packet(&maximum, UINT32_MAX, 0x33, 0xC3);
    deferqueue_add(&queue, &maximum, 3);
    free_original(&maximum);

    int index = deferqueue_smallest_seqnr(&queue);
    assert(index >= 0);
    assert(queue.elements[index].packet != NULL);
    assert(queue.elements[index].packet->sequence_number == UINT32_MAX);

    deferqueue_destroy(&queue);
}

static void cleanup_test_mux(redundancy_mux *mux) {
    for (unsigned int i = 0; i < mux->channel_count; ++i) {
        rasta_red_cleanup(mux->connected_channels[i]);
        rfree(mux->connected_channels[i]);
    }
    for (unsigned int i = 0; i < mux->retired_channel_count; ++i) {
        rasta_red_cleanup(mux->retired_channels[i]);
        rfree(mux->retired_channels[i]);
    }
    rfree(mux->connected_channels);
    rfree(mux->retired_channels);
    pthread_mutex_destroy(&mux->lock);
}

static void test_channel_storage_is_stable(void) {
    redundancy_mux mux;
    memset(&mux, 0, sizeof(mux));
    pthread_mutex_init(&mux.lock, NULL);
    mux.config.redundancy.n_deferqueue_size = 2;
    mux.config.sending.sr_hash_algorithm = RASTA_ALGO_MD4;

    redundancy_mux_add_channel(&mux, 1, NULL);
    rasta_redundancy_channel *first = redundancy_mux_get_channel(&mux, 1);
    assert(first != NULL);

    redundancy_mux_add_channel(&mux, 2, NULL);
    rasta_redundancy_channel *second = redundancy_mux_get_channel(&mux, 2);
    assert(second != NULL);
    assert(redundancy_mux_get_channel(&mux, 1) == first);

    redundancy_mux_add_channel(&mux, 3, NULL);
    assert(redundancy_mux_get_channel(&mux, 1) == first);
    assert(redundancy_mux_get_channel(&mux, 2) == second);

    redundancy_mux_remove_channel(&mux, 2);
    assert(redundancy_mux_get_channel(&mux, 2) == NULL);
    assert(redundancy_mux_get_channel(&mux, 1) == first);
    assert(mux.retired_channel_count == 1);
    assert(mux.retired_channels[0] == second);

    assert(pthread_mutex_lock(&first->channel_lock) == 0);
    assert(pthread_mutex_unlock(&first->channel_lock) == 0);
    assert(pthread_mutex_lock(&second->channel_lock) == 0);
    assert(pthread_mutex_unlock(&second->channel_lock) == 0);

    cleanup_test_mux(&mux);
}

static void test_maximum_ipv4_address_is_terminated(void) {
    struct RastaConfigInfo config;
    rasta_redundancy_channel channel;

    memset(&config, 0, sizeof(config));
    config.redundancy.n_deferqueue_size = 2;
    config.sending.sr_hash_algorithm = RASTA_ALGO_MD4;

    rasta_red_init(&channel, NULL, &config, 1, 7);
    rasta_red_add_transport_channel(&channel, "255.255.255.255", 8888);

    assert(channel.connected_channel_count == 1);
    assert(strcmp(channel.connected_channels[0].ip_address,
                  "255.255.255.255") == 0);
    assert(channel.connected_channels[0].ip_address[IPV4_STR_LEN - 1U] == '\0');

    rasta_red_cleanup(&channel);
}

static void test_mux_timeout_worker_stops_before_cleanup(void) {
    struct RastaConfigInfo config;

    memset(&config, 0, sizeof(config));
    config.redundancy.n_deferqueue_size = 2;
    config.redundancy.t_seq = 10;
    config.redundancy.n_diagnose = 100;
    config.sending.sr_hash_algorithm = RASTA_ALGO_MD4;

    redundancy_mux mux = redundancy_mux_init(NULL, NULL, 0, config);
    redundancy_mux_open(&mux);
    redundancy_mux_close(&mux);

    assert(mux.udp_socket_fds == NULL);
    assert(mux.transport_receive_threads == NULL);
    assert(mux.connected_channels == NULL);
    assert(mux.retired_channels == NULL);
    assert(mux.channel_count == 0);
    assert(mux.retired_channel_count == 0);
}

int main(void) {
    test_deep_copy_and_lifecycle();
    test_duplicate_and_maximum_sequence();
    test_channel_storage_is_stable();
    test_maximum_ipv4_address_is_terminated();
    test_mux_timeout_worker_stops_before_cleanup();
    return 0;
}
