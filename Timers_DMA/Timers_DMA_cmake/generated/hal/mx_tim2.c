/**
  ******************************************************************************
  * @file           : mx_tim2.c
  * @brief          : Peripheral initialization
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the mx_stm32c5xx_hal_drivers_license.md file
  * in the same directory as the generated code.
  * If no mx_stm32c5xx_hal_drivers_license.md file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "mx_tim2.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private functions prototype------------------------------------------------*/
/* Exported variables by reference--------------------------------------------*/
static hal_tim_handle_t hTIM2;
static hal_dma_node_t DMA_Node_TIM2_CC4;
static hal_dma_handle_t hLPDMA1_CH2;

/* Exported function definition ----------------------------------------------*/
/******************************************************************************/
/* Exported functions for TIM2 in HAL layer */
/******************************************************************************/
hal_tim_handle_t *mx_tim2_init(void)
{
  if (HAL_TIM_Init(&hTIM2, HAL_TIM2) != HAL_OK)
  {
    return NULL;
  }

  HAL_RCC_TIM2_EnableClock();

  /* Timer configuration to reach the output frequency at 1 kHz */
  hal_tim_config_t config;
  config.prescaler              = 143;
  config.counter_mode           = HAL_TIM_COUNTER_UP;
  config.period                 = 0x3E7;
  config.repetition_counter     = 0;
  config.clock_sel.clock_source = HAL_TIM_CLK_INTERNAL;
  if (HAL_TIM_SetConfig(&hTIM2, &config) != HAL_OK)
  {
    return NULL;
  }

  /* Sampling Clock */
  if (HAL_TIM_SetDTSPrescaler(&hTIM2, HAL_TIM_DTS_DIV1) != HAL_OK)
  {
    return NULL;
  }
  if (HAL_TIM_SetDTS2Prescaler(&hTIM2, HAL_TIM_DTS2_DIV1) != HAL_OK)
  {
    return NULL;
  }

  hal_tim_ic_channel_config_t ic_config;

  ic_config.source    = HAL_TIM_INPUT_TIM2_TI4_GPIO;
  ic_config.polarity  = HAL_TIM_IC_FALLING;
  ic_config.filter    = HAL_TIM_FDIV1;
  if (HAL_TIM_IC_SetConfigChannel(&hTIM2, HAL_TIM_CHANNEL_4, &ic_config) != HAL_OK)
  {
    return NULL;
  }

  hal_tim_ic_capture_unit_config_t ic_capture_unit_config;

  ic_capture_unit_config.source     = HAL_TIM_IC_DIRECT;
  ic_capture_unit_config.prescaler  = HAL_TIM_IC_DIV1;
  if (HAL_TIM_IC_SetConfigCaptureUnit(&hTIM2, HAL_TIM_IC_CAPTURE_UNIT_4, &ic_capture_unit_config) != HAL_OK)
  {
    return NULL;
  }

  /* Update Event Management */
  if (HAL_TIM_SetUpdateSource(&hTIM2, HAL_TIM_UPDATE_REGULAR) != HAL_OK)
  {
    return NULL;
  }
  if (HAL_TIM_EnableUpdateGeneration(&hTIM2) != HAL_OK)
  {
    return NULL;
  }
  /* Master Mode Configuration */
  /* Capture/Compare DMA request selection */
  if (HAL_TIM_SetCaptureCompareDMAReqSource(&hTIM2, HAL_TIM_CC_DMAREQ_CC) != HAL_OK)
  {
    return NULL;
  }

  /* ### TIM2 GPIO Configuration ########################### */
  /* GPIO Clocks activation */
  HAL_RCC_GPIOC_EnableClock();

  hal_gpio_config_t  gpio_config;

  /**
    [GPIO Pin] ------> [Signal Name] ------> [Labels]

       PC4     ------>   TIM2_CH4   ------>  PC4
    **/
  gpio_config.mode        = HAL_GPIO_MODE_ALTERNATE;
  gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
  gpio_config.pull        = HAL_GPIO_PULL_NO;
  gpio_config.speed       = HAL_GPIO_SPEED_FREQ_LOW;
  gpio_config.alternate   = HAL_GPIO_AF_1;
  HAL_GPIO_Init(PC4_PORT, PC4_PIN, &gpio_config);

  /* Enable the Timer global interrupt */
  HAL_CORTEX_NVIC_SetPriority(TIM2_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(TIM2_IRQn);

  /* Timer Capture/Compare 4 DMA Request Configuration */

  if (HAL_DMA_Init(&hLPDMA1_CH2, HAL_LPDMA1_CH2) != HAL_OK)
  {
    return NULL;
  }

  HAL_RCC_LPDMA1_EnableClock();

  hal_dma_direct_xfer_config_t xfer_cfg_tim2_cc4_dma;
  xfer_cfg_tim2_cc4_dma.request         = HAL_LPDMA1_REQUEST_TIM2_CC4;
  xfer_cfg_tim2_cc4_dma.direction       = HAL_DMA_DIRECTION_PERIPH_TO_MEMORY;
  xfer_cfg_tim2_cc4_dma.src_inc         = HAL_DMA_SRC_ADDR_FIXED;
  xfer_cfg_tim2_cc4_dma.dest_inc        = HAL_DMA_DEST_ADDR_INCREMENTED;
  xfer_cfg_tim2_cc4_dma.src_data_width  = HAL_DMA_SRC_DATA_WIDTH_WORD;
  xfer_cfg_tim2_cc4_dma.dest_data_width = HAL_DMA_DEST_DATA_WIDTH_WORD;
  xfer_cfg_tim2_cc4_dma.priority        = HAL_DMA_PRIORITY_LOW_WEIGHT_HIGH;

  if (HAL_DMA_SetConfigPeriphLinkedListCircularXfer(&hLPDMA1_CH2, &DMA_Node_TIM2_CC4, &xfer_cfg_tim2_cc4_dma) != HAL_OK)
  {
    return NULL;
  }

  /* Enable the interruption for LPDMA1_CH2 */
  HAL_CORTEX_NVIC_SetPriority(LPDMA1_CH2_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(LPDMA1_CH2_IRQn);

  /* Link Capture/Compare 4 DMA request handle to the TIM handle */
  if (HAL_TIM_SetDMA(&hTIM2, HAL_TIM_DMA_ID_CC4, &hLPDMA1_CH2) != HAL_OK)
  {
    return NULL;
  }

  return &hTIM2;
}

void mx_tim2_deinit(void)
{
  (void)HAL_TIM_DeInit(&hTIM2);

  HAL_RCC_TIM2_DisableClock();

  HAL_RCC_TIM2_Reset();

  /* De-initialize all GPIOC pins associated with TIM2 */
  HAL_GPIO_DeInit(PC4_PORT, PC4_PIN);

  /* Disable Timer global interrupt */
  HAL_CORTEX_NVIC_DisableIRQ(TIM2_IRQn);

  /* De-initialize the DMA channel */
  HAL_DMA_DeInit(&hLPDMA1_CH2);

  /* Disable the interruption for DMA */
  HAL_CORTEX_NVIC_DisableIRQ(LPDMA1_CH2_IRQn);
}

hal_tim_handle_t *mx_tim2_gethandle(void)
{
  return &hTIM2;
}

/******************************************************************************/
/*          TIM2 global interrupt is managed directly in user code.           */
/******************************************************************************/
/* void TIM2_IRQHandler(void)
{
}
  */

/******************************************************************************/
/*                      LPDMA1 channel1 global interrupt                      */
/******************************************************************************/
void LPDMA1_CH2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hLPDMA1_CH2);
}
