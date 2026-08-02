/**
 * FreeRTOS AM3352 Message Buffer Demonstration
 * 
 * Demonstrates:
 * - Queue-based message passing between tasks
 * - Message framing with header + payload + footer
 * - CRC16 checksum validation
 * - Variable length messages (16-255 bytes)
 * - Blocking send/receive with timeout
 */

#include "MessageBufferDemo.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "consoleUtils.h"
#include "hw_types.h"
#include "string.h"

// Global queue handle
QueueHandle_t mb_queue;

// Global counters
uint32_t sent_count = 0;
uint32_t received_count = 0;

// CRC16 calculation (CRC-16/CCITT-FALSE) - simple and correct
static uint16_t calc_crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0x8005;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ============================================================
// Task: Message Producer
// ============================================================
void MessageProducerTask(void *pvParameters) {
    TickType_t xLastExecutionTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(500);

    // Create queue with enough space for MB_QUEUE_LENGTH messages
    mb_queue = xQueueCreate(MB_QUEUE_LENGTH, sizeof(MessageBuffer));
    if (mb_queue == NULL) {
        ConsoleUtilsPrintf("[ERROR] Failed to create message queue\r\n");
        vTaskDelete(NULL);
    }

    ConsoleUtilsPrintf("[MSG] Task_MessageProducer created\r\n");
    ConsoleUtilsPrintf("[MSG] Message Buffer demo started\r\n");
    ConsoleUtilsPrintf("[MSG] Queue size=%d Item size=%d\r\n", 
                       MB_QUEUE_LENGTH, (int)sizeof(MessageBuffer));

    for (;;) {
        MessageBuffer msg;
        memset(&msg, 0, sizeof(msg));

        // Fill Header fields one by one
        msg.header.magic = MESSAGE_MAGIC_START;
        msg.header.length = (sent_count % (MB_MAX_MESSAGE_SIZE - 16)) + 16; // 16-255 bytes
        msg.header.flags = (sent_count & 0x01) ? MSG_FLAG_FINAL : 0;
        msg.header.timestamp = xTaskGetTickCount();
        msg.header.sequence = sent_count;

        // Fill Payload (variable length ASCII)
        for (uint16_t i = 0; i < msg.header.length; i++) {
            msg.payload[i] = 'A' + (i % 26);
        }

        // Calculate CRC over header (excluding checksum field) + payload
        // Header without checksum: magic(4) + length(2) + flags(2) + timestamp(4) + sequence(4) = 16 bytes
        uint8_t crc_data[272];
        memcpy(crc_data, &msg.header, 16);  // Copy header fields except checksum
        memcpy(crc_data + 16, msg.payload, msg.header.length);  // Copy payload
        
        msg.header.checksum = calc_crc16(crc_data, 16 + msg.header.length);

        // Fill Footer
        msg.footer.magic_end = MESSAGE_MAGIC_END;
        msg.footer.footer_checksum = calc_crc16((uint8_t*)&msg.footer.magic_end, sizeof(uint32_t));

        // Send to queue (blocking, 200ms timeout)
        if (xQueueSendToBack(mb_queue, &msg, pdMS_TO_TICKS(200)) != pdPASS) {
            ConsoleUtilsPrintf("[WARN] Send timeout\r\n");
        } else {
            sent_count++;
            ConsoleUtilsPrintf("[MSG] Sent #%d len=%d flag=0x%02X\r\n",
                               (int)sent_count, (int)msg.header.length, (int)msg.header.flags);
        }

        vTaskDelayUntil(&xLastExecutionTime, xPeriod);
    }
}

// ============================================================
// Task: Message Consumer 1
// ============================================================
void MessageConsumerTask(void *pvParameters) {
    ConsoleUtilsPrintf("[MSG] Task_MessageConsumer1 created\r\n");

    for (;;) {
        MessageBuffer msg;
        TickType_t xTimeout = pdMS_TO_TICKS(500);

        if (xQueueReceive(mb_queue, &msg, xTimeout) == pdPASS) {
            received_count++;

            // 1. Verify magic
            if (msg.header.magic != MESSAGE_MAGIC_START) {
                ConsoleUtilsPrintf("[ERROR] Bad magic: 0x%08X\r\n", (unsigned)msg.header.magic);
                continue;
            }

            // 2. Verify length
            if (msg.header.length < 16 || msg.header.length > MB_MAX_MESSAGE_SIZE) {
                ConsoleUtilsPrintf("[ERROR] Bad length: %d\r\n", (int)msg.header.length);
                continue;
            }

            // 3. Verify CRC
            uint8_t crc_data[272];
            memcpy(crc_data, &msg.header, 16);
            memcpy(crc_data + 16, msg.payload, msg.header.length);
            uint16_t expected_crc = calc_crc16(crc_data, 16 + msg.header.length);
            
            if (expected_crc != msg.header.checksum) {
                ConsoleUtilsPrintf("[ERROR] CRC bad: got=0x%04X exp=0x%04X\r\n",
                                   (int)msg.header.checksum, (int)expected_crc);
                continue;
            }

            // 4. Verify footer magic
            if (msg.footer.magic_end != MESSAGE_MAGIC_END) {
                ConsoleUtilsPrintf("[ERROR] Bad footer magic: 0x%08X\r\n", (unsigned)msg.footer.magic_end);
                continue;
            }

            // 5. Verify footer CRC
            uint16_t fk_crc = calc_crc16((uint8_t*)&msg.footer.magic_end, sizeof(uint32_t));
            if (fk_crc != msg.footer.footer_checksum) {
                ConsoleUtilsPrintf("[ERROR] Footer CRC bad\r\n");
                continue;
            }

            // All validations passed
            ConsoleUtilsPrintf("[MSG] Rev #%d seq=%d [%d] ",
                               (int)received_count, (int)msg.header.sequence, (int)msg.header.length);
            for (uint16_t i = 0; i < msg.header.length; i++) {
                ConsoleUtilsPrintf("%c", msg.payload[i]);
            }
            if (msg.header.flags & MSG_FLAG_FINAL) {
                ConsoleUtilsPrintf(" FINAL");
            }
            ConsoleUtilsPrintf("\r\n");

        } else {
            ConsoleUtilsPrintf("[WARN] Queue empty\r\n");
        }
    }
}

// ============================================================
// Task: Message Consumer 2
// ============================================================
void MessageConsumerTask2(void *pvParameters) {
    ConsoleUtilsPrintf("[MSG] Task_MessageConsumer2 created\r\n");

    static uint32_t tick_cnt = 0;
    
    for (;;) {
        MessageBuffer msg;
        TickType_t xTimeout = pdMS_TO_TICKS(500);

        if (xQueueReceive(mb_queue, &msg, xTimeout) == pdPASS) {
            received_count++;
            ConsoleUtilsPrintf("[C2] #%d seq=%d ", (int)received_count, (int)msg.header.sequence);
            for (uint16_t i = 0; i < msg.header.length; i++) {
                ConsoleUtilsPrintf("%c", msg.payload[i]);
            }
            ConsoleUtilsPrintf("\r\n");
        } else {
            tick_cnt++;
            if (tick_cnt % 10 == 0) {
                ConsoleUtilsPrintf("[C2] ...\r\n");
            }
        }
    }
}
