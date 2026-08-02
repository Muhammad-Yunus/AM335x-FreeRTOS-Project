/* 
 * \file   MutexDemo.c
 *
 * \brief  Demonstrates FreeRTOS Mutex protecting a Shared Resource.
 *
 * Two tasks (Writer and Reader) access a shared "account" struct. Every
 * read-modify-write and every dump of the shared resource is guarded by
 * xSharedResourceMutex, so the update + read-back is atomic with respect to
 * the other task. Without the mutex, Writer could be preempted between
 * "read balance" and "write balance", losing one update.
 */

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "consoleUtils.h"

#include "MutexDemo.h"

/* Created in main.c before the scheduler starts. */
extern SemaphoreHandle_t xSharedResourceMutex;

#define MUTEX_DEMO_ITERATIONS   5UL
#define MUTEX_DEMO_WRITER_DELAY 150UL  /* ms between writer updates */
#define MUTEX_DEMO_READER_DELAY 175UL  /* ms between reader dumps */
#define MUTEX_DEMO_DELTA        100UL  /* credit applied per write */

/* -----------------------------------------------------------------------
 * Shared resource (a simulated bank account).
 * Protected by xSharedResourceMutex — NEVER touch it outside a critical
 * region guarded by that mutex.
 * ----------------------------------------------------------------------- */

typedef struct {
    uint32_t ulBalance;
    uint32_t ulWrites;
} SharedAccount_t;

static SharedAccount_t gSharedAccount = { 1000UL, 0UL };

/* -----------------------------------------------------------------------
 * Helpers — every access to gSharedAccount happens inside a mutex region.
 * ----------------------------------------------------------------------- */

static void vAccountUpdate(const char *pcOwner, uint32_t ulDelta)
{
    uint32_t ulBefore;

    if (xSemaphoreTake(xSharedResourceMutex, portMAX_DELAY) == pdTRUE)
    {
        /* Protected section: read-modify-write cannot be interleaved. */
        ulBefore = gSharedAccount.ulBalance;
        gSharedAccount.ulBalance += ulDelta;
        gSharedAccount.ulWrites++;

        ConsoleUtilsPrintf("[%s] MUTEX held: balance %u -> %u (writes=%u)\r\n",
                           pcOwner,
                           (unsigned int)ulBefore,
                           (unsigned int)gSharedAccount.ulBalance,
                           (unsigned int)gSharedAccount.ulWrites);

        xSemaphoreGive(xSharedResourceMutex);
    }
}

static void vAccountDump(const char *pcOwner)
{
    if (xSemaphoreTake(xSharedResourceMutex, portMAX_DELAY) == pdTRUE)
    {
        ConsoleUtilsPrintf("[%s] MUTEX held: dump balance=%u writes=%u\r\n",
                           pcOwner,
                           (unsigned int)gSharedAccount.ulBalance,
                           (unsigned int)gSharedAccount.ulWrites);

        xSemaphoreGive(xSharedResourceMutex);
    }
}

/* -----------------------------------------------------------------------
 * Task 1 — Writer (priority 2).
 * ----------------------------------------------------------------------- */

void vMutexWriterTask(void *pvParameters)
{
    uint32_t ulIter;

    (void)pvParameters;

    ConsoleUtilsPrintf("[Writer] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulIter = 0UL; ulIter < MUTEX_DEMO_ITERATIONS; ulIter++)
    {
        vAccountUpdate(pcTaskGetName(NULL), MUTEX_DEMO_DELTA);
        vTaskDelay(pdMS_TO_TICKS(MUTEX_DEMO_WRITER_DELAY));
    }

    ConsoleUtilsPrintf("[Writer] finished %u iterations, deleting self\r\n",
                       (unsigned int)ulIter);
    vTaskDelete(NULL);
}

/* -----------------------------------------------------------------------
 * Task 2 — Reader (priority 1, lower than Writer).
 * ----------------------------------------------------------------------- */

void vMutexReaderTask(void *pvParameters)
{
    uint32_t ulIter;

    (void)pvParameters;

    ConsoleUtilsPrintf("[Reader] task created (priority=%u)\r\n",
                       (unsigned int)uxTaskPriorityGet(NULL));

    for (ulIter = 0UL; ulIter < MUTEX_DEMO_ITERATIONS; ulIter++)
    {
        vAccountDump(pcTaskGetName(NULL));
        vTaskDelay(pdMS_TO_TICKS(MUTEX_DEMO_READER_DELAY));
    }

    ConsoleUtilsPrintf("[Reader] finished %u iterations, deleting self\r\n",
                       (unsigned int)ulIter);
    vTaskDelete(NULL);
}
