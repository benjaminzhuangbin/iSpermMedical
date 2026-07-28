// 测试程序
#include "CountSpermMed.h"

//#include "testbenchMultiThread.h"//多线程测试专用
#include <io.h> //目录文件列表搜索专用
#include <time.h>//打印系统时间专用

#include<opencv/cv.h>   //cv.h OpenCV的主要功能头文件
#include <opencv/highgui.h>//显示图像用的，因为用到了显示图片

//测试计算耗时专用
#include <iostream>  
#include <windows.h>  

//#include "libalg.h"

#ifdef WIN32
#include "algdebug.h"
#else
#define EnableMemLeakCheck()
#define CHECK_MEMORY_LEAKS()
#endif

#include <stdlib.h>
#include <iostream> //cout,cin输出显示用

using namespace std;

//结果写到文件
//int writeInfoToFile(const char *pcFilePath, const char *pcVidFileName, alg_data_out *dataOut);

//一体机医疗版项目
static void test_algsqaMed(void);

int main(int argc, char **argv)
{
	//内存泄漏测试
	EnableMemLeakCheck();
	//_CrtSetBreakAlloc(470);

	test_algsqaMed();

	_CrtDumpMemoryLeaks();//检查内存泄漏
    // 按键退出 
    cout << endl << "Press any key + enter to quit!" << endl;
    char flag;
    cin >> flag;
	
	return 0;
}

//一体机医疗版项目
static void test_algsqaMed(void)
{
	//输入参考：
	struct tag_algsqamed_data_in *algsqamed_data_in_t = (struct tag_algsqamed_data_in *)malloc(sizeof(struct tag_algsqamed_data_in));
	struct tag_algInforMed *algInforMed = (struct tag_algInforMed *)malloc(sizeof(struct tag_algInforMed));

	//系统参数定义
	algsqamed_data_in_t->dRatioImg = 0.958;//0.9068;//图像放大率,um/pixel
	algsqamed_data_in_t->dSampleDepth = 20;//样本厚度
	algsqamed_data_in_t->dVolume= 3;// 精液量，单位ml
	algsqamed_data_in_t->dFrameRate = 24;//相机采样频率
	algsqamed_data_in_t->dShapeRatio = 1;//目标的长宽比，0.5为标粒模式，1以上为猪精模式（该值界面可调，范围1到2）
	algsqamed_data_in_t->dPlateType = 1;//玻片类型，1表示蓝色六腔版，0表示白色四腔版
	algsqamed_data_in_t->pcImgPath = "E:/2012/Coding/SQAM/vsProj/spermAnalysis/spermAnalysis/data/";//图像地址
	algsqamed_data_in_t->pcResultPath = "E:/2012/Coding/SQAM/vsProj/spermAnalysis/spermAnalysis/data/ResultImgs/";
	algsqamed_data_in_t->dDSDensk = 1;//浓度校正系数k，0.1< k <=10

	/*
	// 形态学参数判断范围（人——备男）
	algsqamed_data_in_t->morpPara.dLength.min = 5.0;		//单位um
	algsqamed_data_in_t->morpPara.dLength.max = 7.5;		//单位um
	algsqamed_data_in_t->morpPara.dWidth.min = 3.0;			//单位um
	algsqamed_data_in_t->morpPara.dWidth.max = 4.5;			//单位um
	algsqamed_data_in_t->morpPara.dShape.min = 1.5;			//无量纲
	algsqamed_data_in_t->morpPara.dShape.max = 1.8;			//无量纲
	algsqamed_data_in_t->morpPara.dArea.min = 15.0;			//单位um2
	algsqamed_data_in_t->morpPara.dArea.max = 25.0;			//单位um2
	algsqamed_data_in_t->morpPara.dCircularity.min = 0.6;	//无量纲
	algsqamed_data_in_t->morpPara.dCircularity.max = 0.9;	//无量纲
	*/

	// 形态学参数判断范围（猪——一体机）
	algsqamed_data_in_t->morpPara.dLength.min = 8.5;		//单位um
	algsqamed_data_in_t->morpPara.dLength.max = 11.5;		//单位um
	algsqamed_data_in_t->morpPara.dWidth.min = 3.0;			//单位um
	algsqamed_data_in_t->morpPara.dWidth.max = 5.0;			//单位um
	algsqamed_data_in_t->morpPara.dShape.min = 2.1;			//无量纲
	algsqamed_data_in_t->morpPara.dShape.max = 2.5;			//无量纲
	algsqamed_data_in_t->morpPara.dArea.min = 25.0;			//单位um2
	algsqamed_data_in_t->morpPara.dArea.max = 35.0;			//单位um2
	algsqamed_data_in_t->morpPara.dCircularity.min = 0.4;	//无量纲
	algsqamed_data_in_t->morpPara.dCircularity.max = 0.7;	//无量纲

	//视频分解图片
	//int nNumImg = 0;
	//int nStatusTemp = getVideoFrame(algsqamed_data_in_t->pcImgPath, "Test000.avi", nNumImg);

	//输出参考：
	struct tag_algsqamed_data_out *algsqamed_data_out = (struct tag_algsqamed_data_out *)malloc(sizeof(struct tag_algsqamed_data_out));

	int nStatus = getSpermCountMainMed(algsqamed_data_out, algsqamed_data_in_t);//算法运行状态
	getAlgInforMed(algInforMed);

	//输出结果
	//if (nStatus == 1)
	{
		cout << endl;
		cout << "============================================= " << endl;
		cout << "===============test_algsqaMed================ " << endl;
		cout << "算法版本号：" << algInforMed->cAlgVersion << endl;//算法版本号
		cout << "算法发布日期：" << algInforMed->cAlgReleaseDate << endl;//算法发布日期
		cout << "算法返回状态：" << nStatus << endl;
		cout << endl;

		cout << "============================================== " << endl;
		cout << "===============Animal 动物版本================ " << endl;
		cout << "============================================== " << endl;
		cout << endl;

		cout << "============================================== " << endl;
		cout << " 最终需要导入给APP显示的结果是：结果开头带 # 和 *  " << endl;
		cout << " 结果开头 #  原程序 " << endl;
		cout << " 结果开头 *  新程序 " << endl;
		cout << "============================================== " << endl;
		cout << endl;
		
		cout << "被检精子总数：" << algsqamed_data_out->nTotalSpermNum << " 个" << endl;
		cout << "活动精子数(PR+NP)：" << algsqamed_data_out->nActiveSpermNum << " 个" << endl;
		cout << "曲线运动精子总数： " << algsqamed_data_out->nNumCL  << " 个" << endl;
		cout << "曲线运动精子百分比： " << algsqamed_data_out->dRatioCL  << " %" << endl;
		cout << "曲线运动精子浓度： " << algsqamed_data_out->dSpermDensityCL  << " 百万个/毫升" << endl;
		cout << "直线运动精子总数： " << algsqamed_data_out->nNumSL  << " 个" << endl;
		cout << "直线运动精子百分比： " << algsqamed_data_out->dRatioSL  << " %" << endl;
		cout << "直线运动精子浓度： " << algsqamed_data_out->dSpermDensitySL  << " 百万个/毫升" << endl;
		cout << "图像分析出的形态正常的精子比例(%)： " << algsqamed_data_out->dMorp  << " %" << endl;
		cout << endl;
		cout << "精子运动分级统计，A级 Rapid-PR：" << algsqamed_data_out->dRatioClassA << " %" << endl;//A级
		cout << "精子运动分级统计，C级 NP：" << algsqamed_data_out->dRatioClassC << " %" << endl;//C级
		cout << "精子运动分级统计，A级密度 Rapid-PR：" << algsqamed_data_out->dDensityClassA << " 百万个/毫升" << endl;//A级密度		
		cout << "精子运动分级统计，C级密度 NP：" << algsqamed_data_out->dDensityClassC << " 百万个/毫升" << endl;//C级密度		
		cout << endl;
		cout << endl;
		cout << "============================================== " << endl;
		cout << " 主APP显示的结果如下 " << endl;
		cout << "============================================== " << endl;
		cout << endl;

		cout << "# 01.精子浓度(Conc.)：" << algsqamed_data_out->dTotaSpermDensity << " 百万个/毫升" << endl;
		cout << endl;

		cout << "# 02.总活力（Motile=PR+NP）：" << algsqamed_data_out->dActiveSpermRatio << " %" << endl;//PR, A+B+C
		cout << "# 03.活动精子浓度(Motile=PR+NP)：" << algsqamed_data_out->dActiveSpermDensity << " 百万个/毫升" << endl;
		cout << endl;

		cout << "# 04.前向运动（运动活跃型)(PR）：" << algsqamed_data_out->dRatioClassPR << " %" << endl;//PR, A+B
		cout << "* 05.前向运动（运动活跃型)(PR）密度：" << algsqamed_data_out->dDensityClassPR << " 百万个/毫升" << endl;//PR, A+B
		cout << endl;

		cout << "# 06.精子运动分级统计，B级 Slow-PR：" << algsqamed_data_out->dRatioClassB << " %" << endl;//B级
		cout << "# 07.精子运动分级统计，D级 Immotile(D)：" << algsqamed_data_out->dRatioClassD << " %" << endl;//D级
		cout << endl;

		cout << "* 08.精子运动分级统计，B级密度 Slow-PR：" << algsqamed_data_out->dDensityClassB << " 百万个/毫升" << endl;//B级密度
		cout << "* 09.精子运动分级统计，D级密度 Immotile(D)：" << algsqamed_data_out->dDensityClassD << " 百万个/毫升" << endl;//D级密度
		cout << endl;

		cout << "# 10.平均直线运动速度 VSL： " << algsqamed_data_out->dAveVSL  << " um/s" << endl;
		cout << "* 11.平均直线运动距离 DSL： " << algsqamed_data_out->dDSL  << " um" << endl;

		cout << endl;

		cout << "# 12.平均曲线运动速度 VCL： " << algsqamed_data_out->dAveVCL  << " um/s"  << endl;
		cout << "* 13.平均曲线运动距离 DCL： " << algsqamed_data_out->dDCL  << " um"  << endl;

		cout << endl;

		cout << "# 14.平均路径运动速度 VAP： " << algsqamed_data_out->dAveVAP  << " um/s"  << endl;
		cout << "* 15.平均路径运动距离 DAP： " << algsqamed_data_out->dDAP  << " um"  << endl;
		cout << endl;

		cout << "# 16.运动的线性度 LIN： " << algsqamed_data_out->dLIN  << endl;
		cout << "# 17.运动的前向性 STR： " << algsqamed_data_out->dSTR  << endl;
		cout << "# 18.运动的摆动性 WOB： " << algsqamed_data_out->dWOB  << endl;
		cout << endl;
		
		cout << "* 19.头部侧摆幅度 ALH： " << algsqamed_data_out->dALH  << " um" << endl;  
		cout << "* 20.平均角位移 MAD： " << algsqamed_data_out->dMAD  <<  " degrees" << endl;  
		cout << "* 21.交叉频率 BCF： " << algsqamed_data_out->dBCF  << " Hz" << endl;  
		cout << endl;

		cout << "动物特有形态学估算参数： " << endl; 	
		cout << endl;
	    cout << "* 22.估算值 动物 Normal 形态正常比例（%）： " << algsqamed_data_out->dNormalAnimal  << " %" << endl;  
		cout << "* 23.估算值 动物 Abnormal 形态异常比例（%）： " << algsqamed_data_out->dAbnormalAnimal  << " %" << endl; 
		cout << "* 24.估算值 动物 Head defects 头部异常比例（%）： " << algsqamed_data_out->dHdefectsAnimal  << " %" << endl; 
		cout << "* 25.估算值 动物 DMR 远中反流（%）： " << algsqamed_data_out->dDMRAnimal  << " %" << endl; 
		cout << "* 26.估算值 动物 DCD 远端胞质地（%）： " << algsqamed_data_out->dDCDAnimal  << " %" << endl; 
		cout << "* 27.估算值 动物 PCD 近端胞质地（%）： " << algsqamed_data_out->dPCDAnimal  << " %" << endl; 
		cout << "* 28.估算值 动物(Bent Tail)弯尾（%）： " << algsqamed_data_out->dBTailAnimal  << " %" << endl; 
		cout << "* 29.估算值 动物(Coiled Tail)卷尾（%）： " << algsqamed_data_out->dCTailAnimal  << " %" << endl; 
		cout << endl;
		cout << endl;

		cout << ".曲线运动速度分布图： "  << algsqamed_data_out->dHistVCL[0]  << ", " 
											<< algsqamed_data_out->dHistVCL[1]  << ", "
											<< algsqamed_data_out->dHistVCL[2]  << ", "
											<< algsqamed_data_out->dHistVCL[3]  << ", "
											<< algsqamed_data_out->dHistVCL[4]  << ", "
											<< algsqamed_data_out->dHistVCL[5]  << ", "
											<< algsqamed_data_out->dHistVCL[6]  << ", "
											<< algsqamed_data_out->dHistVCL[7]  << ", "
											<< algsqamed_data_out->dHistVCL[8]  << ", "
											<< algsqamed_data_out->dHistVCL[9]  
											<< endl;

		cout << ".直线运动速度分布图： "  << algsqamed_data_out->dHistVSL[0]  << ", " 
											<< algsqamed_data_out->dHistVSL[1]  << ", "
											<< algsqamed_data_out->dHistVSL[2]  << ", "
											<< algsqamed_data_out->dHistVSL[3]  << ", "
											<< algsqamed_data_out->dHistVSL[4]  << ", "
											<< algsqamed_data_out->dHistVSL[5]  << ", "
											<< algsqamed_data_out->dHistVSL[6]  << ", "
											<< algsqamed_data_out->dHistVSL[7]  << ", "
											<< algsqamed_data_out->dHistVSL[8]  << ", "
											<< algsqamed_data_out->dHistVSL[9]  
											<< endl;

		cout << ".路径运动速度分布图： "  << algsqamed_data_out->dHistVAP[0]  << ", " 
											<< algsqamed_data_out->dHistVAP[1]  << ", "
											<< algsqamed_data_out->dHistVAP[2]  << ", "
											<< algsqamed_data_out->dHistVAP[3]  << ", "
											<< algsqamed_data_out->dHistVAP[4]  << ", "
											<< algsqamed_data_out->dHistVAP[5]  << ", "
											<< algsqamed_data_out->dHistVAP[6]  << ", "
											<< algsqamed_data_out->dHistVAP[7]  << ", "
											<< algsqamed_data_out->dHistVAP[8]  << ", "
											<< algsqamed_data_out->dHistVAP[9]  
											<< endl;

		cout << ".精子活力等级分布图： "  << algsqamed_data_out->dHistRank[0]  << ", " 
											<< algsqamed_data_out->dHistRank[1]  << ", "
											<< algsqamed_data_out->dHistRank[2]  << ", "
											<< algsqamed_data_out->dHistRank[3]  
											<< endl;

		cout << "=========================================== " << endl;
		cout << endl;
		cout << endl;
		cout << endl;
	}

	free(algsqamed_data_in_t);
	free(algsqamed_data_out);
	free(algInforMed);
	algsqamed_data_in_t = NULL;
	algsqamed_data_out = NULL;
	algInforMed = NULL;
}

/*
int writeInfoToFile(const char *pcFilePath, const char *pcVidFileName, alg_data_out *dataOut)
{
	int nStatus = 1;
	
	int MAXPATHLENGTH = 200;
	char *pcFileFullName = (char *)malloc(sizeof(char)*MAXPATHLENGTH);
	if ( NULL == pcFileFullName )
	{
		nStatus = 0;
		return nStatus;//内存异常
	}
	char pcFileName[30] = "AllSampleResult.txt";

	strcpy(pcFileFullName, pcFilePath);
	strcat(pcFileFullName, pcFileName);

	FILE *fp = fopen(pcFileFullName, "a");   //没有这个文件则继续
	if ( fp )
	{
		//图像中心获取
		fprintf(fp, "%-30s\t", pcVidFileName);//文件名

		fprintf(fp, "%4d\t", dataOut->nTotalSpermNum);//1.精子总数
		fprintf(fp, "%6.2f\t", dataOut->dTotaSpermDensity);//2.精子密度
		fprintf(fp, "%4d\t", dataOut->nAliveSpermNum);//3.活动的精子数
		fprintf(fp, "%6.2f\t", dataOut->dAliveSpermDensity);//4.活动精子密度
		fprintf(fp, "%6.3f\t", dataOut->dAliveSpermRatio);//5.精子活率
		fprintf(fp, "%6.3f\t", dataOut->dActiveSpermRatio);//6.精子活力

		fprintf(fp, "%6.2f\t", dataOut->dVelcoitySL);//7.平均直线运动速度
		fprintf(fp, "%4d\t", dataOut->nNumSL);//8.直线运动精子总数
		fprintf(fp, "%6.3f\t", dataOut->dRatioSL);//9.直线运动活率

		fprintf(fp, "%6.2f\t", dataOut->dVelcoityCL);//10.平均曲线运动速度
		fprintf(fp, "%4d\t", dataOut->nNumCL);//11.曲线运动精子总数
		fprintf(fp, "%6.3f\t", dataOut->dRatioCL);//12.曲线运动活率

		fprintf(fp, "%6.2f\t", dataOut->dVelcoityAP);//13.平均路径运动速度
		fprintf(fp, "%4d\t", dataOut->nNumAP);//14.路径运动精子总数
		fprintf(fp, "%6.3f\t", dataOut->dRatioAP);//15.路径运动活率

		fprintf(fp, "%6.3f\t", dataOut->dRatioClassA);//16.精子运动速度分级统计，A级
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassB);//17.精子运动速度分级统计，B级
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassC);//18.精子运动速度分级统计，C级
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassD);//19.精子运动速度分级统计，D级

		fprintf(fp, "%6.3f\t", dataOut->dRatioClassAB);//20.前向运动（运动活跃型PR）
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassCSlowD);//21.非前向运动（非运动活跃型NP）
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassDeadD);//22.不动（完全不动型IM）

		fputc('\n', fp);
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
*/