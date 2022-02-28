#include "mprpcchannel.hpp"
#include <string>
#include "rpcheader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "mprpcapplication.hpp"
#include "mprpccontroller.hpp"
#include "zookeeperutil.hpp"

/*
header_size + service_name method_name args_size + args
*/
// 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送 
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                                google::protobuf::RpcController* controller, 
                                const google::protobuf::Message* request,
                                google::protobuf::Message* response,
                                google::protobuf:: Closure* done)
{
    //能够获取到服务名称和方法名称，因为之前的多态所以已经记录
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name(); // service_name
    std::string method_name = method->name(); // method_name

    // 获取参数的序列化字符串长度 args_size
    uint32_t args_size = 0;
    std::string args_str;
    //只有name和pwd
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        //rpc请求序列化失败，从这儿返回
        controller->SetFailed("serialize request error!");
        return;
    }
    
    // 定义rpc的请求header
    //将服务的名称，方法的名称和传进来的参数封装成protobuf，要传出去的
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    //将刚刚封装好的protobuf对象再一次序列化成字符串，然后算出它的长度
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    // 组织待发送的rpc请求的字符串
    std::string send_rpc_str;
    send_rpc_str.insert(0, std::string((char*)&header_size, 4)); // header_size
    send_rpc_str += rpc_header_str; // rpcheader
    send_rpc_str += args_str; // args

    //e.g. send_rpc_str:23loginrequestLogin6csq123


    // 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl; 
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl; 
    std::cout << "service_name: " << service_name << std::endl; 
    std::cout << "method_name: " << method_name << std::endl; 
    std::cout << "args_str: " << args_str << std::endl; 
    std::cout << "============================================" << std::endl;

    // 使用tcp编程，完成rpc方法的远程调用
    int clientfd = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        char errtxt[512] = {0};
        snprintf(errtxt,512, "create socket error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }
    /*
    客户端是无法直接知道rpc服务节点所在的ip地址和端口号
    需要向zookeeper注册中心询问，从而拿到当前服务的方法所在的rpc服务结点的ip地址和端口号
    需要提供当前服务的方法所在的路径/UserService/Login
    */
    ZkClient zkClient;
    zkClient.start();
    //对service_name和method_name进行封装，形成路径格式
    std::string method_path = "/"+service_name+"/"+method_name;
    //通过路径查找对应的数据-----ip:port(以该形式返回)
    std::string ip_port = zkClient.getData(method_path.c_str());
    if ( "" == ip_port) {
        controller->SetFailed(method_path + " is not exist!");
        return;        
    }
    //分离ip地址(std::string)和端口号(uint16)
    size_t pos = ip_port.find(':');
    if ( std::string::npos == pos ) {
        controller->SetFailed(method_path + " address is invalid!");
        return;        
    }
    std::string ip = ip_port.substr(0,pos);
    uint16_t port = atoi(ip_port.substr(pos+1,ip_port.length()-pos-1).c_str());
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // 连接rpc服务节点
    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        snprintf(errtxt,512, "connect error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 发送rpc请求
    if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        close(clientfd);
        char errtxt[512] = {0};
        snprintf(errtxt, 512, "send error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 接收rpc请求的响应值  
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        snprintf(errtxt, 512, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 反序列化rpc调用的响应数据
    // std::string response_str(recv_buf, 0, recv_size); // bug出现问题，recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败
    // if (!response->ParseFromString(response_str))
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        close(clientfd);
        char errtxt[512] = {0};
        snprintf(errtxt, 512,"parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }

    close(clientfd);
}