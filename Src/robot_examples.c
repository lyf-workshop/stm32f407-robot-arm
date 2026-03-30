/**
  ******************************************************************************
  * @file    robot_examples.c
  * @brief   Robot arm usage examples (can be called from main.c)
  ******************************************************************************
  */

#include "robot_control.h"
#include "robot_sequence.h"
#include "robot_cmd.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * Example 1: Simple joint rotation test
 * 简单的关节旋转测试
 */
void Example_SingleJointTest(void)
{
    RobotCmd_SendResponse("\n=== Example 1: Single Joint Test ===\n");
    
    // 关节 0 顺时针转 10 度
    RobotCtrl_MoveJointRelative(0, 10.0f, 10.0f, 200);
    HAL_Delay(2000);
    
    // 关节 0 逆时针转 10 度（回到原位）
    RobotCtrl_MoveJointRelative(0, -10.0f, 10.0f, 200);
    HAL_Delay(2000);
    
    RobotCmd_SendResponse("Test completed\n");
}

/**
 * Example 2: Multi-axis synchronized motion
 * 多轴同步运动测试
 */
void Example_SyncMotion(void)
{
    RobotCmd_SendResponse("\n=== Example 2: Synchronized Motion ===\n");
    
    float target_angles[6] = {90, 120, 30, 0, 60, 0};
    
    // 所有轴同步运动到目标角度
    RobotCtrl_MoveToJointAngles(target_angles, 10.0f, 200, true);
    HAL_Delay(3000);
    
    // 回到初始位置
    float init_angles[6] = {90, 90, 0, 0, 90, 0};
    RobotCtrl_MoveToJointAngles(init_angles, 10.0f, 200, true);
    HAL_Delay(3000);
    
    RobotCmd_SendResponse("Test completed\n");
}

/**
 * Example 3: Cartesian space control (IK)
 * 笛卡尔空间控制（逆运动学）
 */
void Example_CartesianControl(void)
{
    RobotCmd_SendResponse("\n=== Example 3: Cartesian Control ===\n");
    
    // 注意：执行前必须先归零标定（调用 RobotCtrl_SetCurrentAsZero）
    
    // 移动到正前方某点
    RobotCtrl_MoveToPose(0, -184.5f, 215.5f, 10.0f);
    HAL_Delay(3000);
    
    // 移动到侧方
    RobotCtrl_MoveToPose(100, -150.0f, 200.0f, 10.0f);
    HAL_Delay(3000);
    
    // 回到初始位置
    RobotCtrl_MoveToPose(0, -184.5f, 215.5f, 10.0f);
    HAL_Delay(3000);
    
    RobotCmd_SendResponse("Test completed\n");
}

/**
 * Example 4: Pre-defined sequence execution
 * 预定义动作序列执行
 */
void Example_SequenceExecution(void)
{
    RobotCmd_SendResponse("\n=== Example 4: Sequence Execution ===\n");
    
    // 执行取放物演示序列
    int ret = RobotSeq_PickAndPlace();
    
    if (ret == 0) {
        RobotCmd_SendResponse("Pick-and-place sequence completed\n");
    } else {
        RobotCmd_SendResponse("Sequence failed\n");
    }
}

/**
 * Example 5: RL action execution simulation
 * 强化学习动作执行模拟
 */
void Example_RLActionSimulation(void)
{
    RobotCmd_SendResponse("\n=== Example 5: RL Action Simulation ===\n");
    
    // 模拟强化学习智能体的 10 步探索
    for (int step = 0; step < 10; step++) {
        // 生成随机动作（实际应用中由 RL 模型生成）
        float action[6];
        for (int i = 0; i < 6; i++) {
            action[i] = RobotCtrl_GetJointAngle(i) + ((float)(rand() % 5) - 2.0f);
        }
        
        // 执行动作
        int ret = RobotCtrl_MoveToJointAngles(action, 15.0f, 200, true);
        
        // 获取新状态
        char state_buf[128];
        snprintf(state_buf, sizeof(state_buf), 
                 "Step %d: [%.1f, %.1f, %.1f, %.1f, %.1f, %.1f]\n",
                 step,
                 RobotCtrl_GetJointAngle(0), RobotCtrl_GetJointAngle(1),
                 RobotCtrl_GetJointAngle(2), RobotCtrl_GetJointAngle(3),
                 RobotCtrl_GetJointAngle(4), RobotCtrl_GetJointAngle(5));
        RobotCmd_SendResponse(state_buf);
        
        if (ret != 0) {
            RobotCmd_SendResponse("Hit joint limit!\n");
            break;
        }
        
        HAL_Delay(500);
    }
    
    RobotCmd_SendResponse("RL simulation completed\n");
}

/**
 * 如何在 main.c 中使用这些示例：
 * 
 * 方式 1：在 main 函数初始化完成后直接调用
 * 
 * int main(void) {
 *     // ... 初始化代码 ...
 *     RobotCtrl_Init();
 *     RobotCmd_Init();
 *     
 *     // 自动执行演示序列
 *     HAL_Delay(1000);
 *     Example_SingleJointTest();
 *     
 *     // 然后进入串口命令循环
 *     USART1_StartReceive();
 *     while(1) { ... }
 * }
 * 
 * 方式 2：通过串口命令触发
 * 
 * 在 robot_cmd.c 的 execute_command 函数中添加：
 * else if (strcmp(cmd, "example1") == 0) {
 *     Example_SingleJointTest();
 * }
 */
