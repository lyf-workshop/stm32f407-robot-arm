/**
  ******************************************************************************
  * @file    robot_cmd.c
  * @brief   Robot UART command parser implementation
  ******************************************************************************
  */

#include "robot_cmd.h"
#include "robot_control.h"
#include "robot_sequence.h"
#include "touch_ui.h"
#include "touch_calib.h"
#include "touch.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char g_cmd_buffer[ROBOT_CMD_BUF_SIZE];
static uint16_t g_cmd_index = 0;

void RobotCmd_Init(void)
{
    memset(g_cmd_buffer, 0, sizeof(g_cmd_buffer));
    g_cmd_index = 0;
    RobotCmd_SendResponse("\r\n=== Robot Arm Control System ===\r\n");
    RobotCmd_SendResponse("Commands:\r\n");
    RobotCmd_SendResponse("  rel_rotate <joint_id> <angle>   - Relative rotation\r\n");
    RobotCmd_SendResponse("  abs_rotate <joint_id> <angle>   - Absolute rotation\r\n");
    RobotCmd_SendResponse("  sync <a1> <a2> <a3> <a4> <a5> <a6> - Sync all joints\r\n");
    RobotCmd_SendResponse("  auto <x> <y> <z>                - Cartesian move (IK)\r\n");
    RobotCmd_SendResponse("  zero                            - Set current as zero\r\n");
    RobotCmd_SendResponse("  stop                            - Emergency stop\r\n");
    RobotCmd_SendResponse("  status                          - Show joint angles (software)\r\n");
    RobotCmd_SendResponse("  sync_pos                        - Read actual angles from encoders\r\n");
    RobotCmd_SendResponse("  calibrate <joint_id>            - Compare SW vs encoder angle\r\n");
    RobotCmd_SendResponse("  touch_calib                     - Start touch screen calibration\r\n");
    RobotCmd_SendResponse("  touch_test                      - Print raw ADC values (10 samples)\r\n");
    RobotCmd_SendResponse("  rl_step <d1> <d2> <d3> <d4> <d5> <d6> - RL action step\r\n");
    RobotCmd_SendResponse("  demo_joint                      - Run joint space demo\r\n");
    RobotCmd_SendResponse("  demo_pick                       - Run pick-and-place demo\r\n");
    RobotCmd_SendResponse("  demo_circle                     - Run circular motion demo\r\n");
    RobotCmd_SendResponse("Ready>\r\n");
}

void RobotCmd_SendResponse(const char *msg)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);

    /* Mirror status/result lines to the LCD display */
    if (strncmp(msg, "OK", 2) == 0) {
        /* Show abbreviated OK (strip trailing \r\n) */
        char sts[40];
        uint8_t i = 0;
        while (msg[i] && msg[i] != '\r' && msg[i] != '\n' && i < 39) {
            sts[i] = msg[i]; i++;
        }
        sts[i] = '\0';
        TouchUI_UpdateStatus(sts, 0);
        TouchUI_SetMode(RDISP_MODE_IDLE);
    } else if (strncmp(msg, "Error", 5) == 0) {
        char sts[40];
        uint8_t i = 0;
        while (msg[i] && msg[i] != '\r' && msg[i] != '\n' && i < 39) {
            sts[i] = msg[i]; i++;
        }
        sts[i] = '\0';
        TouchUI_UpdateStatus(sts, 1);
        TouchUI_SetMode(RDISP_MODE_ERROR);
    }
}

static void parse_rel_rotate(char *args)
{
    int joint_id;
    float angle;
    
    if (sscanf(args, "%d %f", &joint_id, &angle) == 2) {
        if (joint_id < 0 || joint_id >= ROBOT_MAX_JOINT_NUM) {
            RobotCmd_SendResponse("Error: Invalid joint_id\r\n");
            return;
        }
        
        int ret = RobotCtrl_MoveJointRelative(joint_id, angle, 10.0f, 200);
        if (ret == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "OK: J%d moved by %.2f deg\r\n", joint_id, angle);
            RobotCmd_SendResponse(buf);
        } else {
            RobotCmd_SendResponse("Error: Move failed\r\n");
        }
    } else {
        RobotCmd_SendResponse("Error: Usage: rel_rotate <joint_id> <angle>\r\n");
    }
}

static void parse_abs_rotate(char *args)
{
    int joint_id;
    float angle;
    
    if (sscanf(args, "%d %f", &joint_id, &angle) == 2) {
        if (joint_id < 0 || joint_id >= ROBOT_MAX_JOINT_NUM) {
            RobotCmd_SendResponse("Error: Invalid joint_id\r\n");
            return;
        }
        
        float angles[6];
        for (int i = 0; i < 6; i++) {
            angles[i] = RobotCtrl_GetJointAngle(i);
        }
        angles[joint_id] = angle;
        
        int ret = RobotCtrl_MoveToJointAngles(angles, 10.0f, 200, false);
        if (ret == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "OK: J%d moved to %.2f deg\r\n", joint_id, angle);
            RobotCmd_SendResponse(buf);
        } else {
            RobotCmd_SendResponse("Error: Move failed\r\n");
        }
    } else {
        RobotCmd_SendResponse("Error: Usage: abs_rotate <joint_id> <angle>\r\n");
    }
}

static void parse_sync(char *args)
{
    float angles[6];
    
    if (sscanf(args, "%f %f %f %f %f %f", 
               &angles[0], &angles[1], &angles[2], &angles[3], &angles[4], &angles[5]) == 6) {
        
        int ret = RobotCtrl_MoveToJointAngles(angles, 10.0f, 200, true);
        if (ret == 0) {
            RobotCmd_SendResponse("OK: All joints synchronized\r\n");
        } else {
            RobotCmd_SendResponse("Error: Sync failed\r\n");
        }
    } else {
        RobotCmd_SendResponse("Error: Usage: sync <a1> <a2> <a3> <a4> <a5> <a6>\r\n");
    }
}

static void parse_auto(char *args)
{
    float x, y, z;
    
    if (sscanf(args, "%f %f %f", &x, &y, &z) == 3) {
        int ret = RobotCtrl_MoveToPose(x, y, z, 10.0f);
        if (ret == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "OK: Moved to (%.1f, %.1f, %.1f)\r\n", x, y, z);
            RobotCmd_SendResponse(buf);
        } else {
            RobotCmd_SendResponse("Error: IK failed or move out of range\r\n");
        }
    } else {
        RobotCmd_SendResponse("Error: Usage: auto <x> <y> <z>\r\n");
    }
}

static void parse_rl_step(char *args)
{
    float delta_angles[6];
    
    if (sscanf(args, "%f %f %f %f %f %f", 
               &delta_angles[0], &delta_angles[1], &delta_angles[2], 
               &delta_angles[3], &delta_angles[4], &delta_angles[5]) == 6) {
        
        float target_angles[6];
        for (int i = 0; i < 6; i++) {
            target_angles[i] = RobotCtrl_GetJointAngle(i) + delta_angles[i];
        }
        
        int ret = RobotCtrl_MoveToJointAngles(target_angles, 20.0f, 200, true);
        
        char state_buf[128];
        snprintf(state_buf, sizeof(state_buf), 
                 "state %.2f %.2f %.2f %.2f %.2f %.2f\r\n",
                 RobotCtrl_GetJointAngle(0), RobotCtrl_GetJointAngle(1),
                 RobotCtrl_GetJointAngle(2), RobotCtrl_GetJointAngle(3),
                 RobotCtrl_GetJointAngle(4), RobotCtrl_GetJointAngle(5));
        RobotCmd_SendResponse(state_buf);
        
        if (ret != 0) {
            RobotCmd_SendResponse("warn: joint_limit\r\n");
        }
    } else {
        RobotCmd_SendResponse("Error: Usage: rl_step <d1> <d2> <d3> <d4> <d5> <d6>\r\n");
    }
}

static void parse_status(void)
{
    char buf[128];
    snprintf(buf, sizeof(buf), 
             "Joints: [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f] deg\r\n",
             RobotCtrl_GetJointAngle(0), RobotCtrl_GetJointAngle(1),
             RobotCtrl_GetJointAngle(2), RobotCtrl_GetJointAngle(3),
             RobotCtrl_GetJointAngle(4), RobotCtrl_GetJointAngle(5));
    RobotCmd_SendResponse(buf);
}

/* Read actual angles from motor encoders and update software state */
static void parse_sync_pos(void)
{
    RobotCmd_SendResponse("Reading motor positions...\r\n");
    int ok = RobotCtrl_SyncAnglesFromMotors();
    if (ok == ROBOT_MAX_JOINT_NUM) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "OK: Synced [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f] deg\r\n",
                 RobotCtrl_GetJointAngle(0), RobotCtrl_GetJointAngle(1),
                 RobotCtrl_GetJointAngle(2), RobotCtrl_GetJointAngle(3),
                 RobotCtrl_GetJointAngle(4), RobotCtrl_GetJointAngle(5));
        RobotCmd_SendResponse(buf);
    } else {
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "Error: Only %d/6 motors responded\r\n", ok);
        RobotCmd_SendResponse(buf);
    }
}

/* Single-axis pulse readback for calibration: calibrate <joint_id> */
static void parse_calibrate(char *args)
{
    int joint_id = -1;
    if (args) { joint_id = atoi(args); }
    if (joint_id < 0 || joint_id >= ROBOT_MAX_JOINT_NUM) {
        RobotCmd_SendResponse("Error: Usage: calibrate <joint_id 0-5>\r\n");
        return;
    }
    int32_t pulses = 0;
    if (RobotCtrl_ReadMotorPulses((uint8_t)joint_id, &pulses) != 0) {
        RobotCmd_SendResponse("Error: Motor not responding\r\n");
        return;
    }
    float ratio = JOINT_CONFIG[joint_id].reduction_ratio;
    float sw_angle  = RobotCtrl_GetJointAngle((uint8_t)joint_id);
    float enc_angle = (float)pulses / (float)ROBOT_PULSES_PER_REV / ratio * 360.0f;
    char buf[128];
    snprintf(buf, sizeof(buf),
             "OK: J%d  SW:%.2f deg  Encoder:%.2f deg  Pulses:%ld\r\n",
             joint_id, sw_angle, enc_angle, (long)pulses);
    RobotCmd_SendResponse(buf);
}

static void parse_zero(void)
{
    RobotCtrl_SetCurrentAsZero();
    RobotCmd_SendResponse("OK: Current position set as zero\r\n");
}

static void parse_stop(void)
{
    RobotCtrl_StopAll();
    RobotCmd_SendResponse("OK: All motors stopped\r\n");
}

static void execute_command(char *cmd_line)
{
    TouchUI_SetMode(RDISP_MODE_RUNNING);

    char *cmd = strtok(cmd_line, " \r\n");
    char *args = strtok(NULL, "\r\n");
    
    if (cmd == NULL) {
        return;
    }
    
    if (strcmp(cmd, "rel_rotate") == 0) {
        parse_rel_rotate(args);
    } else if (strcmp(cmd, "abs_rotate") == 0) {
        parse_abs_rotate(args);
    } else if (strcmp(cmd, "sync") == 0) {
        parse_sync(args);
    } else if (strcmp(cmd, "auto") == 0) {
        parse_auto(args);
    } else if (strcmp(cmd, "rl_step") == 0) {
        parse_rl_step(args);
    } else if (strcmp(cmd, "status") == 0) {
        parse_status();
    } else if (strcmp(cmd, "sync_pos") == 0) {
        parse_sync_pos();
    } else if (strcmp(cmd, "calibrate") == 0) {
        parse_calibrate(args);
    } else if (strcmp(cmd, "zero") == 0) {
        parse_zero();
    } else if (strcmp(cmd, "stop") == 0) {
        parse_stop();
    } else if (strcmp(cmd, "demo_joint") == 0) {
        RobotCmd_SendResponse("Running joint space demo...\r\n");
        int ret = RobotSeq_JointDemo();
        if (ret == 0) {
            RobotCmd_SendResponse("Demo completed\r\n");
        } else {
            RobotCmd_SendResponse("Error: Demo failed\r\n");
        }
    } else if (strcmp(cmd, "demo_pick") == 0) {
        RobotCmd_SendResponse("Running pick-and-place demo...\r\n");
        int ret = RobotSeq_PickAndPlace();
        if (ret == 0) {
            RobotCmd_SendResponse("Demo completed\r\n");
        } else {
            RobotCmd_SendResponse("Error: Demo failed\r\n");
        }
    } else if (strcmp(cmd, "demo_circle") == 0) {
        RobotCmd_SendResponse("Running circular motion demo...\r\n");
        int ret = RobotSeq_CircleDemo();
        if (ret == 0) {
            RobotCmd_SendResponse("Demo completed\r\n");
        } else {
            RobotCmd_SendResponse("Error: Demo failed\r\n");
        }
    } else if (strcmp(cmd, "touch_calib") == 0) {
        RobotCmd_SendResponse("Starting touch calibration...\r\n");
        RobotCmd_SendResponse("Follow on-screen instructions\r\n");
        TouchCalib_Start();
    } else if (strcmp(cmd, "touch_test") == 0) {
        /* Print 10 raw ADC samples to diagnose touch hardware */
        RobotCmd_SendResponse("Touch test: press and release screen 10 times\r\n");
        RobotCmd_SendResponse("Format: raw_X  raw_Y\r\n");
        RobotCmd_SendResponse("Valid range: 80~4010 (0 or 4095 = not touched)\r\n");
        char buf[64];
        for (int i = 0; i < 10; i++) {
            HAL_Delay(500);
            uint16_t rx, ry;
            Touch_ReadRaw(&rx, &ry);
            /* Format numbers manually (no printf) */
            const char *hex = "0123456789";
            buf[0] = 'X'; buf[1] = ':';
            buf[2] = hex[(rx / 1000) % 10];
            buf[3] = hex[(rx / 100)  % 10];
            buf[4] = hex[(rx / 10)   % 10];
            buf[5] = hex[ rx         % 10];
            buf[6] = ' '; buf[7] = ' ';
            buf[8] = 'Y'; buf[9] = ':';
            buf[10] = hex[(ry / 1000) % 10];
            buf[11] = hex[(ry / 100)  % 10];
            buf[12] = hex[(ry / 10)   % 10];
            buf[13] = hex[ ry         % 10];
            buf[14] = '\r'; buf[15] = '\n'; buf[16] = '\0';
            HAL_UART_Transmit(&huart1, (uint8_t*)buf, 16, 50);
        }
        RobotCmd_SendResponse("OK: touch_test done\r\n");
    } else {
        RobotCmd_SendResponse("Error: Unknown command\r\n");
    }
}

void RobotCmd_ReceiveByte(uint8_t data)
{
    if (data == '\n' || data == '\r') {
        if (g_cmd_index > 0) {
            g_cmd_buffer[g_cmd_index] = '\0';
            execute_command(g_cmd_buffer);
            g_cmd_index = 0;
            RobotCmd_SendResponse("Ready>\r\n");
        }
    } else if (data == '\b' || data == 127) {
        if (g_cmd_index > 0) {
            g_cmd_index--;
        }
    } else if (g_cmd_index < ROBOT_CMD_BUF_SIZE - 1) {
        g_cmd_buffer[g_cmd_index++] = data;
    }
}

void RobotCmd_Process(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        RobotCmd_ReceiveByte(data[i]);
    }
}
