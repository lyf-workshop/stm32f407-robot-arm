/**
  ******************************************************************************
  * @file    usart.h
  * @brief   USART1 configuration and initialization
  ******************************************************************************
  */

#ifndef __usart_H
#define __usart_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern UART_HandleTypeDef huart1;

void MX_USART1_UART_Init(void);
void USART1_StartReceive(void);

#ifdef __cplusplus
}
#endif

#endif /* __usart_H */
