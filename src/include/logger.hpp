#pragma once
#include <queue>
#include <thread>
#include <mutex> // pthread_mutex_t
#include <condition_variable> // pthread_condition_t

//日志队列，互斥锁+条件变量实现互斥访问
template<typename T>
class LockQueue
{
private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::queue<T> m_queue;
public:
    //互斥地写日志
    void push(const T& data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(data);
        m_cond.notify_one();
    }
    //读取日志队列中的内容
    T pop()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while ( m_queue.empty() ) {
            m_cond.wait(lock);
        }
        T data = m_queue.front();
        m_queue.pop();
        return data;
    }

};
// 定义日志级别
enum LogLevel
{
    INFO,  // 普通信息
    ERROR, // 错误信息
};
//mprpc框架的日志系统 ---- 单例，异步，
class Logger
{
public:
    //获取日志单例
    static Logger& getInstance();
    //设置日志级别
    void setLogLevel(int logLevel);
    //写日志
    void Log(std::string msg);
private:
    int m_logLevel;
    LockQueue<std::string> m_lockQueue;

    Logger();
    Logger(const Logger&) = delete;
    Logger(Logger&&) = delete;
};

// 定义宏 LOG_INFO("xxx %d %s", 20, "xxxx")
#define LOG_INFO(logmsgformat, ...) \
    do \
    {  \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(INFO); \
        char c[1024] = {0}; \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.Log(c); \
    } while(0) \

#define LOG_ERR(logmsgformat, ...) \
    do \
    {  \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(ERROR); \
        char c[1024] = {0}; \
        snprintf(c, 1024, logmsgformat, ##__VA_ARGS__); \
        logger.Log(c); \
    } while(0) \





    