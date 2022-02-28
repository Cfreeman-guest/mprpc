#include "logger.hpp"
#include <time.h>
#include <iostream>
#include <fstream>
#include <thread>
//构造函数
Logger::Logger()
{
    //开启写日志线程，将从带锁队列中获取到的信息，写入到文件中
    //c++ 11 lambada,chrono库
    //带锁队列中只会记录与代码运行相关的消息，完整的日志内容需要包含日器和文件信息
    //年月日，时分秒
    std::thread writeLogTask([&](){
        while ( true ) {
            time_t now = time(nullptr);
            tm* nowtm = localtime(&now);
            //文件名的构造
            char file_name[128] = {0};
            snprintf(file_name,128,"%4d-%02d-%02d.txt",nowtm->tm_year+1900, nowtm->tm_mon+1,
                                    nowtm->tm_mday);
            
            //一系列文件操作
            std::string filename(file_name);
            std::ofstream file(file_name,std::ios::app | std::ios::out);
            if ( !file.is_open() ) {
                std::cout << "logger file : " << file_name << " open error!" << std::endl;
                exit(EXIT_FAILURE);
            }
            //从有锁的日志对列中取出日志信息
            std::string msg = m_lockQueue.pop();
            //获取当前系统时间
            char time_buf[128] = {0};
            snprintf(time_buf,128,"%02d:%02d:%02d %s ",nowtm->tm_hour,nowtm->tm_min,nowtm->tm_sec,
                                m_logLevel==INFO?"INFO":"ERROR");
            //日志所需的信息都已经获取，接下来要开始字符串的拼接
            msg.insert(0,time_buf);
            msg.append("\n");
            //日志信息封装完毕，输出到文件
            file << msg;
            file.close();
        }
    });
    // 设置分离线程，守护线程
    writeLogTask.detach();
}

//获取日志对象的方式只有这么一个途径--去调用getInstance
//并且是用引用去接收
Logger& Logger::getInstance()
{
    static Logger logger;//对象构造结束，即开启写日志线程
    return logger;
}
void Logger::setLogLevel(int logLevel)
{
    m_logLevel = logLevel;
}
//写日志，把日志信息写入到lockQueue缓冲区中
void Logger::Log(std::string msg )
{
    m_lockQueue.push(msg);
} 
