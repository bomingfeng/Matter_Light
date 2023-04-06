#ifndef RF433_Task_H
#define RF433_Task_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_inclued.h"

void RF433_GPIO_Init(void);
void RF433_TX_ready(uint8_t Encoded);
void RF433_Task(void *pvParam);

#ifdef __cplusplus
}
#endif

#endif /* RF433_Task_H */