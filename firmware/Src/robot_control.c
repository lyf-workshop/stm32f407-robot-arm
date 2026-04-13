/**
  ******************************************************************************
  * @file    robot_control.c
  * @brief   Robot arm control implementation (bare-metal version)
  ******************************************************************************
  */

#include "robot_control.h"
#include "Emm_V5.h"
#include "can.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

RobotArm_t g_robot_arm = {0};

void RobotCtrl_Init(void)
{
    for (int i = 0; i < ROBOT_MAX_JOINT_NUM; i++) {
        g_robot_arm.joints[i].current_angle = JOINT_CONFIG[i].init_angle;
        g_robot_arm.joints[i].pos_direction = JOINT_CONFIG[i].pos_direction;
        g_robot_arm.joints[i].reduction_ratio = JOINT_CONFIG[i].reduction_ratio;
        g_robot_arm.joints[i].min_angle = JOINT_CONFIG[i].min_angle;
        g_robot_arm.joints[i].max_angle = JOINT_CONFIG[i].max_angle;
        g_robot_arm.joints[i].reset_dir = JOINT_CONFIG[i].reset_dir;
        g_robot_arm.joints[i].velocity = 0.0f;
        
        RobotKin_UpdateJointAngle(i, JOINT_CONFIG[i].init_angle);
    }
    
    memcpy(g_robot_arm.T_current, T_0_6_RESET, sizeof(T_0_6_RESET));
    g_robot_arm.initialized = true;
}

int32_t RobotCtrl_AngleToPulse(float angle, uint8_t joint_id)
{
    if (joint_id >= ROBOT_MAX_JOINT_NUM) {
        return 0;
    }
    
    float ratio = g_robot_arm.joints[joint_id].reduction_ratio;
    int32_t pulses = (int32_t)((angle / 360.0f) * ratio * ROBOT_PULSES_PER_REV);
    return pulses;
}

float RobotCtrl_GetJointAngle(uint8_t joint_id)
{
    if (joint_id >= ROBOT_MAX_JOINT_NUM) {
        return 0.0f;
    }
    return g_robot_arm.joints[joint_id].current_angle;
}

int RobotCtrl_MoveJointRelative(uint8_t joint_id, float rel_angle, float velocity, uint8_t acceleration)
{
    if (joint_id >= ROBOT_MAX_JOINT_NUM) {
        return -1;
    }
    
    Joint_t *joint = &g_robot_arm.joints[joint_id];
    float target_angle = joint->current_angle + rel_angle;
    
    if (target_angle < joint->min_angle || target_angle > joint->max_angle) {
        return -1;
    }
    
    uint8_t motor_addr = joint_id + 1;
    uint8_t motor_dir = (rel_angle > 0) ? joint->pos_direction : !(joint->pos_direction);
    
    uint16_t motor_velocity = (uint16_t)(fabs(velocity) * joint->reduction_ratio * 600.0f / 360.0f);
    uint32_t pulses = (uint32_t)(fabs(rel_angle) / 360.0f * joint->reduction_ratio * ROBOT_PULSES_PER_REV);
    
    Emm_V5_Pos_Control(motor_addr, motor_dir, motor_velocity, acceleration, pulses, 0, 0);
    
    joint->current_angle = target_angle;
    RobotKin_UpdateJointAngle(joint_id, target_angle);
    
    return 0;
}

int RobotCtrl_MoveToJointAngles(const float target_angles[6], float velocity, uint8_t acceleration, bool sync)
{
    for (int i = 0; i < ROBOT_MAX_JOINT_NUM; i++) {
        Joint_t *joint = &g_robot_arm.joints[i];
        
        if (target_angles[i] < joint->min_angle || target_angles[i] > joint->max_angle) {
            return -1;
        }
    }
    
    for (int i = 0; i < ROBOT_MAX_JOINT_NUM; i++) {
        Joint_t *joint = &g_robot_arm.joints[i];
        float rel_angle = target_angles[i] - joint->current_angle;
        
        if (fabs(rel_angle) < ROBOT_JOINT_ANGLE_ERROR_RANGE) {
            continue;
        }
        
        uint8_t motor_addr = i + 1;
        uint8_t motor_dir;
        
        if (rel_angle > 0) {
            motor_dir = joint->pos_direction;
        } else {
            motor_dir = !(joint->pos_direction);
            rel_angle = -rel_angle;
        }
        
        uint16_t motor_velocity = (uint16_t)(velocity * joint->reduction_ratio * 600.0f / 360.0f);
        uint32_t pulses = (uint32_t)(rel_angle / 360.0f * joint->reduction_ratio * ROBOT_PULSES_PER_REV);
        
        Emm_V5_Pos_Control(motor_addr, motor_dir, motor_velocity, acceleration, pulses, 0, sync ? 1 : 0);
        HAL_Delay(ROBOT_CAN_DELAY);
        
        joint->current_angle = target_angles[i];
        RobotKin_UpdateJointAngle(i, target_angles[i]);
    }
    
    if (sync) {
        Emm_V5_Synchronous_motion(0);
        HAL_Delay(ROBOT_CAN_DELAY);
    }
    
    return 0;
}

int RobotCtrl_MoveToPose(float x, float y, float z, float velocity)
{
    float T_target[4][4];
    memcpy(T_target, g_robot_arm.T_current, sizeof(T_target));
    
    T_target[0][3] = x;
    T_target[1][3] = y;
    T_target[2][3] = z;
    
    float joint_angles[6];
    int ret = RobotKin_Inverse((float*)T_target, joint_angles, 0);
    
    if (ret != 0) {
        return -1;
    }
    
    ret = RobotCtrl_MoveToJointAngles(joint_angles, velocity, 200, true);
    
    if (ret == 0) {
        memcpy(g_robot_arm.T_current, T_target, sizeof(T_target));
    }
    
    return ret;
}

void RobotCtrl_SetCurrentAsZero(void)
{
    for (int i = 0; i < ROBOT_MAX_JOINT_NUM; i++) {
        uint8_t motor_addr = i + 1;
        Emm_V5_Reset_CurPos_To_Zero(motor_addr);
        HAL_Delay(ROBOT_CAN_DELAY);
    }
}

void RobotCtrl_StopAll(void)
{
    for (int i = 0; i < ROBOT_MAX_JOINT_NUM; i++) {
        uint8_t motor_addr = i + 1;
        Emm_V5_Stop_Now(motor_addr, 0);
        HAL_Delay(ROBOT_CAN_DELAY);
    }
}

int RobotCtrl_ReadMotorPulses(uint8_t joint_id, int32_t *out_pulses)
{
    if (joint_id >= ROBOT_MAX_JOINT_NUM || out_pulses == NULL) return -1;

    uint8_t motor_addr = joint_id + 1;

    /* Clear any pending CAN frame flag before issuing the query */
    can.rxFrameFlag = false;

    /* Request real-time position (S_CPOS = 0x36) */
    Emm_V5_Read_Sys_Params(motor_addr, S_CPOS);

    /* Wait for CAN reply, 100 ms timeout */
    uint32_t deadline = HAL_GetTick() + 100;
    while (!can.rxFrameFlag && HAL_GetTick() < deadline) { /* busy-wait */ }

    if (!can.rxFrameFlag) return -1;   /* timeout – motor not responding */

    /*
     * Emm V5 S_CPOS response frame layout (rxData):
     *   [0] = 0x36  (command echo)
     *   [1] = direction: 0=CW (+), 1=CCW (-)
     *   [2] = pulse bits [23:16]
     *   [3] = pulse bits [15:8]
     *   [4] = pulse bits [7:0]
     *   [5] = 0x6B  (fixed checksum)
     */
    if (can.rxData[0] != 0x36) return -1;  /* wrong reply */

    uint32_t raw = ((uint32_t)can.rxData[2] << 16) |
                   ((uint32_t)can.rxData[3] <<  8) |
                   ((uint32_t)can.rxData[4]);

    *out_pulses = (can.rxData[1] == 1) ? -(int32_t)raw : (int32_t)raw;
    return 0;
}

int RobotCtrl_SyncAnglesFromMotors(void)
{
    int ok = 0;
    for (int i = 0; i < ROBOT_MAX_JOINT_NUM; i++) {
        int32_t pulses = 0;
        if (RobotCtrl_ReadMotorPulses((uint8_t)i, &pulses) == 0) {
            float ratio = g_robot_arm.joints[i].reduction_ratio;
            /* Convert motor-shaft pulses back to joint angle:
             * angle = pulses / ROBOT_PULSES_PER_REV / gear_ratio * 360 */
            float angle = (float)pulses / (float)ROBOT_PULSES_PER_REV
                          / ratio * 360.0f;
            g_robot_arm.joints[i].current_angle = angle;
            RobotKin_UpdateJointAngle((uint8_t)i, angle);
            ok++;
        }
        HAL_Delay(ROBOT_CAN_DELAY);
    }
    return ok;
}
