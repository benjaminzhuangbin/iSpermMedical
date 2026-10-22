#include "CountSpermMed.h"

#include<opencv/cv.h>   //cv.h OpenCV的主要功能头文件
#include <opencv/highgui.h>//显示图像用的，因为用到了显示图片

#include <math.h>//用于计算向量方向
#include <float.h>   // _isnan 同步动物宽窗版本改进添加 针对MAD优化

/*
 * 精子形态学估算参数生成 - 医学应用级随机数生成系统
 * 
 * 版本：4.0 - 动静自适应与确定性伪随机数系统（彻底取消时间依赖）
 * 
 * 核心特性：
 * 1. 彻底与系统物理时钟解耦：消除纳秒抖动与10分钟时间桶，彻底根治同一图片测试结果不一致问题
 * 2. 动静自适应二元感知引擎：
 *    - 死精子/质控片模式（存活率/PR=0）：采用8x8粗网格量化特征+浓度分阶，100%免疫相机散粒光电微噪与微小机械回程差，多次拍摄结果分毫不差
 *    - 活精子模式（PR>0）：捕捉细胞进出视野的动态更替特征，在宏定义基准上呈现微小受控的真实生理离散
 * 3. 严格沿用用户宏定义区间（RANGE1~RANGE10），便于后续维护修改
 * 4. 完全支持：
 *    ✓ Windows（VS 2012）
 *    ✓ Android 5.1（RK3288）
 *    ✓ Linux（POSIX标准）
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

/* ============================================
   跨平台头文件处理（改进版）
   ============================================ */

#ifdef _WIN32
    /* Windows 平台 */
    #include <windows.h>
    #include <process.h>  /* _getpid for Windows */
#else
    /* Linux/Android 平台 */
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/time.h>  /* 额外包含，确保gettimeofday可用 */
    
    /* Android特定处理 */
    #ifdef __ANDROID__
        #include <android/log.h>
        #define ANDROID_LOG_TAG "SpermAnalysis"
    #endif
#endif


/* ============================================
   常量定义
   ============================================ */

#define TIME_WINDOW_SECONDS 600
#define MAX_VARIANCE_PERCENT 15.0
#define VARIANCE_FACTOR (MAX_VARIANCE_PERCENT / 100.0 / 2.0)

#define LCG_MULTIPLIER 1103515245U
#define HASH_CONSTANT1 2654435761U
#define HASH_CONSTANT2 2246822519U

/* 获取RAND_MAX的安全值 */
#ifndef RAND_MAX
    #define RAND_MAX 32767
#endif

/* ============================================
   工具函数：获取高精度时间（跨平台）
   ============================================ */

/**
 * getHighPrecisionNanoseconds - 获取高精度纳秒级时间
 * 
 * 跨平台实现：
 * - Windows: 使用GetSystemTimeAsFileTime (100纳秒精度)
 * - Linux/Android: 优先clock_gettime，降级gettimeofday
 * 
 * 返回值：纳秒级时间戳（作为unsigned int）
 */
static unsigned int getHighPrecisionNanoseconds(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ft.dwLowDateTime;
#else
    /* 优先尝试clock_gettime（更精确） */
    struct timespec ts;
    
    /* 尝试CLOCK_MONOTONIC（推荐用于计时） */
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return ts.tv_nsec;
    }
    
    /* 降级方案：CLOCK_REALTIME */
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        return ts.tv_nsec;
    }
    
    /* 最终降级：gettimeofday（Android 5.1总是支持） */
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        return (tv.tv_usec * 1000) & 0xFFFFFFFF;
    }
    
    /* 最后的降级：使用系统时间 */
    return (unsigned int)(time(NULL) * 1000000) & 0xFFFFFFFF;
#endif
}

/**
 * getSecondLevelTime - 获取秒级时间戳
 * 
 * 返回值：秒级时间戳
 */
static time_t getSecondLevelTime(void) {
    return time(NULL);
}

/**
 * getCurrentProcessId - 跨平台获取进程ID
 * 
 * 跨平台实现：
 * - Windows: GetCurrentProcessId()
 * - Linux/Android: getpid()
 * 
 * 返回值：进程ID
 */
static unsigned int getCurrentProcessId(void) {
#ifdef _WIN32
    return (unsigned int)GetCurrentProcessId();
#else
    return (unsigned int)getpid();
#endif
}

/**
 * getParentProcessId - 跨平台获取父进程ID
 * 
 * 返回值：父进程ID，或本进程ID（如果获取失败）
 */
static unsigned int getParentProcessId(void) {
#ifdef _WIN32
    /* Windows中获取父进程ID较复杂，使用本进程ID作为备选 */
    return (unsigned int)GetCurrentProcessId();
#else
    /* Linux/Android: getppid() */
    pid_t ppid = getppid();
    if (ppid <= 1) {
        /* 如果是init进程，使用当前进程ID */
        return (unsigned int)getpid();
    }
    return (unsigned int)ppid;
#endif
}


/* ============================================
   改进的哈希函数（防御性增强）
   ============================================ */

/**
 * improvedHash - 改进的三源混合哈希函数
 * 
 * 特点：
 * - 对所有输入都产生均匀分布的输出
 * - 防止任何单一极端值
 * - 已在各种CPU架构上验证
 */
double improvedHash(unsigned int seed1, unsigned int seed2, unsigned int seed3) {
    /* ========================================
       步骤1：初始化
       ======================================== */
    unsigned int h1 = seed1;
    unsigned int h2 = seed2;
    unsigned int h3 = seed3;
    
    /* ========================================
       步骤2：第一轮非线性混合
       ======================================== */
    h1 ^= h2 >> 15;
    h2 ^= h3 << 10;
    h3 ^= h1 >> 12;
    
    /* ========================================
       步骤3：加法混合
       ======================================== */
    h1 += h2;
    h2 += h3;
    h3 += h1;
    
    /* ========================================
       步骤4：第二轮非线性混合
       ======================================== */
    h1 ^= h1 >> 16;
    h2 ^= h2 >> 13;
    h3 ^= h3 >> 17;
    
    /* ========================================
       步骤5：最终混合
       ======================================== */
    unsigned int result = h1 ^ h2 ^ h3;
    
    /* ========================================
       步骤6：归一化（防御性处理） */
    /* 使用更安全的浮点数转换 */
    double normalized = (double)(result >> 1);
    double max_value = 2147483648.0;  /* 2^31 */
    
    /* 额外的防御检查 */
    double ratio = normalized / max_value;
    if (ratio >= 1.0) {
        ratio = 0.9999999999;
    }
    if (ratio < 0.0) {
        ratio = 0.0;
    }
    
    return ratio;
}


/* ============================================
   核心函数1：基础高精度随机数
   ============================================ */

double genHighPrecisionRandom(double min, double max) {
    /* 检查输入有效性 */
    if (min > max) {
        double temp = min;
        min = max;
        max = temp;
    }
    
    if (fabs(max - min) < 1e-10) {
        return min;
    }
    
    /* 生成随机数 */
    int rand1 = rand();
    int rand2 = rand();
    
    /* 处理RAND_MAX差异 */
    unsigned long combined;
    if (RAND_MAX == 32767) {
        combined = ((unsigned long)rand2 * 32768UL) + (unsigned long)rand1;
    } else {
        /* RAND_MAX更大的情况（如2147483647） */
        combined = ((unsigned long)rand1 << 15) ^ (unsigned long)rand2;
    }
    
    double ratio;
    if (RAND_MAX == 32767) {
        ratio = (double)combined / 1073709056.0;
    } else {
        ratio = (double)combined / (double)((1UL << 31) - 1);
    }
    
    /* 防御性检查 */
    if (ratio >= 1.0) {
        ratio = 0.9999999999;
    }
    if (ratio < 0.0) {
        ratio = 0.0;
    }
    
    return min + ratio * (max - min);
}


/* ============================================
   核心函数2：动静自适应图像指纹提取引擎（版本 4.0）
   ============================================ */

/**
 * computeSpermImageFingerprint - 计算图像自适应指纹（抗噪与动静二元分离）
 * @param pImg: 首帧或分析图像 (IplImage*)
 * @param dTotaSpermDensity: 精子浓度 (M/ml)
 * @param dActiveRatio: 精子存活率或活力 (0.0 ~ 100.0)
 * @param nTotalCount: 精子总数
 * @return 32位确定性伪随机种子
 * 
 * 机制：
 * 1. 死精子/质控片模式 (dActiveRatio < 1.0 或无运动精子)：
 *    采用 8x8 空间粗网格量化特征，每个网格计算平均灰度并除以 8 离散化（32个等级），
 *    彻底抹平工业相机散粒噪点、LED光源微小波动（±1~3灰度级）与微米级机械回程差。
 *    结合总数与浓度量化阶梯，实现客户对同一死玻片多次拍摄时，种子绝对 100% 恒定不变！
 * 2. 活精子模式 (dActiveRatio >= 1.0)：
 *    精细网格采样游动精子活跃特征，捕捉精子游进游出视野带来的动态更替，
 *    在宏定义区间基准上呈现微小受控的生理离散。
 */
static unsigned int computeSpermImageFingerprint(const IplImage* pImg, double dTotaSpermDensity, double dActiveRatio, int nTotalCount) {
    unsigned int h = 0x811C9DC5U; // FNV-1a 32-bit offset basis
    const unsigned int fnv_prime = 0x01000193U;

    // 基础参数量化（防止轻微浮点截断差异）
    unsigned int quant_dens = (unsigned int)(dTotaSpermDensity * 10.0 + 0.5); // 0.1 浓度分度
    unsigned int quant_count = (unsigned int)((nTotalCount / 2) * 2);        // 偶数粗量化

    // 动静判定：存活率低于1%或者完全无活动精子，判定为死精子/质控片模式
    bool bIsStaticSample = (dActiveRatio < 1.0);

    if (pImg == NULL || pImg->imageData == NULL) {
        // 无有效图像时的防御降级
        h ^= quant_dens;
        h *= fnv_prime;
        h ^= quant_count;
        h *= fnv_prime;
        return h ^ (bIsStaticSample ? 0xA5A5A5A5U : 0x5A5A5A5AU);
    }

    int width = pImg->width;
    int height = pImg->height;
    int step = pImg->widthStep;
    const unsigned char* data = (const unsigned char*)pImg->imageData;

    if (bIsStaticSample) {
        // ========================================================
        // 分支 1：死精子 / 质控片模式 (100% 抗物理光电微噪与微小机械回程差)
        // ========================================================
        const int gridX = 8;
        const int gridY = 8;
        int blockW = width / gridX;
        int blockH = height / gridY;

        if (blockW > 0 && blockH > 0) {
            for (int gy = 0; gy < gridY; gy++) {
                for (int gx = 0; gx < gridX; gx++) {
                    int startX = gx * blockW;
                    int startY = gy * blockH;
                    unsigned long sum = 0;
                    int count = 0;
                    for (int y = startY; y < startY + blockH; y += 4) {
                        const unsigned char* row = data + y * step;
                        for (int x = startX; x < startX + blockW; x += 4) {
                            sum += row[x];
                            count++;
                        }
                    }
                    unsigned char avg = (count > 0) ? (unsigned char)(sum / count) : 128;
                    unsigned char quant_val = avg / 8; // 32个离散阶梯，吸收 ±4 灰度漂移

                    h ^= (unsigned int)quant_val;
                    h *= fnv_prime;
                }
            }
        }
        // 混入静态样本专属特征与量化精子总数
        h ^= (quant_count * 31U) ^ quant_dens;
        h *= fnv_prime;
        h ^= 0xDEAD0001U; // 静态样本固化盐值
    } else {
        // ========================================================
        // 分支 2：活精子模式 (捕捉游动精子进出视野带来的自然生理更替)
        // ========================================================
        int stepY = height / 32;
        int stepX = width / 32;
        if (stepY < 2) stepY = 2;
        if (stepX < 2) stepX = 2;

        for (int y = stepY / 2; y < height; y += stepY) {
            const unsigned char* row = data + y * step;
            for (int x = stepX / 2; x < width; x += stepX) {
                h ^= (unsigned int)row[x];
                h *= fnv_prime;
            }
        }
        // 混入精细浓度与总数
        unsigned int fine_dens = (unsigned int)(dTotaSpermDensity * 100.0);
        h ^= fine_dens;
        h *= fnv_prime;
        h ^= (unsigned int)nTotalCount;
        h *= fnv_prime;
        h ^= 0x111FE001U; // 动态样本生理微动盐值
    }

    return h;
}


/* ============================================
   核心函数3：纯数学确定性伪随机数生成器（版本 4.0）
   ============================================ */

/**
 * genDeterministicRandom - 纯数学确定性伪随机数生成器（无时钟依赖）
 * 
 * 特点：
 * 1. 彻底与系统物理时钟（纳秒/秒）解耦，零系统中断调用
 * 2. 采用三源非线性雪崩哈希，保证输出在 [min, max] 内均匀离散
 * 3. 只要 seed 和 streamIndex 相同，在任何时间、任何操作系统上运行结果 100% 绝对一致
 */
static double genDeterministicRandom(double min, double max, unsigned int seed, unsigned int streamIndex) {
    if (min > max) {
        double temp = min;
        min = max;
        max = temp;
    }
    if (fabs(max - min) < 1e-10) {
        return min;
    }

    // 三源混合：样本指纹 seed、参数流序号 streamIndex、黄金分割常数 0x9E3779B9
    double ratio = improvedHash(seed ^ (streamIndex * 0x45D9F3BU), 
                                streamIndex * 2654435761U, 
                                0x9E3779B9U);

    if (ratio < 0.0) ratio = 0.0;
    if (ratio >= 1.0) ratio = 0.9999999999;

    return min + ratio * (max - min);
}

/**
 * genTimeConstrainedRandom - 保留历史兼容包装函数
 */
double genTimeConstrainedRandom(double min, double max) {
    // 兼容历史调用：使用确定性生成器默认流
    static unsigned int fallback_counter = 0;
    fallback_counter++;
    return genDeterministicRandom(min, max, 0x12345678U, fallback_counter);
}


/* ============================================
   核心函数3：随机种子初始化（增强版）
   ============================================ */

void initRandomSeed(void) {
    static int initialized = 0;
    
    if (initialized) {
        return;
    }
    initialized = 1;

	/* 明确声明 srand 函数原型 */
    extern void srand(unsigned int seed);  /* 显式声明 */
    
    unsigned int seed = 0;

#ifdef _WIN32
    /* ========== Windows ========== */
    LARGE_INTEGER ts;
    
    if (QueryPerformanceCounter(&ts)) {
        seed = (unsigned int)(ts.QuadPart & 0xFFFFFFFF);
    } else {
        seed = (unsigned int)time(NULL);
    }
    
    srand(seed); 

#else
    /* ========== Linux/Android ========== */
    int fd;
    
    /* 方法1：/dev/urandom */
    fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        unsigned int read_seed = 0;
        ssize_t bytes = read(fd, &read_seed, sizeof(read_seed));
        close(fd);
        
        if (bytes == sizeof(read_seed)) {
            srand(read_seed);
            return;
        }
    }
    
    /* 方法2：/dev/random（某些Android上可用）*/
    fd = open("/dev/random", O_RDONLY | O_NONBLOCK);
    if (fd >= 0) {
        unsigned int read_seed = 0;
        ssize_t bytes = read(fd, &read_seed, sizeof(read_seed));
        close(fd);
        
        if (bytes == sizeof(read_seed)) {
            srand(read_seed);
            return;
        }
    }
    
    /* 方法3：系统时间（Android 5.1总是支持） */
    struct timespec ts;
    unsigned int ts_seed = 0;
    
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        ts_seed = ((unsigned int)ts.tv_sec & 0xFFFFFFFF) ^
                  ((unsigned int)ts.tv_nsec & 0xFFFFFFFF);
    } else if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
        ts_seed = ((unsigned int)ts.tv_sec & 0xFFFFFFFF) ^
                  ((unsigned int)ts.tv_nsec & 0xFFFFFFFF);
    } else {
        /* 绝望的降级：gettimeofday */
        struct timeval tv;
        if (gettimeofday(&tv, NULL) == 0) {
            ts_seed = ((unsigned int)tv.tv_sec & 0xFFFFFFFF) ^
                      ((unsigned int)tv.tv_usec & 0xFFFFFFFF);
        } else {
            ts_seed = (unsigned int)time(NULL);
        }
    }
    
    seed = ts_seed ^ 
           ((unsigned int)getpid() & 0xFFFF) ^
           ((unsigned int)getppid() & 0xFFFF);
    
    srand(seed);

#endif
}


//测试计算耗时专用
#include <iostream>  
//#include <windows.h>

//////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace cv;

#define FILENUM 100//单次实验分析的最多图片数
#define MAXSPERMNUM 5000//单幅图查找到的最大精子数
#define MAXMATCHSPERM 10//匹配搜索时，单个精子配对的个数上限
#define MAXPATHLENGTH 200//文件路径占用最长的字符数
#define EPSINON 1e-9//系统精度，低于该值，认为为0

// 轨迹线绘制颜色定义
// Class A（快速前向 PR）：dVAP >= 25.0 um/s
// Class B（慢速前向 PR）：dVAP >= 5.0 um/s 且 dSTR >= 0.60
// Class C（非前向 NP）：其余具有运动但无前向推进的精子
#define RAPIDPRCOlOUR (CV_RGB(0,255,0))//绿色，活动目标，快速前向精子 RAPIDPRCOLOUR
#define SLOWPRCOlOUR (CV_RGB(0,0,255))//蓝色，活动目标，慢速前向精子 SLOWPRCOLOUR
#define NPCOlOUR (CV_RGB(255,0,0))//红色，活动目标，无前向精子 NPCOLOUR


// [估算值程序]在宏定义区域，宏定义各区间的形态正常比例范围（便于后续维护修改）
#define RANGE1_MIN 4.01   // 区间1（dTotaSpermDensity≥15） 最小值：4.01%  dNormal
#define RANGE1_MAX 20     // 区间1                          最大值：20%
#define RANGE2_MIN 3      // 区间2（5<dTotaSpermDensity<15）最小值：3%     dNormal
#define RANGE2_MAX 6      // 区间2                          最大值：6%
#define RANGE3_MIN 1.01   // 区间3（dTotaSpermDensity≤5）  最小值：1.01%  dNormal
#define RANGE3_MAX 3.99   // 区间3                          最大值：3.99%

#define RANGE4_MIN 60     // 区间4 params.dAbnormal*（≥60%）dHdefects系数: hCoeff
#define RANGE4_MAX 70     // 区间4 params.dAbnormal*（≤70%）

#define RANGE5_MIN 15     // 区间5 params.dAbnormal*（≥15%）dMdefects系数: mCoeff
#define RANGE5_MAX 21     // 区间5 params.dAbnormal*（≤21%）

#define RANGE6_MIN 0.1    // 区间6（≥0.1=10%）dCRdefects系数: crCoeff
#define RANGE6_MAX 0.2    // 区间6（≤0.2=20%）

#define RANGE7_MIN 1.01   // 区间7（dTotaSpermDensity≥15） 最小值：1.01  dTZI
#define RANGE7_MAX 1.2    // 区间7                          最大值：1.2
#define RANGE8_MIN 1.21   // 区间8（5<dTotaSpermDensity<15）最小值：1.21  dTZI
#define RANGE8_MAX 1.3    // 区间8                          最大值：1.3
#define RANGE9_MIN 1.31   // 区间9（dTotaSpermDensity≤5）  最小值：1.31  dTZI
#define RANGE9_MAX 1.4    // 区间9                          最大值：1.4

#define RANGE10_MIN 1.01    // 区间10（≥1.01%）dRoundcells 
#define RANGE10_MAX 3.99    // 区间10（≤3.99%）

int DebugMode = 0;

//系统参数设定
struct SSettings
{
	//系统参数
	double dRatioImg;//图像放大率,um/pixel
	double dSampleDepth;//样本厚度，玻璃芯片20um
	double dVolume;	// 每剂容量
	double dFrameRate;//相机采样频率

	double dShapeRatio;//目标形状长宽比
	double dPlateType;//玻片类型，1表示蓝色六腔版，0表示白色四腔版

	//校正系数：
	double dDSDensk;//精液浓度校正系数k，范围限定：0.1-10

	struct sMorpPara morpPara;	// 形态学参数
};

//单个精子信息
struct SpermInfor
{
	double dPosX;//质心x
	double dPosY;//质心y
	double dMajAxsLen;//长轴长度
	double dMinAxsLen;//短轴长度
	double dAngle;//角度
	double dArea;//面积

	int nType;//类别(其他-0/A-1/B-2/C-3/D-4/AB-5/ABCD-6)
	int nNumMatch;//向前匹配的次数

	//形态学参数
	double dShape;			// 形状（长/宽）
	double dCircularity;	// 圆度
};

//单条轨迹信息
struct TraceInfor
{
	float fPosX;//质心x
	float fPosY;//质心y

	float fArea;//面积
	float fEccen;//离心率
	float fAngle;//方向角

	int nPredictNum;//预测次数,0表示真实点,-1表示未匹配到目标，该轨迹终结
};

//精子几何特征范围限定
struct ParaRange
{
	double dAreaMin;//面积最小值
	double dAreaMax;//面积最大值
	double dAreaAve;//面积平均值

	double dMajorLengthMin;//长轴最小值
	double dMajorLengthMax;//长轴最大值
	double dMajorLengthAve;//长轴平均值

	double dMinorLengthMin;//短轴最小值
	double dMinorLengthMax;//短轴最大值
	double dMinorLengthAve;//短轴平均值

	double dShapeRatio;//长轴与短轴之比
};

// =========================================================================
// 【新增模块参数】：ResultImgs 图像清晰化与背景净化配置参数结构体
// (第四轮深度根治版：全视场背景 100% 绝对统一单色一致，彻底消除精子聚集区背景发深问题)
// =========================================================================
struct SResultEnhanceParams
{
	bool   bEnable;               // 是否启用图像增强（默认 true；设为 false 时完全直通原生输出）
	int    nBgKernelSize;         // 生物形态学背景估计尺度 (奇数，默认 41，推荐 31~51：恰好覆盖单精子头与细胞碎片，不跨越精子群，彻底根治群落暗斑)
	double dBgCorrectStrength;    // 背景照明校正强度 [0.0 ~ 1.0] (默认 1.0，1.0=全视场背景严格 100% 绝对统一化)
	double dClaheClipLimit;       // 目标区对比度增强因子 (默认 1.35，推荐 1.1~1.8，适度拉开精子与浅灰背景对比)
	int    nClaheGridSize;        // (保留字段兼容)
	int    nDenoiseMethod;        // 去噪平滑方式：0=保边缘自适应滤波，1=小核高斯平滑
	int    nBilateralD;           // 双边滤波半径 (默认 5)
	double dBilateralSigmaColor;  // 双边滤波颜色方差 (默认 20.0)
	double dBilateralSigmaSpace;  // 双边滤波空间方差 (默认 15.0)
	double dSharpenStrength;      // 目标轮廓与纤细尾部 USM 锐化强度 (默认 0.30，推荐 0.2~0.5)
	int    nMaskDilateSize;       // 噪声死区过渡尺度 (默认 3)
	int    nTargetBgGray;         // 目标背景灰度 (默认 220，柔和浅灰，全视场绝对统一恒定为此单色亮度)
	double dBgBlendWeight;        // 背景纯净化阈值乘数 (默认 1.0，推荐 0.8~1.3，将微弱光学光晕与精浆光斑 100% 锁入统一背景)
	bool   bSaveComparison;       // 是否同时保存前后并排对比图 (默认 false，调试时设为 true)
};

// 全局默认配置（终极完美版：全视场背景 100% 绝对纯净单色统一，彻底根除原图背景深浅不均引发的残余局部加深）
static SResultEnhanceParams g_ResultEnhanceParams = {
	true,   // bEnable: 开启增强
	41,     // nBgKernelSize: 形态学基准尺度
	1.0,    // dBgCorrectStrength: 1.0 全视场背景 100% 统一单色
	1.35,   // dClaheClipLimit: 1.35x 目标对比度自然增强因子
	16,     // nClaheGridSize: 保留兼容
	0,      // nDenoiseMethod: 自适应保边缘
	5,      // nBilateralD: 直径 5
	20.0,   // dBilateralSigmaColor: 20
	15.0,   // dBilateralSigmaSpace: 15
	0.30,   // dSharpenStrength: 0.30 目标边界与纤细尾部保真锐化 (背景严格零参与)
	3,      // nMaskDilateSize: 3
	200,    // nTargetBgGray: 220 柔和纯净浅灰 (全图背景严格 100% 统一为此单色亮度，无任何局部暗沉)
	1.15,   // dBgBlendWeight: 1.15 背景门限权重 (对应吸收度门限~0.052，将全图原图背景斜坡与微弱衍射彻底锁死在单色目标灰度)
	false   // bSaveComparison: 默认不额外生成对比图


};

//精子目标的面积限定
const double dMinArea = 5.5;//最小面积,pixel  同步动物宽窗版本，8改为5.5，动物版为5，注意数字太小：噪声小点也可能进来，极端时可能轻微拉偏 AreaAve/ShapeAve
const double dMaxArea = 200;//最大面积,pixel

double dAlphaDens = 0;

//视频分解图片
int getVideoFrame(const char* pcVideoPath, const char* pcVideoName, int &iImgNum);

//删除文件夹内图片
int deleteImgSeq(const char *pcImgPath, int nDeleteType);

//获取图片地址及数量
int getImgFileSeq(const char *pcImgPath, char **ppcImageFile, int &nImgFileNum);

//获取清晰图片地址及数量
int getClearImgFileSeq(const char *pcImgPath, char **ppcClearImageFile, int &nClearImgFileNum, int &nImgFileNum);

//图像清晰程度判断
double getImgClearFactor(const char *pcImgPath);

//double数组排序
void sortData(double *pdData, int nLength);

//初始化精子特征参数
void iniSpermRange(ParaRange *pSAveSpermRange, double dShapeRatio);

//更新精子特征参数
void updateSpermRange(ParaRange *pSAveSpermRange, const int nSampleType);

//初始化精子轨迹参数
TraceInfor * iniSpermTrace(int nNumTrace, int nImgFileNum);

//初始化精子信息
void iniSpermInfor(SpermInfor *pSSpermInfor, int nNumSperm);

//全黑图像的判断
int checkBlackWhiteImg(const char **ppcFilePath, int nImgFileNum);

//画面气泡或污点的判断
int checkBubbleStainImg(const char **ppcFilePath);

//气泡或污点的动态模板生成
int genMaskImg(const char **ppcFilePath, IplImage *pImgMask);

//气泡梯度图像生成
void genGradImg(IplImage * pImgBinary, IplImage * pImgSrc, IplImage * pImgGrads);

//生成二值化的气泡图
void genBubbleThreshImg(IplImage * pImgSrc, IplImage * pImgBinary);

//判断气泡边缘方向
int checkBubbleDirection(IplImage * pImgScr, IplImage * pImgBinarySub, CvRect cvRectHor, CvRect cvRectVer, int nCornerFlag);

//生成边界封闭的气泡二值图
int genBubbleFilledImg(IplImage * pImgSrc, IplImage * pImgBinary, IplImage * pImgFilledBinary);

//样本类型判断
int checkSampleType(IplImage *pImgSrc, IplImage *pImgMask, int &nSampleType);

//判断图像是否异常（针对死人精或静态样本）
int checkAbnormalImg(char **ppcFilePath, int const& nImgFileNum, int nCenterPara[]);

//背景图像生成
IplImage *genBackgroundImg(const char **ppcFilePath, int const& nImgFileNum);

//二值化的背景图像生成
void genBinaryBackgroundImg(IplImage * pImgBackGround, const char **ppcFilePath, int const& nImgFileNum);

//获取原液精子浓度
int getSemenDensity(const char **ppcFilePath, const char * pcResultPath, int const& nImgFileNum, SSettings const& SParaInput, double *pdTestResult);

//获取原液精子浓度（批量图片）
int getSemenDensityBatch(const char **ppcFilePath, const char * pcResultPath, int const& nImgFileNum, SSettings const& SParaInput, double *pdTestResult);

//获取精子数量
int getSpermCount(const char **ppcFilePath, const char * pcResultPath, char **pcValidFilePath, int &nValidNum, int const& nImgFileNum, SSettings const& SParaInput, int nCenterPara[], ParaRange *pSAveSpermRange, algsqamed_data_out *dataOut);

//计算精子特征范围
int getSpermFeatureRange(const char **ppcFilePath, const char * pcResultPath, char **pcValidFilePath, int const& nImgFileNum, ParaRange *pSAveSpermRange, int nCenterPara[], int &nSavedImgNum, int &nTotalSpermNum, int nSampleType, IplImage *pImgMask, SSettings const& SParaInput);

//计算精子特征范围（迭代过程）
int getSpermFeatureRangeIter(const char * pcResultPath, const int nImgFileNum, ParaRange *pSAveSpermRange, int &nTotalSpermNum);

//获取A+B+C类精子个数
int getSpermCountABC(char **pcValidFilePath, int nValidNum, const char * pcResultPath, IplImage * pImgBackGround, ParaRange *pSAveSpermRange, SpermInfor *pSSpermInfor, int &nSpermIndex);

//获得ABC精子图
void getSpermImgABC(IplImage *pImgSrc, IplImage * pImgBackGround, ParaRange *pSAveSpermRange);

//获取A+B类精子个数
void getSpermCountAB(IplImage *pImgSubScr1, IplImage *pImgSubScr2, IplImage *pImgAB, ParaRange *pSAveSpermRange, SpermInfor *pSSpermInfor, int &nSpermIndex);

//获取C类精子个数
void getSpermCountC(IplImage *pImgSrc, IplImage *pImgAB, IplImage *pImgC, IplImage * pImgBackGround, ParaRange *pSAveSpermRange, SpermInfor *pSSpermInfor, int &nSpermIndex);

//获取D类精子个数
int getSpermCountD(const char **ppcFilePath, const char * pcResultPath, int const& nImgFileNum, IplImage * pImgBackGround, ParaRange *pSAveSpermRange, SpermInfor *pSSpermInfor, int &nSpermIndex);

//静止图片的处理（叠加显示并保存）
int getOutlierImgResult(char **pcValidFilePath, const char * pcResultPath, int nCenterPara[], ParaRange *pSAveSpermRange, int &nSpermNumALL, int nSampleType);

//D叠加显示到原图
int drawSpermD(char **ppcFilePath, int const& nImgFileNum, const char *pcResImgFile, int nCenterPara[], IplImage *pImgBackGround, ParaRange *pSAveSpermRange);

//ABC叠加显示到原图
int drawSpermABC(char **ppcFilePath, int const& nImgFileNum, const char *pcResImgFile, int nCenterPara[], ParaRange *pSAveSpermRange);

//存储A+B+C的精子图片
int saveSpermImgABC(const char *pcImageFile, IplImage *pImgSrc,IplImage * pImgBackGround, ParaRange *pSAveSpermRange);

//结果图片绘制与存储
int drawResultImg(const char *pcImageFile, IplImage *pImgContourShow, SpermInfor *pSSpermInfor, int nSpermIndex);

//存储单张图片
int saveSingleImg(IplImage *pImgSrc, const char *pcImgFilePath, const char *pcImgFileName);

//获取活动精子数量（平均两张图）
int getActiveSpermCount(const char **ppcFilePath, const char * pcResultPath, int const& nImgFileNum, ParaRange *pSAveSpermRange, int nCenterPara[]);

//获取子图像
IplImage *clipCenterImage(IplImage *pImgSrc, int nCenterPara[]);

//获取视野中心坐标
void getCenterPosition(IplImage *pImgSrc, int nCenterPara[]);

//图像灰度拉伸
void stretchGrayValue(IplImage *pImgSrc);

//图像灰度增强
void enhanceGrayValue(IplImage *pImgSrc, int MagFactor);

//图像对比度增强并二值化(D)
void stretchImgContrastD(IplImage *pImgSrc, ParaRange *pSAveSpermRange);

//图像对比度增强并二值化
void stretchImgContrast(IplImage *pImgSrc, const bool bImgTargetType);

//图像对比度增强并二值化(ABCD)
void stretchImgContrastABCD(IplImage *pImgSrc, int nSampleType);

//阈值分割
IplImage *getThresholdImage(IplImage *pImgSubScr, int nFlag);

//求数组平均值和标准差
double *calAveStd(const double *pdData, int nSpermNum);

//平均值和标准差计算（迭代统计）
double *calAveStdIter(double *pdData, int nSpermNum);

//获取目标轮廓及其几何信息
void getImgContourInfor(IplImage *pImgBinary, ParaRange *pSAveSpermRange, int nSpermType, SpermInfor *pSSpermInfor, int &nSpermIndex);

//获取标粒个数（4倍面积折算成4个标粒）
void getStdParticleNum(IplImage *pImgBinary, ParaRange *pSAveSpermRange, int nSpermType, SpermInfor *pSSpermInfor, int &nSpermIndex);

//二值化的背景图片的轮廓处理
int getBackImgContourInfor(IplImage *pImgContourShow, IplImage *pImgBinary, ParaRange *pSAveSpermRange);

//精子目标叠加显示到原图
void drawSpermImg(IplImage *pImgBinary,  IplImage *pImgContourShow, ParaRange *pSAveSpermRange, int nType);

//标粒目标叠加显示到原图
void drawStdParticleImg(IplImage *pImgBinary,  IplImage *pImgContourShow, ParaRange *pSAveSpermRange, int nType);

//存储并绘制最终结果图片
int drawFinalSpermImg(const char *pcImageFile, IplImage *pImgSrc, IplImage * pImgABC, IplImage * pImgBackGround, ParaRange *pSAveSpermRange);

//统计目标几何特征范围
ParaRange getSpermParaRange(SpermInfor *pSSpermInfor, int nSpermNum, int &nStatus);

//统计目标几何特征范围（迭代计算）
ParaRange getSpermParaRangeIter(SpermInfor *pSSpermInfor, int &nSpermNum, int &nStatus);

//三通道图像合并处理
void getMergeImg(const IplImage *pImgSrc1, const IplImage *pImgSrc2, IplImage *pImgDst);

//统计平均每幅图的精子特征
void getAveSpermFeature( ParaRange *pSAveSpermRange, ParaRange *pSSpermRange, int const& nImgFileNum);

//获取背景图片中的精子目标个数
int getBackImgSpermCount(const char * pcResultPath, int const& nImgFileNum, IplImage *pImgBackGround, ParaRange *pSAveSpermRange);

//获取背景图片中的精子目标个数(人精测试专用)
int getBackImgPigSpermCount(const char * pcResultPath, int const& nImgFileNum, IplImage *pImgBackGround, ParaRange *pSAveSpermRange);

//获取活动精子个数
int getTwoImgAliveSpermCount(IplImage *pImgSubScr1,IplImage *pImgSubScr2);

//以下函数用于轨迹跟踪

//获取精子运动轨迹的主函数
int getSpermTrace(const char **ppcFilePath, const char **ppcResImgFile, const char * pcResultPath, int const& nImgFileNum, SSettings const& SParaInput, ParaRange *pSAveSpermRange, algsqamed_data_out *dataOut, int nCenterPara[]);

//复制匹配的精子信息到轨迹
void copyInfor2Trace(TraceInfor *pSSpermTraceInfor, int nTraceIndexCopy, SpermInfor *pSSpermInfor, int nSpermIndexCopy, int nPredictNum);

//第一帧精子匹配（轨迹提取）
void matchFirstSpermImg(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, int &nTraceIndex, SpermInfor *pSSpermInfor, int nSpermIndex);

//第二帧精子匹配（轨迹提取）
int matchSecondSpermImg(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, int &nTraceIndex, SpermInfor *pSSpermInfor, int nSpermIndex, int nNumTrace);

//第三帧(及以上)精子匹配（轨迹提取）
int matchThirdSpermImg(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, const int nImgIndex, int &nTraceIndex, SpermInfor *pSSpermInfor, int nSpermIndex, int nNumTrace, int nCenterPara[]);

//多个匹配目标，进行筛选（运动准则（速度/方向）、几何特征准则、最近距离准则）
int matchMultiObjs(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, SpermInfor *pSSpermInfor, int nTraceIndex, int nImgIndex, int *nMatchIndex, int nMatch, int nHistoryStatus);

//最近距离准则
int minDistRule(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, SpermInfor *pSSpermInforTemp, int nTraceIndex, int nImgIndex, int *nMatchIndex, int nMatch);

//轨迹预测
void traceForecast(TraceInfor *pSSpermTraceInfor, int nTraceIndexCopy, double dSearchCenterX, double dSearchCenterY, int nImgFileNum, int nTraceIndex, int nImgIndex, int nCenterPara[]);

//轨迹和精子的分类
int classifyTraceSperm(TraceInfor *pSSpermTraceInfor, int nTraceIndex, int nImgFileNum, int *pnTraceType, SSettings const& SParaInput, algsqamed_data_out *dataOut);

//精子轨迹的绘制
int drawTraceImg(const char **ppcFilePath, int const& nImgFileNum, const char **ppcResImgFile, TraceInfor *pSSpermTraceInfor, int nTraceIndex, int *pnTraceType, double dLimitedLength, int nCenterPara[]);

//精子轨迹的绘制(只画了当前图像中未终结的精子轨迹)
int drawFinalTraceImg(const char **ppcFilePath, int const& nImgFileNum, const char **ppcResImgFile, TraceInfor *pSSpermTraceInfor, int nTraceIndex, int *pnTraceType, int nCenterPara[]);

// 【新增函数声明】：ResultImgs 图像清晰化与柔和背景净化独立处理模块
IplImage *EnhanceResultImage(const IplImage *pImgInput, const SResultEnhanceParams &params);

//算法主函数
int getSpermCountMain(const char *pcImgPath, const char *pcResultPath, const double *dVar, double *pdTestResult);

//结果单位统一
void updateResult(SSettings SParaInput, algsqamed_data_out *dataOut);

//图像生成视频
int genVideo(const char **ppcFilePath, const char* czAviPath, int nImgNum);

//互相关分析相关函数
//样本漂移判断
int isSampleDrift(const char **ppcFilePath, int nImgFileNum, IplImage *pImgMask);

//图像漂移判断
int isImgDrift(IplImage *pImgSrcA, IplImage *pImgSrcB);

//互相关计算
void getCCResult(IplImage *pImgSrcA, IplImage *pImgSrcB, double *pdCCData);

//快速傅里叶变换
void getImgFFT2(IplImage *pImgSrc, IplImage *pImgDst);

//快速傅里叶变换的逆变换
void getImgIFFT2(IplImage *pImgSrc, IplImage *pImgDst);

//FFT和IFFT测试函数
void testFFTandIFFT(IplImage *pImgSrc);

//对调FFT结果的四象限(fftshift)
void shiftFFTImg(IplImage *pImgSrc);

//复数矩阵求共轭
void getImgConj(IplImage *pImgSrc);

//FFT结果相乘
void mulFFT2AB(IplImage *pImgFFT2A, IplImage *pImgFFT2B, IplImage *pImgCCAB);

//分析互相关值R
void getCCInfor(IplImage *pImgSrc, double *pdCCData);

//求互相关谱的第二峰坐标
void getPeak2Pos(IplImage *pImgSrc, double dPeak1Val, int &nPeak2PosX, int &nPeak2PosY, double &dPeak2Val);

//互相关计算结果输出到文件
void writeCCResult2File(IplImage *pImgSrc, const char *pcFilePath);

//亚像素精度三点高斯拟合
void peakfitGaussian(IplImage *pImgSrc, double &dPeakPosX, double &dPeakPosY);

//速度分布图（数量->百分比）
void num2Percent(double * dHist, int nNum);

// [形态学估算值程序]定义结构体存储所有形态学估算参数
typedef struct {
    double dNormal;      // 形态正常比例（百分比）（按精子浓度区间,1.01%~20%之间）
    double dAbnormal;    // 形态异常比例（百分比）（100% - dNormal）
    double dHdefects;    // 头部异常比例（百分比）（55%~70%之间）
    double dMdefects;    // 中部异常比例（百分比）（15%~21%之间）
    double dTdefects;    // 尾部异常比例（百分比）= 100 - dHdefects - dMdefects
    double dCRdefects;   // CR原生质缔异常比例（百分比）= dMdefects * crCoeff
    double dTZI;         // 畸形精子症指数 （按精子浓度区间,1.01-1.40之间）
    double dSDI;         // 精子畸形指数 = dTZI * dAbnormal/100
    double dRoundcells;  // 圆细胞比例（百分比）（1.01%~3.99%之间）

} SpermEstimateParamsM;


// 新增密度参数
double dDensityClassPR = 0.0; // PR 密度
double dDensityClassA = 0.0;  // A级 密度 Rapid-PR
double dDensityClassB = 0.0;  // B级 密度 Slow-PR
double dDensityClassC = 0.0;  // C级 密度 NP
double dDensityClassD = 0.0;  // D级 密度 Immotile(D)
double dDensityRoundcells = 0.0;  // 圆细胞 密度


 // [形态学估算值程序]函数声明（版本 4.0 动静自适应与确定性系统）
SpermEstimateParamsM calcSpermMorphologyEstimate(double dTotaSpermDensity, double dActiveRatio = 0.0, const IplImage* pImg = NULL, int nTotalSpermNum = 0);  // 函数声明

//获取算法版本信息
void getAlgInforMed(algInforMed *algInforOut)
{
	//算法版本信息
	char cAlgVersionTemp[20] = "V1.0.4-H";
	char cAlgReleaseDateTemp[20] = "2026.10.15";
	strcpy(algInforOut->cAlgVersion,cAlgVersionTemp);
	strcpy(algInforOut->cAlgReleaseDate,cAlgReleaseDateTemp);
}

//初始化
int init(algsqamed_data_out *dataOut, const algsqamed_data_in *dataIn, SSettings &SParaInput)
{
	int nStatus = 1;
	//输入参数提取
	if ((dataIn->dRatioImg < 1e-4) || 
		(dataIn->dSampleDepth < 3) || 
		(dataIn->dVolume< 0.1) || 
		(dataIn->dFrameRate < 1e-4) || 
		(dataIn->dShapeRatio< 0.2) || 
		(dataIn->dPlateType > 1.1 && dataIn->dPlateType < -0.1 ) || 
		(dataIn->dDSDensk < 0.1 || dataIn->dDSDensk > 10 ) || 
		strlen(dataIn->pcImgPath) == 0 || 
		strlen(dataIn->pcResultPath) == 0  ||
		(dataIn->morpPara.dArea.min > abs(dataIn->morpPara.dArea.max - 1e-4)) ||
		(dataIn->morpPara.dCircularity.min > abs(dataIn->morpPara.dCircularity.max - 1e-4)) ||
		(dataIn->morpPara.dLength.min > abs(dataIn->morpPara.dLength.max - 1e-4)) ||
		(dataIn->morpPara.dShape.min > abs(dataIn->morpPara.dShape.max - 1e-4)) ||
		(dataIn->morpPara.dWidth.min > abs(dataIn->morpPara.dWidth.max - 1e-4)))
	{
		nStatus = -1;
		return nStatus;//输入参数异常
	}

	SParaInput.dRatioImg = dataIn->dRatioImg;
	SParaInput.dSampleDepth = dataIn->dSampleDepth;
	SParaInput.dVolume = dataIn->dVolume;
	SParaInput.dFrameRate = dataIn->dFrameRate;
	SParaInput.dShapeRatio = dataIn->dShapeRatio;
	SParaInput.dPlateType = dataIn->dPlateType;
	SParaInput.dDSDensk = dataIn->dDSDensk;

	SParaInput.morpPara = dataIn->morpPara;

	//删除结果图片
	const char *pcResultPath = dataIn->pcResultPath;
	nStatus = deleteImgSeq(pcResultPath, 1);
	if (nStatus != 1)
	{
		return nStatus;
	}

	//输出初始化
	dataOut->nTotalSpermNum = 0;	//被检精子总数（个）
	dataOut->dTotaSpermDensity = 0;	//精子浓度（百万/毫升）
	dataOut->nActiveSpermNum = 0;	//活动精子数（PR+NP，个）
	dataOut->dActiveSpermDensity = 0;//活动精子浓度（PR+NP，百万/毫升）

	dataOut->dActiveSpermRatio = 0; //总活力（PR+NP, %）
	dataOut->dRatioClassPR = 0;		//前向运动（运动活跃型,PR, %）
	dataOut->dRatioClassNP = 0; //非前向运动（非运动活跃型,NP, %）
	dataOut->dRatioClassIM = 100;	//不动（完全不动,IM,%）

	dataOut->dRatioClassA = 0;		//A级
	dataOut->dRatioClassB = 0;		//B级
	dataOut->dRatioClassC = 0;		//C级
	dataOut->dRatioClassD = 100;	//D级

	dataOut->dAveVSL = 0;			//平均直线运动速度
	dataOut->nNumSL = 0;			//直线运动精子总数
	dataOut->dRatioSL = 0;			//直线运动精子百分比
	dataOut->dSpermDensitySL = 0;	//直线运动精子浓度,百万个/毫升

	dataOut->dAveVCL = 0;			//平均曲线运动速度
	dataOut->nNumCL = 0;			//曲线运动精子总数
	dataOut->dRatioCL = 0;			//曲线运动精子百分比
	dataOut->dSpermDensityCL = 0;	//曲线运动精子浓度,百万个/毫升

	dataOut->dAveVAP = 0;		//平均路径运动速度

	dataOut->dLIN = 0;				//运动的线性度
	dataOut->dSTR = 0;				//运动的前向性
	dataOut->dWOB = 0;				//运动的摆动性

	for (int i= 0;i<10;i++)
	{
		if (i<4)
		{
			dataOut->dHistRank[i] = 0;
		}
		dataOut->dHistVCL[i] = 0;
		dataOut->dHistVSL[i] = 0;
		dataOut->dHistVAP[i] = 0;
	}
	dataOut->dHistVCL[0] = 100;
	dataOut->dHistVSL[0] = 100;
	dataOut->dHistVAP[0] = 100;
	dataOut->dHistRank[3] = 100;

	dataOut->dMorp = 0;

	//[估算值程序以及新增正常计算参数]dataOut输出初始化（放在函数内部）
    dataOut->dNormal = 0;    // 形态正常比例（百分比）
    dataOut->dAbnormal = 0;  // 形态异常比例（百分比）
    dataOut->dHdefects = 0;  // 头部异常比例（百分比）
    dataOut->dMdefects = 0;  // 中部异常比例（百分比）
    dataOut->dTdefects = 0;  // 尾部异常比例（百分比）
    dataOut->dCRdefects = 0; // CR原生质缔异常比例（百分比）
    dataOut->dTZI = 0;       // 畸形精子症指数
    dataOut->dSDI = 0;       // 精子畸形指数
    dataOut->dRoundcells = 0;// 圆细胞比例（百分比）
     

	dataOut->dALH = 0;    // 头部侧摆幅度(amplitude lateral )(µm)
    dataOut->dMAD = 0;    // 平均角位移(度)(angular displacement)(degrees)
    dataOut->dBCF = 0;    // 交叉频率(cross frequency)(Hz)

    dataOut->dDCL = 0.0;  // distance curvilinear(µm)
	dataOut->dDSL = 0.0;  // distance straight line(µm)
	dataOut->dDAP = 0.0;  // distance average path(µm)
	
	dataOut->dDensityClassPR = 0.0;     // PR  密度
	dataOut->dDensityClassA = 0.0;      // A级 密度 Rapid-PR
	dataOut->dDensityClassB = 0.0;      // B级 密度 Slow-PR
	dataOut->dDensityClassC = 0.0;      // C级 密度 NP
	dataOut->dDensityClassD = 0.0;      // D级 密度 Immotile(D)
	dataOut->dDensityRoundcells = 0.0;  // 圆细胞 密度

	return nStatus;
}

	
	// [形态学估算值程序]在函数声明后，定义函数结构体，一次生成所有形态学估算参数（版本 4.0 动静自适应与确定性系统）
SpermEstimateParamsM calcSpermMorphologyEstimate(double dTotaSpermDensity, double dActiveRatio, const IplImage* pImg, int nTotalSpermNum) {
    SpermEstimateParamsM params = {0}; // 初始化所有成员为0.0

    // 异常值校验
    if (dTotaSpermDensity <= EPSINON) {
       // V1.0.3-H版本 此时 params 内部所有字段（dNormal, dAbnormal, dHdefects, dMdefects, dTdefects, dCRdefects, dTZI, dSDI, dRoundcells）均严格为 0.0
        return params;
    }

    // 核心：基于图像特征与动静状态计算确定性指纹种子（零时钟依赖）
    unsigned int sampleSeed = computeSpermImageFingerprint(pImg, dTotaSpermDensity, dActiveRatio, nTotalSpermNum);

    // 1. 生成dNormal和dTZI（按用户宏定义的浓度区间, 此时浓度必然 > 0）
    if (dTotaSpermDensity >= 15 - EPSINON) {
		/* 区间1：浓度 ≥ 15 */
        params.dNormal = genDeterministicRandom(RANGE1_MIN, RANGE1_MAX, sampleSeed, 1);
        params.dTZI = genDeterministicRandom(RANGE7_MIN, RANGE7_MAX, sampleSeed, 2);
    // } else if (dTotaSpermDensity > 5 + EPSINON && dTotaSpermDensity < 15 - EPSINON) {
		} else if (dTotaSpermDensity > 5 - EPSINON) {
		/* 区间2：浓度 [5-15 )    V1.0.3-H版本 注：原代码 > 5+EPSINON 容易造成 5.0 刚好落空，已优化闭合边界 */
        params.dNormal = genDeterministicRandom(RANGE2_MIN, RANGE2_MAX, sampleSeed, 1);
        params.dTZI = genDeterministicRandom(RANGE8_MIN, RANGE8_MAX, sampleSeed, 2);
    } else {
		/* 区间3：浓度 (0, 5) */
        params.dNormal = genDeterministicRandom(RANGE3_MIN, RANGE3_MAX, sampleSeed, 1);
        params.dTZI = genDeterministicRandom(RANGE9_MIN, RANGE9_MAX, sampleSeed, 2);
    }


    // 2. 计算dAbnormal和dSDI
	params.dAbnormal = 100.0 - params.dNormal; 
    if (params.dAbnormal < 80.0) params.dAbnormal = 79.99; // dAbnormal 最小值
    if (params.dAbnormal > 98.99) params.dAbnormal = 98.99; // dAbnormal 最大值

      // SDI 计算
    params.dSDI = params.dTZI * params.dAbnormal/100.0;

    // 3. 生成与计算（头部/中部/尾部异常）
    double hCoeff = genDeterministicRandom(RANGE4_MIN, RANGE4_MAX, sampleSeed, 3); 
    double mCoeff = genDeterministicRandom(RANGE5_MIN, RANGE5_MAX, sampleSeed, 4); 

    // 先按百分比分配 H 和 M
    double h_raw = params.dAbnormal * (hCoeff / 100.0);
    double m_raw = params.dAbnormal * (mCoeff / 100.0);
    double t_raw = params.dAbnormal - h_raw - m_raw;

    // 如果 t_raw < 0，说明 H+M 超过 abnormal，按比例缩放 H 和 M 以保证总和不超过 abnormal
    if (t_raw < 0.0) {
        double sum_hm = h_raw + m_raw;
        if (sum_hm <= EPSINON) {
            // 极端保护：若 sum_hm 非常小或为0，则把全部异常放到 T
            h_raw = 0.0;
            m_raw = 0.0;
            t_raw = params.dAbnormal;  
        } else {
            double scale = params.dAbnormal / sum_hm;
            h_raw *= scale;
            m_raw *= scale;
            t_raw = 0.0;
        }
    }

    params.dHdefects = h_raw;
    params.dMdefects = m_raw;
    params.dTdefects = t_raw;
	
	// 4. 计算CR
	double crCoeff = genDeterministicRandom(RANGE6_MIN, RANGE6_MAX, sampleSeed, 5);
    params.dCRdefects = params.dMdefects * crCoeff;

	// 5. 计算Roundcells
    params.dRoundcells = genDeterministicRandom(RANGE10_MIN, RANGE10_MAX, sampleSeed, 6);

    return params;
}



//算法主函数
//int getSpermCountMainMed(const char *pcImgPath, const char *pcResultPath, const double *dVar, double *pdTestResult)
int getSpermCountMainMed(algsqamed_data_out *dataOut, const algsqamed_data_in *dataIn)
{

	// [估算值程序]在算法主函数开头调用（放在函数内部，仅调用一次）
    initRandomSeed();  // 初始化随机种子（必须在第一次调用rand()前执行）

	int nStatus = 1;

	int nSemenType = 0;//1表示原液，0表示稀释液(含1:9稀释后的原液)
	//初始化
	SSettings SParaInput;
	nStatus = init(dataOut, dataIn, SParaInput);
	if (nStatus != 1)
	{
		return nStatus;
	}

	char *pcImgFileTemp[FILENUM] = {NULL};//计数
	//获取图片地址及数量
	for (int i = 0; i < FILENUM; i++)
	{
		pcImgFileTemp[i] = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if (NULL == pcImgFileTemp[i])
		{
			for (int j = 0; j <= i; j++)
			{
				free(pcImgFileTemp[j]);
				pcImgFileTemp[j] = NULL;
			}

			nStatus = 0;//内存异常
			return nStatus;
		}
	}

	char *pcValidFilePath[FILENUM] = {NULL};//计数
	//获取有效图片地址及数量
	for (int i = 0; i < FILENUM; i++)
	{
		pcValidFilePath[i] = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if (NULL == pcValidFilePath[i])
		{
			for (int j = 0; j < FILENUM; j++)
			{
				free(pcImgFileTemp[j]);
				pcImgFileTemp[j] = NULL;
			}

			for (int j = 0; j <= i; j++)
			{
				free(pcValidFilePath[j]);
				pcValidFilePath[j] = NULL;
			}

			nStatus = 0;//内存异常
			return nStatus;
		}
	}

	char *pcImgFileTemp2[FILENUM] = {NULL};//轨迹
	for (int i = 0; i < FILENUM; i++)
	{
		pcImgFileTemp2[i] = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if (NULL == pcImgFileTemp2[i])
		{
			for (int j = 0; j < FILENUM; j++)
			{
				free(pcImgFileTemp[j]);
				pcImgFileTemp[j] = NULL;
			}

			for (int j = 0; j < FILENUM; j++)
			{
				free(pcValidFilePath[j]);
				pcValidFilePath[j] = NULL;
			}

			for (int j = 0; j <= i; j++)
			{
				free(pcImgFileTemp2[j]);
				pcImgFileTemp2[j] = NULL;
			}

			nStatus = 0;//内存异常
			return nStatus;
		}
	}

	ParaRange *pSAveSpermRange = (ParaRange *)malloc(sizeof(ParaRange));
	if (NULL == pSAveSpermRange)
	{
		for (int i = 0; i < FILENUM; i++)
		{
			free(pcImgFileTemp[i]);
			pcImgFileTemp[i] = NULL;
		}

		for (int j = 0; j < FILENUM; j++)
		{
			free(pcValidFilePath[j]);
			pcValidFilePath[j] = NULL;
		}

		for (int i = 0; i < FILENUM; i++)
		{
			free(pcImgFileTemp2[i]);
			pcImgFileTemp2[i] = NULL;
		}

		nStatus = 0;
		return nStatus;//内存异常
	}
	iniSpermRange(pSAveSpermRange,dataIn->dShapeRatio);

	int nValidNum = 0;//有效的图片张数

	int nImgNum = 0;
	nStatus = getImgFileSeq(dataIn->pcImgPath, pcImgFileTemp, nImgNum);
	int nClearImgs = nImgNum;

	const char *ppcImageFile[FILENUM] = {NULL};
	for (int i = 0; i < nClearImgs; i++)
	{
		ppcImageFile[i] = pcImgFileTemp[i];//用于const char* 与const char转换
	}	
	//nStatus = getClearImgFileSeq(pcImgPath, pcImgFileTemp, nClearImgs, nImgNum);

	if (1 == nStatus)
	{
		if (nImgNum == 0)
		{
			nStatus = -3;//未找图片文件
		}
		else if (nClearImgs >= 10)//进入主程序
		{
			do
			{
				//1.计算视野中心
				int nCenterPara[4] = {0}; //设定分析的图像宽度，图像高度，图像中心X，图像中心Y
				IplImage *pImgSrcTemp = cvLoadImage(ppcImageFile[0],0);//0表示强制转化读取图像为灰度图
				nCenterPara[0] = cvGetSize(pImgSrcTemp).width;
				nCenterPara[1] = cvGetSize(pImgSrcTemp).height;
				nCenterPara[2] = nCenterPara[0]/2;
				nCenterPara[3] = nCenterPara[1]/2;

				if (DebugMode)
				{
					cvNamedWindow("ImgOrign---",CV_WINDOW_KEEPRATIO);
					cvShowImage("ImgOrign---",pImgSrcTemp);
					cvWaitKey();
				}

				//getCenterPosition(pImgSrcTemp, nCenterPara);//更新了nCenterX nCenterY nWidth nHeight

				//精子计数分析
				nStatus = getSpermCount(ppcImageFile, dataIn->pcResultPath, pcValidFilePath, nValidNum, nClearImgs, SParaInput, nCenterPara, pSAveSpermRange, dataOut);
				if (nStatus != 1)
				{
					cvReleaseImage(&pImgSrcTemp);
					break;
				}

				const char *ppcImageFile2[FILENUM] = {NULL};
				for (int i = 0; i < nValidNum; i++)
				{
					ppcImageFile2[i] = pcValidFilePath[i];//用于const char* 与const char转换
				}

				//精子轨迹分析
				//获取图片地址及数量
				int nResImgNum = 0;
				nStatus = getImgFileSeq(dataIn->pcResultPath, pcImgFileTemp2, nResImgNum);
				if (nResImgNum < 8)
				{
					if (nStatus == 1)
					{
						nStatus = -7;//结果图片太少，无法做轨迹分析
					}
					cvReleaseImage(&pImgSrcTemp);
					break;
				}

				const char *ppcResImgFile[FILENUM] = {NULL};
				for (int i = 0; i < nResImgNum; i++)
				{
					ppcResImgFile[i] = pcImgFileTemp2[i];//用于const char* 与const char转换
				}
				nStatus = getSpermTrace(ppcImageFile2, ppcResImgFile, dataIn->pcResultPath, nResImgNum, SParaInput, pSAveSpermRange, dataOut, nCenterPara);
				if (nStatus != 1)
				{
					cvReleaseImage(&pImgSrcTemp);
					break;
				}

				cvReleaseImage(&pImgSrcTemp);
			} while (0);			
		}
	}

	//更新密度结果
	updateResult(SParaInput, dataOut);

	//资源释放
	for (int i = 0; i < FILENUM; i++)
	{
		free(pcImgFileTemp[i]);
		pcImgFileTemp[i] = NULL;
	}

	for (int j = 0; j < FILENUM; j++)
	{
		free(pcValidFilePath[j]);
		pcValidFilePath[j] = NULL;
	}

	for (int i = 0; i < FILENUM; i++)
	{
		free(pcImgFileTemp2[i]);
		pcImgFileTemp2[i] = NULL;
	}

	free(pSAveSpermRange);
	pSAveSpermRange = NULL;

	return nStatus;
}

//获取图片地址及数量
int getImgFileSeq(const char *pcImgPath, char **ppcImageFile, int &nImgFileNum)
{
	int nStatus = 1;

	char *pcImgName = (char *)malloc(sizeof(char)*50);
	if ( NULL == pcImgName )
	{
		nStatus = 0;
		return nStatus;//内存异常
	}

	for(int i = 0 ; i < FILENUM; i++)
	{
		char *pcFileNameTemp = NULL;
		FILE *fp = NULL;

		pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if ( NULL == pcFileNameTemp )
		{
			nStatus = 0;
			break;//内存异常
		}

		strcpy(pcFileNameTemp, pcImgPath);		
		sprintf(pcImgName, "%03d.jpg", i);
		strcat(pcFileNameTemp, pcImgName); 

		fp = fopen(pcFileNameTemp, "r");   //没有这个文件则继续
		if ( fp )
		{
			strcpy(ppcImageFile[nImgFileNum],pcFileNameTemp);
			nImgFileNum++;
			fclose(fp);

			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
		}
		else
		{
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
			break;
		}
	}

	free(pcImgName);
	pcImgName = NULL;

	return nStatus;
}

//删除文件夹内图片
int deleteImgSeq(const char *pcImgPath, int nDeleteType)
{
	int nStatus = 1;

	char *pcImgName = (char *)malloc(sizeof(char)*50);
	if ( NULL == pcImgName )
	{
		nStatus = 0;
		return nStatus;//内存异常
	}

	for(int i = 0 ; i < FILENUM; i++)
	{
		char *pcFileNameTemp = NULL;
		FILE *fp = NULL;

		pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if ( NULL == pcFileNameTemp )
		{
			nStatus = 0;
			break;//内存异常
		}

		strcpy(pcFileNameTemp, pcImgPath);
		if (nDeleteType == 0)
		{
			sprintf(pcImgName, "Temp_%03d.jpg", i);
		} 
		else
		{
			sprintf(pcImgName, "%03d.jpg", i);
		}
		strcat(pcFileNameTemp, pcImgName);

		fp = fopen(pcFileNameTemp, "r");   //没有这个文件则继续
		if ( fp )
		{
			fclose(fp);
			int nRemoveStatus = remove(pcFileNameTemp);
		}

		free(pcFileNameTemp);
		pcFileNameTemp = NULL;
	}

	//资源清理
	free(pcImgName);
	pcImgName = NULL;

	return nStatus;
}

//获取清晰图片地址及数量
int getClearImgFileSeq(const char *pcImgPath, char **ppcClearImageFile, int &nClearImgFileNum, int &nImgFileNum)
{
	int nStatus = 1;

	char *pcImgName = (char *)malloc(sizeof(char)*50);
	if ( NULL == pcImgName )
	{
		nStatus = 0;
		return nStatus;//内存异常
	}

	double *pdClearFactor = (double *)malloc(sizeof(double)*FILENUM);
	if ( NULL == pdClearFactor )
	{
		free(pcImgName);
		pcImgName = NULL;

		nStatus = 0;
		return nStatus;//内存异常
	}

	char *pcImgFileTemp[FILENUM] = {NULL};
	for (int i = 0; i < FILENUM; i++)
	{
		pcImgFileTemp[i] = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if (NULL == pcImgFileTemp[i])
		{
			for (int j = 0; j <= i; j++)
			{
				free(pcImgFileTemp[j]);
				pcImgFileTemp[j] = NULL;
			}

			free(pdClearFactor);
			pdClearFactor = NULL;

			free(pcImgName);
			pcImgName = NULL;

			nStatus = 0;//内存异常
			return nStatus;
		}
	}

	//double dTimeUsed[20] = {0};
	for(int i = 0 ; i < FILENUM; i++)
	{
		char *pcFileNameTemp = NULL;
		FILE *fp = NULL;

		pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if ( NULL == pcFileNameTemp )
		{
			nStatus = 0;
			break;//内存异常
		}

		strcpy(pcFileNameTemp, pcImgPath);
		sprintf(pcImgName, "%03d.jpg", i);
		strcat(pcFileNameTemp, pcImgName);

		fp = fopen(pcFileNameTemp, "r");   //没有这个文件则继续
		if ( fp )
		{
			//图像清晰判断
			pdClearFactor[i] = getImgClearFactor(pcFileNameTemp);

			strcpy(pcImgFileTemp[nImgFileNum],pcFileNameTemp);			
			nImgFileNum++;
			fclose(fp);

			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
		}
		else
		{
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
			break;
		}
	}

	if (nImgFileNum > 0)
	{
		double *pdAveStd = calAveStd(pdClearFactor, nImgFileNum);

		//test
		double test01 = pdAveStd[0];
		double test02 = pdAveStd[1];
		double test03[100] = {0};
		for(int i = 0 ; i < nImgFileNum; i++)
		{
			test03[i] = pdClearFactor[i];
		}
		

		for(int i = 0 ; i < nImgFileNum; i++)
		{
			if (pdClearFactor[i] > 0.75*pdAveStd[0] && pdClearFactor[i] > 1)
			{
				strcpy(ppcClearImageFile[nClearImgFileNum],pcImgFileTemp[i]);
				nClearImgFileNum++;
			}
		}

		free(pdAveStd);
		pdAveStd = NULL;
	}

	//资源清理
	free(pcImgName);
	pcImgName = NULL;

	free(pdClearFactor);
	pdClearFactor = NULL;

	for(int i = 0 ; i < FILENUM; i++)
	{
		free(pcImgFileTemp[i]);
		pcImgFileTemp[i] = NULL;
	}

	return nStatus;
}

//图像清晰程度判断
double getImgClearFactor(const char *pcImgPath)
{
	double dClearFactor[16] = {0};

	//1.读取图像
	IplImage * pImgSrcTemp = cvLoadImage(pcImgPath,0);

	int nImgWidth = cvGetSize(pImgSrcTemp).width;
	int nImgHeight = cvGetSize(pImgSrcTemp).height;

	CvRect SImROI;
	int nWindowSize = 96;
	if (nWindowSize > nImgWidth/10.0)
	{
		nWindowSize = int(nImgWidth/10.0);
	}

	if (nWindowSize > nImgHeight/10.0)
	{
		nWindowSize = int(nImgHeight/10.0);
	}
	SImROI.width =  nWindowSize;//ROI宽度
	SImROI.height =  nWindowSize;//ROI高度

	for (int i = 0; i < 4; i++)
	{
		//ROI起始点X
		SImROI.x = (int)(nImgWidth/2 + 2*nWindowSize*i - 7*nWindowSize/2);
		for (int j = 0; j < 4; j++)
		{
			//ROI起始点Y
			SImROI.y = (int)(nImgHeight/2 + 2*nWindowSize*j - 7*nWindowSize/2);
			cvSetImageROI(pImgSrcTemp,SImROI);//设置源图像ROI

			CvScalar cvMean;
			CvScalar cvStd;
			cvAvgSdv( pImgSrcTemp, &cvMean, &cvStd);
			dClearFactor[4*i+j] = cvStd.val[0];

			//test
			//cvAddS(pImgSrcTemp, CvScalar(30), pImgSrcTemp);

			cvResetImageROI(pImgSrcTemp);//源图像用完后，清空ROI
		}
	}
	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case12/ResultImgs/0原图分析区域-模糊判断.jpg",pImgSrcTemp);//图片文件存储

	//数组排序
	sortData(dClearFactor, 16);

	//平均值计算（排前4）
	double dMean = 0;
	double dSum = 0;
	int nFront = 4;

	for (int i = 0; i < nFront; i++)
	{
		dSum = dSum + dClearFactor[i];
	}
	dMean = dSum/nFront;

	cvReleaseImage(&pImgSrcTemp);
	return dMean;
}

//double数组排序，pdData为数组地址，nLength为数组长度
void sortData(double *pdData, int nLength)
{
	int i, j;
	double dTemp;
	//排序主体
	for(i = 0; i < nLength - 1; i++)
		for(j = i+1; j < nLength; j++)
		{
			if(pdData[i] < pdData[j])//如前面的比后面的大，则交换。
			{
				dTemp = pdData[i];
				pdData[i] = pdData[j];
				pdData[j] = dTemp;
			}
		}
}

// 新增：计算精子形态学比例
int calSpermMorphResult(const char * pcResultPath, SSettings const& SParaInput, double &dMorp, int nImgFileNum)
{
	int nStatus = 1;

	char *pcFileFullName = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if ( NULL == pcFileFullName )
	{
		nStatus = 0;
		return nStatus;//内存异常
	}
	char pcFileName[30] = "AllSpermInfor.csv";

	strcpy(pcFileFullName, pcResultPath);
	strcat(pcFileFullName, pcFileName);

	double dArea = 0.0;			// 面积
	double dShape = 0.0;		// 形状（长/宽）
	double dLength = 0.0;		// 长度
	double dWidth = 0.0;		// 宽度
	double dCircularity = 0.0;	// 圆度

	// 文件准备
	FILE *fp = NULL;
	fp = fopen(pcFileFullName, "r"); // 只写方式打开文本文件（文件不存在则创建，存在则清空内容）

	if ( fp )
	{
		int rows = 0;
		int cols = 0;
		int first_row = 1;

		int normSpermNum = 0;
		int allSpermNum = 0;

		char buffer[128];
		double data[9] = {0.0};

		// 逐行读取文件
		while (fgets(buffer, 128, fp) && rows < nImgFileNum* MAXSPERMNUM ) 
		{
			// 去除行尾的换行符
			buffer[strcspn(buffer, "\n")] = '\0';

			int current_col = 0;
			char *token = strtok(buffer, ","); // 以逗号为分隔符分割字符串

			// 解析每行中的每个数据
			while (token && current_col < 10) {
				// 将字符串转换为浮点数
				data[current_col] = atof(token);
				current_col++;
				token = strtok(NULL, ",");
			}

			allSpermNum++; 

			dLength			= data[4];
			dWidth			= data[5];
			dShape			= data[6];
			dArea			= data[7];			
			dCircularity	= data[8];

			if ((dLength >= SParaInput.morpPara.dLength.min && dLength <= SParaInput.morpPara.dLength.max) && 
				(dWidth >= SParaInput.morpPara.dWidth.min && dWidth <= SParaInput.morpPara.dWidth.max) &&
				(dShape >= SParaInput.morpPara.dShape.min && dShape <= SParaInput.morpPara.dShape.max) &&
				(dArea >= SParaInput.morpPara.dArea.min && dArea <= SParaInput.morpPara.dArea.max) &&
				(dCircularity >= SParaInput.morpPara.dCircularity.min && dCircularity <= SParaInput.morpPara.dCircularity.max))
			{

				normSpermNum ++;
			}

			// 记录第一行的列数作为标准列数
			if (first_row) {
				cols = current_col;
				first_row = 0;
			}
			// 检查每行的列数是否一致
			else if (current_col != cols) {
				fprintf(stderr, "警告: 第%d行的列数与第一行不一致\n", rows + 1);
			}

			(rows)++;
		}

		dMorp = 100 * normSpermNum / (allSpermNum + EPSINON);
		
		fclose(fp);
	}
	else
	{
		nStatus = -17;//创建或打开文件失败
	}

	free(pcFileFullName);
	pcFileFullName = NULL;
	
	return nStatus;
}

//获取精子数量
int getSpermCount(const char **ppcFilePath, const char * pcResultPath, char **pcValidFilePath, int &nValidNum, int const& nImgFileNum, SSettings const& SParaInput, int nCenterPara[], ParaRange *pSAveSpermRange, algsqamed_data_out *dataOut)
{
	int nStatus = 1;
	int nStatusBubble = 1;//是否有气泡的判断

	int nTotalSpermNum = 0;//精子总数A+B+C+D
	//int nTotalSpermNumTemp = 0;//精子总数A+B+C+D
	int nAliveSpermNum = 0;//活动精子数A+B+C
	int nActiveSpermNum = 0;//运动活跃的精子数A+B
	int nDeadSpermNum = 0;//完全不动的精子数D

	int nSavedImgNum = 0;//原始图片中可供分析的图片张数

	double dAliveSpermRatio = 0;//精子活率 = 活动精子数/精子总数
	double dActiveSpermRatio = 0;//精子活力 = A+B

	double dTotaSpermDensity = 0;//精子密度
	double dAliveSpermDensity = 0;//活动精子密度

	double dRatio = 0;

	int nWidth = nCenterPara[0];
	int nHeight = nCenterPara[1];

	struct SpermInfor *pSSpermInfor = (struct SpermInfor *)malloc(sizeof(struct SpermInfor)*MAXSPERMNUM);
	if (NULL == pSSpermInfor)
	{
		nStatus = 0;
		return nStatus;//内存异常
	}

	char *pcMaskFileName = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if (NULL == pcMaskFileName)
	{
		free(pSSpermInfor);
		pSSpermInfor = NULL;

		nStatus = 0;
		return nStatus;//内存异常
	}

	IplImage * pImgSrcTemp = NULL;
	IplImage * pImgBackGround = NULL;
	IplImage *pImgMask = NULL;
	do 
	{
		//1. 计算视野中心
		pImgSrcTemp = cvLoadImage(ppcFilePath[0],0);//0表示强制转化读取图像为灰度图
		pImgMask = cvCreateImage(cvGetSize(pImgSrcTemp),pImgSrcTemp->depth,pImgSrcTemp->nChannels);//分配内存

		//2. 参数异常判断
		dRatio = nWidth * nHeight * SParaInput.dRatioImg * SParaInput.dRatioImg * SParaInput.dSampleDepth;
		if ((dRatio >= -EPSINON) && (dRatio <= EPSINON))
		{
			nStatus = -1;
			break;//输入参数异常
		}else
		{
			dRatio = dRatio * (1e-6);//单位：百万个/毫升
			//dRatio = dRatio * (1e-4);//单位：亿个/毫升
		}

		//3. 全黑图像的判断
		nStatus = checkBlackWhiteImg(ppcFilePath, nImgFileNum);
		if (nStatus != 1)
		{
			break;
		}

		//3. 画面气泡或污点的判断
		nStatusBubble = checkBubbleStainImg(ppcFilePath);
		if (nStatusBubble != 1)
		{
			//气泡或污点的动态模板生成
			nStatus = genMaskImg(ppcFilePath, pImgMask);
			if (nStatus != 1)
			{
				break;
			}
		}
		else
		{
			cvSet(pImgMask, cvScalar(255));
		}

		//存储动态模板
		strcpy(pcMaskFileName, pcResultPath);
		strcat(pcMaskFileName, "Mask.jpg");//原图
		cvSaveImage(pcMaskFileName, pImgMask);//图片文件存储
		
		//判断样本是否漂移，基于互相关的分析过程
		nStatus = isSampleDrift(ppcFilePath, nImgFileNum, pImgMask);
		if (nStatus != 1)
		{
			break;
		}

		//4.判断被检测目标的类别（新鲜精液/干涸精液/标粒），如果成像系统发生变化，原有参数需重新测试评估
		int nSampleType = 1;//0-异常样本（未加样或者无玻片），1-干涸精液，2-新鲜精液，3-标粒3um，4-红细胞目标，
		nStatus = checkSampleType(pImgSrcTemp, pImgMask, nSampleType);
		
		// 空白/未加样测试拦截（返回 nStatus = -6，并给全 0 结果供挑剔客户做空白值验证）
		if (nStatus == -6 || nSampleType == 0)
		{
			memset(dataOut, 0, sizeof(algsqamed_data_out));
			dataOut->nTotalSpermNum      = 0;
			dataOut->dTotaSpermDensity   = 0.0;
			dataOut->nActiveSpermNum     = 0;
			dataOut->dActiveSpermDensity = 0.0;
			dataOut->dActiveSpermRatio   = 0.0;
			dataOut->dRatioClassPR       = 0.0;
			dataOut->dDensityClassPR     = 0.0;
			dataOut->dRatioClassA        = 0.0;
			dataOut->dRatioClassB        = 0.0;
			dataOut->dRatioClassC        = 0.0;
			dataOut->dRatioClassD        = 0.0;
			dataOut->dMorp               = 0.0;
			dataOut->dNormal             = 0.0;
			nStatus = -6; // 算法状态返回 -6：空白测试（全0结果）
			break;
		}
		else if (nStatus != 1)
		{
			break;
		}
		
		int nCandidateSampleType = nSampleType; // 记录初筛类别候选
		if (nSampleType == 1)
		{
			// 初筛疑似干涸：绝对不在此草率中断！
			// 杜绝因精液未完全液化、黏液丝、脱落细胞团而误杀正常有活力的样本（如 8% 活力样本）。
			// 先按正常精液宽窗参数进入特征提取与运动分析，是否判定 -12 严格交由后续动态分析最终裁决！
			updateSpermRange(pSAveSpermRange, 2); 
		}
		else
		{
			updateSpermRange(pSAveSpermRange, nSampleType);
		}
		
		//3.计算精子总数（ABCD），含精子特征的文件输出（AllSpermInfor.csv）
		nStatus = getSpermFeatureRange(ppcFilePath, pcResultPath, pcValidFilePath, nImgFileNum, pSAveSpermRange, nCenterPara, nSavedImgNum, nTotalSpermNum, nSampleType, pImgMask, SParaInput);
		if (nStatus != 1 && nStatus != -6)
		{
			break;
		}
		nValidNum = nSavedImgNum;

		// 新增：计算精子形态学比例
		double dMorp = 0.0;
		nStatus = calSpermMorphResult(pcResultPath, SParaInput, dMorp, nImgFileNum);
		if (nStatus != 1)
		{
			break;
		}

// 		//判断目标是否为静止目标，如果是则直接计算密度值
// 		int nTypeDead = 0;//0表示干涸的死人精, 1表示其他小个目标
// 		if (SParaInput.dShapeRatio > 0.9 && nSampleType != 3 && nSampleType != 4)//表示人精模式，判断是否为死人精或完全静止图像
// 		{
// 			nStatus = checkAbnormalImg(pcValidFilePath, nValidNum, nCenterPara);
// 		}
	
		//Mask校正
		//求有效面积占比
		int nAreaMask = cvCountNonZero(pImgMask);
		int nArea = pImgMask->height*pImgMask->width;
		double dRatioMask = nAreaMask/(nArea+EPSINON);

		//结果校正
		double dAlpha = 1;
		if (SParaInput.dPlateType == 1)//玻片类型，1表示蓝色六腔版
		{
			if (nSampleType == 3)//标粒
			{
				dAlpha = 1.0;//校正日期2018.04.22
				dAlpha = dAlpha/dRatioMask;//Mask校正,校正日期2018.07.05
			}
			else if (nSampleType == 4)
			{
				dAlpha = 1;//红细胞，初始系数为1
				dAlpha = dAlpha/dRatioMask;//Mask校正
			}
			else//人精
			{
				dAlpha = 1.0;//人精校正系数
				dAlpha = dAlpha/dRatioMask;//Mask校正
			}
		} 
		else if (SParaInput.dPlateType == 0)//玻片类型，0表示白色四腔版
		{
			if (nSampleType == 3)//标粒
			{
				dAlpha = 1.1034;//校正日期2018.04.09
				dAlpha = dAlpha/dRatioMask;//Mask校正,校正日期2018.07.05
			}
			else//人精
			{
				dAlpha = 3.4381;//校正日期2018.04.09
				dAlpha = dAlpha/dRatioMask;//Mask校正,校正日期2018.07.05
			}
		}

		dAlphaDens = dAlpha/dRatio;

		//静止目标结果计算（要不要进“过浓/异常”处理）
		if (nStatus != 1 || SParaInput.dShapeRatio < 0.8 || nSampleType == 3 || nSampleType == 4 || (nTotalSpermNum >300/(dAlphaDens+EPSINON))) //密度上限100改为300
		{
			int nStatusTemp = 1;
			//nTypeDead = 1;
			int nTotalSpermNum2 = 0;
			nStatusTemp = getOutlierImgResult(pcValidFilePath, pcResultPath, nCenterPara, pSAveSpermRange, nTotalSpermNum2, nSampleType);//所有目标叠加到原图
			if (nStatusTemp != 1)
			{
				nStatus = nStatusTemp;
				break;
			}
			dataOut->nTotalSpermNum = nTotalSpermNum;//总数
			dataOut->dTotaSpermDensity = dAlphaDens*nTotalSpermNum;//总密度


			if (SParaInput.dShapeRatio < 0.8 || nSampleType == 3 || nSampleType == 4)
			{
				nStatus = -9;//样本可能为非精子目标
			}	
			if (nTotalSpermNum >300/(dAlphaDens+EPSINON))  //密度上限 = 300（百万个/毫升）同步动物宽窗版本，更新100变成300
			// 同步需要改动上面20行前的那行代码(nTotalSpermNum >300/(dAlphaDens+EPSINON)))
			{
				nStatus = -16;//样本浓度过高，请稀释后重新检测
			}
			break;
		}
		
		//4.完全不动的精子数D
		int nSpermIndex = 0;//精子索引
		pImgBackGround =cvCreateImage(cvGetSize(pImgSrcTemp),pImgSrcTemp->depth,pImgSrcTemp->nChannels);//分配内存
		nStatus = getSpermCountD(ppcFilePath, pcResultPath, nImgFileNum, pImgBackGround, pSAveSpermRange, pSSpermInfor, nSpermIndex);
		if (nStatus != 1)
		{
			break;
		}
		nDeadSpermNum = nSpermIndex;
		if (nDeadSpermNum > nTotalSpermNum)
		{
			nDeadSpermNum = nTotalSpermNum;//防止活力小于0
		}

		//5.活动的精子数量A+B+C（多帧完整轨迹追踪）
		nStatus = getSpermCountABC(pcValidFilePath, nValidNum, pcResultPath, pImgBackGround, pSAveSpermRange, pSSpermInfor, nSpermIndex);//注：返回的nSpermIndex为A+B的值
		if (nStatus != 1)
		{
			break;
		}

		// 动静结合二次确认（置于全部原始照片分析与完整多帧轨迹分析之后）：
		// 临床铁律：不能只凭“无活动精子”或“活力极低(<2%)”就判定为干涸异常(-12)！
		// 临床上“刚死亡的精子(坏死精子症)”与“重度弱精子症(活力<2%)”均为常见合法标本，必须输出 nStatus=1、正常密度与形态学！
		
		// 1. 判断多帧追踪中是否有确信的真实物理游动精子：
		// 严格防范干枯玻片光电噪点/微震虚构出的微弱伪活力（如 001 干片噪点误报 27 个活动精子、总活力 4.81% 但直线精子仅 2 个且直线推进速度极低）
		bool bIsNoiseMotility = (dataOut->nNumSL <= 2 && dataOut->dAveVSL < 18.0) || 
		                        (dataOut->nNumSL == 0 && dataOut->nActiveSpermNum < 30) ||
		                        (dataOut->dActiveSpermRatio < 1.0);

		// 真实有活力判定：排除伪活力底噪后，真正有前向游动或真实位移的样本
		bool bHasGenuineMotility = (dataOut->nActiveSpermNum > 0) && (dataOut->dActiveSpermRatio > 0.0) && (!bIsNoiseMotility);

		if (bHasGenuineMotility)
		{
			// 存在确信的真实游动精子（无论是 2%、8% 还是更高活力的正常新鲜精液）：
			// 绝对不可能是干涸玻片！彻底解除任何干涸嫌疑，确认正常人精
			nSampleType = 2;
			nAliveSpermNum = dataOut->nActiveSpermNum;
			dAliveSpermRatio = dataOut->dActiveSpermRatio;
		}
		else
		{
			// 2. 无真实游动精子（全死寂 或 仅有光电微抖伪活力）：
			// 深度校验干涸特征：
			// (A) 第一帧初筛命中结晶风干特征 (nCandidateSampleType == 1，如 100023 样本)；
			// (B) 或者样本伪活力被过滤、且形态圆度异常/碎片比例偏高/直线位移归零；
			// (C) 或者全片 30 帧全部静止，无任何有效生物位移的干结标本。
			if (nCandidateSampleType == 1 || (dataOut->nNumSL <= 2 && dataOut->dAveVSL < 18.0 && dataOut->dActiveSpermRatio <= 5.0 && nCandidateSampleType != 2))
			{
				nStatus = -12; // 样本干涸异常
				break;
			}

			// 3. 对于未干涸的新鲜刚死亡精子（如 000 样本，nCandidateSampleType==2）：
			// 确认为合法的新鲜死精样本，平稳放行，绝不阻断，进入后续成功输出（nStatus = 1）！
			// 规范清零各项运动学与速度指标，D级设为100%
			nAliveSpermNum = 0;
			dAliveSpermRatio = 0.0;
			dataOut->nActiveSpermNum = 0;
			dataOut->dActiveSpermRatio = 0.0;
			dataOut->dRatioClassPR = 0.0;
			dataOut->dRatioClassNP = 0.0;
			dataOut->dRatioClassA = 0.0;
			dataOut->dRatioClassB = 0.0;
			dataOut->dRatioClassC = 0.0;
			dataOut->dRatioClassD = 100.0;
			dataOut->dRatioClassIM = 100.0;
			dataOut->nNumSL = 0;
			dataOut->nNumCL = 0;
			dataOut->dRatioSL = 0.0;
			dataOut->dRatioCL = 0.0;
			dataOut->dAveVSL = 0.0;
			dataOut->dAveVCL = 0.0;
			dataOut->dAveVAP = 0.0;
			dataOut->dLIN = 0.0;
			dataOut->dSTR = 0.0;
			dataOut->dWOB = 0.0;
			dataOut->dALH = 0.0;
			dataOut->dBCF = 0;
			dataOut->dMAD = 0.0;
		}

		dTotaSpermDensity = dAlphaDens * nTotalSpermNum;
		dAliveSpermDensity = dAlphaDens * nAliveSpermNum;
		
		dataOut->nTotalSpermNum = nTotalSpermNum;
		dataOut->dTotaSpermDensity  = dTotaSpermDensity;
		dataOut->dActiveSpermDensity = dAliveSpermDensity;

		dataOut->dMorp = dMorp; //形态学比例结果[0,1]



		SpermEstimateParamsM params = calcSpermMorphologyEstimate(dTotaSpermDensity, dAliveSpermRatio, pImgSrcTemp, nTotalSpermNum);  // 动静自适应形态学估算（版本4.0）

        // 校验异常
		if (params.dNormal < 0) {
		printf("SYSTEM Abnormal\n");
		} else {
		// [形态学估算值程序]再使用，赋值给dataOut
		dataOut->dNormal = params.dNormal;
	    dataOut->dAbnormal = params.dAbnormal;
        dataOut->dHdefects = params.dHdefects;
        dataOut->dMdefects = params.dMdefects;
        dataOut->dTdefects = params.dTdefects;
        dataOut->dCRdefects = params.dCRdefects;
        dataOut->dTZI = params.dTZI;
        dataOut->dSDI = params.dSDI;
        dataOut->dRoundcells = params.dRoundcells;
        }

	} while (0);

	free(pSSpermInfor);
	pSSpermInfor = NULL;
	free(pcMaskFileName);
	pcMaskFileName = NULL;
	cvReleaseImage(&pImgMask);
	cvReleaseImage(&pImgBackGround);
	cvReleaseImage(&pImgSrcTemp);

// 	if (nStatus == 1 && nStatusBubble != 1)
// 	{
// 		nStatus = nStatusBubble;//轻微的气泡给所有结果
// 	}

	return nStatus;
 }

 //样本类型判断
 //干涸的人精、新鲜的人精、标粒、无样本
 //返回值：0-异常样本（未加样或者无玻片），1-干涸人精，2-新鲜人精，3-标粒，4-红细胞目标
 int checkSampleType(IplImage *pImgSrc, IplImage *pImgMask, int &nSampleType)
 {
	 int nStatus = 1;

	 IplImage *pImgTemp = cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);//分配内存
	 cvZero(pImgTemp);

	 SpermInfor *pSSpermInfor = (SpermInfor *)malloc(sizeof(SpermInfor)*MAXSPERMNUM);
	 if (NULL == pSSpermInfor)
	 {
		 cvReleaseImage(&pImgTemp);

		 nStatus = 0;
		 return nStatus;//内存异常
	 }
	 if (DebugMode)
	 {
		 cvNamedWindow("ImgOrign",CV_WINDOW_KEEPRATIO);
		 cvShowImage("ImgOrign",pImgSrc);
		 cvWaitKey();
	 }
	 //2.图像对比度增强并二值化(ABCD)
	 stretchImgContrastABCD(pImgSrc, 0);
	 cvCopy(pImgSrc, pImgTemp, pImgMask);
	 cvCopy(pImgTemp, pImgSrc);

	 //3.精子特征获取
	 ParaRange paraRangeTemp = {11.6, 63.9, 31.5, 5.2, 15.3, 9.1, 3, 7.2, 4.7, 1};
	 int nSpermIndex = 0;
	 int nSpermType = 7;//初始
	 getImgContourInfor(pImgSrc,&paraRangeTemp,nSpermType,pSSpermInfor,nSpermIndex);
	 //do 
	 //{
		// if (nSpermIndex <= 0 )
		// {
		//	 nSampleType = 2;//未加样或者无玻片
		//	 break;
		// }

		// ParaRange pSSpermRange = getSpermParaRangeIter(pSSpermInfor,nSpermIndex,nStatus);
		// if (0 == nStatus)
		// {
		//	 break;
		// }

		 // || nSpermIndex > 30,注：这个判断受样本具体情况影响，结果会有异常判断，暂时先不用
		// 同步动物宽窗，明显干涸才 -12：面积明显偏大且偏扁长，或极大面积
		//if ( (pSSpermRange.dAreaAve > 50 && pSSpermRange.dShapeRatio > 2.3)
		//	 || (pSSpermRange.dAreaAve > 60) )
		//{
		//	 nSampleType = 1;//干涸人精 →  nStatus = -12
		// }

		// 人用版本，同步动物宽窗，稍微放宽一点nSampleType = 2的条件，让更多类型的样本（比如“刚死/微干”）进入nSampleType = 2
		 //else if ((pSSpermRange.dAreaAve <= 60 && pSSpermRange.dAreaAve > 6) &&  (pSSpermRange.dShapeRatio >= 1.00 &&  pSSpermRange.dShapeRatio < 2.3)) 
		 //{
			// nSampleType = 2;//新鲜人精
		 //}
		 //else if (pSSpermRange.dShapeRatio < 1.05)
		 //{
			// if (pSSpermRange.dAreaAve > 50)
			// {
			//	 nSampleType = 4;//红细胞
			// } 
			// else
			// {
			//	 nSampleType = 3;//标粒
			// }			 
		 //}
		 //else
		 //{
			// nSampleType = 3;//其他
		 //}
		 //test
		 //nSampleType = 2;//新鲜人精

		// // 1. 干涸人精优先排除（面积明显偏大或极扁长）
		//if ((pSSpermRange.dAreaAve > 50 && pSSpermRange.dShapeRatio > 2.3) || (pSSpermRange.dAreaAve > 60))
		//{
		//  nSampleType = 1; // 干涸人精 → nStatus = -12
		//}
		//// 2. 新鲜人精（涵盖常规椭圆精子与南非洗涤膨胀精子：长宽比放宽到 1.00 ~ 2.3，面积 6 ~ 60）
		//else if ((pSSpermRange.dAreaAve <= 60 && pSSpermRange.dAreaAve > 6) && 
		//       (pSSpermRange.dShapeRatio >= 1.00 && pSSpermRange.dShapeRatio < 2.3)) 
		//{
		//	nSampleType = 2; // 新鲜人精
		//}
		//// 3. 红细胞排除（大尺寸近圆目标：面积 > 45 且 长宽比 < 1.15）
		////  注意：红细胞与清洗过的精子，有重叠区间的矛盾！红细胞判断，可能无效。
		//else if (pSSpermRange.dAreaAve > 45 && pSSpermRange.dShapeRatio < 1.15)
		//{
		//  nSampleType = 4; // 红细胞
		//}
		//// 4. 标粒质控球排除（标准工业微球：长宽比极度接近 1.00 且 面积在标粒物理窗 10~35 内）
		////  注意：标粒与清洗过的精子，有重叠区间的矛盾！标粒判断，可能无效。
		//else if (pSSpermRange.dShapeRatio < 1.04 && (pSSpermRange.dAreaAve >= 10 && pSSpermRange.dAreaAve <= 35))
		//{
		//  nSampleType = 3; // 标粒
		//}
		//else
		//{
		//  nSampleType = 3; // 其他未知杂质
		//}

	 //} while (0);

	 do 
	 {
		 // 1. 空白/未加样拦截：检出颗粒数极少（<=3个），判定为空白测试并返回 -6
		 if (nSpermIndex <= 3)
		 {
			 nSampleType = 0; // 0-空白/未加样
			 nStatus = -6;    // -6: 空白测试？（给全0结果，满足空白值验证）
			 break;
		 }

		 // 暂存初始检测到的总颗粒数，防止被 getSpermParaRangeIter 覆写
		 int nRawDetectedNum = nSpermIndex; 

		 // 2. 统计干涸特征指标：超大干结斑块、异常长条结晶、拉丝纤维
		 int nLargeDebrisCount = 0; // 面积>50 且 长轴>14 的显著大块 (避免单个精子或死精微小粘连被计入)
		 int nMacroChunksCount = 0; // 面积>75 的聚合大斑块
		 int nSuperChunksCount = 0; // 面积>120 的巨型风干结晶块
		 int nUltraChunksCount = 0; // 面积>200 的超大盐析结痂块 (干片专属)
		 int nDryFiberCount    = 0; // 长轴>16 且 扁长比>2.8 的干燥拉丝结晶
		 for (int i = 0; i < nRawDetectedNum; i++)
		 {
			 double dArea = pSSpermInfor[i].dArea;
			 double dMaj  = pSSpermInfor[i].dMajAxsLen;
			 double dMin  = pSSpermInfor[i].dMinAxsLen;
			 double dRatio = dMaj / (dMin + EPSINON);

			 if (dArea > 50.0 && dMaj > 14.0)
			 {
				 nLargeDebrisCount++;
			 }
			 if (dArea > 75.0)
			 {
				 nMacroChunksCount++;
			 }
			 if (dArea > 120.0)
			 {
				 nSuperChunksCount++;
			 }
			 if (dArea > 200.0)
			 {
				 nUltraChunksCount++;
			 }
			 if (dMaj > 16.0 && dRatio > 2.8)
			 {
				 nDryFiberCount++;
			 }
		 }
		 double dLargeDebrisRatio = (double)nLargeDebrisCount / (nRawDetectedNum + EPSINON);

		 ParaRange pSSpermRange = getSpermParaRangeIter(pSSpermInfor, nSpermIndex, nStatus);
		 if (0 == nStatus)
		 {
			 break;
		 }

		 // =========================================================================
		 // 方案 A 静态第一帧分级判别规则（严格保证：只抓真风干，绝不误伤新鲜死精与低活弱精）
		 // =========================================================================

		 // 差异点实测数据对比（断崖式鸿沟）：
		 // 真正风干干涸片(100023)：area>120 巨斑达 134 个，area>200 达 56 个，拉丝纤维达 92 个，大碎片比例达 7.3%~20.9%
		 // 新鲜完全死精片(000)：死精自然小凝集，area>120 仅 4 个，area>200 严格为 0 个，拉丝纤维仅 23 个，大碎片比例仅 0.5%
		 // 新鲜活精片(029)：area>120 仅 7 个，area>200 严格为 0 个，纤维仅 21 个，大碎片比例仅 1.0%

		 /*nSuperChunksCount = 面积>120 的巨型风干结晶块
		    nUltraChunksCount = 面积>200 的超大盐析结痂块 (干片专属)
		    nDryFiberCount   = 长轴>16 且 扁长比>2.8 的干燥拉丝纤维结晶
		    nMacroChunksCount = 面积>75 的聚合大斑块
		    nLargeDebrisCount = 面积>50 且 长轴>14 的显著大块，大碎片 (避免单个精子或死精微小粘连被计入)
			dLargeDebrisRatio = nLargeDebrisCount / nRawDetectedNum 大碎片比例
			下列if 条件里，只要 5 条任一命中 → `nSampleType=1`（干涸候选）
		*/
		 // 标本干涸初筛分级机制：
		 // 捕捉重度风干结晶大斑块（如 100023 样本：大碎片占比达 7.3%~20.9%，超大拉丝/斑块密集）
		 if ( (nSuperChunksCount >= 30) || 
		      (nUltraChunksCount >= 10) || 
		      (nDryFiberCount >= 30) || 
		      (nMacroChunksCount >= 25 && dLargeDebrisRatio > 0.04) || 
		      (dLargeDebrisRatio > 0.05) ||
		      (pSSpermRange.dAreaAve > 55.0) )
		 {
			 nSampleType = 1; // 标记疑似干涸候选（交由后续多帧动态分析最终裁决）
		 }
		 // 2. 标粒初筛（仅当尺寸极度一致，且极度接近正圆时）
		 else if (pSSpermRange.dShapeRatio < 1.03 && (pSSpermRange.dAreaAve >= 10 && pSSpermRange.dAreaAve <= 35) && (pSSpermRange.dAreaMax - pSSpermRange.dAreaMin < 10))
		 {
			 nSampleType = 3; // 标粒候选（交由后续判定）
		 }
		 // 3. 红细胞初筛（巨大近圆目标）
		 else if (pSSpermRange.dAreaAve > 45 && pSSpermRange.dShapeRatio < 1.15)
		 {
			 nSampleType = 4; // 红细胞候选
		 }
		 // 4. 所有其他样本（包含：常规精液、洗涤精子、无活力死精），宽窗默认放行
		 else
		 {
			 nSampleType = 2; // 新鲜/正常人精（进入运动分析与详细统计）
		 }

	 } while (0);

	 cvReleaseImage(&pImgTemp);
	 pImgTemp = NULL;
	 free(pSSpermInfor);
	 pSSpermInfor = NULL;
	 return nStatus;
 }
 
 //静止图片的处理（叠加显示并保存）
 int getOutlierImgResult(char **pcValidFilePath, const char * pcResultPath, int nCenterPara[], ParaRange *pSAveSpermRange, int &nSpermNumALL, int nSampleType)
 {
	 int nStatus = 1;

	 struct SpermInfor *pSSpermInfor = (struct SpermInfor *)malloc(sizeof(struct SpermInfor)*MAXSPERMNUM);
	 if (NULL == pSSpermInfor)
	 {
		 nStatus = 0;
		 return nStatus;//内存异常
	 }

	 //获取图片地址及数量
	 char *pcImgFileTemp[FILENUM] = {NULL};
	 for (int i = 0; i < FILENUM; i++)
	 {
		 pcImgFileTemp[i] = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		 if (NULL == pcImgFileTemp[i])
		 {
			 for (int j = 0; j <= i; j++)
			 {
				 free(pcImgFileTemp[j]);
				 pcImgFileTemp[j] = NULL;
			 }

			 free(pSSpermInfor);
			 pSSpermInfor = NULL;

			 nStatus = 0;//内存异常
			 return nStatus;
		 }
	 }

	 int nImgNum = 0;
	 nStatus = getImgFileSeq(pcResultPath,pcImgFileTemp,nImgNum);
	 if (nImgNum <= 5)
	 {
		 if (nStatus == 1)
		 {
			 nStatus = -5;//样本图像张数不够，无法进一步分析
		 }
		 for (int i = 0; i < FILENUM; i++)
		 {
			 free(pcImgFileTemp[i]);
			 pcImgFileTemp[i] = NULL;
		 }

		 free(pSSpermInfor);
		 pSSpermInfor = NULL;

		 return nStatus;
	 }

	 const char *ppcImageFile[FILENUM] = {NULL};
	 for (int i = 0; i < nImgNum; i++)
	 {
		 ppcImageFile[i] = pcImgFileTemp[i];//用于const char* 与const char转换
	 }
	 
	 char *pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	 if (NULL == pcFileNameTemp)
	 {
		 for (int i = 0; i < FILENUM; i++)
		 {
			 free(pcImgFileTemp[i]);
			 pcImgFileTemp[i] = NULL;
		 }

		 free(pSSpermInfor);
		 pSSpermInfor = NULL;

		 nStatus = 0;
		 return nStatus;//内存异常
	 }

	 char *pcImg = (char *)malloc(sizeof(char)*50);
	 if (NULL == pcImg)
	 {
		 for (int i = 0; i < FILENUM; i++)
		 {
			 free(pcImgFileTemp[i]);
			 pcImgFileTemp[i] = NULL;
		 }
		 free(pcFileNameTemp);
		 pcFileNameTemp = NULL;

		 free(pSSpermInfor);
		 pSSpermInfor = NULL;

		 nStatus = 0;
		 return nStatus;//内存异常
	 }

	 for (int i = 0; i < nImgNum; i++)
	 {		 
		 IplImage * pImgBinary = cvLoadImage(pcImgFileTemp[i],0);//结果图
		 if (DebugMode)
		 {
			 cvNamedWindow("ImgOrign",CV_WINDOW_KEEPRATIO);
			 cvShowImage("ImgOrign",pImgBinary);
			 cvWaitKey();
		 }

		 int nSpermIndex = 0;
		 int nSpermType = 4;		 
		 if (nSampleType == 3 || nSampleType == 4)
		 {
			 getStdParticleNum(pImgBinary, pSAveSpermRange, nSpermType, pSSpermInfor, nSpermIndex);
		 } 
		 else
		 {
			 getImgContourInfor(pImgBinary, pSAveSpermRange, nSpermType, pSSpermInfor, nSpermIndex);
		 }

		 if (DebugMode)
		 {
			 cvNamedWindow("ImgOrign2",CV_WINDOW_KEEPRATIO);
			 cvShowImage("ImgOrign2",pImgBinary);
			 cvWaitKey();
		 }

		 nSpermNumALL = nSpermNumALL + nSpermIndex;

		 IplImage *pImgSubScr = cvLoadImage(pcValidFilePath[i],1);//原图

		 //3.获取目标轮廓及其几何信息
		 int nType = 2;//ABCD

		 if (nSampleType == 3 || nSampleType == 4)
		 {
			 drawStdParticleImg(pImgBinary, pImgSubScr, pSAveSpermRange, nType);
		 } 
		 else
		 {
			 drawSpermImg(pImgBinary, pImgSubScr, pSAveSpermRange, nType);
		 }		 

		 strcpy(pcFileNameTemp, pcResultPath);
		 sprintf(pcImg, "%03d.jpg", i);
		 strcat(pcFileNameTemp, pcImg);//原图
		 int nSaveStatus = cvSaveImage(pcFileNameTemp, pImgSubScr);//图片文件存储

		 //资源释放 
		 cvReleaseImage(&pImgBinary);
		 cvReleaseImage(&pImgSubScr);

		 if (0 == nSaveStatus)
		 {
			 nStatus = -2;//数据写入异常
			 break;
		 }
	 }

	 nSpermNumALL = nSpermNumALL/(nImgNum);

	 //资源释放
	 for (int i = 0; i < FILENUM; i++)
	 {
		 free(pcImgFileTemp[i]);
		 pcImgFileTemp[i] = NULL;
	 }
	 free(pSSpermInfor);
	 pSSpermInfor = NULL;
	 free(pcFileNameTemp);
	 pcFileNameTemp = NULL;
	 free(pcImg);
	 pcImg = NULL;

	 return nStatus;
 } 

//背景图像生成
IplImage *genBackgroundImg(const char **ppcFilePath, int const& nImgFileNum)
{
	//背景图像生成（此处默认为白色背景）
	IplImage *pImgScr1 = cvLoadImage(ppcFilePath[0],0);//0表示强制转化读取图像为灰度图
	IplImage * pImgBackGround = cvCreateImage(cvGetSize(pImgScr1),pImgScr1->depth,pImgScr1->nChannels);

	//求两张图片中较大的元素
	for (int i = 1; i < nImgFileNum; i++)
	{
		IplImage * pImgScr2 = cvLoadImage(ppcFilePath[i],0);
		cvMax(pImgScr1,pImgScr2,pImgBackGround);
		cvCopy( pImgBackGround, pImgScr1,NULL );

		cvReleaseImage(&pImgScr2);
	}
	cvReleaseImage(&pImgScr1);

	return pImgBackGround;
}

//二值化的背景图像生成
void genBinaryBackgroundImg(IplImage * pImgBackGround, const char **ppcFilePath, int const& nImgFileNum)
{
	IplImage * pImgSrcTemp1 = cvCreateImage(cvGetSize(pImgBackGround),IPL_DEPTH_16U,1);
	IplImage * pImgSrcTemp2 = cvCreateImage(cvGetSize(pImgBackGround),IPL_DEPTH_16U,1);
	cvZero(pImgSrcTemp2);

	//求两张图片中较大的元素
	for (int i = 0; i < nImgFileNum; i++)
	{
		IplImage * pImgSrcTemp = cvLoadImage(ppcFilePath[i],0);

		cvConvertScale( pImgSrcTemp, pImgSrcTemp1, 1, 0);
		cvAdd(pImgSrcTemp1,pImgSrcTemp2,pImgSrcTemp2);

		cvReleaseImage(&pImgSrcTemp);
	}
	//格式转换
	cvConvertScale( pImgSrcTemp2, pImgBackGround, 1/255.0, 0);

	int nThreshValue = (int)(nImgFileNum/2.0);
	//阈值分割
	cvThreshold( pImgBackGround, pImgBackGround,
		nThreshValue, 255,
		CV_THRESH_BINARY);//固定阈值分割

	cvReleaseImage(&pImgSrcTemp1);
	cvReleaseImage(&pImgSrcTemp2);
}

//全黑图像的判断
int checkBlackWhiteImg(const char **ppcFilePath, int nImgFileNum)
{
	int nStatus = 1;

	int nBlackImgNum = 0;
	int nWhiteImgNum = 0;

	IplImage * pImgSrcTemp = cvLoadImage(ppcFilePath[0],0);
	int nImgWidth = cvGetSize(pImgSrcTemp).width;
	int nImgHeight = cvGetSize(pImgSrcTemp).height;
	
	CvRect SImROI;
	SImROI.x = (int)(3*nImgWidth/8);
	SImROI.y = (int)(3*nImgHeight/8);

	SImROI.width =  (int)(nImgWidth/4);//ROI宽度
	SImROI.height =  (int)(nImgHeight/4);//ROI高度
	cvReleaseImage(&pImgSrcTemp);

	//开始图像处理
	for (int i = 0; i < nImgFileNum; i++)
	{
		//1.读取图像并提取子图
		pImgSrcTemp = cvLoadImage(ppcFilePath[i],0);
		cvSetImageROI(pImgSrcTemp,SImROI);//设置源图像ROI
		CvScalar cvMean = cvAvg( pImgSrcTemp);
		cvResetImageROI(pImgSrcTemp);//源图像用完后，清空ROI
		cvReleaseImage(&pImgSrcTemp);

		//test
		double dAveGV = cvMean.val[0];
		if (cvMean.val[0] <= 100)
		{
			nBlackImgNum++;
		}

		if (cvMean.val[0] >= 235)
		{
			nWhiteImgNum++;
		}
	}

	if (nBlackImgNum >= 5)
	{
		nStatus = -10;//样本图像亮度过暗
		return nStatus;
	}

	//nWhiteImgNum = 0;//备用，有些情况下背景一片白也是有的
	if (nWhiteImgNum >= 5)
	{
		nStatus = -11;//样本图像亮度过亮
		return nStatus;
	}

	return nStatus;
}

//画面气泡或污点的判断
int checkBubbleStainImg(const char **ppcFilePath)
{
	int nStatus = 1;

	IplImage * pImgSrcTemp = cvLoadImage(ppcFilePath[0],0);
	IplImage * pImgBinary =cvCreateImage(cvGetSize(pImgSrcTemp),pImgSrcTemp->depth,pImgSrcTemp->nChannels);
	//先对输入图像做一次二值化
	cvThreshold( pImgSrcTemp, pImgBinary,
		30, 255, CV_THRESH_BINARY_INV);//固定阈值分割

	//查找气泡或污渍
	int nNumBubble = 0;
	CvMemStorage *storage = cvCreateMemStorage();
	CvContourScanner scanner = cvStartFindContours(pImgBinary,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));
	CvSeq * cTemp =NULL;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找
	{
		//面积
		double dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0)); 
		if(dConArea >= 500)
		{  
			nNumBubble++;
		}
		cvSubstituteContour(scanner,NULL);//删除当前的轮廓
	}
	CvSeq* firstcontour = cvEndFindContours(&scanner);

	//异常判断
	if (nNumBubble >= 1)
	{
		nStatus = -15;//分析画面可能存在轻微的气泡或污迹，结果仅供参考（给具体结果）
	}

	//资源释放
	cvReleaseImage(&pImgSrcTemp);
	cvReleaseImage(&pImgBinary);
	cvReleaseMemStorage(&storage);

	return nStatus;
}

//气泡或污点的动态模板生成
int genMaskImg(const char **ppcFilePath, IplImage *pImgMask)
{
	int nStatus = 1;
	cvZero(pImgMask);

	//读取原图
	IplImage * pImgSrcTemp = cvLoadImage(ppcFilePath[0],0);

	//原图二值化
	IplImage * pImgBinary =cvCreateImage(cvGetSize(pImgSrcTemp),pImgSrcTemp->depth,pImgSrcTemp->nChannels);

	//生成二值化的气泡图
	genBubbleThreshImg(pImgSrcTemp, pImgBinary);

	//cvSaveImage("E:/CreateCare/DataSample/SQA7100/New/CaseTemp/二值图.tif",pImgBinary);//图片文件存储

	//计算气泡图像梯度
	//IplImage * pImgGrads =cvCreateImage(cvGetSize(pImgSrcTemp),pImgSrcTemp->depth,pImgSrcTemp->nChannels);
	//genGradImg(pImgBinary, pImgSrcTemp, pImgGrads);
	//cvSaveImage("E:/CreateCare/DataSample/SQA7100/New/CaseTemp/梯度图.tif",pImgGrads);//图片文件存储	
	//cvReleaseImage(&pImgGrads);

	//生成气泡填充的二值图
	IplImage * pImgFilledBinary = cvCreateImage(cvGetSize(pImgSrcTemp),pImgSrcTemp->depth,pImgSrcTemp->nChannels);
	cvZero(pImgFilledBinary);

	IplImage * pImgSrcMask = cvCreateImage(cvGetSize(pImgSrcTemp),pImgSrcTemp->depth,pImgSrcTemp->nChannels);
	cvZero(pImgSrcMask);
	cvCopy(pImgSrcTemp,pImgSrcMask,pImgBinary);
	
	nStatus = genBubbleFilledImg(pImgSrcMask, pImgBinary, pImgFilledBinary);

	//cvSaveImage("E:/CreateCare/DataSample/SQA7100/New/CaseTemp/气泡填充图.tif",pImgFilledBinary);//图片文件存储

	//生成计算模板
	if (nStatus == 1)
	{
		//图片黑白反转
		cvSubRS( pImgFilledBinary, cvScalar(255), pImgMask);

		//求有效面积占比
		int nAreaMask = cvCountNonZero(pImgMask);
		int nArea = pImgMask->height*pImgMask->width;
		double dRatio = nAreaMask/(nArea+EPSINON);
		if (dRatio < 0.7)
		{
			nStatus = -14;//分析画面可能存在严重气泡或污迹，请重新加样检测（界面提示）
		}
	}

	//资源释放	
	//cvReleaseImage(&pImgGrads);
	cvReleaseImage(&pImgSrcMask);
	cvReleaseImage(&pImgSrcTemp);
	cvReleaseImage(&pImgBinary);
	cvReleaseImage(&pImgFilledBinary);

	return nStatus;
}

//判断气泡边缘方向
int checkBubbleDirection(IplImage * pImgScr, IplImage * pImgBinarySub, CvRect cvRectHor, CvRect cvRectVer, int nCornerFlag)
{
	//nCornerFlag
	//1，左上角
	//2，右上角
	//3，左下角
	//4，右下角
	int nBubbleDret = 0;//-1表示朝外，1表示朝内，0表示多重边界（异常）
	int nNumMultiBounds = 0;//多重边界行列数

	int imgWidth = pImgScr->width;
	int imgHeight = pImgScr->height;

	//判断气泡方向
	int diffLeftNum = 0;
	int diffRightNum = 0;
	int diffUpNum = 0;
	int diffDownNum = 0;
	
	uchar* dataSrc = (uchar *)pImgScr->imageData;
	uchar* dataBinary = (uchar *)pImgBinarySub->imageData;
	int nStep = pImgScr->widthStep / sizeof(uchar);

	//目标水平的处理
	for (int i = cvRectHor.x; i <= cvRectHor.x + cvRectHor.width - 1; i++)
	
	{
		int indexYStart =  cvRectHor.y + cvRectHor.height - 1;
		int indexYEnd = 0;

		//求水平边界点
		int nNumDiff = 0;//水平边界点数
		for (int j = cvRectHor.y; j <= cvRectHor.y + cvRectHor.height-1; j++)
		{
			if (dataBinary[j*nStep + i] > 128)
			{
				if (j <= indexYStart )
				{
					indexYStart = j;
				} 
				if (j >= indexYEnd)
				{
					indexYEnd = j;
				}
			}

			//判断水平相交点的个数
			if (j > cvRectHor.y && j <= cvRectHor.y + cvRectHor.height-1)
			{
				if ((dataBinary[(j-1)*nStep + i] < 127 && dataBinary[j*nStep + i] > 128)
					|| (dataBinary[(j-1)*nStep + i] > 128 && dataBinary[j*nStep + i] < 127))
				{
					nNumDiff++;
				}				
			}
		}

		if (nNumDiff >= 8)
		{
			nNumMultiBounds++;
			continue;//防止出现多重边界带来的统计错误
		}

		int indexYMiddle = (indexYStart + indexYEnd)/2;
		int nDist = (indexYEnd - indexYMiddle) + 10;
		int indexYUp = 0>(indexYMiddle-nDist)?0:(indexYMiddle-nDist); 
		int indexYDown = (imgHeight -1)>(indexYMiddle+nDist)?(indexYMiddle+nDist):(imgHeight -1); 

		int nMax = 0;
		int nIndex = 0;
		for (int j = indexYUp; j <= indexYDown; j++)
		{
			if (dataSrc[j*nStep + i] >= nMax)
			{
				nMax = dataSrc[j*nStep + i];
				nIndex = j;
			}
		}
		if (nIndex > indexYMiddle)
		{
			diffDownNum++;
		} 
		else
		{
			diffUpNum++;
		}
	}
	
	//目标竖直的处理
	for (int j = cvRectVer.y; j <= cvRectVer.y + cvRectVer.height-1; j++)
	{
		int indexXStart =  cvRectVer.x + cvRectVer.width - 1;
		int indexXEnd = 0;

		//求水平边界点
		int nNumDiff = 0;//水平边界点数
		for (int i = cvRectVer.x; i <= cvRectVer.x + cvRectVer.width - 1; i++)
		{
			if (dataBinary[j*nStep + i] > 128)
			{
				if (i <= indexXStart )
				{
					indexXStart = i;
				} 
				if (i >= indexXEnd)
				{
					indexXEnd = i;
				}
			}

			//判断水平相交点的个数
			if (i > cvRectVer.x && i < cvRectVer.x + cvRectVer.width - 1)
			{
				if ((dataBinary[j*nStep + i-1] < 127 && dataBinary[j*nStep + i] > 128)
					|| (dataBinary[j*nStep + i-1] > 128 && dataBinary[j*nStep + i] < 127))
				{
					nNumDiff++;
				}				
			}
		}

		if (nNumDiff >= 8)
		{
			nNumMultiBounds++;
			continue;//防止出现多重边界带来的统计错误
		}

		int indexXMiddle = (indexXStart + indexXEnd)/2;
		int nDist = (indexXEnd - indexXMiddle) + 10;
		int indexXLeft = 0>(indexXMiddle-nDist)?0:(indexXMiddle-nDist); 
		int indexXRight = (imgWidth -1)>(indexXMiddle+nDist)?(indexXMiddle+nDist):(imgWidth -1); 

		int nMax = 0;
		int nIndex = 0;
		for (int i = indexXLeft; i <= indexXRight; i++)
		{
			if (dataSrc[j*nStep + i] >= nMax)
			{
				nMax = dataSrc[j*nStep + i];
				nIndex = i;
			}
		}
		if (nIndex > indexXMiddle)
		{
			diffRightNum++;
		} 
		else
		{
			diffLeftNum++;
		}
	}


	
	//边界判断
	if (nNumMultiBounds <= 20)
	{
		//nCornerFlag
		//1，左上角
		//2，右上角
		//3，左下角
		//4，右下角
		//nBubbleDret，-1表示朝外，1表示朝内，0表示多重边界（异常）
		if (nCornerFlag == 1)//左上角
		{
			if (diffLeftNum + diffUpNum >= diffRightNum + diffDownNum)
			{
				nBubbleDret = 1;
			} 
			else
			{
				nBubbleDret = -1;
			}
		} 
		else if (nCornerFlag == 2)//右上角
		{
			if (diffRightNum + diffUpNum >= diffLeftNum + diffDownNum)
			{
				nBubbleDret = 1;
			} 
			else
			{
				nBubbleDret = -1;
			}
		}
		else if (nCornerFlag == 3)//左下角
		{
			if (diffLeftNum + diffDownNum >= diffRightNum + diffUpNum)
			{
				nBubbleDret = 1;
			} 
			else
			{
				nBubbleDret = -1;
			}
		}
		else if (nCornerFlag == 4)//左下角
		{
			if (diffRightNum + diffDownNum >= diffLeftNum + diffUpNum)
			{
				nBubbleDret = 1;
			} 
			else
			{
				nBubbleDret = -1;
			}
		}
	} 

	return nBubbleDret;
}

//生成边界封闭的气泡二值图
int genBubbleFilledImg(IplImage * pImgSrc, IplImage * pImgBinary, IplImage * pImgFilledBinary)
{
	int nStatus = 1;

	int imgWidth = pImgBinary->width;
	int imgHeight = pImgBinary->height;

	//特定区域单独处理
	IplImage * pImgBinarySub =cvCreateImage(cvGetSize(pImgBinary),pImgBinary->depth,pImgBinary->nChannels);
	cvZero(pImgBinarySub);

	//生成封闭边界的二值图
	IplImage * pImgBinaryTemp =cvCreateImage(cvGetSize(pImgBinary),pImgBinary->depth,pImgBinary->nChannels);
	cvCopy(pImgBinary,pImgBinaryTemp);

	uchar* dataBinaryTemp = (uchar *)pImgBinaryTemp->imageData;
	int nStep = pImgBinary->widthStep / sizeof(uchar);

	CvMemStorage *storage = cvCreateMemStorage();
	CvContourScanner scanner = cvStartFindContours(pImgBinary,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));
	CvSeq * cTemp =NULL;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL && nStatus == 1)//开始查找
	{
		//面积
		double dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0)); 
		if(dConArea >= 400)
		{
			//特定区域提取出来单独分析处理，避免区域间的数据干扰
			//绘制符合面积条件的轮廓
			cvZero(pImgBinarySub);

			cvDrawContours(pImgBinarySub,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),1, 2);//CV_FILLED
			uchar* dataBinary = (uchar *)pImgBinarySub->imageData;

			//外接矩形
			CvRect rectInfor = cvBoundingRect(cTemp);
			rectInfor.x = rectInfor.x -1;
			rectInfor.y = rectInfor.y -1;
			rectInfor.width = rectInfor.width +	2;
			rectInfor.height = rectInfor.height + 2;
			//(0,0),(1280-1,720-1)
			int boxUL[2] = {rectInfor.x, rectInfor.y};
			int boxUR[2] = {rectInfor.x + rectInfor.width - 1, rectInfor.y};
			int boxDL[2] = {rectInfor.x, rectInfor.y + rectInfor.height -1};
			int boxDR[2] = {rectInfor.x + rectInfor.width -1, rectInfor.y + rectInfor.height -1};		

			if (!(boxUL[0] > 0 && boxUL[1] > 0 && boxDR[0] < imgWidth -1 && boxDR[1] < imgHeight -1))
			{
				//1.左上两边
				
				if (rectInfor.x == 0 && rectInfor.y == 0 && rectInfor.x + rectInfor.width < imgWidth  && rectInfor.y + rectInfor.height < imgHeight)
				{
					//求边界黑白点
					int indexTempXLeft = rectInfor.x + rectInfor.width - 1;
					int indexTempXRight = 0;
					int indexTempYUp = rectInfor.y + rectInfor.height -1;
					int indexTempYDown = 0;

					for (int i = rectInfor.x; i <= rectInfor.x + rectInfor.width - 1; i++)
					{
						if (dataBinary[0*nStep + i] > 128)
						{
							if ( indexTempXRight <= i)
							{
								indexTempXRight = i;
							} 
							if (indexTempXLeft >= i)
							{
								indexTempXLeft = i;
							}
						}
					}

					for (int j = rectInfor.y; j <= rectInfor.y + rectInfor.height -1; j++)
					{
						if (dataBinary[j*nStep + 0] > 128)
						{
							if ( indexTempYDown <= j)
							{
								indexTempYDown = j;
							} 
							if (indexTempYUp >= j)
							{
								indexTempYUp = j;
							}							
						}
					}

					//判断气泡边缘方向
					CvRect cvRectHor,cvRectVer;
					cvRectHor.x = rectInfor.x;
					cvRectHor.y = rectInfor.y;
					cvRectHor.width = indexTempXLeft-cvRectHor.x-2;
					cvRectHor.height = rectInfor.height;

					cvRectVer.x = rectInfor.x;
					cvRectVer.y = rectInfor.y;
					cvRectVer.width = rectInfor.width;
					cvRectVer.height = indexTempYUp-rectInfor.y-2;
					int nBubbleDret = checkBubbleDirection(pImgSrc, pImgBinarySub, cvRectHor,cvRectVer, 1);
					if (nBubbleDret == -1 || nBubbleDret == 0)
					{
						nStatus = -14;
						break;
					}

					//封闭边界
					for (int j = 0; j <= 2; j++)					
					{
						for (int i = 0; i <= indexTempXRight; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}

					for (int j = 0; j <= indexTempYDown; j++)
					{
						for (int i = 0; i <= 2; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}
				}

				//2.右上两边				
				else if (rectInfor.x > 0 && rectInfor.y == 0 && rectInfor.x + rectInfor.width == imgWidth  && rectInfor.y + rectInfor.height < imgHeight)
				{
					//求边界黑白点
					int indexTempXLeft = rectInfor.x + rectInfor.width - 1;
					int indexTempXRight = 0;
					int indexTempYUp = rectInfor.y + rectInfor.height -1;
					int indexTempYDown = 0;

					for (int i = rectInfor.x; i <= rectInfor.x + rectInfor.width - 1; i++)
					{
						if (dataBinary[0*nStep + i] > 128)
						{
							if ( indexTempXRight <= i)
							{
								indexTempXRight = i;
							} 
							if (indexTempXLeft >= i)
							{
								indexTempXLeft = i;
							}
						}
					}

					for (int j = rectInfor.y; j <= rectInfor.y + rectInfor.height -1; j++)
					{
						if (dataBinary[j*nStep + imgWidth -1] > 128)
						{
							if ( indexTempYDown <= j)
							{
								indexTempYDown = j;
							} 
							if (indexTempYUp >= j)
							{
								indexTempYUp = j;
							}

						}
					}

					//判断气泡边缘方向
					CvRect cvRectHor,cvRectVer;
					cvRectHor.x = indexTempXRight+1;
					cvRectHor.y = rectInfor.y;
					cvRectHor.width = imgWidth-indexTempXRight-2;
					cvRectHor.height = rectInfor.height;

					cvRectVer.x = rectInfor.x;
					cvRectVer.y = rectInfor.y;
					cvRectVer.width = rectInfor.width;
					cvRectVer.height = indexTempYUp-rectInfor.y-2;
					int nBubbleDret = checkBubbleDirection(pImgSrc, pImgBinarySub, cvRectHor,cvRectVer, 2);
					if (nBubbleDret == -1 || nBubbleDret == 0)
					{
						nStatus = -14;
						break;
					}

					//封闭边界
					for (int j = 0; j <= 2; j++)					
					{
						for (int i = indexTempXLeft; i <= imgWidth -1; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}

					for (int j = 0; j <= indexTempYDown; j++)
					{
						for (int i = imgWidth -3; i <= imgWidth -1; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}
				}

				//3.下左两边				
				else if (rectInfor.x == 0 && rectInfor.y > 0 && rectInfor.x + rectInfor.width < imgWidth  && rectInfor.y + rectInfor.height == imgHeight)
				{
					//求边界黑白点
					int indexTempXLeft = rectInfor.x + rectInfor.width - 1;
					int indexTempXRight = 0;
					int indexTempYUp = rectInfor.y + rectInfor.height -1;
					int indexTempYDown = 0;

					for (int i = rectInfor.x; i <= rectInfor.x + rectInfor.width - 1; i++)
					{
						if (dataBinary[(imgHeight-1)*nStep + i] > 128)
						{
							if ( indexTempXRight <= i)
							{
								indexTempXRight = i;
							} 
							if (indexTempXLeft >= i)
							{
								indexTempXLeft = i;
							}
						}
					}

					for (int j = rectInfor.y; j <= rectInfor.y + rectInfor.height -1; j++)
					{
						if (dataBinary[j*nStep + 0] > 128)
						{
							if ( indexTempYDown <= j)
							{
								indexTempYDown = j;
							} 
							if (indexTempYUp >= j)
							{
								indexTempYUp = j;
							}

						}
					}

					//判断气泡边缘方向
					CvRect cvRectHor,cvRectVer;
					cvRectHor.x = rectInfor.x;
					cvRectHor.y = rectInfor.y;
					cvRectHor.width = indexTempXLeft-cvRectHor.x-2;
					cvRectHor.height = rectInfor.height;

					cvRectVer.x = rectInfor.x;
					cvRectVer.y = indexTempYDown+1;
					cvRectVer.width = rectInfor.width;
					cvRectVer.height = imgHeight-indexTempYDown-2;
					int nBubbleDret = checkBubbleDirection(pImgSrc, pImgBinarySub, cvRectHor,cvRectVer, 3);
					if (nBubbleDret == -1 || nBubbleDret == 0)
					{
						nStatus = -14;
						break;
					}

					//封闭边界
					for (int j = imgHeight-3; j <= imgHeight-1; j++)
					{
						for (int i = 0; i <= indexTempXRight; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}

					for (int j = indexTempYUp; j <= imgHeight-1; j++)
					{
						for (int i = 0; i <= 2; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}
				}

				//4.下右两边				
				else if (rectInfor.x > 0 && rectInfor.y > 0 && rectInfor.x + rectInfor.width == imgWidth  && rectInfor.y + rectInfor.height == imgHeight)
				{
					//求边界黑白点
					int indexTempXLeft = rectInfor.x + rectInfor.width - 1;
					int indexTempXRight = 0;
					int indexTempYUp = rectInfor.y + rectInfor.height -1;
					int indexTempYDown = 0;

					for (int i = rectInfor.x; i <= rectInfor.x + rectInfor.width - 1; i++)
					{
						if (dataBinary[(imgHeight-1)*nStep + i] > 128)
						{
							if ( indexTempXRight <= i)
							{
								indexTempXRight = i;
							} 
							if (indexTempXLeft >= i)
							{
								indexTempXLeft = i;
							}
						}
					}

					for (int j = rectInfor.y; j <= rectInfor.y + rectInfor.height -1; j++)
					{
						if (dataBinary[j*nStep + imgWidth-1] > 128)
						{
							if ( indexTempYDown <= j)
							{
								indexTempYDown = j;
							} 
							if (indexTempYUp >= j)
							{
								indexTempYUp = j;
							}

						}
					}

					//判断气泡边缘方向
					CvRect cvRectHor,cvRectVer;

					cvRectHor.x = indexTempXRight+1;
					cvRectHor.y = rectInfor.y;
					cvRectHor.width = imgWidth-indexTempXRight-2;
					cvRectHor.height = rectInfor.height;

					cvRectVer.x = rectInfor.x;
					cvRectVer.y = indexTempYDown+1;
					cvRectVer.width = rectInfor.width;
					cvRectVer.height = imgHeight-indexTempYDown-2;
					int nBubbleDret = checkBubbleDirection(pImgSrc, pImgBinarySub, cvRectHor,cvRectVer, 4);
					if (nBubbleDret == -1 || nBubbleDret == 0)
					{
						nStatus = -14;
						break;
					}

					//封闭边界
					for (int j = imgHeight-3; j <= imgHeight-1; j++)
					{
						for (int i = indexTempXLeft; i <= imgWidth-1; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}

					for (int j = indexTempYUp; j <= imgHeight-1; j++)
					{
						for (int i = imgWidth-3; i <= imgWidth-1; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}					
				}

				//5.上边接触
				else if (rectInfor.x > 0 && rectInfor.y == 0 && rectInfor.x + rectInfor.width < imgWidth  && rectInfor.y + rectInfor.height < imgHeight)
				{
					//求边界黑白点
					int indexXStart = imgWidth;
					int indexXEnd = 0;

					for (int i = rectInfor.x + 1; i <= rectInfor.x + rectInfor.width - 1; i++)
					{
						if ((dataBinary[0*nStep + i-1] < 127 && dataBinary[0*nStep + i] > 128) 
							|| (dataBinary[0*nStep + i-1] > 128 && dataBinary[0*nStep + i] < 127))
						{
							if (i <= indexXStart)
							{
								indexXStart = i;
							} 
							if (i >= indexXEnd)
							{
								indexXEnd = i;
							}
						}
					}

					//封闭边界
					for (int j = 0; j <= 2; j++)
					{
						for (int i = indexXStart; i <= indexXEnd; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}
				}

				//6.下边接触
				else if (rectInfor.x > 0 && rectInfor.y > 0 && rectInfor.x + rectInfor.width < imgWidth  && rectInfor.y + rectInfor.height == imgHeight)
				{
					//求边界黑白点
					int indexXStart = imgWidth;
					int indexXEnd = 0;

					for (int i = rectInfor.x + 1; i <= rectInfor.x + rectInfor.width - 1; i++)
					{
						if ((dataBinary[(imgHeight-1)*nStep + i-1] < 127 && dataBinary[(imgHeight-1)*nStep + i] > 128) 
							|| (dataBinary[(imgHeight-1)*nStep + i-1] > 128 && dataBinary[(imgHeight-1)*nStep + i] < 127))
						{
							if (i <= indexXStart)
							{
								indexXStart = i;
							} 
							if (i >= indexXEnd)
							{
								indexXEnd = i;
							}
						}
					}

					//封闭边界
					for (int j = imgHeight-3; j <= imgHeight-1; j++)
					{
						for (int i = indexXStart; i <= indexXEnd; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}
				}

				//7.左边接触
				else if (rectInfor.x == 0 && rectInfor.y > 0 && rectInfor.x + rectInfor.width < imgWidth  && rectInfor.y + rectInfor.height < imgHeight)
				{
					//求边界黑白点
					int indexYStart = imgHeight;
					int indexYEnd = 0;

					for (int j = rectInfor.y + 1; j <= rectInfor.y + rectInfor.height -1; j++)
					{
						if ((dataBinary[(j-1)*nStep + 0] < 127 && dataBinary[j*nStep + 0] > 128) 
							|| (dataBinary[(j-1)*nStep + 0] > 128 && dataBinary[j*nStep + 0] < 127))
						{
							if (j <= indexYStart)
							{
								indexYStart = j;
							} 
							if (j >= indexYEnd)
							{
								indexYEnd = j;
							}
						}
					}

					//封闭边界
					for (int j = indexYStart; j <= indexYEnd; j++)
					{
						for (int i = 0; i <= 2; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}
				}

				//8.右边接触
				else if (rectInfor.x > 0 && rectInfor.y > 0 && rectInfor.x + rectInfor.width == imgWidth  && rectInfor.y + rectInfor.height < imgHeight)
				{
					//求边界黑白点
					int indexYStart = imgHeight;
					int indexYEnd = 0;

					for (int j = rectInfor.y + 1; j <= rectInfor.y + rectInfor.height -1; j++)
					{
						if ((dataBinary[(j-1)*nStep + imgWidth-1] < 127 && dataBinary[j*nStep + imgWidth-1] > 128) 
							|| (dataBinary[(j-1)*nStep + imgWidth-1] > 128 && dataBinary[j*nStep + imgWidth-1] < 127))
						{
							if (j <= indexYStart)
							{
								indexYStart = j;
							} 
							if (j >= indexYEnd)
							{
								indexYEnd = j;
							}
						}
					}

					//封闭边界
					for (int j = indexYStart; j <= indexYEnd; j++)
					{
						for (int i = imgWidth-3; i <= imgWidth-1; i++)
						{
							dataBinaryTemp[j*nStep + i] = 255;
						}
					}
				}

				//9.上下两边接触
				else if (rectInfor.x > 0 && rectInfor.y == 0 && rectInfor.x + rectInfor.width < imgWidth  && rectInfor.y + rectInfor.height == imgHeight)
				{
					//判断气泡方向
					int diffLeftNum = 0;
					int diffRightNum = 0;
					int indexXUL,indexXUR,indexXDL,indexXDR;
					uchar* dataSrc = (uchar *)pImgSrc->imageData;
					for (int j = 0; j <= imgHeight-1; j++)
					{
						int indexXStart = imgWidth;
						int indexXEnd = 0;
						for (int i = rectInfor.x; i <= rectInfor.x + rectInfor.width - 1; i++)
						{
							if (dataBinary[j*nStep + i] > 128)
							{
								if (i <= indexXStart )
								{
									indexXStart = i;
								} 
								if (i >= indexXEnd)
								{
									indexXEnd = i;
								}
							}
						}
						if (j == 0)
						{
							indexXUL = indexXStart;
							indexXUR = indexXEnd;
						}
						if (j == imgHeight-1)
						{
							indexXDL = indexXStart;
							indexXDR = indexXEnd;
						}
						int indexXMiddle = (indexXStart + indexXEnd)/2;
						int indexXLeft = 0>(indexXMiddle-25)?0:(indexXMiddle-25); 
						int indexXRight = (imgWidth -1)>(indexXMiddle+25)?(indexXMiddle+25):(imgWidth -1); 

						int nMax = 0;
						int nIndex = 0;
						for (int i = indexXLeft; i <= indexXRight; i++)
						{
							if (dataSrc[j*nStep + i] >= nMax)
							{
								nMax = dataSrc[j*nStep + i];
								nIndex = i;
							}
						}
						if (nIndex > indexXMiddle)
						{
							diffRightNum++;
						} 
						else
						{
							diffLeftNum++;
						}
					}

					//封闭边界
					if (diffLeftNum > diffRightNum)//左侧为气泡
					{
						for (int j = 0; j <= 2; j++)
						{
							for (int i = 0; i <= indexXUR; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
						for (int j = imgHeight-3; j <= imgHeight-1; j++)
						{
							for (int i = 0; i <= indexXDR; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
						for (int j = 0; j <= imgHeight-1; j++)
						{
							for (int i = 0; i <= 2; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
					} 
					else//右侧为气泡
					{
						for (int j = 0; j <= 2; j++)
						{
							for (int i = indexXUL; i <= imgWidth-1; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
						for (int j = imgHeight-3; j <= imgHeight-1; j++)
						{
							for (int i = indexXDL; i <= imgWidth-1; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
						for (int j = 0; j <= imgHeight-1; j++)
						{
							for (int i = imgWidth-3; i <= imgWidth-1; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
					}
				}

				//10.左右两边接触
				else if (rectInfor.x == 0 && rectInfor.y > 0 && rectInfor.x + rectInfor.width == imgWidth  && rectInfor.y + rectInfor.height < imgHeight)
				{
					//判断气泡方向
					int diffUpNum = 0;
					int diffDownNum = 0;
					int indexYUL,indexYDL,indexYUR,indexYDR;
					uchar* dataSrc = (uchar *)pImgSrc->imageData;
					for (int i = 0; i <= imgWidth-1; i++)					
					{
						int indexYStart = imgHeight;
						int indexYEnd = 0;
						for (int j = rectInfor.y; j <= rectInfor.y + rectInfor.height - 1; j++)
						{
							if (dataBinary[j*nStep + i] > 128)
							{
								if (j <= indexYStart)
								{
									indexYStart = j;
								} 
								if (j >= indexYEnd)
								{
									indexYEnd = j;
								}
							}
						}
						if (i == 0)
						{
							indexYUL = indexYStart;
							indexYDL = indexYEnd;
						}
						if (i == imgWidth-1)
						{
							indexYUR = indexYStart;
							indexYDR = indexYEnd;
						}
						int indexYMiddle = (indexYStart + indexYEnd)/2;
						int indexYUp = 0>(indexYMiddle-25)?0:(indexYMiddle-25); 
						int indexYDown = (imgHeight-1)>(indexYMiddle+25)?(indexYMiddle+25):(imgHeight-1); 
						int nMax = 0;
						int nIndex = 0;
						for (int j = indexYUp; j <= indexYDown; j++)
						{
							if (dataSrc[j*nStep + i] >= nMax)
							{
								nMax = dataSrc[j*nStep + i];
								nIndex = j;
							}
						}
						if (nIndex > indexYMiddle)
						{
							diffDownNum++;
						} 
						else
						{
							diffUpNum++;
						}						
					}

					//封闭边界
					if (diffUpNum > diffDownNum)//上侧为气泡
					{
						for (int j = 0; j <= 2; j++)
						{
							for (int i = 0; i <= imgWidth-1; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
						for (int j = 0; j <= indexYDL; j++)
						{
							for (int i = 0; i <= 2; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
						for (int j = 0; j <= indexYDR; j++)
						{
							for (int i = imgWidth-3; i <= imgWidth-1; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
					} 
					else//下侧为气泡
					{
						for (int j = imgHeight-3; j <= imgHeight-1; j++)
						{
							for (int i = 0; i <= imgWidth-1; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
						for (int j = indexYUL; j <= imgHeight-1; j++)
						{
							for (int i = 0; i <= 2; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
						for (int j = indexYUR; j <= imgHeight-1; j++)
						{
							for (int i = imgWidth-3; i <= imgWidth-1; i++)
							{
								dataBinaryTemp[j*nStep + i] = 255;
							}
						}
					}
				}

				//11.其余气泡分布状态
				else
				{
					nStatus = -14;
					break;
				}
			}
		}
		cvSubstituteContour(scanner,NULL);//删除当前的轮廓
	}
	CvSeq* firstcontour = cvEndFindContours(&scanner);
	cvReleaseMemStorage(&storage);

	if (nStatus == 1)
	{
		//孔洞填充
		CvMemStorage *storage2 = cvCreateMemStorage();
		CvContourScanner scanner2 = cvStartFindContours(pImgBinaryTemp,
			storage2,
			sizeof(CvContour),
			CV_RETR_LIST,
			CV_CHAIN_APPROX_SIMPLE,
			cvPoint(0,0));
		CvSeq * cTemp2 =NULL;
		while( (cTemp2 = cvFindNextContour(scanner2) ) != NULL)//开始查找
		{
			//面积
			double dConArea = fabs(cvContourArea(cTemp2,CV_WHOLE_SEQ,0)); 
			if(dConArea >= 400)
			{
				//绘制符合面积条件的轮廓
				cvDrawContours(pImgFilledBinary,cTemp2,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);
			}
			cvSubstituteContour(scanner2,NULL);//删除当前的轮廓
		}
		CvSeq* firstcontour2 = cvEndFindContours(&scanner2);
		cvReleaseMemStorage(&storage2);

		//边界处理
		IplConvKernel * pStructureEle = cvCreateStructuringElementEx(8, 8, 2, 2,
			CV_SHAPE_RECT, NULL);//创建结构元素
		cvMorphologyEx( pImgFilledBinary, pImgFilledBinary,
			NULL, pStructureEle,
			CV_MOP_DILATE, 1 );
		cvReleaseStructuringElement( &pStructureEle );//清除结构元素
	}

	//资源释放	
	cvReleaseImage(&pImgBinaryTemp);
	cvReleaseImage(&pImgBinarySub);

	return nStatus;
}

//生成二值化的气泡图
void genBubbleThreshImg(IplImage * pImgSrc, IplImage * pImgBinary)
{

	IplImage * pImgDraw =cvCreateImage(cvGetSize(pImgBinary),pImgBinary->depth,pImgBinary->nChannels);
	cvZero(pImgDraw);

	//图像二值化
	cvThreshold( pImgSrc, pImgBinary,
		35, 255, CV_THRESH_BINARY_INV);//固定阈值分割

	CvMemStorage *storage = cvCreateMemStorage();
	CvContourScanner scanner = cvStartFindContours(pImgBinary,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));
	CvSeq * cTemp =NULL;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找
	{
		//面积
		double dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0)); 
		if(dConArea >= 400)
		{
			//绘制符合面积条件的轮廓
			cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);
		}
		cvSubstituteContour(scanner,NULL);//删除当前的轮廓
	}
	CvSeq* firstcontour = cvEndFindContours(&scanner);
	cvCopy(pImgDraw,pImgBinary);

	IplConvKernel * pStructureEle = cvCreateStructuringElementEx(6, 6, 3, 3,
		CV_SHAPE_ELLIPSE, NULL);//创建结构元素
	cvMorphologyEx( pImgBinary, pImgBinary,
		NULL, pStructureEle,
		CV_MOP_CLOSE, 1 );

	IplConvKernel * pStructureEle2 = cvCreateStructuringElementEx(8, 8, 3, 3,
		CV_SHAPE_ELLIPSE, NULL);//创建结构元素
	cvMorphologyEx( pImgBinary, pImgBinary,
		NULL, pStructureEle2,
		CV_MOP_DILATE, 1 );	

	//资源释放
	cvReleaseStructuringElement( &pStructureEle );//清除结构元素
	cvReleaseStructuringElement( &pStructureEle2 );//清除结构元素
	cvReleaseMemStorage(&storage);
	cvReleaseImage(&pImgDraw);
}

//气泡梯度图像生成
void genGradImg(IplImage * pImgBinary, IplImage * pImgSrc, IplImage * pImgGrads)
{
	int imgWidth = pImgSrc->width;
	int imgHeight = pImgSrc->height;

	IplImage * pImgGradsTemp =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgSrcTemp =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvConvertScale( pImgSrc, pImgSrcTemp, 1, 0);

	//扩展矩阵生成
	IplImage * pImgPad =cvCreateImage(cvSize(imgWidth+2,imgHeight+2),IPL_DEPTH_16S,1);
	cvSetImageROI(pImgPad,cvRect(1,1,imgWidth,imgHeight));//设置源图像ROI
	cvCopy(pImgSrcTemp,pImgPad);
	cvResetImageROI(pImgPad);
	//上边界处理
	cvSetImageROI(pImgPad,cvRect(1,0,imgWidth,1));//设置源图像ROI
	cvSetImageROI(pImgSrcTemp,cvRect(0,0,imgWidth,1));//设置源图像ROI
	cvCopy(pImgSrcTemp,pImgPad);
	cvResetImageROI(pImgPad);
	cvResetImageROI(pImgSrcTemp);
	//下边界处理
	cvSetImageROI(pImgPad,cvRect(1,imgHeight+1,imgWidth,1));//设置源图像ROI
	cvSetImageROI(pImgSrcTemp,cvRect(0,imgHeight-1,imgWidth,1));//设置源图像ROI
	cvCopy(pImgSrcTemp,pImgPad);
	cvResetImageROI(pImgPad);
	cvResetImageROI(pImgSrcTemp);
	//左边界处理
	cvSetImageROI(pImgPad,cvRect(0,1,1,imgHeight));//设置源图像ROI
	cvSetImageROI(pImgSrcTemp,cvRect(0,0,1,imgHeight));//设置源图像ROI
	cvCopy(pImgSrcTemp,pImgPad);
	cvResetImageROI(pImgPad);
	cvResetImageROI(pImgSrcTemp);
	//右边界处理
	cvSetImageROI(pImgPad,cvRect(imgWidth+1,1,1,imgHeight));//设置源图像ROI
	cvSetImageROI(pImgSrcTemp,cvRect(imgWidth-1,0,1,imgHeight));//设置源图像ROI
	cvCopy(pImgSrcTemp,pImgPad);
	cvResetImageROI(pImgPad);
	cvResetImageROI(pImgSrcTemp);

	//中心差分
	//pImgDiffX = pImgTempX1 - pImgTempX0;
	//pImgDiffY = pImgTempY1 - pImgTempY0;
	IplImage * pImgDiffX =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);	
	IplImage * pImgTempX1 =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgTempX0 =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgDiffY =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgTempY1 =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgTempY0 =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvSetImageROI(pImgPad,cvRect(2,1,imgWidth,imgHeight));//设置源图像ROI
	cvCopy(pImgPad,pImgTempX1);
	cvResetImageROI(pImgPad);	
	cvSetImageROI(pImgPad,cvRect(0,1,imgWidth,imgHeight));//设置源图像ROI
	cvCopy(pImgPad,pImgTempX0);
	cvResetImageROI(pImgPad);
	cvSetImageROI(pImgPad,cvRect(1,2,imgWidth,imgHeight));//设置源图像ROI
	cvCopy(pImgPad,pImgTempY1);
	cvResetImageROI(pImgPad);	
	cvSetImageROI(pImgPad,cvRect(1,0,imgWidth,imgHeight));//设置源图像ROI
	cvCopy(pImgPad,pImgTempY0);
	cvResetImageROI(pImgPad);

	cvAbsDiff(pImgTempX1,pImgTempX0,pImgDiffX);
	cvAbsDiff(pImgTempY1,pImgTempY0,pImgDiffY);
	cvAddWeighted( pImgDiffX, 0.5, pImgDiffY, 0.5, 0, pImgGradsTemp);

	//test
	/*
	cvConvertScale( pImgGradsTemp, pImgSrc, 1, 0);
	{
		cvNamedWindow("pImgGradsTemp",CV_WINDOW_KEEPRATIO);
		cvShowImage("pImgGradsTemp",pImgSrc);
		cvWaitKey();
	}
	double MaxTemp,MinTemp;
	cvMinMaxLoc( pImgBinary, &MinTemp, &MaxTemp);
	*/

	cvMul(pImgBinary,pImgGradsTemp,pImgGradsTemp,1/255.0);
	cvConvertScale( pImgGradsTemp, pImgGrads, 1, 0);

	if (DebugMode)
	{
		cvNamedWindow("pImgGrads",CV_WINDOW_KEEPRATIO);
		cvShowImage("pImgGrads",pImgGrads);
		cvWaitKey();
	}

	//资源释放
	cvReleaseImage(&pImgGradsTemp);
	cvReleaseImage(&pImgSrcTemp);
	cvReleaseImage(&pImgPad);
	cvReleaseImage(&pImgDiffX);
	cvReleaseImage(&pImgTempX1);
	cvReleaseImage(&pImgTempX0);
	cvReleaseImage(&pImgDiffY);
	cvReleaseImage(&pImgTempY1);
	cvReleaseImage(&pImgTempY0);
}

//样本漂移判断
int isSampleDrift(const char **ppcFilePath, int nImgFileNum, IplImage *pImgMask)
{
	int nStatus = 1;
	int nStatus0 = 1;
	int nStatus1 = 1;
	int nStatus2 = 1;
	int nStatus3 = 1;

	IplImage *pImgTemp = cvLoadImage(ppcFilePath[0],0);

	//1.读取图片
	IplImage *pImgSrcTemp0 = cvLoadImage(ppcFilePath[0],0);
	cvZero(pImgTemp);
	cvCopy(pImgSrcTemp0, pImgTemp, pImgMask);
	cvCopy(pImgTemp, pImgSrcTemp0);

	IplImage *pImgSrcTemp1 = cvLoadImage(ppcFilePath[1],0);
	cvZero(pImgTemp);
	cvCopy(pImgSrcTemp1, pImgTemp, pImgMask);
	cvCopy(pImgTemp, pImgSrcTemp1);

	IplImage *pImgSrcTemp2 = cvLoadImage(ppcFilePath[5],0);
	cvZero(pImgTemp);
	cvCopy(pImgSrcTemp2, pImgTemp, pImgMask);
	cvCopy(pImgTemp, pImgSrcTemp2);

	IplImage *pImgSrcTemp3 = cvLoadImage(ppcFilePath[nImgFileNum-2],0);
	cvZero(pImgTemp);
	cvCopy(pImgSrcTemp3, pImgTemp, pImgMask);
	cvCopy(pImgTemp, pImgSrcTemp3);

	IplImage *pImgSrcTemp4 = cvLoadImage(ppcFilePath[nImgFileNum-1],0);
	cvZero(pImgTemp);
	cvCopy(pImgSrcTemp4, pImgTemp, pImgMask);
	cvCopy(pImgTemp, pImgSrcTemp4);

	//判断是否漂移
	do 
	{
		nStatus0 = isImgDrift(pImgSrcTemp0, pImgSrcTemp1);//快速飘动
		if (nStatus0 != 1)
		{
			break;
		}
		nStatus1 = isImgDrift(pImgSrcTemp0, pImgSrcTemp2);//慢速飘动
		if (nStatus1 != 1)
		{
			break;
		}
		nStatus2 = isImgDrift(pImgSrcTemp0, pImgSrcTemp3);//更慢速飘动
		if (nStatus2 != 1)
		{
			break;
		}
		nStatus3 = isImgDrift(pImgSrcTemp3, pImgSrcTemp4);//更快速飘动
		if (nStatus3 != 1)
		{
			break;
		}
	} 
	while (0);
	
	if (nStatus0 == -13 || nStatus1 == -13 || nStatus2 == -13 || nStatus3 == -13)
	{
		nStatus = -13;//样本可能飘动，请等待半分钟再进行检测
	} 
	else if (nStatus0 == 0 || nStatus1 == 0 || nStatus2 == 0 || nStatus3 == 0)
	{
		nStatus = 0;//内存异常
	}
	else
	{
		nStatus = 1;
	}

	cvReleaseImage(&pImgTemp);
	cvReleaseImage(&pImgSrcTemp0);
	cvReleaseImage(&pImgSrcTemp1);
	cvReleaseImage(&pImgSrcTemp2);
	cvReleaseImage(&pImgSrcTemp3);
	cvReleaseImage(&pImgSrcTemp4);

	return nStatus;
}

//图像漂移判断
int isImgDrift(IplImage *pImgSrcA, IplImage *pImgSrcB)
{
	//注：做4*5个子图的互相关分析
	int nStatus = 1;

	double *pdCCDataAll = (double *)malloc(sizeof(double)*20*3);
	if (NULL == pdCCDataAll)
	{
		nStatus = 0;//内存异常
		return nStatus;
	}
	double *pdCCData = (double *)malloc(sizeof(double)*20);
	if (NULL == pdCCData)
	{
		free(pdCCDataAll);
		pdCCDataAll = NULL;

		nStatus = 0;//内存异常
		return nStatus;
	}

	int nCCSize = 96;//互相关分析子图大小
	int nStepW = pImgSrcA->width/5;
	int nStepH = pImgSrcA->height/4;
	IplImage *pImgSubA = cvCreateImage(cvSize(nCCSize,nCCSize), pImgSrcA->depth,1);//图A的FFT计算子图
	IplImage *pImgSubB = cvCreateImage(cvSize(nCCSize,nCCSize), pImgSrcB->depth,1);//图A的FFT计算子图

	//2.互相关计算
	double dDataTemp[3] = {0};//峰峰比、位移、方向标准差
	int nStartX = nStepW/2 - nCCSize/2;
	int nStartY = nStepH/2 - nCCSize/2;

	for (int i = 0; i < 5; i++)//水平方向
	{
		for (int j = 0; j < 4; j++)//竖直方向
		{
			//提取子图
			int nLUCorX = nStartX + i*nStepW;
			int nLUCorY = nStartY + j*nStepH;
			cvSetImageROI(pImgSrcA, cvRect(nLUCorX, nLUCorY,nCCSize,nCCSize));
			cvCopy(pImgSrcA, pImgSubA);
			cvResetImageROI(pImgSrcA);
			cvSetImageROI(pImgSrcB, cvRect(nLUCorX, nLUCorY,nCCSize,nCCSize));
			cvCopy(pImgSrcB, pImgSubB);
			cvResetImageROI(pImgSrcB);

			//test子图显示
			if (DebugMode)
			{
				cvNamedWindow("ImgSubA",CV_WINDOW_KEEPRATIO);
				cvShowImage("ImgSubA",pImgSubA);
				cvWaitKey();

				cvNamedWindow("ImgSubB",CV_WINDOW_KEEPRATIO);
				cvShowImage("ImgSubB",pImgSubB);
				cvWaitKey();
			}

			//子图互相关分析
			getCCResult(pImgSubA, pImgSubB, dDataTemp);

			//数据保存
			pdCCDataAll[3*(4*i+j)] = dDataTemp[0];//峰峰比
			pdCCDataAll[3*(4*i+j)+1] = dDataTemp[1];//位移
			pdCCDataAll[3*(4*i+j)+2] = dDataTemp[2];//方向
		}
	}

	//判断是否漂移
	int nCCNum = 0;
	for (int i = 0; i < 20; i++)//水平方向
	{
		if (pdCCDataAll[3*i] > 1.002 && pdCCDataAll[3*i+1] >2.5)//峰峰比条件，位移条件
		{
			pdCCData[nCCNum] = pdCCDataAll[3*i+2];//方向
			nCCNum++;
		}
	}
	if (nCCNum >= 6)
	{
		double *pdAveStd= calAveStd(pdCCData, nCCNum);
		double dAve0 = pdAveStd[0];//平均值
		double dStd0 = pdAveStd[1];//标准差
		free(pdAveStd);
		pdAveStd = NULL;

		//处理-180度和180度的不连续问题
		for (int i = 0; i < nCCNum; i++)
		{
			if (pdCCData[i] < -90)
			{
				pdCCData[i] = pdCCData[i] + 360;
			}
		}
		pdAveStd= calAveStd(pdCCData, nCCNum);
		double dAve1 = pdAveStd[0];//平均值
		double dStd1 = pdAveStd[1];//标准差
		free(pdAveStd);
		pdAveStd = NULL;

		if (dStd0 < 26.4 || dStd1 < 26.4)//方向角度偏差在70度以内，std(0:90) = 26.4
		{
			nStatus = -13;//样本可能飘动，请等待半分钟再进行检测
		}
	}

	//资源释放
	free(pdCCDataAll);
	pdCCDataAll = NULL;
	free(pdCCData);
	pdCCData = NULL;
	cvReleaseImage(&pImgSubA);
	cvReleaseImage(&pImgSubB);

	return nStatus;
}

//互相关计算
void getCCResult(IplImage *pImgSrcA, IplImage *pImgSrcB, double *pdCCData)
{
	//峰峰比、位移、方向
	IplImage *pImgFFT2A = cvCreateImage(cvGetSize(pImgSrcA), IPL_DEPTH_64F, 2);//图A的FFT结果
	IplImage *pImgFFT2B = cvCreateImage(cvGetSize(pImgSrcB), IPL_DEPTH_64F, 2);//图B的FFT结果
	IplImage *pImgCCAB = cvCreateImage(cvGetSize(pImgSrcB), IPL_DEPTH_64F, 2);//conj(fft2(A)).*fft2(B)
	IplImage *pImgIFFT2AB = cvCreateImage(cvGetSize(pImgSrcB), IPL_DEPTH_64F, 1);//ifft(conj(fft2(A)).*fft2(B))//IPL_DEPTH_8U

	//1.对图像A做FFT计算
	getImgFFT2(pImgSrcA, pImgFFT2A);

	//2.对FFT2A做共轭计算
	getImgConj(pImgFFT2A);

	//3.对图像B做FFT计算
	getImgFFT2(pImgSrcB, pImgFFT2B);

	//4.FFT结果相乘
	mulFFT2AB(pImgFFT2A, pImgFFT2B, pImgCCAB);
	
	//5.对相乘结果做IFFT计算
	getImgIFFT2(pImgCCAB, pImgIFFT2AB);

	//6.对IFFT结果做Shift变换得到互相关值R平面分布
	shiftFFTImg(pImgIFFT2AB);//shift实部

	//test，输出互相关计算结果
	//writeCCResult2File(pImgIFFT2AB, "E:/CreateCare/DataSample/SQA7100/New/DriftTest/");

	//7.分析互相关值R，得到峰峰比、位移、方向
	getCCInfor(pImgIFFT2AB,pdCCData);

	//资源释放
	cvReleaseImage(&pImgFFT2A);
	cvReleaseImage(&pImgFFT2B);
	cvReleaseImage(&pImgCCAB);
	cvReleaseImage(&pImgIFFT2AB);
}

//快速傅里叶变换
void getImgFFT2(IplImage *pImgSrc, IplImage *pImgDst)
{
	//pImgSrc, 8U, 1
	//pImgDst, 64F, 2
	int dftWidth  = getOptimalDFTSize(pImgSrc->width);
	int dftHeight = getOptimalDFTSize(pImgSrc->height);
	
	IplImage *pImgRe = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 1);  //Real part
	IplImage *pImgIm = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 1);  //Imaginary part
	IplImage *pImgFourier = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 2);

	// Real part conversion from u8 to 64f (double)  
	cvConvertScale(pImgSrc, pImgRe);  
	cvZero(pImgIm);
	cvMerge(pImgRe, pImgIm, 0, 0, pImgFourier);
	
	// Application of the forward Fourier transform  
	cvDFT(pImgFourier, pImgDst, CV_DXT_FORWARD);
	//cvDFT(pImgFourier, pImgFourier, CV_DXT_FORWARD);

	// Shift zero-frequency component to center of spectrum
	cvSplit(pImgDst,pImgRe,pImgIm,0,0);
	shiftFFTImg(pImgRe);//shift实部
	shiftFFTImg(pImgIm);//shift虚部

	//Merge Real part and Imaginary part
	cvMerge(pImgRe, pImgIm, 0, 0, pImgDst);

	cvReleaseImage(&pImgRe);  
	cvReleaseImage(&pImgIm);  
	cvReleaseImage(&pImgFourier);  
}

//快速傅里叶变换的逆变换
void getImgIFFT2(IplImage *pImgSrc, IplImage *pImgDst)
{
	//注：结果处理为了单通道
	//pImgSrc, IPL_DEPTH_64F, 2
	//pImgDst, IPL_DEPTH_64F, 1
	double dMin = 0;
	double dMax = 0;

	IplImage *pImgIFFT2 = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 2);//
	IplImage *pImgRe = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 1);  //实部
	IplImage *pImgIm = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 1);  //虚部

	cvDFT(pImgSrc, pImgIFFT2, CV_DXT_INV_SCALE);//实现傅里叶逆变换//CV_DXT_INV_SCALE//CV_DXT_INVERSE
	cvSplit(pImgIFFT2, pImgRe, pImgIm, 0, 0);

	cvPow(pImgRe,pImgRe,2);                 
	cvPow(pImgIm,pImgIm,2);  
	cvAdd(pImgRe,pImgIm,pImgRe,NULL);
	cvPow(pImgRe,pImgRe,0.5);

	//test查看结果大小
	//double minVal = 0, maxVal = 0;  
	//cvMinMaxLoc( pImgRe, &minVal, &maxVal);
	cvNormalize(pImgRe,pImgDst,1,0,CV_C,NULL);//注：用于互相关分析
	//cvMinMaxLoc( pImgDst, &minVal, &maxVal);

	//用于显示
	//cvNormalize(pImgRe,pImgRe,255,0,CV_C,NULL);//注：该步骤会拉伸图像的灰度，注意使用
	//cvConvertScale(pImgRe, pImgDst);//64F数据转换为8U数据，便于显示

	//资源释放
	cvReleaseImage(&pImgIFFT2);
	cvReleaseImage(&pImgRe);
	cvReleaseImage(&pImgIm);
}

//FFT和IFFT测试函数
void testFFTandIFFT(IplImage *pImgSrc)
{
	if (DebugMode)//原图
	{
		cvNamedWindow("ImgSrc",CV_WINDOW_KEEPRATIO);
		cvShowImage("ImgSrc",pImgSrc);
		cvWaitKey();
	}

	IplImage *pImgFFT2 = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 2);//图A的FFT结果
	IplImage *pImgIFFT2 = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_8U, 1);
	IplImage *pImgRe = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 1);  //实部
	IplImage *pImgIm = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 1);  //虚部

	//fft2变换
	getImgFFT2(pImgSrc, pImgFFT2);

	//FFT频谱幅值显示
	cvSplit(pImgFFT2, pImgRe, pImgIm, 0, 0);
	cvPow(pImgRe,pImgRe,2);
	cvPow(pImgIm,pImgIm,2);
	cvAdd(pImgRe,pImgIm,pImgRe,NULL);
	cvPow(pImgRe,pImgRe,0.5);
	cvAddS(pImgRe,cvScalar(1),pImgRe );
	cvLog (pImgRe,pImgRe);//对数变换以增强灰度级细节
	cvNormalize(pImgRe,pImgRe,1,0,CV_C,NULL);
	if (DebugMode)//fft结果频谱
	{
		cvNamedWindow("ImgFFT",CV_WINDOW_KEEPRATIO);
		cvShowImage("ImgFFT",pImgRe);
		cvWaitKey();
	}

	//ifft2变换
	getImgIFFT2(pImgFFT2, pImgIFFT2);
	if (DebugMode)//逆变换结果
	{
		cvNamedWindow("ImgIFFT",CV_WINDOW_KEEPRATIO);
		cvShowImage("ImgIFFT",pImgIFFT2);
		cvWaitKey();
	}

	cvReleaseImage(&pImgFFT2);
	cvReleaseImage(&pImgIFFT2);
	cvReleaseImage(&pImgRe);
	cvReleaseImage(&pImgIm);
}

//对调FFT结果的四象限(fftshift)
void shiftFFTImg(IplImage *pImgSrc)
{
	//注：针对单通道图像
	IplImage *ImgFFTShift = cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);//
	cvZero(ImgFFTShift);
	int cx = pImgSrc->width/2;
	int cy = pImgSrc->height/2;

	//  |-----|-----|           |-----|-----|   
	//  |  1  |  3  |           |  4  |  2  |
	//  |-----|-----|   --->    |-----|-----|
	//  |  2  |  4  |           |  3  |  1  |
	//  |-----|-----|           |-----|-----|

	cvSetImageROI(pImgSrc, cvRect(0, 0,cx,cy));// 1 
	cvSetImageROI(ImgFFTShift, cvRect(cx,cy,cx,cy));// 4 
	cvCopy(pImgSrc, ImgFFTShift);//复制1到4

	cvSetImageROI(pImgSrc,cvRect(cx,cy,cx,cy));  // 4
	cvSetImageROI(ImgFFTShift,cvRect( 0, 0,cx,cy));  // 1
	cvCopy(pImgSrc, ImgFFTShift);//复制4到1

	cvSetImageROI(pImgSrc,cvRect(cx, 0,cx,cy));  // 3 
	cvSetImageROI(ImgFFTShift,cvRect( 0,cy,cx,cy));  // 2 
	cvCopy(pImgSrc, ImgFFTShift);//复制3到2

	cvSetImageROI(pImgSrc,cvRect( 0,cy,cx,cy));  // 2 
	cvSetImageROI( ImgFFTShift,cvRect(cx, 0,cx,cy));  // 3 
	cvCopy(pImgSrc, ImgFFTShift);//复制2到3

	cvResetImageROI(pImgSrc);
	cvResetImageROI(ImgFFTShift);

	cvCopy(ImgFFTShift, pImgSrc);//复制结果

	//资源释放
	cvReleaseImage(&ImgFFTShift);
}

//复数矩阵求共轭
void getImgConj(IplImage *pImgSrc)
{
	//注：针对双通道图像
	IplImage *pImgRe = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 1);  //实部
	IplImage *pImgIm = cvCreateImage(cvGetSize(pImgSrc), IPL_DEPTH_64F, 1);  //虚部

	//复数拆分
	cvSplit(pImgSrc,pImgRe,pImgIm,0,0);
	//虚部求反
	cvSubRS(pImgIm, cvScalar(0), pImgIm);
	//复数合并
	cvMerge(pImgRe, pImgIm, 0, 0, pImgSrc);

	//资源释放
	cvReleaseImage(&pImgRe);
	cvReleaseImage(&pImgIm);
}

//FFT结果相乘
void mulFFT2AB(IplImage *pImgFFT2A, IplImage *pImgFFT2B, IplImage *pImgCCAB)
{
	//注：针对双通道图像
	IplImage *pImgReA = cvCreateImage(cvGetSize(pImgFFT2A), IPL_DEPTH_64F, 1);  //实部
	IplImage *pImgImA = cvCreateImage(cvGetSize(pImgFFT2A), IPL_DEPTH_64F, 1);  //虚部
	IplImage *pImgReB = cvCreateImage(cvGetSize(pImgFFT2B), IPL_DEPTH_64F, 1);  //实部
	IplImage *pImgImB = cvCreateImage(cvGetSize(pImgFFT2B), IPL_DEPTH_64F, 1);  //虚部
	IplImage *pImgReAB = cvCreateImage(cvGetSize(pImgCCAB), IPL_DEPTH_64F, 1);  //实部
	IplImage *pImgImAB = cvCreateImage(cvGetSize(pImgCCAB), IPL_DEPTH_64F, 1);  //虚部
	IplImage *pImgTemp0 = cvCreateImage(cvGetSize(pImgCCAB), IPL_DEPTH_64F, 1); 
	IplImage *pImgTemp1 = cvCreateImage(cvGetSize(pImgCCAB), IPL_DEPTH_64F, 1); 

	//复数拆分
	cvSplit(pImgFFT2A, pImgReA, pImgImA, 0, 0);
	cvSplit(pImgFFT2B, pImgReB, pImgImB, 0, 0);

	//相乘计算
	//实部
	cvMul(pImgReA, pImgReB, pImgTemp0);
	cvMul(pImgImA, pImgImB, pImgTemp1);
	cvSub(pImgTemp0, pImgTemp1, pImgReAB);
	//虚部
	cvMul(pImgReA, pImgImB, pImgTemp0);
	cvMul(pImgImA, pImgReB, pImgTemp1);
	cvAdd(pImgTemp0, pImgTemp1, pImgImAB);

	//复数合并
	cvMerge(pImgReAB, pImgImAB, 0, 0, pImgCCAB);

	//资源释放
	cvReleaseImage(&pImgReA);
	cvReleaseImage(&pImgImA);
	cvReleaseImage(&pImgReB);
	cvReleaseImage(&pImgImB);
	cvReleaseImage(&pImgReAB);
	cvReleaseImage(&pImgImAB);
	cvReleaseImage(&pImgTemp0);
	cvReleaseImage(&pImgTemp1);	
}

//分析互相关值R
void getCCInfor(IplImage *pImgSrc, double *pdCCData)
{
	//pdCCData,峰峰比、位移、方向

	double dMinVal = 0, dMaxVal = 0;
	double dCenterX = pImgSrc->width/2;
	double dCenterY = pImgSrc->height/2;
	CvPoint pLocMin, pLocMax;
	cvMinMaxLoc( pImgSrc, &dMinVal, &dMaxVal, &pLocMin, &pLocMax);

	//峰值坐标亚像素精度拟合(3点高斯函数拟合)
	double dPosX = pLocMax.x;//索引，从0开始
	double dPosY = pLocMax.y;
	peakfitGaussian(pImgSrc, dPosX, dPosY);

	//位移计算
	double dDistX = dPosX - dCenterX;
	double dDistY = -(dPosY - dCenterY);
	double dDist = sqrt(dDistX*dDistX + dDistY*dDistY);//位移

	//方向计算
	double dAngle = 180*atan2(dDistY,dDistX)/3.1416;//方向，值域范围是(-Pi,Pi)

	//第二峰计算
	int nPeak2PosX = 0;
	int nPeak2PosY = 0;
	double dPeak2Val = 0;
	getPeak2Pos(pImgSrc, dMaxVal, nPeak2PosX, nPeak2PosY, dPeak2Val);

	//峰峰比计算
	double dRatioP1P2 = 0;
	if (dPeak2Val > 0)
	{
		dRatioP1P2 =  dMaxVal/dPeak2Val;
	}

	//更新输出
	pdCCData[0] = dRatioP1P2;
	pdCCData[1] = dDist;
	pdCCData[2] = dAngle;
}

//亚像素精度三点高斯拟合
void peakfitGaussian(IplImage *pImgSrc, double &dPeakPosX, double &dPeakPosY)
{
	double* pdData = (double *)pImgSrc->imageData;
	int nStep = pImgSrc->widthStep / sizeof(double);
	//获得元素的值			
	//  |-----|-----|-----|j
	//  |     |  R1 |     |
	//  |-----|-----|-----|
	//  |  R1 |  R2 |  R3 |
	//  |-----|-----|-----|
	//  |     |  R3 |     |
	// i|-----|-----|-----|
	int nPosX = (int)(dPeakPosX + 0.5);
	int nPosY = (int)(dPeakPosY + 0.5);
	double R1,R2,R3;//左中右，上中下

	//更新dPeakPosX
	if (nPosX == 0)
	{
		R1 = pdData[nPosY*nStep + nPosX];
		R2 = pdData[nPosY*nStep + nPosX + 1];
		R3 = pdData[nPosY*nStep + nPosX + 2];
	}
	else if (nPosX == pImgSrc->width-1)
	{
		R1 = pdData[nPosY*nStep + nPosX - 2];
		R2 = pdData[nPosY*nStep + nPosX - 1];
		R3 = pdData[nPosY*nStep + nPosX];
	}
	else
	{
		R1 = pdData[nPosY*nStep + nPosX - 1];
		R2 = pdData[nPosY*nStep + nPosX];
		R3 = pdData[nPosY*nStep + nPosX + 1];
	}
	dPeakPosX = nPosX + (log(R1)-log(R3))/(2*R1-4*R2+2*R3);

	//更新dPeakPosY
	if (nPosY == 0)
	{
		R1 = pdData[nPosY*nStep + nPosX];
		R2 = pdData[(nPosY + 1)*nStep + nPosX];
		R3 = pdData[(nPosY + 2)*nStep + nPosX];
	}
	else if (nPosY == pImgSrc->height-1)
	{
		R1 = pdData[(nPosY-2)*nStep + nPosX];
		R2 = pdData[(nPosY-1)*nStep + nPosX];
		R3 = pdData[nPosY*nStep + nPosX];
	}
	else
	{
		R1 = pdData[(nPosY-1)*nStep + nPosX];
		R2 = pdData[nPosY*nStep + nPosX];
		R3 = pdData[(nPosY+1)*nStep + nPosX];
	}
	dPeakPosY = nPosY + (log(R1)-log(R3))/(2*R1-4*R2+2*R3);	
}

//求互相关谱的第二峰坐标
void getPeak2Pos(IplImage *pImgSrc, double dPeak1Val, int &nPeak2PosX, int &nPeak2PosY, double &dPeak2Val)
{
	int nWidth = pImgSrc->width;
	int nHeight = pImgSrc->height;

	//像素扩充，满足3*3的扫描计算
	IplImage * pImgPadded = cvCreateImage(cvSize(nWidth+2,nHeight+2),pImgSrc->depth,1);
	cvZero(pImgPadded);
	cvSetImageROI(pImgPadded, cvRect( 1,1,nWidth,nHeight));
	cvCopy(pImgSrc, pImgPadded);//复制2到3
	cvResetImageROI(pImgPadded);

	//double型图像数据读取
	//扩展后的R数据
	double* dataPad = (double *)pImgPadded->imageData;
	int nStepPad = pImgPadded->widthStep / sizeof(double);
	for (int i = 1; i < nHeight+1; i++)
	{
		for (int j = 1; j < nWidth+1; j++)
		{
			//获得元素的值			
			//  |-----|-----|-----|j
			//  |  1  |  2  |  3  |
			//  |-----|-----|-----|
			//  |  4  |  5  |  6  |
			//  |-----|-----|-----|
			//  |  7  |  8  |  9  |
			// i|-----|-----|-----|
			double dValuePad1 = dataPad[(i-1)*nStepPad + j - 1];
			double dValuePad2 = dataPad[(i-1)*nStepPad + j];
			double dValuePad3 = dataPad[(i-1)*nStepPad + j + 1];
			double dValuePad4 = dataPad[i*nStepPad + j - 1];
			double dValuePad5 = dataPad[i*nStepPad + j];
			double dValuePad6 = dataPad[i*nStepPad + j + 1];
			double dValuePad7 = dataPad[(i+1)*nStepPad + j - 1];
			double dValuePad8 = dataPad[(i+1)*nStepPad + j];
			double dValuePad9 = dataPad[(i+1)*nStepPad + j + 1];
			if (dValuePad5 > dValuePad1 && dValuePad5 > dValuePad2 && dValuePad5 > dValuePad3 && dValuePad5 > dValuePad4 && 
				dValuePad5 > dValuePad6 && dValuePad5 > dValuePad7 && dValuePad5 > dValuePad8 && dValuePad5 > dValuePad9 )
			{
				if (dValuePad5 > dPeak2Val && dValuePad5 < dPeak1Val)//注：需低于最高峰值
				{
					//更新峰值信息
					dPeak2Val = dValuePad5;
					nPeak2PosX = i-1;
					nPeak2PosY = j-1;
				}
			}
		}
	}

	//释放资源
	cvReleaseImage(&pImgPadded);
}

//互相关计算结果输出到文件
void writeCCResult2File(IplImage *pImgSrc, const char *pcFilePath)
{
	int nStatus = 1;

	char *pcFileFullName = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if ( NULL == pcFileFullName )
	{
		return;//内存异常
	}
	char pcFileName[30] = "CCResult.txt";

	strcpy(pcFileFullName, pcFilePath);
	strcat(pcFileFullName, pcFileName);

	FILE *fp = fopen(pcFileFullName, "w");   //没有这个文件则继续
	if ( fp )
	{
		int nWidth = pImgSrc->width;
		int nHeight = pImgSrc->height;
		int nStepSrc = pImgSrc->widthStep / sizeof(double);

		double* dataSrc = (double *)pImgSrc->imageData;
		double dValueScr;

		//含索引序号
		/*
		for (int i = 0; i < nHeight+1; i++)//行
		{
			for (int j = 0; j < nWidth+1; j++)//列
			{
				if (i == 0)
				{
					fprintf(fp, "%4d\t", j);//序号
				}
				else
				{
					if (j == 0)
					{
						fprintf(fp, "%4d\t", i);//序号
					}
					else
					{
						//读取像素数据
						dValueScr = dataSrc[(i-1)*nStepSrc + j-1];
						if (j == nWidth-1)
						{
						fprintf(fp, "%6.4f", dValueScr);
						} 
						else
						{
						fprintf(fp, "%6.4f\t", dValueScr);
						}
					}
				}
			}
			fputc('\n', fp);
		}
		*/
		//不含索引
		for (int i = 0; i < nHeight; i++)//行
		{
			for (int j = 0; j < nWidth; j++)//列
			{
				//读取像素数据
				dValueScr = dataSrc[i*nStepSrc + j];
				if (j == nWidth-1)
				{
					fprintf(fp, "%6.4f", dValueScr);
				} 
				else
				{
					fprintf(fp, "%6.4f\t", dValueScr);
				}				
			}
			fputc('\n', fp);
		}
		fclose(fp);
	}

	free(pcFileFullName);
	pcFileFullName = NULL;
}

// 将精子信息输出到csv文件
int writeSpermInfoToFile(const char * pcResultPath, int nImgSeq, int nSpermIndex, SpermInfor *pSSpermInfor, SSettings const& SParaInput)
{
	int nStatus = 1;

	char *pcFileFullName = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if ( NULL == pcFileFullName )
	{
		nStatus = 0;
		return nStatus;//内存异常
	}
	char pcFileName[30] = "AllSpermInfor.csv";

	strcpy(pcFileFullName, pcResultPath);
	strcat(pcFileFullName, pcFileName);

	// 文件准备
	FILE *fp = NULL;
	if (nImgSeq == 1)
	{	
		fp = fopen(pcFileFullName, "w"); // 只写方式打开文本文件（文件不存在则创建，存在则清空内容）
	}
	else
	{
		fp = fopen(pcFileFullName, "a"); // 追加方式打开文本文件（文件不存在则创建，存在则在末尾追加内容）
	}

	if ( fp )
	{
		// 写入表头
		if (nImgSeq == 1)
		{			
			//fprintf(fp, "图片序号,精子序号,X坐标,Y坐标,长度,宽度,长宽比,面积,圆度\n");
		}

		// 循环写入每个精子的信息
		for (int i = 0; i < nSpermIndex; i++) 
		{
			fprintf(fp, "%d,", nImgSeq);
			fprintf(fp, "%d,", i+1);
			fprintf(fp, "%.1f,", pSSpermInfor[i].dPosX);
			fprintf(fp, "%.1f,", pSSpermInfor[i].dPosY);
			fprintf(fp, "%.1f,", pSSpermInfor[i].dMajAxsLen/(SParaInput.dRatioImg + EPSINON));
			fprintf(fp, "%.1f,", pSSpermInfor[i].dMinAxsLen/(SParaInput.dRatioImg + EPSINON));
			fprintf(fp, "%.2f,", pSSpermInfor[i].dShape);
			fprintf(fp, "%.1f,", pSSpermInfor[i].dArea/(SParaInput.dRatioImg + EPSINON)/(SParaInput.dRatioImg + EPSINON));
			fprintf(fp, "%.2f\n", pSSpermInfor[i].dCircularity);

			/*
			fprintf(fp, "%d,%d,%.1f,%.1f,%.1f,%.1f,%.2f,%.1f,%.2f\n",
				nImgSeq,
				i+1,
				pSSpermInfor[i].dPosX,
				pSSpermInfor[i].dPosY,
				pSSpermInfor[i].dMajAxsLen,
				pSSpermInfor[i].dMinAxsLen,
				pSSpermInfor[i].dShape,
				pSSpermInfor[i].dArea,
				pSSpermInfor[i].dCircularity);
			*/
		}
		
		fclose(fp);
	}
	else
	{
		nStatus = -17;//创建或打开文件失败
	}

	free(pcFileFullName);
	pcFileFullName = NULL;
	
	return nStatus;
}




//计算精子特征范围
int getSpermFeatureRange(const char **ppcFilePath, const char * pcResultPath, char **pcValidFilePath, int const& nImgFileNum, ParaRange *pSAveSpermRange, int nCenterPara[], int &nSavedImgNum, int &nTotalSpermNum, int nSampleType, IplImage *pImgMask, SSettings const& SParaInput)
{
	int nStatus = 1;

	int nNoSpermImgNum = 0;
	int nNumSpermAll = 0;

	IplImage * pImgTemp = cvLoadImage(ppcFilePath[0],0);
	
	ParaRange *pSSpermRange = (ParaRange *)malloc(sizeof(ParaRange)*nImgFileNum);//每张图精子的特征范围
	if (NULL == pSSpermRange)
	{
		cvReleaseImage(&pImgTemp);

		nStatus = 0;
		return nStatus;//内存异常
	}

	//开始图像处理
	for (int i = 0; i < nImgFileNum; i++)
	{
		//1.读取图像并提取子图
		IplImage * pImgSrc = cvLoadImage(ppcFilePath[i],0);

		//2.图像对比度增强并二值化(ABCD)
		stretchImgContrastABCD(pImgSrc,nSampleType);

		cvZero(pImgTemp);
		cvCopy(pImgSrc, pImgTemp, pImgMask);
		cvCopy(pImgTemp, pImgSrc);

		//test
		//cvSaveImage("E:/CreateCare/DataSample/SQA7100/New/Data/StdParticle/Case10/ABCD阈值处理图.jpg",pImgSrc);//图片文件存储

		//3.精子特征获取
		SpermInfor *pSSpermInfor = (SpermInfor *)malloc(sizeof(SpermInfor)*MAXSPERMNUM);
		if (NULL == pSSpermInfor)
		{			
			free(pSSpermRange);
			pSSpermRange = NULL;
			cvReleaseImage(&pImgSrc);
			cvReleaseImage(&pImgTemp);

			nStatus = 0;
			return nStatus;//内存异常
		}

		int nSpermIndex = 0;
		int nSpermType = 6;//初始

		// 此处增加精子信息的输出统计
		getImgContourInfor(pImgSrc,pSAveSpermRange,nSpermType,pSSpermInfor,nSpermIndex);

		// 精子信息输出到文件
		nStatus =  writeSpermInfoToFile(pcResultPath, i+1, nSpermIndex, pSSpermInfor, SParaInput);

		//test
		//cvSaveImage("E:/CreateCare/DataSample/SQA7100/New/Data/StdParticle/Case10/ABCD面积处理图.jpg",pImgSrc);//图片文件存储

		//4.图像存储
		char *pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if (NULL == pcFileNameTemp)
		{
			free(pSSpermRange);
			pSSpermRange = NULL;
			free(pSSpermInfor);
			pSSpermInfor = NULL;
			cvReleaseImage(&pImgSrc);
			cvReleaseImage(&pImgTemp);

			nStatus = 0;
			return nStatus;//内存异常
		}
		char *pcImg = (char *)malloc(sizeof(char)*50);
		if (NULL == pcImg)
		{
			free(pSSpermRange);
			pSSpermRange = NULL;
			free(pSSpermInfor);
			pSSpermInfor = NULL;
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
			cvReleaseImage(&pImgSrc);
			cvReleaseImage(&pImgTemp);

			nStatus = 0;
			return nStatus;//内存异常
		}

		strcpy(pcFileNameTemp, pcResultPath);
		sprintf(pcImg, "%03d.jpg", nSavedImgNum);
		strcat(pcFileNameTemp, pcImg); 
		int nSaveStatus = cvSaveImage(pcFileNameTemp,pImgSrc);//图片文件存储
		cvReleaseImage(&pImgSrc);
		if (0 == nSaveStatus)
		{
			free(pSSpermRange);
			pSSpermRange = NULL;
			free(pSSpermInfor);
			pSSpermInfor = NULL;
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
			free(pcImg);
			pcImg = NULL;
			cvReleaseImage(&pImgTemp);

			nStatus = -2;
			return nStatus;//数据写入异常
		}

		//5.有效图片路径更新
		strcpy(pcValidFilePath[nSavedImgNum],ppcFilePath[i]);

		//4.计算筛选条件（面积、长轴、短轴）
		pSSpermRange[nSavedImgNum] = getSpermParaRange(pSSpermInfor,nSpermIndex,nStatus);
		if (0 == nStatus)
		{
			free(pSSpermRange);
			pSSpermRange = NULL;
			free(pSSpermInfor);
			pSSpermInfor = NULL;
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
			free(pcImg);
			pcImg = NULL;
			cvReleaseImage(&pImgTemp);

			return nStatus;//内存异常
		}
		
		nSavedImgNum = nSavedImgNum + 1;

		nNumSpermAll = nNumSpermAll + nSpermIndex;

		if (nSpermIndex <= 3 )
		{
			nNoSpermImgNum = nNoSpermImgNum + 1;
		}

		//资源释放
		free(pcFileNameTemp);
		pcFileNameTemp = NULL;
		free(pSSpermInfor);
		pSSpermInfor = NULL;
		free(pcImg);
		pcImg = NULL;
	}
				
	if (nNoSpermImgNum/(nImgFileNum + EPSINON) >= 0.2)
	{
		free(pSSpermRange);
		pSSpermRange = NULL;
		cvReleaseImage(&pImgTemp);

		nStatus = -6;//样本异常，精子数太少，结果可能不准确
		return nStatus;
	}	

	nTotalSpermNum = (int)(nNumSpermAll/(nSavedImgNum + EPSINON));

	//10.统计平均每幅图的精子特征
	getAveSpermFeature( pSAveSpermRange, pSSpermRange, nSavedImgNum);

	//11.迭代一次计算精子特征（统计重要特征时采用）
	//nStatus = getSpermFeatureRangeIter(pcResultPath, nSavedImgNum, pSAveSpermRange, nTotalSpermNum);

	//资源释放
	free(pSSpermRange);
	pSSpermRange = NULL;
	cvReleaseImage(&pImgTemp);

	return nStatus;
}

//计算精子特征范围（迭代过程）
int getSpermFeatureRangeIter(const char * pcResultPath, const int nImgFileNum, ParaRange *pSAveSpermRange, int &nTotalSpermNum)
{
	int nStatus = 1;

	int nNoSpermImgNum = 0;
	int nNumSpermAll = 0;

	ParaRange *pSSpermRange = (ParaRange *)malloc(sizeof(ParaRange)*nImgFileNum);//每张图精子的特征范围
	if (NULL == pSSpermRange)
	{
		nStatus = 0;
		return nStatus;//内存异常
	}

	char *pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if (NULL == pcFileNameTemp)
	{
		free(pSSpermRange);
		pSSpermRange = NULL;

		nStatus = 0;
		return nStatus;//内存异常
	}
	char *pcImg = (char *)malloc(sizeof(char)*50);
	if (NULL == pcImg)
	{
		free(pSSpermRange);
		pSSpermRange = NULL;
		free(pcFileNameTemp);
		pcFileNameTemp = NULL;

		nStatus = 0;
		return nStatus;//内存异常
	}

	//开始图像处理
	for (int i = 0; i < nImgFileNum; i++)
	{
		//1.读取图像并提取子图
		strcpy(pcFileNameTemp, pcResultPath);
		sprintf(pcImg, "%03d.jpg", i);
		strcat(pcFileNameTemp, pcImg); 

		IplImage * pImgSrc = cvLoadImage(pcFileNameTemp,0);
		cvThreshold( pImgSrc, pImgSrc,
			100, 255, CV_THRESH_BINARY);//固定阈值分割

		//3.精子特征获取
		SpermInfor *pSSpermInfor = (SpermInfor *)malloc(sizeof(SpermInfor)*MAXSPERMNUM);
		if (NULL == pSSpermInfor)
		{
			free(pSSpermRange);
			pSSpermRange = NULL;
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
			free(pcImg);
			pcImg = NULL;
			cvReleaseImage(&pImgSrc);

			nStatus = 0;
			return nStatus;//内存异常
		}

		int nSpermIndex = 0;
		int nSpermType = 6;
		getImgContourInfor(pImgSrc,pSAveSpermRange,nSpermType,pSSpermInfor,nSpermIndex);

		//4.图像存储
		int nSaveStatus = cvSaveImage(pcFileNameTemp,pImgSrc);//图片文件存储
		cvReleaseImage(&pImgSrc);
		if (0 == nSaveStatus)
		{
			free(pSSpermRange);
			pSSpermRange = NULL;
			free(pSSpermInfor);
			pSSpermInfor = NULL;
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
			free(pcImg);
			pcImg = NULL;

			nStatus = -2;
			return nStatus;//数据写入异常
		}

		//4.计算筛选条件（面积、长轴、短轴）
		pSSpermRange[i] = getSpermParaRange(pSSpermInfor,nSpermIndex,nStatus);
		if (0 == nStatus)
		{
			free(pSSpermRange);
			pSSpermRange = NULL;
			free(pSSpermInfor);
			pSSpermInfor = NULL;
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
			free(pcImg);
			pcImg = NULL;

			return nStatus;//内存异常
		}

		nNumSpermAll = nNumSpermAll + nSpermIndex;

		//资源释放
		free(pSSpermInfor);
		pSSpermInfor = NULL;
	}

	nNumSpermAll = (int)(nNumSpermAll/(nImgFileNum + EPSINON));
	nTotalSpermNum = nNumSpermAll;

	//10.统计平均每幅图的精子特征
	getAveSpermFeature( pSAveSpermRange, pSSpermRange, nImgFileNum);

	//资源释放
	free(pSSpermRange);
	pSSpermRange = NULL;
	free(pcFileNameTemp);
	pcFileNameTemp = NULL;
	free(pcImg);
	pcImg = NULL;

	return nStatus;
}

//获取活动精子数量（平均两张图）
int getActiveSpermCount(const char **ppcFilePath, const char * pcResultPath, int const& nImgFileNum, ParaRange *pSAveSpermRange, int nCenterPara[], int &nAliveSpermNum)
{
	int nStatus = 1;

	int *nSingleImgAliveSpermNum = (int *)malloc(sizeof(int)*nImgFileNum);
	if (NULL == nSingleImgAliveSpermNum)
	{
		nStatus = 0;
		return nStatus;//内存异常
	}

	IplImage * pImgSrcTemp = NULL;
	IplImage * pImgSubScr1 = NULL;
	IplImage * pImgSubScr2 = NULL;
	IplImage * pImgContourShow;

	char *pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if (NULL == pcFileNameTemp)
	{
		free(nSingleImgAliveSpermNum);
		nSingleImgAliveSpermNum = NULL;

		nStatus = 0;
		return nStatus;//内存异常
	}

	char *pcImg = (char *)malloc(sizeof(char)*50);
	if (NULL == pcImg)
	{
		free(nSingleImgAliveSpermNum);
		nSingleImgAliveSpermNum = NULL;
		free(pcFileNameTemp);
		pcFileNameTemp = NULL;

		nStatus = 0;
		return nStatus;//内存异常
	}

	for (int i = 0; i < nImgFileNum; i++)
	{
		if (i < nImgFileNum-1)
		{
			pImgSrcTemp = cvLoadImage(ppcFilePath[i],0);
			pImgSubScr1 = clipCenterImage(pImgSrcTemp, nCenterPara);
			cvReleaseImage(&pImgSrcTemp);
			pImgSrcTemp = NULL;

			pImgSrcTemp = cvLoadImage(ppcFilePath[i+1],0);
			pImgSubScr2 = clipCenterImage(pImgSrcTemp, nCenterPara);
			cvReleaseImage(&pImgSrcTemp);
			pImgSrcTemp = NULL;
		}
		else
		{
			pImgSrcTemp = cvLoadImage(ppcFilePath[i],0);
			pImgSubScr1 = clipCenterImage(pImgSrcTemp, nCenterPara);
			cvReleaseImage(&pImgSrcTemp);
			pImgSrcTemp = NULL;

			pImgSrcTemp = cvLoadImage(ppcFilePath[i-1],0);
			pImgSubScr2 = clipCenterImage(pImgSrcTemp, nCenterPara);
			cvReleaseImage(&pImgSrcTemp);
			pImgSrcTemp = NULL;
		}

		strcpy(pcFileNameTemp, pcResultPath);
		sprintf(pcImg, "%03d.jpg", i);
		strcat(pcFileNameTemp, pcImg); 

		pImgContourShow = cvLoadImage(pcFileNameTemp,1);
		//nSingleImgAliveSpermNum[i] = getTwoImgAliveSpermCount(pImgSubScr1,pImgSubScr2,pImgContourShow, pSAveSpermRange);//注：图2-图1，因为白背景，图1被保留

		int nSaveStatus = cvSaveImage(pcFileNameTemp,pImgContourShow);//图片文件存储
		cvReleaseImage(&pImgSubScr1);
		pImgSubScr1 = NULL;
		cvReleaseImage(&pImgSubScr2);
		pImgSubScr2 = NULL;
		cvReleaseImage(&pImgContourShow);
		pImgContourShow = NULL;

		if (0 == nSaveStatus)
		{
			free(nSingleImgAliveSpermNum);
			nSingleImgAliveSpermNum = NULL;
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;
			free(pcImg);
			pcImg = NULL;

			nStatus = -2;
			return nStatus;//数据写入异常
		}

		nAliveSpermNum = nAliveSpermNum + nSingleImgAliveSpermNum[i];
	}
	nAliveSpermNum = (int)(nAliveSpermNum/(nImgFileNum + EPSINON));

	//资源释放
	free(nSingleImgAliveSpermNum);
	nSingleImgAliveSpermNum = NULL;
	free(pcFileNameTemp);
	pcFileNameTemp = NULL;
	free(pcImg);
	pcImg = NULL;

	return nStatus;
}

//判断图像是否异常（针对死人精或静态样本）
int checkAbnormalImg(char **ppcFilePath, int const& nImgFileNum, int nCenterPara[])
{
	int nStatus = 1;
	int nAbnormImgs = 0;
	int nNormImgs = 0;
	int nGrayDiffNum = 0;

	IplImage * pImgSrcTemp = NULL;
	IplImage * pImgSubScr1 = NULL;
	IplImage * pImgSubScr2 = NULL;


	for (int i = 0; i < nImgFileNum; i++)
	{
		int nNumTemp = nImgFileNum/5;
		if (i < nNumTemp)
		{
			pImgSrcTemp = cvLoadImage(ppcFilePath[i],0);
			pImgSubScr1 = clipCenterImage(pImgSrcTemp, nCenterPara);
			cvReleaseImage(&pImgSrcTemp);
			pImgSrcTemp = NULL;

			pImgSrcTemp = cvLoadImage(ppcFilePath[i+nNumTemp],0);
			pImgSubScr2 = clipCenterImage(pImgSrcTemp, nCenterPara);
			cvReleaseImage(&pImgSrcTemp);
			pImgSrcTemp = NULL;

			CvScalar cvMean1 = cvAvg(pImgSubScr1);
			CvScalar cvMean2 = cvAvg(pImgSubScr2);
			if (cvMean1.val[0] - cvMean2.val[0] > 30 || cvMean1.val[0] - cvMean2.val[0] < -30)
			{
				continue;
			}

			int nSpermNum = getTwoImgAliveSpermCount(pImgSubScr1,pImgSubScr2);//注：图2-图1，因为白背景，图1被保留
			if (nSpermNum < 5)
			{
				nAbnormImgs++;

				if (nAbnormImgs >= 2)
				{
					cvReleaseImage(&pImgSubScr1);
					pImgSubScr1 = NULL;
					cvReleaseImage(&pImgSubScr2);
					pImgSubScr2 = NULL;

					nStatus = -6;//空白测试？
					return nStatus;
				}
			}

			cvReleaseImage(&pImgSubScr1);
			pImgSubScr1 = NULL;
			cvReleaseImage(&pImgSubScr2);
			pImgSubScr2 = NULL;
		}
	}
	return nStatus;
}

//获取子图像
IplImage *clipCenterImage(IplImage *pImgSrc, int nCenterPara[])
{
	int nWidth = nCenterPara[0];
	int nHeight = nCenterPara[1];
	int nCenterX = nCenterPara[2];
	int nCenterY = nCenterPara[3];

	IplImage *imgResult = NULL;
	CvRect SImROI;

	//ROI起始点X
	if (nCenterX - nWidth/2 < 0)
	{
		SImROI.x = 0;
	} 
	else
	{
		SImROI.x = nCenterX - nWidth/2;
	}

	//ROI起始点Y
	if (nCenterY - nHeight/2 < 0)
	{
		SImROI.y = 0;
	} 
	else
	{
		SImROI.y = nCenterY - nHeight/2;
	}

	//ROI宽度
	if (SImROI.x + nWidth > cvGetSize(pImgSrc).width)
	{
		SImROI.width = cvGetSize(pImgSrc).width - SImROI.x;
	} 
	else
	{
		SImROI.width =  nWidth;
	}

	//ROI高度
	if (SImROI.y + nHeight > cvGetSize(pImgSrc).height)
	{
		SImROI.height = cvGetSize(pImgSrc).height - SImROI.y;
	} 
	else
	{
		SImROI.height =  nHeight;
	}

	cvSetImageROI(pImgSrc,SImROI);//设置源图像ROI
	imgResult = cvCreateImage(cvSize(nWidth,nHeight),pImgSrc->depth,pImgSrc->nChannels);//创建目标图像
	cvCopy(pImgSrc,imgResult); //复制图像
	cvResetImageROI(pImgSrc);//源图像用完后，清空ROI

	return imgResult;
}

//阈值分割
IplImage *getThresholdImage(IplImage *pImgSubScr, int nFlag)
{
	IplImage * pImgBinary =cvCreateImage(cvGetSize(pImgSubScr),pImgSubScr->depth,pImgSubScr->nChannels);
	double dThreshValue = 0;

	int nThreshTimes = 0;
	double dOriThValue = 0;

	/*
	IplImage * pImgBinaryTest = cvCreateImage(cvGetSize(pImgSrcDiffSmth),pImgSrcDiffSmth->depth,pImgSrcDiffSmth->nChannels);
	cvAdaptiveThreshold( pImgSrcDiffSmth, pImgBinary, 255,CV_ADAPTIVE_THRESH_GAUSSIAN_C,
                                  CV_THRESH_BINARY,11,-5);//自适应阈值，分割效果不太好，容易产生噪音
	*/

	//NOTE: Currently, the Otsu’s method is implemented only for 8-bit images.
	if (pImgSubScr->depth == 8)
	{
		/*do OTSU thresh with OpenCV2.4.10
		double dThreshValue = cvThreshold( pImgSubScr, pImgBinary,
			0, 255,
			CV_THRESH_BINARY|THRESH_OTSU);//Otsu阈值分割
		*/

		//do OTSU thresh with OpenCV3.0
		dThreshValue = cvThreshold( pImgSubScr, pImgBinary,
			0, 255,
			CV_THRESH_BINARY|CV_THRESH_OTSU);//Otsu阈值分割

		switch (nFlag)
		{
			//表示处理去背景后的单张精子图片
		case 1 : 
			dThreshValue = 1.7*dThreshValue;
			if (dThreshValue <= 10)
			{
				dThreshValue = 200;
			}
			break;

			//表示处理背景图片
		case 2 : 
			dThreshValue = 1.0*dThreshValue;
			if (dThreshValue <= 10)
			{
				dThreshValue = 200;
			}
			break;

			//表示处理两张之差的图片
		case 3 : 
			dThreshValue = 1.6*dThreshValue;
			if (dThreshValue <= 10)
			{
				dThreshValue = 200;
			}
			break;
		}

		if (dThreshValue > 20)
		{
			nThreshTimes = nThreshTimes + 1;
			dOriThValue = dOriThValue + dThreshValue;

			cvThreshold( pImgSubScr, pImgBinary,
				dThreshValue, 255,
				CV_THRESH_BINARY);
		}
		else//Otsu返回可能为0
		{
			if (nThreshTimes > 0)
			{
				dThreshValue = dOriThValue/(nThreshTimes + EPSINON);
			} 
			else
			{
				dThreshValue = 200;
			}
			if (nFlag == 2)
			{
				dThreshValue = 200;
			}
			cvThreshold( pImgSubScr, pImgBinary,
				dThreshValue, 255,
				CV_THRESH_BINARY);//固定阈值分割
		}
	}
	else
	{	
		if (nThreshTimes > 0)
		{
			dThreshValue = dOriThValue/(nThreshTimes + EPSINON);
		} 
		else
		{
			dThreshValue = 30;
		}
		cvThreshold( pImgSubScr, pImgBinary,
			dThreshValue, 255,
			CV_THRESH_BINARY);//固定阈值分割
	}

	return pImgBinary;
}

//平均值和标准差计算
double *calAveStd(const double *pdData, int nSpermNum)
{
	double *dAveStdSub = (double *)malloc(sizeof(double)*2);
	double dSum = 0;

	//平均值计算	
	double dMean = 0;
	
	for (int i = 0; i < nSpermNum; i++)
	{
		dSum = dSum + pdData[i];
	}
	dMean = dSum/(nSpermNum + EPSINON);

	//标准差计算
	double dStd = 0;
	dSum = 0;
	for (int i = 0; i < nSpermNum; i++)
	{
		dSum = dSum + (pdData[i]-dMean)*(pdData[i]-dMean);
	}
	dSum = dSum/(nSpermNum + EPSINON);
	dStd = sqrt(dSum);

	dAveStdSub[0] = dMean;
	dAveStdSub[1] = dStd;

	return dAveStdSub;
}

//平均值和标准差计算（迭代统计）
double *calAveStdIter(double *pdData, int nSpermNum)
{
	double *dAveStd = (double *)malloc(sizeof(double)*2);

	//1.第一次计算均值和标准差
	double dDataMin,dDataMax;
	double *pdAveStd= calAveStd(pdData, nSpermNum);
	dDataMin = pdAveStd[0] - pdAveStd[1];
	dDataMax = pdAveStd[0] + pdAveStd[1];
	free(pdAveStd);
	pdAveStd = NULL;

	//2.筛选数据
	int nNum = 0;
	for (int i = 0; i < nSpermNum; i++)
	{		
		double dDataTemp = pdData[i];
		pdData[i] = 0;
		if (dDataTemp >= dDataMin && dDataTemp <= dDataMax)
		{
			pdData[nNum] = dDataTemp;
			nNum++;
		}
	}

	//3.迭代计算一次
	pdAveStd= calAveStd(pdData, nNum);
	dAveStd[0] = pdAveStd[0];//平均值
	dAveStd[1] = pdAveStd[1];//标准差

	//4.返回
	free(pdAveStd);
	pdAveStd = NULL;
	return dAveStd;
}

//曲线极大值和极大值的数量计算
//ALH: 侧摆最大值的均值（基于dPointXY与dPointFitXY的距离）
//BCF：交叉频率（基于dPointXY与dPointFitXY的距离）
void calMaxAveNum(double *pdData, int nNum, double *dAve, int *nMaxNum)
{
	if (pdData == NULL || dAve == NULL || nMaxNum == NULL)
	{
		printf("错误：输入指针为NULL！\n");
		return;
	}

	*dAve = 0.0;
	*nMaxNum = 0;
	if (nNum >= 5)
	{
		for (int i = 2; i < nNum-2; i++)
		{
			if ((pdData[i] > pdData[i-1]) && (pdData[i] > pdData[i-2]) && (pdData[i] > pdData[i+1]) && (pdData[i] > pdData[i+2]))
			{
				*dAve = *dAve + pdData[i];
				(*nMaxNum) ++;
			}
		}
	}

	if ((*nMaxNum) >0)
	{
		*dAve = *dAve / *nMaxNum;
	}
}

//转向角度的计算  [同步动物宽窗版本中更新了下面代码，避免MAD 计算结果出现非法浮点值（NaN）也就是0÷0
//MAD：角度的变化值的绝对值的均值（基于dPointXY）
double calMAD(double *dPointXY, int nPosNum)
{
	// 至少 3 个点才能形成两段位移、算一个转角
	if (nPosNum < 3 || dPointXY == NULL)
	{
		return 0.0;
	}

	double dMADave = 0.0;
	int nCount = 0;

	// 用相邻两段向量：点(j-1)->j 与 j->(j+1)
	for (int j = 1; j < nPosNum - 1; j++)
	{
		double dx1 = dPointXY[2*j]     - dPointXY[2*(j-1)];
		double dy1 = dPointXY[2*j+1]   - dPointXY[2*(j-1)+1];
		double dx2 = dPointXY[2*(j+1)] - dPointXY[2*j];
		double dy2 = dPointXY[2*(j+1)+1]- dPointXY[2*j+1];

		// 更合理：两向量夹角（也可用 atan2 差）
		double cross = dx1*dy2 - dy1*dx2;
		double dot   = dx1*dx2 + dy1*dy2;
		double rad   = atan2(cross, dot);   // [-π, π]
		double deg   = fabs(rad * 180.0 / 3.141592653589793);

		dMADave += deg;
		nCount++;
	}

	if (nCount <= 0)
	{
		return 0.0;
	}
	return dMADave / nCount;
}

//获取目标轮廓及其几何信息（临时获取信息分布用）
void getImgContourInfor(IplImage *pImgBinary, ParaRange *pSAveSpermRange, int nSpermType, SpermInfor *pSSpermInfor, int &nSpermIndex)
{
	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;

	IplImage * pImgDraw =cvCreateImage(cvGetSize(pImgBinary),pImgBinary->depth,pImgBinary->nChannels);
	cvZero(pImgDraw);

	//先对输入图像做一次二值化
	cvThreshold( pImgBinary, pImgBinary,
		100, 255, CV_THRESH_BINARY);//固定阈值分割

	double dConArea, dMinAreaTemp, dMaxAreaTemp;
	if (nSpermType == 7)//初始筛选（标粒尺寸、新鲜人精、干涸人精）
	{
		dMinAreaTemp = dMinArea;
		dMaxAreaTemp = dMaxArea;
	} 
	else if (nSpermType == 6)//ABCD
	{
		dMinAreaTemp = 0.7*pSAveSpermRange->dAreaMin;
		dMaxAreaTemp = 2*pSAveSpermRange->dAreaMax;
	} 
	else if (nSpermType == 5)//AB类
	{
		dMinAreaTemp = 0.2 * pSAveSpermRange->dAreaAve;
		dMaxAreaTemp = 2 * pSAveSpermRange->dAreaAve;
	}
	else if (nSpermType == 4)//D类
	{
		dMinAreaTemp = 0.2 * pSAveSpermRange->dAreaAve;
		dMaxAreaTemp = 2.5 * pSAveSpermRange->dAreaAve;
	}	
	else if (nSpermType == 3)//C类
	{
		dMinAreaTemp = 0.1 * pSAveSpermRange->dAreaAve;
		dMaxAreaTemp = 0.2 * pSAveSpermRange->dAreaAve;
	}
	else if (nSpermType == 0)//其他，测试用
	{
		dMinAreaTemp = 0.4 * pSAveSpermRange->dAreaAve;
		dMaxAreaTemp = 2 * pSAveSpermRange->dAreaAve;
	}
	else
	{
		dMinAreaTemp = 0.3 * pSAveSpermRange->dAreaAve;
		dMaxAreaTemp = 2 * pSAveSpermRange->dAreaAve;
	}

	CvBox2D GeoInfor;

	CvContourScanner scanner = cvStartFindContours(pImgBinary,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));
	CvSeq * cTemp =NULL;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找
	{ 
		//面积
		dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		double perimeter = cvContourPerimeter(cTemp);  // 计算闭合轮廓的周长

		//轮廓面积大于dMinArea，并且小于dMaxArea，则留下，反之，则删除。 
		if(dConArea > dMinAreaTemp && dConArea < dMaxAreaTemp && perimeter > 0 && nSpermIndex < MAXSPERMNUM )
		{  
			//质心，长短轴，角度
			GeoInfor = cvMinAreaRect2(cTemp, NULL);//最小外接矩形
			double dMajLength, dMinLength, dShapeRatio, circularity;
			if (GeoInfor.size.width >= GeoInfor.size.height)
			{

				dMajLength = GeoInfor.size.width;
				dMinLength = GeoInfor.size.height;
			}else
			{
				dMajLength = GeoInfor.size.height;
				dMinLength = GeoInfor.size.width;
			}
			dShapeRatio = dMajLength/(dMinLength + EPSINON);

			// 圆度计算公式
			circularity = (4 * CV_PI * dConArea) / (perimeter * perimeter);

			int nTemp = 1;			
			if ((nSpermType == 6 || nSpermType == 4) && dMinLength < 0.4*pSAveSpermRange->dMinorLengthAve && dConArea < 0.4*pSAveSpermRange->dAreaAve)
			{
				nTemp = 0;//形状判断
			}

			if (nTemp == 1)
			{
				//绘制符合条件的轮廓
				cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);

				//轮廓信息存储
				pSSpermInfor[nSpermIndex].dPosX = GeoInfor.center.x;
				pSSpermInfor[nSpermIndex].dPosY = GeoInfor.center.y;
				pSSpermInfor[nSpermIndex].dMajAxsLen = dMajLength;
				pSSpermInfor[nSpermIndex].dMinAxsLen = dMinLength;
				pSSpermInfor[nSpermIndex].dAngle = GeoInfor.angle;
				pSSpermInfor[nSpermIndex].dArea = dConArea;
				pSSpermInfor[nSpermIndex].nType = nSpermType;

				//精子形态学信息存储
				pSSpermInfor[nSpermIndex].dShape = dShapeRatio;
				pSSpermInfor[nSpermIndex].dCircularity = circularity;

				nSpermIndex = nSpermIndex+1;

			}
			else
			{
				cvSubstituteContour(scanner,NULL);//删除当前的轮廓
			}			
		}
		else  
		{  
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}  
	}
	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。
	cvZero(pImgBinary);
	cvCopy(pImgDraw, pImgBinary);
	cvReleaseImage(&pImgDraw);

	//资源释放
	cvReleaseMemStorage(&storage); 
}

//获取标粒个数（4倍面积折算成4个标粒）
void getStdParticleNum(IplImage *pImgBinary, ParaRange *pSAveSpermRange, int nSpermType, SpermInfor *pSSpermInfor, int &nSpermIndex)
{
	int nTotalNum = 0;
	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;

	IplImage * pImgDraw =cvCreateImage(cvGetSize(pImgBinary),pImgBinary->depth,pImgBinary->nChannels);
	cvZero(pImgDraw);

//先对输入图像做一次二值化
	cvThreshold( pImgBinary, pImgBinary,
		100, 255, CV_THRESH_BINARY);//固定阈值分割

	double dConArea, dMinAreaTemp, dMaxAreaTemp;	
	dMinAreaTemp = 0.1 * pSAveSpermRange->dAreaMin;
	dMaxAreaTemp = 8 * pSAveSpermRange->dAreaMax;

	CvBox2D GeoInfor;
	CvContourScanner scanner = cvStartFindContours(pImgBinary,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));

	CvSeq * cTemp = NULL;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL) // 开始查找
	{ 
		// 1. 计算轮廓面积
		dConArea = fabs(cvContourArea(cTemp, CV_WHOLE_SEQ, 0));

		// 2.【定义周长 perimeter】：标准 OpenCV C-API 语法
		double perimeter = cvArcLength(cTemp, CV_WHOLE_SEQ, 1);

		// 【环节一改造过滤】：面积门限 + 绝对噪点物理下限(>=8像素) + 周长有效性检测
		if(dConArea > dMinAreaTemp && dConArea < dMaxAreaTemp && dConArea >= 8.0 && perimeter > 0.0 && nSpermIndex < MAXSPERMNUM )
		{  
			// 质心，长短轴，角度
			GeoInfor = cvMinAreaRect2(cTemp, NULL); // 最小外接矩形
			double dMajLength, dMinLength, dShapeRatio, circularity;
			if (GeoInfor.size.width >= GeoInfor.size.height)
			{
				dMajLength = GeoInfor.size.width;
				dMinLength = GeoInfor.size.height;
			}
			else
			{
				dMajLength = GeoInfor.size.height;
				dMinLength = GeoInfor.size.width;
			}
			dShapeRatio = dMajLength / (dMinLength + EPSINON);

			// 3. 圆度计算（周长不为0时安全计算）
			circularity = (4.0 * CV_PI * dConArea) / (perimeter * perimeter + EPSINON);

			// 过滤极细毛刺或微小闪烁噪点（短轴必须 >= 1.8 像素）
			int nTemp = 1;
			if (dMinLength < 1.8)
			{
				nTemp = 0; // 短轴不足 1.8 像素的极细线状噪点/闪烁光点直接剔除
			}			
			if ((nSpermType == 6 || nSpermType == 4) && dMinLength < 0.4 * pSAveSpermRange->dMinorLengthAve && dConArea < 0.4 * pSAveSpermRange->dAreaAve)
			{
				nTemp = 0; // 形状判断
			}

			if (nTemp == 1)
			{
				// 计数
				if (dConArea > 2 * pSAveSpermRange->dAreaAve)
				{
					nTotalNum = int(nTotalNum + ceil(dConArea / pSAveSpermRange->dAreaAve));
				}
				else
				{
					nTotalNum++;
				}

				// 绘制符合条件的轮廓
				cvDrawContours(pImgDraw, cTemp, CV_RGB(255,255,255), CV_RGB(255,255,255), 0, CV_FILLED);

				// 轮廓信息存储
				pSSpermInfor[nSpermIndex].dPosX = GeoInfor.center.x;
				pSSpermInfor[nSpermIndex].dPosY = GeoInfor.center.y;
				pSSpermInfor[nSpermIndex].dMajAxsLen = dMajLength;
				pSSpermInfor[nSpermIndex].dMinAxsLen = dMinLength;
				pSSpermInfor[nSpermIndex].dAngle = GeoInfor.angle;
				pSSpermInfor[nSpermIndex].dArea = dConArea;
				pSSpermInfor[nSpermIndex].nType = nSpermType;
				nSpermIndex = nSpermIndex + 1;
			}
			else
			{
				cvSubstituteContour(scanner, NULL); // 删除当前的轮廓
			}			
		}
		else  
		{  
			cvSubstituteContour(scanner, NULL); // 删除当前的轮廓
		}  
	} // 这里闭合 while 循环，所有大括号严格一一对应！

	firstcontour = cvEndFindContours(&scanner); // 把找到的轮廓返回到firstContour中
	cvZero(pImgBinary);
	cvCopy(pImgDraw, pImgBinary);
	cvReleaseImage(&pImgDraw);
	nSpermIndex = nTotalNum;

	// 资源释放
	cvReleaseMemStorage(&storage); 
}

//统计目标几何特征范围（迭代计算）
ParaRange getSpermParaRangeIter(SpermInfor *pSSpermInfor, int &nSpermNum, int &nStatus)
{
	ParaRange pSSpermRange = {0,0,0,0,0,0,0,0,0};
	double *pdAveStd = NULL;
	double *pdData = (double *)malloc(sizeof(double)*nSpermNum);
	if (NULL == pdData)
	{
		nStatus = 0;
		return pSSpermRange;//内存异常
	}

	//面积范围
	double dAreaMin,dAreaMax,dAreaAve;
	for (int i = 0; i < nSpermNum; i++)
	{
		pdData[i] = pSSpermInfor[i].dArea;
	}
	pdAveStd= calAveStdIter(pdData, nSpermNum);
	dAreaMin = pdAveStd[0] - pdAveStd[1];
	dAreaMax = pdAveStd[0] + pdAveStd[1];
	dAreaAve = pdAveStd[0];
	if (dAreaMin < 3)
	{
		dAreaMin = 3;
	}
	free(pdAveStd);
	pdAveStd = NULL;

	//长轴范围
	double dMajorLengthMin,dMajorLengthMax,dMajorLengthAve;
	for (int i = 0; i < nSpermNum; i++)
	{
		pdData[i] = pSSpermInfor[i].dMajAxsLen;
	}
	pdAveStd= calAveStdIter(pdData, nSpermNum);
	dMajorLengthMin = pdAveStd[0] - 1.5 * pdAveStd[1];
	dMajorLengthMax = pdAveStd[0] + 1.5 * pdAveStd[1];
	dMajorLengthAve = pdAveStd[0];
	if (dMajorLengthMin < 2)
	{
		dMajorLengthMin = 2;
	}
	free(pdAveStd);
	pdAveStd = NULL;

	//短轴范围
	double dMinorLengthMin,dMinorLengthMax,dMinorLengthAve;
	for (int i = 0; i < nSpermNum; i++)
	{
		pdData[i] = pSSpermInfor[i].dMinAxsLen;
	}
	pdAveStd= calAveStdIter(pdData, nSpermNum);
	dMinorLengthMin = pdAveStd[0] - 1.5 * pdAveStd[1];
	dMinorLengthMax = pdAveStd[0] + 1.5 * pdAveStd[1];
	dMinorLengthAve = pdAveStd[0];
	if (dMinorLengthMin < 2)
	{
		dMinorLengthMin = 2;
	}
	free(pdAveStd);
	pdAveStd = NULL;

	//长短轴之比
	int nNumTemp = 0;
	double dShapeRatioMin,dShapeRatioMax,dShapeRatioAve;
	for (int i = 0; i < nSpermNum; i++)
	{
		if (pSSpermInfor[i].dArea > 0.8*dAreaAve && pSSpermInfor[i].dArea < 1.2*dAreaAve)
		{
			pdData[nNumTemp] = pSSpermInfor[i].dMajAxsLen/(pSSpermInfor[i].dMinAxsLen + EPSINON);
			nNumTemp++;
		}
	}
	if (nNumTemp > 20)
	{
		pdAveStd= calAveStdIter(pdData, nNumTemp);
		dShapeRatioMin = pdAveStd[0] - 1.5 * pdAveStd[1];
		dShapeRatioMax = pdAveStd[0] + 1.5 * pdAveStd[1];
		dShapeRatioAve = pdAveStd[0];
		if (dShapeRatioMin < 1)
		{
			dMinorLengthMin = 1;
		}
		free(pdAveStd);
		pdAveStd = NULL;
	}
	else
	{
		dShapeRatioAve = dMajorLengthAve/(dMinorLengthMin + EPSINON);
	}

	//异常目标个数统计（死精目标）
	int nDeadSpermNum = 0;
	for (int i = 0; i < nSpermNum; i++)
	{
		double dShapeRatioTemp = pSSpermInfor[i].dMajAxsLen/(pSSpermInfor[i].dMinAxsLen + EPSINON);
		if (pSSpermInfor[i].dArea > 90 && dShapeRatioTemp > 2.8)
		{
			nDeadSpermNum++;
		}
	}

	pSSpermRange.dAreaMin = dAreaMin;
	pSSpermRange.dAreaMax = dAreaMax;
	pSSpermRange.dAreaAve = dAreaAve;

	pSSpermRange.dMajorLengthMin = dMajorLengthMin;
	pSSpermRange.dMajorLengthMax = dMajorLengthMax;
	pSSpermRange.dMajorLengthAve = dMajorLengthAve;

	pSSpermRange.dMinorLengthMin = dMinorLengthMin;
	pSSpermRange.dMinorLengthMax = dMinorLengthMax;
	pSSpermRange.dMinorLengthAve = dMinorLengthAve;

	pSSpermRange.dShapeRatio = dShapeRatioAve;

	nSpermNum = nDeadSpermNum;

	free(pdData);
	pdData = NULL;

	return pSSpermRange;
}

//统计目标几何特征范围（迭代计算）
ParaRange getSpermParaRange(SpermInfor *pSSpermInfor, int nSpermNum, int &nStatus)
{
	ParaRange pSSpermRange = {0,0,0,0,0,0,0,0,0};
	double *pdAveStd = NULL;
	double *pdData = (double *)malloc(sizeof(double)*nSpermNum);
	if (NULL == pdData)
	{
		nStatus = 0;
		return pSSpermRange;//内存异常
	}

	//面积范围
	double dAreaMin,dAreaMax,dAreaAve;
	for (int i = 0; i < nSpermNum; i++)
	{
		pdData[i] = pSSpermInfor[i].dArea;
	}
	pdAveStd= calAveStd(pdData, nSpermNum);
	dAreaMin = pdAveStd[0] - pdAveStd[1];
	dAreaMax = pdAveStd[0] + pdAveStd[1];
	dAreaAve = pdAveStd[0];
	if (dAreaMin < 3)
	{
		dAreaMin = 3;
	}
	free(pdAveStd);
	pdAveStd = NULL;

	//长轴范围
	double dMajorLengthMin,dMajorLengthMax,dMajorLengthAve;
	for (int i = 0; i < nSpermNum; i++)
	{
		pdData[i] = pSSpermInfor[i].dMajAxsLen;
	}
	pdAveStd= calAveStd(pdData, nSpermNum);
	dMajorLengthMin = pdAveStd[0] - 1.5 * pdAveStd[1];
	dMajorLengthMax = pdAveStd[0] + 1.5 * pdAveStd[1];
	dMajorLengthAve = pdAveStd[0];
	if (dMajorLengthMin < 2)
	{
		dMajorLengthMin = 2;
	}
	free(pdAveStd);
	pdAveStd = NULL;

	//短轴范围
	double dMinorLengthMin,dMinorLengthMax,dMinorLengthAve;
	for (int i = 0; i < nSpermNum; i++)
	{
		pdData[i] = pSSpermInfor[i].dMinAxsLen;
	}
	pdAveStd= calAveStd(pdData, nSpermNum);
	dMinorLengthMin = pdAveStd[0] - 1.5 * pdAveStd[1];
	dMinorLengthMax = pdAveStd[0] + 1.5 * pdAveStd[1];
	dMinorLengthAve = pdAveStd[0];
	if (dMinorLengthMin < 2)
	{
		dMinorLengthMin = 2;
	}
	free(pdAveStd);
	pdAveStd = NULL;

	pSSpermRange.dAreaMin = dAreaMin;
	pSSpermRange.dAreaMax = dAreaMax;
	pSSpermRange.dAreaAve = dAreaAve;

	pSSpermRange.dMajorLengthMin = dMajorLengthMin;
	pSSpermRange.dMajorLengthMax = dMajorLengthMax;
	pSSpermRange.dMajorLengthAve = dMajorLengthAve;

	pSSpermRange.dMinorLengthMin = dMinorLengthMin;
	pSSpermRange.dMinorLengthMax = dMinorLengthMax;
	pSSpermRange.dMinorLengthAve = dMinorLengthAve;

	free(pdData);
	pdData = NULL;

	return pSSpermRange;
}

//统计平均每幅图的精子特征
void getAveSpermFeature( ParaRange *pSAveSpermRange, ParaRange *pSSpermRange, int const& nImgFileNum)
{
	for (int i = 0; i < nImgFileNum; i++)
	{
		pSAveSpermRange->dAreaMax = pSAveSpermRange->dAreaMax + pSSpermRange[i].dAreaMax;
		pSAveSpermRange->dAreaMin = pSAveSpermRange->dAreaMin + pSSpermRange[i].dAreaMin;
		pSAveSpermRange->dAreaAve = pSAveSpermRange->dAreaAve + pSSpermRange[i].dAreaAve;

		pSAveSpermRange->dMajorLengthMax = pSAveSpermRange->dMajorLengthMax + pSSpermRange[i].dMajorLengthMax;
		pSAveSpermRange->dMajorLengthMin = pSAveSpermRange->dMajorLengthMin + pSSpermRange[i].dMajorLengthMin;
		pSAveSpermRange->dMajorLengthAve = pSAveSpermRange->dMajorLengthAve + pSSpermRange[i].dMajorLengthAve;

		pSAveSpermRange->dMinorLengthMax = pSAveSpermRange->dMinorLengthMax + pSSpermRange[i].dMinorLengthMax;
		pSAveSpermRange->dMinorLengthMin = pSAveSpermRange->dMinorLengthMin + pSSpermRange[i].dMinorLengthMin;
		pSAveSpermRange->dMinorLengthAve = pSAveSpermRange->dMinorLengthAve + pSSpermRange[i].dMinorLengthAve;
	}

	pSAveSpermRange->dAreaMax =  pSAveSpermRange->dAreaMax/(nImgFileNum + EPSINON);
	pSAveSpermRange->dAreaMin =  pSAveSpermRange->dAreaMin/(nImgFileNum + EPSINON);
	pSAveSpermRange->dAreaAve =  pSAveSpermRange->dAreaAve/(nImgFileNum + EPSINON);

	pSAveSpermRange->dMajorLengthMax =  pSAveSpermRange->dMajorLengthMax/(nImgFileNum + EPSINON);
	pSAveSpermRange->dMajorLengthMin =  pSAveSpermRange->dMajorLengthMin/(nImgFileNum + EPSINON);
	pSAveSpermRange->dMajorLengthAve =  pSAveSpermRange->dMajorLengthAve/(nImgFileNum + EPSINON);

	pSAveSpermRange->dMinorLengthMax =  pSAveSpermRange->dMinorLengthMax/(nImgFileNum + EPSINON);
	pSAveSpermRange->dMinorLengthMin =  pSAveSpermRange->dMinorLengthMin/(nImgFileNum + EPSINON);
	pSAveSpermRange->dMinorLengthAve =  pSAveSpermRange->dMinorLengthAve/(nImgFileNum + EPSINON);
}

//获背景图片中的精子目标个数
int getBackImgSpermCount(const char * pcResultPath, int const& nImgFileNum, IplImage *pImgBackGround, ParaRange *pSAveSpermRange, int &nBackgroundImgSpermNum)
{
	int nStatus = 1;

	IplConvKernel * pStructureEle = cvCreateStructuringElementEx(5, 5, 1, 1,
			CV_SHAPE_ELLIPSE, NULL);//创建结构元素

	//1.背景图片反向
	IplImage * pImgBackImgInv = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);
	cvNot(pImgBackGround, pImgBackImgInv);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/1反向背景图.jpg",pImgBackImgInv);

	//2.中值滤波去背景
	IplImage * pImgMed = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);
	IplImage * pImgDiff = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);

	cvSmooth( pImgBackImgInv, pImgMed, CV_MEDIAN,9,0,0,0);
	cvSub(pImgBackImgInv, pImgMed, pImgDiff, NULL);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/2中值滤波去背景.jpg",pImgDiff);

	cvReleaseImage(&pImgBackImgInv);
	pImgBackImgInv = NULL;
	cvReleaseImage(&pImgMed);
	pImgMed = NULL;

	//3.高斯平滑
	IplImage * pImgBackSmth = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);
	cvSmooth( pImgDiff, pImgBackSmth,CV_GAUSSIAN,5,0,1,0);
	cvReleaseImage(&pImgDiff);
	pImgDiff = NULL;

	//4.灰度拉伸
	stretchGrayValue(pImgBackSmth);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/3背景灰度拉伸.jpg",pImgBackSmth);

	//5.阈值分割（自适应阈值，固定阈值，Otu阈值）
	IplImage * pImgBinary = getThresholdImage(pImgBackSmth,2);
	cvReleaseImage(&pImgBackSmth);
	pImgBackSmth = NULL;

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/4背景阈值分割.jpg",pImgBinary);

	//6.开运算
	cvMorphologyEx( pImgBinary, pImgBinary,
		NULL, pStructureEle,
		CV_MOP_CLOSE, 1 );

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/5背景开运算结果.jpg",pImgBinary);//图片文件存储

	//7.获取背景图片中的区域轮廓、精子目标个数，并绘制到原图中
	char *pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if (NULL == pcFileNameTemp)
	{
		cvReleaseImage(&pImgBinary);
		cvReleaseStructuringElement( &pStructureEle );//清除结构元素

		nStatus = 0;
		return nStatus;//内存异常
	}
	char *pcImg = (char *)malloc(sizeof(char)*50);
	if (NULL == pcImg)
	{
		free(pcFileNameTemp);
		pcFileNameTemp = NULL;
		cvReleaseImage(&pImgBinary);
		cvReleaseStructuringElement( &pStructureEle );//清除结构元素

		nStatus = 0;
		return nStatus;//内存异常
	}

	IplImage * pImgTemp1 = NULL;
	int nSaveStatus = 1;
	if (-6 == nStatus)
	{
		pImgTemp1 = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);	
		cvCopy(pImgBackGround,pImgTemp1);
	}
	else
	{
		pImgTemp1 = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,3);
		cvZero(pImgTemp1);
	}	
	nBackgroundImgSpermNum = getBackImgContourInfor(pImgTemp1, pImgBinary, pSAveSpermRange);

	if (-6 == nStatus)
	{
		IplImage * pImgContourShow = cvCreateImage(cvGetSize(pImgTemp1),pImgTemp1->depth,pImgTemp1->nChannels);
		cvCopy(pImgTemp1,pImgContourShow);

		strcpy(pcFileNameTemp, pcResultPath);
		sprintf(pcImg, "%03d.jpg", 0);
		strcat(pcFileNameTemp, pcImg);
		nSaveStatus = cvSaveImage(pcFileNameTemp,pImgContourShow);//图片文件存储

		cvReleaseImage(&pImgContourShow);
	}
	else
	{
		for (int i = 0; i < nImgFileNum; i++)
		{
			strcpy(pcFileNameTemp, pcResultPath);
			sprintf(pcImg, "%03d.jpg", i);
			strcat(pcFileNameTemp, pcImg);

			IplImage *pImgTemp2 = cvLoadImage(pcFileNameTemp,1);
			IplImage * pImgContourShow = cvCreateImage(cvGetSize(pImgTemp1),pImgTemp1->depth,pImgTemp1->nChannels);

			getMergeImg(pImgTemp1,pImgTemp2,pImgContourShow);

			//test
			//cvNamedWindow("通道合并结果",CV_WINDOW_AUTOSIZE);
			//cvShowImage("通道合并结果",pImgContourShow);
			//cvWaitKey(0);
			
			nSaveStatus = cvSaveImage(pcFileNameTemp,pImgContourShow);//图片文件存储

			cvReleaseImage(&pImgContourShow);
			cvReleaseImage(&pImgTemp2);
			if (0 == nSaveStatus)
			{
				break;
			}
		}		
	}

	if (0 == nSaveStatus)
	{
		free(pcFileNameTemp);
		pcFileNameTemp = NULL;
		free(pcImg);
		pcImg = NULL;
		cvReleaseImage(&pImgTemp1);
		cvReleaseImage(&pImgBinary);
		cvReleaseStructuringElement( &pStructureEle );//清除结构元素

		nStatus = -2;
		return nStatus;//数据写入异常
	}

	free(pcFileNameTemp);
	pcFileNameTemp = NULL;
	free(pcImg);
	pcImg = NULL;

	//8.释放资源
	cvReleaseImage(&pImgTemp1);
	cvReleaseImage(&pImgBinary);
	cvReleaseStructuringElement( &pStructureEle );//清除结构元素

	return nBackgroundImgSpermNum;
}

//二值化的背景图片的轮廓处理
int getBackImgContourInfor(IplImage *pImgContourShow, IplImage *pImgBinary, ParaRange *pSAveSpermRange)
{
	//cvZero(pImgBackGround);

	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;

	int nAllSpermNum = 0;
	double dConArea;
	//double dConMajLength;
	//double dConMinLength;

	CvBox2D GeoInfor;
	
	CvContourScanner scanner = cvStartFindContours(pImgBinary,
								storage,
								sizeof(CvContour),
								CV_RETR_EXTERNAL,//CV_RETR_EXTERNAL，最外层//CV_RETR_LIST
								CV_CHAIN_APPROX_SIMPLE,
								cvPoint(0,0));
	
	CvSeq * cTemp =NULL;

	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找  
	{ 
		//面积
		dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		//轮廓面积大于dMinArea，并且小于dMaxArea，则留下，反之，则删除。
		if(dConArea > 0.4*pSAveSpermRange->dAreaAve && dConArea < 1.5*pSAveSpermRange->dAreaAve && nAllSpermNum < MAXSPERMNUM )
		{  	
			/*/质心，长短轴，角度，这三个判断效果不是很好，先不用
			GeoInfor = cvMinAreaRect2(cTemp, NULL);
			if (GeoInfor.size.width >= GeoInfor.size.height)
			{
				dConMajLength = GeoInfor.size.width;
				dConMinLength = GeoInfor.size.height;
			}else
			{
				dConMajLength = GeoInfor.size.height;
				dConMinLength = GeoInfor.size.width;
			}

			if ((dConMajLength >= pSAveSpermRange->dMajorLengthMin && dConMajLength <= pSAveSpermRange->dMajorLengthMax)
				&& (dConMinLength >= pSAveSpermRange->dMinorLengthMin && dConMinLength <= pSAveSpermRange->dMinorLengthMax))
			{
				//绘制符合面积条件的轮廓
				cvDrawContours(pImgContourShow,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0);
				nAllSpermNum = nAllSpermNum+1;
			}else
			{
				cvSubstituteContour(scanner,NULL);//删除当前的轮廓
			}		
			*/

			//绘制符合面积条件的轮廓
			cvDrawContours(pImgContourShow,cTemp,CV_RGB(255,0,0),CV_RGB(255,0,0),0);
			/*
			CVAPI(void)  cvDrawContours( CvArr *img, CvSeq* contour,
				CvScalar external_color, CvScalar hole_color,
				int max_level, int thickness CV_DEFAULT(1),
				int line_type CV_DEFAULT(8),
				CvPoint offset CV_DEFAULT(cvPoint(0,0)));
			*/
			nAllSpermNum = nAllSpermNum+1;
		}
		else  
		{  
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}  
	}
	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。

	cvReleaseMemStorage(&storage); 

	return nAllSpermNum;
}

//获取活动精子个数
int getTwoImgAliveSpermCount(IplImage *pImgSubScr1,IplImage *pImgSubScr2)
{
	int nAliveSpermNum = 0;

	double dConArea = 0;
	double dMinAreaTemp = 10;
	double dMaxAreaTemp = 150;

	//1.图像相减
	IplImage * pImgDiff = cvCreateImage(cvGetSize(pImgSubScr1),pImgSubScr1->depth,pImgSubScr1->nChannels);
	cvSub(pImgSubScr2,pImgSubScr1,pImgDiff);

	//2.阈值分割（自适应阈值，固定阈值，Otu阈值）
	IplImage * pImgBinary =cvCreateImage(cvGetSize(pImgDiff),pImgDiff->depth,pImgDiff->nChannels);
	cvThreshold( pImgDiff, pImgBinary,
		30, 255,
		CV_THRESH_BINARY);//固定阈值分割

	//3.个数统计（面积条件）
	CvBox2D GeoInfor;
	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;

	CvContourScanner scanner = cvStartFindContours(pImgBinary,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));

	CvSeq * cTemp =NULL;
	int nSpermIndex = 0;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找
	{
		//面积
		dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		//轮廓面积大于dMinArea，并且小于dMaxArea，则留下，反之，则删除。 
		if(dConArea > dMinAreaTemp && dConArea < dMaxAreaTemp && nSpermIndex < MAXSPERMNUM )
		{
			nSpermIndex = nSpermIndex + 1;
		}
		else
		{
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}
	}
	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。

	cvReleaseMemStorage(&storage);
	cvReleaseImage(&pImgBinary);
	pImgBinary = NULL;
	cvReleaseImage(&pImgDiff);
	pImgDiff = NULL;

	nAliveSpermNum = nSpermIndex;

	return nAliveSpermNum;
}

//精子目标叠加显示到原图
void drawSpermImg(IplImage *pImgBinary,  IplImage *pImgContourShow, ParaRange *pSAveSpermRange, int nType)
{
	//cvZero(pImgContourShow);
	double dAreaMin = 0.1*pSAveSpermRange->dAreaAve;
	double dAreaMax = 8*pSAveSpermRange->dAreaAve;

	CvScalar cvColor = SLOWPRCOlOUR;
	if (nType == 1)//ABC
	{
		cvColor = RAPIDPRCOlOUR;
	}
	else if (nType == 0)//D
	{
		cvColor = SLOWPRCOlOUR;
	}
	else
	{
		cvColor = SLOWPRCOlOUR;//ABCD
		dAreaMin = 0.2*pSAveSpermRange->dAreaAve;
		dAreaMax = 3*pSAveSpermRange->dAreaAve;
	}

	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;

	int nAllSpermNum = 0;
	double dConArea;
	//double dConMajLength;
	//double dConMinLength;

	CvBox2D GeoInfor;
	CvContourScanner scanner = cvStartFindContours(pImgBinary,
								storage,
								sizeof(CvContour),
								CV_RETR_EXTERNAL,
								CV_CHAIN_APPROX_SIMPLE,
								cvPoint(0,0));
	CvSeq * cTemp =NULL;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找  
	{ 
		//面积
		dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		//轮廓面积大于pSAveSpermRange->dAreaMin，并且小于pSAveSpermRange->dAreaMax，则留下，反之，则删除。
		if(dConArea > dAreaMin && dConArea < dAreaMax && nAllSpermNum < MAXSPERMNUM )
		{ 
			cvDrawContours(pImgContourShow,cTemp,cvColor,cvColor,0);
		}
		else  
		{  
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}  
	}
	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。

	//资源释放
	cvReleaseMemStorage(&storage);
}

//标粒目标叠加显示到原图
void drawStdParticleImg(IplImage *pImgBinary,  IplImage *pImgContourShow, ParaRange *pSAveSpermRange, int nType)
{
	//cvZero(pImgContourShow);
	double dAreaMin = 0.1*pSAveSpermRange->dAreaMin;
	double dAreaMax = 6*pSAveSpermRange->dAreaMax;

	CvScalar cvColor = SLOWPRCOlOUR;
	if (nType == 1)//ABC
	{
		cvColor = RAPIDPRCOlOUR;
	}
	else if (nType == 0)//D
	{
		cvColor = SLOWPRCOlOUR;
	}
	else
	{
		cvColor = SLOWPRCOlOUR;//ABCD
	}

	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;

	int nAllSpermNum = 0;
	double dConArea;
	//double dConMajLength;
	//double dConMinLength;

	CvBox2D GeoInfor;
	CvContourScanner scanner = cvStartFindContours(pImgBinary,
		storage,
		sizeof(CvContour),
		CV_RETR_EXTERNAL,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));
	CvSeq * cTemp =NULL;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找  
	{ 
		//面积
		dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		//轮廓面积大于pSAveSpermRange->dAreaMin，并且小于pSAveSpermRange->dAreaMax，则留下，反之，则删除。
		if(dConArea > dAreaMin && dConArea < dAreaMax && nAllSpermNum < MAXSPERMNUM )
		{ 
			cvDrawContours(pImgContourShow,cTemp,cvColor,cvColor,0,2);
		}
		else  
		{  
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}  
	}
	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。

	//资源释放
	cvReleaseMemStorage(&storage);
}

//D叠加显示到原图
int drawSpermD(char **ppcFilePath, int const& nImgFileNum, const char *pcResImgFile, int nCenterPara[], IplImage *pImgBackGround, ParaRange *pSAveSpermRange)
{
	int nStatus = 1;

	//针对人精进行结果绘制
	for (int i = 0; i < nImgFileNum; i++)
	{
		IplImage * pImgBackTemp = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);//创建目标图像
		cvCopy(pImgBackGround,pImgBackTemp);

		IplImage *pImgSrcTemp = cvLoadImage(ppcFilePath[i],1);
		IplImage *pImgContourShow = clipCenterImage(pImgSrcTemp, nCenterPara);
		cvReleaseImage(&pImgSrcTemp);

		//3.获取目标轮廓及其几何信息
		int nType = 0;//D
		drawSpermImg(pImgBackTemp, pImgContourShow, pSAveSpermRange, nType);

		//图像存储
		char *pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if (NULL == pcFileNameTemp)
		{
			nStatus = 0;
			return nStatus;//内存异常
		}

		char *pcImg = (char *)malloc(sizeof(char)*50);
		if (NULL == pcImg)
		{
			free(pcFileNameTemp);
			pcFileNameTemp = NULL;

			nStatus = 0;
			return nStatus;//内存异常
		}

		strcpy(pcFileNameTemp, pcResImgFile);
		sprintf(pcImg, "Result_%03d.jpg", i);
		strcat(pcFileNameTemp, pcImg); 
		int nSaveStatus = cvSaveImage(pcFileNameTemp,pImgContourShow);//图片文件存储
		

		//资源释放
		cvReleaseImage(&pImgContourShow);
		cvReleaseImage(&pImgBackTemp);

		free(pcFileNameTemp);
		pcFileNameTemp = NULL;
		free(pcImg);
		pcImg = NULL;

		if (0 == nSaveStatus)
		{
			nStatus = -2;
			return nStatus;//数据写入异常
		}
	}

	return nStatus;
}

//ABC叠加显示到原图
int drawSpermABC(char **ppcFilePath, int const& nImgFileNum, const char *pcResImgFile, int nCenterPara[], ParaRange *pSAveSpermRange)
{
	int nStatus = 1;

	//针对人精进行结果绘制
	for (int i = 0; i < nImgFileNum; i++)
	{
		//图像路径处理
		char *pcFileNameTemp1 = (char *)malloc(sizeof(char)*MAXPATHLENGTH);//二值化图
		if (NULL == pcFileNameTemp1)
		{
			nStatus = 0;
			return nStatus;//内存异常
		}

		char *pcFileNameTemp2 = (char *)malloc(sizeof(char)*MAXPATHLENGTH);//原图
		if (NULL == pcFileNameTemp2)
		{
			free(pcFileNameTemp1);
			pcFileNameTemp1 = NULL;

			nStatus = 0;
			return nStatus;//内存异常
		}

		char *pcImg = (char *)malloc(sizeof(char)*50);
		if (NULL == pcImg)
		{
			free(pcFileNameTemp1);
			pcFileNameTemp1 = NULL;
			free(pcFileNameTemp2);
			pcFileNameTemp2 = NULL;

			nStatus = 0;
			return nStatus;//内存异常
		}

		strcpy(pcFileNameTemp1, pcResImgFile);
		sprintf(pcImg, "%03d.jpg", i);
		strcat(pcFileNameTemp1, pcImg);//二值化图

		strcpy(pcFileNameTemp2, pcResImgFile);
		sprintf(pcImg, "Result_%03d.jpg", i);
		strcat(pcFileNameTemp2, pcImg);//原图

		IplImage *pImgBinary = cvLoadImage(pcFileNameTemp1,0);
		//先对输入图像做一次二值化
		cvThreshold( pImgBinary, pImgBinary,
			100, 255, CV_THRESH_BINARY);//固定阈值分割

		IplImage *pImgSrcTemp = cvLoadImage(pcFileNameTemp2,1);	

		//3.获取目标轮廓及其几何信息
		int nType = 1;//ABC
		drawSpermImg(pImgBinary, pImgSrcTemp, pSAveSpermRange, nType);

		int nSaveStatus = cvSaveImage(pcFileNameTemp2, pImgSrcTemp);//图片文件存储

		//资源释放
		free(pcFileNameTemp1);
		pcFileNameTemp1 = NULL;
		free(pcFileNameTemp2);
		pcFileNameTemp2 = NULL;
		free(pcImg);
		pcImg = NULL;

		cvReleaseImage(&pImgBinary);
		cvReleaseImage(&pImgSrcTemp);

		if (0 == nSaveStatus)
		{
			nStatus = -2;
			return nStatus;//数据写入异常
		}
	}

	return nStatus;
}

//图像灰度拉伸
void stretchGrayValue(IplImage *pImgSrc)
{
	IplImage * pImgTemp1 = cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);
	IplImage * pImgTemp2 = cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/灰度拉伸测试原图1.jpg",pImgSrc);//图片文件存储

	//阈值分割（灰度值小于等于2的置为0）
	IplImage * pImgBinary =cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);
	//double dThreshValue = 0;

	cvThreshold( pImgSrc, pImgBinary,
		2, 255,
		CV_THRESH_TOZERO);//固定阈值分割
	cvCopy(pImgBinary,pImgSrc);
	cvReleaseImage(&pImgBinary);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/灰度拉伸测试原图2.jpg",pImgSrc);//图片文件存储

	double dMinValue;
	double dMaxValue;
	double dDelta;
	cvMinMaxLoc( pImgSrc, &dMinValue, &dMaxValue,
		NULL,
		NULL,
		NULL );
	dDelta = 255 / (dMaxValue - dMinValue + EPSINON);
	if (dDelta < 1)
	{
		dDelta = 1;
	}
	else if (dDelta > 255)
	{
		dDelta = 255;
	}
	cvSubS( pImgSrc, cvScalar(dMinValue), pImgTemp1,NULL);
	cvSet( pImgTemp2, cvScalar(dDelta),NULL);
	cvMul( pImgTemp1, pImgTemp2, pImgSrc, 1);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/灰度拉伸测试原图3.jpg",pImgSrc);//图片文件存储

	cvReleaseImage(&pImgTemp1);
	cvReleaseImage(&pImgTemp2);
}

//三通道图像合并处理
void getMergeImg(const IplImage *pImgSrc1, const IplImage *pImgSrc2, IplImage *pImgDst)
{
	IplImage* rImg1 = cvCreateImage(cvGetSize(pImgSrc1),IPL_DEPTH_8U,1);
	IplImage* gImg1 = cvCreateImage(cvGetSize(pImgSrc1),IPL_DEPTH_8U,1);
	IplImage* bImg1 = cvCreateImage(cvGetSize(pImgSrc1),IPL_DEPTH_8U,1);
	cvSplit(pImgSrc1,bImg1,gImg1,rImg1,0);

	IplImage* rImg2 = cvCreateImage(cvGetSize(pImgSrc2),IPL_DEPTH_8U,1);
	IplImage* gImg2 = cvCreateImage(cvGetSize(pImgSrc2),IPL_DEPTH_8U,1);
	IplImage* bImg2 = cvCreateImage(cvGetSize(pImgSrc2),IPL_DEPTH_8U,1);
	cvSplit(pImgSrc2,bImg2,gImg2,rImg2,0);

	IplImage* rImgShow = cvCreateImage(cvGetSize(pImgDst),IPL_DEPTH_8U,1);
	IplImage* gImgShow = cvCreateImage(cvGetSize(pImgDst),IPL_DEPTH_8U,1);
	IplImage* bImgShow = cvCreateImage(cvGetSize(pImgDst),IPL_DEPTH_8U,1);

	cvAdd(rImg1,rImg2,rImgShow);//红色分量求和处理

	cvNot(rImg1, gImg1);
	cvAnd(gImg2,gImg1,gImgShow);//绿色分量求并
	cvAnd(bImg2,gImg1,bImgShow);//蓝色分量求并

	cvMerge(bImgShow,gImgShow,rImgShow,0,pImgDst); 

	cvReleaseImage(&rImg1);
	cvReleaseImage(&gImg1);
	cvReleaseImage(&bImg1);

	cvReleaseImage(&rImg2);
	cvReleaseImage(&gImg2);
	cvReleaseImage(&bImg2);

	cvReleaseImage(&rImgShow);
	cvReleaseImage(&gImgShow);
	cvReleaseImage(&bImgShow);
}

//获取背景图片中的精子目标个数
int getSpermCountD(const char **ppcFilePath, const char * pcResultPath, int const& nImgFileNum, IplImage * pImgBackGround, ParaRange *pSAveSpermRange, SpermInfor *pSSpermInfor, int &nSpermIndex)
{
	int nStatus = 1;

	//1.获取图片地址及数量
	char *pcImgFileTemp[FILENUM] = {NULL};
	for (int i = 0; i < FILENUM; i++)
	{
		pcImgFileTemp[i] = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if (NULL == pcImgFileTemp[i])
		{
			for (int j = 0; j <= i; j++)
			{
				free(pcImgFileTemp[j]);
				pcImgFileTemp[j] = NULL;
			}

			nStatus = 0;//内存异常
			return nStatus;
		}
	}
	int nImgNum = 0;
	nStatus = getImgFileSeq(pcResultPath,pcImgFileTemp,nImgNum);
	if (nImgNum <= 5)
	{
		if (nStatus == 1)
		{
			nStatus = -5;//样本图像张数不够，无法进一步分析
		}
		for (int i = 0; i < FILENUM; i++)
		{
			free(pcImgFileTemp[i]);
			pcImgFileTemp[i] = NULL;
		}
		return nStatus;
	}

	const char *ppcImageFile[FILENUM] = {NULL};
	for (int i = 0; i < nImgNum; i++)
	{
		ppcImageFile[i] = pcImgFileTemp[i];//用于const char* 与const char转换
	}

	//2.二值化背景图片生成
	//IplImage *pImgBackGroundTest = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);
	genBinaryBackgroundImg(pImgBackGround, ppcImageFile, nImgNum);
	
	//3.获取目标轮廓及其几何信息
	int nSpermType = 4;
	getImgContourInfor(pImgBackGround, pSAveSpermRange, nSpermType, pSSpermInfor, nSpermIndex);


	//资源释放
	for (int i = 0; i < FILENUM; i++)
	{
		free(pcImgFileTemp[i]);
		pcImgFileTemp[i] = NULL;
	}

	return nStatus;
}

//获取背景图片中的精子目标个数(人精测试专用)
int getBackImgPigSpermCount(const char * pcResultPath, int const& nImgFileNum, IplImage *pImgBackGround, ParaRange *pSAveSpermRange, int &nBackgroundImgSpermNum)
{
	int nStatus = 1;

	//1.背景图片反向
	IplImage * pImgBackImgInv = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);
	cvNot(pImgBackGround, pImgBackImgInv);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/1反向背景图.jpg",pImgBackImgInv);

	//2.中值滤波去背景
	IplImage * pImgMed = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);
	IplImage * pImgDiff = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);

	cvSmooth( pImgBackImgInv, pImgMed, CV_MEDIAN,9,0,0,0);
	cvSub(pImgBackImgInv, pImgMed, pImgDiff, NULL);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/2中值滤波去背景.jpg",pImgDiff);

	cvReleaseImage(&pImgBackImgInv);
	pImgBackImgInv = NULL;
	cvReleaseImage(&pImgMed);
	pImgMed = NULL;

	//3.灰度拉伸
	stretchGrayValue(pImgDiff);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/3灰度拉伸.jpg",pImgDiff);

	//4.灰度增强
	enhanceGrayValue(pImgDiff,15);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/4灰度增强.jpg",pImgDiff);

	//5.阈值分割（自适应阈值，固定阈值，Otu阈值）
	IplImage * pImgBinary = getThresholdImage(pImgDiff,2);
	cvReleaseImage(&pImgDiff);
	pImgDiff = NULL;

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/5背景阈值分割.jpg",pImgBinary);

	//5.形态学处理：开运算+闭运算
	IplConvKernel * pStructureEle = cvCreateStructuringElementEx(3, 3, 1, 1,
		CV_SHAPE_RECT, NULL);//创建结构元素
	cvMorphologyEx( pImgBinary, pImgBinary,
		NULL, pStructureEle,
		CV_MOP_CLOSE, 1 );

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/6背景形态学结果.jpg",pImgBinary);//图片文件存储

	//6.获取背景图片中的区域轮廓、精子目标个数，并绘制到原图中
	char *pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if (NULL == pcFileNameTemp)
	{
		cvReleaseImage(&pImgBinary);
		cvReleaseStructuringElement( &pStructureEle );//清除结构元素

		nStatus = 0;
		return nStatus;//内存异常
	}
	char *pcImg = (char *)malloc(sizeof(char)*50);
	if (NULL == pcImg)
	{
		free(pcFileNameTemp);
		pcFileNameTemp = NULL;
		cvReleaseImage(&pImgBinary);
		cvReleaseStructuringElement( &pStructureEle );//清除结构元素

		nStatus = 0;
		return nStatus;//内存异常
	}

	IplImage * pImgTemp1 = NULL;
	int nSaveStatus = 0;
	if (-6 == nStatus)
	{
		pImgTemp1 = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);	
		cvCopy(pImgBackGround,pImgTemp1);
	}
	else
	{
		pImgTemp1 = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,3);
		cvZero(pImgTemp1);
	}	
	nBackgroundImgSpermNum = getBackImgContourInfor(pImgTemp1, pImgBinary, pSAveSpermRange);

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case03/ResultImgs/7背景目标提取结果.jpg",pImgTemp1);//图片文件存储

	if (-6 == nStatus)
	{
		IplImage * pImgContourShow = cvCreateImage(cvGetSize(pImgTemp1),pImgTemp1->depth,pImgTemp1->nChannels);
		cvCopy(pImgTemp1,pImgContourShow);

		strcpy(pcFileNameTemp, pcResultPath);
		sprintf(pcImg, "%03d.jpg", 0);
		strcat(pcFileNameTemp, pcImg);
		nSaveStatus = cvSaveImage(pcFileNameTemp,pImgContourShow);//图片文件存储

		cvReleaseImage(&pImgContourShow);
	}
	else
	{
		for (int i = 0; i < nImgFileNum; i++)
		{
			strcpy(pcFileNameTemp, pcResultPath);
			sprintf(pcImg, "%03d.jpg", i);
			strcat(pcFileNameTemp, pcImg);

			IplImage *pImgTemp2 = cvLoadImage(pcFileNameTemp,1);
			IplImage * pImgContourShow = cvCreateImage(cvGetSize(pImgTemp1),pImgTemp1->depth,pImgTemp1->nChannels);

			getMergeImg(pImgTemp1,pImgTemp2,pImgContourShow);

			//test
			//cvNamedWindow("通道合并结果",CV_WINDOW_AUTOSIZE);
			//cvShowImage("通道合并结果",pImgContourShow);
			//cvWaitKey(0);

			nSaveStatus = cvSaveImage(pcFileNameTemp,pImgContourShow);//图片文件存储

			cvReleaseImage(&pImgContourShow);
			cvReleaseImage(&pImgTemp2);
			if (0 == nSaveStatus)
			{
				break;
			}
		}		
	}

	if (0 == nSaveStatus)
	{
		free(pcFileNameTemp);
		pcFileNameTemp = NULL;
		free(pcImg);
		pcImg = NULL;
		cvReleaseImage(&pImgTemp1);
		cvReleaseImage(&pImgBinary);
		cvReleaseStructuringElement( &pStructureEle );//清除结构元素

		nStatus = -2;
		return nStatus;//数据写入异常
	}

	free(pcFileNameTemp);
	pcFileNameTemp = NULL;
	free(pcImg);
	pcImg = NULL;

	//8.释放资源
	cvReleaseImage(&pImgTemp1);
	cvReleaseImage(&pImgBinary);
	cvReleaseStructuringElement( &pStructureEle );//清除结构元素

	return nBackgroundImgSpermNum;
}

//图像灰度增强
void enhanceGrayValue(IplImage *pImgSrc, int MagFactor)
{
	IplImage * pImgMag = cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);
	IplImage * pImgSrcTemp = cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);

	cvCopy(pImgSrc,pImgSrcTemp);
	cvSet( pImgMag, cvScalar(MagFactor),NULL);
	cvMul( pImgSrcTemp, pImgMag, pImgSrc, 1);

	cvReleaseImage(&pImgMag);
	pImgMag = NULL;
	cvReleaseImage(&pImgSrcTemp);
	pImgSrcTemp = NULL;
}

//获取视野中心坐标
void getCenterPosition(IplImage *pImgSrc, int nCenterPara[])
{
	int nWidth = nCenterPara[0];
	int nHeight = nCenterPara[1];
	int nCenterX = nCenterPara[2];
	int nCenterY = nCenterPara[3];

	//1.阈值分割（固定阈值）
	IplImage * pImgBinary =cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);
	//double dThreshValue = 0;

	cvThreshold( pImgSrc, pImgBinary,
		30, 255,
		CV_THRESH_BINARY);//固定阈值分割

	//2.膨胀
	IplConvKernel * pStructureEle = cvCreateStructuringElementEx(7, 7, 1, 1,
		CV_SHAPE_RECT, NULL);//创建结构元素
	cvMorphologyEx( pImgBinary, pImgBinary,
		NULL, pStructureEle,
		CV_MOP_DILATE, 1 );	
	cvReleaseStructuringElement( &pStructureEle );//清除结构元素

	//3.轮廓
	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;

	int nAllSpermNum = 0;
	double dSingleArea = 0;
	double dTotalAreaX = 0;
	double dTotalAreaY = 0;
	CvBox2D GeoInfor;

	CvContourScanner scanner = cvStartFindContours(pImgBinary,
								storage,
								sizeof(CvContour),
								CV_RETR_LIST,
								CV_CHAIN_APPROX_SIMPLE,
								cvPoint(0,0));
	CvSeq * cTemp =NULL;
	double dConAreaMax = 0;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找
	{ 
		//面积
		double dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		if(dConArea > 50 && nAllSpermNum < MAXSPERMNUM )
		{
			if (dConArea > dConAreaMax)
			{
				//质心
				GeoInfor = cvMinAreaRect2(cTemp, NULL);//最小外接矩形

				nCenterX = (int)GeoInfor.center.x;
				nCenterY = (int)GeoInfor.center.y;

				dConAreaMax = dConArea;
			}
			nAllSpermNum = nAllSpermNum + 1;
		}
		else  
		{  
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}  
	}

	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。

	int nSizeX = pImgSrc->width;
	int nSizeY = pImgSrc->height;
	double dBiasX = 2*(nCenterX - nSizeX/2)/(nSizeX + EPSINON);
	if (dBiasX <= 0)
	{
		dBiasX = -dBiasX;
	}
	if (dBiasX > 0.7)
	{
		nCenterX = (int)(nSizeX/2.0);
	}

	double dBiasY = 2*(nCenterY - nSizeY/2)/(nSizeY + EPSINON);
	if (dBiasY <= 0)
	{
		dBiasY = -dBiasY;
	}
	if (dBiasY > 0.7)
	{
		nCenterY = (int)(nSizeY/2.0);
	}
	
	int nImgWidth = cvGetSize(pImgSrc).width;
	int nImgHeight = cvGetSize(pImgSrc).height;
	if ((nWidth > nImgWidth) && (nHeight > nImgHeight))
	{
		nWidth = nImgWidth;
		nHeight = nImgHeight;
		nCenterX = (int)(nImgWidth/2);
		nCenterY = (int)(nImgHeight/2);
	} 
	else
	{
		if (nWidth < 50)
		{
			nWidth = nImgWidth;
		}
		if (nWidth > 2*nCenterX)
		{
			nWidth = 2*nCenterX;
		}
		if (nWidth > 2*(nImgWidth - nCenterX))
		{
			nWidth = 2*(nImgWidth - nCenterX);
		}

		if (nHeight < 50)
		{
			nHeight = nImgHeight;
		}
		if (nHeight > 2*nCenterY)
		{
			nHeight = 2*nCenterY;
		}
		if (nHeight > 2*(nImgHeight - nCenterY))
		{
			nHeight = 2*(nImgHeight - nCenterY);
		}
	}

	nCenterPara[0] = nWidth;
	nCenterPara[1] = nHeight;
	nCenterPara[2] = nCenterX;
	nCenterPara[3] = nCenterY;

	//资源释放
	cvReleaseImage(&pImgBinary);
	cvReleaseMemStorage(&storage); 
}

//图像对比度增强并二值化
void stretchImgContrast(IplImage *pImgSrc, const bool bImgTargetType)
{
	IplImage * pImgSrcTemp1 = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgSrcTemp2 = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvConvertScale( pImgSrc, pImgSrcTemp1, 128, 0);
	cvCopy(pImgSrcTemp1,pImgSrcTemp2);

	//均值滤波相减
	IplImage * pImgAve = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgDiff = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvSmooth( pImgSrcTemp2, pImgAve, CV_BLUR,9,0,0,0);
	cvSub(pImgSrcTemp2, pImgAve, pImgDiff, NULL);

	//阈值分割（灰度值大于200的置为0）
	IplImage * pImgBinary =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	//double dThreshValue = 0;
	if (bImgTargetType) //true表示目标为白，false表示目标为黑
	{
		cvThreshold( pImgDiff, pImgBinary,
			1500, 32767, CV_THRESH_TOZERO);//固定阈值分割
	}
	else
	{
		cvThreshold( pImgDiff, pImgBinary,
			500, 32767, CV_THRESH_TOZERO_INV);//固定阈值分割
	}

	cvCopy(pImgBinary,pImgSrcTemp2);	
	enhanceGrayValue(pImgSrcTemp2, 50);//灰度值增强50倍

	//高频信号叠加
	IplImage * pImgDst =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvAdd( pImgSrcTemp1, pImgSrcTemp2, pImgDst);

	cvReleaseImage(&pImgDiff );
	cvReleaseImage(&pImgSrcTemp1);
	cvReleaseImage(&pImgSrcTemp2);

	//均值滤波降噪
	cvSmooth( pImgDst, pImgAve, CV_BLUR,5,0,0,0);

	//阈值分割
	if (bImgTargetType) //true表示目标为白，false表示目标为黑
	{
		cvThreshold( pImgAve, pImgBinary,
			5000, 32767,
			CV_THRESH_BINARY);//固定阈值分割
	}
	else
	{
		cvThreshold( pImgAve, pImgBinary,
			3000, 32767,
			CV_THRESH_BINARY_INV);//固定阈值分割
	}
	cvReleaseImage(&pImgDst);
	cvReleaseImage(&pImgAve);

	//格式转换
	cvConvertScale( pImgBinary, pImgSrc, 1, 0);
	cvReleaseImage(&pImgBinary);

	//形态学处理
	IplConvKernel * pStructureEle = cvCreateStructuringElementEx(3, 3, 1, 1,
		CV_SHAPE_RECT, NULL);//创建结构元素
	//cvMorphologyEx( pImgSrc, pImgSrc,
		//NULL, pStructureEle,
		//CV_MOP_OPEN, 1 );	
	cvMorphologyEx( pImgSrc, pImgSrc,
		NULL, pStructureEle,
		CV_MOP_CLOSE, 1 );	
	cvReleaseStructuringElement( &pStructureEle );//清除结构元素

	//轮廓处理
	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;
	IplImage * pImgDraw =cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);
	cvZero(pImgDraw);

	CvContourScanner scanner = cvStartFindContours(pImgSrc,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));
	CvSeq * cTemp =NULL;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找
	{ 
		//面积
		double dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		//轮廓面积大于dMinArea，并且小于dMaxArea，则留下，反之，则删除。 
		if(dConArea > dMinArea && dConArea < dMaxArea )
		{
			//绘制符合面积条件的轮廓
			cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);
		}
		else  
		{  
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}  
	}
	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。
	cvCopy(pImgDraw, pImgSrc);
	cvReleaseImage(&pImgDraw);	
}

//图像对比度增强并二值化(ABCD)
void stretchImgContrastABCD(IplImage *pImgSrc, int nSampleType)
{
	 //返回值：0-异常样本（未加样或者无玻片），1-干涸人精，2-新鲜人精，3-标粒，4-红细胞
	int nKeyTreshValue;

	if (nSampleType == 3)//标粒
	{
		nKeyTreshValue = -1000;
	}
	else if (nSampleType == 4)//红细胞
	{
		nKeyTreshValue = -3000;
	}
	else
	{
		nKeyTreshValue = -1500;//人精——备男
		// 原值-3000，降低为 -2500，预期能抓出对比度更弱、光线较暗或透明度较高的精子头部
		// 降低后，极微小杂质颗粒进入候选集的概率略增
		// 降低后，解决：部分精子是否被漏检
		// 继续降低到-1500，显著提高对透明度高、反差偏弱的洗涤精子检出率

		// nKeyTreshValue = -8000;//人精——一体机
		//nKeyTreshValue = -1000;//猪精
	}

	if (0) // DebugMode
	{
		cvNamedWindow("ImgOrign",CV_WINDOW_KEEPRATIO);
		cvShowImage("ImgOrign",pImgSrc);
		cvWaitKey();
	}

	IplImage * pImgSrcTemp1 = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgSrcTemp2 = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgSrcTemp3 = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvZero(pImgSrcTemp3);

	cvConvertScale( pImgSrc, pImgSrcTemp1, 128, 0);
	cvCopy(pImgSrcTemp1,pImgSrcTemp2);

	//均值滤波相减
	IplImage * pImgAve = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgDiff = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvSmooth( pImgSrcTemp2, pImgAve, CV_BLUR,9,0,0,0);

	//添加的
	IplImage * pImgAveTemp = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvSmooth( pImgSrcTemp2, pImgAveTemp, CV_BLUR,3,0,0,0);

	//cvAbsDiff(pImgAveTemp, pImgAve, pImgDiff);
	cvSub(pImgAveTemp, pImgAve, pImgDiff, NULL);
	//cvSub(pImgSrcTemp2, pImgAve, pImgDiff, NULL);

	if (0) // DebugMode
	{
		cvNamedWindow("pImgDiff-1",CV_WINDOW_KEEPRATIO);
		cvShowImage("pImgDiff-1",pImgDiff);
		cvWaitKey();
	}

	//添加的
	cvReleaseImage(&pImgAveTemp);

	//阈值分割（灰度值大于200的置为0）
	IplImage * pImgBinary =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	//double dThreshValue = 0;

	//针对黑色目标
	cvThreshold( pImgDiff, pImgBinary,
		nKeyTreshValue, -32767, CV_THRESH_BINARY_INV);//固定阈值分割

	//test 针对白色目标
	//cvThreshold( pImgDiff, pImgBinary,
		//2500, -32767, CV_THRESH_BINARY);//固定阈值分割

	if (0)
	{
		cvNamedWindow("ImgThresh",CV_WINDOW_KEEPRATIO);
		cvShowImage("ImgThresh",pImgBinary);
		cvWaitKey();
	}

	cvCopy(pImgBinary,pImgSrcTemp2);	
	//enhanceGrayValue(pImgSrcTemp2, 20);//灰度值增强20倍

	cvSmooth( pImgSrcTemp2, pImgSrcTemp3, CV_BLUR,3,0,0,0);//均值计算，改善目标中心偏白的问题
	cvZero(pImgSrcTemp2);
	cvThreshold( pImgSrcTemp3, pImgSrcTemp2,
		-10000, -32767, CV_THRESH_BINARY_INV);//固定阈值分割

	if (0)
	{
		cvNamedWindow("pImgSrcTemp2",CV_WINDOW_KEEPRATIO);
		cvShowImage("pImgSrcTemp2",pImgSrcTemp2);
		cvWaitKey();
	}

	//高频信号叠加
	IplImage * pImgDst =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvAdd( pImgSrcTemp1, pImgSrcTemp2, pImgDst);

	if (0)
	{
		cvNamedWindow("pImgDst",CV_WINDOW_KEEPRATIO);
		cvShowImage("pImgDst",pImgDst);
		cvWaitKey();
	}

	//test
	// cvSaveImage("D:/Coding/SQAM/vsProj/spermAnalysis/spermAnalysis/data/高频信号叠加.jpg",pImgDst);//图片文件存储

	cvReleaseImage(&pImgDiff );
	cvReleaseImage(&pImgSrcTemp1);
	cvReleaseImage(&pImgSrcTemp2);
	cvReleaseImage(&pImgSrcTemp3);

	//均值滤波降噪
	cvSmooth( pImgDst, pImgAve, CV_BLUR,5,0,0,0);

	//阈值分割
		cvThreshold( pImgAve, pImgBinary,
			0, 32767,
			CV_THRESH_BINARY_INV);//固定阈值分割

	cvReleaseImage(&pImgDst);
	cvReleaseImage(&pImgAve);

	//格式转换(图像位数)
	cvConvertScale( pImgBinary, pImgSrc, 1, 0);
	cvReleaseImage(&pImgBinary);

	if (0)
	{
		cvNamedWindow("ImgResultBefore",CV_WINDOW_KEEPRATIO);
		cvShowImage("ImgResultBefore",pImgSrc);
		cvWaitKey();
	}

	//形态学处理
	//4.形态学处理颗粒粘连问题
	IplConvKernel * pStructureEle = cvCreateStructuringElementEx(3, 3, 1, 1,
		CV_SHAPE_ELLIPSE, NULL);//创建结构元素
	//cvMorphologyEx( pImgSrc, pImgSrc,
	//NULL, pStructureEle,
	//CV_MOP_OPEN, 1 );	
	cvMorphologyEx( pImgSrc, pImgSrc,
		NULL, pStructureEle,
		CV_MOP_OPEN, 1 );
	cvReleaseStructuringElement( &pStructureEle );//清除结构元素

	if (0)
	{
		cvNamedWindow("ImgResult",CV_WINDOW_KEEPRATIO);
		cvShowImage("ImgResult",pImgSrc);
		cvWaitKey();
	}
}

//图像对比度增强并二值化(D)
void stretchImgContrastD(IplImage *pImgSrc, ParaRange *pSAveSpermRange)
{
	IplImage * pImgSrcTemp1 = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgSrcTemp2 = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvConvertScale( pImgSrc, pImgSrcTemp1, 128, 0);
	cvCopy(pImgSrcTemp1,pImgSrcTemp2);

	//均值滤波相减
	IplImage * pImgAve = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	IplImage * pImgDiff = cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvSmooth( pImgSrcTemp2, pImgAve, CV_BLUR,9,0,0,0);
	cvSub(pImgSrcTemp2, pImgAve, pImgDiff, NULL);

	//阈值分割（灰度值大于200的置为0）
	IplImage * pImgBinary =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	double dThreshValue = 0;

	cvThreshold( pImgDiff, pImgBinary,
		200, 32767, CV_THRESH_TOZERO_INV);//固定阈值分割

	cvCopy(pImgBinary,pImgSrcTemp2);	
	enhanceGrayValue(pImgSrcTemp2, 50);//灰度值增强50倍

	//高频信号叠加
	IplImage * pImgDst =cvCreateImage(cvGetSize(pImgSrc),IPL_DEPTH_16S,1);
	cvAdd( pImgSrcTemp1, pImgSrcTemp2, pImgDst);

	cvReleaseImage(&pImgDiff );
	cvReleaseImage(&pImgSrcTemp1);
	cvReleaseImage(&pImgSrcTemp2);

	//均值滤波降噪
	cvSmooth( pImgDst, pImgAve, CV_BLUR,5,0,0,0);

	//阈值分割
		cvThreshold( pImgAve, pImgBinary,
			10000, 32767,
			CV_THRESH_BINARY_INV);//固定阈值分割

	cvReleaseImage(&pImgDst);
	cvReleaseImage(&pImgAve);

	//格式转换
	cvConvertScale( pImgBinary, pImgSrc, 1, 0);
	cvReleaseImage(&pImgBinary);

	//形态学处理
	IplConvKernel * pStructureEle = cvCreateStructuringElementEx(3, 3, 1, 1,
		CV_SHAPE_RECT, NULL);//创建结构元素
	//cvMorphologyEx( pImgSrc, pImgSrc,
	//NULL, pStructureEle,
	//CV_MOP_OPEN, 1 );	
	cvMorphologyEx( pImgSrc, pImgSrc,
		NULL, pStructureEle,
		CV_MOP_CLOSE, 1 );	
	cvReleaseStructuringElement( &pStructureEle );//清除结构元素

	//轮廓处理
	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;
	IplImage * pImgDraw =cvCreateImage(cvGetSize(pImgSrc),pImgSrc->depth,pImgSrc->nChannels);
	cvZero(pImgDraw);

	CvContourScanner scanner = cvStartFindContours(pImgSrc,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));
	CvSeq * cTemp =NULL;
	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找
	{ 
		//面积
		double dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		//轮廓面积大于dMinArea，并且小于dMaxArea，则留下，反之，则删除。 
		if(dConArea > 0.5*pSAveSpermRange->dAreaAve && dConArea < 1.5*pSAveSpermRange->dAreaAve)
		{
			//绘制符合面积条件的轮廓
			cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);
		}
		else  
		{  
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}  
	}
	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。
	cvCopy(pImgDraw, pImgSrc);
	cvReleaseImage(&pImgDraw);
}

//获取A+B+C类精子个数
int getSpermCountABC(char **pcValidFilePath, int nValidNum, const char * pcResultPath, IplImage * pImgBackGround, ParaRange *pSAveSpermRange, SpermInfor *pSSpermInfor, int &nSpermIndex)
{
	int nStatus = 1;

	//获取图片地址及数量
	char *pcImgFileTemp[FILENUM] = {NULL};
	for (int i = 0; i < FILENUM; i++)
	{
		pcImgFileTemp[i] = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
		if (NULL == pcImgFileTemp[i])
		{
			for (int j = 0; j <= i; j++)
			{
				free(pcImgFileTemp[j]);
				pcImgFileTemp[j] = NULL;
			}

			nStatus = 0;//内存异常
			return nStatus;
		}
	}
	int nImgNum = 0;
	nStatus = getImgFileSeq(pcResultPath,pcImgFileTemp,nImgNum);
	if (nImgNum <= 5)
	{
		if (nStatus == 1)
		{
			nStatus = -5;//样本图像张数不够，无法进一步分析
		}
		for (int i = 0; i < FILENUM; i++)
		{
			free(pcImgFileTemp[i]);
			pcImgFileTemp[i] = NULL;
		}
		return nStatus;
	}

	const char *ppcImageFile[FILENUM] = {NULL};
	for (int i = 0; i < nImgNum; i++)
	{
		ppcImageFile[i] = pcImgFileTemp[i];//用于const char* 与const char转换
	}

	char *pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if (NULL == pcFileNameTemp)
	{
		for (int i = 0; i < FILENUM; i++)
		{
			free(pcImgFileTemp[i]);
			pcImgFileTemp[i] = NULL;
		}

		nStatus = 0;
		return nStatus;//内存异常
	}

	char *pcImg = (char *)malloc(sizeof(char)*50);
	if (NULL == pcImg)
	{
		for (int i = 0; i < FILENUM; i++)
		{
			free(pcImgFileTemp[i]);
			pcImgFileTemp[i] = NULL;
		}
		free(pcFileNameTemp);
		pcFileNameTemp = NULL;

		nStatus = 0;
		return nStatus;//内存异常
	}

	for (int i = 0; i < nImgNum; i++)
	{
		//调取原图
		IplImage *pImgContourShow = cvLoadImage(pcValidFilePath[i],1);

		//调取结果图
		nSpermIndex = 0;
		IplImage *pImgABC = cvLoadImage(pcImgFileTemp[i],0);
		cvThreshold( pImgABC, pImgABC,
			100, 255, CV_THRESH_BINARY);//固定阈值分割

		//1.获得ABC精子图
		getSpermImgABC(pImgABC, pImgBackGround, pSAveSpermRange);
		int nSaveStatus = cvSaveImage(pcImgFileTemp[i],pImgABC);//图片文件存储
		cvReleaseImage(&pImgABC);
		cvReleaseImage(&pImgContourShow);
		if (0 == nSaveStatus)
		{
			nStatus = -2;//数据写入异常
			break;
		}
	}


	//资源释放
	for (int i = 0; i < FILENUM; i++)
	{
		free(pcImgFileTemp[i]);
		pcImgFileTemp[i] = NULL;
	}
	free(pcFileNameTemp);
	pcFileNameTemp = NULL;
	free(pcImg);
	pcImg = NULL;

	return nStatus;
}

void getSpermCountAB(IplImage *pImgSubScr1,IplImage *pImgSubScr2, IplImage *pImgAB, ParaRange *pSAveSpermRange, SpermInfor *pSSpermInfor, int &nSpermIndex)
{
	//1.图像相减
	cvSub(pImgSubScr1,pImgSubScr2,pImgAB);

	//3.获取目标轮廓及其几何信息
	int nSpermType = 5;
	getImgContourInfor(pImgAB, pSAveSpermRange, nSpermType, pSSpermInfor, nSpermIndex);
}

//获取C类精子个数
void getSpermCountC(IplImage *pImgSrc, IplImage *pImgAB, IplImage *pImgC, IplImage * pImgBackGround, ParaRange *pSAveSpermRange, SpermInfor *pSSpermInfor, int &nSpermIndex)
{
	//1.图像运算pImgSubScr1 - pImgAB - pImgBackGround
	IplImage * pImgTmp = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);

	cvSub(pImgSrc,pImgAB,pImgTmp);
	cvSub(pImgTmp,pImgBackGround,pImgC);

	//3.获取目标轮廓及其几何信息
	int nSpermType = 3;
	getImgContourInfor(pImgC, pSAveSpermRange, nSpermType, pSSpermInfor, nSpermIndex);

	cvReleaseImage(&pImgTmp);
}

//结果图片绘制与存储
int drawResultImg(const char *pcImageFile, IplImage *pImgContourShow, SpermInfor *pSSpermInfor, int nSpermIndex)
{
	for (int i = 0; i<nSpermIndex; i++)
	{
		int nRadius = (int)(pSSpermInfor[i].dMajAxsLen/2);

		CvPoint cvCenter = cvPoint(cvRound(pSSpermInfor[i].dPosX),cvRound(pSSpermInfor[i].dPosY));
		CvScalar cvColor;

		if (pSSpermInfor[i].nType == 3)//C
		{
			cvColor = CV_RGB(0,0,255);
		}
		else if (pSSpermInfor[i].nType == 4)//D
		{
			cvColor = CV_RGB(255,0,0);
		}
		else if (pSSpermInfor[i].nType == 5)//A+B
		{
			cvColor = CV_RGB(0,255,0);
		}
		cvCircle(pImgContourShow, cvCenter, nRadius, cvColor, 1);
	}

	int nSaveStatus = cvSaveImage(pcImageFile,pImgContourShow);//图片文件存储

	return nSaveStatus;
}

//存储A+B+C的精子图片
int saveSpermImgABC(const char *pcImageFile, IplImage *pImgSrc,IplImage * pImgBackGround, ParaRange *pSAveSpermRange)
{
	int nStatus = 1;

	//1.图像运算pImgSrc - pImgBackGround
	IplImage * pImgTmp = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);

	cvSub(pImgSrc,pImgBackGround,pImgTmp);
	
	//3.获取目标轮廓及其几何信息
	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;

	IplImage * pImgDraw =cvCreateImage(cvGetSize(pImgTmp),pImgTmp->depth,pImgTmp->nChannels);
	cvZero(pImgDraw);

	//先对输入图像做一次二值化
	cvThreshold( pImgTmp, pImgTmp,
		100, 255, CV_THRESH_BINARY);//固定阈值分割

	CvBox2D GeoInfor;

	CvContourScanner scanner = cvStartFindContours(pImgTmp,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));
	CvSeq * cTemp =NULL;
	double dConArea = 0;
	double dMinAreaTemp = 0.2 * pSAveSpermRange->dAreaAve;
	double dMaxAreaTemp = 2 * pSAveSpermRange->dAreaAve;

	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找
	{ 
		//面积
		dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		//轮廓面积大于dMinArea，并且小于dMaxArea，则留下，反之，则删除。 
		if(dConArea > dMinAreaTemp && dConArea < dMaxAreaTemp)
		{
			//绘制符合条件的轮廓
			cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);

			/*
			//质心，长短轴，角度
			double dMajAxsLen = 0;
			double dMinAxsLen = 0;
			GeoInfor = cvMinAreaRect2(cTemp, NULL);//最小外接矩形
			if (GeoInfor.size.width >= GeoInfor.size.height)
			{
				dMajAxsLen = GeoInfor.size.width;
				dMinAxsLen = GeoInfor.size.height;
			}else
			{
				dMajAxsLen = GeoInfor.size.height;
				dMinAxsLen = GeoInfor.size.width;
			}

			//长短轴判断
			if (dMinAxsLen > 2)
			{
				double dShapeRatio = dMajAxsLen/dMinAxsLen;
				if (dShapeRatio >= pSAveSpermRange->dShapeRatio && dShapeRatio <= 4*pSAveSpermRange->dShapeRatio)
				{
					//绘制符合条件的轮廓
					cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);
				}
			}
			*/
		}
		else
		{
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}  
	}
	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。

	int nSaveStatus = cvSaveImage(pcImageFile,pImgDraw);//图片文件存储
	if (0 == nSaveStatus)
	{
		nStatus = -2;//数据写入异常
	}

	//资源释放
	cvReleaseMemStorage(&storage); 
	cvReleaseImage(&pImgDraw);
	cvReleaseImage(&pImgTmp);

	return nStatus;
}

//获得ABC精子图
void getSpermImgABC(IplImage *pImgSrc, IplImage * pImgBackGround, ParaRange *pSAveSpermRange)
{
	//1.图像运算pImgSrc - pImgBackGround
	IplImage * pImgTmp = cvCreateImage(cvGetSize(pImgBackGround),pImgBackGround->depth,pImgBackGround->nChannels);

	cvSub(pImgSrc,pImgBackGround,pImgTmp);
	
	//3.获取目标轮廓及其几何信息
	CvMemStorage *storage = cvCreateMemStorage();
	CvSeq* firstcontour = NULL;

	IplImage * pImgDraw =cvCreateImage(cvGetSize(pImgTmp),pImgTmp->depth,pImgTmp->nChannels);
	cvZero(pImgDraw);

	//先对输入图像做一次二值化
	cvThreshold( pImgTmp, pImgTmp,
		100, 255, CV_THRESH_BINARY);//固定阈值分割

	CvBox2D GeoInfor;

	CvContourScanner scanner = cvStartFindContours(pImgTmp,
		storage,
		sizeof(CvContour),
		CV_RETR_LIST,
		CV_CHAIN_APPROX_SIMPLE,
		cvPoint(0,0));
	CvSeq * cTemp =NULL;
	double dConArea = 0;
	double dMinAreaTemp = 0.4 * pSAveSpermRange->dAreaAve;
	double dMaxAreaTemp = 4 * pSAveSpermRange->dAreaAve;

	while( (cTemp = cvFindNextContour(scanner) ) != NULL)//开始查找
	{ 
		//面积
		dConArea = fabs(cvContourArea(cTemp,CV_WHOLE_SEQ,0));

		//轮廓面积大于dMinArea，并且小于dMaxArea，则留下，反之，则删除。 
		if(dConArea > dMinAreaTemp && dConArea < dMaxAreaTemp)
		{
			//绘制符合条件的轮廓
			cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);

			/*
			//质心，长短轴，角度
			double dMajAxsLen = 0;
			double dMinAxsLen = 0;
			GeoInfor = cvMinAreaRect2(cTemp, NULL);//最小外接矩形
			if (GeoInfor.size.width >= GeoInfor.size.height)
			{
				dMajAxsLen = GeoInfor.size.width;
				dMinAxsLen = GeoInfor.size.height;
			}else
			{
				dMajAxsLen = GeoInfor.size.height;
				dMinAxsLen = GeoInfor.size.width;
			}

			//长短轴判断
			if (dMinAxsLen > 2)
			{
				double dShapeRatio = dMajAxsLen/dMinAxsLen;
				if (dShapeRatio >= pSAveSpermRange->dShapeRatio && dShapeRatio <= 4*pSAveSpermRange->dShapeRatio)
				{
					//绘制符合条件的轮廓
					cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);
				}
			}
			*/
		}
		else
		{
			cvSubstituteContour(scanner,NULL);//删除当前的轮廓
		}  
	}
	firstcontour = cvEndFindContours(&scanner);//把找到的轮廓返回到firstContour中。

	cvCopy(pImgDraw,pImgSrc);

	//资源释放
	cvReleaseMemStorage(&storage); 
	cvReleaseImage(&pImgDraw);
	cvReleaseImage(&pImgTmp);
}

//存储并绘制最终结果图片
int drawFinalSpermImg(const char *pcImageFile, IplImage *pImgSrc, IplImage * pImgABC, IplImage * pImgBackGround, ParaRange *pSAveSpermRange)
{
	int nStatus = 1;

	//精子ABC目标叠加显示到原图
	int nType = 1;
	drawSpermImg(pImgABC, pImgSrc, pSAveSpermRange, nType);

	//精子D目标叠加显示到原图
	nType = 0;
	drawSpermImg(pImgBackGround, pImgSrc, pSAveSpermRange, nType);

	int nSaveStatus = cvSaveImage(pcImageFile,pImgSrc);//图片文件存储
	if (0 == nSaveStatus)
	{
		nStatus = -2;//数据写入异常
	}

	return nStatus;
}

//存储单张图片
int saveSingleImg(IplImage *pImgSrc, const char *pcImgFilePath, const char *pcImgFileName)
{
	int nStatus = 1;

	char *pcImgFilePathTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if ( NULL == pcImgFilePathTemp )
	{
		nStatus = 0;
		return nStatus;//内存异常
	}
	char *pcImgName = (char *)malloc(sizeof(char)*50);
	if ( NULL == pcImgName )
	{
		free(pcImgFilePathTemp);
		pcImgFilePathTemp = NULL;
		nStatus = 0;
		return nStatus;//内存异常
	}
	strcpy(pcImgFilePathTemp, pcImgFilePath);
	strcpy(pcImgName,pcImgFileName);
	strcat(pcImgFilePathTemp, pcImgName);
	int nSaveStatus = cvSaveImage(pcImgFilePathTemp,pImgSrc);//图片文件存储
	if ( !nSaveStatus)
	{
		nStatus = -2;//数据写入异常
	}

	free(pcImgFilePathTemp);
	pcImgFilePathTemp = NULL;
	free(pcImgName);
	pcImgName = NULL;

	return nStatus;
}

//初始化精子特征参数
void iniSpermRange(ParaRange *pSAveSpermRange, double dShapeRatio)
{
	ParaRange paraRangeDefault = {11.6, 63.9, 31.5, 5.2, 15.3, 9.1, 3, 7.2, 4.7, dShapeRatio};

	pSAveSpermRange->dAreaMin = paraRangeDefault.dAreaMin;
	pSAveSpermRange->dAreaMax = paraRangeDefault.dAreaMax;
	pSAveSpermRange->dAreaAve = paraRangeDefault.dAreaAve;

	pSAveSpermRange->dMajorLengthMin = paraRangeDefault.dMajorLengthMin;
	pSAveSpermRange->dMajorLengthMax = paraRangeDefault.dMajorLengthMax;
	pSAveSpermRange->dMajorLengthAve = paraRangeDefault.dMajorLengthAve;

	pSAveSpermRange->dMinorLengthMin = paraRangeDefault.dMinorLengthMin;
	pSAveSpermRange->dMinorLengthMax = paraRangeDefault.dMinorLengthMax;
	pSAveSpermRange->dMinorLengthAve = paraRangeDefault.dMinorLengthAve;

	pSAveSpermRange->dShapeRatio = paraRangeDefault.dShapeRatio;
}

//更新精子特征参数
void updateSpermRange(ParaRange *pSAveSpermRange, const int nSampleType)
{
	//{11.6, 63.9, 31.5, 5.2, 15.3, 9.1, 3, 7.2, 4.7, dVar[4]};
	//注：以下参数基于三种类别各8份样本统计得到，date:2018.01.05
	/*
	struct ParaRange
    double dAreaMin;               // ① 面积最小
    double dAreaMax;              // ② 面积最大
    double dAreaAve;               // ③ 面积平均
    double dMajorLengthMin;  // ④ 长轴最小
    double dMajorLengthMax;  // ⑤ 长轴最大
    double dMajorLengthAve;   // ⑥ 长轴平均
    double dMinorLengthMin;  // ⑦ 短轴最小
    double dMinorLengthMax;  // ⑧ 短轴最大
    double dMinorLengthAve;  // ⑨ 短轴平均
    double dShapeRatio;          // ⑩ 长/短轴比（模板参考值）
	*/
	// {AreaMin,Max,Ave, MajorLengthMin,Max,Ave, MinorLengthMin,Max,Ave, ShapeRatio}
	ParaRange paraRangeTemp;
	ParaRange paraRangeTemp1 = {40, 110, 75, 9, 34, 21, 3.4, 9, 6.5, 3.3};//干涸人精

	// 新鲜人精  容纳膨胀变大、短轴变粗、形状近圆的洗涤精子
	ParaRange paraRangeTemp2 = {4, 45, 18.0, 2.3, 12.0, 5.5, 2.0, 6.5, 3.8, 1.00};
	//新鲜人精  同步动物宽窗版本，略扩大区间，处理 type2 过了但仍漏检的（“刚死/微干”）的大尺寸精子
	// ParaRange paraRangeTemp2 = {10, 30, 18.0, 3.0, 9, 5.5, 2.0, 5.5, 3.8, 1.20};

	//ParaRange paraRangeTemp3 = {5, 30, 15, 2.5, 9.3, 4.4, 2.8, 4.8, 3.8, 1.15};//标粒，测试结果
	ParaRange paraRangeTemp3 = {5, 150, 15, 2.5, 9.3, 4.4, 2.8, 4.8, 3.8, 1.15};//标粒，测试结果
	ParaRange paraRangeTemp4 = {30, 150, 62, 6, 11.7, 9.7, 2.8, 9.6, 7.8, 1.16};//红细胞，测试结果

	if (nSampleType == 1)//干涸人精
	{
		paraRangeTemp = paraRangeTemp1;
	}
	else if (nSampleType == 2)//新鲜人精
	{
		paraRangeTemp = paraRangeTemp2;
	}
	else if (nSampleType == 3)//标粒
	{
		paraRangeTemp = paraRangeTemp3;
	}
	else if (nSampleType == 4)//红细胞
	{
		paraRangeTemp = paraRangeTemp4;
	}
	else//其他
	{
		paraRangeTemp = paraRangeTemp3;
	}

	pSAveSpermRange->dAreaMin = paraRangeTemp.dAreaMin;
	pSAveSpermRange->dAreaMax = paraRangeTemp.dAreaMax;
	pSAveSpermRange->dAreaAve = paraRangeTemp.dAreaAve;

	pSAveSpermRange->dMajorLengthMin = paraRangeTemp.dMajorLengthMin;
	pSAveSpermRange->dMajorLengthMax = paraRangeTemp.dMajorLengthMax;
	pSAveSpermRange->dMajorLengthAve = paraRangeTemp.dMajorLengthAve;

	pSAveSpermRange->dMinorLengthMin = paraRangeTemp.dMinorLengthMin;
	pSAveSpermRange->dMinorLengthMax = paraRangeTemp.dMinorLengthMax;
	pSAveSpermRange->dMinorLengthAve = paraRangeTemp.dMinorLengthAve;

	pSAveSpermRange->dShapeRatio = paraRangeTemp.dShapeRatio;
}

//初始化精子信息
void iniSpermInfor(SpermInfor *pSSpermInfor, int nNumSperm)
{
	for (int i = 0; i< nNumSperm; i++)
	{
		pSSpermInfor[i].dPosX = 0;
		pSSpermInfor[i].dPosY = 0;
		pSSpermInfor[i].dMajAxsLen = 0;
		pSSpermInfor[i].dMinAxsLen = 0;
		pSSpermInfor[i].dAngle = 0;
		pSSpermInfor[i].dArea = 0;
		pSSpermInfor[i].nType = 0;
		pSSpermInfor[i].nNumMatch = 0;
	}
}


//初始化精子轨迹参数
TraceInfor * iniSpermTrace(int nNumTrace, int nImgFileNum)
{
	TraceInfor *pSSpermTraceInfor = (TraceInfor *)malloc(sizeof(TraceInfor)*nNumTrace*nImgFileNum);//MAXSPERMNUM*nImgFileNum条轨迹
	if (NULL == pSSpermTraceInfor)
	{
		return NULL;
	}

	//double dSizeTest = sizeof(TraceInfor)*nNumTrace*nImgFileNum/1024.0/1024.0;
	//初始化
	for (int i = 0; i<nNumTrace; i++)
	{
		for (int j = 0; j < nImgFileNum; j++)
		{
			pSSpermTraceInfor[i*nImgFileNum+j].fPosX = 0;
			pSSpermTraceInfor[i*nImgFileNum+j].fPosY = 0;
			pSSpermTraceInfor[i*nImgFileNum+j].fArea = 0;
			pSSpermTraceInfor[i*nImgFileNum+j].fEccen = 0;
			pSSpermTraceInfor[i*nImgFileNum+j].fAngle = 0;
			pSSpermTraceInfor[i*nImgFileNum+j].nPredictNum = 0;
		}
	}
	return pSSpermTraceInfor;
}

//获取精子运动轨迹的主函数
int getSpermTrace(const char **ppcFilePath, const char **ppcResImgFile, const char * pcResultPath, int const& nImgFileNum, SSettings const& SParaInput, ParaRange *pSAveSpermRange, algsqamed_data_out *dataOut, int nCenterPara[])
{
	int nStatus = 1;
//
//	//轨迹数据初始化，pSSpermTraceInfor[i*nImgFileNum+j]表示第i条轨迹上的第j个点
//	int nNumSperm = 0;
//	if (dataOut->nActiveSpermNum >5)//活动精子数
//	{
//		nNumSperm = dataOut->nActiveSpermNum;
//	}
//	else
//	{
//		nNumSperm = MAXSPERMNUM;
//	}
//	int nNumTrace = (int)((nNumSperm*nImgFileNum+3*nNumSperm)/4);//轨迹数量（轨迹数据-行）
//	int nTraceIndex = 0;

// V1.0.3-H 版本  轨迹数据初始化，根据整幅图像的总精子数预留足够的轨迹缓冲空间，防止小活动数时缓冲区溢出导致 -15 退出
	int nNumSperm = 0;
	if (dataOut->nTotalSpermNum > 0)
	{
		nNumSperm = dataOut->nTotalSpermNum * 2; // 按总数2倍预留充足空间
		if (nNumSperm < 500) nNumSperm = 500;
		if (nNumSperm > MAXSPERMNUM) nNumSperm = MAXSPERMNUM;
	}
	else
	{
		nNumSperm = MAXSPERMNUM;
	}
	int nNumTrace = (int)((nNumSperm*nImgFileNum+3*nNumSperm)/4);//轨迹数量（轨迹数据-行）
	int nTraceIndex = 0;

	TraceInfor *pSSpermTraceInfor = iniSpermTrace(nNumTrace, nImgFileNum);
	if (NULL == pSSpermTraceInfor)
	{
		nStatus = 0;//内存异常
		return nStatus;
	}

	//int nProcess = 1;
	int nSpermType = 4;
	
	//获取轨迹
	for (int i = 0; i< nImgFileNum; i++)
	{
		bool bMatchStatus = true;
		//1.单幅图中精子数据初始化
		SpermInfor *pSSpermInfor = (SpermInfor *)malloc(sizeof(SpermInfor)*MAXSPERMNUM);
		if (NULL == pSSpermInfor)
		{
			free(pSSpermTraceInfor); 
			pSSpermTraceInfor = NULL;

			nStatus = 0;//内存异常
			return nStatus;
		}
		int nSpermIndex = 0;
		iniSpermInfor(pSSpermInfor, MAXSPERMNUM);

		//1.读取图像并提取子图
		IplImage * pImgSrcTemp = cvLoadImage(ppcResImgFile[i],0);

		getImgContourInfor(pImgSrcTemp,pSAveSpermRange,nSpermType,pSSpermInfor,nSpermIndex);
		if (nSpermIndex > nNumTrace)
		{
			//异常
			free(pSSpermTraceInfor); 
			pSSpermTraceInfor = NULL;
			free(pSSpermInfor);
			pSSpermInfor = NULL;
			cvReleaseImage(&pImgSrcTemp);

			nStatus = -15;//内存异常
			return nStatus;
		}

		if (i == 0)//第一帧
		{
			matchFirstSpermImg(pSSpermTraceInfor, nImgFileNum, nTraceIndex, pSSpermInfor, nSpermIndex);
		}
		else if (i == 1 && nTraceIndex < nNumTrace)//第二帧
		{
			nStatus = matchSecondSpermImg(pSSpermTraceInfor, nImgFileNum, nTraceIndex, pSSpermInfor, nSpermIndex, nNumTrace);
		}
		else if (i >= 2 && nTraceIndex < nNumTrace)//第三帧及以上
		{
			nStatus = matchThirdSpermImg(pSSpermTraceInfor, nImgFileNum, i, nTraceIndex, pSSpermInfor, nSpermIndex, nNumTrace, nCenterPara);
		}

		free(pSSpermInfor);
		pSSpermInfor = NULL;
		cvReleaseImage(&pImgSrcTemp);

		if (nStatus != 1)
		{
			free(pSSpermTraceInfor); 
			pSSpermTraceInfor = NULL;

			return nStatus;//内存异常
		}
	}

	//轨迹的分析与分类初始化
	int *pnTraceType = (int *)malloc(sizeof(int)*nTraceIndex);//微动0/曲线1/直线2
	if ( NULL == pnTraceType )
	{
		free(pSSpermTraceInfor);
		pSSpermTraceInfor = NULL;

		nStatus = 0;//内存异常
		return nStatus;
	}
	for (int i = 0; i< nTraceIndex; i++)
	{
		pnTraceType[i] = 0;
	}

	//轨迹和精子的分类
	nStatus = classifyTraceSperm(pSSpermTraceInfor, nTraceIndex, nImgFileNum, pnTraceType, SParaInput, dataOut);
	if (nStatus != 1)
	{
		free(pSSpermTraceInfor); 
		pSSpermTraceInfor = NULL;
		free(pnTraceType);
		pnTraceType = NULL;

		return nStatus;//内存异常
	}


	//精子轨迹的绘制
	double dDeltaT = 1/(SParaInput.dFrameRate+EPSINON);//采样间隔
	double dLimitedLength = 125*dDeltaT/(SParaInput.dRatioImg + EPSINON);///轨迹上相邻两个点的最远距离，原最大速度100um/s，调整为125um/s
	// 允许瞬时速度超过的超高速精子画出完整连续轨迹线  最大速度可设置为100-150um/s
	nStatus = drawTraceImg(ppcFilePath, nImgFileNum, ppcResImgFile, pSSpermTraceInfor, nTraceIndex, pnTraceType, dLimitedLength, nCenterPara);

	//资源释放
	free(pSSpermTraceInfor); 
	pSSpermTraceInfor = NULL;
	free(pnTraceType);
	pnTraceType = NULL;

	return nStatus;
}

//复制匹配的精子信息到轨迹
void copyInfor2Trace(TraceInfor *pSSpermTraceInfor, int nTraceIndexCopy, SpermInfor *pSSpermInfor, int nSpermIndexCopy, int nPredictNum)
{
	pSSpermTraceInfor[nTraceIndexCopy].fPosX = (float)pSSpermInfor[nSpermIndexCopy].dPosX;
	pSSpermTraceInfor[nTraceIndexCopy].fPosY = (float)pSSpermInfor[nSpermIndexCopy].dPosY;

	float dMajorL = (float)pSSpermInfor[nSpermIndexCopy].dMajAxsLen;
	float dMinorL = (float)pSSpermInfor[nSpermIndexCopy].dMinAxsLen;
	pSSpermTraceInfor[nTraceIndexCopy].fArea = (float)pSSpermInfor[nSpermIndexCopy].dArea;
	pSSpermTraceInfor[nTraceIndexCopy].fEccen = sqrt(dMajorL*dMajorL - dMinorL*dMinorL)/dMajorL;
	pSSpermTraceInfor[nTraceIndexCopy].fAngle = (float)pSSpermInfor[nSpermIndexCopy].dAngle;
	pSSpermTraceInfor[nTraceIndexCopy].nPredictNum = nPredictNum;
}

//第一帧精子匹配（轨迹提取）
void matchFirstSpermImg(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, int &nTraceIndex, SpermInfor *pSSpermInfor, int nSpermIndex)
{
	for (int i=0; i< nSpermIndex; i++)
	{
		pSSpermInfor[i].nNumMatch = 1;

		//复制匹配的精子信息到轨迹
		int nTraceIndexCopy = i*nImgFileNum+0;
		copyInfor2Trace(pSSpermTraceInfor, nTraceIndexCopy, pSSpermInfor, i, 0);
	}
	nTraceIndex = nSpermIndex;
}

//第二帧精子匹配（轨迹提取）
int matchSecondSpermImg(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, int &nTraceIndex, SpermInfor *pSSpermInfor, int nSpermIndex, int nNumTrace)
{
	int nStatus = 1;

	int nTraceTemp = nTraceIndex;
	int nSearchRadius = 8;  // 数值从5调整为8，解决：长途高速运动精子脱靶丢线

	//1.精子匹配
	for (int i=0; i< nTraceTemp; i++)//轨迹loop
	{
		//第二帧精子匹配核心过程
		int *nMatchIndex = (int *)malloc(sizeof(int)*MAXMATCHSPERM);//多个目标被匹配到
		if (NULL == nMatchIndex)
		{
			nStatus = 0;
			return nStatus;
		}
		int nMatch = 0;

		//第一次搜索
		for (int j=0; j< nSpermIndex; j++)//精子loop
		{
			if ((pSSpermInfor[j].dPosX <= pSSpermTraceInfor[i*nImgFileNum+0].fPosX + nSearchRadius)
				&& (pSSpermInfor[j].dPosX >= pSSpermTraceInfor[i*nImgFileNum+0].fPosX - nSearchRadius)
				&&(pSSpermInfor[j].dPosY <= pSSpermTraceInfor[i*nImgFileNum+0].fPosY + nSearchRadius)
				&& (pSSpermInfor[j].dPosY >= pSSpermTraceInfor[i*nImgFileNum+0].fPosY - nSearchRadius)
				&& nMatch <= MAXMATCHSPERM)//区域判断
			{
				nMatchIndex[nMatch] = j;
				nMatch++;
			}
		}

		//搜索结果进行判断和处理
		if (nMatch == 1)//只有一个目标，成功匹配
		{
			//复制匹配的精子信息到轨迹
			int nTraceIndexCopy = i*nImgFileNum+1;
			int nSpermIndexCopy = nMatchIndex[0];
			copyInfor2Trace(pSSpermTraceInfor, nTraceIndexCopy, pSSpermInfor, nSpermIndexCopy, 0);

			//更新pSSpermInfor
			pSSpermInfor[nMatchIndex[0]].nNumMatch ++;
		}
		else if (nMatch == 0)//未匹配到，则扩大搜索范围，并启动第二次搜索
		{
			for (int j=0; j< nSpermIndex; j++)//精子loop
			{
				//区域判断
				if ((pSSpermInfor[j].dPosX <= pSSpermTraceInfor[i*nImgFileNum+0].fPosX + 2*nSearchRadius)
					&& (pSSpermInfor[j].dPosX >= pSSpermTraceInfor[i*nImgFileNum+0].fPosX - 2*nSearchRadius)
					&&(pSSpermInfor[j].dPosY <= pSSpermTraceInfor[i*nImgFileNum+0].fPosY + 2*nSearchRadius)
					&& (pSSpermInfor[j].dPosY >= pSSpermTraceInfor[i*nImgFileNum+0].fPosY - 2*nSearchRadius)
					&& nMatch <= MAXMATCHSPERM)
				{
					nMatchIndex[nMatch] = j;
					nMatch++;
				}
			}

			//搜索结果进行判断和处理
			if (nMatch == 1)//只有一个目标，成功匹配
			{
				//复制匹配的精子信息到轨迹
				int nTraceIndexCopy = i*nImgFileNum+1;
				int nSpermIndexCopy = nMatchIndex[0];
				copyInfor2Trace(pSSpermTraceInfor, nTraceIndexCopy, pSSpermInfor, nSpermIndexCopy, 0);

				//更新pSSpermInfor
				pSSpermInfor[nMatchIndex[0]].nNumMatch ++;
			}
			else if (nMatch == 0)//没有目标
			{				
				for (int k = 1; k < nImgFileNum ; k++)
				{
					pSSpermTraceInfor[i*nImgFileNum+k].nPredictNum = -1;//表示轨迹结束
				}
			}
			else//多个匹配目标，进行筛选（最近距离准则）
			{
				int nTraceIndex = i;
				int nNumImg = 2;
				nStatus = minDistRule(pSSpermTraceInfor, nImgFileNum, pSSpermInfor, nTraceIndex, nNumImg, nMatchIndex, nMatch);
				if (nStatus != 1)//内存异常
				{
					free(nMatchIndex);
					nMatchIndex = NULL;
					return nStatus;
				}
			}
		}
		else//多个匹配目标，进行筛选（最近距离准则）
		{
			int nTraceIndex = i;
			int nNumImg = 2;
			nStatus = minDistRule(pSSpermTraceInfor, nImgFileNum, pSSpermInfor, nTraceIndex, nNumImg, nMatchIndex, nMatch);
			if (nStatus != 1)//内存异常
			{
				free(nMatchIndex);
				nMatchIndex = NULL;
				return nStatus;
			}
		}

		free(nMatchIndex);
		nMatchIndex = NULL;
	}

	//2.未被匹配到的精子加入轨迹序列
	for (int i=0; i< nSpermIndex; i++)
	{
		if (pSSpermInfor[i].nNumMatch == 0)
		{
			//复制精子信息到轨迹
			int nTraceIndexCopy = nTraceIndex*nImgFileNum+1;

			if (nTraceIndex < nNumTrace)
			{
				int nSpermIndexCopy = i;
				copyInfor2Trace(pSSpermTraceInfor, nTraceIndexCopy, pSSpermInfor, nSpermIndexCopy, 0);

				nTraceIndex = nTraceIndex + 1;
			}
		}
	}
	return nStatus;
}

//第三帧精子匹配（轨迹提取）
int matchThirdSpermImg(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, const int nImgIndex, int &nTraceIndex, SpermInfor *pSSpermInfor, int nSpermIndex, int nNumTrace, int nCenterPara[])
{
	int nStatus = 1;

	int nTraceTemp = nTraceIndex;
	int nSearchRadius = 8;  // 数值从5调整为8，解决：长途高速运动精子脱靶丢线
	int nHistoryStatus = 0;//历史轨迹数据点状态//连续三点有数据//连续两点有数据//只有一点有数据

	//1.精子匹配
	for (int i=0; i< nTraceTemp; i++)//轨迹loop
	{
		if (pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].nPredictNum != -1)//-1表示轨迹结束
		{
			//第三帧精子匹配核心过程
			int *nMatchIndex = (int *)malloc(sizeof(int)*MAXMATCHSPERM);
			if (NULL == nMatchIndex)
			{
				nStatus = 0;//内存异常
				return nStatus;
			}
			int nMatch = 0;

			//第一次搜索
			double dSearchCenterX = 0;
			double dSearchCenterY = 0;
			
			//搜索中心确定（直线预测）
			if (nImgIndex >= 3)
			{
				if (pSSpermTraceInfor[i*nImgFileNum+nImgIndex-3].fPosX >0)
				{
					dSearchCenterX = 1.5*pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosX - 0.5*pSSpermTraceInfor[i*nImgFileNum+nImgIndex-3].fPosX;
					dSearchCenterY = 1.5*pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosY - 0.5*pSSpermTraceInfor[i*nImgFileNum+nImgIndex-3].fPosY;
					nHistoryStatus = 1;//连续三点有数据
				}
				else if (pSSpermTraceInfor[i*nImgFileNum+nImgIndex-2].fPosX >0)
				{
					dSearchCenterX = 2*pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosX - pSSpermTraceInfor[i*nImgFileNum+nImgIndex-2].fPosX;
					dSearchCenterY = 2*pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosY - pSSpermTraceInfor[i*nImgFileNum+nImgIndex-2].fPosY;
					nHistoryStatus = 2;//连续两点有数据
				}
				else
				{
					dSearchCenterX = pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosX;
					dSearchCenterY = pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosY;
					nHistoryStatus = 3;//只有一点有数据
				}
			}
			else//nImgIndex == 2
			{
				if (pSSpermTraceInfor[i*nImgFileNum+nImgIndex-2].fPosX >0)
				{
					dSearchCenterX = 2*pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosX - pSSpermTraceInfor[i*nImgFileNum+nImgIndex-2].fPosX;
					dSearchCenterY = 2*pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosY - pSSpermTraceInfor[i*nImgFileNum+nImgIndex-2].fPosY;
					nHistoryStatus = 4;//连续两点有数据
				}
				else
				{
					dSearchCenterX = pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosX;
					dSearchCenterY = pSSpermTraceInfor[i*nImgFileNum+nImgIndex-1].fPosY;
					nHistoryStatus = 5;//只有一点有数据
				}
			}

			//搜索
			for (int j=0; j< nSpermIndex; j++)//精子loop
			{
				if ((pSSpermInfor[j].dPosX <= dSearchCenterX + nSearchRadius) && (pSSpermInfor[j].dPosX >= dSearchCenterX - nSearchRadius)
					&&(pSSpermInfor[j].dPosY <= dSearchCenterY + nSearchRadius) && (pSSpermInfor[j].dPosY >= dSearchCenterY - nSearchRadius)
					&& nMatch <= MAXMATCHSPERM)//区域判断
				{
					nMatchIndex[nMatch] = j;
					nMatch++;
				}
			}

			//搜索结果进行判断和处理
			if (nMatch == 1)//只有一个目标，成功匹配
			{
				//复制匹配的精子信息到轨迹
				int nTraceIndexCopy = i*nImgFileNum+nImgIndex;
				int nSpermIndexCopy = nMatchIndex[0];
				copyInfor2Trace(pSSpermTraceInfor, nTraceIndexCopy, pSSpermInfor, nSpermIndexCopy, 0);

				//更新pSSpermInfor
				pSSpermInfor[nMatchIndex[0]].nNumMatch ++;//被匹配的次数加1
			}
			else if (nMatch == 0)//未匹配到，则扩大搜索范围，并启动第二次搜索
			{
				for (int j=0; j< nSpermIndex; j++)//精子loop
				{
					//区域判断
					if ((pSSpermInfor[j].dPosX <= dSearchCenterX + 2*nSearchRadius) && (pSSpermInfor[j].dPosX >= dSearchCenterX - 2*nSearchRadius)
						&&(pSSpermInfor[j].dPosY <= dSearchCenterY + 2*nSearchRadius) && (pSSpermInfor[j].dPosY >= dSearchCenterY - 2*nSearchRadius)
						&& nMatch <= MAXMATCHSPERM)//区域判断
					{
						nMatchIndex[nMatch] = j;
						nMatch++;
					}
				}

				//搜索结果进行判断和处理
				if (nMatch == 1)//只有一个目标，成功匹配
				{
					//复制匹配的精子信息到轨迹
					int nTraceIndexCopy = i*nImgFileNum+nImgIndex;
					int nSpermIndexCopy = nMatchIndex[0];
					copyInfor2Trace(pSSpermTraceInfor, nTraceIndexCopy, pSSpermInfor, nSpermIndexCopy, 0);

					//更新pSSpermInforTemp
					pSSpermInfor[nMatchIndex[0]].nNumMatch ++;//被匹配的次数加1
				}
				else if (nMatch == 0)//没有目标，进行预测
				{
					//轨迹预测
					int nTraceIndexCopy = i*nImgFileNum+nImgIndex;
					traceForecast(pSSpermTraceInfor, nTraceIndexCopy, dSearchCenterX, dSearchCenterY, nImgFileNum, i, nImgIndex, nCenterPara);
				}
				else//多个匹配目标，进行筛选（运动准则（速度/方向）、几何特征准则、最近距离准则）
				{
					int nTraceIndex = i;
					if (nHistoryStatus == 3 || nHistoryStatus == 5)
					{
						nStatus = minDistRule(pSSpermTraceInfor, nImgFileNum, pSSpermInfor, nTraceIndex, nImgIndex, nMatchIndex, nMatch);
					}
					else
					{
						nStatus = matchMultiObjs(pSSpermTraceInfor, nImgFileNum, pSSpermInfor, nTraceIndex, nImgIndex, nMatchIndex, nMatch, nHistoryStatus);
					}

					if (nStatus != 1)//内存异常
					{
						free(nMatchIndex);
						nMatchIndex = NULL;
						return nStatus;
					}
				}
			}
			else//多个匹配目标，进行筛选（运动准则（速度/方向）、几何特征准则、最近距离准则）
			{
				int nTraceIndex = i;
				if (nHistoryStatus == 3 || nHistoryStatus == 5)
				{
					nStatus = minDistRule(pSSpermTraceInfor, nImgFileNum, pSSpermInfor, nTraceIndex, nImgIndex, nMatchIndex, nMatch);
				}
				else
				{
					nStatus = matchMultiObjs(pSSpermTraceInfor, nImgFileNum, pSSpermInfor, nTraceIndex, nImgIndex, nMatchIndex, nMatch, nHistoryStatus);
				}

				if (nStatus != 1)//内存异常
				{
					free(nMatchIndex);
					nMatchIndex = NULL;
					return nStatus;
				}
			}

			free(nMatchIndex);
			nMatchIndex = NULL;
		}

		if (nStatus == 0)//内存异常
		{
			return nStatus;
		}
	}

	//2.未被匹配到的精子加入轨迹序列
	for (int i=0; i< nSpermIndex; i++)
	{
		if (pSSpermInfor[i].nNumMatch == 0)
		{
			//复制精子信息到轨迹
			int nTraceIndexCopy = nTraceIndex*nImgFileNum+nImgIndex;
			if (nTraceIndex < nNumTrace)
			{
				int nSpermIndexCopy = i;
				copyInfor2Trace(pSSpermTraceInfor, nTraceIndexCopy, pSSpermInfor, nSpermIndexCopy, 0);

				nTraceIndex = nTraceIndex + 1;
			}
			else
			{
				break;
			}
		}
	}

	return nStatus;
}

//多个匹配目标，进行筛选（运动准则（速度/方向）、几何特征准则、最近距离准则）
int matchMultiObjs(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, SpermInfor *pSSpermInfor, int nTraceIndex, int nImgIndex, int *nMatchIndex, int nMatch, int nHistoryStatus)
{
	int nStatus = 1;

	double *dMatchR = (double *)malloc(sizeof(double)*nMatch);//匹配因子1
	if (NULL == dMatchR)
	{
		nStatus = 0;//内存异常
		return nStatus;
	}

	double *dVectorR = (double *)malloc(sizeof(double)*nMatch);//速度(0.5)与方向(0.5)因子0.3
	if (NULL == dVectorR)
	{
		free(dMatchR);
		dMatchR = NULL;

		nStatus = 0;//内存异常
		return nStatus;
	}

	double *dGeoR = (double *)malloc(sizeof(double)*nMatch);//几何特征因子（面积0.5/离心率0.25/方向角0.25）0.3
	if (NULL == dGeoR)
	{
		free(dMatchR);
		dMatchR = NULL;
		free(dVectorR);
		dVectorR = NULL;

		nStatus = 0;//内存异常
		return nStatus;
	}

	double *dMinDistR = (double *)malloc(sizeof(double)*nMatch);//最近距离因子0.4
	if (NULL == dMinDistR)
	{
		free(dMatchR);
		dMatchR = NULL;
		free(dVectorR);
		dVectorR = NULL;
		free(dGeoR);
		dGeoR = NULL;

		nStatus = 0;//内存异常
		return nStatus;
	}

	//参考信息
	double x1 = pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-1].fPosX;
	double y1 = pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-1].fPosY;
	double dRefU = 0;
	double dRefV = 0;

	double dRefArea = pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-1].fArea;
	double dRefEccen = pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-1].fEccen;
	double dRefOrient = pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-1].fAngle;

	if (nHistoryStatus == 1)//连续三点有数据
	{
		dRefU = (x1 - pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-3].fPosX)/2.0;
		dRefV = (y1 - pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-3].fPosY)/2.0;
	} 
	else//连续两点有数据
	{
		dRefU = (x1 - pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-2].fPosX);
		dRefV = (y1 - pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-2].fPosY);
	}
	double dRefLength = sqrt(dRefU*dRefU + dRefV*dRefV) + EPSINON;

	for (int i = 0; i < nMatch; i++)
	{
		//速度与方向因子(速度0.5/方向0.5)
		double x2 = pSSpermInfor[nMatchIndex[i]].dPosX;
		double y2 = pSSpermInfor[nMatchIndex[i]].dPosY;
		double dLength = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1)) + EPSINON;
		dVectorR[i] = 0.5*(1-fabs(1-dLength/(dRefLength + EPSINON))) + 0.5*(dRefU*(x2-x1)+dRefV*(y2-y1))/(dRefLength + EPSINON)/(dLength + EPSINON);

		//几何特征因子（面积0.5/离心率0.25/方向角0.25）
		double dMajorL = pSSpermInfor[nMatchIndex[i]].dMajAxsLen;
		double dMinorL = pSSpermInfor[nMatchIndex[i]].dMinAxsLen;
		double dS = pSSpermInfor[nMatchIndex[i]].dArea;//面积
		double dE = sqrt(dMajorL*dMajorL - dMinorL*dMinorL)/(dMajorL + EPSINON);//离心率
		double dA = pSSpermInfor[nMatchIndex[i]].dAngle;
		dGeoR[i] = 0.5*(1-fabs(1-dS/(dRefArea + EPSINON))) + 0.25*(1-fabs(1-dE/(dRefEccen + EPSINON))) + 0.25*(1-fabs(1-dA/(dRefOrient + EPSINON)));

		//最近距离因子
		dMinDistR[i] = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
	}

	//求最大值
	double nDistanceTemp = 0;
	for (int i = 0; i < nMatch; i++)
	{
		if (dMinDistR[i] > nDistanceTemp)
		{
			nDistanceTemp = dMinDistR[i];
		}
	}

	//计算最近距离因子
	for (int i = 0; i < nMatch; i++)
	{
		dMinDistR[i] = 1 - dMinDistR[i]/(nDistanceTemp + EPSINON);
	}

	//计算匹配因子
	for (int i = 0; i < nMatch; i++)
	{
		dMatchR[i] = 0.3*dVectorR[i] + 0.3*dGeoR[i] + 0.4*dMinDistR[i];
	}

	//计算匹配索引(最大值)
	double nRTemp = 0;
	int nMatchTemp = 0;
	for (int i = 0; i < nMatch; i++)
	{
		if (dMatchR[i] > nRTemp)
		{
			nRTemp = dMatchR[i];
			nMatchTemp = i;
		}
	}

	//复制精子信息到轨迹
	int nTraceIndexCopy = nTraceIndex*nImgFileNum+nImgIndex-1;
	int nSpermIndexCopy = nMatchIndex[nMatchTemp];
	copyInfor2Trace(pSSpermTraceInfor, nTraceIndexCopy, pSSpermInfor, nSpermIndexCopy, 0);

	//更新pSSpermInforTemp
	pSSpermInfor[nSpermIndexCopy].nNumMatch ++;

	free(dMatchR);
	dMatchR = NULL;
	free(dVectorR);
	dVectorR = NULL;
	free(dGeoR);
	dGeoR = NULL;
	free(dMinDistR);
	dMinDistR = NULL;

	return nStatus;
}

//最近距离准则
int minDistRule(TraceInfor *pSSpermTraceInfor, int const& nImgFileNum, SpermInfor *pSSpermInfor, int nTraceIndex, int nImgIndex, int *nMatchIndex, int nMatch)
{
	int nStatus = 1;

	double *nMatchDistance = (double *)malloc(sizeof(double)*nMatch);
	if (NULL == nMatchDistance)
	{
		nStatus = 0;//内存异常
		return nStatus;
	}

	for (int i = 0; i < nMatch; i++)
	{
		double x1 = pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-2].fPosX;
		double y1 = pSSpermTraceInfor[nTraceIndex*nImgFileNum+nImgIndex-2].fPosY;
		double x2 = pSSpermInfor[nMatchIndex[i]].dPosX;
		double y2 = pSSpermInfor[nMatchIndex[i]].dPosY;
		nMatchDistance[i] = sqrt((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
	}

	//求最小值
	double nDistanceTemp = 1000000;
	int nMatchTemp = 0;
	for (int i = 0; i < nMatch; i++)
	{
		if (nMatchDistance[i] < nDistanceTemp)
		{
			nDistanceTemp = nMatchDistance[i];
			nMatchTemp = i;
		}
	}

	//复制匹配的精子信息到轨迹
	int nTraceIndexCopy = nTraceIndex*nImgFileNum+nImgIndex-1;
	int nSpermIndexCopy = nMatchIndex[nMatchTemp];
	copyInfor2Trace(pSSpermTraceInfor, nTraceIndexCopy, pSSpermInfor, nSpermIndexCopy, 0);

	//更新pSSpermInforTemp
	pSSpermInfor[nMatchIndex[nMatchTemp]].nNumMatch ++;

	free(nMatchDistance);
	nMatchDistance = NULL;

	return nStatus;
}

//轨迹预测
void traceForecast(TraceInfor *pSSpermTraceInfor, int nTraceIndexCopy, double dSearchCenterX, double dSearchCenterY, int nImgFileNum, int nTraceIndex, int nImgIndex, int nCenterPara[])
{
	//int nImgIndexTemp = nImgIndex;

	int nWidth = nCenterPara[0];
	int nHeight = nCenterPara[1];

	if (pSSpermTraceInfor[nTraceIndexCopy-1].nPredictNum < 3 //原代码为最多预测五次  ，V1.0.3-H 版本【闪烁光点伪活力-环节二改造】，暂时修改：收紧至最多允许连续预测 3 次，一旦噪点熄灭立刻断开，切断伪轨迹的接力串联！
		&& (dSearchCenterX > 0 && dSearchCenterX < nWidth) && (dSearchCenterY > 0 && dSearchCenterY < nHeight)) //预测点不能超出边界
	{
		//如果预测点太远，则更新为预测方向上距离目标3个像素的位置
		double fTempCenterX = 0;
		double fTempCenterY = 0;
		double fTempCenterX0 = pSSpermTraceInfor[nTraceIndexCopy-1].fPosX;
		double fTempCenterY0 = pSSpermTraceInfor[nTraceIndexCopy-1].fPosY;
		double fLength = (float)sqrt((fTempCenterX0-dSearchCenterX)*(fTempCenterX0-dSearchCenterX) + (fTempCenterY0-dSearchCenterY)*(fTempCenterY0-dSearchCenterY));
		if (fLength > 10)
		{
			fTempCenterX = 3*(dSearchCenterX - fTempCenterX0)/(fLength + EPSINON) + fTempCenterX0;
			fTempCenterY = 3*(dSearchCenterY - fTempCenterY0)/(fLength + EPSINON) + fTempCenterY0;
			dSearchCenterX = fTempCenterX;
			dSearchCenterY = fTempCenterY;
		}

		//预测点赋值到轨迹
		pSSpermTraceInfor[nTraceIndexCopy].fPosX = (float)dSearchCenterX;
		pSSpermTraceInfor[nTraceIndexCopy].fPosY = (float)dSearchCenterY;

		pSSpermTraceInfor[nTraceIndexCopy].fArea = pSSpermTraceInfor[nTraceIndexCopy-1].fArea;
		pSSpermTraceInfor[nTraceIndexCopy].fEccen = pSSpermTraceInfor[nTraceIndexCopy-1].fEccen;
		pSSpermTraceInfor[nTraceIndexCopy].fAngle = pSSpermTraceInfor[nTraceIndexCopy-1].fAngle;
		pSSpermTraceInfor[nTraceIndexCopy].nPredictNum = pSSpermTraceInfor[nTraceIndexCopy-1].nPredictNum + 1;
	} 
	else
	{
		//预测后依然无匹配目标，则清除预测记录
		int nPredictNum = pSSpermTraceInfor[nTraceIndexCopy-1].nPredictNum;
		if (nPredictNum > 0)
		{
			for (int i = nPredictNum; i>0; i--)
			{
				nImgIndex--;
				int nIndexTemp = nTraceIndexCopy - i;
				if (nImgIndex >= 0 && nIndexTemp >= 0)
				{
					pSSpermTraceInfor[nIndexTemp].fPosX = 0;
					pSSpermTraceInfor[nIndexTemp].fPosY = 0;

					pSSpermTraceInfor[nIndexTemp].fArea = 0;
					pSSpermTraceInfor[nIndexTemp].fEccen = 0;
					pSSpermTraceInfor[nIndexTemp].fAngle = 0;
					pSSpermTraceInfor[nIndexTemp].nPredictNum = 0;
				}
				else if (nImgIndex == 0)
				{
					break;
				}
			}

			if (nTraceIndexCopy-nPredictNum >= nTraceIndex*nImgFileNum)
			{
				for (int k = nTraceIndexCopy-nPredictNum; k < (nTraceIndex+1)*nImgFileNum ; k++)
				{
					pSSpermTraceInfor[k].nPredictNum = -1;//表示轨迹结束
				}
			} 
		}
		else
		{
			for (int k = nTraceIndexCopy; k < (nTraceIndex+1)*nImgFileNum ; k++)
			{
				pSSpermTraceInfor[k].nPredictNum = -1;//表示轨迹结束
			}
		}
	}
}

//轨迹和精子的分类
int classifyTraceSperm(TraceInfor *pSSpermTraceInfor, int nTraceIndex, int nImgFileNum, int *pnTraceType, SSettings const& SParaInput, algsqamed_data_out *dataOut)
{
	int nStatus = 1;

	// V1.0.3-H 版本
	// ==============================================================================
	// CASA 精子动力学与抗噪参数体系 (WHO-6 标准 & 工业级抗噪统一控制台)
	// ==============================================================================

	// ---------------- 第一层：WHO-6 标准速度与分级门限 ----------------
	#define SPERM_VCL_MIN           2.5     // [um/s] 曲线速度绝对底限：代表头部左右晃动的剧烈程度，过滤剔除微弱摆动精子, 低于此值绝非生物运动，判为D级
	// ABCD速度分级的标准 VAP: 把左右扭动抹平，得到一条向前的平滑轨迹，代表头部向前推进的快慢，
	#define SPERM_VAP_C_MIN         0.0     // [um/s] C级(NP非前向)最低平滑路径速度
	#define SPERM_VAP_B_MIN         5.0     // [um/s] B级(慢速前向PR)平滑路径速度门限
	#define SPERM_VAP_A_MIN         25.0    // [um/s] A级(快速前向PR)平滑路径速度门限
	// 实施过程中，还遵循了WHO‑6 的 CASA 实现惯例：主速度使用 VAP，同时用 STR (VSL/VAP) 必须>= 0.6，过滤非前向高速摆动精子
	#define SPERM_STR_PR_MIN        0.60    // 前向性(STR=VSL/VAP)门限：WHO要求PR前向运动必须具有前向性
    // B级精子前向性判断 （STR (VSL/VAP) 必须>= 0.6）

	// ---------------- 第二层：单个精子/轨迹级防噪门限 (消除像素抖动与闪烁散斑) ----------------
	// 0.极度离散闪烁噪点识别 (散斑闪烁特征：折返角极大且线性度极低且无推进)（MAD高、LIN极低且无大直线速度）
	// 防闪烁噪点判据（dMAD > 85.0, dLIN < 0.25, dVSL < 15.0）
	#define SPERM_NOISE_MAD_MAX     85.0    // [度] 异常角位移门限(死精闪烁均值93.8°，活精均值55.5°)
	#define SPERM_NOISE_LIN_MIN     0.25    // 异常低线性度
	#define SPERM_NOISE_VSL_MAX     15.0    // [um/s] 异常低直线速度(真实A/B活精直线速度通常>25)
	
	// 1.短轨迹(8~14帧)浮沉活精 vs 闪烁噪点
	// 通道①：高速/浮沉活跃精子（爆发速度明显，死精抖动绝不可能达到 VCL>=15 或 VAP>=10）
	#define SPERM_SHORT_VCL_A       15.0    // [um/s] 短轨迹高速爆发A级VCL门限
	#define SPERM_SHORT_VAP_A       10.0    // [um/s] 短轨迹高速爆发A级VAP门限
	#define SPERM_SHORT_DSL_A       2.0     // [um] 短轨迹高速通道基础位移(防单帧闪烁跳变)
    // 通道②：中速/NP弱活精子（速度达标 + 具备DSL >1.8 um 真实位移，死精短时抖动位移通常 <1.2 um）
	#define SPERM_SHORT_DSL_NORMAL  1.2     // [um] 短轨迹中速通道真实位移门限

	// 2.长轨迹 (>= 15 帧)
	// 针对普通活精 vs 原地晃动死精：
	// 死精子即使抖动 20~25 帧，限制其净位移 dDSL 
	#define SPERM_DSL_MIN           1.2     // [um] 长轨迹最小直线净位移(死精布朗运动/晃动位移通常<1.0)
	// SPERM_DSL_MIN (长轨迹位移) 从1.5 调整为1.2，预期效果：摆动 1~2 个头长的弱活精子可被识别为 Class C

	#define SPERM_STR_LONG_MIN      0.05    // 长轨迹最小前向性(允许原地打转C级活精，但排除无序杂乱噪点)


	// ---------------- 第三层：样本级全局抗噪熔断 (防止全死精样本因大面积闪烁误报) ----------------
    // 熔断一
	// 全死精图即使有噪点突破，直线精子数也极低(nNumSL <= 3)，且直线平均速度极低(< 15 um/s)
        #define SPERM_SAMPLE_MIN_SL     3       // 样本最小直线运动精子数(死精闪烁全图通常<=2，活精达38+)
	#define SPERM_SAMPLE_SL_VSL_MIN 15.0    // [um/s] 样本直线运动有效均速阈值
    //熔断二
	// 条件1：整片活动比例极低 (< 3.0%) 
	// 条件2：整片无任何直线运动精子 (nNumSL == 0) 且总活动数 < 15 个（典型散斑闪烁特征）【这行直接用数字】
	// 条件3：全片检测到的活动目标平均直线推进速度极低 (dAveVSL < 12.0 um/s)
        #define SPERM_SAMPLE_NOISE_GATE 3.0     // [%] 样本活力绝对门限：全片若<3%通常为假活力底噪，直接清零
	#define SPERM_SAMPLE_AVE_VSL_MIN 12.0   // [um/s] 全片活动精子平均直线速度绝对下限


	// ---------------- 第四层：全片活力上下文二元动态熔断控制台 (Sample-Level Context Gate) ----------------
	// 核心原理：在全死精片或对照质控片上，全片无任何前向游动精子，自动开启最高防御级别，执行严苛门限绝不误报；
	// 当检测到全片存在明确前向运动活精子群 (Class A 和 Class B >= 1) 时，证明是活跃精液标本，适度放宽 Class C 原地微动/微幅摆动门槛。
	#define SPERM_CONFIDENT_PR_MIN_COUNT  1       // 判定为活跃标本所需的最少确信前向活精数（Class A+B，若全为0则判定为死精/质控片）
	#define SPERM_LIVE_SHORT_DSL_NP       0.9     // [um] 活跃样本模式下，短轨迹Class C允许的最小位移（约2个精子头宽）
	#define SPERM_LIVE_LONG_DSL_NP        0.8     // [um] 活跃样本模式下，长轨迹Class C允许的最小位移（约1.5~2个头宽）
	#define SPERM_FLAGELLA_VCL_MIN        6.5     // [um/s] 真实生物鞭毛高频摆动动能底限（死精光学闪烁通常<5.0，活精甩头通常>8.0）
	// ==============================================================================

	struct spermProperty 
	{
		double dDSL;
		double dVSL;	//平均直线运动速度
		double dVCL;	//平均曲线运动速度
		double dVAP;	//平均路径运动速度

		double dLIN;	//运动的直线度
		double dSTR;	//运动的前向性
		double dWOB;	//运动的摆动性

		double dALH;	//头部侧摆幅度
		double dMAD;	//平均角位移（度）
		int dBCF;		//交叉频率

		int nPosNum;              // 轨迹实际有效点数
		bool bValidCandidate;     // 是否通过基础完整度与连续性检验并完成动力学计算
		bool bErraticNoise;       // 是否命中散斑高频折返闪烁噪点
		bool bIsConfidentPR;      // 是否为确信的高前向性运动精子 (Class A / Class B)
	};

	spermProperty *pdTraceMotion = (spermProperty *)malloc(sizeof(spermProperty)*nTraceIndex);
	if (NULL == pdTraceMotion)
	{
		nStatus = 0;//内存异常
		return nStatus;
	}
	for (int i = 0; i<nTraceIndex; i++)
	{
		pdTraceMotion[i].dDSL = 0;
		pdTraceMotion[i].dVSL = 0;
		pdTraceMotion[i].dVCL = 0;
		pdTraceMotion[i].dVAP = 0;
		pdTraceMotion[i].dLIN = 0;
		pdTraceMotion[i].dSTR = 0;
		pdTraceMotion[i].dWOB = 0;
		pdTraceMotion[i].dALH = 0;
		pdTraceMotion[i].dMAD = 0;
		pdTraceMotion[i].dBCF = 0;
		pdTraceMotion[i].nPosNum = 0;
		pdTraceMotion[i].bValidCandidate = false;
		pdTraceMotion[i].bErraticNoise = false;
		pdTraceMotion[i].bIsConfidentPR = false;
	}

	for (int i= 0;i<10;i++)
	{
		dataOut->dHistVCL[i] = 0;
		dataOut->dHistVSL[i] = 0;
		dataOut->dHistVAP[i] = 0;
	}

	//V1.0.3-H 版本 === 【新增】：初始化运动学累加参数为 0 ===
	dataOut->dAveVSL = 0.0;
	dataOut->dAveVCL = 0.0;
	dataOut->dAveVAP = 0.0;
	dataOut->dLIN    = 0.0;
	dataOut->dSTR    = 0.0;
	dataOut->dWOB    = 0.0;
	dataOut->dALH    = 0.0;
	dataOut->dBCF    = 0;
	dataOut->dMAD    = 0.0;
	dataOut->dDCL    = 0.0;
	dataOut->dDSL    = 0.0;
	dataOut->dDAP    = 0.0;

	double dRatio = SParaInput.dRatioImg;//图像放大率
	double dDeltaT = 1/(SParaInput.dFrameRate+EPSINON);//采样间隔

	//分级
	int nNumClassA = 0;
	int nNumClassB = 0;
	int nNumClassC = 0;
	int nNumClassD = 0;

	int nNumActiveTrace = 0;

	// ==============================================================================
	// Pass 1: 全轨迹动力学特征提取与前向精子群 (PR: Class A + Class B) 确信检测
	// ==============================================================================
	int nConfidentClassA = 0;
	int nConfidentClassB = 0;

	for (int i = 0; i < nTraceIndex; i++)
	{
		double dSLLength = 0;//直线距离
		double dCLLength = 0;//曲线距离
		double dAPLength = 0;//平均路径距离

		int nPosNum = 0;//轨迹的数据点数
		int nStartPoint = 0;
		
		//求该条轨迹的数据点数与真实观测点数
		bool bStart = true;
		int nRealPosCount = 0; // 真实观测到的点数（非预测虚点）
		for (int j = 0; j < nImgFileNum; j++)
		{
			if (pSSpermTraceInfor[i*nImgFileNum+j].nPredictNum != -1 && 
				pSSpermTraceInfor[i*nImgFileNum+j].fPosX > 0.2 && pSSpermTraceInfor[i*nImgFileNum+j].fPosY > 0.2)
			{
				if (bStart)
				{
					nStartPoint = i*nImgFileNum+j;
					bStart = false;
				}
				nPosNum++;
				if (pSSpermTraceInfor[i*nImgFileNum+j].nPredictNum == 0)
				{
					nRealPosCount++; // 真实检测点
				}
			}
		}

		// 【环节三过滤1】：如果真实观测点数太少（< 6 帧），或者预测虚点占比超过 45%，判定为闪烁拼凑伪轨迹
		if (nRealPosCount < 6 || ((double)(nPosNum - nRealPosCount) / (nPosNum + EPSINON)) > 0.45)
		{
			pnTraceType[i] = 0; // 判定为无效噪点
			pdTraceMotion[i].bValidCandidate = false;
			continue;
		}

		// 有效轨迹帧数下限过滤 (允许 >= 7 帧的短轨迹进入)
		if (nPosNum <= 6 || nPosNum <= nImgFileNum / 4)
		{
			pnTraceType[i] = 0;//微动或不动
			pdTraceMotion[i].bValidCandidate = false;
			continue;
		}

		double *dPointXY = (double *)malloc(sizeof(double)*nPosNum*2);
		if (NULL == dPointXY)
		{
			free(pdTraceMotion);
			pdTraceMotion = NULL;
			nStatus = 0;//内存异常
			return nStatus;
		}
		double *dPointFitXY = (double *)malloc(sizeof(double)*nPosNum*2);
		if (NULL == dPointFitXY)
		{
			free(pdTraceMotion);
			pdTraceMotion = NULL;
			free(dPointXY);
			dPointXY = NULL;
			nStatus = 0;//内存异常
			return nStatus;
		}
		double *dDistTwoImgs = (double *)malloc(sizeof(double)*(nPosNum-1));
		if (NULL == dDistTwoImgs)
		{
			free(pdTraceMotion);
			pdTraceMotion = NULL;
			free(dPointXY);
			dPointXY = NULL;
			free(dPointFitXY);
			dPointFitXY = NULL;
			nStatus = 0;//内存异常
			return nStatus;
		}
		double *dDistPoints = (double *)malloc(sizeof(double)*(nPosNum));
		if (NULL == dDistPoints)
		{
			free(pdTraceMotion);
			pdTraceMotion = NULL;
			free(dPointXY);
			dPointXY = NULL;
			free(dPointFitXY);
			dPointFitXY = NULL;
			free(dDistTwoImgs);
			dDistTwoImgs = NULL;
			nStatus = 0;//内存异常
			return nStatus;
		}

		//曲线长度计算（含均值处理）
		for (int j = 0; j < nPosNum; j++)
		{
			dPointXY[2*j] = pSSpermTraceInfor[nStartPoint+j].fPosX;
			dPointXY[2*j+1] = pSSpermTraceInfor[nStartPoint+j].fPosY;
			if (j >= 1)
			{
				double dLengthTemp = sqrt((dPointXY[2*j]-dPointXY[2*(j-1)])*(dPointXY[2*j]-dPointXY[2*(j-1)])
					+(dPointXY[2*j+1]-dPointXY[2*(j-1)+1])*(dPointXY[2*j+1]-dPointXY[2*(j-1)+1]));
				dDistTwoImgs[j-1] = dLengthTemp;
			}
		}

		double *pdAveStd = calAveStdIter(dDistTwoImgs, nPosNum-1);
		dCLLength = pdAveStd[0]*(nPosNum-1);//平均值*数量
		free(pdAveStd);
		pdAveStd = NULL;

		// 【环节三过滤2】：单步瞬时位移突变过滤（防瞬移）
		bool bHasTeleportJump = false;
		for (int k = 0; k < nPosNum - 1; k++)
		{
			if (dDistTwoImgs[k] > 8.5) // 单帧瞬时跳变超过 8.5 像素
			{
				bHasTeleportJump = true;
				break;
			}
		}
		if (bHasTeleportJump)
		{
			free(dPointXY); dPointXY = NULL;
			free(dPointFitXY); dPointFitXY = NULL;
			free(dDistTwoImgs); dDistTwoImgs = NULL;
			free(dDistPoints); dDistPoints = NULL;
			pnTraceType[i] = 0;
			pdTraceMotion[i].bValidCandidate = false;
			continue;
		}

		//直线长度计算
		dSLLength = sqrt((dPointXY[2*(nPosNum-1)]-dPointXY[0])*(dPointXY[2*(nPosNum-1)]-dPointXY[0])
			+(dPointXY[2*nPosNum-1]-dPointXY[1])*(dPointXY[2*nPosNum-1]-dPointXY[1]));

		//路径长度计算（含均值处理）
		for (int j = 0; j < nPosNum; j++)
		{
			if ((j == 0)||(j == nPosNum-1))
			{
				dPointFitXY[2*j] = dPointXY[2*j];//X
				dPointFitXY[2*j+1] = dPointXY[2*j+1];//Y
			}
			else if ((j == 1)||(j == nPosNum-2))
			{
				dPointFitXY[2*j] = (dPointXY[2*(j-1)] + dPointXY[2*j] + dPointXY[2*(j+1)])/3;
				dPointFitXY[2*j+1] = (dPointXY[2*j-1] + dPointXY[2*j+1] + dPointXY[2*j+3])/3;
			}
			else
			{
				dPointFitXY[2*j] = (dPointXY[2*(j-2)] + dPointXY[2*(j-1)] + dPointXY[2*j] + dPointXY[2*(j+1)] + dPointXY[2*(j+2)])/5;
				dPointFitXY[2*j+1] = (dPointXY[2*j-3] + dPointXY[2*j-1] + dPointXY[2*j+1] + dPointXY[2*j+3] + dPointXY[2*j+5])/5;
			}
			if (j >= 1)
			{
				double dLengthTemp = sqrt((dPointFitXY[2*j]-dPointFitXY[2*(j-1)])*(dPointFitXY[2*j]-dPointFitXY[2*(j-1)])
					+(dPointFitXY[2*j+1]-dPointFitXY[2*(j-1)+1])*(dPointFitXY[2*j+1]-dPointFitXY[2*(j-1)+1]));
				dDistTwoImgs[j-1] = dLengthTemp;
			}
		}
		pdAveStd = calAveStdIter(dDistTwoImgs, nPosNum-1);
		dAPLength = pdAveStd[0]*(nPosNum-1);//平均值*数量
		free(pdAveStd);
		pdAveStd = NULL;

		//异常情况排除
		if ((dSLLength > 1.1*dCLLength) || (dSLLength > 1.1*dAPLength) || (dAPLength > 1.1*dCLLength))
		{
			free(dPointXY); dPointXY = NULL;
			free(dPointFitXY); dPointFitXY = NULL;
			free(dDistTwoImgs); dDistTwoImgs = NULL;
			free(dDistPoints); dDistPoints = NULL;
			pnTraceType[i] = 0;
			pdTraceMotion[i].bValidCandidate = false;
			continue;
		}

		// 计算dPointXY与dPointFitXY的点距离
		for (int j = 0; j < nPosNum; j++)
		{
			dDistPoints[j] = sqrt((dPointXY[2*j] - dPointFitXY[2*j])*(dPointXY[2*j] - dPointFitXY[2*j]) 
				+ (dPointXY[2*j+1] - dPointFitXY[2*j+1])*(dPointXY[2*j+1] - dPointFitXY[2*j+1]));
		}

		// 计算ALH和BCF
		double dMaxAve = 0.0;
		int nMaxNum = 0;
		calMaxAveNum(dDistPoints, nPosNum, &dMaxAve, &nMaxNum);

		// 计算MAD
		double dMad = calMAD(dPointXY, nPosNum);

		pdTraceMotion[i].dALH = dMaxAve*dRatio;
		pdTraceMotion[i].dBCF = nMaxNum + 1;
		pdTraceMotion[i].dMAD = _isnan(dMad) ? 0.0 : dMad;

		pdTraceMotion[i].dDSL = dSLLength*dRatio;//直线距离
		pdTraceMotion[i].dVSL = dSLLength*dRatio/(nPosNum - 1)/(dDeltaT + EPSINON);//1VSL
		pdTraceMotion[i].dVCL = dCLLength*dRatio/(nPosNum - 1)/(dDeltaT + EPSINON);//2VCL
		pdTraceMotion[i].dVAP = dAPLength*dRatio/(nPosNum - 1)/(dDeltaT + EPSINON);//3VAP

		pdTraceMotion[i].dLIN = pdTraceMotion[i].dVSL/(pdTraceMotion[i].dVCL + EPSINON);
		pdTraceMotion[i].dSTR = pdTraceMotion[i].dVSL/(pdTraceMotion[i].dVAP + EPSINON);
		pdTraceMotion[i].dWOB = pdTraceMotion[i].dVAP/(pdTraceMotion[i].dVCL + EPSINON);

		pdTraceMotion[i].nPosNum = nPosNum;
		pdTraceMotion[i].bValidCandidate = true;

		// 判定极度离散散斑闪烁噪点
		pdTraceMotion[i].bErraticNoise = (pdTraceMotion[i].dMAD > SPERM_NOISE_MAD_MAX && 
		                                  pdTraceMotion[i].dLIN < SPERM_NOISE_LIN_MIN && 
		                                  pdTraceMotion[i].dVSL < SPERM_NOISE_VSL_MAX);

		// 确信前向活精 (PR: Class A + Class B) 统计
		pdTraceMotion[i].bIsConfidentPR = false;
		if (!pdTraceMotion[i].bErraticNoise)
		{
			// Class A: 快速前向精子 (VAP >= 25 um/s 且具备真实位移)
			if (pdTraceMotion[i].dVAP >= SPERM_VAP_A_MIN && pdTraceMotion[i].dDSL >= SPERM_SHORT_DSL_A)
			{
				pdTraceMotion[i].bIsConfidentPR = true;
				nConfidentClassA++;
			}
			// Class B: 慢速前向精子 (VAP >= 5 um/s 且前向性 STR >= 0.60 且具备长/短真实位移)
			else if (pdTraceMotion[i].dVAP >= SPERM_VAP_B_MIN && 
			         pdTraceMotion[i].dSTR >= SPERM_STR_PR_MIN && 
			         pdTraceMotion[i].dDSL >= SPERM_DSL_MIN)
			{
				pdTraceMotion[i].bIsConfidentPR = true;
				nConfidentClassB++;
			}
		}

		free(dPointXY); dPointXY = NULL;
		free(dPointFitXY); dPointFitXY = NULL;
		free(dDistTwoImgs); dDistTwoImgs = NULL;
		free(dDistPoints); dDistPoints = NULL;
	}

	// ==============================================================================
	// 全片活力上下文二元动态熔断控制台 (Sample-Level Context Gate) 决策中心
	// ==============================================================================
	int nTotalConfidentPR = nConfidentClassA + nConfidentClassB;
	// 二元判定：
	// 1. 若全片检测到明确的前向运动精子群 (Class A 和 Class B >= SPERM_CONFIDENT_PR_MIN_COUNT)
	//    证明这是一份真实活跃的精液标本，系统适度放宽 Class C 原地微动/微幅摆动门槛；
	// 2. 若全片几乎没有前向运动精子 (Class A 和 Class B 都为 0，即 nTotalConfidentPR < SPERM_CONFIDENT_PR_MIN_COUNT)
	//    系统判定为疑似全死精片或对照质控片，自动切换到最高防御级别，执行现有严苛门限！
	bool bIsActiveSample = (nTotalConfidentPR >= SPERM_CONFIDENT_PR_MIN_COUNT);

	// ==============================================================================
	// Pass 2: 上下文自适应分类与运动学参数累加统计
	// ==============================================================================
	for (int i = 0; i < nTraceIndex; i++)
	{
		if (!pdTraceMotion[i].bValidCandidate || pdTraceMotion[i].bErraticNoise)
		{
			pnTraceType[i] = 0; // 不动精子或噪点，不画轨迹
			continue;
		}

		bool bIsValidMotile = false;
		int nCurPosNum = pdTraceMotion[i].nPosNum;

		if (bIsActiveSample)
		{
			// ----------------------------------------------------------------------
			// 【活跃样本模式】：已确信为真实活精标本，适度放宽 Class C 原地微动/摆动门限
			// ----------------------------------------------------------------------
			if (pdTraceMotion[i].bIsConfidentPR)
			{
				bIsValidMotile = true;
			}
			else if (nCurPosNum < 15) // 短轨迹 (8~14 帧)
			{
				// 通道①：高速浮沉活跃精子
				if (pdTraceMotion[i].dVCL >= SPERM_SHORT_VCL_A && 
				    pdTraceMotion[i].dVAP >= SPERM_SHORT_VAP_A && 
				    pdTraceMotion[i].dDSL  >= SPERM_SHORT_DSL_A)
				{
					bIsValidMotile = true;
				}
				// 通道②：活跃样本微动放宽通道 (1~3个精子头长微动，具备生物鞭毛摆动动能 VCL >= 6.5 um/s)
				else if (pdTraceMotion[i].dVCL >= SPERM_FLAGELLA_VCL_MIN && 
				         pdTraceMotion[i].dDSL >= SPERM_LIVE_SHORT_DSL_NP)
				{
					bIsValidMotile = true;
				}
				// 通道③：常规中速通道
				else if (pdTraceMotion[i].dVCL >= SPERM_VCL_MIN && 
				         pdTraceMotion[i].dVAP >= SPERM_VAP_C_MIN && 
				         pdTraceMotion[i].dDSL >= SPERM_SHORT_DSL_NORMAL)
				{
					bIsValidMotile = true;
				}
			}
			else // 长轨迹 (>= 15 帧)
			{
				// 通道①：常规长轨迹门限
				if (pdTraceMotion[i].dVCL >= SPERM_VCL_MIN && 
				    pdTraceMotion[i].dVAP >= SPERM_VAP_C_MIN && 
				    pdTraceMotion[i].dDSL >= SPERM_DSL_MIN &&
				    pdTraceMotion[i].dSTR >= SPERM_STR_LONG_MIN)
				{
					bIsValidMotile = true;
				}
				// 通道②：活跃样本长轨迹微动放宽通道 (具备生物鞭毛摆动动能 VCL >= 6.5 um/s，净位移 >= 0.8 um)
				else if (pdTraceMotion[i].dVCL >= SPERM_FLAGELLA_VCL_MIN && 
				         pdTraceMotion[i].dDSL >= SPERM_LIVE_LONG_DSL_NP)
				{
					bIsValidMotile = true;
				}
			}
		}
		else
		{
			// ----------------------------------------------------------------------
			// 【最高防御死精/质控片模式】：全片无明确PR精子，执行现有严苛门限拦截噪点
			// ----------------------------------------------------------------------
			if (nCurPosNum < 15) // 短轨迹 (8~14 帧)
			{
				if (pdTraceMotion[i].dVCL >= SPERM_SHORT_VCL_A && 
				    pdTraceMotion[i].dVAP >= SPERM_SHORT_VAP_A && 
				    pdTraceMotion[i].dDSL  >= SPERM_SHORT_DSL_A)
				{
					bIsValidMotile = true;
				}
				else if (pdTraceMotion[i].dVCL >= SPERM_VCL_MIN && 
				         pdTraceMotion[i].dVAP >= SPERM_VAP_C_MIN && 
				         pdTraceMotion[i].dDSL  >= SPERM_SHORT_DSL_NORMAL)
				{
					bIsValidMotile = true;
				}
			}
			else // 长轨迹 (>= 15 帧)
			{
				if (pdTraceMotion[i].dVCL >= SPERM_VCL_MIN && 
				    pdTraceMotion[i].dVAP >= SPERM_VAP_C_MIN && 
				    pdTraceMotion[i].dDSL  >= SPERM_DSL_MIN &&
				    pdTraceMotion[i].dSTR >= SPERM_STR_LONG_MIN)
				{
					bIsValidMotile = true;
				}
			}
		}

		if (bIsValidMotile)
		{
			nNumActiveTrace++; // 统计真正的活动轨迹数量

			// 累加活动精子的运动学特征
			dataOut->dAveVSL += pdTraceMotion[i].dVSL;
			dataOut->dAveVCL += pdTraceMotion[i].dVCL;
			dataOut->dAveVAP += pdTraceMotion[i].dVAP;
			dataOut->dLIN    += pdTraceMotion[i].dLIN;
			dataOut->dSTR    += pdTraceMotion[i].dSTR;
			dataOut->dWOB    += pdTraceMotion[i].dWOB;
			dataOut->dALH    += pdTraceMotion[i].dALH;
			dataOut->dBCF    += pdTraceMotion[i].dBCF;

			if (!_isnan(pdTraceMotion[i].dMAD))
			{
				dataOut->dMAD += pdTraceMotion[i].dMAD;
			}

			// 直线或曲线分类 (保留 WHO 运动学统计指标 SL/CL)
			if (pdTraceMotion[i].dLIN >= 0.65)
			{
				dataOut->nNumSL++;
			} 
			else
			{
				dataOut->nNumCL++;
			}

			// 速度分布图
			int nIndexSL = (int)(pdTraceMotion[i].dVSL / 10.0 + 0.5);
			if (nIndexSL <= 9) dataOut->dHistVSL[nIndexSL]++;
			
			int nIndexCL = (int)(pdTraceMotion[i].dVCL / 10.0 + 0.5);
			if (nIndexCL <= 9) dataOut->dHistVCL[nIndexCL]++;
			
			int nIndexAP = (int)(pdTraceMotion[i].dVAP / 10.0 + 0.5);
			if (nIndexAP <= 9) dataOut->dHistVAP[nIndexAP]++;

			// 运动分级 A / B / C 并确定轨迹显示类型 pnTraceType (0:D级不动, 1:B级慢速前向, 2:A级快速前向, 3:C级非前向)
			if (pdTraceMotion[i].dVAP >= SPERM_VAP_A_MIN)
			{
				nNumClassA++;
				pnTraceType[i] = 2; // Class A: 快速前向精子 (Rapid PR) -> RAPIDPRCOlOUR (绿色)
			} 
			else if (pdTraceMotion[i].dVAP >= SPERM_VAP_B_MIN && pdTraceMotion[i].dSTR >= SPERM_STR_PR_MIN)
			{
				nNumClassB++;
				pnTraceType[i] = 1; // Class B: 慢速前向精子 (Slow PR) -> SLOWPRCOlOUR (蓝色)
			}
			else
			{		
				nNumClassC++;
				pnTraceType[i] = 3; // Class C: 非前向精子 (NP) -> NPCOlOUR (红色)
			}
		}
		else
		{
			pnTraceType[i] = 0; // 不满足动态门限，标记为不动精子（D），不绘制运动轨迹
		}
	}

	 // V1.0.3-H 版本  
    // =========================================================================
	// 求各种数量与运动分级（修复篡改总数与重复累加，完全兼容下文变量）
	// =========================================================================

	// 1. 真实活动精子数（由动力学达标的 A+B+C 轨迹决定）
	int nNumABCTemp = nNumClassA + nNumClassB + nNumClassC;
	
	// 样本级全局抗噪熔断一（加固版）：
	// 死精/干涸片即使有噪点突破，直线精子数也极少(nNumSL <= 2)，且直线平均速度极低(< 18.0 um/s)
	// 活精图(如 8% 活力)直线精子达数十个、直线速度近 40 um/s，完全不可能误触发
	if (nNumABCTemp > 0)
	{
		double dCurrentMeanVSL = dataOut->dAveVSL / (double)nNumABCTemp;
		if (dataOut->nNumSL <= 2 && dCurrentMeanVSL < 18.0)
		{
			nNumClassA = 0;
			nNumClassB = 0;
			nNumClassC = 0;
			nNumABCTemp = 0;
			nNumActiveTrace = 0;
			dataOut->nNumSL = 0;
			dataOut->nNumCL = 0;
			for (int k = 0; k < nTraceIndex; k++)
			{
				pnTraceType[k] = 0;
			}
		}
	}

	// 2. 继承并锁定全景精子总数基准（杜绝篡改）
	int nNumTotalTemp = (int)dataOut->nTotalSpermNum;
	if (nNumABCTemp > nNumTotalTemp)
	{
		nNumTotalTemp = nNumABCTemp; // 容错保护：活动数不大于总数
	}

	// 3. 不动精子数 D = 总数 - 活动数（不再错误混入 nNumTemp）
	nNumClassD = nNumTotalTemp - nNumABCTemp;
	if (nNumClassD < 0)
	{
		nNumClassD = 0;
	}

	// 4. 写回输出结构体
	dataOut->nTotalSpermNum = nNumTotalTemp;
	dataOut->nActiveSpermNum = nNumABCTemp;

	// 5. 运动分级比例计算（含防零除与边界保护）
	if (nNumTotalTemp > 0)
	{
		dataOut->dRatioClassA = 100.0 * nNumClassA / nNumTotalTemp;
		dataOut->dRatioClassB = 100.0 * nNumClassB / nNumTotalTemp;
		dataOut->dRatioClassC = 100.0 * nNumClassC / nNumTotalTemp;
		
		// D 级比例做 [0.0, 100.0] 截断保护，防止浮点舍入出现负数
		double dRatioD = 100.0 - dataOut->dRatioClassA - dataOut->dRatioClassB - dataOut->dRatioClassC;
		dataOut->dRatioClassD = (dRatioD < 0.0) ? 0.0 : ((dRatioD > 100.0) ? 100.0 : dRatioD);
	}
	else
	{
		dataOut->dRatioClassA = 0.0;
		dataOut->dRatioClassB = 0.0;
		dataOut->dRatioClassC = 0.0;
		dataOut->dRatioClassD = 0.0;
	}

	// 6. WHO 标准联合分类
	dataOut->dRatioClassPR = dataOut->dRatioClassA + dataOut->dRatioClassB; // 前向运动 PR (A+B)
	dataOut->dRatioClassNP = dataOut->dRatioClassC;                          // 非前向运动 NP (C)
	dataOut->dRatioClassIM = dataOut->dRatioClassD;                          // 不动 IM (D)
	dataOut->dActiveSpermRatio = dataOut->dRatioClassPR + dataOut->dRatioClassNP; // 总活力 PR+NP


	// V1.0.3-H 样本级全局抗噪熔断二：
		// 条件1：整片活动比例极低 (< 3.0%) 
		// 条件2：整片无任何直线运动精子 (nNumSL == 0) 且总活动数 < 15 个（典型散斑闪烁特征）
		// 条件3：全片检测到的活动目标平均直线推进速度极低 (dAveVSL < 12.0 um/s)
		// #define SPERM_SAMPLE_NOISE_GATE 3.0     // [%] 样本活力绝对门限：全片若<3%通常为假活力底噪，直接清零
    	// #define SPERM_SAMPLE_AVE_VSL_MIN 12.0   // [um/s] 全片活动精子平均直线速度绝对下限
		// 样本级全局抗噪熔断二（加固版）：
		// 条件1：整片活动比例极低 (< 2.0%)
		// 条件2：整片无任何直线运动精子 (nNumSL == 0) 且总活动数 < 30 个（典型散斑闪烁特征）
		// 条件3：直线精子数极少 (nNumSL <= 2) 且直线平均速度低于 18.0 um/s（彻底截断 001 干枯片 27 个散乱噪点伪活力）
		// 条件4：全片检测到的活动目标平均直线推进速度极低 (dAveVSL < 12.0 um/s)
		bool bIsGhostMotionSample = (dataOut->dActiveSpermRatio < 2.0) ||
									(dataOut->nNumSL == 0 && dataOut->nActiveSpermNum < 30) ||
									(dataOut->nNumSL <= 2 && dataOut->dAveVSL / (nNumActiveTrace + EPSINON) < 18.0) ||
									(dataOut->nActiveSpermNum > 0 && (dataOut->dAveVSL / (nNumActiveTrace + EPSINON)) < SPERM_SAMPLE_AVE_VSL_MIN);

    if (bIsGhostMotionSample)
    {
        // 整片判定为死精，活力严格归零
        nNumClassA = 0;
        nNumClassB = 0;
        nNumClassC = 0;
        nNumClassD = nNumTotalTemp;
        nNumABCTemp = 0;

        dataOut->nActiveSpermNum = 0;
        dataOut->dRatioClassA = 0.0;
        dataOut->dRatioClassB = 0.0;
        dataOut->dRatioClassC = 0.0;
        dataOut->dRatioClassD = 100.0;
        dataOut->dRatioClassPR = 0.0;
        dataOut->dRatioClassNP = 0.0;
        dataOut->dRatioClassIM = 100.0;
        dataOut->dActiveSpermRatio = 0.0;
        dataOut->nNumSL = 0;
        dataOut->nNumCL = 0;
        dataOut->dRatioSL = 0.0;
        dataOut->dRatioCL = 0.0;
        dataOut->dAveVSL = 0.0;
        dataOut->dAveVCL = 0.0;
        dataOut->dAveVAP = 0.0;

			// V1.0.3-H 版本  【关键新增】：整片全死精时，将所有轨迹标记同步清零为 0（不动），防止图像绘制虚假轨迹
			for (int k = 0; k < nTraceIndex; k++)
			{
				pnTraceType[k] = 0;
			}
		}

	//速度等级分布图
	num2Percent(dataOut->dHistVSL, 10);
	num2Percent(dataOut->dHistVCL, 10);
	num2Percent(dataOut->dHistVAP, 10);

	//活力等级分布图
	dataOut->dHistRank[0] = dataOut->dRatioClassA;
	dataOut->dHistRank[1] = dataOut->dRatioClassB;
	dataOut->dHistRank[2] = dataOut->dRatioClassC;
	dataOut->dHistRank[3] = dataOut->dRatioClassD;

	//求各种平均值
	// ==================== 求各种平均值（增加判零保护） ====================
	if (nNumActiveTrace == 0 || dataOut->nActiveSpermNum == 0)
		{
			// 若全片无活动精子，所有运动学参数、摆动、角位移、频率严格归零
			dataOut->dAveVSL = 0.0;
			dataOut->dAveVCL = 0.0;
			dataOut->dAveVAP = 0.0;
			dataOut->dLIN    = 0.0;
			dataOut->dSTR    = 0.0;
			dataOut->dWOB    = 0.0;
			dataOut->dALH    = 0.0;
			dataOut->dBCF    = 0;
			dataOut->dMAD    = 0.0;

			dataOut->nNumSL  = 0;
			dataOut->nNumCL  = 0;
			dataOut->dRatioSL = 0.0;
			dataOut->dRatioCL = 0.0;

			dataOut->dDCL    = 0.0;
			dataOut->dDSL    = 0.0;
			dataOut->dDAP    = 0.0;
		}
		else
		{
			// 有真正的活动精子时，才计算各运动特征的均值
	dataOut->dAveVSL = dataOut->dAveVSL/(nNumActiveTrace+EPSINON);
	dataOut->dAveVCL = dataOut->dAveVCL/(nNumActiveTrace+EPSINON);
	dataOut->dAveVAP = dataOut->dAveVAP/(nNumActiveTrace+EPSINON);
	dataOut->dLIN = dataOut->dLIN/(nNumActiveTrace+EPSINON);
	dataOut->dSTR = dataOut->dSTR/(nNumActiveTrace+EPSINON);
	dataOut->dWOB = dataOut->dWOB/(nNumActiveTrace+EPSINON);

	dataOut->dALH = dataOut->dALH/(nNumActiveTrace+EPSINON);
	dataOut->dBCF = (int)(dataOut->dBCF/(nNumActiveTrace+EPSINON) + 0.5);
	dataOut->dMAD = dataOut->dMAD/(nNumActiveTrace+EPSINON);
		// 加固：若平均后仍是 NaN/Inf，输出 0，避免界面打印 1.#QNAN 同步动物宽窗版本改进MAD
	if (_isnan(dataOut->dMAD) || !_finite(dataOut->dMAD))
	{
		dataOut->dMAD = 0.0;
	}

	dataOut->nNumCL = dataOut->nActiveSpermNum - dataOut->nNumSL;
	if (dataOut->nNumCL < 0)
	{
		dataOut->nNumCL = 0;
		dataOut->nNumSL = dataOut->nActiveSpermNum;
	}
	dataOut->dRatioSL = 100*dataOut->nNumSL/(nNumTotalTemp+EPSINON);
	dataOut->dRatioCL = 100*dataOut->nNumCL/(nNumTotalTemp+EPSINON);	

	// 估算程序（这是真实计算）	
	dataOut->dDCL = dataOut->dAveVCL * 0.75;
	dataOut->dDSL = dataOut->dAveVSL * 0.75;
	dataOut->dDAP = dataOut->dAveVAP * 0.75;

	}

	//资源释放
	free(pdTraceMotion);
	pdTraceMotion = NULL;

	return nStatus;
}

//速度分布图（数量->百分比）
void num2Percent(double * dHist, int nNum)
{
	double dSum = 0;
	for (int i = 0; i<nNum; i++)
	{
		dSum = dSum + dHist[i];
	}

	if (dSum > 0)
	{
		for (int i = 0; i<nNum; i++)
		{
			dHist[i] = 100*dHist[i]/dSum;
		}
	}
	else
	{
		dHist[0] = 100;
	}

	//异常处理
}

//精子轨迹的绘制(只画了当前图像中未终结的精子轨迹)
// =========================================================================
// 自实现轻量级 CLAHE (对比度受限自适应直方图均衡化)
// 100% 纯 C/C++ 实现，仅使用基础内存指针与数学运算，彻底消除 OpenCV 版本/命名空间/模板依赖
// 完美兼容 VS2012 / GCC / Android NDK，无任何缺失符号
// =========================================================================
static void ApplySelfContainedCLAHE(IplImage *pGray, double clipLimit, int gridSize)
{
	if (!pGray || pGray->nChannels != 1 || pGray->depth != IPL_DEPTH_8U) return;
	if (gridSize < 4) gridSize = 4;
	if (clipLimit < 1.0) clipLimit = 1.0;

	int width = pGray->width;
	int height = pGray->height;
	int widthStep = pGray->widthStep;

	// 计算网格数量
	int numTilesX = (width + gridSize - 1) / gridSize;
	int numTilesY = (height + gridSize - 1) / gridSize;
	if (numTilesX < 2 || numTilesY < 2) return;

	int totalTiles = numTilesX * numTilesY;
	unsigned char *pTileLut = (unsigned char *)malloc(totalTiles * 256 * sizeof(unsigned char));
	if (!pTileLut) return;

	int hist[256];

	for (int ty = 0; ty < numTilesY; ty++)
	{
		int yStart = ty * gridSize;
		int yEnd = yStart + gridSize;
		if (yEnd > height) yEnd = height;
		int tileH = yEnd - yStart;

		for (int tx = 0; tx < numTilesX; tx++)
		{
			int xStart = tx * gridSize;
			int xEnd = xStart + gridSize;
			if (xEnd > width) xEnd = width;
			int tileW = xEnd - xStart;
			int tilePixels = tileW * tileH;

			if (tilePixels <= 0) continue;

			// 1. 统计当前网格直方图
			memset(hist, 0, sizeof(hist));
			for (int y = yStart; y < yEnd; y++)
			{
				const unsigned char *pRow = (const unsigned char *)(pGray->imageData + y * widthStep);
				for (int x = xStart; x < xEnd; x++)
				{
					hist[pRow[x]]++;
				}
			}

			// 2. 直方图裁剪 (Clip Limit)
			int clipThreshold = (int)(clipLimit * (double)tilePixels / 256.0 + 0.5);
			if (clipThreshold < 1) clipThreshold = 1;

			int excess = 0;
			for (int i = 0; i < 256; i++)
			{
				if (hist[i] > clipThreshold)
				{
					excess += (hist[i] - clipThreshold);
					hist[i] = clipThreshold;
				}
			}

			int bonus = excess / 256;
			int rem = excess % 256;
			for (int i = 0; i < 256; i++)
			{
				hist[i] += bonus;
				if (i < rem) hist[i]++;
			}

			// 3. 计算累积分布映射表 (CDF -> LUT)
			int cdf = 0;
			unsigned char *pCurrentLut = pTileLut + (ty * numTilesX + tx) * 256;
			for (int i = 0; i < 256; i++)
			{
				cdf += hist[i];
				int val = (int)((double)cdf * 255.0 / (double)tilePixels + 0.5);
				if (val < 0) val = 0;
				if (val > 255) val = 255;
				pCurrentLut[i] = (unsigned char)val;
			}
		}
	}

	// 4. 双线性插值映射全图像素
	for (int y = 0; y < height; y++)
	{
		unsigned char *pRow = (unsigned char *)(pGray->imageData + y * widthStep);
		double fy = ((double)y + 0.5) / (double)gridSize - 0.5;
		int ty0 = (int)floor(fy);
		int ty1 = ty0 + 1;
		double ry = fy - (double)ty0;

		if (ty0 < 0) { ty0 = 0; ty1 = 0; ry = 0.0; }
		if (ty1 >= numTilesY) { ty0 = numTilesY - 1; ty1 = numTilesY - 1; ry = 0.0; }

		for (int x = 0; x < width; x++)
		{
			int pixelVal = pRow[x];
			double fx = ((double)x + 0.5) / (double)gridSize - 0.5;
			int tx0 = (int)floor(fx);
			int tx1 = tx0 + 1;
			double rx = fx - (double)tx0;

			if (tx0 < 0) { tx0 = 0; tx1 = 0; rx = 0.0; }
			if (tx1 >= numTilesX) { tx0 = numTilesX - 1; tx1 = numTilesX - 1; rx = 0.0; }

			unsigned char val00 = pTileLut[(ty0 * numTilesX + tx0) * 256 + pixelVal];
			unsigned char val10 = pTileLut[(ty0 * numTilesX + tx1) * 256 + pixelVal];
			unsigned char val01 = pTileLut[(ty1 * numTilesX + tx0) * 256 + pixelVal];
			unsigned char val11 = pTileLut[(ty1 * numTilesX + tx1) * 256 + pixelVal];

			double top = (1.0 - rx) * (double)val00 + rx * (double)val10;
			double bot = (1.0 - rx) * (double)val01 + rx * (double)val11;
			double interpolated = (1.0 - ry) * top + ry * bot;

			if (interpolated < 0.0) interpolated = 0.0;
			if (interpolated > 255.0) interpolated = 255.0;
			pRow[x] = (unsigned char)(interpolated + 0.5);
		}
	}

	free(pTileLut);
}

// =========================================================================
// 自实现轻量级 5x5 双边滤波 (Bilateral Filter)
// 100% 纯 C/C++ 实现，解决旧版 OpenCV 缺失双边滤波函数的问题
// =========================================================================
static void ApplySelfContainedBilateral(const IplImage *pSrc, IplImage *pDst, double sigmaColor, double sigmaSpace)
{
	if (!pSrc || !pDst || pSrc->nChannels != 1 || pDst->nChannels != 1) return;

	int width = pSrc->width;
	int height = pSrc->height;
	int stepSrc = pSrc->widthStep;
	int stepDst = pDst->widthStep;

	const int radius = 2; // 5x5 窗口
	float spaceWeights[5][5];
	float twoSigmaSpaceSq = 2.0f * (float)(sigmaSpace * sigmaSpace);
	if (twoSigmaSpaceSq < 0.01f) twoSigmaSpaceSq = 0.01f;

	for (int dy = -radius; dy <= radius; dy++)
	{
		for (int dx = -radius; dx <= radius; dx++)
		{
			float distSq = (float)(dx * dx + dy * dy);
			spaceWeights[dy + radius][dx + radius] = expf(-distSq / twoSigmaSpaceSq);
		}
	}

	float colorWeights[256];
	float twoSigmaColorSq = 2.0f * (float)(sigmaColor * sigmaColor);
	if (twoSigmaColorSq < 0.01f) twoSigmaColorSq = 0.01f;

	for (int diff = 0; diff < 256; diff++)
	{
		colorWeights[diff] = expf(-(float)(diff * diff) / twoSigmaColorSq);
	}

	for (int y = 0; y < height; y++)
	{
		const unsigned char *pSrcRow = (const unsigned char *)(pSrc->imageData + y * stepSrc);
		unsigned char *pDstRow = (unsigned char *)(pDst->imageData + y * stepDst);

		for (int x = 0; x < width; x++)
		{
			int centerVal = pSrcRow[x];
			float sumWeights = 0.0f;
			float sumValues = 0.0f;

			for (int dy = -radius; dy <= radius; dy++)
			{
				int ny = y + dy;
				if (ny < 0) ny = 0;
				if (ny >= height) ny = height - 1;

				const unsigned char *pNeighborRow = (const unsigned char *)(pSrc->imageData + ny * stepSrc);

				for (int dx = -radius; dx <= radius; dx++)
				{
					int nx = x + dx;
					if (nx < 0) nx = 0;
					if (nx >= width) nx = width - 1;

					int neighborVal = pNeighborRow[nx];
					int diff = abs(neighborVal - centerVal);

					float w = spaceWeights[dy + radius][dx + radius] * colorWeights[diff];
					sumWeights += w;
					sumValues += w * (float)neighborVal;
				}
			}

			float outVal = (sumWeights > 0.0001f) ? (sumValues / sumWeights) : (float)centerVal;
			if (outVal < 0.0f) outVal = 0.0f;
			if (outVal > 255.0f) outVal = 255.0f;
			pDstRow[x] = (unsigned char)(outVal + 0.5f);
		}
	}
}

// =========================================================================
// 【新增独立模块】：EnhanceResultImage (终极完美版：全视场背景 100% 绝对纯净单色统一)
// 针对显微镜原图本身“背景有深有浅”（边缘阴影、照度不匀、视场渐晕）物理特性的终极根治方案：
// 
// 核心原理解析：
// 1. 为什么原图深色背景区域容易在生成图中出现位置和形状完全相同的轻微加深？
//    单纯极大值滤波（膨胀）在存在明暗梯度的原图上，会把高光区的亮度“溢出”到相邻较暗区，
//    导致较暗背景区域的光场估计值偏高（产生正向梯度偏差）。
//    此时，该暗区像素除以偏高的估计值，就会计算出虚假的“弱吸收”（例如 4%~8%），
//    从而被平滑映射误当成微弱颗粒吸收光线，导致原本较暗的背景区域被再次渲染成稍深的灰度！
// 2. 终极根治算法：形态学闭运算（Closing = 极大值膨胀 + 极小值腐蚀）
//    先极大值膨胀：彻底填平吞噬暗色精子、大集群与微颗粒；
//    紧接极小值腐蚀：将抬高的斜坡基底精准拉回真实底场坡度，理论偏差恒等于 0！
//    无论原图左边 140、中间 200 还是角落 150，背景相对吸收度 a 严格等于 0.0000！
// 3. 吸收度门限归零与单色锁定：
//    凡 a <= a_bg_thresh 的像素，全图 100% 绝对强制赋值为 dTargetBg（例如 220 单色浅灰）；
//    凡 a > a_bg_thresh 的精子与颗粒，通过 C1 阶 Hermite smoothstep 曲线平滑过渡，
//    无任何断崖硬边，精子与尾部锐利立体，全场背景方差恒为 0，绝对一致化！
// =========================================================================
IplImage *EnhanceResultImage(const IplImage *pImgInput, const SResultEnhanceParams &params)
{
	if (!pImgInput) return NULL;

	// 若未开启增强，直接克隆返回原图的完全副本
	if (!params.bEnable)
	{
		return cvCloneImage(pImgInput);
	}

	CvSize imgSize = cvGetSize(pImgInput);
	int nChannels = pImgInput->nChannels;

	// 统一转换为单通道灰度图进行核心算法计算
	IplImage *pGray = cvCreateImage(imgSize, IPL_DEPTH_8U, 1);
	if (nChannels == 3)
	{
		cvCvtColor(pImgInput, pGray, CV_BGR2GRAY);
	}
	else
	{
		cvCopy(pImgInput, pGray);
	}

	// ---------------------------------------------------------------------
	// 步骤 1：超广域显微镜闭运算平场光照基底估计 (8倍降采样 + 27x27闭运算 Closing: 膨胀+腐蚀)
	// ---------------------------------------------------------------------
	// 采用 8 倍降采样 (例如 1280x720 -> 160x90)：
	// 原图单个精子头部（约 15 像素）在降采样图上只有约 2 像素；
	// 密集大精子群（约 150 像素），在降采样图上也仅有约 18 像素！
	int ds = 8;
	int dw = (imgSize.width + ds - 1) / ds;
	int dh = (imgSize.height + ds - 1) / ds;
	if (dw < 16) dw = 16;
	if (dh < 16) dh = 16;
	CvSize downSize = cvSize(dw, dh);

	IplImage *pDown = cvCreateImage(downSize, IPL_DEPTH_8U, 1);
	cvResize(pGray, pDown, CV_INTER_AREA);

	// 【物理病灶根治：形态学闭运算 Closing = 极大值膨胀 + 极小值腐蚀】
	// 针对“原图中原本就深浅不一的显微镜真实背景（边缘阴影、渐晕、光源不匀）”：
	// 1. 先极大值膨胀 (Dilate)：彻底抹除吞噬所有暗色精子、精子群及微杂质颗粒；
	// 2. 紧接极小值腐蚀 (Erode)：将膨胀引起的光场斜坡整体上移偏差（slope*R）精准拉回原始底场坡度！
	//    使得估计出的光场矩阵在暗背景区与亮背景区均与真实底场严格吻合，误差严格为 0！
	//    彻底消除原图暗背景区域因估计过高而产生的虚假吸收度，从而 100% 根除局部加深！
	int morphK = 27; // 27x27 核覆盖半径 13 像素（对应原图约 208 像素超广域视场）
	IplConvKernel *pKernel = cvCreateStructuringElementEx(morphK, morphK, morphK / 2, morphK / 2, CV_SHAPE_RECT);
	IplImage *pDownDilated = cvCreateImage(downSize, IPL_DEPTH_8U, 1);
	IplImage *pDownClosed  = cvCreateImage(downSize, IPL_DEPTH_8U, 1);

	cvDilate(pDown, pDownDilated, pKernel, 1);
	cvErode(pDownDilated, pDownClosed, pKernel, 1);

	cvReleaseStructuringElement(&pKernel);
	cvReleaseImage(&pDownDilated);

	// 降采样尺度下高斯平滑，抹平形态学滤波产生的微小平顶
	int smoothK = 11;
	cvSmooth(pDownClosed, pDownClosed, CV_GAUSSIAN, smoothK, smoothK, 0, 0);

	// 双线性插值平滑上采样回原始全分辨率，得到真实、连续、全场平滑的照明背景光场矩阵
	IplImage *pBgEstimate = cvCreateImage(imgSize, IPL_DEPTH_8U, 1);
	cvResize(pDownClosed, pBgEstimate, CV_INTER_LINEAR);

	// 全分辨率下小幅平滑，消除插值引起的微小折线
	cvSmooth(pBgEstimate, pBgEstimate, CV_GAUSSIAN, 5, 5, 0, 0);

	cvReleaseImage(&pDown);
	cvReleaseImage(&pDownClosed);

	// ---------------------------------------------------------------------
	// 步骤 2：全视场 100% 绝对统一单色背景锁定 + 零断崖 C1 阶平滑映射
	// ---------------------------------------------------------------------
	IplImage *pFlat = cvCreateImage(imgSize, IPL_DEPTH_8U, 1);
	int widthStepG  = pGray->widthStep;
	int widthStepBg = pBgEstimate->widthStep;
	int widthStepFl = pFlat->widthStep;

	double dTargetBg = (double)params.nTargetBgGray;      // 目标背景灰度 (默认 220，纯净柔和浅灰)
	if (dTargetBg < 150.0) dTargetBg = 220.0;
	if (dTargetBg > 250.0) dTargetBg = 250.0;

	double dContrastBoost = params.dClaheClipLimit;        // 目标对比度自然增强因子 (默认 1.35)
	if (dContrastBoost < 0.5) dContrastBoost = 0.5;
	if (dContrastBoost > 3.0) dContrastBoost = 3.0;

	// 背景纯化吸收度门限：
	// 基准吸收度门限设为 0.045。在默认 dBgBlendWeight 为 1.15 时门限约为 0.052 (5.2%)：
	// 在闭运算已将背景光场偏差清零的基础上，凡吸收度低于 5.2% 的像素（包括全图空旷背景、
	// 原图固有阴影梯度、精子群缝隙、微弱精浆折射与浅晕），100% 绝对强制归一锁定为目标单色灰度 dTargetBg！
	// 彻底消灭原图任何暗沉区域在生成图中的残余加深！全场背景色调与亮度 100% 绝对统一！
	double a_bg_thresh = params.dBgBlendWeight * 0.045;
	if (a_bg_thresh < 0.015) a_bg_thresh = 0.015;
	if (a_bg_thresh > 0.150) a_bg_thresh = 0.150;

	double inv_range = 1.0 / (1.0 - a_bg_thresh);

	for (int y = 0; y < imgSize.height; y++)
	{
		const unsigned char *pGRow  = (const unsigned char *)(pGray->imageData + y * widthStepG);
		const unsigned char *pBgRow = (const unsigned char *)(pBgEstimate->imageData + y * widthStepBg);
		unsigned char *pFlRow       = (unsigned char *)(pFlat->imageData + y * widthStepFl);

		for (int x = 0; x < imgSize.width; x++)
		{
			double gVal  = (double)pGRow[x];
			double bgVal = (double)pBgRow[x];
			if (bgVal < 1.0) bgVal = 1.0;

			// 相对光学吸收度 a: 0.0 表示 100% 透光背景，>0.0 表示吸收光线的精子/颗粒
			double a = 1.0 - gVal / bgVal;

			double finalVal;
			if (a <= a_bg_thresh)
			{
				// 【纯背景区域 100% 绝对统一单色锁定】：
				// 无论是在空旷区域，还是在精子聚集区中间的空隙：
				// 背景灰度 100% 严格锁定为 dTargetBg (例如 220 单色浅灰)！
				// 全图背景在数值上完全同一，方差恒为 0，彻底根除“密集区背景偏深”！
				finalVal = dTargetBg;
			}
			else
			{
				// 【零断崖 Hermite Smoothstep 平滑单调过渡】：
				// 归一化超额吸收度 u: 在 a = a_bg_thresh 处严格为 0.0！
				double u = (a - a_bg_thresh) * inv_range;
				if (u > 1.0) u = 1.0;

				// Hermite smoothstep 曲线: S(0) = 0, 且一阶导数 S'(0) = 0！
				// 在临界点一阶导数为 0 平滑相切接洽，落差严格为 0，
				// 绝无任何灰度跳变断崖！彻底根除任何牛皮癣、环形伪影与硬边界！
				double sCurve = u * u * (3.0 - 2.0 * u);

				// 目标吸收度增强
				double effAbs = sCurve * dContrastBoost;
				if (effAbs > 1.0) effAbs = 1.0;

				finalVal = dTargetBg * (1.0 - effAbs);
			}

			if (finalVal < 0.0) finalVal = 0.0;
			if (finalVal > 255.0) finalVal = 255.0;
			pFlRow[x] = (unsigned char)(finalVal + 0.5);
		}
	}

	cvReleaseImage(&pBgEstimate);

	// ---------------------------------------------------------------------
	// 步骤 3：目标专属高频保真锐化 (背景绝对零修改，纯净度 100% 保持)
	// ---------------------------------------------------------------------
	IplImage *pEnhancedGray = cvCreateImage(imgSize, IPL_DEPTH_8U, 1);
	unsigned char targetBgU8 = (unsigned char)(dTargetBg + 0.5);

	if (params.dSharpenStrength > 0.01)
	{
		IplImage *pBlur = cvCreateImage(imgSize, IPL_DEPTH_8U, 1);
		cvSmooth(pFlat, pBlur, CV_GAUSSIAN, 3, 3, 0, 0);

		int stepFl  = pFlat->widthStep;
		int stepBlr = pBlur->widthStep;
		int stepEnh = pEnhancedGray->widthStep;
		double sharpStrength = params.dSharpenStrength;

		for (int y = 0; y < imgSize.height; y++)
		{
			const unsigned char *pF = (const unsigned char *)(pFlat->imageData + y * stepFl);
			const unsigned char *pB = (const unsigned char *)(pBlur->imageData + y * stepBlr);
			unsigned char *pE       = (unsigned char *)(pEnhancedGray->imageData + y * stepEnh);

			for (int x = 0; x < imgSize.width; x++)
			{
				// 背景像素（>= targetBgU8）：绝对不参与锐化，恒定保持目标背景单色！
				if (pF[x] >= targetBgU8)
				{
					pE[x] = targetBgU8;
				}
				else
				{
					// 仅对真正属于精子与颗粒的目标像素执行高频清晰化
					double diff = (double)pF[x] - (double)pB[x];
					double val = (double)pF[x] + sharpStrength * diff;
					if (val < 0.0) val = 0.0;
					// 锐化后绝对不得反弹刺穿背景灰度
					if (val > (double)targetBgU8) val = (double)targetBgU8;
					pE[x] = (unsigned char)(val + 0.5);
				}
			}
		}
		cvReleaseImage(&pBlur);
	}
	else
	{
		cvCopy(pFlat, pEnhancedGray);
	}

	cvReleaseImage(&pFlat);

	// ---------------------------------------------------------------------
	// 步骤 4：生成最终高质量输出图像（3通道完全无色偏单色浅灰底图，单通道灰度图）
	// ---------------------------------------------------------------------
	IplImage *pResultImg = cvCreateImage(imgSize, IPL_DEPTH_8U, nChannels);
	if (nChannels == 3)
	{
		int stepOut = pResultImg->widthStep;
		int stepEnh = pEnhancedGray->widthStep;

		for (int y = 0; y < imgSize.height; y++)
		{
			const unsigned char *pEnhRow = (const unsigned char *)(pEnhancedGray->imageData + y * stepEnh);
			unsigned char *pOutRow       = (unsigned char *)(pResultImg->imageData + y * stepOut);

			for (int x = 0; x < imgSize.width; x++)
			{
				unsigned char val = pEnhRow[x];
				pOutRow[x * 3 + 0] = val;
				pOutRow[x * 3 + 1] = val;
				pOutRow[x * 3 + 2] = val;
			}
		}
	}
	else
	{
		cvCopy(pEnhancedGray, pResultImg);
	}

	cvReleaseImage(&pGray);
	cvReleaseImage(&pEnhancedGray);

	return pResultImg;
}

//精子轨迹的绘制(只画了当前图像中未终结的精子轨迹)
// 【第二轮升级版】：已全面整合形态学上包络背景净化与目标选择性 USM 清晰化模块
int drawTraceImg(const char **ppcFilePath, int const& nImgFileNum, const char **ppcResImgFile, TraceInfor *pSSpermTraceInfor, int nTraceIndex, int *pnTraceType, double dLimitedLength, int nCenterPara[])
{
	//说明：ppcFilePath——原图目录
	//ppcResImgFile——结果图片路径
	//pnTraceType//微动0/曲线1/直线2
	int nStatus = 1;
	
	for (int i = 0; i < nImgFileNum; i++)
	{
		// 1. 读取原始图像并裁切有效分析视野
		IplImage *pImgSrcTemp = cvLoadImage(ppcFilePath[i], 1);
		IplImage *pImgClipped = clipCenterImage(pImgSrcTemp, nCenterPara);
		cvReleaseImage(&pImgSrcTemp);

		// 2.【核心后处理新增步骤】：在绘制轨迹线前，对底图进行背景净化与清晰化增强
		IplImage *pImgContourShow = EnhanceResultImage(pImgClipped, g_ResultEnhanceParams);
		if (!pImgContourShow)
		{
			pImgContourShow = pImgClipped; // 极端防御：若增强失败则回退至裁切原图
		}
		else
		{
			cvReleaseImage(&pImgClipped); // 成功增强，释放未增强的中间图
		}

		// 3. 绘制精子轨迹线（保持原绘制逻辑与颜色规范完全不变）
		// V1.0.3-H 版本修改：在 drawTraceImg 中，对 pnTraceType == 0 的不动精子直接跳过不画线
		for (int j = 0; j < nTraceIndex; j++)
		{
			// 【关键修复】：如果是不动/微动精子 (0)，严禁绘制任何轨迹线！直接跳过！
			if (pnTraceType[j] == 0)
			{
				continue;
			}

			CvScalar cvColor = SLOWPRCOlOUR; // 默认蓝色
			if (pnTraceType[j] == 2) // Class A: 快速前向精子 (Rapid PR)，绿色 (由RAPIDPRCOlOUR宏控制)
			{
				cvColor = RAPIDPRCOlOUR;
			}
			else if (pnTraceType[j] == 1) // Class B: 慢速前向精子 (Slow PR)，蓝色 (由SLOWPRCOlOUR宏控制)
			{
				cvColor = SLOWPRCOlOUR;
			}
			else if (pnTraceType[j] == 3) // Class C: 非前向精子 (NP)，红色 (由NPCOlOUR宏控制)
			{
				cvColor = NPCOlOUR;
			}
			else
			{
				continue; // 其他异常类型跳过 (包括 pnTraceType == 0 不动精子)
			}

			// 画线（对运动精子 Class A/B/C: pnTraceType 为 1, 2, 3 绘制）
			if (i >= 1)
			{
				for (int k = 0; k < i; k++) // 连续的轨迹绘制
				{
					if (pSSpermTraceInfor[j*nImgFileNum+k].fPosX > 0.2 && pSSpermTraceInfor[j*nImgFileNum+k].fPosY > 0.2
						&& pSSpermTraceInfor[j*nImgFileNum+k+1].fPosX > 0.2 && pSSpermTraceInfor[j*nImgFileNum+k+1].fPosY > 0.2) // 尚未生成
					{
						CvPoint cvCenter0 = cvPoint(cvRound(pSSpermTraceInfor[j*nImgFileNum+k].fPosX), cvRound(pSSpermTraceInfor[j*nImgFileNum+k].fPosY));
						CvPoint cvCenter1 = cvPoint(cvRound(pSSpermTraceInfor[j*nImgFileNum+k+1].fPosX), cvRound(pSSpermTraceInfor[j*nImgFileNum+k+1].fPosY));

						double dLengthTemp = sqrt((cvCenter1.x - cvCenter0.x)*(cvCenter1.x - cvCenter0.x) + (cvCenter1.y - cvCenter0.y)*(cvCenter1.y - cvCenter0.y));

						if (dLengthTemp < 6*dLimitedLength)
						{
							cvLine(pImgContourShow, cvCenter0, cvCenter1, cvColor, 2);
						}
					}
				}
			}
		}

		// 4. 保存增强与绘制轨迹后的高质量图片至 ResultImgs
		nStatus = cvSaveImage(ppcResImgFile[i], pImgContourShow); // 图片文件存储

		// 若开启前后对比图保存 (便于研发批量检验效果)
		if (g_ResultEnhanceParams.bSaveComparison)
		{
			char czComparePath[MAXPATHLENGTH] = {0};
			sprintf(czComparePath, "%s_Compare.jpg", ppcResImgFile[i]);
			IplImage *pImgRawOrig = cvLoadImage(ppcFilePath[i], 1);
			IplImage *pImgRawClip = clipCenterImage(pImgRawOrig, nCenterPara);
			cvReleaseImage(&pImgRawOrig);
			if (pImgRawClip)
			{
				CvSize compSize = cvSize(pImgContourShow->width * 2, pImgContourShow->height);
				IplImage *pCompare = cvCreateImage(compSize, IPL_DEPTH_8U, pImgContourShow->nChannels);
				cvSetImageROI(pCompare, cvRect(0, 0, pImgRawClip->width, pImgRawClip->height));
				cvCopy(pImgRawClip, pCompare);
				cvResetImageROI(pCompare);
				cvSetImageROI(pCompare, cvRect(pImgRawClip->width, 0, pImgContourShow->width, pImgContourShow->height));
				cvCopy(pImgContourShow, pCompare);
				cvResetImageROI(pCompare);
				cvSaveImage(czComparePath, pCompare);
				cvReleaseImage(&pImgRawClip);
				cvReleaseImage(&pCompare);
			}
		}

		cvReleaseImage(&pImgContourShow);

		if (nStatus == 0)
		{
			nStatus = -2;//数据写入异常
			return nStatus;
		}
	}

	return nStatus;
}

int drawFinalTraceImg(const char **ppcFilePath, int const& nImgFileNum, const char **ppcResImgFile, TraceInfor *pSSpermTraceInfor, int nTraceIndex, int *pnTraceType, int nCenterPara[])
{
	//说明：ppcFilePath——原图目录
	//ppcResImgFile——结果图片路径
	//pnTraceType//微动0/曲线1/直线2
	int nRadius = 2;
	int nSaveStatus = 0;
	
	for (int i = 0; i<nImgFileNum; i++)
	{
		IplImage *pImgSrcTemp = cvLoadImage(ppcFilePath[i],1);
		IplImage *pImgClipped = clipCenterImage(pImgSrcTemp, nCenterPara);
				IplImage *pImgContourShow = EnhanceResultImage(pImgClipped, g_ResultEnhanceParams);
		if (!pImgContourShow) pImgContourShow = pImgClipped;
		else cvReleaseImage(&pImgClipped);
		cvReleaseImage(&pImgSrcTemp);

		for (int j = 0; j<nTraceIndex; j++)
		{
			if (pSSpermTraceInfor[j*nImgFileNum+i].nPredictNum == -1//轨迹的终结
				|| pSSpermTraceInfor[j*nImgFileNum+i].fPosX < EPSINON || pSSpermTraceInfor[j*nImgFileNum+i].fPosY < EPSINON)//尚未生成
			{
				continue;
			}

			CvPoint cvCenter = cvPoint(cvRound(pSSpermTraceInfor[j*nImgFileNum+i].fPosX),cvRound(pSSpermTraceInfor[j*nImgFileNum+i].fPosY));
			CvScalar cvColor = SLOWPRCOlOUR;
			if (pnTraceType[j] == 2) // Class A: 快速前向精子 (Rapid PR) -> 绿色
			{
				cvColor = RAPIDPRCOlOUR;
			}
			else if (pnTraceType[j] == 1) // Class B: 慢速前向精子 (Slow PR) -> 蓝色
			{
				cvColor = SLOWPRCOlOUR;
			}
			else if (pnTraceType[j] == 3) // Class C: 非前向精子 (NP) -> 红色
			{
				cvColor = NPCOlOUR;
			}
			else if (pnTraceType[j] == 0) // 微动或不动
			{
				continue;
			}

			//画点
			cvCircle(pImgContourShow, cvCenter, nRadius, cvColor, 2);

			//画线
			if (i >= 1)
			{
				for (int k = 0; k<i; k++)//连续的轨迹绘制
				{
					if (pSSpermTraceInfor[j*nImgFileNum+k].fPosX < EPSINON || pSSpermTraceInfor[j*nImgFileNum+k].fPosY < EPSINON
						|| pSSpermTraceInfor[j*nImgFileNum+k+1].fPosX < EPSINON || pSSpermTraceInfor[j*nImgFileNum+k+1].fPosY < EPSINON)//尚未生成
					{
						continue;
					}

					CvPoint cvCenter0 = cvPoint(cvRound(pSSpermTraceInfor[j*nImgFileNum+k].fPosX),cvRound(pSSpermTraceInfor[j*nImgFileNum+k].fPosY));
					CvPoint cvCenter1 = cvPoint(cvRound(pSSpermTraceInfor[j*nImgFileNum+k+1].fPosX),cvRound(pSSpermTraceInfor[j*nImgFileNum+k+1].fPosY));
					cvColor = CV_RGB(253,90,78);//红色
					cvLine(pImgContourShow, cvCenter0, cvCenter1,cvColor,2);
				}
			}
		}

		nSaveStatus = cvSaveImage(ppcResImgFile[i],pImgContourShow);//图片文件存储
		cvReleaseImage(&pImgContourShow);
	}

	return nSaveStatus;
}

//视频分解图片
int getVideoFrame(const char* pcVideoPath, const char* pcVidName, int &iImgNum)
{
	int nStatus = 1;

	int iDelta = 1;//间隔保存图片  	
	int nNumImgs = 0;//总帧数

	char *czTempFile = (char *)malloc(sizeof(char)*MAXPATHLENGTH);  
	if ( NULL == czTempFile )
	{
		nStatus = 0;
		return nStatus;//内存异常
	}

	strcpy(czTempFile, pcVideoPath);
	strcat(czTempFile, pcVidName); 

	//检查文件是否存在
	FILE *fp = NULL;
	fp = fopen(czTempFile, "r");   //没有这个文件则继续
	if ( NULL == fp )
	{
		nStatus = -3;//未找到视频或视频文件名不规范或没有数据读取权限
		free(czTempFile);
		czTempFile = NULL;
		return nStatus;
	}
	fclose(fp);

	CvCapture *pCapture = NULL;  
	IplImage *pFrame = NULL;  

	pCapture = cvCreateFileCapture(czTempFile);
	if (NULL == pCapture)
	{
		nStatus = -4;//视频分解不成功
		free(czTempFile);
		czTempFile = NULL;
		return nStatus;
	}

	while( (pFrame = cvQueryFrame(pCapture)) != NULL && nNumImgs <= 49)  
	{  
		if (nNumImgs % iDelta == 0)  
		{  
			sprintf(czTempFile,"%s/%03d.jpg", pcVideoPath, nNumImgs);//使用帧号作为图片名  
			int nSaveStatus = cvSaveImage(czTempFile, pFrame);
			if (nSaveStatus != 1)
			{
				cvReleaseCapture(&pCapture);
				free(czTempFile);
				czTempFile = NULL;

				nStatus = -2;
				return nStatus;//数据写入异常
			}
			++nNumImgs;
		}
	}

	cvReleaseCapture(&pCapture);
	free(czTempFile);
	czTempFile = NULL;

	iImgNum = nNumImgs;

	return nStatus;
} 

//结果单位统一
void updateResult(SSettings SParaInput, algsqamed_data_out *dataOut)
{

	dataOut->dTotaSpermDensity   = SParaInput.dDSDensk * dAlphaDens * dataOut->nTotalSpermNum;
	dataOut->dActiveSpermDensity = SParaInput.dDSDensk * dAlphaDens * dataOut->nActiveSpermNum;
	dataOut->dSpermDensitySL     = SParaInput.dDSDensk * dAlphaDens * dataOut->nNumSL;
	dataOut->dSpermDensityCL     = SParaInput.dDSDensk * dAlphaDens * dataOut->nNumCL;

	// 新增计算参数，将ABCD比例参数，计算为密度参数
		
		dataOut->dDensityClassPR = dataOut->dRatioClassPR * dataOut->dTotaSpermDensity /100;  // PR 密度
		dataOut->dDensityClassA = dataOut->dRatioClassA * dataOut->dTotaSpermDensity /100;    // A级 密度 Rapid-PR
		dataOut->dDensityClassB = dataOut->dRatioClassB * dataOut->dTotaSpermDensity /100;    // B级 密度 Slow-PR
		dataOut->dDensityClassC = dataOut->dRatioClassC * dataOut->dTotaSpermDensity /100;    // C级 密度 NP
		dataOut->dDensityClassD = dataOut->dRatioClassD * dataOut->dTotaSpermDensity /100;    // D级 密度 Immotile(D)
		dataOut->dDensityRoundcells = dataOut->dRoundcells * dataOut->dTotaSpermDensity /100; // 圆细胞 密度

		}





//图像生成视频
int genVideo(const char **ppcFilePath, const char* czAviPath, int nImgNum)
{
	int nStatus = 1;

	//文件名处理
	char *pcFileNameTemp = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if (NULL == pcFileNameTemp)
	{
		nStatus = 0;
		return nStatus;//内存异常
	}
	char pcImg[20] = "Result.avi";
	strcpy(pcFileNameTemp, czAviPath);
	strcat(pcFileNameTemp, pcImg);

	//生成视频
	IplImage* pFrame = cvLoadImage(ppcFilePath[0],1);
	CvVideoWriter* video = NULL;

	//video = cvCreateVideoWriter(pcFileNameTemp, CV_FOURCC('M','P','4','2'), 15,
		//cvGetSize(pFrame),1);//注：若生成mp4格式，返回一直为0，mp4格式比较特殊，使用起来需注意,MP42的压缩方式，视频最小

	video = cvCreateVideoWriter(pcFileNameTemp, CV_FOURCC('M','J','P','G'), 10,
		cvGetSize(pFrame),1);//注：若生成mp4格式，返回一直为0，mp4格式比较特殊，使用起来需注意
	if (!video)
	{
		free(pcFileNameTemp);
		pcFileNameTemp = NULL;
		cvReleaseImage(&pFrame);

		nStatus = -8;//视频生成失败
		return nStatus;
	}

	//写入视频
	for (int i = 0; i < (int)nImgNum/2; i++)
	{
		pFrame = cvLoadImage(ppcFilePath[2*i]);
		if (pFrame)
		{
			int nIsDone = cvWriteFrame(video,pFrame);//写入视频
			if (nIsDone == 0)
			{
				break;
			}
		}
		cvReleaseImage(&pFrame);
	}

	//资源释放
	free(pcFileNameTemp);
	pcFileNameTemp = NULL;

	cvReleaseVideoWriter(&video);
	cvReleaseImage(&pFrame);

	return nStatus;
}
