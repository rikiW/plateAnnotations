#include <iostream>
#include "json/config.h"
#include "json/json.h"
#include <iomanip> 
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable 
#define OPENCV
#include "vehicle_feature.hpp"
#include "cv_process.hpp"	// imported functions from DLL

#ifdef OPENCV
#include <opencv2/opencv.hpp>			// C++
#include "opencv2/core/version.hpp"
#ifndef CV_VERSION_EPOCH
#include "opencv2/videoio/videoio.hpp"
#pragma comment(lib, "opencv_world320.lib")  
#else
#pragma comment(lib, "opencv_core2413.lib")  
#pragma comment(lib, "opencv_imgproc2413.lib")  
#pragma comment(lib, "opencv_highgui2413.lib") 
#endif

using namespace std;

void write_to_xml_head(ofstream &out,string filename,int width,int height,int depth){
	
    out<<"<annotation>"<<endl; 	
	out<<"  <folder>JPEGImages</folder>"<<endl; 
	out<<"  <filename>" <<filename<< "</filename>" <<endl; 
	out<<"  <path>/home/fs/"<< filename<<".jpg</path>" <<endl; 
	out<<"  <source>" <<endl; 
	out<<"    <database>Unknown</database>" <<endl; 
	out<<"  </source>" <<endl; 
	out<<"  <size>" <<endl; 
	stringstream w;
	w<<"    <width>" << width << "</width>" ;
	out<< w.str() << endl; 
	stringstream h;
	h<<"    <height>"<< height << "</height>";
	out<< h.str() <<endl; 
	stringstream d;
	d<<"    <depth>"<< depth << "</depth>";
	out<< d.str() <<endl; 
    out<<"  </size>" <<endl; 
	out<<"  <segmented>0</segmented>" <<endl;  
 
}

void write_to_xml_body(ofstream &out,string className,int xmin,int ymin,int xmax,int ymax){
 
    out<<"  <object>"<<endl; 	
	out<<"    <name>" <<className<<"</name>"<<endl; 
	out<<"    <pose>Unspecified</pose>"<<endl; 
	out<<"    <truncated>0</truncated>"<<endl; 
	out<<"    <difficult>0</difficult>"<<endl; 
	out<<"    <bndbox>"<<endl; 
	stringstream xmi;
	xmi<<"      <xmin>" << xmin <<"</xmin>";
	out<< xmi.str()<< endl;
	stringstream ymi;
	ymi<<"      <ymin>" << ymin <<"</ymin>";
	out<< ymi.str()<< endl;
	stringstream xma;
	xma<<"      <xmax>" << xmax <<"</xmax>";
	out<< xma.str()<< endl;
	stringstream yma;
	yma<<"      <ymax>" << ymax <<"</ymax>";
	out<< yma.str()<< endl;
	out<<"    </bndbox>"<<endl; 
    out<<"  </object>"<<endl;  

}

void write_to_xml_end(ofstream &out){

	out<<"</annotation>"<<endl;
              
}


void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names,
	unsigned int wait_msec = 0, int current_det_fps = -1, int current_cap_fps = -1)
{
	int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };

	for (auto &i : result_vec) {
		int const offset = i.obj_id * 123457 % 6;
		int const color_scale = 150 + (i.obj_id * 123457) % 100;
		cv::Scalar color(colors[offset][0], colors[offset][1], colors[offset][2]);
		color *= color_scale;
		cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 5);
		
		if (obj_names.size() > i.obj_id) {
			std::string obj_name = obj_names[i.obj_id];	
			
			if (i.track_id > 0) obj_name += " - " + std::to_string(i.track_id);			
			cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, 2, 0);
			int const max_width = (text_size.width > i.w + 2) ? text_size.width : (i.w + 2);
			cv::rectangle(mat_img, cv::Point2f(std::max((int)i.x - 3, 0), std::max((int)i.y - 30, 0)), 
				cv::Point2f(std::min((int)i.x + max_width, mat_img.cols-1), std::min((int)i.y, mat_img.rows-1)), 
				color, CV_FILLED, 8, 0);
			putText(mat_img, obj_name, cv::Point2f(i.x, i.y - 10), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(0, 0, 0), 2);
		}
	}
	if (current_det_fps >= 0 && current_cap_fps >= 0) {
		std::string fps_str = "FPS detection: " + std::to_string(current_det_fps) + "   FPS capture: " + std::to_string(current_cap_fps);
		putText(mat_img, fps_str, cv::Point2f(10, 20), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(50, 255, 0), 2);
	}
	cv::imshow("window name", mat_img);
	cv::waitKey(wait_msec);
}
#endif	// OPENCV


void show_console_result(std::vector<bbox_t> const result_vec, std::vector<std::string> const obj_names) {
	for (auto &i : result_vec) {
		if (obj_names.size() > i.obj_id) std::cout << obj_names[i.obj_id] << " - ";
		std::cout << "obj_id = " << i.obj_id << ",  x = " << i.x << ", y = " << i.y 
			<< ", w = " << i.w << ", h = " << i.h
			<< std::setprecision(3) << ", prob = " << i.prob << std::endl;
	}
}

std::vector<std::string> objects_names_from_file(std::string const filename) {
	std::ifstream file(filename);
	std::vector<std::string> file_lines;
	if (!file.is_open()) return file_lines;
	for(std::string line; getline(file, line);) file_lines.push_back(line);
	std::cout << "object names loaded \n";
	return file_lines;
}

//get model,test car,return label
int main(int argc, char *argv[])
{
	std::string filename;
	
	if (argc > 1) filename = argv[1];

    //Detector detector("autoLabel.cfg", "autoLabel_32000.weights");
    Detector detector(VehicleDetection,0,416,416);
    Classfier vechile_classifier(VechileTypeClassification, 0, 224, 224);
    Classfier color_classifier(ColorClassification, 0, 224, 224);

    //densenet201 157
    Classfier brand_classifier(BrandClassification, 0, 224, 224);
    Classfier plateType_classifier(PlateTypeClassification, 0, 224, 224);

    //plate detection and plate recognition
    Detector recognize_plate(PlateRecognition, 0, 224, 640);
    Detector vehicle_detection(PlateDetecton, 0, 416, 416);

	auto obj_names = objects_names_from_file("data/voc.names");
	std::string out_videofile = "data/tr.mp4";
	bool const save_output_videofile = false;

	   
			        
	while (true) 
	{		

   
		std::string imgJsonData;
        std::string filename;
        std::string encoded_string;
    
        VechicleFeature finalFeature; 
        float confidencePlate = 0.0;

		std::cout << "input image or video filename: ";
		if(filename.size() == 0) std::cin >> filename;
		if (filename.size() == 0) break;
		
		try {
#ifdef OPENCV
			std::string const file_ext = filename.substr(filename.find_last_of(".") + 1);
			std::string const protocol = filename.substr(0, 7);
			if (file_ext == "avi" || file_ext == "mp4" || file_ext == "mjpg" || file_ext == "mov" || 	// video file
				protocol == "rtsp://" || protocol == "http://" || protocol == "https:/")	// video network stream
			{
				cv::Mat cap_frame, cur_frame, det_frame, write_frame;
				std::shared_ptr<image_t> det_image;
				std::vector<bbox_t> result_vec, thread_result_vec;
							
				detector.nms = 0.02;	// comment it - if track_id is not required
				std::atomic<bool> consumed, videowrite_ready;
				consumed = true;
				videowrite_ready = true;
				std::atomic<int> fps_det_counter, fps_cap_counter;
				fps_det_counter = 0;
				fps_cap_counter = 0;
				int current_det_fps = 0, current_cap_fps = 0;
				std::thread t_detect, t_cap, t_videowrite;
				std::mutex mtx;
				std::condition_variable cv;
				std::chrono::steady_clock::time_point steady_start, steady_end;
				cv::VideoCapture cap(filename); 
				
				cap >> cur_frame;//获取视频帧
				int const video_fps = cap.get(CV_CAP_PROP_FPS);//获取的频率
				cv::Size const frame_size = cur_frame.size();
				cv::VideoWriter output_video;
				if (save_output_videofile)
					output_video.open(out_videofile, CV_FOURCC('D', 'I', 'V', 'X'), std::max(35, video_fps), frame_size, true);
				
				//保存图片每一帧
				int num = 100000;
				
 
				while (!cur_frame.empty()) {   		
				
					if (t_cap.joinable()) {
						t_cap.join();
						++fps_cap_counter;
						cur_frame = cap_frame.clone();
					}
					t_cap = std::thread([&]() { cap >> cap_frame; });

					// swap result and input-frame
					if(consumed)
					{
						std::unique_lock<std::mutex> lock(mtx);
						det_image = detector.mat_to_image_resize(cur_frame);
 					//test
						
						result_vec = thread_result_vec;
						result_vec = detector.tracking(result_vec);	// comment it - if track_id is not required

						consumed = false;
					}
					// launch thread once
					if (!t_detect.joinable()) {
						t_detect = std::thread([&]() {
							auto current_image = det_image;
							consumed = true;
							while (current_image.use_count() > 0) {
								auto result = detector.detect_resized(*current_image, frame_size, 0.3, true);
								++fps_det_counter;
								std::unique_lock<std::mutex> lock(mtx);
								thread_result_vec = result;
								current_image = det_image;
								consumed = true;
								cv.notify_all();
								
				
							}
						});
					}

					if (!cur_frame.empty()) {
						steady_end = std::chrono::steady_clock::now();
						if (std::chrono::duration<double>(steady_end - steady_start).count() >= 1) {
							current_det_fps = fps_det_counter;
							current_cap_fps = fps_cap_counter;
							steady_start = steady_end;
							fps_det_counter = 0;
							fps_cap_counter = 0;
						}
						
						draw_boxes(cur_frame, result_vec, obj_names, 3, current_det_fps, current_cap_fps);
						//show_console_result(result_vec, obj_names);
						
			 		
						
						if(!cur_frame.empty())
				       {

					     //保存的图片名

						stringstream con;
				        con << "/home/riki/riki_annotationSDK/pic/" << num <<".png" ;
				        con.str();
                        imwrite(con.str(), cap_frame);//保存保存一帧图片    
					
					    ostringstream oss;
						ostringstream os;
						os <<""<<num;
						 
						string s = "/home/riki/riki_annotationSDK/Annotations/";
						string b = ".xml";
						oss << s << num << b;

						string filename2=os.str();
						string filename3=oss.str();	//保留后的纯文件名	

						int width =  cur_frame.cols;
						int height =  cur_frame.rows;
						int depth=3;
																
					   
						ofstream out(filename3); 
								
					    bool op=out.is_open();
						  if (op)   
                    { 
					
						write_to_xml_head(out,filename2,width, height, depth); //写入文件头 
						 
						
                        Json::Value arr;//定义存放图片的数组
                        Json::Value json_temp1;
         
						json_temp1["file_name"]= Json::Value(filename);  
						json_temp1["height"]= Json::Value(height);
						json_temp1["width"]= Json::Value(width);
						json_temp1["depth"]= Json::Value(depth);
					    arr["info"] =json_temp1;//存放视频的信息

						for (auto &b : result_vec){//遍历result_vec生成xml文件

						std::string className = obj_names[b.obj_id];	
					
						int left = b.x;
						int right= b.x+b.w;
						int top=b.y;
						int bot=b.y+b.h;	

						stringstream con1;
						stringstream con2;
				        con1 << "/home/riki/riki_annotationSDK/pic1/"<<num<<".png" ;
				        con2 << num <<".png";
				        con2.str();
				        con1.str();
                       
						cv::Mat src, image_src;                 //原图      
						cv::Mat imageROI;                       //ROI区域  
						cv::Mat TempImg;                        //裁取出的区域存储为Mat  
						int x_begin, y_begin, swidth, sheight;        //裁取区域的坐标及大小  
						int srcWidth, srcHeight;                    //存储原图宽、高  
						 //赋初值  
						x_begin = b.x;    
						y_begin = b.y;  
						swidth = b.w;  
						sheight = b.h;  
						  
						//读取原图  
						src =cv::imread(con.str(), 1);      
						if(!src.data){  
						    cout<<" image read error!"<<endl;  
						    return -1;   
						}  

						srcWidth = src.cols;    //获取原图宽、高
						srcHeight = src.rows;  
						  
						//控制裁取区域不超过原图  
						 
						if(x_begin + swidth > srcWidth)         
						    swidth = srcWidth - x_begin;  
						if(y_begin + sheight > srcHeight)  
						    sheight = srcHeight - y_begin;  
						  
						//区域裁取  
						image_src = src.clone();            //备份原图  
						imageROI = image_src(cv::Rect(x_begin, y_begin, swidth, sheight));    //设置待裁取ROI  
						imageROI.convertTo(TempImg, TempImg.type());        //将ROI区域拷贝至dst
						 
                
				    	std::vector<classes_t> vechile_prediction = vechile_classifier.classfier(TempImg);//把截取下来的目标进行分类
				    	std::vector<classes_t> color_prediction = color_classifier.classfier(TempImg);
         				std::vector<classes_t> brand_prediction = brand_classifier.classfier(TempImg);
         		       /* std::vector<bbox_t> result_vec1 = vehicle_detection.detect(src, 0.35); */

         				
						Json::Value json_temp;      // 用来存储每一个小目标的信息
						
                   
                             for (auto &vec : vechile_prediction) 
              {
			            finalFeature.setId(con1.str());


                }

						   	 for (auto &vec : vechile_prediction) 
              {
			            className=obj_names_classifier[vec.obj_id];   //将分类后的目标真实名字返回给className
			           /* write_to_xml_body(out,className, left, top, right, bot);//写入文件中间部分*/	
   						/*json_temp["class"]= Json::Value(className);*/
   						finalFeature.setType(obj_names_classifier[vec.obj_id]);
   					 
                }
						    for (auto &vec : color_prediction)
              {
                       /* string color=obj_names_color[vec.obj_id];
                        json_temp["color"]= Json::Value(color);*/
                        finalFeature.setColor(obj_names_color[vec.obj_id]);
              
                }
			                for (auto &vec : brand_prediction)
			  {
			           /* string brand=carBrand[vec.obj_id];
			            json_temp["brand"]= Json::Value(brand);*/
			            finalFeature.setModel(carBrand[vec.obj_id]);  
			       }
			
			       
			  		std::vector<bbox_t> result_vec = vehicle_detection.detect(TempImg, 0.35); 

			        finalFeature.setResultVec(result_vec);
			        finalFeature.setOutMethod(topConfi);
					plate_recognize(plateType_classifier, TempImg, recognize_plate, finalFeature);

			        Json::Value root;
			        root["id"] = Json::Value(finalFeature.getId());
			        root["bbox"].append(left);  
					root["bbox"].append(right);
					root["bbox"].append(top); 
					root["bbox"].append(bot);
			        root["licenseNo"] = Json::Value(finalFeature.getLicenseNo());
			        root["licenseType"] = Json::Value(finalFeature.getLicenseType());
			        root["color"] = Json::Value(finalFeature.getColor());
					
					 if (finalFeature.getLicenseNo() == "NULL")
          {
            confidencePlate =  0.0;
          }
                  root["confidence"] = Json::Value((int)(finalFeature.getConfidence()*100));
                    stringstream temp;
				    temp << "/home/riki/riki_annotationSDK/pic1/" << finalFeature.getLicenseNo() <<"_"<<finalFeature.getConfidence()<<".png" ;
				    temp.str();

				      for (auto &vec : result_vec) 
			{
				    imwrite(temp.str(),TempImg); 
							
							}

			        if(finalFeature.getType() == "CAR"){
			        root["model"] = Json::Value(finalFeature.getModel());
			        }
			        else{
			        root["model"] = Json::Value("UNKNOWN");}
			        root["type"] = Json::Value(finalFeature.getType());

						
						arr["annotations"].append(Json::Value(root));
                        num+=1;
                        
                    	Json::StyledWriter sw;
					    std::string res_str = sw.write(arr);
					    std::cout << res_str << std::endl;
					    

						}	

						write_to_xml_end(out);//写文件尾部
						out.close();
						
					}  
				
						}

			
						if (output_video.isOpened() && videowrite_ready) {
							if (t_videowrite.joinable()) t_videowrite.join();
							write_frame = cur_frame.clone();
							videowrite_ready = false;
							t_videowrite = std::thread([&]() { 
								 output_video << write_frame; videowrite_ready = true;
							});
						}
					}

					// wait detection result for video-file only (not for net-cam)
					if (protocol != "rtsp://" && protocol != "http://" && protocol != "https:/") {
						std::unique_lock<std::mutex> lock(mtx);
						while (!consumed) cv.wait(lock);
					}
				}
				if (t_cap.joinable()) t_cap.join();
				if (t_detect.joinable()) t_detect.join();
				if (t_videowrite.joinable()) t_videowrite.join();
				std::cout << "Video ended \n";
			}
			else if (file_ext == "txt") {	// list of image files
				std::ifstream file(filename);
				if (!file.is_open()) std::cout << "File not found! \n";
				else 
					for (std::string line; file >> line;) {
						std::cout << line << std::endl;
						cv::Mat mat_img = cv::imread(line);
						std::vector<bbox_t> result_vec = detector.detect(mat_img);
						show_console_result(result_vec, obj_names);
						
						//draw_boxes(mat_img, result_vec, obj_names);
						//cv::imwrite("res_" + line, mat_img);
					}
				
			}
			else {	// image file
				cv::Mat mat_img = cv::imread(filename);
				std::vector<bbox_t> result_vec = detector.detect(mat_img);
				result_vec = detector.tracking(result_vec);	// comment it - if track_id is not required
				draw_boxes(mat_img, result_vec, obj_names);
				//test
				show_console_result(result_vec, obj_names);
				
			
			}
#else
		//	std::vector<bbox_t> result_vec = detector.detect(filename);//

			auto img = detector.load_image(filename);
			std::vector<bbox_t> result_vec = detector.detect(img);
			detector.free_image(img);
			show_result(result_vec, obj_names,);
				
#endif			
		}
		catch (std::exception &e) { std::cerr << "exception: " << e.what() << "\n"; getchar(); }
		catch (...) { std::cerr << "unknown exception \n"; getchar(); }
		filename.clear();
	}

	return 0;
}

