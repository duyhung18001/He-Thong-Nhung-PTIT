#include "stm32f10x.h"
#include <stdio.h>

/* Hàm Delay đơn giản (tương đối cho tần số 72MHz) */
void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 12000; i++) {
        __NOP();
    }
}

/* Gửi 1 ký tự qua UART1 */
void UART1_SendChar(char c) {
    while (!(USART1->SR & USART_SR_TXE)); /* Chờ bộ đệm trống */
    USART1->DR = c;
}

/* Gửi một chuỗi ký tự qua UART1 */
void UART1_SendString(char *str) {
    while (*str) {
        UART1_SendChar(*str++);
    }
}

int main(void) {
    /* 1. Bật Clock cho GPIOA, ADC1, USART1 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_ADC1EN | RCC_APB2ENR_USART1EN;

    /* 2. Cấu hình chân PA0 làm Input Analog (MODE0=00, CNF0=00) */
    GPIOA->CRL &= ~(0x0F << 0); 

    /* 3. Cấu hình chân PA9 (TX USART1) -> Alternate Function Push-Pull 10MHz */
    GPIOA->CRH &= ~(0x0F << 4); /* Xóa 4 bit của PA9 (Bit 4..7) */
    GPIOA->CRH |=  (0x09 << 4); /* MODE9=01 (10MHz), CNF9=10 (AF-PP) -> 0x9 */

    /* 4. Cấu hình USART1 (Baudrate 9600 tại PCLK2 = 72MHz -> USARTDIV = 468.75) */
    USART1->BRR = (468 << 4) | 12;               /* 0x1D4C */
    USART1->CR1 |= USART_CR1_TE | USART_CR1_UE; /* Bật Tx và Bật USART1 */

    /* 5. Cấu hình ADC1 */
    ADC1->CR2 |= ADC_CR2_ADON;                   /* Bật nguồn ADC1 */
    delay_ms(1);                                 /* Chờ ổn định */
    ADC1->CR2 |= ADC_CR2_CAL;                    /* Hiệu chuẩn ADC */
    while (ADC1->CR2 & ADC_CR2_CAL);             /* Chờ hiệu chuẩn hoàn tất */

    ADC1->SQR3 = 0;                              /* Chọn kênh 0 (PA0) */
    ADC1->SMPR2 |= ADC_SMPR2_SMP0_2 | ADC_SMPR2_SMP0_1 | ADC_SMPR2_SMP0_0; /* 239.5 cycles */

    char buffer[64];

    /* In thông báo khởi tạo thành công để test UART ngay khi bật nguồn */
    UART1_SendString("\r\n=== STM32 ADC & UART READY ===\r\n");

    /* 6. Vòng lặp chính */
    while (1) {
        /* Bắt đầu chuyển đổi ADC */
        ADC1->SR &= ~ADC_SR_EOC;                 /* Xóa cờ EOC */
        ADC1->CR2 |= ADC_CR2_ADON;               /* Bắt đầu chuyển đổi */
        while (!(ADC1->SR & ADC_SR_EOC));        /* Chờ chuyển đổi xong */

        uint16_t adc_val = ADC1->DR;             /* Đọc giá trị ADC 12-bit (0 - 4095) */

        /* Tính toán điện áp (mV) */
        uint32_t voltage_mv = (adc_val * 3300) / 4095;
        uint32_t volt_int = voltage_mv / 1000;   /* Phần nguyên */
        uint32_t volt_dec = voltage_mv % 1000;   /* Phần thập phân */

        /* Format chuỗi xuất ra UART */
        sprintf(buffer, "ADC Raw: %4d | Dien ap: %lu.%03lu V\r\n", adc_val, volt_int, volt_dec);
        UART1_SendString(buffer);

        /* Chờ 1 giây */
        delay_ms(1000);
    }

    return 0;
}

