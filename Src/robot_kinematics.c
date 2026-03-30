/**
  ******************************************************************************
  * @file    robot_kinematics.c
  * @brief   Robot inverse kinematics implementation
  *          Based on zero-robotic-arm project by dearxie
  *          Uses Pieper analytical method for 6-DOF arm
  ******************************************************************************
  */

#include "robot_kinematics.h"
#include "robot_config.h"
#include <string.h>
#include <stdio.h>

static RobotKinematics_t g_robot_kinematics = {0};
static float g_current_joint_angle[ROBOT_MAX_JOINT_NUM] = {0};

#define a2 D_H[2][0]
#define a3 D_H[3][0]
#define d4 D_H[3][2]

static void robot_kinematics_calc_theta3(void)
{
    float px = g_robot_kinematics.T[0][3];
    float py = g_robot_kinematics.T[1][3];
    float pz = g_robot_kinematics.T[2][3];

    float _2_a2_d4 = 2 * a2 * d4;
    float _2_pow_a2_2 = 2 * pow(a2, 2);
    float _2_pow_a3_2 = 2 * pow(a3, 2);
    float _2_pow_d4_2 = 2 * pow(d4, 2);
    float const_eq1 = -pow(a2, 4) + _2_pow_a2_2 * (pow(a3, 2) + pow(d4, 2))
        - pow(a3, 4) - 2*pow(a3, 2)*pow(d4, 2) - pow(d4, 4);
    float const_eq2 = -pow(a2, 2) + 2*a2*a3 - pow(a3, 2) - pow(d4, 2);
    
    float pow_px_2 = pow(px, 2);
    float pow_py_2 = pow(py, 2);
    float pow_pz_2 = pow(pz, 2);
    float pow_distance_2 = pow_px_2 + pow_py_2 + pow_pz_2;

    float eq1 = (const_eq1 + _2_pow_a2_2*pow_distance_2 
        + _2_pow_a3_2*pow_distance_2 + _2_pow_d4_2*pow_distance_2 
        - pow(px, 4) - pow(py, 4) - pow(pz, 4) 
        - 2*pow_px_2*(pow_py_2 + pow_pz_2) - 2*pow_py_2*pow_pz_2);

    float u_theta3_1 = -(_2_a2_d4 + sqrt(eq1)) / (const_eq2 + pow_distance_2);
    float u_theta3_2 = -(_2_a2_d4 - sqrt(eq1)) / (const_eq2 + pow_distance_2);

    float theta3_1 = atan(u_theta3_1) * 2;
    float theta3_2 = atan(u_theta3_2) * 2;

    for (int i = 0; i < ROBOT_KINEMATICS_RESULT_NUM/2; i++) {
        g_robot_kinematics.result[i][ROBOT_JOINT_3] = theta3_1;
    }

    for (int i = ROBOT_KINEMATICS_RESULT_NUM/2; i < ROBOT_KINEMATICS_RESULT_NUM; i++) {
        g_robot_kinematics.result[i][ROBOT_JOINT_3] = theta3_2;
    }
}

static void __robot_kinematics_calc_theta2(float theta3, float *theta2_1, float *theta2_2)
{
    float pz = g_robot_kinematics.T[2][3];

    float _pow_a2_2 = pow(a2, 2);
    float _pow_a3_2 = pow(a3, 2);
    float _pow_d4_2 = pow(d4, 2);
    float _2_a2_a3 = 2 * a2 * a3;
    float _2_a2_d4 = 2 * a2 * d4;
    
    float cos_theta3 = cos(theta3);
    float sin_theta3 = sin(theta3);

    float const_eq1 = _pow_a2_2 + _pow_a3_2 + _pow_d4_2;
    float eq1 = sqrt(const_eq1 + _2_a2_a3*cos_theta3 - _2_a2_d4*sin_theta3 - pow(pz, 2));
    float eq2 = a3*cos_theta3 - d4*sin_theta3;
    float eq3 = (d4*cos_theta3 - pz + a3*sin_theta3);

    float u_theta2_1 = -(a2 + eq1 + eq2) / eq3;
    float u_theta2_2 = -(a2 - eq1 + eq2) / eq3;

    *theta2_1 = atan(u_theta2_1) * 2;
    *theta2_2 = atan(u_theta2_2) * 2;
}

static void robot_kinematics_calc_theta2(void)
{
    float theta3 = 0;
    float theta2_1 = 0;
    float theta2_2 = 0;

    theta3 = g_robot_kinematics.result[0][ROBOT_JOINT_3];
    __robot_kinematics_calc_theta2(theta3, &theta2_1, &theta2_2);
    g_robot_kinematics.result[0][ROBOT_JOINT_2] = theta2_1;
    g_robot_kinematics.result[1][ROBOT_JOINT_2] = theta2_2;

    theta3 = g_robot_kinematics.result[2][ROBOT_JOINT_3];
    __robot_kinematics_calc_theta2(theta3, &theta2_1, &theta2_2);
    g_robot_kinematics.result[2][ROBOT_JOINT_2] = theta2_1;
    g_robot_kinematics.result[3][ROBOT_JOINT_2] = theta2_2;
}

static float __robot_kinematics_calc_theta1(float theta2, float theta3)
{ 
    float px = g_robot_kinematics.T[0][3];
    float py = g_robot_kinematics.T[1][3];

    float diff_theta2_3 = theta2 - theta3;
    float cos_diff_theta2_3 = cos(diff_theta2_3);
    float sin_diff_theta2_3 = sin(diff_theta2_3);
    float cos_theta2 = cos(theta2);
    float sin_theta2 = sin(theta2);
    float cos_theta3 = cos(theta3);
    float sin_theta3 = sin(theta3);

    float eq1 = a2*cos_theta2 + a3*cos_diff_theta2_3 + d4*sin_diff_theta2_3;
    float u_theta1 = sqrt((-px + eq1)/(px + eq1));

    float eq2 = (2*u_theta1*(cos_theta2*(a2 + a3*cos_theta3 - d4*sin_theta3) + sin_theta2*(d4*cos_theta3 + a3*sin_theta3))) / (pow(u_theta1, 2) + 1);
    
    if (fabs(py - eq2) > ROBOT_ERROR_RANGE) {
        u_theta1 = -u_theta1;
    }

    float theta1 = atan(u_theta1) * 2;
    return theta1;
}

static void robot_kinematics_calc_theta1(void)
{
    for (int i = 0; i < ROBOT_KINEMATICS_RESULT_NUM; i++) {
        float theta2 = g_robot_kinematics.result[i][ROBOT_JOINT_2];
        float theta3 = g_robot_kinematics.result[i][ROBOT_JOINT_3];
        g_robot_kinematics.result[i][ROBOT_JOINT_1] = __robot_kinematics_calc_theta1(theta2, theta3);
    }
}

static float __robot_kinematics_calc_theta5(float theta1, float theta2, float theta3)
{
    float nx = g_robot_kinematics.T[0][0];
    float ny = g_robot_kinematics.T[1][0];
    float nz = g_robot_kinematics.T[2][0];

    float ox = g_robot_kinematics.T[0][1];
    float oy = g_robot_kinematics.T[1][1];
    float oz = g_robot_kinematics.T[2][1];

    float ax = g_robot_kinematics.T[0][2];
    float ay = g_robot_kinematics.T[1][2];
    float az = g_robot_kinematics.T[2][2];

    float cos_theta1 = cos(theta1);
    float sin_theta1 = sin(theta1);
    float cos_theta2 = cos(theta2);
    float sin_theta2 = sin(theta2);
    float cos_theta3 = cos(theta3);
    float sin_theta3 = sin(theta3);

    float r31 = nx*cos_theta1*cos_theta3*sin_theta2 - nz*sin_theta2*sin_theta3 - nx*cos_theta1*cos_theta2*sin_theta3 - nz*cos_theta2*cos_theta3 - ny*cos_theta2*sin_theta1*sin_theta3 + ny*cos_theta3*sin_theta1*sin_theta2;
    float r32 = ox*cos_theta1*cos_theta3*sin_theta2 - oz*sin_theta2*sin_theta3 - ox*cos_theta1*cos_theta2*sin_theta3 - oz*cos_theta2*cos_theta3 - oy*cos_theta2*sin_theta1*sin_theta3 + oy*cos_theta3*sin_theta1*sin_theta2;
    float r33 = ax*cos_theta1*cos_theta3*sin_theta2 - az*sin_theta2*sin_theta3 - ax*cos_theta1*cos_theta2*sin_theta3 - az*cos_theta2*cos_theta3 - ay*cos_theta2*sin_theta1*sin_theta3 + ay*cos_theta3*sin_theta1*sin_theta2;
    
    float theta5_zyz = atan2(sqrt(pow(r31, 2) + pow(r32, 2)), r33);
    float theta5 = -theta5_zyz + M_PI;
    return theta5;
}

static void robot_kinematics_calc_theta5(void)
{
    for (int i = 0; i < ROBOT_KINEMATICS_RESULT_NUM; i++) {
        float theta2 = g_robot_kinematics.result[i][ROBOT_JOINT_2];
        float theta3 = g_robot_kinematics.result[i][ROBOT_JOINT_3];
        float theta1 = g_robot_kinematics.result[i][ROBOT_JOINT_1];
        g_robot_kinematics.result[i][ROBOT_JOINT_5] = __robot_kinematics_calc_theta5(theta1, theta2, theta3);
    }
}

static float __robot_kinematics_calc_theta4(float theta1, float theta2, float theta3, float theta5)
{
    float theta5_zyz = M_PI - theta5;
    if ((fabs(theta5_zyz) < ROBOT_ERROR_RANGE) || (fabs(theta5_zyz - M_PI) < ROBOT_ERROR_RANGE)) {
        return 0;
    }
    
    float ax = g_robot_kinematics.T[0][2];
    float ay = g_robot_kinematics.T[1][2];
    float az = g_robot_kinematics.T[2][2];

    float theta4 = 0;
    float cos_theta1 = cos(theta1);
    float sin_theta1 = sin(theta1);
    float cos_theta2 = cos(theta2);
    float sin_theta2 = sin(theta2);
    float cos_theta3 = cos(theta3);
    float sin_theta3 = sin(theta3);
    float cos_theta4 = cos(theta4);
    float sin_theta4 = sin(theta4);

    float r23 = ax*cos_theta4*sin_theta1 - ay*cos_theta1*cos_theta4 + az*cos_theta2*sin_theta3*sin_theta4 
        - az*cos_theta3*sin_theta2*sin_theta4 - ax*cos_theta1*cos_theta2*cos_theta3*sin_theta4 - ay*cos_theta2*cos_theta3*sin_theta1*sin_theta4 
        - ax*cos_theta1*sin_theta2*sin_theta3*sin_theta4 - ay*sin_theta1*sin_theta2*sin_theta3*sin_theta4;
    
    float r13 = ax*sin_theta1*sin_theta4 - ay*cos_theta1*sin_theta4 
        - az*cos_theta2*cos_theta4*sin_theta3 + az*cos_theta3*cos_theta4*sin_theta2 
        + ax*cos_theta1*cos_theta2*cos_theta3*cos_theta4 + ay*cos_theta2*cos_theta3*cos_theta4*sin_theta1 
        + ax*cos_theta1*cos_theta4*sin_theta2*sin_theta3 + ay*cos_theta4*sin_theta1*sin_theta2*sin_theta3;

    theta4 = atan2(r23, r13);
    return theta4;
}

static void robot_kinematics_calc_theta4(void)
{
    for (int i = 0; i < ROBOT_KINEMATICS_RESULT_NUM; i++) {
        float theta2 = g_robot_kinematics.result[i][ROBOT_JOINT_2];
        float theta3 = g_robot_kinematics.result[i][ROBOT_JOINT_3];
        float theta1 = g_robot_kinematics.result[i][ROBOT_JOINT_1];
        float theta5 = g_robot_kinematics.result[i][ROBOT_JOINT_5];
        g_robot_kinematics.result[i][ROBOT_JOINT_4] = __robot_kinematics_calc_theta4(theta1, theta2, theta3, theta5);
    }
}

static float __robot_kinematics_calc_theta6(float theta1, float theta2, float theta3, float theta4, float theta5)
{
    float theta5_zyz = M_PI - theta5;
    
    float nx = g_robot_kinematics.T[0][0];
    float ny = g_robot_kinematics.T[1][0];
    float nz = g_robot_kinematics.T[2][0];

    float ox = g_robot_kinematics.T[0][1];
    float oy = g_robot_kinematics.T[1][1];
    float oz = g_robot_kinematics.T[2][1];

    float cos_theta1 = cos(theta1);
    float sin_theta1 = sin(theta1);
    float cos_theta2 = cos(theta2);
    float sin_theta2 = sin(theta2);
    float cos_theta3 = cos(theta3);
    float sin_theta3 = sin(theta3);
    
    float theta6_zyz = 0;
    float theta6 = 0;
    
    if ((fabs(theta5_zyz) < ROBOT_ERROR_RANGE) || (fabs(theta5_zyz - M_PI) < ROBOT_ERROR_RANGE)) {
        float r12 = -oz*cos_theta2*sin_theta3 + oz*cos_theta3*sin_theta2 + ox*cos_theta1*cos_theta2*cos_theta3+ oy*cos_theta2*cos_theta3*sin_theta1 + ox*cos_theta1*sin_theta2*sin_theta3 + oy*sin_theta1*sin_theta2*sin_theta3;
        float r11 = -nz*cos_theta2*sin_theta3 + nz*cos_theta3*sin_theta2 + nx*cos_theta1*cos_theta2*cos_theta3+ ny*cos_theta2*cos_theta3*sin_theta1 + nx*cos_theta1*sin_theta2*sin_theta3 + ny*sin_theta1*sin_theta2*sin_theta3;
        theta6_zyz = atan2(-r12, r11);
        theta6 = theta6_zyz - M_PI;
        return theta6;
    }

    float r32 = ox*cos_theta1*cos_theta3*sin_theta2 - oz*sin_theta2*sin_theta3 - ox*cos_theta1*cos_theta2*sin_theta3 - oz*cos_theta2*cos_theta3 - oy*cos_theta2*sin_theta1*sin_theta3 + oy*cos_theta3*sin_theta1*sin_theta2;
    float r31 = nx*cos_theta1*cos_theta3*sin_theta2 - nz*sin_theta2*sin_theta3 - nx*cos_theta1*cos_theta2*sin_theta3 - nz*cos_theta2*cos_theta3 - ny*cos_theta2*sin_theta1*sin_theta3 + ny*cos_theta3*sin_theta1*sin_theta2;
    theta6_zyz = atan2(r32, -r31);
    theta6 = theta6_zyz - M_PI;
    return theta6;
}

static void robot_kinematics_calc_theta6(void)
{
    for (int i = 0; i < ROBOT_KINEMATICS_RESULT_NUM; i++) {
        float theta1 = g_robot_kinematics.result[i][ROBOT_JOINT_1];
        float theta2 = g_robot_kinematics.result[i][ROBOT_JOINT_2];
        float theta3 = g_robot_kinematics.result[i][ROBOT_JOINT_3];
        float theta4 = g_robot_kinematics.result[i][ROBOT_JOINT_4];
        float theta5 = g_robot_kinematics.result[i][ROBOT_JOINT_5];
        g_robot_kinematics.result[i][ROBOT_JOINT_6] = __robot_kinematics_calc_theta6(theta1, theta2, theta3, theta4, theta5);
    }
}

static void robot_kinematics_calc(void)
{
    robot_kinematics_calc_theta3();
    robot_kinematics_calc_theta2();
    robot_kinematics_calc_theta1();
    robot_kinematics_calc_theta5();
    robot_kinematics_calc_theta4();
    robot_kinematics_calc_theta6();
    g_robot_kinematics.result_invalid_mask = 0;
}

static float radians_to_degrees_0_360(float radians)
{
    float degrees = radians * (180.0f / M_PI);
    degrees = fmod(degrees, 360.0f);
    if (degrees < 0) {
        degrees += 360.0f;
    }
    
    if (fabs(degrees - 360.0f) < ROBOT_ERROR_RANGE) {
        degrees = 0.0f;
    }
    return degrees;
}

static void robot_kinematics_radians_to_degrees(void)
{
    for (int i = 0; i < ROBOT_KINEMATICS_RESULT_NUM; i++) {
        for (int j = 0; j < ROBOT_MAX_JOINT_NUM; j++) {
            g_robot_kinematics.result[i][j] = radians_to_degrees_0_360(g_robot_kinematics.result[i][j]); 
        }
    }
}

static void robot_kinematics_joint_angle_map(void)
{
    for (int i = 0; i < ROBOT_KINEMATICS_RESULT_NUM; i++) {
        for (int j = 0; j < ROBOT_MAX_JOINT_NUM; j++) {
            float angle = g_robot_kinematics.result[i][j];
            float min_angle = JOINT_LIMITS[j][0];
            float max_angle = JOINT_LIMITS[j][1];

            if (fabs(angle - min_angle) < ROBOT_ERROR_RANGE) {
                angle = min_angle; 
            }

            if (fabs(angle - max_angle) < ROBOT_ERROR_RANGE) {
                angle = max_angle; 
            }

            if (angle < min_angle) {
                angle += 360;
            } else if (angle > max_angle) {
                angle -= 360;
            }
            
            if ((angle < min_angle) || (angle > max_angle)) {
                g_robot_kinematics.result_invalid_mask |= (1 << i);
            }
            g_robot_kinematics.result[i][j] = angle;
        } 
    } 
}

static int robot_kinematics_get_optimal_result(float *result)
{
    float diff = 0;
    float min_diff = 0xfffffff;
    int min_diff_result_index = -1;
    
    for (int i = 0; i < ROBOT_KINEMATICS_RESULT_NUM; i++) {
        if (g_robot_kinematics.result_invalid_mask & (1 << i)) {
            continue;
        }
        diff = 0;
        for (int j = 0; j < ROBOT_MAX_JOINT_NUM; j++) {
            diff += fabs(g_robot_kinematics.result[i][j] - g_current_joint_angle[j]) * JOINT_WEIGHT[j];
        }
        if (diff < min_diff) {
            min_diff = diff;
            min_diff_result_index = i; 
        }
    }

    if (min_diff_result_index == -1) {
        return -1;
    }

    for (int j = 0; j < ROBOT_MAX_JOINT_NUM; j++) {
        result[j] = g_robot_kinematics.result[min_diff_result_index][j]; 
    }

    return 0;
}

int RobotKin_Inverse(float *T_target, float *result, int show)
{
    memcpy(g_robot_kinematics.T, T_target, sizeof(float)*16);

    robot_kinematics_calc();
    
    robot_kinematics_radians_to_degrees();
    robot_kinematics_joint_angle_map();

    int ret = robot_kinematics_get_optimal_result(result);
    return ret;
}

void RobotKin_CalcT(const float T_in[4][4], float T_out[4][4], Position_t *pos)
{
    memcpy(T_out, T_in, sizeof(float)*16);
    T_out[0][3] = pos->x + T_out[0][3];
    T_out[1][3] = pos->y + T_out[1][3];
    T_out[2][3] = pos->z + T_out[2][3];
}

void RobotKin_UpdateJointAngle(uint32_t joint_id, float angle)
{
    if (joint_id >= ROBOT_MAX_JOINT_NUM) {
        return; 
    }
    g_current_joint_angle[joint_id] = angle;
}

void RobotKin_UpdateAllJointAngles(float *joint_angle)
{
    memcpy(g_current_joint_angle, joint_angle, sizeof(float)*ROBOT_MAX_JOINT_NUM);
}

void RobotKin_ShowResult(void)
{
    for (int i = 0; i < ROBOT_KINEMATICS_RESULT_NUM; i++) {
        printf("result[%d]: ", i);
        for (int j = 0; j < ROBOT_MAX_JOINT_NUM; j++) {
            printf("%.2f ", g_robot_kinematics.result[i][j]);
        }
        int valid = g_robot_kinematics.result_invalid_mask & (1 << i) ? 0 : 1;
        printf(" valid:%d\n", valid);
    }
}

void RobotKin_ShowT(float T[4][4])
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            printf("%.2f ", T[i][j]);
        }
        printf("\n");
    }
}
