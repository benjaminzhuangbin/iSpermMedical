# Version 2.3 完成报告

## 目标

修复真实现场故障：ping OK + TCP 5555 OK + Windows `adb devices` 显示 `offline`。

2.2 仅用 `ADBD=YES` + `PORT5555=YES` 判定健康，不足以覆盖 ADB 协议/会话失效。

## 修改的源文件

| 文件 | 变更 |
|------|------|
| `include/adb_proto.h` / `src/adb_proto.c` | **新增** localhost CNXN 协议探测 + inject 标志 |
| `include/health.h` / `src/health.c` | `ADB_HEALTH` / `ADB_PROTOCOL_FAULT` / `SESSION_STALE` |
| `include/recovery.h` / `src/recovery.c` | CASE D；窗口 rate-limit；每次恢复后 cooldown；无永久关闭 |
| `include/config.h` / `src/config.c` | 版本 2.3；`adb_proto_*` 配置项 |
| `include/status.h` / `src/status.c` | 状态首行 `Nexus ADB Watchdog 2.3`；`ADB_HEALTH=` |
| `src/watchdog.c` | Layer 4 探测调度；session stale |
| `src/inject.c` | `ADB_PROTOCOL_FAULT` |
| `src/logger.c` / `src/main.c` | 版本与日志字段 |
| `CMakeLists.txt` | 加入 `adb_proto.c` |
| `watchdog.conf` / bats / README | 2.3 文档与脚本 |

## 逻辑要点

1. **Layer 4**：连接 `127.0.0.1:5555`，发送 ADB `CNXN`，期望 `CNXN`/`AUTH`。
2. **`ESTABLISHED=0` 不是故障** → `TCP_HEALTH=NO_CLIENT`。
3. **Rate limit**：`max_restart` / `restart_window_sec`；窗口结束 count 归零；**永不永久关闭 recovery**。
4. **Cooldown**：每次恢复后等待 `cooldown_sec`。
5. **Inject**：`ADB_PROTOCOL_FAULT` 粘性标志，恢复后清除。

## 编译

```bash
# NDK armeabi-v7a / android-22
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=armeabi-v7a -DANDROID_PLATFORM=android-22 \
  -DANDROID_STL=none -DCMAKE_BUILD_TYPE=Release
cmake --build build-android
# 产物: release/watchdog
```

Windows: `build.bat`

## 安装 / 启动

```bat
install.bat   # 有 [1/6]…[OK] 输出；watchdog -h 显示 2.3
start.bat
status.bat    # 首行 Nexus ADB Watchdog 2.3
```

## 测试矩阵

| 测试 | 预期 | 本环境实际结果 |
|------|------|----------------|
| Host health classify | NO_CLIENT / protocol FAULT 等 | **PASS** (`build_host_check.sh`) |
| Host CNXN probe 无监听端口 | 返回 0（FAULT） | **PASS** |
| NDK ARM 编译 | `release/watchdog` ARM ELF | **PASS** |
| 二进制字符串 | `2.3` / `ADB_PROTOCOL_FAULT` / `FIX_ADB_PROTOCOL` | **PASS** |
| TEST A–F on RK3288 | 见 README | **需真机**（本 cloud 环境无设备） |

真机验收：

```bat
test_case_a.bat   REM STOP_ADBD
test_case_b.bat   REM BREAK_PORT
test_case_d.bat   REM ADB_PROTOCOL_FAULT
REM TEST C: 无 PC client → ESTABLISHED=0 不得 recovery
REM TEST E/F: 3 次 recovery 后 RECOVERY_ENABLED 仍为 1；窗口后 count 归零
```
