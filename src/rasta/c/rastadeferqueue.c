#include <stdlib.h>
#include <string.h>
#include "rmemory.h"
#include "rastadeferqueue.h"

static void free_packet_copy(struct RastaRedundancyPacket *packet) {
    if (packet == NULL) {
        return;
    }

    freeRastaByteArray(&packet->data.data);
    packet->data.data.bytes = NULL;
    freeRastaByteArray(&packet->data.checksum);
    packet->data.checksum.bytes = NULL;
    rfree(packet);
}

static int copy_byte_array(struct RastaByteArray *destination,
                           const struct RastaByteArray *source) {
    destination->bytes = NULL;
    destination->length = 0;

    if (source->length == 0) {
        return 1;
    }
    if (source->bytes == NULL) {
        return 0;
    }

    allocateRastaByteArray(destination, source->length);
    if (destination->bytes == NULL) {
        destination->length = 0;
        return 0;
    }

    rmemcpy(destination->bytes, source->bytes, source->length);
    return 1;
}

static struct RastaRedundancyPacket *copy_packet(
    const struct RastaRedundancyPacket *source
) {
    struct RastaRedundancyPacket *copy;

    if (source == NULL) {
        return NULL;
    }

    copy = rmalloc(sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }

    *copy = *source;
    copy->data.data.bytes = NULL;
    copy->data.data.length = 0;
    copy->data.checksum.bytes = NULL;
    copy->data.checksum.length = 0;

    if (!copy_byte_array(&copy->data.data, &source->data.data) ||
        !copy_byte_array(&copy->data.checksum, &source->data.checksum)) {
        free_packet_copy(copy);
        return NULL;
    }

    return copy;
}

static void clear_elements(struct defer_queue *queue) {
    for (unsigned int i = 0; i < queue->count; ++i) {
        free_packet_copy(queue->elements[i].packet);
        queue->elements[i].packet = NULL;
        queue->elements[i].received_timestamp = 0;
    }
    queue->count = 0;
}

/**
 * finds the index of a given element inside the given queue.
 * The sequence_number is used as the unique identifier
 * @param queue the queue that will be searched
 * @param seq_nr the sequence number to be located
 * @return -1 if there is no element with the specified @p seq_nr, index of the element otherwise
 */
static int find_index(const struct defer_queue *queue, unsigned long seq_nr) {
    for (unsigned int i = 0; i < queue->count; ++i) {
        if (queue->elements[i].packet != NULL &&
            queue->elements[i].packet->sequence_number == seq_nr) {
            return (int)i;
        }
    }

    return -1;
}

static int compare_timestamps(const void *a, const void *b) {
    const struct rasta_redundancy_packet_wrapper *left = a;
    const struct rasta_redundancy_packet_wrapper *right = b;

    if (left->received_timestamp < right->received_timestamp) {
        return -1;
    }
    if (left->received_timestamp > right->received_timestamp) {
        return 1;
    }
    return 0;
}

/**
 * sorts the elements in the queue in ascending time (first element has oldest timestamp)
 * @param queue the queue that will be sorted
 */
static void sort(struct defer_queue *queue) {
    qsort(queue->elements, queue->count,
          sizeof(struct rasta_redundancy_packet_wrapper), compare_timestamps);
}

void defer_queue_init(struct defer_queue *queue, unsigned int n_max) {

    // allocate the array
    queue->elements = rmalloc(n_max * sizeof(struct rasta_redundancy_packet_wrapper));
    memset(queue->elements, 0x00, n_max*sizeof(struct rasta_redundancy_packet_wrapper));

    // set count to 0
    queue->count = 0;

    // set max count
    queue->max_count = n_max;

    // init the mutex
    pthread_mutex_init(&queue->mutex, NULL);
}

int deferqueue_isfull(struct defer_queue * queue){
    // acquire lock
    pthread_mutex_lock(&queue->mutex);

    int result = (queue->count >= queue->max_count);

    // free lock
    pthread_mutex_unlock(&queue->mutex);

    return result;
}

void deferqueue_add(struct defer_queue * queue, struct RastaRedundancyPacket *packet, unsigned long recv_ts){
    struct RastaRedundancyPacket *packet_copy;

    // acquire lock
    // TODO Mutex reintroduced
    pthread_mutex_lock(&queue->mutex);

    if(packet == NULL || queue->count >= queue->max_count ||
       find_index(queue, packet->sequence_number) != -1){
        // queue full or duplicate sequence number, return
        pthread_mutex_unlock(&queue->mutex);
        return;
    }

    packet_copy = copy_packet(packet);
    if (packet_copy == NULL) {
        pthread_mutex_unlock(&queue->mutex);
        return;
    }

    struct rasta_redundancy_packet_wrapper element;
    element.packet = packet_copy;
    element.received_timestamp = recv_ts;

    // add element to the end
    queue->elements[queue->count] = element;

    // increase count
    queue->count = queue->count + 1;

    // sort array
    sort(queue);

    // free lock
    pthread_mutex_unlock(&queue->mutex);
}

void deferqueue_remove(struct defer_queue * queue, unsigned long seq_nr){
    // acquire lock
    pthread_mutex_lock(&queue->mutex);

    int index = find_index(queue, seq_nr);
    if(index == -1){
        pthread_mutex_unlock(&queue->mutex);
        // element not in queue
        return;
    }

    unsigned int last_index = queue->count - 1;
    free_packet_copy(queue->elements[index].packet);

    if(index != (int)last_index){
        // element to delete isn't at the last position
        // to be able to add the next element to the last position without overriding something
        // the currently last element is moved to the index where the element to delete is located
        queue->elements[index] = queue->elements[last_index];
    }

    queue->elements[last_index].packet = NULL;
    queue->elements[last_index].received_timestamp = 0;

    // decrease counter
    queue->count = queue->count -1;

    // sort the array
    sort(queue);

    // free lock
    pthread_mutex_unlock(&queue->mutex);
}

int deferqueue_contains(struct defer_queue * queue, unsigned long seq_nr){
    // acquire lock
    pthread_mutex_lock(&queue->mutex);

    int result = (find_index(queue, seq_nr) != -1);

    // free lock
    pthread_mutex_unlock(&queue->mutex);

    return result;
}

void deferqueue_destroy(struct defer_queue * queue){
    pthread_mutex_lock(&queue->mutex);
    clear_elements(queue);
    rfree(queue->elements);
    queue->elements = NULL;

    queue->max_count= 0;
    pthread_mutex_unlock(&queue->mutex);

    pthread_mutex_destroy(&queue->mutex);
}

int deferqueue_smallest_seqnr(struct defer_queue * queue){
    // acquire lock
    pthread_mutex_lock(&queue->mutex);

    int index = -1;

    uint32_t smallest = 0;

    // naive implementation of search. performance shouldn't be an issue as the amount of messages in the queue is small
    for (unsigned int i = 0; i < queue->count; ++i) {
        if(queue->elements[i].packet != NULL &&
           (index == -1 ||
            queue->elements[i].packet->sequence_number < smallest)){
            smallest = queue->elements[i].packet->sequence_number;
            index = (int)i;
        }
    }

    // free lock
    pthread_mutex_unlock(&queue->mutex);

    return index;
}

void
deferqueue_get(struct RastaRedundancyPacket *result, struct defer_queue *queue, unsigned long seq_nr) {
    // acquire lock
    pthread_mutex_lock(&queue->mutex);

    int index = find_index(queue, seq_nr);

    if(index == -1){
        pthread_mutex_unlock(&queue->mutex);
        // element not in queue, return uninitialized struct
        *result =  (const struct RastaRedundancyPacket){ 0 };
        return;
    }

    *result = *queue->elements[index].packet;

    // free lock
    pthread_mutex_unlock(&queue->mutex);
}

unsigned long deferqueue_get_ts(struct defer_queue * queue, unsigned long seq_nr){
    // acquire lock
    pthread_mutex_lock(&queue->mutex);

    int index = find_index(queue, seq_nr);

    if(index == -1){
        pthread_mutex_unlock(&queue->mutex);
        // element not in queue, return uninitialized struct
        return 0;
    }

    unsigned long result = queue->elements[index].received_timestamp;

    // free lock
    pthread_mutex_unlock(&queue->mutex);

    // return element at index
    return result;
}

void deferqueue_clear(struct defer_queue * queue){
    // acquire lock
    pthread_mutex_lock(&queue->mutex);

    clear_elements(queue);

    // free lock
    pthread_mutex_unlock(&queue->mutex);
}
