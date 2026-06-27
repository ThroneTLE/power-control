#ifndef SDRAM_H
#define SDRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

void SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram);

#ifdef __cplusplus
}
#endif

#endif /* SDRAM_H */
