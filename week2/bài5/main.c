#include "stm32f10x.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define RX_BUFFER_SIZE 64

/* Khai báo biến toàn cục điều khiển */
volatile char rx_buffer[RX_BUFFER_SIZE];
volatile uint8_t rx_index = 0;
volatile uint8_t command_ready = 0;

uint8_t led_status = 0;       /* 0: OFF, 1: ON */
uint8_t current_duty = 0;     /* Mức Duty cycle lưu gần nhất (0 - 100%) */

/* Khai báo hàm prototype */
void USART2_SendString(const char *str);
void Process_Command(void);

int main(void) {
    /* 1. Bật Clock cho GPIOA, TIM2, USART2 và AFIO */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_USART2EN;

    /* 2. Cấu hình Chân GPIOA */
    /* PA0: Alternate Function Push-Pull 10MHz (TIM2_CH1 PWM) */
    GPIOA->CRL &= ~GPIO_CRL_MODE0;
    GPIOA->CRL |= GPIO_CRL_MODE0_0;       
    GPIOA->CRL &= ~GPIO_CRL_CNF0;
    GPIOA->CRL |= GPIO_CRL_CNF0_1;        

    /* PA2: Alternate Function Push-Pull 10MHz (USART2_TX) */
    GPIOA->CRL &= ~GPIO_CRL_MODE2;
    GPIOA->CRL |= GPIO_CRL_MODE2_0;
    GPIOA->CRL &= ~GPIO_CRL_CNF2;
    GPIOA->CRL |= GPIO_CRL_CNF2_1;

    /* PA3: Input with Pull-Up (USART2_RX) */
    GPIOA->CRL &= ~(GPIO_CRL_MODE3 | GPIO_CRL_CNF3);
    GPIOA->CRL |= GPIO_CRL_CNF3_1;        /* CNF3 = 10 (Input Pull-up/Pull-down) */
    GPIOA->ODR |= GPIO_ODR_ODR3;          /* ODR3 = 1 (Kích hoạt Pull-up) */

    /* 3. Cấu hình TIM2 Channel 1 (Tần số PWM = 1kHz) */
    TIM2->PSC = 71;                       /* Prescaler: 72MHz / (71 + 1) = 1MHz */
    TIM2->ARR = 999;                      /* Period: 1MHz / (999 + 1) = 1kHz */
    
    TIM2->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM2->CCMR1 |= (6 << 4) | TIM_CCMR1_OC1PE;
    TIM2->CCR1 = 0;                       
    TIM2->CCER |= TIM_CCER_CC1E;          
    TIM2->CR1 |= TIM_CR1_CEN;             

    /* 4. Cấu hình USART2 (Baudrate 115200, 8N1, Bật Ngắt RX) */
    /* USART2 chạy trên APB1 (36MHz). BRR = 36000000 / (16 * 115200) = 19.53125 -> 19 = 0x13, 0.53125*16 = 8.5 ~ 9 -> 0x139 */
    USART2->BRR = 0x139;                  
    
    USART2->CR1 |= USART_CR1_TE | USART_CR1_RE;     /* Bật Truyền/Nhận (TX/RX) */
    USART2->CR1 |= USART_CR1_RXNEIE;                 /* Bật ngắt khi nhận dữ liệu (RXNE) */
    USART2->CR1 |= USART_CR1_UE;                     /* Cho phép USART2 hoạt động */

    /* 5. Cấu hình NVIC cho ngắt USART2 */
    NVIC_EnableIRQ(USART2_IRQn);

    /* Thông báo khởi tạo thành công */
    USART2_SendString("\r\nSTM32 Ready (USART2)! Gui lenh ket thuc bang '!'\r\n");

    /* Vòng lặp chính */
    while (1) {
        if (command_ready) {
            Process_Command();
            command_ready = 0;            
        }
    }
}

/* ================== TRÌNH XỬ LÝ NGẮT USART2 ================== */
void USART2_IRQHandler(void) {
    if (USART2->SR & USART_SR_RXNE) {
        char ch = (char)(USART2->DR & 0xFF);

        if (ch == '!') {
            rx_buffer[rx_index] = '\0';   
            rx_index = 0;                 
            command_ready = 1;            
        } 
        else if (ch != '\r' && ch != '\n') {
            if (rx_index < RX_BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = ch;   
            } else {
                rx_index = 0;
            }
        }
    }
}

/* ================== HÀM PHÂN TÍCH & XỬ LÝ LỆNH ================== */
void Process_Command(void) {
    char response[64];

    if (strcmp((char*)rx_buffer, "ON") == 0) {
        led_status = 1;
        TIM2->CCR1 = (uint32_t)(current_duty * 10);
        snprintf(response, sizeof(response), "OK: LED ON (Duty: %d%%)\r\n", current_duty);
        USART2_SendString(response);
    }
    else if (strcmp((char*)rx_buffer, "OFF") == 0) {
        led_status = 0;
        TIM2->CCR1 = 0;                   
        USART2_SendString("OK: LED OFF\r\n");
    }
    else if (strncmp((char*)rx_buffer, "PWM:", 4) == 0) {
        int percent = atoi((char*)&rx_buffer[4]);

        if (percent >= 0 && percent <= 100) {
            current_duty = (uint8_t)percent;  

            if (led_status == 1) {
                TIM2->CCR1 = (uint32_t)(current_duty * 10);
                snprintf(response, sizeof(response), "OK: PWM updated to %d%%\r\n", current_duty);
            } else {
                snprintf(response, sizeof(response), "OK: PWM saved to %d%% (LED is OFF)\r\n", current_duty);
            }
            USART2_SendString(response);
        } else {
            USART2_SendString("ERROR: Percent must be 0 - 100\r\n");
        }
    }
    else if (strcmp((char*)rx_buffer, "Status") == 0) {
        snprintf(response, sizeof(response), "STATUS: State=%s | DutyConfig=%d%%\r\n",
                 (led_status == 1) ? "ON" : "OFF", current_duty);
        USART2_SendString(response);
    }
    else {
        snprintf(response, sizeof(response), "ERROR: Unknown Command ('%s')\r\n", rx_buffer);
        USART2_SendString(response);
    }
}

/* ================== HÀM HỖ TRỢ GỬI UART ================== */
void USART2_SendString(const char *str) {
    while (*str) {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = (*str++ & 0xFF);
    }
}
