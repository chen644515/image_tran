#include <ros/ros.h>
#include <sensor_msgs/Image.h>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <queue>
#include <vector>
#include <iostream>
#include "threadpool.hpp"

#define SERVER_IP "192.168.168.101"

using namespace std;
using namespace cv;
class ImageTCPSend {
public:
    cv::Mat img;
    ImageTCPSend(const char *IP, uint16_t p);
    int init();
    int process();
    ~ImageTCPSend() {
        close(this->sock);
    }
private:
    const char *serverIP;
    uint16_t port;
    int sock;
    struct sockaddr_in serverAddr;
};

ImageTCPSend::ImageTCPSend(const char *IP, uint16_t p) : serverIP(IP) 
{
    port = p;
}

int ImageTCPSend::init() {
    std::cout << "in\n";
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cout << "sock failded\n";
    }
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP, &serverAddr.sin_addr) <= 0) {
        std::cout << "Error: inet_pton failed.\n";
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cout << "Error: Connection failed.\n";
        close(sock);
        return 1;
    }
    return 0;
}

int ImageTCPSend::process() 
{
    // cout << "iniiiiiiiiiiiiiii\n";
    vector<uchar> decode;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 60};
    imencode(".jpg", img, decode, params);
    uint32_t len = decode.size();
    // std::cout << "size:" << len << '\n';
    vector<uchar> buffer(len + 8);
    buffer[0] = 'B';
    buffer[1] = 'F';
    buffer[2] = 'B';
    buffer[3] = 'F';
    buffer[4] = static_cast<unsigned char>((len) & 0xFF);
    buffer[5] = static_cast<unsigned char>((len >> 8) & 0xFF);
    buffer[6] = static_cast<unsigned char>((len >> 16) & 0xFF);
    buffer[7] = static_cast<unsigned char>((len >> 24) & 0xFF);
    // std::cout << "head:" << buffer[0] << buffer[1] << buffer[2] << buffer[3] << (uint8_t)buffer[4] << (uint8_t)buffer[5] <<'\n';

    // std::cout << "buffer.size:" << buffer.size() << "\n";
    for (int i = 0; i < len; i++) {
        // std::cout << i << ' ';
        buffer[i + 8] = decode[i];
    }
    if (send(sock, buffer.data(), buffer.size(), 0) == -1) {
        std::cout << "Error: Failed to send image data.\n";
        return -1;
    }
    return 0;
}

// #define PORT1 12348
class AskRev {
public:
    AskRev(uint16_t);
    int init();
    int run();
    ~AskRev() {
        // 关闭套接字
        close(newSocket);
        close(serverSock);
    }
private:
    uint16_t port;
    int serverSock, newSocket;
    struct sockaddr_in serverAddr;
    struct sockaddr_storage serverStorage;
    socklen_t addr_size;
};

AskRev::AskRev(uint16_t p) 
{
    port = p;
}

int AskRev::init()
{
    // 创建 TCP 套接字
    serverSock = socket(AF_INET, SOCK_STREAM, 0);

    // 设置服务器地址
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字
    bind(serverSock, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
    // 监听传入的连接
    if (listen(serverSock, 10) == 0) {
        cout << "Listening for incoming connections..." << endl;
    } else {
        cout << "Failed to listen" << endl;
        return -1;
    }
    // 等待客户端连接
    addr_size = sizeof(serverStorage);
    newSocket = accept(serverSock, (struct sockaddr *) &serverStorage, &addr_size);
    cout << "Client connected" << endl;
    return 0;
}

int AskRev::run()
{
    int ret;
    vector<uchar> rev(2);
    for (int i = 0; i < 2; i++) {
        uchar s;
        ret = recv(newSocket, &s, 1, 0);
        rev[i] = s;
    }
    if (rev[0] == 'O' && rev[1] == 'K') return 0;
    return 0;
}


struct Img
{
    double time1;
    double time2;
    cv::Mat img1;
    cv::Mat img2;
};
Img image;
// std::queue<Img> q;
int flag = 0;
// 图像回调函数
void imageCallback_left(const sensor_msgs::ImageConstPtr& msg)
{
    // std::cout << "in3\n";
    try
    {
       cout << flag << '\n'; 
        // 将ROS图像消息转换为OpenCV图像
        if (((flag >> 1) & 1) == 0) {
            // cout << "in1\n";
            cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::MONO8);
            image.img1 = cv_ptr->image;
            ros::Time timestamp = msg->header.stamp;
            image.time1 = timestamp.toSec();
            flag |= 2;
            // if (flag == 3) {
            //     q.push(mk);
            //     flag  = 0;
            // }
            
        }
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
    }
}

void imageCallback_right(const sensor_msgs::ImageConstPtr& msg)
{
    // std::cout << "in4\n";
    try
    {
        cout << flag << '\n'; 
        if (((flag) & 1) == 0) {
            // cout << "in2\n";
            cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::MONO8);
            image.img2 = cv_ptr->image;
            flag |= 1;
            ros::Time timestamp = msg->header.stamp;
            image.time2 = timestamp.toSec();
            // if (flag == 3) {
            //     q.push(mk);
            //     flag  = 0;
            // }
        
        }
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
    }
}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "image_subscriber");
    ros::NodeHandle nh;

    // 订阅图像话题
    // ros::Subscriber sub_left = nh.subscribe("/camera/fisheye1/image_raw", 10, imageCallback_left);
    // ros::Subscriber sub_right = nh.subscribe("/camera/fisheye2/image_raw", 10, imageCallback_right);

    // 初始化OpenCV窗口
    // cv::namedWindow("Fisheye Image", cv::WINDOW_AUTOSIZE);

    ImageTCPSend imgSend1(SERVER_IP, 8881);
    ImageTCPSend imgSend2(SERVER_IP, 8882);
    ImageTCPSend imgSend3(SERVER_IP, 8883);
    ImageTCPSend imgSend4(SERVER_IP, 8884);
    ImageTCPSend imgSend5(SERVER_IP, 8885);
    ImageTCPSend imgSend6(SERVER_IP, 8886);
    ImageTCPSend imgSend7(SERVER_IP, 8887);
    ImageTCPSend imgSend8(SERVER_IP, 8888);
    std::cout << "start init socket\n";
    if (imgSend1.init() != 0) {
        std::cout << "imgSend1 failed\n";
        return 1;
    } else {
        std::cout << "imgSend1 successfully\n";
    }
    usleep(1000000);

    if (imgSend2.init() != 0) {
        std::cout << "imgSend2 failed\n";
        return 1;
    } else {
        std::cout << "imgSend2 successfully\n";
    }
    usleep(1000000);
    if (imgSend3.init() != 0) {
        std::cout << "imgSend3 failed\n";
        return 1;
    } else {
        std::cout << "imgSend3 successfully\n";
    }
    usleep(1000000);
    if (imgSend4.init() != 0) {
        std::cout << "imgSend4 failed\n";
        return 1;cout << "33333333333333333333\n";
    } else {
        std::cout << "imgSend4 successfully\n";
    }
    std::cout << "1111111\n";
    usleep(1000000);
    if (imgSend5.init() != 0) {
        std::cout << "imgSend5 failed\n";
        return 1;
    } else {
        std::cout << "imgSend5 successfully\n";
    }
    usleep(1000000);

    if (imgSend6.init() != 0) {
        std::cout << "imgSend6 failed\n";
        return 1;
    } else {
        std::cout << "imgSend6 successfully\n";
    }
    usleep(1000000);
    if (imgSend7.init() != 0) {
        std::cout << "imgSend7 failed\n";
        return 1;
    } else {
        std::cout << "imgSend7 successfully\n";
    }
    usleep(1000000);
    if (imgSend8.init() != 0) {
        std::cout << "imgSend8 failed\n";
        return 1;
    } else {
        std::cout << "imgSend8 successfully\n";
    }

    AskRev askrev1(9991);
    AskRev askrev2(9992);
    AskRev askrev3(9993);
    AskRev askrev4(9994);
    AskRev askrev5(9995);
    AskRev askrev6(9996);
    AskRev askrev7(9997);
    AskRev askrev8(9998);





    if (askrev1.init() != 0) {
        std::cout << "askrev1 failed\n";
        return 1;
    } else {
        std::cout << "askrev1 successfully\n";
    }

    if (askrev2.init() != 0) {
        std::cout << "askrev2 failed\n";
        return 1;
    } else {
        std::cout << "askrev2 successfully\n";
    }

    if (askrev3.init() != 0) {
        std::cout << "askrev3 failed\n";
        return 1;
    } else {
        std::cout << "askrev3 successfully\n";
    }

    if (askrev4.init() != 0) {
        std::cout << "askrev4 failed\n";
        return 1;
    } else {
        std::cout << "askrev4 successfully\n";
    }

    if (askrev5.init() != 0) {
        std::cout << "askrev5 failed\n";
        return 1;
    } else {
        std::cout << "askrev5 successfully\n";
    }

    if (askrev6.init() != 0) {
        std::cout << "askrev6 failed\n";
        return 1;
    } else {
        std::cout << "askrev6 successfully\n";
    }

    if (askrev7.init() != 0) {
        std::cout << "askrev7 failed\n";
        return 1;
    } else {
        std::cout << "askrev7 successfully\n";
    }

    if (askrev8.init() != 0) {
        std::cout << "askrev8 failed\n";
        return 1;
    } else {
        std::cout << "askrev8 successfully\n";
    }

    //创建线程池
    threadpool<ImageTCPSend> *pool = NULL;
    try
    {
        pool = new threadpool<ImageTCPSend>(8, 8);
    }
    catch (...)
    {
        return 1;
    }

    ros::Subscriber sub_left = nh.subscribe("/camera/fisheye1/image_raw", 10, imageCallback_left);
    ros::Subscriber sub_right = nh.subscribe("/camera/fisheye2/image_raw", 10, imageCallback_right);
    // imgSend1.img = cv::Mat(halfHeight, halfWidth, CV_8UC1);
    // imgSend2.img = cv::Mat(halfHeight, halfWidth, CV_8UC1);
    // imgSend3.img = cv::Mat(halfHeight, halfWidth, CV_8UC1);
    // imgSend4.img = cv::Mat(halfHeight, halfWidth, CV_8UC1);
    // imgSend5.img = cv::Mat(halfHeight, halfWidth, CV_8UC1);
    // imgSend6.img = cv::Mat(halfHeight, halfWidth, CV_8UC1);
    // imgSend7.img = cv::Mat(halfHeight, halfWidth, CV_8UC1);
    // imgSend8.img = cv::Mat(halfHeight, halfWidth, CV_8UC1);
    // 进入ROS事件处理循环
    while (ros::ok()) {
        ros::spinOnce();
        // cout << 1 << '\n';
        // if (q.empty()) continue;
        // Img image = q.front();
        // q.pop();
        // cout << "time:" << image.time1 - image.time2 << '\n';
        if (flag == 3) {
            
            // cout << "send\n";
            int height = image.img1.rows;
            int width = image.img1.cols;
            int halfHeight = height / 2;
            int halfWidth = width / 2;
            // cout << height << ' ' << width << ' ' << halfHeight << ' ' << halfWidth << '\n';
            // cv::imshow("Fisheye Image", image.img1);
            // cv::waitKey(0); 
            cout << imgSend1.img.channels() << '\n';
            // cout << "iiiiiiiiiiiiiiiiiiiiiiiiiiii\n";
            imgSend1.img = image.img1(Rect(0, 0, halfWidth, halfHeight));
            imgSend2.img = image.img1(Rect(halfWidth, 0, width - halfWidth, halfHeight));
            imgSend3.img = image.img1(Rect(0, halfHeight, halfWidth, height - halfHeight));
            imgSend4.img = image.img1(Rect(halfWidth, halfHeight, width - halfWidth, height - halfHeight));
            imgSend5.img = image.img2(Rect(0, 0, halfWidth, halfHeight));
            imgSend6.img = image.img2(Rect(halfWidth, 0, width - halfWidth, halfHeight));
            imgSend7.img = image.img2(Rect(0, halfHeight, halfWidth, height - halfHeight));
            imgSend8.img = image.img2(Rect(halfWidth, halfHeight, width - halfWidth, height - halfHeight));
            pool->append(&imgSend1);
            pool->append(&imgSend2);
            pool->append(&imgSend3);
            pool->append(&imgSend4);
            pool->append(&imgSend5);
            pool->append(&imgSend6);
            pool->append(&imgSend7);
            pool->append(&imgSend8);

            if (askrev1.run() != 0 ||
                askrev2.run() != 0 ||
                askrev3.run() != 0 ||
                askrev4.run() != 0 ||
                askrev5.run() != 0 ||
                askrev6.run() != 0 ||
                askrev7.run() != 0 ||
                askrev8.run() != 0 
                ) {
                std::cout << "rev ask failded\n";
            }
            flag = 0;
        } 
    }

    // 销毁OpenCV窗口
    cv::destroyWindow("Fisheye Image");

    return 0;
}
