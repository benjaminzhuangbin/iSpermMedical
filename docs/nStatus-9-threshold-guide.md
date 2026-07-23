# nStatus = -9 阀值说明书与调参指导

文档对应源码：`CountSpermMed.cpp`  
状态含义：**样本可能为非精子目标**

---

## 1. 结论先说

**是的，你给出的定义就是 `nStatus = -9` 的唯一赋值点**（全文件仅此一处）：

```cpp
if (SParaInput.dShapeRatio < 0.8 || nSampleType == 3 || nSampleType == 4)
{
    nStatus = -9;//样本可能为非精子目标
}
```

位置：约 L1902–L1905。

它不是“尺寸过小/过大”的直接判定，而是：

1. **输入期望长宽比**低于阀值，或  
2. **自动样本分类**判定为标粒/红细胞  

时，把结果标成“可能非精子”。

要兼容更小/更大的真实精子且不出现 `-9`，不能只改这一个 `0.8`，还要改上游的 `checkSampleType()` 分类窗，以及（如需正确检出）`updateSpermRange()` / `getImgContourInfor()` 的尺寸范围。

---

## 2. 完整逻辑链路（从输入到 -9）

```text
init()
  └─ 校验 dataIn->dShapeRatio >= 0.2（否则 nStatus=-1，不是-9）
  └─ SParaInput.dShapeRatio = dataIn->dShapeRatio

checkSampleType(img)  →  nSampleType ∈ {1,2,3,4,...}
  ├─ 1: 干涸精液 → 直接 nStatus=-12（不会走到-9）
  ├─ 2: 新鲜人精 → 继续活力分析
  ├─ 3: 标粒     ┐
  └─ 4: 红细胞   ┴─ 后续会触发 -9

updateSpermRange(nSampleType)  → 切换面积/长短轴模板

getSpermFeatureRange(...)  → 计数

进入“静止/异常结果”分支后：
  if (dShapeRatio < 0.8 || nSampleType==3 || nSampleType==4)
      nStatus = -9
  if (浓度过高)
      nStatus = -16   // 会覆盖 -9
```

触发 `-9` 的 **三个 OR 条件**：

| # | 条件 | 含义 |
|---|------|------|
| A | `SParaInput.dShapeRatio < 0.8` | 调用方输入的“期望目标长宽比”过低，算法认为按非精子模式跑 |
| B | `nSampleType == 3` | `checkSampleType` 判为标粒（约 3μm 圆粒类） |
| C | `nSampleType == 4` | `checkSampleType` 判为红细胞类目标 |

任一成立即 `-9`。

---

## 3. 阀值一览表（与 -9 直接/间接相关）

### 3.1 直接阀值（决定是否写 -9）

| 参数/表达式 | 当前值 | 位置 | 意义 |
|-------------|--------|------|------|
| `SParaInput.dShapeRatio` 下限 | **0.8** | L1902 / L1887 | 输入期望长宽比；`<0.8` 视为非精子模式并置 `-9` |
| `nSampleType` | **3 或 4** | L1902 | 自动分类为标粒/红细胞 → `-9` |

输入合法性（与 -9 无关，但相关）：

| 参数 | 阀值 | 位置 | 失败状态 |
|------|------|------|----------|
| `dataIn->dShapeRatio` | 必须 `>= 0.2` | L872 | `-1` 输入异常 |

### 3.2 上游分类阀值 `checkSampleType()`（决定是否变成 type 3/4）

基于轮廓统计得到的：

- `dAreaAve`：平均面积（pixel²）
- `dShapeRatio`：平均长轴/短轴

| 判定顺序 | 条件 | 结果 `nSampleType` | 后续状态 |
|----------|------|---------------------|----------|
| 1 | `(AreaAve>35 && Shape>2)` 或 `(AreaAve>40 && Shape>2.8)` 或 `(AreaAve>60 && Shape>2.5)` 或 `AreaAve>75` | **1 干涸人精** | **-12** |
| 2（人-备男） | `11 < AreaAve ≤ 35` 且 `1.15 ≤ Shape < 1.8` | **2 新鲜人精** | 正常继续（不因分类得 -9） |
| 3a | `Shape < 1.15` 且 `AreaAve > 50` | **4 红细胞** | **-9** |
| 3b | `Shape < 1.15` 且 `AreaAve ≤ 50` | **3 标粒** | **-9** |
| 4（兜底） | 其余（含偏椭圆但面积不在 11–35） | **3 其他→按标粒** | **-9** |

注释中保留的猪精旧窗（已注释）：`11 < Area ≤ 40` 且 `1.4 ≤ Shape < 2.6`。

**关键理解：**

- 想当“新鲜精子”处理，必须落在 **面积约 11–35** 且 **长宽比约 1.15–1.8**。
- 更小、更大、更圆、更细长，容易掉进 type 3/4 兜底 → **必然 -9**。
- 这与目标“真实尺寸”是否合理无关，纯分类窗问题。

### 3.3 尺寸模板 `updateSpermRange()`（分类后的检出范围）

| 类型 | AreaMin/Max/Ave | Major Min/Max/Ave | Minor Min/Max/Ave | ShapeRatio |
|------|-----------------|-------------------|-------------------|------------|
| 1 干涸 | 40 / 110 / 75 | 9 / 34 / 21 | 3.4 / 9 / 6.5 | 3.3 |
| 2 新鲜 | **11 / 22 / 16.8** | 3.2 / 7 / 5.08 | 2 / 4.8 / 3.95 | **1.23** |
| 3 标粒 | 5 / 150 / 15 | 2.5 / 9.3 / 4.4 | 2.8 / 4.8 / 3.8 | 1.15 |
| 4 红细胞 | 30 / 150 / 62 | 6 / 11.7 / 9.7 | 2.8 / 9.6 / 7.8 | 1.16 |

### 3.4 轮廓筛选（影响能否被统计，间接影响分类）

| 常量/系数 | 值 | 用途 |
|-----------|-----|------|
| `dMinArea` | **8** px² | 初始筛选（type=7）最小面积 |
| `dMaxArea` | **200** px² | 初始筛选最大面积 |
| ABCD 检出 | `[0.7*AreaMin, 2*AreaMax]` | 正式计数面积窗 |
| 短轴剔除 | `MinLen < 0.4*MinorAve` 且 `Area < 0.4*AreaAve` | 过小目标丢弃 |
| 形状接受 | `[ShapeRatio, 4*ShapeRatio]` | 部分绘制/筛选逻辑 |

---

## 4. 两个容易混淆的 `dShapeRatio`

| 名字 | 来源 | 作用 |
|------|------|------|
| `SParaInput.dShapeRatio` / `dataIn->dShapeRatio` | **外部输入** | 与 **0.8** 比较；可直接触发 `-9` |
| `pSSpermRange.dShapeRatio`（分类内） | **图像统计** | 决定 `nSampleType`；间接触发 `-9` |
| `pSAveSpermRange->dShapeRatio` | 模板/更新后 | 后续轮廓筛选的形状下限 |

改输入 `dShapeRatio` 到 `≥0.8` 只能消掉条件 A；若图像被判成 type 3/4，仍会 `-9`。

---

## 5. 如何兼容更小 / 更大精子且避免 -9

### 5.1 先排查是哪条路径触发的

建议临时打日志（或断点）：

```cpp
// 在 L1902 前
printf("dbg -9 path: inputShape=%.3f sampleType=%d areaAve=? shapeAve=?\n",
       SParaInput.dShapeRatio, nSampleType);
```

在 `checkSampleType` 里打印 `pSSpermRange.dAreaAve`、`pSSpermRange.dShapeRatio`。

| 现象 | 原因 | 优先改什么 |
|------|------|------------|
| `inputShape < 0.8`，`sampleType==2` | 输入参数 | 把调用方 `dShapeRatio` 调到 ≥0.8（人精常用 ~1.2） |
| `sampleType==3/4` | 分类窗不匹配真实精子 | 扩 `checkSampleType` 新鲜人精窗 |
| 分类正确但仍漏检 | 尺寸模板过窄 | 扩 `paraRangeTemp2` 与 `dMinArea`/`dMaxArea` |

### 5.2 只改输入参数（不改代码）

- 人精模式：保证 **`dataIn->dShapeRatio ≥ 0.8`**（建议 **1.2～1.5**，与 `paraRangeTemp2` 的 1.23 一致）。
- 若故意做标粒/红细胞质控，`-9` 是预期行为，不要强行消掉。

### 5.3 改代码：扩大“新鲜人精”分类窗（消除误报 -9 的主手段）

当前（L2040）：

```cpp
else if ((pSSpermRange.dAreaAve <= 35 && pSSpermRange.dAreaAve > 11)
      && (pSSpermRange.dShapeRatio >= 1.15 && pSSpermRange.dShapeRatio < 1.8))
{
    nSampleType = 2;//新鲜人精
}
```

**建议按物种/放大率重标定**（示例，需用真实样本均值±标准差验证）：

| 目标 | AreaAve 建议窗 | ShapeRatio 建议窗 |
|------|----------------|-------------------|
| 更小精子（或更高放大率像素更大？注意：更高放大→像素面积更大） | 例如 `6～40` | 例如 `1.05～2.2` |
| 更大精子 | 例如 `11～55` | 例如 `1.10～2.4` |
| 偏圆头 | 降低 Shape 下限到 `1.05` | 避免掉进 `<1.15 → type3/4` |
| 偏细长 | 提高 Shape 上限到 `2.2～2.5` | 避免兜底 type3；注意别与干涸 type1 冲突 |

**改码注意：**

1. type1（干涸）条件更“大而细长”，扩 type2 时不要与 type1 重叠。  
2. 兜底 `else → type3` 会把“略出窗的真精子”打成 `-9`；扩窗或增加物种分支比硬删 `-9` 更安全。  
3. 猪精可恢复注释中的窗，或按 `dShapeRatio` 输入切换物种模板。

示例改法（示意）：

```cpp
// 更宽容的新鲜精子窗（请用标定数据替换数字）
else if ((pSSpermRange.dAreaAve <= 50 && pSSpermRange.dAreaAve > 6)
      && (pSSpermRange.dShapeRatio >= 1.05 && pSSpermRange.dShapeRatio < 2.2))
{
    nSampleType = 2;
}
```

### 5.4 改代码：扩大新鲜人精尺寸模板（保证小/大目标被检出）

`updateSpermRange()` 中 `paraRangeTemp2`（L7571）：

```cpp
ParaRange paraRangeTemp2 = {11, 22, 16.8, 3.2, 7, 5.08, 2, 4.8, 3.95, 1.23};
```

| 想兼容 | 建议调哪些字段 |
|--------|----------------|
| 更小 | 降低 `dAreaMin`（如 11→6～8）、`dMajorLengthMin`、`dMinorLengthMin`；必要时降低全局 `dMinArea`（8→4～6） |
| 更大 | 提高 `dAreaMax`（22→35～50）、长/短轴 Max；必要时提高 `dMaxArea`（200） |
| 形状更宽 | 调整模板 `dShapeRatio`（如 1.23→1.1），并同步检查 `4*dShapeRatio` 上限逻辑 |

ABCD 检出面积为 `[0.7*AreaMin, 2*AreaMax]`，改模板会自动放大/缩小检出窗。

### 5.5 改代码：直接放宽 / 关闭 -9（仅调试或特殊产品需求）

```cpp
// 原：
if (SParaInput.dShapeRatio < 0.8 || nSampleType == 3 || nSampleType == 4)

// 方案1：降低输入阀值（如兽用更圆的目标）
if (SParaInput.dShapeRatio < 0.5 || nSampleType == 3 || nSampleType == 4)

// 方案2：仅保留分类触发，不看输入
if (nSampleType == 3 || nSampleType == 4)

// 方案3：完全不报 -9（仍可走静止叠加分支）——慎用，会把标粒/红细胞当精子流程下游处理
// 删除或注释该赋值块
```

**不推荐**只删 `-9` 却不改分类：`nSampleType==3/4` 时仍走 `getStdParticleNum` 等标粒路径，计数语义会错。

### 5.6 静止分支入口（L1887）也含同样阀值

```cpp
if (nStatus != 1 || SParaInput.dShapeRatio < 0.8
    || nSampleType == 3 || nSampleType == 4
    || (nTotalSpermNum >100/(dAlphaDens+EPSINON)))
```

这里决定是否走“静止叠加/非完整活力分析”。若只改 L1902 而不改 L1887，行为会不一致。两处 **0.8 / type3 / type4** 应同步修改。

---

## 6. 推荐调参步骤（实操）

1. 对目标物种采 8～20 份样本，统计每份 `AreaAve`、`ShapeAve`（可复用 `checkSampleType` 内统计）。  
2. 画分布，设定 type2 窗：覆盖真精子 P5～P95，并与 type1、type3/4 分离。  
3. 用中位数更新 `paraRangeTemp2`。  
4. 确认调用方 `dShapeRatio ≥ 0.8`（或同步下调两处 0.8）。  
5. 回归：真精子不再 `-9`；标粒/红细胞质控仍应 `-9`（若产品仍需要该提示）。  
6. 检查过浓样本：`-16` 会覆盖 `-9`。

---

## 7. 与相邻状态码对照

| nStatus | 含义 | 与 -9 关系 |
|---------|------|------------|
| -1 | 输入参数异常（含 `dShapeRatio<0.2`） | 更早失败 |
| -9 | 可能非精子（标粒/红细胞/输入形状过低） | 本文 |
| -12 | 干涸/异常样本（type1） | 分类后直接 break，不到 -9 |
| -16 | 浓度过高 | 同分支内可覆盖 -9 |

---

## 8. 一句话回答你的问题

- **定义**：你贴的那段就是完整的 `-9` 赋值逻辑，判断正确。  
- **控制阀值**：直接阀值是 **输入 `dShapeRatio < 0.8`** 与 **`nSampleType∈{3,4}`**；间接阀值是 `checkSampleType` 的面积/形状窗（新鲜人精 `11–35` & `1.15–1.8` 等）。  
- **兼容更小/更大精子**：优先扩大 `checkSampleType` 的 type2 窗 + `updateSpermRange` 的 `paraRangeTemp2`（及必要时 `dMinArea`/`dMaxArea`）；仅当产品允许时再改 `0.8` 或去掉 `-9` 赋值，且 **L1887 与 L1902 同步**。
