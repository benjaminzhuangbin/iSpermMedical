#ifndef _COUNTSPERMMED_H_
#define _COUNTSPERMMED_H_

#include "libalg.h"

struct sBoudary
{
	double min;
	double max;
};

// 形态学参数
struct sMorpPara 
{
	struct sBoudary dArea;			// 面积
	struct sBoudary dShape;			// 形状（长/宽）
	struct sBoudary dLength;		// 长度
	struct sBoudary dWidth;			// 宽度
	struct sBoudary dCircularity;	// 圆度
};

// 算法输入数据
typedef struct tag_algsqamed_data_in
{
	double  dRatioImg;          // 图像放大率，um/pixel
	double  dSampleDepth;       // 样本厚度
	double  dVolume;			// 精液量，单位ml
	double  dFrameRate;         // 相机采样频率

	double  dShapeRatio;		//目标的长宽比，0.5为标粒模式，1以上为猪精模式
	double  dPlateType;			//玻片类型，1表示蓝色六腔版，0表示白色四腔版

	char    *pcImgPath;         // 图像文件路径
	char    *pcResultPath;      // 分析结果（图片）路径

	double  dDSDensk;			//浓度校正系数k，0.1< k <=10

	struct sMorpPara morpPara;	// 形态学参数
}algsqamed_data_in;

// 算法输出数据
typedef struct tag_algsqamed_data_out
{
	int		 nTotalSpermNum;	//被检精子总数（个）
	double	 dTotaSpermDensity;	//精子浓度（百万/毫升）
	int		 nActiveSpermNum;	//活动精子数（PR+NP，个）
	double	 dActiveSpermDensity;//活动精子浓度（PR+NP，百万/毫升）

	double   dActiveSpermRatio; //总活力（PR+NP, %）
	double	 dRatioClassPR;		//前向运动（运动活跃型,PR, %）
	double	 dRatioClassNP;		//非前向运动（非运动活跃型,NP, %）
	double	 dRatioClassIM;		//不动（完全不动,IM,%）

	double	 dRatioClassA;		//A级
	double	 dRatioClassB;		//B级
	double	 dRatioClassC;		//C级
	double	 dRatioClassD;		//D级

	double	 dAveVSL;			//平均直线运动速度
	int		 nNumSL;			//直线运动精子总数
	double	 dRatioSL;			//直线运动精子百分比
	double	 dSpermDensitySL;	//直线运动精子浓度,百万个/毫升

	double	 dAveVCL;			//平均曲线运动速度
	int		 nNumCL;			//曲线运动精子总数
	double	 dRatioCL;			//曲线运动精子百分比
	double	 dSpermDensityCL;	//曲线运动精子浓度,百万个/毫升

	double	 dAveVAP;			//平均路径运动速度

	double   dLIN;				//运动的直线度
	double   dSTR;				//运动的前向性
	double   dWOB;				//运动的摆动性

	double	 dHistVCL[10];		//曲线运动速度分布图0-100um/s
	double	 dHistVSL[10];		//直线运动速度分布图0-100um/s
	double	 dHistVAP[10];		//路径运动速度分布图0-100um/s
	double	 dHistRank[4];		//精子活力等级分布图

	double	 dMorp;				//精子形态学参数, %
	
	// [估算值程序]新增参数
	double   dNormal;        // 形态正常比例（百分比）     
	double   dAbnormal;      // 形态异常比例（百分比）
	double   dHdefects;      // 头部异常比例（百分比）
	double   dMdefects;      // 中部异常比例（百分比）
	double   dTdefects;      // 尾部异常比例（百分比）
	double   dCRdefects;     // CR原生质缔异常比例（百分比）
	double   dTZI;           // 畸形精子症指数 
	double   dSDI;           // 精子畸形指数
	double   dRoundcells;    // 圆细胞比例（百分比）

	double dALH;  // 头部侧摆幅度(amplitude lateral )(µm)
	double dMAD;  //平均角位移(度)(angular displacement)(degrees)
	double dBCF;  //交叉频率(cross frequency)(Hz)

	double dDCL;  // distance curvilinear
	double dDSL;  // distance straight line
	double dDAP;  // distance average path

	
	double dDensityClassPR;     // PR 密度
	double dDensityClassA;      // A级 密度 Rapid-PR
	double dDensityClassB;      // B级 密度 Slow-PR
	double dDensityClassC;      // C级 密度 NP
	double dDensityClassD;      // D级 密度 Immotile(D)
	double dDensityRoundcells;  // 圆细胞 密度 

}algsqamed_data_out;


//算法版本信息
typedef struct tag_algInforMed
{
	char cAlgVersion[20];//算法版本
	char cAlgReleaseDate[20];//算法发布日期
}algInforMed;

#ifdef __cplusplus
extern "C"
{
#endif

//算法入口
//ALG_EXPORT
int getSpermCountMainMed(algsqamed_data_out *dataOut, const algsqamed_data_in *dataIn);

//获取算法版本信息
void getAlgInforMed(algInforMed *algInforOut);

//视频分解为图片
int getVideoFrame(const char* pcVideoPath, const char* pcVidName, int &iImgNum);

//判断光源灯是否亮起
//ALG_EXPORT bool isImgBrightMed(const char *pcImgPath);

#ifdef __cplusplus
}
#endif


#endif // _COUNTSPERMMED_H_
