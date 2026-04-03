/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Robot Arm Control System Main Program
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "can.h"
#include "dma.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "Emm_V5.h"
#include "robot_config.h"
#include "robot_control.h"
#include "robot_kinematics.h"
#include "robot_cmd.h"
#include "lcd.h"
#include "usart.h"
#include "touch.h"
#include "touch_ui.h"
#include "touch_calib.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

  /* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static uint32_t s_last_display_ms = 0;   /* display refresh timestamp */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */
  

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_CAN1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
	
	// Initialize CAN filter and start CAN communication
	USER_CAN1_Filter_Init();
	if(HAL_CAN_Start(&hcan1) != HAL_OK) { 
		Error_Handler(); 
	}
	if(HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) { 
		Error_Handler(); 
	}
	
	// Wait for motor drivers to initialize (???????????????????????)
	HAL_Delay(500);
	
	// Initialize robot arm control system
	RobotCtrl_Init();
	
	// Initialize LCD, touch controller and unified UI
	Touch_Init();
	TouchUI_Init();
	
	// Initialize UART command parser and show welcome message
	RobotCmd_Init();
	
	// Enable UART receive interrupt
	USART1_StartReceive();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	
	static Touch_State_t touch_prev = {0};  // Previous touch state for edge detection
	
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		
		// Main loop: command processing is handled in UART interrupt

		// 1. Poll touch input (every 50ms to avoid bouncing)
		static uint32_t s_last_touch_ms = 0;
		uint32_t now = HAL_GetTick();
		
		if (now - s_last_touch_ms >= 50u) {
			s_last_touch_ms = now;
			
			Touch_State_t touch_current;
			if (Touch_Read(&touch_current)) {
				// Check if in calibration mode
				TouchCalib_State_t calib_state = TouchCalib_GetState();
				
				if (calib_state != CALIB_STATE_IDLE && calib_state != CALIB_STATE_DONE) {
					// Feed raw touch data to calibration process
					if (touch_current.pressed && !touch_prev.pressed) {
						bool done = TouchCalib_ProcessTouch(touch_current.raw_x, touch_current.raw_y);
						if (done) {
							HAL_Delay(1500);
							TouchCalib_Cancel();
							LCD_Clear(LCD_BLACK);
							TouchUI_Init();
						}
					}
				} else if (calib_state == CALIB_STATE_DONE) {
					// Touch to exit calibration "Complete" screen
					if (touch_current.pressed && !touch_prev.pressed) {
						TouchCalib_Cancel();
						LCD_Clear(LCD_BLACK);
						TouchUI_Init();
					}
				} else {
					// Normal UI mode: detect touch press (rising edge)
					if (touch_current.pressed && !touch_prev.pressed) {
						TouchUI_Event_t event = TouchUI_HandleTouch(touch_current.x, touch_current.y);
						
						// Handle button events
						float step = TouchUI_GetStepSize();
						switch (event) {
							case BTN_EVENT_J1_PLUS:
								RobotCtrl_MoveJointRelative(0, step, 30.0f, 50);
								break;
							case BTN_EVENT_J1_MINUS:
								RobotCtrl_MoveJointRelative(0, -step, 30.0f, 50);
								break;
							case BTN_EVENT_J2_PLUS:
								RobotCtrl_MoveJointRelative(1, step, 30.0f, 50);
								break;
							case BTN_EVENT_J2_MINUS:
								RobotCtrl_MoveJointRelative(1, -step, 30.0f, 50);
								break;
							case BTN_EVENT_J3_PLUS:
								RobotCtrl_MoveJointRelative(2, step, 30.0f, 50);
								break;
							case BTN_EVENT_J3_MINUS:
								RobotCtrl_MoveJointRelative(2, -step, 30.0f, 50);
								break;
							case BTN_EVENT_J4_PLUS:
								RobotCtrl_MoveJointRelative(3, step, 30.0f, 50);
								break;
							case BTN_EVENT_J4_MINUS:
								RobotCtrl_MoveJointRelative(3, -step, 30.0f, 50);
								break;
							case BTN_EVENT_J5_PLUS:
								RobotCtrl_MoveJointRelative(4, step, 30.0f, 50);
								break;
							case BTN_EVENT_J5_MINUS:
								RobotCtrl_MoveJointRelative(4, -step, 30.0f, 50);
								break;
							case BTN_EVENT_J6_PLUS:
								RobotCtrl_MoveJointRelative(5, step, 30.0f, 50);
								break;
							case BTN_EVENT_J6_MINUS:
								RobotCtrl_MoveJointRelative(5, -step, 30.0f, 50);
								break;
							case BTN_EVENT_ZERO:
								RobotCtrl_SetCurrentAsZero();
								TouchUI_UpdateStatus("Zero set", 0);
								break;
							case BTN_EVENT_STOP:
								RobotCtrl_StopAll();
								TouchUI_UpdateStatus("STOPPED", 1);
								break;
							case BTN_EVENT_STATUS:
								RobotCtrl_SyncAnglesFromMotors();
								TouchUI_UpdateStatus("Sync OK", 0);
								break;
							default:
								// Step size buttons are handled inside TouchUI_HandleTouch
								break;
						}
					}
				}
				touch_prev = touch_current;
			} else {
				touch_prev.pressed = false;
			}
		}
		
		// 2. Refresh LCD joint angles every 200 ms
		if (now - s_last_display_ms >= 200u) {
			s_last_display_ms = now;
			float display_angles[6];
			for (int _j = 0; _j < 6; _j++) {
				display_angles[_j] = RobotCtrl_GetJointAngle((uint8_t)_j);
			}
			TouchUI_UpdateJoints(display_angles);
		}
		
		HAL_Delay(10);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage 
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

	while(1);

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{ 
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
