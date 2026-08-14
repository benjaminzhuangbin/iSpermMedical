# Version 2.4 完成报告

## 目标

形成可用于 Data Manager 集成的稳定 Android-side Watchdog。

修正 2.3 现场误判：PC `adb devices=device` + QtScrcpy 正常时，Watchdog 因 localhost CNXN probe 失败反复 `FIX_ADB_PROTOCOL`，cooldown 到期后再 restart，导致投屏中断。

## 产品硬性要求（已实现）

1. adbd + :5555 LISTEN 且无明确 Android-side 故障 → **绝不**主动 restart。
2. `ESTABLISHED>0`（QtScrcpy/PC 在线）→ **绝不** restart adbd。
3. `ESTABLISHED=0` / CNXN / 内部 probe → **不是**故障，不 recovery。
4. Cooldown **不是**周期重启：恢复正常后立即结束 recovery 状态。
5. 长期运行不得主动打断正常投屏。

## 逻辑自检

| 检查项 | 结果 |
|--------|------|
| ESTABLISHED=0 不 recovery | PASS |
| ESTABLISHED>0 不 restart | PASS（硬守卫） |
| localhost CNXN 不触发 recovery | PASS（已移除） |
| 健康时 cooldown 到期不 re-restart | PASS（healthy 立即清 cooldown） |
| 健康 TCP ADB 不反复 restart | PASS |
| 3 次后不永久 disable | PASS |
| STOP_ADBD / BREAK_PORT inject | PASS |

## 编译 / 安装

见 README.md。产物：`release/watchdog`（armeabi-v7a）。install.bat 为 **CRLF**。
