import numpy as np
import gymnasium as gym
from gymnasium import spaces
import mujoco
import mujoco.viewer

# Joint limits from XML (radians). Used to clip incremental position targets.
# Joints without an XML range (J1, J4, J6) use ±π as a safe working envelope.
JOINT_LIMITS_LOW  = np.array([-np.pi, -1.5708,  0.0,    -np.pi, -1.5708, -np.pi], dtype=np.float32)
JOINT_LIMITS_HIGH = np.array([ np.pi,  1.5708,  3.1416,  np.pi,  0.0,     np.pi], dtype=np.float32)

# Maximum angle increment per policy step (rad).  ±30° matches a comfortable
# single rl_step command on the real hardware.
MAX_DELTA = np.deg2rad(30.0)   # 0.5236 rad


class RobotArmEnv(gym.Env):
    """
    6-DOF robot arm end-effector reaching environment (MuJoCo, position control).

    Control interface
    -----------------
    Action  : Δθ per joint (rad), shape=(6,), clipped to ±MAX_DELTA (±30°).
              Each policy step the env sets ctrl = clip(qpos + Δθ, joint_limits)
              and runs FRAME_SKIP MuJoCo substeps (2 ms each → 100 ms per step).
    Observation : 18-dim
              [ relative_pos(3), joint_angles(6), joint_velocities(6), ee_velocity(3) ]

    Sim-to-real mapping
    -------------------
    The action Δθ (rad) maps directly to the STM32 rl_step command:
        rl_step d1 d2 d3 d4 d5 d6   (degrees)
    via:  d_i = Δθ_i × (180 / π)
    This is handled in robot_rl_interface.py; the env itself works in radians.
    """

    metadata = {"render_modes": ["human"], "render_fps": 30}

    # Number of MuJoCo substeps per policy step.
    # 50 × 0.002 s = 0.1 s per step  (matches ~100 ms real-hardware step period)
    FRAME_SKIP = 50

    def __init__(self, render_mode=None):
        self.model = mujoco.MjModel.from_xml_path("robot_arm_mujoco.xml")
        self.data  = mujoco.MjData(self.model)

        self.nu = self.model.nu   # number of actuators (6)
        self.nq = self.model.nq   # number of joint positions (6)

        # ------------------------------------------------------------------
        # Action space: angle increment per joint, ±MAX_DELTA rad (±30°)
        # ------------------------------------------------------------------
        self.action_space = spaces.Box(
            low=-MAX_DELTA, high=MAX_DELTA,
            shape=(self.nu,), dtype=np.float32
        )

        # ------------------------------------------------------------------
        # Observation space: 18-dim
        #   [0:3]   relative_pos  = target_pos − ee_pos
        #   [3:9]   joint_angles  (qpos)
        #   [9:15]  joint_velocities (qvel)
        #   [15:18] ee_velocity   (Cartesian)
        # ------------------------------------------------------------------
        state_dim = 3 + 6 + 6 + 3   # = 18
        self.observation_space = spaces.Box(
            low=-np.inf, high=np.inf,
            shape=(state_dim,), dtype=np.float32
        )

        self.target_pos        = np.zeros(3, dtype=np.float32)
        self.previous_distance = None
        self.min_distance      = None
        self.previous_action   = np.zeros(self.nu, dtype=np.float32)

        self.render_mode = render_mode
        self.viewer      = None

        self.reset()

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _ee_pos(self) -> np.ndarray:
        ee_site_id = mujoco.mj_name2id(self.model, mujoco.mjtObj.mjOBJ_SITE, "ee_site")
        return self.data.site_xpos[ee_site_id].copy()

    def _ee_vel(self) -> np.ndarray:
        ee_body_id = mujoco.mj_name2id(self.model, mujoco.mjtObj.mjOBJ_BODY, "ee_link")
        return self.data.cvel[ee_body_id][:3].copy()

    def _get_state(self) -> np.ndarray:
        relative_pos      = self.target_pos - self._ee_pos()
        joint_angles      = self.data.qpos[:self.nu].copy()
        joint_velocities  = self.data.qvel[:self.nu].copy()
        ee_vel            = self._ee_vel()

        return np.concatenate([
            relative_pos,      # (3,)
            joint_angles,      # (6,)
            joint_velocities,  # (6,)
            ee_vel             # (3,)
        ]).astype(np.float32)

    # ------------------------------------------------------------------
    # Gymnasium API
    # ------------------------------------------------------------------

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)

        mujoco.mj_resetData(self.model, self.data)

        # Randomise target inside a reachable workspace (metres)
        self.target_pos = np.array([
            self.np_random.uniform(-0.2,  0.2),
            self.np_random.uniform(-0.37, -0.17),
            self.np_random.uniform( 0.2,  0.4)
        ], dtype=np.float32)

        self.success_threshold = 0.01   # 1 cm
        self.step_count        = 0
        self.previous_distance = None
        self.min_distance      = None
        self.previous_action   = np.zeros(self.nu, dtype=np.float32)
        self._phase_rewards_given = set()

        # Initialise ctrl to current qpos so the servo starts at rest
        self.data.ctrl[:self.nu] = self.data.qpos[:self.nu]

        return self._get_state(), {}

    def step(self, action):
        action = np.clip(action, -MAX_DELTA, MAX_DELTA).astype(np.float32)

        # ------------------------------------------------------------------
        # Position control: target = current_angle + delta, clamped to limits
        # ------------------------------------------------------------------
        current_qpos  = self.data.qpos[:self.nu].copy()
        target_angles = np.clip(current_qpos + action,
                                JOINT_LIMITS_LOW, JOINT_LIMITS_HIGH)
        self.data.ctrl[:self.nu] = target_angles

        # Run FRAME_SKIP physics substeps (= 100 ms per policy step)
        for _ in range(self.FRAME_SKIP):
            mujoco.mj_step(self.model, self.data)

        self.step_count += 1
        max_steps = 500

        state    = self._get_state()
        ee_pos   = self._ee_pos()
        distance = float(np.linalg.norm(ee_pos - self.target_pos))
        reward   = 0.0

        # ── Step penalty (time pressure) ──────────────────────────────────
        reward -= 0.1

        # ── Distance improvement reward ───────────────────────────────────
        if self.min_distance is None:
            self.min_distance      = distance
            self.previous_distance = distance
            improvement_reward     = 0.0
        elif distance < self.min_distance:
            improvement_reward    = 1.0 * (self.min_distance - distance)
            self.min_distance     = distance
        elif distance > self.previous_distance:
            improvement_reward    = -0.8 * (distance - self.previous_distance)
        else:
            improvement_reward    = 0.0
        self.previous_distance = distance

        # ── Continuous distance penalty ───────────────────────────────────
        base_distance_penalty = -(distance ** 0.5) * 0.8

        # ── One-time phase bonuses ────────────────────────────────────────
        phase_distance_reward = 0.0
        phase_thresholds = [0.5, 0.3, 0.1, 0.05, 0.01, 0.005, 0.002]
        phase_rewards    = [100., 200., 300., 500., 1000., 1500., 2000.]
        for thresh, pr in zip(phase_thresholds, phase_rewards):
            if distance < thresh and thresh not in self._phase_rewards_given:
                phase_distance_reward += pr
                self._phase_rewards_given.add(thresh)

        reward += improvement_reward + base_distance_penalty + phase_distance_reward

        # ── End-effector direction reward ─────────────────────────────────
        ee_vel   = self._ee_vel()
        ee_speed = float(np.linalg.norm(ee_vel))
        if ee_speed > 0.5:
            reward -= 0.2   # high-speed penalty
        to_target    = self.target_pos - ee_pos
        to_target   /= (np.linalg.norm(to_target) + 1e-6)
        move_dir     = ee_vel / (ee_speed + 1e-6)
        dir_cos      = np.dot(to_target, move_dir)
        reward      += max(0.0, float(dir_cos)) ** 2 * 1.0

        # ── Action smoothness penalty ─────────────────────────────────────
        # Penalise large or jerky joint angle increments.
        # Replaces the previous_torque penalty from the torque-control version.
        action_penalty   = -0.05 * float(np.sum(np.abs(action)))
        jerk_penalty     = -0.03 * float(np.sum(np.abs(action - self.previous_action)))
        reward          += action_penalty + jerk_penalty
        self.previous_action = action

        # ── Collision penalty ─────────────────────────────────────────────
        # Now effective: contype=1 in XML means non-adjacent links DO generate
        # contacts.  Only triggered for unexpected self-collision or floor hits.
        collision_detected = self.data.ncon > 0
        if collision_detected:
            reward -= 5000.0

        # ── Termination ───────────────────────────────────────────────────
        done = False

        if distance <= self.success_threshold:
            done     = True
            reward  += 10000.0
            reward  += 4.0 * (max_steps - self.step_count)   # time bonus
            # Speed bonus at arrival
            if   ee_speed < 0.01: reward += 2000.0
            elif ee_speed < 0.05: reward += 1000.0
            elif ee_speed < 0.10: reward +=  500.0

        if collision_detected:
            # Deduct remaining step penalties so the agent can't gain by
            # crashing early (suicide prevention)
            reward  -= 0.1 * (max_steps - self.step_count)
            done     = True

        truncated = self.step_count >= max_steps

        return state, reward, done, truncated, {}

    # ------------------------------------------------------------------
    # Rendering
    # ------------------------------------------------------------------

    def render(self):
        if self.render_mode is None:
            return
        if self.viewer is None:
            try:
                self.viewer = mujoco.viewer.launch_passive(self.model, self.data)
                self._target_viz_added = False
            except Exception as e:
                print(f"Cannot launch viewer: {e}")
                return
        if self.viewer:
            try:
                self._add_target_visualization()
                self.viewer.sync()
            except Exception:
                self.viewer = None

    def _add_target_visualization(self):
        if not self.viewer:
            return
        scn = self.viewer.user_scn
        if not getattr(self, "_target_viz_added", False):
            if scn.ngeom < scn.maxgeom:
                self._target_geom = scn.ngeom
                geom = scn.geoms[scn.ngeom]
                scn.ngeom += 1
                mujoco.mjv_initGeom(
                    geom,
                    mujoco.mjtGeom.mjGEOM_SPHERE,
                    np.array([0.02, 0.02, 0.02]),
                    self.target_pos,
                    np.eye(3).flatten(),
                    np.array([0.0, 1.0, 0.0, 0.8])
                )
                geom.category = mujoco.mjtCatBit.mjCAT_DECOR
                self._target_viz_added = True
        else:
            scn.geoms[self._target_geom].pos[:] = self.target_pos

    def close(self):
        if self.viewer:
            self.viewer.close()
            self.viewer = None
