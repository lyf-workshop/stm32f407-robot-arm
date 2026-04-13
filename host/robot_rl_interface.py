"""
Robot Arm RL Interface Example
基于串口通信的机械臂强化学习接口示例

Requirements:
    pip install pyserial numpy

Usage:
    python robot_rl_interface.py
"""

import serial
import time
import numpy as np
from typing import List, Tuple, Optional

class RobotArmEnv:
    """
    机械臂 RL 环境接口
    通过串口 rl_step 命令与 STM32 通信
    """
    
    def __init__(self, port='COM3', baudrate=115200, timeout=1.0):
        """
        初始化串口连接
        
        Args:
            port: 串口号 (Windows: 'COM3', Linux: '/dev/ttyUSB0')
            baudrate: 波特率 (默认 115200)
            timeout: 超时时间 (秒)
        """
        self.ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(2)  # 等待 STM32 复位
        
        # 清空缓冲区
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        
        # 读取欢迎信息
        time.sleep(0.5)
        welcome = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
        print(welcome)
        
        self.observation_space = 6  # 6 个关节角度
        self.action_space = 6       # 6 个关节角度增量
        
        print(f"[RobotArmEnv] Connected to {port}")
    
    def reset(self) -> np.ndarray:
        """
        重置环境到初始状态
        
        Returns:
            observation: 当前关节角度 [6]
        """
        # 发送 zero 命令，将当前位置设为零点
        self._send_command("zero")
        time.sleep(0.5)
        
        # 读取当前状态
        observation = self._get_status()
        return observation
    
    def step(self, action: np.ndarray,
             action_in_radians: bool = True) -> Tuple[np.ndarray, float, bool, dict]:
        """
        执行一步动作

        Args:
            action: 关节角度增量 [6]
                    当 action_in_radians=True 时单位为弧度（对应 MuJoCo 策略输出）；
                    当 action_in_radians=False 时单位为度（直接传入）。
            action_in_radians: 是否将弧度转换为度数再发送给 STM32。
                    MuJoCo 环境输出 ±0.524 rad (±30°) 的增量，
                    STM32 rl_step 命令接受度数，所以默认 True。

        Returns:
            observation: 新的关节角度 [6]（度）
            reward: 奖励值
            done: 是否结束
            info: 额外信息字典
        """
        if action_in_radians:
            # 弧度 → 度，并限制在 ±30° 以保护真机
            action_deg = np.clip(np.rad2deg(action), -30.0, 30.0)
        else:
            action_deg = np.clip(action, -30.0, 30.0)

        # 发送 rl_step 命令（STM32 接受度数）
        cmd = f"rl_step {' '.join(f'{x:.2f}' for x in action_deg)}\r\n"
        self.ser.write(cmd.encode())
        
        # 读取响应
        response = self._read_until_prompt()
        
        # 解析状态
        observation = self._parse_state(response)
        
        # 检查是否触发限位
        done = "warn: joint_limit" in response
        
        # 计算奖励（示例：保持在中心位置的奖励）
        reward = self._calculate_reward(observation, action, done)
        
        info = {
            'raw_response': response,
            'limit_hit': done
        }
        
        return observation, reward, done, info
    
    def _send_command(self, cmd: str):
        """发送命令"""
        self.ser.write((cmd + '\r\n').encode())
    
    def _read_until_prompt(self, timeout=2.0) -> str:
        """读取响应直到出现 Ready> 提示符"""
        start_time = time.time()
        buffer = ""
        
        while (time.time() - start_time) < timeout:
            if self.ser.in_waiting > 0:
                data = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
                buffer += data
                if "Ready>" in buffer:
                    return buffer
            time.sleep(0.01)
        
        return buffer
    
    def _parse_state(self, response: str) -> np.ndarray:
        """
        从响应中解析关节角度
        格式: state 90.50 89.70 -89.80 0.00 90.10 0.00
        """
        for line in response.split('\n'):
            if line.strip().startswith('state'):
                parts = line.strip().split()
                if len(parts) == 7:  # 'state' + 6 angles
                    angles = [float(parts[i]) for i in range(1, 7)]
                    return np.array(angles, dtype=np.float32)
        
        # 如果解析失败，返回零向量
        return np.zeros(6, dtype=np.float32)
    
    def _get_status(self) -> np.ndarray:
        """获取当前状态"""
        self._send_command("status")
        response = self._read_until_prompt()
        
        # 解析格式: Joints: [90.00, 90.00, -90.00, 0.00, 90.00, 0.00] deg
        for line in response.split('\n'):
            if 'Joints:' in line:
                # 提取方括号内的内容
                start = line.find('[')
                end = line.find(']')
                if start != -1 and end != -1:
                    angles_str = line[start+1:end]
                    angles = [float(x.strip()) for x in angles_str.split(',')]
                    return np.array(angles, dtype=np.float32)
        
        return np.zeros(6, dtype=np.float32)
    
    def _calculate_reward(self, obs: np.ndarray, action: np.ndarray, done: bool) -> float:
        """
        计算奖励值（示例）
        
        可根据具体任务自定义：
        - 到达目标位置
        - 避免奇异点
        - 最小化能量消耗
        - 平滑运动
        """
        # 示例：惩罚过大的关节角度偏差
        target_angles = np.array([90, 90, 0, 0, 90, 0])
        error = np.abs(obs - target_angles).sum()
        reward = -error * 0.01
        
        # 惩罚触碰限位
        if done:
            reward -= 10.0
        
        # 惩罚过大动作
        action_penalty = np.abs(action).sum() * 0.01
        reward -= action_penalty
        
        return reward
    
    def close(self):
        """关闭串口连接"""
        if self.ser.is_open:
            self.ser.close()
        print("[RobotArmEnv] Connection closed")


# ========== 使用示例 Example Usage ==========

def example_manual_control():
    """手动控制示例"""
    env = RobotArmEnv(port='COM3')
    
    try:
        # 重置
        obs = env.reset()
        print(f"Initial state: {obs}")
        
        # 执行几步随机动作
        for i in range(5):
            action = np.random.uniform(-2, 2, size=6)
            obs, reward, done, info = env.step(action)
            print(f"Step {i+1}: obs={obs}, reward={reward:.2f}, done={done}")
            
            if done:
                print("Hit joint limit, resetting...")
                obs = env.reset()
        
    finally:
        env.close()


def example_td3_integration():
    """
    集成 Stable-Baselines3 TD3 的示例框架
    
    完整实现参考原项目：
    https://gitee.com/dearxie/zero-robotic-arm/tree/master/5.%20Deep_RL
    """
    print("TD3 Integration Example (Framework)")
    print("For full implementation, see zero-robotic-arm/Deep_RL/")
    
    # 伪代码框架
    """
    from stable_baselines3 import TD3
    
    env = RobotArmEnv(port='COM3')
    
    # 训练（通常在仿真中）
    # model = TD3("MlpPolicy", env, verbose=1)
    # model.learn(total_timesteps=10000)
    
    # 部署到实体机械臂
    model = TD3.load("robot_td3_model.zip")
    
    obs = env.reset()
    for step in range(100):
        action, _states = model.predict(obs, deterministic=True)
        obs, reward, done, info = env.step(action)
        
        if done:
            obs = env.reset()
    
    env.close()
    """


def example_simple_test():
    """简单测试：让关节 0 来回摆动"""
    env = RobotArmEnv(port='COM3')
    
    try:
        obs = env.reset()
        print(f"Initial: {obs}")
        
        # 关节 0 正向 5 度
        obs, _, _, _ = env.step([5.0, 0, 0, 0, 0, 0])
        print(f"After +5 deg: {obs[0]:.2f}")
        time.sleep(1)
        
        # 关节 0 负向 5 度
        obs, _, _, _ = env.step([-5.0, 0, 0, 0, 0, 0])
        print(f"After -5 deg: {obs[0]:.2f}")
        
        print("Test completed!")
        
    finally:
        env.close()


if __name__ == "__main__":
    print("=" * 60)
    print("Robot Arm RL Interface Example")
    print("=" * 60)
    print("\n请选择测试模式 Select test mode:")
    print("1. 手动控制示例 Manual control")
    print("2. 简单测试 Simple test")
    print("3. TD3 集成框架 TD3 integration framework")
    
    choice = input("输入选项 Enter choice (1-3): ").strip()
    
    if choice == '1':
        example_manual_control()
    elif choice == '2':
        example_simple_test()
    elif choice == '3':
        example_td3_integration()
    else:
        print("Invalid choice")
