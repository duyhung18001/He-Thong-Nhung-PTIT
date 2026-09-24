/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32F103 + MAX7219 Matrix (SPI2 Register + PB12 CS Bare-metal)
  ******************************************************************************
  */

#include "stm32f10x.h"

/* Danh sach thanh ghi MAX7219 */
#define MAX7219_REG_NOOP        0x00
#define MAX7219_REG_DIGIT0      0x01
#define MAX7219_REG_DIGIT1      0x02
#define MAX7219_REG_DIGIT2      0x03
#define MAX7219_REG_DIGIT3      0x04
#define MAX7219_REG_DIGIT4      0x05
#define MAX7219_REG_DIGIT5      0x06
#define MAX7219_REG_DIGIT6      0x07
#define MAX7219_REG_DIGIT7      0x08
#define MAX7219_REG_DECODEMODE  0x09
#define MAX7219_REG_INTENSITY   0x0A
#define MAX7219_REG_SCANLIMIT   0x0B
#define MAX7219_REG_SHUTDOWN    0x0C
#define MAX7219_REG_DISPLAYTEST 0x0F

/* Bitmap hinh so "1" */
const uint8_t NUMBER_ONE[8] = {
    0b00010000, //   *  
    0b00110000, //  **  
    0b00010000, //   *  
    0b00010000, //   *  
    0b00010000, //   *  
    0b00010000, //   *  
    0b00111100, // **** 
    0b00000000  //      
};

/* Nguyen mau ham */
void SystemClock_Config(void);
void User_GPIO_Init(void);
void SPI2_Init(void);
void MAX7219_Write(uint8_t address, uint8_t data);
void MAX7219_Init(void);
void MAX7219_Clear(void);
void MAX7219_DisplayImage(const uint8_t *image);
void Delay_Ms(uint32_t ms);

int main(void)
{
    SystemClock_Config();
    
    // Khoi tao GPIO va SPI2 bang thanh ghi
    User_GPIO_Init();
    SPI2_Init();

    // Khoi tao MAX7219
    MAX7219_Init();
    MAX7219_Clear();
    Delay_Ms(50);

    // Hien thi so 1 len ma tran LED
    MAX7219_DisplayImage(NUMBER_ONE);

    while (1)
    {
        // Nhap nhay LED onboard PC13 (500ms) xac nhan chip chay
        GPIOC->ODR ^= (1 << 13);
        Delay_Ms(500);
    }
}

/* Khoi tao GPIO: PB12 (CS), PB13 (SCK), PB15 (MOSI), PC13 (LED Onboard) */
void User_GPIO_Init(void)
{
    // Bat clock cho GPIOB va GPIOC
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    // --- Cau hinh PB12 (CS): Output Push-Pull, Max speed 50MHz ---
    GPIOB->CRH &= ~(GPIO_CRH_CNF12 | GPIO_CRH_MODE12);
    GPIOB->CRH |= GPIO_CRH_MODE12_0 | GPIO_CRH_MODE12_1; 
    GPIOB->BSRR = (1 << 12); // Keo muc HIGH ban dau cho CS

    // --- Cau hinh PB13 (SCK) va PB15 (MOSI): Alternate Function Push-Pull, Max speed 50MHz ---
    GPIOB->CRH &= ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13);
    GPIOB->CRH |= GPIO_CRH_CNF13_1 | GPIO_CRH_MODE13_0 | GPIO_CRH_MODE13_1; 
    
    GPIOB->CRH &= ~(GPIO_CRH_CNF15 | GPIO_CRH_MODE15);
    GPIOB->CRH |= GPIO_CRH_CNF15_1 | GPIO_CRH_MODE15_0 | GPIO_CRH_MODE15_1; 

    // --- Cau hinh PC13 (LED Onboard): Output Push-Pull, Max speed 2MHz ---
    GPIOC->CRH &= ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13);
    GPIOC->CRH |= GPIO_CRH_MODE13_1; 
    GPIOC->BSRR = (1 << 13);
}

/* Khoi tao ngoai vi SPI2 */
void SPI2_Init(void)
{
    // Bat clock cho SPI2
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;

    // Cau hinh SPI2: Master mode, CPOL=0, CPHA=0, BaudRate = fPCLK/64, SSI/SSM active
    SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_2 | SPI_CR1_BR_0 | SPI_CR1_SSM | SPI_CR1_SSI;
    
    // Bat SPI2
    SPI2->CR1 |= SPI_CR1_SPE;
}

/* Ham truyen du lieu qua thanh ghi SPI2 */
void MAX7219_Write(uint8_t address, uint8_t data)
{
    // Keo CS (PB12) xuong LOW
    GPIOB->BSRR = (1 << (12 + 16)); 

    // Gui Byte 1: Dia chi thanh ghi
    while (!(SPI2->SR & SPI_SR_TXE)); 
    *(volatile uint8_t *)(&SPI2->DR) = address;

    // Gui Byte 2: Du lieu
    while (!(SPI2->SR & SPI_SR_TXE));
    *(volatile uint8_t *)(&SPI2->DR) = data;

    // Doi truyen hoan tat
    while (SPI2->SR & SPI_SR_BSY);

    // Keo CS (PB12) len HIGH
    GPIOB->BSRR = (1 << 12); 
}

void MAX7219_Init(void)
{
    MAX7219_Write(MAX7219_REG_DISPLAYTEST, 0x00); 
    MAX7219_Write(MAX7219_REG_SCANLIMIT, 0x07);   
    MAX7219_Write(MAX7219_REG_DECODEMODE, 0x00);  
    MAX7219_Write(MAX7219_REG_INTENSITY, 0x07);   
    MAX7219_Write(MAX7219_REG_SHUTDOWN, 0x01);    
}

void MAX7219_Clear(void)
{
    for (uint8_t i = 1; i <= 8; i++)
    {
        MAX7219_Write(i, 0x00);
    }
}

void MAX7219_DisplayImage(const uint8_t *image)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        MAX7219_Write(i + 1, image[i]);
    }
}

void Delay_Ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        for (uint32_t j = 0; j < 7200; j++)
        {
            __NOP();
        }
    }
}

void SystemClock_Config(void)
{
}
