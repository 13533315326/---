#include <opencv2/opencv.hpp>
#include <iostream>
using namespace cv;
using namespace std;

VideoCapture cap(1);                                                                                     //��USB����ͷ

#define KNOWN_DISTANCE  30
#define KNOWN_WIDTH    10
#define KNOWN_HEIGHT   10
#define KNOWN_FOCAL_LENGTH  500 
double FocalLength = 0.0;                                                                                //��������ر���
int main()
{
	RNG rng;
	//1.kalman filter setup  
	const int stateNum = 4;                                                                             //״ֵ̬4��1����(x,y,��x,��y)  
	const int measureNum = 2;                                                                           //����ֵ2��1����(x,y)    
	KalmanFilter KF(stateNum, measureNum, 0);                                                           //���忨�����˲���

	KF.transitionMatrix = (Mat_<float>(4, 4) << 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1);        //ת�ƾ���A  
	setIdentity(KF.measurementMatrix);                                                                  //��������H  
	setIdentity(KF.processNoiseCov, Scalar::all(1));                                                    //ϵͳ�����������Q  
	setIdentity(KF.measurementNoiseCov, Scalar::all(1e-60));                                            //���������������R  
	setIdentity(KF.errorCovPost, Scalar::all(10000));                                                   //����������Э�������P    
	Mat measurement = Mat::zeros(measureNum, 1, CV_32F);                                                //��ʼ����ֵx'(0)����Ϊ����Ҫ�������ֵ�����Ա����ȶ���  



	while (1)
	{
		Mat SrcImage;                                                                                    
		cap >> SrcImage;                                                                                //��֡ץȡ����ͷ��ȡ����
		Mat orangeROI = SrcImage.clone();                                  
		Mat kernel = getStructuringElement(MORPH_RECT, Size(4, 4), Point(-1, -1));                      
		inRange(orangeROI, Scalar(20, 43, 46), Scalar(25, 255, 255), orangeROI);                        //�����ɫ
		threshold(orangeROI, orangeROI, 240, 245, THRESH_BINARY);                                       //��ֵ��
		blur(orangeROI, orangeROI, Size(4, 4));                                                         //ģ����
		dilate(orangeROI, orangeROI, kernel, Point(-1, -1), 8);                                         //����
		vector<RotatedRect> vc;
		vector<RotatedRect> vRec;
		vector<vector<Point>>Light_Contour;
		findContours(orangeROI, Light_Contour, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);                     //�ҳ���ɫ�������
		for (int i = 0; i < Light_Contour.size(); i++)
		{
			
			float Light_Contour_Area = contourArea(Light_Contour[i]);                                   //���������        
			
			if (Light_Contour_Area < 15 || Light_Contour[i].size() <= 20)                               //ȥ����С����fitllipse����������
				continue;
			
			RotatedRect Light_Rec = fitEllipse(Light_Contour[i]);                                       // ����Բ�������õ���Ӿ���
			
					
			Light_Rec.size.height *= 1.1;
			Light_Rec.size.width *= 1.1;                                                                // ������������
			vc.push_back(Light_Rec);
		}

		
		for (size_t i = 0; i < vc.size(); i++)                                                          //�ӵ������������ɸѡ����
		{
			for (size_t j = i + 1; (j < vc.size()); j++)
			{
				
				
				float Contour_Len1 = abs(vc[i].size.height - vc[j].size.height) / max(vc[i].size.height, vc[j].size.height);//���Ȳ����
				
				float Contour_Len2 = abs(vc[i].size.width - vc[j].size.width) / max(vc[i].size.width, vc[j].size.width);//��Ȳ����
				


				RotatedRect ARMOR;                                                                        //����װ�װ����Ӿ���
				ARMOR.center.x = (vc[i].center.x + vc[j].center.x) / 2.;                                  //x����
				ARMOR.center.y = (vc[i].center.y + vc[j].center.y) / 2.;                                  //y����
				ARMOR.angle = (vc[i].angle + vc[j].angle) / 2.;                                           //�Ƕ�
				float nh, nw, yDiff, xDiff;
				nh = (vc[i].size.height + vc[j].size.height) / 2;                                         //�߶�
				
				nw = sqrt((vc[i].center.x - vc[j].center.x) * (vc[i].center.x - vc[j].center.x) + (vc[i].center.y - vc[j].center.y) * (vc[i].center.y - vc[j].center.y));// ���
				float ratio = nw / nh;                                                                    //ƥ�䵽��װ�װ�ĳ����
				xDiff = abs(vc[i].center.x - vc[j].center.x) / nh;                                        //x�����
				yDiff = abs(vc[i].center.y - vc[j].center.y) / nh;                                        //y�����
				if (ratio < 1.0 || ratio > 5.0 || xDiff < 0.5 || yDiff > 2.5)
					continue;
				ARMOR.size.height = nh;
				ARMOR.size.width = nw;
				vRec.push_back(ARMOR);
				Point2f point1;
				Point2f point2;
				point1.x = vc[i].center.x; point1.y = vc[i].center.y + 20;
				point2.x = vc[j].center.x; point2.y = vc[j].center.y - 20;
				int xmidnum = (point1.x + point2.x) / 2;
				int ymidnum = (point1.y + point2.y) / 2;

				  
				measurement.at<float>(0) = (float)ARMOR.center.x;
				measurement.at<float>(1) = (float)ARMOR.center.y;                                        //update measurement

				
				KF.correct(measurement);                                                                 //update  
				Mat prediction = KF.predict();
				Point predict_pt = Point(prediction.at<float>(0), prediction.at<float>(1));   //Ԥ��ֵ(x',y')  
				FocalLength = (ARMOR.size.height * KNOWN_DISTANCE) / KNOWN_WIDTH;
				double DistanceInches = 0.0;
				DistanceInches = (KNOWN_WIDTH * FocalLength) / ARMOR.size.width;
				DistanceInches = DistanceInches * 2.54;
				putText(SrcImage, format("Distance(CM):%f", DistanceInches), Point(10, 30), FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0, 0, 255), 1, LINE_8);
				
				Scalar color(0, 255, 255);
				rectangle(SrcImage, point1, point2, color, 2);//��װ�װ������
				circle(SrcImage, ARMOR.center, 10, color);//��װ�װ����Ļ�һ��Բ
				circle(SrcImage, predict_pt, 10, color);
			}
		}

		//���ųɹ�
		namedWindow("SrcImage", 0);
		imshow("SrcImage", SrcImage);
		//imshow("SrcImage", orangeROI);
		waitKey(1);
	}
	return 0;
}




