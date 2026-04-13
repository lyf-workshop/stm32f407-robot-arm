"""
Raspberry Pi Robot Arm Controller
树莓派机械臂串口控制脚本

硬件连接:
    树莓派 GPIO14 (TXD)  --> STM32 PA10 (RX)
    树莓派 GPIO15 (RXD)  --> STM32 PA9  (TX)
    树莓派 GND           --> STM32 GND

串口配置: 115200, 8N1, 无流控

Requirements:
    pip3 install pyserial

Usage:
    python3 raspberry_pi_control.py              # 交互式命令行模式
    python3 raspberry_pi_control.py --demo       # 运行内置演示
    python3 raspberry_pi_control.py --test       # 连接测试
"""

import serial
import time
import sys
import argparse
import threading
from typing import Optional, List


# ─────────────────────────────────────────────
#  串口端口选项（取消注释对应你的树莓派型号）
# ─────────────────────────────────────────────
# 树莓派 3B / 4B / 5 (使用硬件 UART，需关闭蓝牙或重映射):
SERIAL_PORT = "/dev/ttyAMA0"
# 树莓派 3B / 4B 备用软件 UART (mini UART，波特率精度较低):
# SERIAL_PORT = "/dev/ttyS0"
# USB 转 TTL 模块 (CH340/CP2102 等):
# SERIAL_PORT = "/dev/ttyUSB0"

BAUD_RATE   = 115200
TIMEOUT     = 2.0      # 等待响应超时（秒）
PROMPT      = "Ready>" # STM32 命令完成提示符


class RobotArmController:
    """
    STM32 机械臂串口控制器

    支持所有固件命令：
        rel_rotate, abs_rotate, sync, auto (IK),
        zero, stop, status, sync_pos, calibrate,
        rl_step, demo_joint, demo_pick, demo_circle
    """

    def __init__(self, port: str = SERIAL_PORT, baudrate: int = BAUD_RATE,
                 timeout: float = TIMEOUT, verbose: bool = True):
        self.verbose  = verbose
        self._lock    = threading.Lock()
        self._connect(port, baudrate, timeout)

    # ──────────────── 连接管理 ────────────────

    def _connect(self, port: str, baudrate: int, timeout: float):
        try:
            self.ser = serial.Serial(
                port     = port,
                baudrate = baudrate,
                bytesize = serial.EIGHTBITS,
                parity   = serial.PARITY_NONE,
                stopbits = serial.STOPBITS_ONE,
                timeout  = timeout,
                xonxoff  = False,
                rtscts   = False,
                dsrdtr   = False,
            )
        except serial.SerialException as e:
            print(f"[错误] 无法打开串口 {port}: {e}")
            print("请检查:")
            print("  1. 接线是否正确（TX/RX 是否交叉连接）")
            print("  2. 树莓派 UART 是否已启用（见 RASPBERRY_PI_GUIDE.md）")
            print("  3. 当前用户是否在 dialout 组: sudo usermod -aG dialout $USER")
            sys.exit(1)

        time.sleep(0.5)
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()

        # 读取 STM32 上电欢迎信息
        time.sleep(1.5)
        welcome = self._read_available()
        if self.verbose and welcome:
            print(welcome)

        if self.verbose:
            print(f"[已连接] {port} @ {baudrate} baud")

    def close(self):
        """关闭串口"""
        if self.ser and self.ser.is_open:
            self.ser.close()
        if self.verbose:
            print("[已断开]")

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    # ──────────────── 底层收发 ────────────────

    def _read_available(self) -> str:
        if self.ser.in_waiting > 0:
            return self.ser.read(self.ser.in_waiting).decode("utf-8", errors="ignore")
        return ""

    def _read_until_prompt(self, timeout: float = TIMEOUT) -> str:
        """读取串口数据直到出现 Ready> 提示符"""
        buf   = ""
        start = time.time()
        while (time.time() - start) < timeout:
            if self.ser.in_waiting > 0:
                chunk = self.ser.read(self.ser.in_waiting).decode("utf-8", errors="ignore")
                buf  += chunk
                if PROMPT in buf:
                    break
            else:
                time.sleep(0.005)
        return buf

    def send_raw(self, cmd: str, response_timeout: float = TIMEOUT) -> str:
        """
        发送原始命令字符串并返回响应文本。
        线程安全。
        """
        with self._lock:
            self.ser.reset_input_buffer()
            self.ser.write((cmd.strip() + "\r\n").encode())
            response = self._read_until_prompt(response_timeout)
        if self.verbose:
            # 只打印非空、非提示符的行
            for line in response.splitlines():
                line = line.strip()
                if line and line != PROMPT:
                    print(f"  << {line}")
        return response

    # ──────────────── 运动控制命令 ────────────────

    def rel_rotate(self, joint_id: int, angle: float) -> bool:
        """
        单轴相对转动
        joint_id: 0~5
        angle:    度数（正/负）
        """
        resp = self.send_raw(f"rel_rotate {joint_id} {angle:.2f}")
        return resp.strip().find("OK") != -1

    def abs_rotate(self, joint_id: int, angle: float) -> bool:
        """单轴绝对转动到指定角度"""
        resp = self.send_raw(f"abs_rotate {joint_id} {angle:.2f}")
        return resp.strip().find("OK") != -1

    def sync(self, angles: List[float]) -> bool:
        """
        六轴同步运动到绝对角度
        angles: [j0, j1, j2, j3, j4, j5]（度）
        """
        if len(angles) != 6:
            raise ValueError("sync() 需要 6 个关节角度")
        cmd = "sync " + " ".join(f"{a:.2f}" for a in angles)
        resp = self.send_raw(cmd, response_timeout=10.0)
        return "OK" in resp

    def move_to_pose(self, x: float, y: float, z: float) -> bool:
        """
        笛卡尔空间运动（自动 IK 求解）
        x, y, z: 目标坐标（mm）
        """
        resp = self.send_raw(f"auto {x:.1f} {y:.1f} {z:.1f}", response_timeout=10.0)
        return "OK" in resp

    def rl_step(self, deltas: List[float]) -> Optional[List[float]]:
        """
        强化学习步进：各关节增量运动
        deltas: 6 个关节的角度增量（度），会被 STM32 截断到 [-5, 5]
        返回: 执行后各关节角度列表，失败返回 None
        """
        if len(deltas) != 6:
            raise ValueError("rl_step() 需要 6 个增量值")
        cmd  = "rl_step " + " ".join(f"{d:.2f}" for d in deltas)
        resp = self.send_raw(cmd, response_timeout=5.0)
        return self._parse_state(resp)

    def zero(self) -> bool:
        """将当前位置设为软件零点"""
        resp = self.send_raw("zero")
        return "OK" in resp

    def stop(self) -> bool:
        """紧急停止所有电机"""
        resp = self.send_raw("stop")
        return "OK" in resp

    def status(self) -> Optional[List[float]]:
        """
        查询当前关节角度（软件值）
        返回: [j0, j1, j2, j3, j4, j5] 度，失败返回 None
        """
        resp = self.send_raw("status")
        return self._parse_joints(resp)

    def sync_pos(self) -> Optional[List[float]]:
        """
        从电机编码器读取实际角度并同步软件状态
        返回: 同步后的角度列表
        """
        resp = self.send_raw("sync_pos", response_timeout=8.0)
        return self._parse_joints(resp)

    def calibrate(self, joint_id: int) -> str:
        """
        对比指定关节的软件角度与编码器角度
        返回原始响应字符串
        """
        return self.send_raw(f"calibrate {joint_id}")

    # ──────────────── 演示命令 ────────────────

    def demo_joint(self) -> bool:
        """运行关节空间演示动作"""
        resp = self.send_raw("demo_joint", response_timeout=30.0)
        return "completed" in resp

    def demo_pick(self) -> bool:
        """运行抓取放置演示"""
        resp = self.send_raw("demo_pick", response_timeout=30.0)
        return "completed" in resp

    def demo_circle(self) -> bool:
        """运行圆弧轨迹演示"""
        resp = self.send_raw("demo_circle", response_timeout=30.0)
        return "completed" in resp

    # ──────────────── 响应解析 ────────────────

    @staticmethod
    def _parse_joints(response: str) -> Optional[List[float]]:
        """解析 status / sync_pos 返回的关节角度"""
        for line in response.splitlines():
            if "Joints:" in line:
                start = line.find("[")
                end   = line.find("]")
                if start != -1 and end != -1:
                    try:
                        return [float(v.strip()) for v in line[start+1:end].split(",")]
                    except ValueError:
                        pass
        return None

    @staticmethod
    def _parse_state(response: str) -> Optional[List[float]]:
        """解析 rl_step 返回的 state 行"""
        for line in response.splitlines():
            parts = line.strip().split()
            if parts and parts[0] == "state" and len(parts) == 7:
                try:
                    return [float(p) for p in parts[1:]]
                except ValueError:
                    pass
        return None


# ═══════════════════════════════════════════════════
#  交互式命令行 Shell
# ═══════════════════════════════════════════════════

def interactive_shell(robot: RobotArmController):
    """
    交互式命令行，直接透传命令给 STM32。
    输入 help 查看固件支持的命令，输入 quit 退出。
    """
    print("\n" + "="*55)
    print("  机械臂交互控制台  （输入 quit 退出）")
    print("="*55)
    print("  常用命令:")
    print("    status                       查看关节角度")
    print("    rel_rotate <关节0-5> <角度>   相对旋转")
    print("    abs_rotate <关节0-5> <角度>   绝对旋转")
    print("    sync <a0> <a1> <a2> <a3> <a4> <a5>  六轴同步")
    print("    auto <x> <y> <z>             笛卡尔移动")
    print("    zero                         设置当前零点")
    print("    stop                         急停")
    print("    sync_pos                     从编码器同步位置")
    print("    demo_joint                   关节演示")
    print("="*55 + "\n")

    while True:
        try:
            cmd = input("robot> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n退出")
            break

        if not cmd:
            continue
        if cmd.lower() in ("quit", "exit", "q"):
            break

        robot.send_raw(cmd, response_timeout=15.0)


# ═══════════════════════════════════════════════════
#  内置演示
# ═══════════════════════════════════════════════════

def run_builtin_demo(robot: RobotArmController):
    """基本功能演示：查询状态 → 关节运动 → 回零"""
    print("\n[演示] 开始基本功能演示...")

    # 1. 查询初始状态
    print("\n[1/5] 查询当前关节角度...")
    angles = robot.status()
    if angles:
        print(f"      当前角度: {[f'{a:.1f}°' for a in angles]}")
    else:
        print("      警告: 状态读取失败，请检查连接")

    # 2. 从编码器同步真实位置
    print("\n[2/5] 从电机编码器同步实际位置...")
    synced = robot.sync_pos()
    if synced:
        print(f"      同步后: {[f'{a:.1f}°' for a in synced]}")

    # 3. 关节 0 相对转动 +10°
    print("\n[3/5] 关节 0 相对转动 +10°...")
    ok = robot.rel_rotate(0, 10.0)
    print(f"      结果: {'成功' if ok else '失败'}")
    time.sleep(1.0)

    # 4. 关节 0 回到 0°
    print("\n[4/5] 关节 0 回到 0°...")
    ok = robot.abs_rotate(0, 0.0)
    print(f"      结果: {'成功' if ok else '失败'}")
    time.sleep(1.0)

    # 5. 查询末态
    print("\n[5/5] 查询末态...")
    angles = robot.status()
    if angles:
        print(f"      当前角度: {[f'{a:.1f}°' for a in angles]}")

    print("\n[演示] 完成!")


def run_connection_test(robot: RobotArmController):
    """连接测试：仅读取状态验证通信是否正常"""
    print("\n[测试] 串口连接测试...")
    angles = robot.status()
    if angles and len(angles) == 6:
        print(f"[测试] 通信正常 ✓")
        print(f"       关节角度: {[f'{a:.2f}°' for a in angles]}")
    else:
        print("[测试] 通信异常 ✗ — 未收到有效响应")
        print("       请检查接线和串口配置")


# ═══════════════════════════════════════════════════
#  主入口
# ═══════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(
        description="树莓派机械臂串口控制脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--port",    default=SERIAL_PORT, help=f"串口设备 (默认: {SERIAL_PORT})")
    parser.add_argument("--baud",    default=BAUD_RATE,   type=int, help=f"波特率 (默认: {BAUD_RATE})")
    parser.add_argument("--demo",    action="store_true", help="运行内置演示动作")
    parser.add_argument("--test",    action="store_true", help="只做连接测试后退出")
    parser.add_argument("--quiet",   action="store_true", help="关闭详细输出")
    args = parser.parse_args()

    with RobotArmController(port=args.port, baudrate=args.baud,
                            verbose=not args.quiet) as robot:
        if args.test:
            run_connection_test(robot)
        elif args.demo:
            run_builtin_demo(robot)
        else:
            interactive_shell(robot)


if __name__ == "__main__":
    main()
