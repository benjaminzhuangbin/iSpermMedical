# Version 2.4 完成报告

## 目标

形成可用于 Data Manager 集成的稳定 Android-side Watchdog。

修正 2.3 现场误判：PC `adb devices=device` + QtScrcpy 正常时，Watchdog 因 localhost CNXN probe 失败反复 `FIX_ADB_PROTOCOL`。

## 逻辑自检

| 检查项 | 结果 |
|--------|------|
| ESTABLISHED=0 不 recovery | PASS（health/recovery 均忽略） |
| localhost CNXN 不触发 recovery | PASS（已移除 probe 与 CASE D） |
| 健康 TCP ADB 不反复 restart | PASS（service_ok 直接 return） |
| 3 次后不永久 disable | PASS（RATE_LIMIT + 窗口归零） |
| STOP_ADBD / BREAK_PORT inject | PASS（保留） |

## 修改要点

- 删除 `adb_proto.c` 与 CNXN 恢复路径
- `TCP_HEALTH=OK` + `ADB_HEALTH=OK` 即使 `ESTABLISHED=0`
- 新增 `CLIENT_STATE=CONNECTED|NO_CLIENT`
- install.bat **CRLF** + STARTING 横幅

## 编译 / 安装

见 README.md。产物：`release/watchdog`（armeabi-v7a）。
