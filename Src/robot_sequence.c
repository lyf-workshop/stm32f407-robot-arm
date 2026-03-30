/**
  ******************************************************************************
  * @file    robot_sequence.c
  * @brief   Pre-defined motion sequences implementation
  ******************************************************************************
  */

#include "robot_sequence.h"
#include "robot_control.h"
#include "main.h"

static const WaypointCartesian_t g_pick_place_sequence[] = {
    {0,    -184.5, 215.5, 0.3f},  // Home position (above)
    {100,  -150,   200,   0.5f},  // Move to above target
    {100,  -150,   80,    0.2f},  // Lower to pick
    {100,  -150,   200,   0.3f},  // Lift up
    {-100, -150,   200,   0.5f},  // Move to place position above
    {-100, -150,   80,    0.2f},  // Lower to place
    {-100, -150,   200,   0.3f},  // Lift up
    {0,    -184.5, 215.5, 0.5f},  // Return home
};

static const WaypointJoint_t g_joint_demo_sequence[] = {
    {{90, 90,  0,  0, 90, 0},   0.3f},  // Home
    {{45, 120, 30, 0, 60, 0},   0.5f},  // Position 1
    {{135,90, -30, 0, 90, 90},  0.5f},  // Position 2
    {{90, 90,  0,  0, 90, 180}, 0.5f},  // Position 3
    {{90, 90,  0,  0, 90, 0},   0.5f},  // Return home
};

int RobotSeq_PickAndPlace(void)
{
    int seq_len = sizeof(g_pick_place_sequence) / sizeof(WaypointCartesian_t);
    
    for (int i = 0; i < seq_len; i++) {
        WaypointCartesian_t wp = g_pick_place_sequence[i];
        
        float velocity = 10.0f * wp.velocity_scale;
        int ret = RobotCtrl_MoveToPose(wp.x, wp.y, wp.z, velocity);
        
        if (ret != 0) {
            return -1;
        }
        
        HAL_Delay(500);
    }
    
    return 0;
}

int RobotSeq_JointDemo(void)
{
    int seq_len = sizeof(g_joint_demo_sequence) / sizeof(WaypointJoint_t);
    
    for (int i = 0; i < seq_len; i++) {
        WaypointJoint_t wp = g_joint_demo_sequence[i];
        
        float velocity = 10.0f * wp.velocity_scale;
        int ret = RobotCtrl_MoveToJointAngles(wp.joints, velocity, 200, true);
        
        if (ret != 0) {
            return -1;
        }
        
        HAL_Delay(1000);
    }
    
    return 0;
}

int RobotSeq_CircleDemo(void)
{
    const float center_x = 50.0f;
    const float center_y = -184.5f;
    const float center_z = 200.0f;
    const float radius = 30.0f;
    const int num_points = 12;
    
    for (int i = 0; i <= num_points; i++) {
        float angle = 2.0f * M_PI * i / num_points;
        float x = center_x + radius * cos(angle);
        float y = center_y;
        float z = center_z + radius * sin(angle);
        
        int ret = RobotCtrl_MoveToPose(x, y, z, 8.0f);
        if (ret != 0) {
            return -1;
        }
        
        HAL_Delay(300);
    }
    
    return 0;
}
