#ifndef TASKQUEUE_H
#define TASKQUEUE_H

#include <stdint.h>

#define QUEUE_LENGTH             (4U)
#define QUEUE_ITEM_SIZE          (sizeof(AppQueueMessage_DSType))

typedef struct {
    uint32_t ulMsgId;
    uint32_t ulData;
} AppQueueMessage_DSType;

void vProducerTask(void *pvParameters);
void vConsumerTask(void *pvParameters);

#endif /* TASKQUEUE_H */
