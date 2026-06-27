#include "sdram.h"

#define SDRAM_TIMEOUT                 0xFFFFU
#define SDRAM_REFRESH_COUNT           839U
#define SDRAM_TARGET_BANK             FMC_SDRAM_CMD_TARGET_BANK1

#define SDRAM_MODEREG_BURST_LENGTH_1  0x0000U
#define SDRAM_MODEREG_BURST_TYPE_SEQ  0x0000U
#define SDRAM_MODEREG_CAS_LATENCY_2   0x0020U
#define SDRAM_MODEREG_OPERATING_MODE  0x0000U
#define SDRAM_MODEREG_WRITEBURST_MODE 0x0200U

void SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram)
{
  FMC_SDRAM_CommandTypeDef command = {0};
  const uint32_t mode_register =
      SDRAM_MODEREG_BURST_LENGTH_1 |
      SDRAM_MODEREG_BURST_TYPE_SEQ |
      SDRAM_MODEREG_CAS_LATENCY_2 |
      SDRAM_MODEREG_OPERATING_MODE |
      SDRAM_MODEREG_WRITEBURST_MODE;

  command.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
  command.CommandTarget = SDRAM_TARGET_BANK;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = 0;
  HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT);
  HAL_Delay(1);

  command.CommandMode = FMC_SDRAM_CMD_PALL;
  HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT);

  command.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
  command.AutoRefreshNumber = 8;
  HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT);

  command.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
  command.AutoRefreshNumber = 1;
  command.ModeRegisterDefinition = mode_register;
  HAL_SDRAM_SendCommand(hsdram, &command, SDRAM_TIMEOUT);

  HAL_SDRAM_ProgramRefreshRate(hsdram, SDRAM_REFRESH_COUNT);
}
