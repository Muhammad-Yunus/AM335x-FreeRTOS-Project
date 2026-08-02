/* 
 * \file   UARTProtectionDemo.c
 *
 * \brief  Demonstrates Peripheral Protection — the UART console as a shared
 *         peripheral guarded by a mutex.
 *
 * Two demonstrations:
 *   1. Frame writers: vUARTFrameTaskA / vUARTFrameTaskB each print a
 *      multi-line "frame". The whole frame (BEGIN ... END) is printed while
 *      holding xUARTMutex, so frames never interleave. Without the mutex the
 *      lines of the two tasks would mix in the middle of a frame.
 *   2. UART echo simulation: vUARTEchoProducerTask "receives" characters on a
 *      simulated RX queue; vUARTEchoTask dequeues them and echoes them back
 *      out on TX. The read + echo transaction is atomic because it is done
 *      inside xUARTMutex — just like a real driver serializing TX access.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "consoleUtils.h"

#include "UARTProtectionDemo.h"

/* Created in main.c before the scheduler starts. */
extern SemaphoreHandle_t xUARTMutex;

#define UART_FRAME_LINES      4UL
#define UART_FRAME_A_DELAY    400UL  /* ms between frames */
#define UART_FRAME_B_DELAY    600UL  /* ms between frames */
#define UART_FRAME_ITERATIONS 3UL

#define UART_ECHO_CHARS       "A1B2C3D4E5F6G7H8I9J0"
#define UART_ECHO_PERIOD_MS   250UL
#define UART_ECHO_QUEUE_LEN   8U

/* Simulated UART RX queue (filled by producer, drained by echo task). */
static QueueHandle_t xRxQueue = NULL;

/* -----------------------------------------------------------------------
 * Frame writer — prints one whole frame atomically under xUARTMutex.
 * ----------------------------------------------------------------------- */

static void vPrintUARTFrame(const char *pcOwner)
{
    uint32_t ulLine;

    if (xSemaphoreTake(xUARTMutex, portMAX_DELAY) == pdTRUE)
    {
        ConsoleUtilsPrintf("[UART] frame BEGIN  owner=%s\r\n", pcOwner);
        for (ulLine = 0UL; ulLine < UART_FRAME_LINES; ulLine++)
        {
            ConsoleUtilsPrintf("[UART]   %s frame line %u (protected)\r\n",
                               pcOwner, (unsigned int)(ulLine + 1UL));
        }
        ConsoleUtilsPrintf("[UART] frame END    owner=%s\r\n", pcOwner);
        xSemaphoreGive(xUARTMutex);
    }
}

void vUARTFrameTaskA(void *pvParameters)
{
    uint32_t ulIter;

    (void)pvParameters;

    ConsoleUtilsPrintf("[UART-A] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulIter = 0UL; ulIter < UART_FRAME_ITERATIONS; ulIter++)
    {
        vPrintUARTFrame("A");
        vTaskDelay(pdMS_TO_TICKS(UART_FRAME_A_DELAY));
    }

    ConsoleUtilsPrintf("[UART-A] finished %u frames, deleting self\r\n",
                       (unsigned int)ulIter);
    vTaskDelete(NULL);
}

void vUARTFrameTaskB(void *pvParameters)
{
    uint32_t ulIter;

    (void)pvParameters;

    ConsoleUtilsPrintf("[UART-B] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulIter = 0UL; ulIter < UART_FRAME_ITERATIONS; ulIter++)
    {
        vPrintUARTFrame("B");
        vTaskDelay(pdMS_TO_TICKS(UART_FRAME_B_DELAY));
    }

    ConsoleUtilsPrintf("[UART-B] finished %u frames, deleting self\r\n",
                       (unsigned int)ulIter);
    vTaskDelete(NULL);
}

/* -----------------------------------------------------------------------
 * UART echo simulation — producer generates "received" characters.
 * ----------------------------------------------------------------------- */

void vUARTEchoProducerTask(void *pvParameters)
{
    uint32_t ulIdx;
    const char *pcChars = UART_ECHO_CHARS;

    (void)pvParameters;

    ConsoleUtilsPrintf("[ECHO-RX] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulIdx = 0UL; pcChars[ulIdx] != '\0'; ulIdx++)
    {
        char c = pcChars[ulIdx];
        xQueueSend(xRxQueue, &c, portMAX_DELAY);
        ConsoleUtilsPrintf("[ECHO-RX] simulated UART RX byte '%c' queued\r\n", c);
        vTaskDelay(pdMS_TO_TICKS(UART_ECHO_PERIOD_MS));
    }

    ConsoleUtilsPrintf("[ECHO-RX] finished, deleting self\r\n");
    vTaskDelete(NULL);
}

/* -----------------------------------------------------------------------
 * UART echo simulation — dequeues a byte and echoes it back on TX under the
 * UART mutex (peripheral TX is a shared, serialized resource).
 * ----------------------------------------------------------------------- */

void vUARTEchoTask(void *pvParameters)
{
    char c;

    (void)pvParameters;

    ConsoleUtilsPrintf("[ECHO-TX] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (;;)
    {
        if (xQueueReceive(xRxQueue, &c, portMAX_DELAY) == pdTRUE)
        {
            if (xSemaphoreTake(xUARTMutex, portMAX_DELAY) == pdTRUE)
            {
                ConsoleUtilsPrintf("[ECHO-TX] echo '%c' -> %c (UART TX protected)\r\n", c, c);
                xSemaphoreGive(xUARTMutex);
            }
        }
    }
}

/* Exposed for main.c: creates the simulated RX queue before scheduler start. */
QueueHandle_t xUARTEchoQueueInit(void)
{
    xRxQueue = xQueueCreate(UART_ECHO_QUEUE_LEN, sizeof(char));
    return xRxQueue;
}
