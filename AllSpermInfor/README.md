# AllSpermInfor - 精子十字形态位置图绘制工具

本文件夹包含用于解析精子分析数据（如 `AllSpermInfor-100023.csv`）并在 1280×720 画布上绘制红色十字形态图的脚本及生成结果。

---

## 1. 对应数据说明

CSV 文件采用的标准参数定义如下：

| 列字段 | 意义 | 绘制映射 |
| :--- | :--- | :--- |
| `i+1` | 精子/颗粒编号 (从 1 开始) | 标记点序号 |
| `dPosX` | X 轴中心坐标 (像素) | 对应画布宽度（1280 像素范围内） |
| `dPosY` | Y 轴中心坐标 (像素) | 对应画布高度（720 像素范围内） |
| `dMajAxsLen` | 长轴长度 (像素) | **十字的竖线长度**，以 `dPosY` 为中心垂直延伸 |
| `dMinAxsLen` | 短轴长度 (像素) | **十字的横线长度**，以 `dPosX` 为中心水平延伸 |
| `dShape` | 长宽比 (dMajAxsLen / dMinAxsLen) | 形态比率 |
| `dArea` | 颗粒投影面积 (像素) | 面积 |
| `dCircularity`| 圆度 (4π*Area / Perimeter²) | 圆度指标 |

---

## 2. 文件列表

- **`draw_sperm_crosses.py`**: 核心 Python 绘制程序。
  - **零外部依赖**：纯 Python 标准库原生实现（内置 `zlib` 生成标准 24-bit PNG，以及标准 BMP）。无需 `pip install pillow` 或 `opencv`，在任何标准 Python 3 环境下均可直接运行！
  - 自动输出黑色高对比背景和白色打印背景两套图像（包含 PNG 与 BMP 格式）。
- **`run_draw.bat`**: Windows 一键双击运行脚本。
- **`AllSpermInfor-100023.csv`**: 本次分析的 627 颗颗粒数据文件。
- **`sperm_cross_map_black_bg.png`**: 1280×720 黑色显微视野背景的红色十字图（高对比度推荐）。
- **`sperm_cross_map_white_bg.png`**: 1280×720 白色背景的红色十字图（适合打印或插入报告）。
- **`sperm_cross_map_black_bg.bmp` / `sperm_cross_map_white_bg.bmp`**: 24 位未压缩标准位图，支持直接用画图或医学图像软件打开。

---

## 3. 运行方法

### 方式 A: Windows 用户直接双击
直接双击运行本目录下的 `run_draw.bat` 即可自动执行并生成图像。

### 方式 B: 命令行运行

```bash
# 进入当前目录
cd AllSpermInfor

# 默认运行 (自动加载 AllSpermInfor-100023.csv，输出 1280x720 图像)
python draw_sperm_crosses.py

# 指定线条宽度为 2 像素（让十字更醒目粗壮）
python draw_sperm_crosses.py --linewidth 2

# 指定其它 CSV 文件
python draw_sperm_crosses.py --csv /path/to/another.csv --out result_map
```

---

## 4. 统计结果示例 (AllSpermInfor-100023.csv)
- **总检测颗粒数**：627 个
- **X 坐标分布**：`2.0 px ~ 1276.5 px`
- **Y 坐标分布**：`1.3 px ~ 717.8 px`
- **竖线长轴 (dMajAxsLen)**：最小 `2.1 px`，最大 `29.0 px`，平均值 `7.73 px`
- **横线短轴 (dMinAxsLen)**：最小 `1.9 px`，最大 `10.6 px`，平均值 `3.89 px`
