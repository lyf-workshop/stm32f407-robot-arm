/**
  ******************************************************************************
  * @file    robot_cmd.h
  * @brief   Robot UART command parser for control and RL interface
  ******************************************************************************
  */

#ifndef __ROBOT_CMD_H__
#define __ROBOT_CMD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define ROBOT_CMD_BUF_SIZE 256

/**
 * @brief Initialize UART command parser
 */
void RobotCmd_Init(void);

/**
 * @brief Process received UART data
 * @param data Received data buffer
 * @param len Data length
 */
void RobotCmd_Process(uint8_t *data, uint16_t len);

/**
 * @brief UART receive complete callback (called from ISR or DMA callback)
 * @param data Received byte
 */
void RobotCmd_ReceiveByte(uint8_t data);

/**
 * @brief Send response via UART
 * @param msg Message string
 */
void RobotCmd_SendResponse(const char *msg);

#ifdef __cplusplus
}
#endif

#endif /* __ROBOT_CMD_H__ */
