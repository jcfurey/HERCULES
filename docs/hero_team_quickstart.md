# Running a UAV–UGV Team

This page runs HERCULES' heterogeneous mode (`"SimMode": "Hero"`) in the Blocks environment with two drones (`Drone1`, `Drone2`) and two Husky UGVs (`Husky1`, `Husky2`). You record a trajectory for each robot by teleoperation, then replay all four together with the waypoint controllers while the ROS 2 bridges publish every robot's sensors. See [Heterogeneous UAV–UGV Autonomy](heterogeneous_autonomy.md) for what the mode provides.

Hero mode is part of the HERCULES Unreal plugin. The upstream Cosys-AirSim packaged Blocks demo used in the [ROS 2 wrapper](ros_cplusplus.md) quick start does not include it, so this page builds the plugin into a Blocks project.

## Requirements

- Linux with an NVIDIA GPU and driver.
- [Unreal Engine 5.2.1](install_linux.md#install-unreal-engine).
- ROS 2 Jazzy on Ubuntu 24.04 or ROS 2 Lyrical on Ubuntu 26.04, for the ROS 2 bridges.

Run the commands below from the HERCULES repository root unless a step says otherwise. Replace `/path/to/UnrealEngine` with the folder that contains `Engine/`.

## 1. Build the plugin

```bash
./setup.sh
./build.sh --ue-root /path/to/UnrealEngine
```

`--ue-root` builds AirLib with Unreal Engine's bundled toolchain, which Ubuntu 24.04 and newer need (see [Install from Source on Linux](install_linux.md#build-hercules)). If you built before without `--ue-root`, delete `build_release/` first; CMake keeps the compiler a build folder was configured with. The build also produces the waypoint controllers in `build_release/output/bin/`.

## 2. Create the Blocks project

The repository holds the Blocks configuration and code, but not `Blocks.uproject` or its `Content/`. Copy them from Cosys-AirSim 5.2-v3.2, the Unreal Engine 5.2 release HERCULES builds on:

```bash
git clone --depth 1 --branch 5.2-v3.2 --filter=blob:none --sparse \
  https://github.com/Cosys-Lab/Cosys-AirSim /tmp/cosys-blocks
git -C /tmp/cosys-blocks sparse-checkout set Unreal/Environments/Blocks/Content
cp /tmp/cosys-blocks/Unreal/Environments/Blocks/Blocks.uproject Unreal/Environments/Blocks/
cp -r /tmp/cosys-blocks/Unreal/Environments/Blocks/Content Unreal/Environments/Blocks/
```

Then copy the freshly built plugin into the project:

```bash
(cd Unreal/Environments/Blocks && ./update_from_git.sh)
```

Rerun `update_from_git.sh` after every `./build.sh`.

## 3. Start the simulator

`ros2/settings/hero_blocks_team.json` declares the team. Drones spawn at the PlayerStart and 6 m to its right, and the Huskies 6 m ahead of each drone. Every robot carries a `front_center` camera (scene, planar and perspective depth, segmentation), `stereo_left`/`stereo_right` cameras, a 16-channel `LidarSensor1`, IMU, GPS, barometer and magnetometer. Keep the `Drone<N>`/`Husky<N>` names: the ROS bridges, run scripts and planner select vehicles by these prefixes.

Open the project with that settings file:

```bash
/path/to/UnrealEngine/Engine/Binaries/Linux/UnrealEditor \
  "$PWD/Unreal/Environments/Blocks/Blocks.uproject" \
  -settings="$PWD/ros2/settings/hero_blocks_team.json"
```

Answer **Yes** if the editor offers to build the `Blocks` module, and **No** if it offers to disable the AirSim plugin. When the editor has loaded, press **Play**. In Hero mode the simulator serves drones on RPC port 41451 and ground vehicles on 41452; its `ApiServerPort` setting does not apply. Press **Stop** and **Play** again to return the robots to their start positions.

## 4. Start the ROS 2 bridges

Build the ROS 2 workspace as described in the [ROS 2 wrapper](ros_cplusplus.md) page, then, from `HERCULES/ros2`:

```bash
source /opt/ros/jazzy/setup.bash   # or /opt/ros/lyrical/setup.bash
source install/setup.bash
ros2 launch airsim_ros_pkgs hercules_hero_team.launch.py
```

The launch file starts one bridge per RPC port, `hercules_uav_bridge` and `hercules_ugv_bridge`. Both publish below `/hercules_node/<Vehicle>/`, for example `/hercules_node/Husky1/lidar/points/LidarSensor1` and `/hercules_node/Drone2/front_center_Scene/image`. The drone bridge publishes `/clock` and offers `/hercules_node/<Drone>/takeoff`, `/land` and the `all_robots` services. Wait for `AirsimROSWrapper Initialized!` from both bridges.

To view all four robots, open the bundled RViz configuration from another sourced terminal:

```bash
rviz2 -d rviz2_configs/hercules_2UGVUAV.rviz
```

## 5. Record a trajectory for each robot

The recorders use the `hercules_cosysairsim` client from this repository, which needs NumPy and `rpc-msgpack`:

```bash
python3 -m venv herculesvenv
source herculesvenv/bin/activate
pip install numpy rpc-msgpack
```

Record one robot at a time, with the terminal focused:

```bash
python PythonClient/hero/record_drone_waypoints_teleop.py --vehicle Drone1
python PythonClient/hero/record_drone_waypoints_teleop.py --vehicle Drone2
python PythonClient/hero/record_ugv_waypoints_teleop.py --vehicle Husky1
python PythonClient/hero/record_ugv_waypoints_teleop.py --vehicle Husky2
```

| | Drone | Husky |
| --- | --- | --- |
| Start | **T** take off | — |
| Move | **W/S/A/D** body-frame velocity, **R/F** altitude, **J/L** yaw | **W/S** throttle, **A/D** steer |
| Stop | **Space** or **H** hover | **Space** brake |
| Recording on/off | **C** | **V** |
| Quit | **Q** | **Q** |

Each recording is written to `trajectory_data/<Vehicle>_trajectory.txt`, overwriting an earlier one; pass `--out <file>` to write elsewhere. The full key lists are in each script's header. Press **Stop** and **Play** in the editor before replaying, so every robot starts from its spawn point.

## 6. Replay the team

Start both controller sets; the UGV command runs in the background:

```bash
./UGVWaypointControl/run_UGVs_waypoints.sh 2 &
./DroneWaypointControl/run_drones_waypoints.sh 2
```

The drones take off, fly their recorded paths at 0.75 m/s and 4 m above the start, hover, and land. The Huskies track theirs at 0.29 m/s. These defaults are the speeds HERCULES' dataset collection uses. The scripts read trajectories from `trajectory_data/` and take these overrides:

| Setting | Example |
| --- | --- |
| Trajectory folder | `WAYPOINT_DIR=/path/to/trajectories` |
| Drone speed and altitude (NED, negative is up) | `WAYPOINT_VELOCITY=1.5 FLY_ALTITUDE=-6` |
| Fly each waypoint's recorded altitude | `USE_WAYPOINT_Z=true` |
| Return to the start before landing | `DISABLE_RETURN_HOME=false` |
| UGV count, speed and control rate | `./UGVWaypointControl/run_UGVs_waypoints.sh 2 0.3 20` |

To record the run, start `ros2 bag record -o hero_run -a` in another sourced terminal before replaying.

## What has been tested

The plugin build, the waypoint controllers and the ROS 2 bridges were exercised on Ubuntu 24.04 and 26.04 without a running Unreal Engine:

- `./setup.sh` and `./build.sh --ue-root` with Unreal Engine 5.2's toolchain build AirLib, rpclib and MavLinkCom with its clang 15 against its glibc 2.17 sysroot, and the waypoint controllers run on both releases. A libc++ built from LLVM 15 stood in for the engine's own.
- `hero_blocks_team.json` loads in HERCULES' settings parser as Hero mode with the `UGVPawn` Husky.
- Against a stand-in RPC server for the two Hero ports, the team launch file brings up both bridges with every robot's topics, and `DroneWaypointControl` completes its takeoff, path and landing sequence.

Building the plugin into Unreal Engine 5.2.1 and the robots' behaviour in the simulator have not been tested in this setup.
