#include "mprpcconfig.hpp"
#include "logger.hpp"
#include <fstream>
#include <iostream>

// 负责解析加载配置文件
void MprpcConfig::LoadConfigFile(const char *config_file)
{
    //读取文件内容，
    /*
        以只读的方式打开该文件
        1. 读取一行内容，去掉开头空格，结尾空格
            读取第一个字符，如果是'#'，说明使注释，舍弃该行内容，继续读取下一行
        2. 如果不是'#',那么利用'='分割该字符串, 获取两个字符串，用哈希表存起来
    */
    std::ifstream file(config_file,std::ios::in);
    if ( !file.is_open() ) {
        LOG_ERR("%s:%03d %s config file: %s open failed!", __func__, __LINE__, __FILE__,config_file);
        //文件无法读取，ip地址和端口号就无法拿到，接下来的联网操作则无法完成
        //所以直接退出进程
        exit(EXIT_FAILURE);
    }
    std::string buffer;
    while ( getline(file,buffer) ) {
        trim(buffer);
        if ( buffer[0] == '#' ) {
            continue;
        }
        //我们需要的内容
        size_t pos =buffer.find('=');
        if ( std::string::npos == pos ) {
            continue;
        }
        std::string key = buffer.substr(0,pos);
        std::string value = buffer.substr(pos+1,buffer.length()-pos-1);
        //这一步操作允许了等号左右有空格
        trim(key);
        trim(value);
        m_configMap.insert({key,value});
    }
    file.close();
}

// 查询配置项信息
std::string MprpcConfig::Load(const std::string &key)
{
    auto it = m_configMap.find(key);
    if (it == m_configMap.end())
    {
        return "";
    }
    return it->second;
}
void MprpcConfig::trim(std::string& buffer)
{
    size_t front_pos = buffer.find_first_not_of(' ');
    size_t back_pos = buffer.find_last_not_of(' ');
    buffer = buffer.substr(front_pos,back_pos-front_pos+1);
}