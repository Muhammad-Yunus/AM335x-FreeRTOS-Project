#ifndef STREAM_BUFFER_DEMO_H
#define STREAM_BUFFER_DEMO_H

#include "FreeRTOS.h"
#include "stream_buffer.h"

extern StreamBufferHandle_t gStreamBuffer;

void vProducerTask(void *pvParameters);
void vConsumerTask(void *pvParameters);

#endif /* STREAM_BUFFER_DEMO_H */
