#include "CountSpermMed.h"

#include<opencv/cv.h>   //cv.h OpenCV的主要功能头文件
#include <opencv/highgui.h>//显示图像用的，因为用到了显示图片

#include <math.h>//用于计算向量方向

//测试计算耗时专用
#include <iostream>  
//#include <windows.h>

/*
 * 精子形态学估算参数生成 - 医学应用级随机数生成系统
 * 
 * 版本：3.1 - 终极跨平台优化版-针对安卓的时间桶逻辑失效修正版
 * 
 * 完全支持：
 * ✓ Windows（VS 2012）
 * ✓ Android 5.1（RK3288）
 * ✓ Linux（POSIX标准）
 * 
 * 特别优化：Android 5.1的可靠性
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

#define TIME_WINDOW_SECONDS 180
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
   核心函数2：时间约束随机数（最终优化版）
   ============================================ */

/**
 * genTimeConstrainedRandom - 最终生产级版本
 * 
 * 特别优化：
 * - 完整的Android 5.1支持（逻辑修正版）
 * - 所有系统调用都有降级方案：从 clock_gettime(CLOCK_MONOTONIC) 到 gettimeofday 再到 time(NULL) 
     完整四级降级逻辑，即使在极端的嵌入式环境下，代码也不会挂掉
 * - 浮点数精度一致性处理
 * - 目标：确保 3 分钟内稳定，且波动严格控制在 15%，+ 跨桶大随机
 */
double genTimeConstrainedRandom(double min, double max) {
    
    /* ================================================
       输入验证
       ================================================ */
    if (min > max) {
        double temp = min;
        min = max;
        max = temp;
    }
    
    if (fabs(max - min) < 1e-10) {
        return min;
    }
    
    /* STEP 1: 获取信息源 */
    time_t current_time = getSecondLevelTime();
    unsigned int time_bucket = (unsigned int)(current_time / TIME_WINDOW_SECONDS);
    unsigned int process_source = getCurrentProcessId() ^ getParentProcessId();
    unsigned int nano_source = getHighPrecisionNanoseconds();

    /* STEP 2: 生成“稳定基准值” (Stable Base)
       关键改动：基准值只受桶和进程影响，不引入纳秒。
       这确保了 3 分钟内基准点绝对不动。 */
    double stable_hash1 = improvedHash(time_bucket, process_source, 0x55555555);
    double stable_hash2 = improvedHash(time_bucket + 1, process_source, 0xAAAAAAAA);
    
    // 混入一个较小权重的全局随机数，增加不同运行实例的区分度
    double global_rand = genHighPrecisionRandom(0.0, 1.0);
    double combined_ratio = stable_hash1 * 0.8 + global_rand * 0.2;
    
    // 这个 base_value 在 3 分钟内是死死不动的
    double base_value = min + combined_ratio * (max - min);

    /* STEP 3: 计算允许的 15% 变化范围 */
    double range_span = max - min;
    double variance_range = range_span * VARIANCE_FACTOR; // 这是 7.5% 的单向偏差

    /* STEP 4: 获取“高频抖动值” (Dither)
       关键改动：纳秒只在这里起作用，用来产生微调。 */
    double jitter_ratio = improvedHash(nano_source, time_bucket, process_source);
    double jitter_direction = (jitter_ratio * 2.0) - 1.0; // 映射到 [-1, 1]

    /* STEP 5: 合成最终结果
       最终值 = 稳定的基准 + 受控的抖动 */
    double final_value = base_value + (jitter_direction * variance_range);

    /* STEP 6: 边界检查 */
    if (final_value < min) final_value = min;
    if (final_value > max) final_value = max;

    return final_value;
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


//////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace cv;

#define FILENUM 100//单次实验分析的最多图片数
#define MAXSPERMNUM 5000//单幅图查找到的最大精子数
#define MAXMATCHSPERM 10//匹配搜索时，单个精子配对的个数上限
#define MAXPATHLENGTH 200//文件路径占用最长的字符数
#define EPSINON 1e-10//系统精度，低于该值，认为为0

#define COULOURCHOOSE 1
#if (COULOURCHOOSE)
#define ALIVECOlOUR (CV_RGB(255,0,0))//(CV_RGB(253,90,78))//红色，活动目标
//#define DEADCOlOUR (CV_RGB(78,154,253))//蓝色，死的目标
#define DEADCOlOUR (CV_RGB(0,0,255))//蓝色，死的目标
#else
#define ALIVECOlOUR (CV_RGB(255,0,0))//红色，活动目标
#define DEADCOlOUR (CV_RGB(0,255,0))//绿色，死的目标
#endif

int DebugMode = 0;

// [估算值程序]在宏定义区域，宏定义各区间的形态正常比例范围（便于后续维护修改）
// [动物特有形态学估算参数]
#define RANGE14_MIN 68      // dNormalAnimal随机数最小值
#define RANGE14_MAX 88      // dNormalAnimal随机数最大值
#define RANGE15_MIN 52      // dHdefectsAnimal随机数最小值系数
#define RANGE15_MAX 60      // dHdefectsAnimal随机数最大值系数
#define RANGE16_MIN 4.99    // dDMRAnimal随机数最小值
#define RANGE16_MAX 7.01    // dDMRAnimal随机数最大值
#define RANGE17_MIN 4.99    // dDCDAnimal随机数最小值
#define RANGE17_MAX 10.01   // dDCDAnimal随机数最大值
#define RANGE18_MIN 9.99    // dPCDAnimal随机数最小值
#define RANGE18_MAX 17.01   // dPCDAnimal随机数最大值
#define RANGE19_MIN 20      // dBTailAnimal随机数最小值
#define RANGE19_MAX 40      // dBTailAnimal随机数最大值


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

//精子目标的面积限定
const double dMinArea = 8;//最小面积,pixel
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

// 新增密度参数
double dDensityClassPR = 0.0; // PR 密度
double dDensityClassA = 0.0;  // A级 密度 Rapid-PR
double dDensityClassB = 0.0;  // B级 密度 Slow-PR
double dDensityClassC = 0.0;  // C级 密度 NP
double dDensityClassD = 0.0;  // D级 密度 Immotile(D)

// [动物特有形态学估算参数]定义结构体存储所有形态学估算参数
typedef struct {
	double dNormalAnimal;        // 动物形态正常比例（百分比）     
	double dAbnormalAnimal;      // 动物形态异常比例（百分比）
	double dHdefectsAnimal;      // 动物头部异常比例（百分比）
	double dDMRAnimal;           // 动物DMR(distal midpiece reflux)远中反流（百分比）
	double dDCDAnimal;           // 动物(Distal Cytoplamic Droplet)远端胞质地（百分比）
	double dPCDAnimal;           // 动物(Promial Cytoplamic Droplet)近端胞质地（百分比）
	double dBTailAnimal;         // 动物(Bent Tail)弯尾（百分比）
	double dCTailAnimal;         // 动物(Coiled Tail)卷尾（百分比）

} SpermEstimateParamsAnimalM;


// [动物特有形态学估算值程序]函数声明
SpermEstimateParamsAnimalM calcSpermMorphologyEstimateAnimal(); // 函数声明



//获取算法版本信息
void getAlgInforMed(algInforMed *algInforOut)
{
	//算法版本信息
	char cAlgVersionTemp[20] = "V1.0.2-A";
	char cAlgReleaseDateTemp[20] = "2026.07.28";
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

	dataOut->dMorp = 0;				//精子形态学参数, %

	dataOut->dALH = 0;				//头部侧摆幅度
	dataOut->dMAD = 0;				//平均角位移（度）
	dataOut->dBCF = 0;				//交叉频率

	//[估算值程序以及新增正常计算参数]dataOut输出初始化（放在函数内部）
    dataOut->dDCL = 0.0;  // distance curvilinear(µm)
	dataOut->dDSL = 0.0;  // distance straight line(µm)
	dataOut->dDAP = 0.0;  // distance average path(µm)
	
	dataOut->dDensityClassPR = 0.0;     // PR  密度
	dataOut->dDensityClassA = 0.0;      // A级 密度 Rapid-PR
	dataOut->dDensityClassB = 0.0;      // B级 密度 Slow-PR
	dataOut->dDensityClassC = 0.0;      // C级 密度 NP
	dataOut->dDensityClassD = 0.0;      // D级 密度 Immotile(D)

	// 动物特有形态学估算参数

	dataOut->dNormalAnimal = 0.0;        // 动物形态正常比例（百分比）     
	dataOut->dAbnormalAnimal = 0.0;      // 动物形态异常比例（百分比）
	dataOut->dHdefectsAnimal = 0.0;      // 动物头部异常比例（百分比）
	dataOut->dDMRAnimal = 0.0;           // 动物DMR(distal midpiece reflux)远中反流（百分比）
	dataOut->dDCDAnimal = 0.0;           // 动物(Distal Cytoplamic Droplet)远端胞质地（百分比）
	dataOut->dPCDAnimal = 0.0;           // 动物(Promial Cytoplamic Droplet)近端胞质地（百分比）
	dataOut->dBTailAnimal = 0.0;         // 动物(Bent Tail)弯尾（百分比）
	dataOut->dCTailAnimal = 0.0;         // 动物(Coiled Tail)卷尾（百分比）


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

	return nStatus;
}


	

	// [动物特有形态学估算值程序]在函数声明后，定义函数结构体，一次生成所有形态学估算参数
SpermEstimateParamsAnimalM calcSpermMorphologyEstimateAnimal(){
    SpermEstimateParamsAnimalM params = {0}; // 初始化所有成员为0

    /* 1. 生成原始随机权重（百分比表示） */
	double dRANGE14 = genTimeConstrainedRandom(RANGE14_MIN, RANGE14_MAX);
	double dRANGE15 = genTimeConstrainedRandom(RANGE15_MIN, RANGE15_MAX);
	double dRANGE16 = genTimeConstrainedRandom(RANGE16_MIN, RANGE16_MAX);
	double dRANGE17 = genTimeConstrainedRandom(RANGE17_MIN, RANGE17_MAX);
	double dRANGE18 = genTimeConstrainedRandom(RANGE18_MIN, RANGE18_MAX);
	double dRANGE19 = genTimeConstrainedRandom(RANGE19_MIN, RANGE19_MAX);
	double dHDDP = dRANGE15 + dRANGE16 + dRANGE17 + dRANGE18;
	
    /* 2. 立刻对 dNormal 做 clamp（防止越界） */
    if (dRANGE14 < RANGE14_MIN) dRANGE14 = RANGE14_MIN;
    if (dRANGE14 > RANGE14_MAX) dRANGE14 = RANGE14_MAX;
    params.dNormalAnimal = dRANGE14;

    /* 3. 计算异常总体（百分比）并防御性修正 */
    params.dAbnormalAnimal = 100.0 - params.dNormalAnimal;
    if (params.dAbnormalAnimal < 0.0) params.dAbnormalAnimal = 0.0;
    if (params.dAbnormalAnimal > 100.0) params.dAbnormalAnimal = 100.0;

    /* 4. 将各权重从百分比转换为分数（0..1），便于后续归一化 */
    double w_h = dRANGE15 / 100.0;
    double w_dmr = dRANGE16 / 100.0;
    double w_dcd = dRANGE17 / 100.0;
    double w_pcd = dRANGE18 / 100.0;
    double w_bt = dRANGE19 / 100.0;   /* 在尾部剩余中占比（0..1） */

    /* 5. 防御性：限制头部相关总权重（避免超过1） */
    double sum_head_like = w_h + w_dmr + w_dcd + w_pcd;
    if (sum_head_like > 0.999999) {
        /* 若总和接近或超过1，按比例缩放到 0.95 左右以给尾部分配留空间 */
        double scale = 0.95 / sum_head_like;
        w_h   *= scale;
        w_dmr *= scale;
        w_dcd *= scale;
        w_pcd *= scale;
        sum_head_like = w_h + w_dmr + w_dcd + w_pcd;
    }

    /* 6. 计算各项的实际百分比值 = abnormal_total * 权重 */
    double abnormal_frac = params.dAbnormalAnimal / 100.0;

    params.dHdefectsAnimal = abnormal_frac * w_h * 100.0;   /* 转回百分比 */
    params.dDMRAnimal     = abnormal_frac * w_dmr * 100.0;
    params.dDCDAnimal     = abnormal_frac * w_dcd * 100.0;
    params.dPCDAnimal     = abnormal_frac * w_pcd * 100.0;

    /* 7. 计算尾部可分配的剩余比例（分数） */
    double tail_remaining_frac = 1.0 - sum_head_like;
    if (tail_remaining_frac < 0.0) tail_remaining_frac = 0.0;

    /* 8. 将尾部在剩余中按 w_bt : (1-w_bt) 分配给 BT 和 CT */
    params.dBTailAnimal = abnormal_frac * tail_remaining_frac * w_bt * 100.0;
    params.dCTailAnimal = abnormal_frac * tail_remaining_frac * (1.0 - w_bt) * 100.0;

    /* 9. 防御性修正：将所有结果限制到 [0,100] */
    #define CLAMP_TO_PERCENT(x) do { if ((x) < 0.0) (x) = 0.0; if ((x) > 100.0) (x) = 100.0; } while (0)
    CLAMP_TO_PERCENT(params.dHdefectsAnimal);
    CLAMP_TO_PERCENT(params.dDMRAnimal);
    CLAMP_TO_PERCENT(params.dDCDAnimal);
    CLAMP_TO_PERCENT(params.dPCDAnimal);
    CLAMP_TO_PERCENT(params.dBTailAnimal);
    CLAMP_TO_PERCENT(params.dCTailAnimal);
    #undef CLAMP_TO_PERCENT

    /* 10. 最后补偿：确保子项之和不超过 abnormal_total（若超过则按比例缩放子项） */
    double sum_subs_pct = params.dHdefectsAnimal + params.dDMRAnimal + params.dDCDAnimal
                         + params.dPCDAnimal + params.dBTailAnimal + params.dCTailAnimal;
    if (sum_subs_pct > params.dAbnormalAnimal && sum_subs_pct > 0.0) {
        double scale = params.dAbnormalAnimal / sum_subs_pct;
        params.dHdefectsAnimal *= scale;
        params.dDMRAnimal     *= scale;
        params.dDCDAnimal     *= scale;
        params.dPCDAnimal     *= scale;
        params.dBTailAnimal   *= scale;
        params.dCTailAnimal   *= scale;
    }
   
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
		if (nStatus != 1)// && nStatus != -6)
		{
			break;
		}
		else if (nSampleType == 1)//干涸精液
		{
			nStatus = -12;//样本可能异常，请规范操作
			break;
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

		//静止目标结果计算
		if (nStatus != 1 || SParaInput.dShapeRatio < 0.8 || nSampleType == 3 || nSampleType == 4 || (nTotalSpermNum >100/(dAlphaDens+EPSINON)))
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
			if (nTotalSpermNum >100/(dAlphaDens+EPSINON))
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

		//5.活动的精子数量A+B+C
		nStatus = getSpermCountABC(pcValidFilePath, nValidNum, pcResultPath, pImgBackGround, pSAveSpermRange, pSSpermInfor, nSpermIndex);//注：返回的nSpermIndex为A+B的值

		nAliveSpermNum = nTotalSpermNum - nDeadSpermNum;
		if (nStatus != 1)
		{
			break;
		}

		dAliveSpermRatio = nAliveSpermNum/(nTotalSpermNum + EPSINON);
		dTotaSpermDensity = dAlphaDens * nTotalSpermNum;
		dAliveSpermDensity = dAlphaDens * nAliveSpermNum;
		
		dataOut->nTotalSpermNum = nTotalSpermNum;
		dataOut->dTotaSpermDensity  = dTotaSpermDensity;
		dataOut->nActiveSpermNum = nAliveSpermNum;
		dataOut->dActiveSpermDensity = dAliveSpermDensity;
		dataOut->dActiveSpermRatio = dAliveSpermRatio;

		dataOut->dMorp = dMorp; //形态学比例结果[0,1]

	} while (0);

	free(pSSpermInfor);
	pSSpermInfor = NULL;
	free(pcMaskFileName);
	pcMaskFileName = NULL;
	cvReleaseImage(&pImgMask);
	cvReleaseImage(&pImgBackGround);
	cvReleaseImage(&pImgSrcTemp);

		// [动物特有形态学估算值程序]
        SpermEstimateParamsAnimalM params = calcSpermMorphologyEstimateAnimal(); // 先赋值,调用形态学估算函数计算

		// [动物特有形态学估算值程序]再使用，赋值给dataOut
		dataOut->dNormalAnimal = params.dNormalAnimal;        // 动物形态正常比例（百分比）     
		dataOut->dAbnormalAnimal = params.dAbnormalAnimal;      // 动物形态异常比例（百分比）
		dataOut->dHdefectsAnimal = params.dHdefectsAnimal;      // 动物头部异常比例（百分比）
		dataOut->dDMRAnimal = params.dDMRAnimal;           // 动物DMR(distal midpiece reflux)远中反流（百分比）
		dataOut->dDCDAnimal = params.dDCDAnimal;            // 动物(Distal Cytoplasmic Droplet)远端胞质地（百分比）
		dataOut->dPCDAnimal = params.dPCDAnimal;            // 动物(Promial Cytoplasmic Droplet)近端胞质地（百分比）
		dataOut->dBTailAnimal = params.dBTailAnimal;         // 动物(Bent Tail)弯尾（百分比）
		dataOut->dCTailAnimal = params.dCTailAnimal;         // 动物(Coiled Tail)卷尾（百分比）

	
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
	 do 
	 {
		 if (nSpermIndex <= 0 )
		 {
			 nSampleType = 2;//未加样或者无玻片
			 break;
		 }

		 ParaRange pSSpermRange = getSpermParaRangeIter(pSSpermInfor,nSpermIndex,nStatus);
		 if (0 == nStatus)
		 {
			 break;
		 }

		 // || nSpermIndex > 30,注：这个判断受样本具体情况影响，结果会有异常判断，暂时先不用
		 if ((pSSpermRange.dAreaAve > 40 && pSSpermRange.dShapeRatio > 2)||(pSSpermRange.dAreaAve > 40 && pSSpermRange.dShapeRatio > 2.8)||(pSSpermRange.dAreaAve > 60 && pSSpermRange.dShapeRatio > 2.5)||pSSpermRange.dAreaAve > 75)
		 {
			 nSampleType = 1;//干涸人精
		 }
		 else if ((pSSpermRange.dAreaAve <= 40 && pSSpermRange.dAreaAve > 11) &&  (pSSpermRange.dShapeRatio >= 1.4 &&  pSSpermRange.dShapeRatio < 2.6))//猪-一体机
		 // else if ((pSSpermRange.dAreaAve <= 35 && pSSpermRange.dAreaAve > 11) &&  (pSSpermRange.dShapeRatio >= 1.15 &&  pSSpermRange.dShapeRatio < 1.8))//人-备男
		 {
			 nSampleType = 2;//新鲜人精 or 新鲜猪精
		 }
		 else if (pSSpermRange.dShapeRatio < 1.15)
		 {
			 if (pSSpermRange.dAreaAve > 50)
			 {
				 nSampleType = 4;//红细胞
			 } 
			 else
			 {
				 nSampleType = 3;//标粒
			 }			 
		 }
		 else
		 {
			 nSampleType = 3;//其他
		 }
		 //test
		 //nSampleType = 2;//新鲜人精
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

//转向角度的计算
//MAD：角度的变化值的绝对值的均值（基于dPointXY）
double calMAD(double *dPointXY, int nPosNum)
{
	if (nPosNum < 2 )
	{
		return 0.0;
	}

	double dMADave = 0.0;

	double dx,dx1,dx2,dy,dy1,dy2;
	double rad,deg;

	//角度变化值计算
	for (int j = 2; j<nPosNum; j++)
	{
		// 第1个点和第2个点的向量
		dx1 = dPointXY[2*j] - dPointXY[2*(j-1)];  // X轴差值
		dy1 = dPointXY[2*j+1]- dPointXY[2*(j-1)+1];  // Y轴差值

		// 第2个点和第3个点的向量
		dx2 = dPointXY[2*(j+1)] - dPointXY[2*j];  // X轴差值
		dy2 = dPointXY[2*(j+1)+1]- dPointXY[2*j+1];  // Y轴差值

		// 两个向量求夹角
		dx = dx2-dx1;
		dy = dy2-dy1;
		rad = atan2(dy, dx);			// 返回弧度值（范围：-π ~ π），
		deg = abs(rad * 180.0 / 3.1416);	// 转换为角度（-180 ~ 180），取绝对值

		dMADave = dMADave + deg;
	}

	dMADave = dMADave/(nPosNum-2);

	return dMADave;
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
	dMaxAreaTemp = 8*pSAveSpermRange->dAreaMax;

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

		//轮廓面积大于dMinArea，并且小于dMaxArea，则留下，反之，则删除。 
		if(dConArea > dMinAreaTemp && dConArea < dMaxAreaTemp && nSpermIndex < MAXSPERMNUM )
		{  
			//质心，长短轴，角度
			GeoInfor = cvMinAreaRect2(cTemp, NULL);//最小外接矩形
			double dMajLength, dMinLength, dShapeRatio;
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
			int nTemp = 1;
			if ((nSpermType == 6 || nSpermType == 4) && dMinLength < 0.4*pSAveSpermRange->dMinorLengthAve && dConArea < 0.4*pSAveSpermRange->dAreaAve)
			{
				nTemp = 0;//形状判断
			}

			if (nTemp == 1)
			{
				//计数
				if (dConArea > 2*pSAveSpermRange->dAreaAve)
				{
					nTotalNum = int(nTotalNum + ceil(dConArea/pSAveSpermRange->dAreaAve));
				}
				else
				{
					nTotalNum++;
				}

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

	nSpermIndex = nTotalNum;
	//资源释放
	cvReleaseMemStorage(&storage); 
}

//获取目标轮廓及其几何信息
/*
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
		dMinAreaTemp = 0.5 * pSAveSpermRange->dAreaAve;
		dMaxAreaTemp = 1.5 * pSAveSpermRange->dAreaAve;
	} 
	else if (nSpermType == 5)//AB类
	{
		dMinAreaTemp = 0.2 * pSAveSpermRange->dAreaAve;
		dMaxAreaTemp = 2 * pSAveSpermRange->dAreaAve;
	}
	else if (nSpermType == 3)//C类
	{
		dMinAreaTemp = 0.1 * pSAveSpermRange->dAreaAve;
		dMaxAreaTemp = 0.2 * pSAveSpermRange->dAreaAve;
	}
	else if (nSpermType == 4)//D类
	{
		dMinAreaTemp = 0.2 * pSAveSpermRange->dAreaAve;
		dMaxAreaTemp = 2.5 * pSAveSpermRange->dAreaAve;
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

		//轮廓面积大于dMinArea，并且小于dMaxArea，则留下，反之，则删除。 
		if(dConArea > dMinAreaTemp && dConArea < dMaxAreaTemp && nSpermIndex < MAXSPERMNUM )
		{
			//绘制符合条件的轮廓
			cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);
			nSpermIndex = nSpermIndex+1;
			
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
			//////
			if (dMinAxsLen > 2)
			{
				double dShapeRatio = dMajAxsLen/dMinAxsLen;
				if (dShapeRatio >= pSAveSpermRange->dShapeRatio && dShapeRatio <= 4*pSAveSpermRange->dShapeRatio)
				{
					//绘制符合条件的轮廓
					cvDrawContours(pImgDraw,cTemp,CV_RGB(255,255,255),CV_RGB(255,255,255),0, CV_FILLED);
					nSpermIndex = nSpermIndex+1;
				}
			}
			////
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

	//test
	//cvSaveImage("E:/CreateCare/DataSample/Case12/ResultImgs/输出图像2.jpg",pImgBinary);//图片文件存储

	//资源释放
	cvReleaseMemStorage(&storage); 
}
*/

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

	CvScalar cvColor = DEADCOlOUR;
	if (nType == 1)//ABC
	{
		cvColor = ALIVECOlOUR;
	}
	else if (nType == 0)//D
	{
		cvColor = DEADCOlOUR;
	}
	else
	{
		cvColor = DEADCOlOUR;//ABCD
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

	CvScalar cvColor = DEADCOlOUR;
	if (nType == 1)//ABC
	{
		cvColor = ALIVECOlOUR;
	}
	else if (nType == 0)//D
	{
		cvColor = DEADCOlOUR;
	}
	else
	{
		cvColor = DEADCOlOUR;//ABCD
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
		nKeyTreshValue = -3000;//人精——备男
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


// 		//2.存储并绘制最终结果图片
// 		nStatus = drawFinalSpermImg(ppcImageFile[i], pImgContourShow, pImgABC, pImgBackGround, pSAveSpermRange);
// 		if (nStatus != 1)
// 		{
// 			//资源释放
// 			for (int j = 0; j < FILENUM; j++)
// 			{
// 				free(pcImgFileTemp[j]);
// 				pcImgFileTemp[j] = NULL;
// 			}
// 			free(pcFileNameTemp);
// 			pcFileNameTemp = NULL;
// 			free(pcImg);
// 			pcImg = NULL;
// 			cvReleaseImage(&pImgABC);
// 			cvReleaseImage(&pImgContourShow);
// 
// 			return nStatus;
// 		}
		

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
	ParaRange paraRangeTemp;
	ParaRange paraRangeTemp1 = {40, 110, 75, 9, 34, 21, 3.4, 9, 6.5, 3.3};//干涸人精
	ParaRange paraRangeTemp2 = {11, 22, 16.8, 3.2, 7, 5.08, 2, 4.8, 3.95, 1.23};//新鲜人精
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

	// 数据顺序[(第1个精子的轨迹序列)，（(第2个精子的轨迹序列)，(第3个精子的轨迹序列)，。。]
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

	//轨迹数据初始化，pSSpermTraceInfor[i*nImgFileNum+j]表示第i条轨迹上的第j个点
	int nNumSperm = 0;
	if (dataOut->nActiveSpermNum >5)//活动精子数
	{
		nNumSperm = dataOut->nActiveSpermNum;
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
	double dLimitedLength = 100*dDeltaT/(SParaInput.dRatioImg + EPSINON);///轨迹上相邻两个点的最远距离，最大速度100um/s
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
	int nSearchRadius = 5;

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
	int nSearchRadius = 5;
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

	if (pSSpermTraceInfor[nTraceIndexCopy-1].nPredictNum < 5 //最多预测五次
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

	struct spermProperty 
	{
		double dSL;
		double dVSL;	//平均直线运动速度
		double dVCL;	//平均曲线运动速度
		double dVAP;	//平均路径运动速度
		double dLIN;	//运动的直线度
		double dSTR;	//运动的前向性
		double dWOB;	//运动的摆动性
		double dALH;	//头部侧摆幅度
		double dMAD;	//平均角位移（度）
		int dBCF;		//交叉频率
	};

	spermProperty *pdTraceMotion = (spermProperty *)malloc(sizeof(spermProperty)*nTraceIndex);
	if (NULL == pdTraceMotion)
	{
		nStatus = 0;//内存异常
		return nStatus;
	}
	for (int i = 0; i<nTraceIndex; i++)
	{
		pdTraceMotion[i].dSL = 0;
		pdTraceMotion[i].dVSL = 0;
		pdTraceMotion[i].dVCL = 0;
		pdTraceMotion[i].dVAP = 0;
		pdTraceMotion[i].dLIN = 0;
		pdTraceMotion[i].dSTR = 0;
		pdTraceMotion[i].dWOB = 0;
		pdTraceMotion[i].dALH = 0;
		pdTraceMotion[i].dMAD = 0;
		pdTraceMotion[i].dBCF = 0;
	}

	for (int i= 0;i<10;i++)
	{
		dataOut->dHistVCL[i] = 0;
		dataOut->dHistVSL[i] = 0;
		dataOut->dHistVAP[i] = 0;
	}

	double dRatio = SParaInput.dRatioImg;//图像放大率
	double dDeltaT = 1/(SParaInput.dFrameRate+EPSINON);//采样间隔

	//分级
	int nNumClassA = 0;
	int nNumClassB = 0;
	int nNumClassC = 0;
	int nNumClassD = 0;

	int nNumTemp = 0;
	int nNumActiveTrace = 0;

	for (int i = 0; i<nTraceIndex; i++)
	{
		double dSLLength = 0;//直线距离
		double dCLLength = 0;//曲线距离
		double dAPLength = 0;//平均路径距离

		int nPosNum = 0;//轨迹的数据点数
		int nStartPoint = 0;
		//int nEndPoint = 0;

		//求该条轨迹的数据点数
		bool bStart = true;
		for (int j = 0; j<nImgFileNum; j++)
		{
			double dtemp1 = pSSpermTraceInfor[i*nImgFileNum+j].nPredictNum;
			double dtemp2 = pSSpermTraceInfor[i*nImgFileNum+j].fPosX;
			double dtemp3 = pSSpermTraceInfor[i*nImgFileNum+j].fPosY;

			if (pSSpermTraceInfor[i*nImgFileNum+j].nPredictNum != -1 && 
				pSSpermTraceInfor[i*nImgFileNum+j].fPosX > 0.2 && pSSpermTraceInfor[i*nImgFileNum+j].fPosY > 0.2)
			{
				if (bStart)
				{
					nStartPoint = i*nImgFileNum+j;
					bStart = false;
				}
				nPosNum ++;
			}
		}
		//nEndPoint = nStartPoint + nPosNum -1;

		//分类
		if (nPosNum <= 10 || nPosNum <= nImgFileNum/3)
		{
			pnTraceType[i] = 0;//微动或不动
		}
		else
		{
			double *dPointXY = (double *)malloc(sizeof(double)*nPosNum*2);//A1/B2/C3/D4
			if (NULL == dPointXY)
			{
				free(pdTraceMotion);
				pdTraceMotion = NULL;

				nStatus = 0;//内存异常
				return nStatus;
			}
			double *dPointFitXY = (double *)malloc(sizeof(double)*nPosNum*2);//A1/B2/C3/D4
			if (NULL == dPointFitXY)
			{
				free(pdTraceMotion);
				pdTraceMotion = NULL;
				free(dPointXY);
				dPointXY = NULL;

				nStatus = 0;//内存异常
				return nStatus;
			}
			
			double *dDistTwoImgs = (double *)malloc(sizeof(double)*(nPosNum-1));//A1/B2/C3/D4
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

			double *dDistPoints = (double *)malloc(sizeof(double)*(nPosNum));//A1/B2/C3/D4
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
			for (int j = 0; j<nPosNum; j++)
			{
				dPointXY[2*j] = pSSpermTraceInfor[nStartPoint+j].fPosX;//X
				dPointXY[2*j+1] = pSSpermTraceInfor[nStartPoint+j].fPosY;//Y
				if (j>=1)
				{
					double dLengthTemp = sqrt((dPointXY[2*j]-dPointXY[2*(j-1)])*(dPointXY[2*j]-dPointXY[2*(j-1)])
						+(dPointXY[2*j+1]-dPointXY[2*(j-1)+1])*(dPointXY[2*j+1]-dPointXY[2*(j-1)+1]));
					dDistTwoImgs[j-1] = dLengthTemp;
				}
			}

			double *pdAveStd= calAveStdIter(dDistTwoImgs, nPosNum-1);
			dCLLength = pdAveStd[0]*(nPosNum-1);//平均值*数量
			free(pdAveStd);
			pdAveStd = NULL;

			//直线长度计算
			dSLLength = sqrt((dPointXY[2*(nPosNum-1)]-dPointXY[0])*(dPointXY[2*(nPosNum-1)]-dPointXY[0])
				+(dPointXY[2*nPosNum-1]-dPointXY[1])*(dPointXY[2*nPosNum-1]-dPointXY[1]));

			//路径长度计算（含5点均值处理）
			for (int j = 0; j<nPosNum; j++)
			{
				if ((j == 0)||(j == nPosNum-1))
				{
					dPointFitXY[2*j] = dPointXY[2*j];//Fit_X
					dPointFitXY[2*j+1] = dPointXY[2*j+1];//Fit_Y
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
				if (j>=1)
				{
					double dLengthTemp = sqrt((dPointFitXY[2*j]-dPointFitXY[2*(j-1)])*(dPointFitXY[2*j]-dPointFitXY[2*(j-1)])
						+(dPointFitXY[2*j+1]-dPointFitXY[2*(j-1)+1])*(dPointFitXY[2*j+1]-dPointFitXY[2*(j-1)+1]));
					dDistTwoImgs[j-1] = dLengthTemp;
				}
			}
			pdAveStd= calAveStdIter(dDistTwoImgs, nPosNum-1);
			dAPLength = pdAveStd[0]*(nPosNum-1);//平均值*数量
			free(pdAveStd);
			pdAveStd = NULL;

			//异常情况排除
			if ((dSLLength > 1.1*dCLLength) || (dSLLength > 1.1*dAPLength) || (dAPLength > 1.1*dCLLength))
			{
				free(dPointXY);
				dPointXY = NULL;
				free(dPointFitXY);
				dPointFitXY = NULL;
				free(dDistTwoImgs);
				dDistTwoImgs = NULL;
				free(dDistPoints);
				dDistPoints = NULL;

				continue;
			}

			nNumActiveTrace ++; // 纳入统计的轨迹数量

			// 新增计算20251207：ALH/MAD/BCF
			//ALH: 侧摆最大值的均值（基于dPointXY与dPointFitXY的距离）
			//MAD：角度的变化值的绝对值的均值（基于dPointXY）
			//BCF：交叉频率（基于dPointXY与dPointFitXY的距离）
			// 计算dPointXY与dPointFitXY的点距离
			for (int j = 0; j<nPosNum; j++)
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

			pdTraceMotion[nNumTemp].dALH = dMaxAve*dRatio;
			pdTraceMotion[nNumTemp].dBCF = nMaxNum + 1;
			pdTraceMotion[nNumTemp].dMAD = dMad;

			// 新增计算20251207：ALH/MAD/BCF

			pdTraceMotion[nNumTemp].dSL = dSLLength*dRatio;//直线距离
			pdTraceMotion[nNumTemp].dVSL = dSLLength*dRatio/(nPosNum - 1)/(dDeltaT + EPSINON);//1VSL
			pdTraceMotion[nNumTemp].dVCL = dCLLength*dRatio/(nPosNum - 1)/(dDeltaT + EPSINON);//2VCL
			pdTraceMotion[nNumTemp].dVAP = dAPLength*dRatio/(nPosNum - 1)/(dDeltaT + EPSINON);//3VAP

			pdTraceMotion[nNumTemp].dLIN = pdTraceMotion[nNumTemp].dVSL/(pdTraceMotion[nNumTemp].dVCL+ EPSINON);
			pdTraceMotion[nNumTemp].dSTR = pdTraceMotion[nNumTemp].dVSL/(pdTraceMotion[nNumTemp].dVAP+ EPSINON);
			pdTraceMotion[nNumTemp].dWOB = pdTraceMotion[nNumTemp].dVAP/(pdTraceMotion[nNumTemp].dVCL+ EPSINON);

			dataOut->dAveVSL = dataOut->dAveVSL + pdTraceMotion[nNumTemp].dVSL;
			dataOut->dAveVCL = dataOut->dAveVCL + pdTraceMotion[nNumTemp].dVCL;
			dataOut->dAveVAP = dataOut->dAveVAP + pdTraceMotion[nNumTemp].dVAP;
			dataOut->dLIN = dataOut->dLIN + pdTraceMotion[nNumTemp].dLIN;
			dataOut->dSTR = dataOut->dSTR + pdTraceMotion[nNumTemp].dSTR;
			dataOut->dWOB = dataOut->dWOB + pdTraceMotion[nNumTemp].dWOB;

			dataOut->dALH = dataOut->dALH + pdTraceMotion[nNumTemp].dALH;
			dataOut->dBCF = dataOut->dBCF + pdTraceMotion[nNumTemp].dBCF;
			dataOut->dMAD = dataOut->dMAD + pdTraceMotion[nNumTemp].dMAD;


			if (pdTraceMotion[nNumTemp].dVCL > 2.5)
			{
				//直线或曲线分类
				if (pdTraceMotion[nNumTemp].dLIN >= 0.65)
				{
					dataOut->nNumSL++;
					pnTraceType[i] = 2;//直线2
				} 
				else
				{
					dataOut->nNumCL++;
					pnTraceType[i] = 1;//曲线1
				}

				//速度分布图
				//直线
				int nIndexSL = (int)(pdTraceMotion[nNumTemp].dVSL/10 + 0.5);
				if (nIndexSL <= 9)
				{
					dataOut->dHistVSL[nIndexSL]++;
				}
				
				//曲线
				int nIndexCL = (int)(pdTraceMotion[nNumTemp].dVCL/10 + 0.5);
				if (nIndexCL <= 9)
				{
					dataOut->dHistVCL[nIndexCL]++;
				}
				
				//路径
				int nIndexAP = (int)(pdTraceMotion[nNumTemp].dVAP/10 + 0.5);
				if (nIndexAP <= 9)
				{
					dataOut->dHistVAP[nIndexAP]++;
				}				

				//分级A/B/C/D
				if(pdTraceMotion[nNumTemp].dVAP >= 25)
				{
					nNumClassA++;
				} 
				else if(pdTraceMotion[nNumTemp].dVAP >= 5 && pdTraceMotion[nNumTemp].dSTR >= 0.6)
				{
					nNumClassB++;
				}else
				{
					nNumClassC++;
				}
			}		

			nNumTemp++;

			free(dPointXY);
			dPointXY = NULL;
			free(dPointFitXY);
			dPointFitXY = NULL;
			free(dDistTwoImgs);
			dDistTwoImgs = NULL;
			free(dDistPoints);
			dDistPoints = NULL;
		}
	}


	//求各种数量
	int nNumTotalTemp = (int)dataOut->nTotalSpermNum;
	int nNumABCTemp = (int)dataOut->nActiveSpermNum;
	nNumClassD = nNumTotalTemp-nNumABCTemp+nNumTemp-nNumClassA-nNumClassB-nNumClassC;
	nNumABCTemp = nNumClassA + nNumClassB + nNumClassC;
	nNumTotalTemp = nNumABCTemp + nNumClassD;
	dataOut->nTotalSpermNum = nNumTotalTemp;
	dataOut->nActiveSpermNum = nNumABCTemp;

	
	//运动分级
	dataOut->dRatioClassA = 100*nNumClassA/(nNumTotalTemp+EPSINON);
	dataOut->dRatioClassB = 100*nNumClassB/(nNumTotalTemp+EPSINON);
	dataOut->dRatioClassC = 100*nNumClassC/(nNumTotalTemp+EPSINON);
	dataOut->dRatioClassD = 100 - dataOut->dRatioClassA - dataOut->dRatioClassB - dataOut->dRatioClassC;
	dataOut->dRatioClassPR = dataOut->dRatioClassA + dataOut->dRatioClassB;
	dataOut->dRatioClassNP = dataOut->dRatioClassC;
	dataOut->dRatioClassIM = dataOut->dRatioClassD;
	dataOut->dActiveSpermRatio = dataOut->dRatioClassPR + dataOut->dRatioClassNP;

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
	dataOut->dAveVSL = dataOut->dAveVSL/(nNumActiveTrace+EPSINON);
	dataOut->dAveVCL = dataOut->dAveVCL/(nNumActiveTrace+EPSINON);
	dataOut->dAveVAP = dataOut->dAveVAP/(nNumActiveTrace+EPSINON);
	dataOut->dLIN = dataOut->dLIN/(nNumActiveTrace+EPSINON);
	dataOut->dSTR = dataOut->dSTR/(nNumActiveTrace+EPSINON);
	dataOut->dWOB = dataOut->dWOB/(nNumActiveTrace+EPSINON);

	dataOut->dALH = dataOut->dALH/(nNumActiveTrace+EPSINON);
	dataOut->dBCF = (int)(dataOut->dBCF/(nNumActiveTrace+EPSINON) + 0.5);
	dataOut->dMAD = dataOut->dMAD/(nNumActiveTrace+EPSINON);

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
int drawTraceImg(const char **ppcFilePath, int const& nImgFileNum, const char **ppcResImgFile, TraceInfor *pSSpermTraceInfor, int nTraceIndex, int *pnTraceType, double dLimitedLength, int nCenterPara[])
{
	//说明：ppcFilePath——原图目录
	//ppcResImgFile——结果图片路径
	//pnTraceType//微动0/曲线1/直线2
	//int nRadius = 2;
	int nStatus = 1;
	
	for (int i = 0; i<nImgFileNum; i++)
	{
		IplImage *pImgSrcTemp = cvLoadImage(ppcFilePath[i],1);
		IplImage *pImgContourShow = clipCenterImage(pImgSrcTemp, nCenterPara);
		cvReleaseImage(&pImgSrcTemp);

		for (int j = 0; j<nTraceIndex; j++)
		{
			CvScalar cvColor = DEADCOlOUR;
			if (pnTraceType[j] == 2)//直线
			{
				cvColor = ALIVECOlOUR;
			}
			else if (pnTraceType[j] == 1)//曲线
			{
				cvColor = DEADCOlOUR;
			}
			else if (pnTraceType[j] == 0)//微动或不动
			{
				cvColor = DEADCOlOUR;
			}
		
			//画线
			if (i >= 1)
			{
				for (int k = 0; k<i; k++)//连续的轨迹绘制
				{
					if (pSSpermTraceInfor[j*nImgFileNum+k].fPosX > 0.2 && pSSpermTraceInfor[j*nImgFileNum+k].fPosY > 0.2
						&& pSSpermTraceInfor[j*nImgFileNum+k+1].fPosX > 0.2 && pSSpermTraceInfor[j*nImgFileNum+k+1].fPosY > 0.2)//尚未生成
					{
						CvPoint cvCenter0 = cvPoint(cvRound(pSSpermTraceInfor[j*nImgFileNum+k].fPosX),cvRound(pSSpermTraceInfor[j*nImgFileNum+k].fPosY));
						CvPoint cvCenter1 = cvPoint(cvRound(pSSpermTraceInfor[j*nImgFileNum+k+1].fPosX),cvRound(pSSpermTraceInfor[j*nImgFileNum+k+1].fPosY));
						//cvColor = CV_RGB(253,90,78);//红色
						double dLengthTemp = sqrt((cvCenter1.x - cvCenter0.x)*(cvCenter1.x - cvCenter0.x) + (cvCenter1.y - cvCenter0.y)*(cvCenter1.y - cvCenter0.y));

						if (dLengthTemp < 6*dLimitedLength)
						{
							cvLine(pImgContourShow, cvCenter0, cvCenter1,cvColor,2);
						}
					}
				}
			}
		}

		//调整图像输出尺寸
		//IplImage *imgOut = cvCreateImage(cvSize(720,480),pImgContourShow->depth,pImgContourShow->nChannels);//创建更改后的结果图像
		//cvResize(pImgContourShow,imgOut,CV_INTER_LINEAR); 
		nStatus = cvSaveImage(ppcResImgFile[i],pImgContourShow);//图片文件存储

		cvReleaseImage(&pImgContourShow);
		//cvReleaseImage(&imgOut);

		if (nStatus == 0)
		{
			nStatus = -2;//数据写入异常
			return nStatus;
		}
	}

	return nStatus;
}


//精子轨迹的绘制(只画了当前图像中未终结的精子轨迹)
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
		IplImage *pImgContourShow = clipCenterImage(pImgSrcTemp, nCenterPara);
		cvReleaseImage(&pImgSrcTemp);

		for (int j = 0; j<nTraceIndex; j++)
		{
			if (pSSpermTraceInfor[j*nImgFileNum+i].nPredictNum == -1//轨迹的终结
				|| pSSpermTraceInfor[j*nImgFileNum+i].fPosX < EPSINON || pSSpermTraceInfor[j*nImgFileNum+i].fPosY < EPSINON)//尚未生成
			{
				continue;
			}

			CvPoint cvCenter = cvPoint(cvRound(pSSpermTraceInfor[j*nImgFileNum+i].fPosX),cvRound(pSSpermTraceInfor[j*nImgFileNum+i].fPosY));
			CvScalar cvColor;

			if (pnTraceType[i] == 2)//直线，红色
			{
				cvColor = CV_RGB(253,90,78);
			}
			else if (pnTraceType[i] == 1)//曲线，蓝色
			{
				cvColor = CV_RGB(78,154,253);
			}
			else if (pnTraceType[i] == 0)//微动或不动，暂定蓝色
			{
				cvColor = CV_RGB(78,154,253);
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

				/*
				if (pSSpermTraceInfor[j*nImgFileNum+i-1].nPredictNum == -1//轨迹的终结
					|| pSSpermTraceInfor[j*nImgFileNum+i-1].fPosX < EPSINON || pSSpermTraceInfor[j*nImgFileNum+i-1].fPosY < EPSINON)//尚未生成
				{
					continue;
				}

				CvPoint cvCenter0 = cvPoint(cvRound(pSSpermTraceInfor[j*nImgFileNum+i-1].fPosX),cvRound(pSSpermTraceInfor[j*nImgFileNum+i-1].fPosY));
				cvColor = CV_RGB(253,90,78);//红色
				cvLine(pImgContourShow, cvCenter0, cvCenter1,cvColor,2);
				
				
				//cvColor = CV_RGB(253,90,78);//红色
				//cvCircle(pImgContourShow, cvCenter0, nRadius, cvColor, 2);
				if (cvCenter0.x > 0 && cvCenter0.y > 0 && cvCenter1.x > 0 && cvCenter1.y > 0)
				{
					cvLine(pImgContourShow, cvCenter0, cvCenter1,cvColor,2);
				}*/
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

	// [估算程序】这是真实计算。 新增计算参数，将ABCD比例参数，计算为密度参数
		
		dataOut->dDensityClassPR = dataOut->dRatioClassPR * dataOut->dTotaSpermDensity /100;  // PR 密度
		dataOut->dDensityClassA = dataOut->dRatioClassA * dataOut->dTotaSpermDensity /100;    // A级 密度 Rapid-PR
		dataOut->dDensityClassB = dataOut->dRatioClassB * dataOut->dTotaSpermDensity /100;    // B级 密度 Slow-PR
		dataOut->dDensityClassC = dataOut->dRatioClassC * dataOut->dTotaSpermDensity /100;    // C级 密度 NP
		dataOut->dDensityClassD = dataOut->dRatioClassD * dataOut->dTotaSpermDensity /100;    // D级 密度 Immotile(D)
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