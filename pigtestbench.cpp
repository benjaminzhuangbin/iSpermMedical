// ���Գ���
#include "CountSpermMed.h"

//#include "testbenchMultiThread.h"//���̲߳���ר��
#include <io.h> //Ŀ¼�ļ��б�����ר��
#include <time.h>//��ӡϵͳʱ��ר��

#include<opencv/cv.h>   //cv.h OpenCV����Ҫ����ͷ�ļ�
#include <opencv/highgui.h>//��ʾͼ���õģ���Ϊ�õ�����ʾͼƬ

//���Լ����ʱר��
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
#include <iostream> //cout,cin�����ʾ��

using namespace std;

//���д���ļ�
//int writeInfoToFile(const char *pcFilePath, const char *pcVidFileName, alg_data_out *dataOut);

//һ���ҽ�ư���Ŀ
static void test_algsqaMed(void);

int main(int argc, char **argv)
{
	//�ڴ�й©����
	EnableMemLeakCheck();
	//_CrtSetBreakAlloc(470);

	test_algsqaMed();

	_CrtDumpMemoryLeaks();//����ڴ�й©
    // �����˳� 
    cout << endl << "Press any key + enter to quit!" << endl;
    char flag;
    cin >> flag;
	
	return 0;
}

//һ���ҽ�ư���Ŀ
static void test_algsqaMed(void)
{
	//����ο���
	struct tag_algsqamed_data_in *algsqamed_data_in_t = (struct tag_algsqamed_data_in *)malloc(sizeof(struct tag_algsqamed_data_in));
	struct tag_algInforMed *algInforMed = (struct tag_algInforMed *)malloc(sizeof(struct tag_algInforMed));

	//ϵͳ��������
	algsqamed_data_in_t->dRatioImg = 0.958;//0.9068;//ͼ��Ŵ���,um/pixel
	algsqamed_data_in_t->dSampleDepth = 20;//�������
	algsqamed_data_in_t->dVolume= 3;// ��Һ������λml
	algsqamed_data_in_t->dFrameRate = 24;//�������Ƶ��
	algsqamed_data_in_t->dShapeRatio = 1;//Ŀ��ĳ���ȣ�0.5Ϊ����ģʽ��1����Ϊ��ģʽ����ֵ����ɵ�����Χ1��2��
	algsqamed_data_in_t->dPlateType = 1;//��Ƭ���ͣ�1��ʾ��ɫ��ǻ�棬0��ʾ��ɫ��ǻ��
	algsqamed_data_in_t->pcImgPath = "E:/2012/Coding/SQAM/vsProj/spermAnalysis/spermAnalysis/data/";//ͼ���ַ
	algsqamed_data_in_t->pcResultPath = "E:/2012/Coding/SQAM/vsProj/spermAnalysis/spermAnalysis/data/ResultImgs/";
	algsqamed_data_in_t->dDSDensk = 1;//Ũ��У��ϵ��k��0.1< k <=10

	/*
	// ��̬ѧ�����жϷ�Χ���ˡ������У�
	algsqamed_data_in_t->morpPara.dLength.min = 5.0;		//��λum
	algsqamed_data_in_t->morpPara.dLength.max = 7.5;		//��λum
	algsqamed_data_in_t->morpPara.dWidth.min = 3.0;			//��λum
	algsqamed_data_in_t->morpPara.dWidth.max = 4.5;			//��λum
	algsqamed_data_in_t->morpPara.dShape.min = 1.5;			//������
	algsqamed_data_in_t->morpPara.dShape.max = 1.8;			//������
	algsqamed_data_in_t->morpPara.dArea.min = 15.0;			//��λum2
	algsqamed_data_in_t->morpPara.dArea.max = 25.0;			//��λum2
	algsqamed_data_in_t->morpPara.dCircularity.min = 0.6;	//������
	algsqamed_data_in_t->morpPara.dCircularity.max = 0.9;	//������
	*/

	// ��̬ѧ�����жϷ�Χ������һ�����
	algsqamed_data_in_t->morpPara.dLength.min = 8.5;		//��λum
	algsqamed_data_in_t->morpPara.dLength.max = 11.5;		//��λum
	algsqamed_data_in_t->morpPara.dWidth.min = 3.0;			//��λum
	algsqamed_data_in_t->morpPara.dWidth.max = 5.0;			//��λum
	algsqamed_data_in_t->morpPara.dShape.min = 2.1;			//������
	algsqamed_data_in_t->morpPara.dShape.max = 2.5;			//������
	algsqamed_data_in_t->morpPara.dArea.min = 25.0;			//��λum2
	algsqamed_data_in_t->morpPara.dArea.max = 35.0;			//��λum2
	algsqamed_data_in_t->morpPara.dCircularity.min = 0.4;	//������
	algsqamed_data_in_t->morpPara.dCircularity.max = 0.7;	//������

	//��Ƶ�ֽ�ͼƬ
	//int nNumImg = 0;
	//int nStatusTemp = getVideoFrame(algsqamed_data_in_t->pcImgPath, "Test000.avi", nNumImg);

	//����ο���
	struct tag_algsqamed_data_out *algsqamed_data_out = (struct tag_algsqamed_data_out *)malloc(sizeof(struct tag_algsqamed_data_out));

	int nStatus = getSpermCountMainMed(algsqamed_data_out, algsqamed_data_in_t);//�㷨����״̬
	getAlgInforMed(algInforMed);

	//������
	//if (nStatus == 1)
	{
		cout << endl;
		cout << "============================================= " << endl;
		cout << "===============test_algsqaMed================ " << endl;
		cout << "�㷨�汾�ţ�" << algInforMed->cAlgVersion << endl;//�㷨�汾��
		cout << "�㷨�������ڣ�" << algInforMed->cAlgReleaseDate << endl;//�㷨��������
		cout << "�㷨����״̬��" << nStatus << endl;
		cout << endl;

		cout << "============================================== " << endl;
		cout << "===============Animal ����汾================ " << endl;
		cout << "============================================== " << endl;
		cout << endl;

		cout << "============================================== " << endl;
		cout << " ������Ҫ�����APP��ʾ�Ľ���ǣ������ͷ�� # �� *  " << endl;
		cout << " �����ͷ #  ԭ���� " << endl;
		cout << " �����ͷ *  �³��� " << endl;
		cout << "============================================== " << endl;
		cout << endl;
		
		cout << "���쾫��������" << algsqamed_data_out->nTotalSpermNum << " ��" << endl;
		cout << "�������(PR+NP)��" << algsqamed_data_out->nActiveSpermNum << " ��" << endl;
		cout << "�����˶����������� " << algsqamed_data_out->nNumCL  << " ��" << endl;
		cout << "�����˶����Ӱٷֱȣ� " << algsqamed_data_out->dRatioCL  << " %" << endl;
		cout << "�����˶�����Ũ�ȣ� " << algsqamed_data_out->dSpermDensityCL  << " �����/����" << endl;
		cout << "ֱ���˶����������� " << algsqamed_data_out->nNumSL  << " ��" << endl;
		cout << "ֱ���˶����Ӱٷֱȣ� " << algsqamed_data_out->dRatioSL  << " %" << endl;
		cout << "ֱ���˶�����Ũ�ȣ� " << algsqamed_data_out->dSpermDensitySL  << " �����/����" << endl;
		cout << "ͼ�����������̬�����ľ��ӱ���(%)�� " << algsqamed_data_out->dMorp  << " %" << endl;
		cout << endl;
		cout << "�����˶��ּ�ͳ�ƣ�A�� Rapid-PR��" << algsqamed_data_out->dRatioClassA << " %" << endl;//A��
		cout << "�����˶��ּ�ͳ�ƣ�C�� NP��" << algsqamed_data_out->dRatioClassC << " %" << endl;//C��
		cout << "�����˶��ּ�ͳ�ƣ�A���ܶ� Rapid-PR��" << algsqamed_data_out->dDensityClassA << " �����/����" << endl;//A���ܶ�		
		cout << "�����˶��ּ�ͳ�ƣ�C���ܶ� NP��" << algsqamed_data_out->dDensityClassC << " �����/����" << endl;//C���ܶ�		
		cout << endl;
		cout << endl;
		cout << "============================================== " << endl;
		cout << " ��APP��ʾ�Ľ������ " << endl;
		cout << "============================================== " << endl;
		cout << endl;

		cout << "# 01.����Ũ��(Conc.)��" << algsqamed_data_out->dTotaSpermDensity << " �����/����" << endl;
		cout << endl;

		cout << "# 02.�ܻ�����Motile=PR+NP����" << algsqamed_data_out->dActiveSpermRatio << " %" << endl;//PR, A+B+C
		cout << "# 03.�����Ũ��(Motile=PR+NP)��" << algsqamed_data_out->dActiveSpermDensity << " �����/����" << endl;
		cout << endl;

		cout << "# 04.ǰ���˶����˶���Ծ��)(PR����" << algsqamed_data_out->dRatioClassPR << " %" << endl;//PR, A+B
		cout << "* 05.ǰ���˶����˶���Ծ��)(PR���ܶȣ�" << algsqamed_data_out->dDensityClassPR << " �����/����" << endl;//PR, A+B
		cout << endl;

		cout << "# 06.�����˶��ּ�ͳ�ƣ�B�� Slow-PR��" << algsqamed_data_out->dRatioClassB << " %" << endl;//B��
		cout << "# 07.�����˶��ּ�ͳ�ƣ�D�� Immotile(D)��" << algsqamed_data_out->dRatioClassD << " %" << endl;//D��
		cout << endl;

		cout << "* 08.�����˶��ּ�ͳ�ƣ�B���ܶ� Slow-PR��" << algsqamed_data_out->dDensityClassB << " �����/����" << endl;//B���ܶ�
		cout << "* 09.�����˶��ּ�ͳ�ƣ�D���ܶ� Immotile(D)��" << algsqamed_data_out->dDensityClassD << " �����/����" << endl;//D���ܶ�
		cout << endl;

		cout << "# 10.ƽ��ֱ���˶��ٶ� VSL�� " << algsqamed_data_out->dAveVSL  << " um/s" << endl;
		cout << "* 11.ƽ��ֱ���˶����� DSL�� " << algsqamed_data_out->dDSL  << " um" << endl;

		cout << endl;

		cout << "# 12.ƽ�������˶��ٶ� VCL�� " << algsqamed_data_out->dAveVCL  << " um/s"  << endl;
		cout << "* 13.ƽ�������˶����� DCL�� " << algsqamed_data_out->dDCL  << " um"  << endl;

		cout << endl;

		cout << "# 14.ƽ��·���˶��ٶ� VAP�� " << algsqamed_data_out->dAveVAP  << " um/s"  << endl;
		cout << "* 15.ƽ��·���˶����� DAP�� " << algsqamed_data_out->dDAP  << " um"  << endl;
		cout << endl;

		cout << "# 16.�˶������Զ� LIN�� " << algsqamed_data_out->dLIN  << endl;
		cout << "# 17.�˶���ǰ���� STR�� " << algsqamed_data_out->dSTR  << endl;
		cout << "# 18.�˶��İڶ��� WOB�� " << algsqamed_data_out->dWOB  << endl;
		cout << endl;
		
		cout << "* 19.ͷ����ڷ��� ALH�� " << algsqamed_data_out->dALH  << " um" << endl;  
		cout << "* 20.ƽ����λ�� MAD�� " << algsqamed_data_out->dMAD  <<  " degrees" << endl;  
		cout << "* 21.����Ƶ�� BCF�� " << algsqamed_data_out->dBCF  << " Hz" << endl;  
		cout << endl;

		cout << "����������̬ѧ��������� " << endl; 	
		cout << endl;
	    cout << "* 22.����ֵ ���� Normal ��̬����������%���� " << algsqamed_data_out->dNormalAnimal  << " %" << endl;  
		cout << "* 23.����ֵ ���� Abnormal ��̬�쳣������%���� " << algsqamed_data_out->dAbnormalAnimal  << " %" << endl; 
		cout << "* 24.����ֵ ���� Head defects ͷ���쳣������%���� " << algsqamed_data_out->dHdefectsAnimal  << " %" << endl; 
		cout << "* 25.����ֵ ���� DMR Զ�з�����%���� " << algsqamed_data_out->dDMRAnimal  << " %" << endl; 
		cout << "* 26.����ֵ ���� DCD Զ�˰��ʵأ�%���� " << algsqamed_data_out->dDCDAnimal  << " %" << endl; 
		cout << "* 27.����ֵ ���� PCD ���˰��ʵأ�%���� " << algsqamed_data_out->dPCDAnimal  << " %" << endl; 
		cout << "* 28.����ֵ ����(Bent Tail)��β��%���� " << algsqamed_data_out->dBTailAnimal  << " %" << endl; 
		cout << "* 29.����ֵ ����(Coiled Tail)��β��%���� " << algsqamed_data_out->dCTailAnimal  << " %" << endl; 
		cout << endl;
		cout << endl;

		cout << ".�����˶��ٶȷֲ�ͼ�� "  << algsqamed_data_out->dHistVCL[0]  << ", " 
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

		cout << ".ֱ���˶��ٶȷֲ�ͼ�� "  << algsqamed_data_out->dHistVSL[0]  << ", " 
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

		cout << ".·���˶��ٶȷֲ�ͼ�� "  << algsqamed_data_out->dHistVAP[0]  << ", " 
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

		cout << ".���ӻ����ȼ��ֲ�ͼ�� "  << algsqamed_data_out->dHistRank[0]  << ", " 
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
		return nStatus;//�ڴ��쳣
	}
	char pcFileName[30] = "AllSampleResult.txt";

	strcpy(pcFileFullName, pcFilePath);
	strcat(pcFileFullName, pcFileName);

	FILE *fp = fopen(pcFileFullName, "a");   //û������ļ������
	if ( fp )
	{
		//ͼ�����Ļ�ȡ
		fprintf(fp, "%-30s\t", pcVidFileName);//�ļ���

		fprintf(fp, "%4d\t", dataOut->nTotalSpermNum);//1.��������
		fprintf(fp, "%6.2f\t", dataOut->dTotaSpermDensity);//2.�����ܶ�
		fprintf(fp, "%4d\t", dataOut->nAliveSpermNum);//3.��ľ�����
		fprintf(fp, "%6.2f\t", dataOut->dAliveSpermDensity);//4.������ܶ�
		fprintf(fp, "%6.3f\t", dataOut->dAliveSpermRatio);//5.���ӻ���
		fprintf(fp, "%6.3f\t", dataOut->dActiveSpermRatio);//6.���ӻ���

		fprintf(fp, "%6.2f\t", dataOut->dVelcoitySL);//7.ƽ��ֱ���˶��ٶ�
		fprintf(fp, "%4d\t", dataOut->nNumSL);//8.ֱ���˶���������
		fprintf(fp, "%6.3f\t", dataOut->dRatioSL);//9.ֱ���˶�����

		fprintf(fp, "%6.2f\t", dataOut->dVelcoityCL);//10.ƽ�������˶��ٶ�
		fprintf(fp, "%4d\t", dataOut->nNumCL);//11.�����˶���������
		fprintf(fp, "%6.3f\t", dataOut->dRatioCL);//12.�����˶�����

		fprintf(fp, "%6.2f\t", dataOut->dVelcoityAP);//13.ƽ��·���˶��ٶ�
		fprintf(fp, "%4d\t", dataOut->nNumAP);//14.·���˶���������
		fprintf(fp, "%6.3f\t", dataOut->dRatioAP);//15.·���˶�����

		fprintf(fp, "%6.3f\t", dataOut->dRatioClassA);//16.�����˶��ٶȷּ�ͳ�ƣ�A��
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassB);//17.�����˶��ٶȷּ�ͳ�ƣ�B��
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassC);//18.�����˶��ٶȷּ�ͳ�ƣ�C��
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassD);//19.�����˶��ٶȷּ�ͳ�ƣ�D��

		fprintf(fp, "%6.3f\t", dataOut->dRatioClassAB);//20.ǰ���˶����˶���Ծ��PR��
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassCSlowD);//21.��ǰ���˶������˶���Ծ��NP��
		fprintf(fp, "%6.3f\t", dataOut->dRatioClassDeadD);//22.��������ȫ������IM��

		fputc('\n', fp);
		fclose(fp);
	}
	else
	{
		nStatus = -17;//��������ļ�ʧ��
	}

	free(pcFileFullName);
	pcFileFullName = NULL;
	
	return nStatus;
}
*/