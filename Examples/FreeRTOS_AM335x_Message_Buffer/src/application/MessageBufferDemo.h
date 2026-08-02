#ifndef MESSAGE_BUFFER_DEMO_H
#define MESSAGE_BUFFER_DEMO_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "stdint.h"
#include "stdbool.h"

// Message Buffer Configuration
#define MB_MAX_MESSAGE_SIZE   256
#define MB_MAX_NUM_MESSAGES   16
#define MB_QUEUE_LENGTH       MB_MAX_NUM_MESSAGES
#define MESSAGE_MAGIC_START   0xDEADBEEF
#define MESSAGE_MAGIC_END     0xBADCAFE

// Message Flags
#define MSG_FLAG_FINAL        (1 << 0)  // Final fragment of message
#define MSG_FLAG_VALID        (1 << 1)  // Valid message

// Message Header (exactly 16 bytes when packed)
// magic(4) + length(2) + flags(2) + timestamp(4) + sequence(4) + checksum(2) = 18 bytes
typedef struct {
    uint32_t magic;
    uint16_t length;
    uint16_t flags;
    uint32_t timestamp;
    uint32_t sequence;
    uint16_t checksum;
} __attribute__((packed)) MessageHeader;

// Message Footer (exactly 6 bytes)
// magic_end(4) + footer_checksum(2) = 6 bytes
typedef struct {
    uint32_t magic_end;
    uint16_t footer_checksum;
} __attribute__((packed)) MessageFooter;

// Complete message: Header(18) + Payload(256) + Footer(6) = 280 bytes
typedef struct {
    MessageHeader header;
    uint8_t payload[MB_MAX_MESSAGE_SIZE];
    MessageFooter footer;
} __attribute__((packed)) MessageBuffer;

// Global queue handle (defined in .c)
extern QueueHandle_t mb_queue;

// Global counters (defined in .c)
extern uint32_t sent_count;
extern uint32_t received_count;

// Task prototypes
void MessageProducerTask(void *pvParameters);
void MessageConsumerTask(void *pvParameters);
void MessageConsumerTask2(void *pvParameters);

#endif /* MESSAGE_BUFFER_DEMO_H */
