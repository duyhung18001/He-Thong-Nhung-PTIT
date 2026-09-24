#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>


// Bộ đệm cho DMA truyền dữ liệu
char tx_buffer[64];
uint32_t btn_count = 0;

// Cấu hình Nút nhấn (Giả sử nối vào chân PA0)
void GPIO_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

// Hàm cấu hình riêng cho UART
void UART_Config(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // Cấp xung nhịp cho USART1 và GPIOA
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    // Cấu hình chân TX (PA9)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Cấu hình chân RX (PA10)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Cấu hình thông số USART1 (Baud 115200)
    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);

    // Bật tín hiệu yêu cầu DMA từ USART1 cho bộ truyền (TX)
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    
    // Kích hoạt USART1
    USART_Cmd(USART1, ENABLE);
}

// Hàm cấu hình riêng cho DMA
void DMA_Config(void) {
    DMA_InitTypeDef DMA_InitStructure;

    // Cấp xung nhịp cho DMA1
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // Cấu hình DMA1 Channel 4 (Kênh mặc định phục vụ USART1_TX)
    DMA_DeInit(DMA1_Channel4);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR; // Địa chỉ ghi dữ liệu của UART
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)tx_buffer;       // Địa chỉ chuỗi trên RAM
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;                // Hướng: RAM -> Ngoại vi
    DMA_InitStructure.DMA_BufferSize = 0;                             // Kích thước (sẽ set lúc truyền)
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;  // Địa chỉ UART giữ nguyên
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;           // Địa chỉ chuỗi trên RAM tự tăng
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; 
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;                     // Truyền 1 lần rồi dừng
    DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &DMA_InitStructure);
}

// Hàm kích hoạt gửi DMA 
void Send_UART_DMA(void) {
    sprintf(tx_buffer, "Lop04_Nhom06:BTN:%lu\n\r", btn_count);
    
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_SetCurrDataCounter(DMA1_Channel4, strlen(tx_buffer));
    DMA_Cmd(DMA1_Channel4, ENABLE);
}

// Hàm trễ cơ bản dùng cho chống dội phím
void Delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 4000; i++); 
}

int main(void) {
    // Gọi lần lượt các hàm khởi tạo
    GPIO_Config();
    UART_Config();
    DMA_Config();

    uint8_t btn_last_state = 1; 
    
    while (1) {
        uint8_t btn_current_state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);

        if (btn_current_state == 0 && btn_last_state == 1) {
            Delay_ms(20); 
            
            if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 0) {
                btn_count++;       
                Send_UART_DMA();   
            }
        }
        
        btn_last_state = btn_current_state;
        Delay_ms(10); 
    }
}