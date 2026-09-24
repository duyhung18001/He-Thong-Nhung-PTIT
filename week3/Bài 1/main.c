
     /* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Optimized BMP280 I2C1 + Internal Pull-Up + USART2 Demo
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Thu vien cho labs()
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t bmp280_addr = 0;

uint16_t dig_T1;
int16_t  dig_T2, dig_T3;
uint16_t dig_P1;
int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
int32_t  t_fine;
uint8_t  bmp280_ok = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_Init(void);

/* USER CODE BEGIN PFP */
uint8_t BMP280_Init(void);
int32_t BMP280_CompensateT(int32_t adc_T);
uint32_t BMP280_CompensateP(int32_t adc_P);
void Force_I2C_Internal_Pullup(void);
/* USER CODE END PFP */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART2_Init();

  /* USER CODE BEGIN 2 */
  // Ep kieu bat Internal Pull-up cho PB6 va PB7 sau khi I2C Init xong
  Force_I2C_Internal_Pullup();

  char uart_buf[128];
  
  char *init_msg = "\r\n========================================\r\n"
                   "  STM32F103 - BMP280 (Auto-Scan I2C)    \r\n"
                   "========================================\r\n";
  HAL_UART_Transmit(&huart2, (uint8_t*)init_msg, strlen(init_msg), 100);

  if (BMP280_Init() == HAL_OK)
  {
      bmp280_ok = 1;
      snprintf(uart_buf, sizeof(uart_buf), "[OK] Ket noi BMP280 thanh cong tai dia chi: 0x%02X!\r\n", bmp280_addr >> 1);
      HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, strlen(uart_buf), 100);
  }
  else
  {
      bmp280_ok = 0;
      char *err_msg = "[Loi] Khong tim thay BMP280! Kiem tra PB6(SCL), PB7(SDA).\r\n";
      HAL_UART_Transmit(&huart2, (uint8_t*)err_msg, strlen(err_msg), 100);
  }
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
    if (bmp280_ok)
    {
        uint8_t raw_data[6];
        if (HAL_I2C_Mem_Read(&hi2c1, bmp280_addr, 0xF7, I2C_MEMADD_SIZE_8BIT, raw_data, 6, 100) == HAL_OK)
        {
            int32_t adc_P = (raw_data[0] << 12) | (raw_data[1] << 4) | (raw_data[2] >> 4);
            int32_t adc_T = (raw_data[3] << 12) | (raw_data[4] << 4) | (raw_data[5] >> 4);

            int32_t temp = BMP280_CompensateT(adc_T);
            uint32_t press = BMP280_CompensateP(adc_P) / 256;

            // Su dung labs() de tranh loi so am khi % 100
            int len = sprintf(uart_buf, "Nhiet do: %ld.%02ld C | Ap suat: %ld Pa\r\n", 
                              temp / 100, labs(temp % 100), press);
            HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, len, 100);
        }
        else
        {
            bmp280_ok = 0; // Mat ket noi, quay lai che do scan
            char *read_err = "[Loi] Mat ket noi I2C trong khi doc!\r\n";
            HAL_UART_Transmit(&huart2, (uint8_t*)read_err, strlen(read_err), 100);
        }
    }
    else
    {
        if (BMP280_Init() == HAL_OK)
        {
            bmp280_ok = 1;
            snprintf(uart_buf, sizeof(uart_buf), "[OK] Da ket noi lai BMP280 tai: 0x%02X!\r\n", bmp280_addr >> 1);
            HAL_UART_Transmit(&huart2, (uint8_t*)uart_buf, strlen(uart_buf), 100);
        }
        else
        {
            char *ping_msg = "Dang cho ket noi BMP280...\r\n";
            HAL_UART_Transmit(&huart2, (uint8_t*)ping_msg, strlen(ping_msg), 100);
        }
    }

    HAL_Delay(1000);
    /* USER CODE END 3 */
  }
}

/* USER CODE BEGIN 4 */
// Ham ho tro bat Pull-up cho I2C1
void Force_I2C_Internal_Pullup(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

uint8_t BMP280_Init(void)
{
  uint8_t addrs[2] = {0x76 << 1, 0x77 << 1};
  uint8_t calib[24];
  HAL_StatusTypeDef status = HAL_ERROR;

  for (int i = 0; i < 2; i++)
  {
      if (HAL_I2C_IsDeviceReady(&hi2c1, addrs[i], 2, 10) == HAL_OK)
      {
          bmp280_addr = addrs[i];
          status = HAL_OK;
          break;
      }
  }

  if (status != HAL_OK) return HAL_ERROR;

  if (HAL_I2C_Mem_Read(&hi2c1, bmp280_addr, 0x88, I2C_MEMADD_SIZE_8BIT, calib, 24, 100) != HAL_OK)
  {
      return HAL_ERROR;
  }

  dig_T1 = (calib[1] << 8) | calib[0];
  dig_T2 = (calib[3] << 8) | calib[2];
  dig_T3 = (calib[5] << 8) | calib[4];
  dig_P1 = (calib[7] << 8) | calib[6];
  dig_P2 = (calib[9] << 8) | calib[8];
  dig_P3 = (calib[11] << 8) | calib[10];
  dig_P4 = (calib[13] << 8) | calib[12];
  dig_P5 = (calib[15] << 8) | calib[14];
  dig_P6 = (calib[17] << 8) | calib[16];
  dig_P7 = (calib[19] << 8) | calib[18];
  dig_P8 = (calib[21] << 8) | calib[20];
  dig_P9 = (calib[23] << 8) | calib[22];

  uint8_t config[2] = {0xF4, 0x27};
  return HAL_I2C_Master_Transmit(&hi2c1, bmp280_addr, config, 2, 100);
}

int32_t BMP280_CompensateT(int32_t adc_T)
{
  int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
  int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
  t_fine = var1 + var2;
  return (t_fine * 5 + 128) >> 8;
}

uint32_t BMP280_CompensateP(int32_t adc_P)
{
  int64_t var1, var2, p;
  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)dig_P6;
  var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
  var2 = var2 + (((int64_t)dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
  if (var1 == 0) return 0;
  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
  return (uint32_t)p;
}
/* USER CODE END 4 */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { while(1); }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) { while(1); }
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) { while(1); }
}

static void MX_USART2_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK) { while(1); }
}

static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
}
