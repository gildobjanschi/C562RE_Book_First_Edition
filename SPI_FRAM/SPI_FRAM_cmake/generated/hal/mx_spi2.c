/**
  ******************************************************************************
  * @file           : mx_spi2.c
  * @brief          : SPI2 Peripheral initialization
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
#include "mx_spi2.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private functions prototype------------------------------------------------*/
/* Exported variables by reference--------------------------------------------*/
static hal_spi_handle_t hSPI2;
static hal_dma_handle_t hLPDMA1_CH0;
static hal_dma_handle_t hLPDMA1_CH1;

/******************************************************************************/
/* Exported functions for SPI in HAL layer */
/******************************************************************************/
hal_spi_handle_t *mx_spi2_init(void)
{
  hal_spi_config_t spi_config;

  if (HAL_SPI_Init(&hSPI2, HAL_SPI2) != HAL_OK)
  {
    return NULL;
  }

  HAL_RCC_SPI2_EnableClock();

  if (HAL_RCC_SPI2_SetKernelClkSource(HAL_RCC_SPI2_CLK_SRC_PCLK1) != HAL_OK)
  {
    return NULL;
  }

  spi_config.mode = HAL_SPI_MODE_MASTER;
  spi_config.direction = HAL_SPI_DIRECTION_FULL_DUPLEX;
  spi_config.data_width = HAL_SPI_DATA_WIDTH_8_BIT;
  spi_config.clock_polarity = HAL_SPI_CLOCK_POLARITY_LOW;
  spi_config.clock_phase = HAL_SPI_CLOCK_PHASE_1_EDGE;
  spi_config.baud_rate_prescaler = HAL_SPI_BAUD_RATE_PRESCALER_8;
  spi_config.first_bit = HAL_SPI_MSB_FIRST;
  spi_config.nss_pin_management = HAL_SPI_NSS_PIN_MGMT_OUTPUT;

  if (HAL_SPI_SetConfig(&hSPI2, &spi_config) != HAL_OK)
  {
    return NULL;
  }

  hal_spi_nss_config_t spi_nss_config;

  spi_nss_config.nss_pulse = HAL_SPI_NSS_PULSE_DISABLE;
  spi_nss_config.nss_polarity = HAL_SPI_NSS_POLARITY_LOW;
  spi_nss_config.nss_mssi_delay = HAL_SPI_NSS_MSSI_DELAY_0_CYCLE;

  if (HAL_SPI_SetConfigNSS(&hSPI2, &spi_nss_config) != HAL_OK)
  {
    return NULL;
  }

  /* ### SPI2 GPIO Configuration ########################### */
  /* GPIO Clocks activation */
  HAL_RCC_GPIOB_EnableClock();

  hal_gpio_config_t  gpio_config;

  /**
    [GPIO Pin] ------> [Signal Name] ------> [Labels]

       PB10    ------>   SPI2_SCK   ------>  PB10
       PB12    ------>   SPI2_NSS   ------>  PB12
    **/
  gpio_config.mode        = HAL_GPIO_MODE_ALTERNATE;
  gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
  gpio_config.pull        = HAL_GPIO_PULL_NO;
  gpio_config.speed       = HAL_GPIO_SPEED_FREQ_HIGH;
  gpio_config.alternate   = HAL_GPIO_AF_5;
  HAL_GPIO_Init(HAL_GPIOB, PB10_PIN | PB12_PIN, &gpio_config);

  /**
    [GPIO Pin] ------> [Signal Name] ------> [Labels]

       PB14    ------>   SPI2_MISO   ------>  PB14
       PB15    ------>   SPI2_MOSI   ------>  PB15
    **/
  gpio_config.mode        = HAL_GPIO_MODE_ALTERNATE;
  gpio_config.output_type = HAL_GPIO_OUTPUT_PUSHPULL;
  gpio_config.pull        = HAL_GPIO_PULL_NO;
  gpio_config.speed       = HAL_GPIO_SPEED_FREQ_VERY_HIGH;
  gpio_config.alternate   = HAL_GPIO_AF_5;
  HAL_GPIO_Init(HAL_GPIOB, PB14_PIN | PB15_PIN, &gpio_config);

  /* Configure the DMA TX */

  if (HAL_DMA_Init(&hLPDMA1_CH0, HAL_LPDMA1_CH0) != HAL_OK)
  {
    return NULL;
  }

  HAL_RCC_LPDMA1_EnableClock();

  hal_dma_direct_xfer_config_t xfer_cfg_spi2_tx_dma;
  xfer_cfg_spi2_tx_dma.request         = HAL_LPDMA1_REQUEST_SPI2_TX;
  xfer_cfg_spi2_tx_dma.direction       = HAL_DMA_DIRECTION_MEMORY_TO_PERIPH;
  xfer_cfg_spi2_tx_dma.src_inc         = HAL_DMA_SRC_ADDR_INCREMENTED;
  xfer_cfg_spi2_tx_dma.dest_inc        = HAL_DMA_DEST_ADDR_FIXED;
  xfer_cfg_spi2_tx_dma.src_data_width  = HAL_DMA_SRC_DATA_WIDTH_BYTE;
  xfer_cfg_spi2_tx_dma.dest_data_width = HAL_DMA_DEST_DATA_WIDTH_BYTE;
  xfer_cfg_spi2_tx_dma.priority        = HAL_DMA_PRIORITY_LOW_WEIGHT_LOW;

  if (HAL_DMA_SetConfigPeriphDirectXfer(&hLPDMA1_CH0, &xfer_cfg_spi2_tx_dma) != HAL_OK)
  {
    return NULL;
  }

  /* Enable the interruption for LPDMA1_CH0 */
  HAL_CORTEX_NVIC_SetPriority(LPDMA1_CH0_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(LPDMA1_CH0_IRQn);

  /* Link the Transmit DMA handle to the SPI handle */
  if (HAL_SPI_SetTxDMA(&hSPI2, &hLPDMA1_CH0) != HAL_OK)
  {
    return NULL;
  }

  /* Configure the DMA RX */

  if (HAL_DMA_Init(&hLPDMA1_CH1, HAL_LPDMA1_CH1) != HAL_OK)
  {
    return NULL;
  }

  HAL_RCC_LPDMA1_EnableClock();

  hal_dma_direct_xfer_config_t xfer_cfg_spi2_rx_dma;
  xfer_cfg_spi2_rx_dma.request         = HAL_LPDMA1_REQUEST_SPI2_RX;
  xfer_cfg_spi2_rx_dma.direction       = HAL_DMA_DIRECTION_PERIPH_TO_MEMORY;
  xfer_cfg_spi2_rx_dma.src_inc         = HAL_DMA_SRC_ADDR_FIXED;
  xfer_cfg_spi2_rx_dma.dest_inc        = HAL_DMA_DEST_ADDR_INCREMENTED;
  xfer_cfg_spi2_rx_dma.src_data_width  = HAL_DMA_SRC_DATA_WIDTH_BYTE;
  xfer_cfg_spi2_rx_dma.dest_data_width = HAL_DMA_DEST_DATA_WIDTH_BYTE;
  xfer_cfg_spi2_rx_dma.priority        = HAL_DMA_PRIORITY_LOW_WEIGHT_LOW;

  if (HAL_DMA_SetConfigPeriphDirectXfer(&hLPDMA1_CH1, &xfer_cfg_spi2_rx_dma) != HAL_OK)
  {
    return NULL;
  }

  /* Enable the interruption for LPDMA1_CH1 */
  HAL_CORTEX_NVIC_SetPriority(LPDMA1_CH1_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(LPDMA1_CH1_IRQn);

  /* Link the Receive DMA handle to the SPI handle */
  if (HAL_SPI_SetRxDMA(&hSPI2, &hLPDMA1_CH1) != HAL_OK)
  {
    return NULL;
  }

  /* Enable the interrupt for SPI */
  HAL_CORTEX_NVIC_SetPriority(SPI2_IRQn, HAL_CORTEX_NVIC_PREEMP_PRIORITY_5, HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(SPI2_IRQn);

  return &hSPI2;
}

void mx_spi2_deinit(void)
{
  /* Disable the interrupt for SPI */
  HAL_CORTEX_NVIC_DisableIRQ(SPI2_IRQn);

  (void)HAL_SPI_DeInit(&hSPI2);

  HAL_RCC_SPI2_Reset();

  HAL_RCC_SPI2_DisableClock();

  /* De-initialize all GPIOB pins associated with SPI2 */
  HAL_GPIO_DeInit(HAL_GPIOB, PB10_PIN | PB12_PIN | PB14_PIN | PB15_PIN);

  /* De-initialize the DMA channel */
  HAL_DMA_DeInit(&hLPDMA1_CH0);

  /* Disable the interruption for DMA */
  HAL_CORTEX_NVIC_DisableIRQ(LPDMA1_CH0_IRQn);

  /* De-initialize the DMA channel */
  HAL_DMA_DeInit(&hLPDMA1_CH1);

  /* Disable the interruption for DMA */
  HAL_CORTEX_NVIC_DisableIRQ(LPDMA1_CH1_IRQn);
}

hal_spi_handle_t *mx_spi2_gethandle(void)
{
  return &hSPI2;
}

/******************************************************************************/
/*                      LPDMA1 channel0 global interrupt                      */
/******************************************************************************/
void LPDMA1_CH0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hLPDMA1_CH0);
}

/******************************************************************************/
/*                      LPDMA1 channel1 global interrupt                      */
/******************************************************************************/
void LPDMA1_CH1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hLPDMA1_CH1);
}

/******************************************************************************/
/*                           SPI2 global interrupt                            */
/******************************************************************************/
void SPI2_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hSPI2);
}
