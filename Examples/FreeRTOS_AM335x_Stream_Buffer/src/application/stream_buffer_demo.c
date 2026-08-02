#include "FreeRTOS.h"
#include "task.h"
#include "stream_buffer.h"
#include "consoleUtils.h"
#include "stream_buffer_demo.h"

StreamBufferHandle_t gStreamBuffer = NULL;

#define STREAM_BUFFER_SIZE  (64U)

static void prvPrintBytes(const uint8_t *pBuf, size_t xLen)
{
    size_t x;
    size_t xLimit = (xLen < 16) ? xLen : 16;
    for (x = 0; x < xLimit; x++)
        ConsoleUtilsPrintf(" 0x%02X", pBuf[x]);
    if (xLen > 16) ConsoleUtilsPrintf(" ...");
    ConsoleUtilsPrintf("\r\n");
}

void vProducerTask(void *pvParameters)
{
    size_t xReturned;
    uint8_t ucOneByte;
    uint32_t ulFourBytes;
    uint8_t ucSixteenBytes[16];
    uint8_t ucDeleteSignal;
    TickType_t xTimeoutMs;

    (void)pvParameters;

    /* --- Test 1: Send 1 byte (blocking) --- */
    ucOneByte = 0xAB;
    ConsoleUtilsPrintf("Producer: send 1 byte 0xAB (blocking)\r\n");
    xReturned = xStreamBufferSend(gStreamBuffer, &ucOneByte, 1, portMAX_DELAY);
    ConsoleUtilsPrintf("Producer: -> %u bytes sent\r\n", xReturned);

    /* --- Test 2: Send 4 bytes (blocking) --- */
    ulFourBytes = 0xDEADBEEF;
    ConsoleUtilsPrintf("Producer: send 4 bytes 0xDEADBEEF (blocking)\r\n");
    xReturned = xStreamBufferSend(gStreamBuffer, &ulFourBytes, 4, portMAX_DELAY);
    ConsoleUtilsPrintf("Producer: -> %u bytes sent\r\n", xReturned);

    /* --- Test 3: Send 16 bytes (blocking) --- */
    for (ucOneByte = 0; ucOneByte < 16; ucOneByte++)
        ucSixteenBytes[ucOneByte] = ucOneByte;
    ConsoleUtilsPrintf("Producer: send 16 bytes (0x00..0x0F) (blocking)\r\n");
    xReturned = xStreamBufferSend(gStreamBuffer, ucSixteenBytes, 16, portMAX_DELAY);
    ConsoleUtilsPrintf("Producer: -> %u bytes sent\r\n", xReturned);

    /* --- Test 4: Timeout send — send 64 bytes rapidly, then non-blocking test ---
       The consumer prints via UART (slow ~1ms/byte), so while consumer is
       printing, producer can fill the buffer faster than consumer drains it.
       After the rapid fill, we try a non-blocking send (timeout=0). */
    uint8_t ucRapidFill[64];
    for (ucOneByte = 0; ucOneByte < 64; ucOneByte++)
        ucRapidFill[ucOneByte] = (uint8_t)(0x10 + ucOneByte);

    ConsoleUtilsPrintf("Producer: rapid fill 64 bytes (consumer slow-prints)\r\n");
    xReturned = xStreamBufferSend(gStreamBuffer, ucRapidFill, sizeof(ucRapidFill), portMAX_DELAY);
    ConsoleUtilsPrintf("Producer: -> %u bytes sent\r\n", xReturned);

    /* Now try non-blocking send (timeout = 0 ticks).
       If buffer still has space → send succeeds.
       If buffer is full → send returns 0 (timeout). */
    xTimeoutMs = (TickType_t)0;  /* non-blocking */
    ucOneByte = 0xFF;
    xReturned = xStreamBufferSend(gStreamBuffer, &ucOneByte, 1, xTimeoutMs);
    if (xReturned == 0)
        ConsoleUtilsPrintf("Producer: non-blocking TIMEOUT (buffer full, 0 bytes sent)\r\n");
    else
        ConsoleUtilsPrintf("Producer: non-blocking SUCCESS (%u bytes sent, buffer had space)\r\n", xReturned);

    /* --- Test 5: Delete signal --- */
    ConsoleUtilsPrintf("Producer: sending delete signal (0xFE)\r\n");
    ucDeleteSignal = 0xFE;
    xStreamBufferSend(gStreamBuffer, &ucDeleteSignal, 1, portMAX_DELAY);

    ConsoleUtilsPrintf("Producer: deleting self\r\n");
    vTaskDelete(NULL);
}

void vConsumerTask(void *pvParameters)
{
    uint8_t ucRecvBuf[16];
    size_t xReturned;
    uint8_t ucMsgCount = 0;
    uint8_t ucTimeoutStreak = 0;

    (void)pvParameters;

    for (;;)
    {
        xReturned = xStreamBufferReceive(gStreamBuffer, ucRecvBuf, sizeof(ucRecvBuf), pdMS_TO_TICKS(500));

        if (xReturned == 0)
        {
            ucTimeoutStreak++;
            if (ucTimeoutStreak >= 2 && ucMsgCount > 0)
            {
                /* Two consecutive timeouts after receiving data → assume done */
                ConsoleUtilsPrintf("Consumer: timeout after %u messages\r\n", ucMsgCount);
                break;
            }
            continue;
        }

        ucTimeoutStreak = 0;
        ucMsgCount++;

        /* Check for delete signal */
        if (xReturned == 1 && ucRecvBuf[0] == 0xFE)
        {
            ConsoleUtilsPrintf("Consumer: recv 1 bytes 0xFE\r\n");
            ConsoleUtilsPrintf("Consumer: received delete signal\r\n");
            ConsoleUtilsPrintf("Consumer: deleting self\r\n");
            vTaskDelete(NULL);
            return;
        }

        ConsoleUtilsPrintf("Consumer: recv %u bytes", xReturned);
        prvPrintBytes(ucRecvBuf, xReturned);
    }
}
