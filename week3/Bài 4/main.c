#include "stm32f10x.h"

#define BUFFER_SIZE 100
volatile uint16_t adc_buffer[BUFFER_SIZE];

volatile uint8_t flag_send_half = 0;
volatile uint8_t flag_send_full = 0;

// --- 1. CẤU HÌNH CLOCK CHUẨN HSI 8MHz ---
void Clock_Init_8MHz(void) {
    RCC->CR |= (1 << 0);                  // Bật HSI
    while (!(RCC->CR & (1 << 1)));        // Chờ HSI sẵn sàng

    RCC->CFGR &= ~(3 << 0);               // SYSCLK = HSI (8MHz)
    while ((RCC->CFGR & (3 << 2)) != 0);  // Chờ chuyển đổi SYSCLK

    RCC->CFGR &= ~(7 << 8);               // APB1 Prescaler = 1 (8MHz)
    RCC->CFGR &= ~(7 << 11);              // APB2 Prescaler = 1 (8MHz)
}

// --- 2. CẤU HÌNH UART2 (PA2 = TX, 9600 Baud) ---
void UART2_Init(void) {
    RCC->APB2ENR |= (1 << 2);  // Bật Clock GPIOA
    RCC->APB1ENR |= (1 << 17); // Bật Clock USART2

    // PA2: Alternate Function Push-Pull (50MHz)
    GPIOA->CRL &= ~(0xF << 8);
    GPIOA->CRL |=  (0xB << 8);

    // Baudrate = 8MHz / (16 * 9600) = 52.0833 -> BRR = 0x341
    USART2->BRR = 0x341;
    USART2->CR1 |= (1 << 3) | (1 << 2) | (1 << 13); // TE, RE, UE
}

void UART2_SendChar(char c) {
    while (!(USART2->SR & (1 << 7))); // Chờ TXE
    USART2->DR = c;
}

void UART2_SendString(const char *str) {
    while (*str) {
        UART2_SendChar(*str++);
    }
}

void UART2_SendNumber(uint16_t num) {
    char buf[10];
    int i = 0;
    if (num == 0) {
        UART2_SendChar('0');
        return;
    }
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    while (i > 0) {
        UART2_SendChar(buf[--i]);
    }
}

// --- 3. CẤU HÌNH PERIPHERALS (TIM3 + ADC1 + DMA1) ---
void Peripherals_Init(void) {
    // Bật Clock
    RCC->APB2ENR |= (1 << 2) | (1 << 9); // GPIOA, ADC1
    RCC->APB1ENR |= (1 << 1);            // TIM3 (Bit 1)
    RCC->AHBENR  |= (1 << 0);            // DMA1

    RCC->CFGR &= ~(3 << 14);             // ADCPRE = PCLK2 / 2 = 4MHz

    // PA0: Analog Input (ADC Channel 0)
    GPIOA->CRL &= ~(0xF << 0);

    // Timer 3: Tạo tần số 100Hz tại SYSCLK 8MHz
    // 8,000,000 / (799 + 1) / (99 + 1) = 100Hz
    TIM3->PSC = 799;
    TIM3->ARR = 99;
    TIM3->CR2 &= ~(7 << 4);
    TIM3->CR2 |=  (2 << 4);              // MMS = 010 (Update Event làm TRGO)

    // DMA1 Channel 1 cho ADC1
    DMA1_Channel1->CPAR  = (uint32_t)&(ADC1->DR);
    DMA1_Channel1->CMAR  = (uint32_t)(adc_buffer);
    DMA1_Channel1->CNDTR = BUFFER_SIZE;
    
    DMA1_Channel1->CCR = 0;
    DMA1_Channel1->CCR |= (1 << 5);      // CIRC (Circular)
    DMA1_Channel1->CCR |= (1 << 7);      // MINC (Memory Increment)
    DMA1_Channel1->CCR |= (1 << 10);     // PSIZE = 16-bit
    DMA1_Channel1->CCR |= (1 << 8);      // MSIZE = 16-bit
    DMA1_Channel1->CCR |= (1 << 3);      // TCIE (Transfer Complete Interrupt)
    DMA1_Channel1->CCR |= (1 << 2);      // HTIE (Half Transfer Interrupt)
    DMA1_Channel1->CCR |= (1 << 0);      // EN

    // Cấu hình ADC1
    ADC1->SMPR2 &= ~(7 << 0);
    ADC1->SMPR2 |=  (7 << 0);            // Sample time max cho Ch0
    ADC1->SQR3  &= ~0x1F;                 // Channel 0

    ADC1->CR2 &= ~(7 << 17);  
    ADC1->CR2 |=  (4 << 17);              // EXTSEL = 100 (TIM3 TRGO)
    ADC1->CR2 |=  (1 << 20);              // EXTTRIG = 1
    ADC1->CR2 |=  (1 << 8);               // DMA = 1

    // Calibration ADC
    ADC1->CR2 |= (1 << 0);                // ADON = 1
    for (volatile int i = 0; i < 2000; i++);

    ADC1->CR2 |= (1 << 2);                // Reset calibration
    while (ADC1->CR2 & (1 << 2));
    ADC1->CR2 |= (1 << 3);                // Start calibration
    while (ADC1->CR2 & (1 << 3));

    // Bật ngắt DMA1 Channel 1 trong NVIC
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    // Bắt đầu Timer 3
    TIM3->CR1 |= (1 << 0);                 // CEN = 1
}

int main(void) {
    Clock_Init_8MHz();
    UART2_Init();
    Peripherals_Init();

    // In kiểm tra UART ngay khi khởi động
    UART2_SendString("SYSTEM STARTED\r\n");

    while (1) {
        if (flag_send_half) {
            flag_send_half = 0;
            for (int i = 0; i < BUFFER_SIZE / 2; i++) {
                UART2_SendNumber(adc_buffer[i]);
                UART2_SendString("\r\n");
            }
        }

        if (flag_send_full) {
            flag_send_full = 0;
            for (int i = BUFFER_SIZE / 2; i < BUFFER_SIZE; i++) {
                UART2_SendNumber(adc_buffer[i]);
                UART2_SendString("\r\n");
            }
        }
    }
}

// --- HÀM XỬ LÝ NGẮT DMA1 CHANNEL 1 ---
void DMA1_Channel1_IRQHandler(void) {
    if (DMA1->ISR & (1 << 2)) { // HTIF1
        DMA1->IFCR |= (1 << 2); 
        flag_send_half = 1;     
    }

    if (DMA1->ISR & (1 << 1)) { // TCIF1
        DMA1->IFCR |= (1 << 1); 
        flag_send_full = 1;     
    }
}
