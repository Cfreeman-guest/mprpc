#include <iostream>
#include "mprpcapplication.hpp"
#include "user.pb.h"


int main(int argc, char **argv)
{
    // 整个程序启动以后，想使用mprpc框架来享受rpc服务调用，一定需要先调用框架的初始化函数（只初始化一次）
    MprpcApplication::Init(argc, argv);

    // 演示调用远程发布的rpc方法Login
    mprpc::UserServiceRpc_Stub stub(new MprpcChannel());
    // rpc方法的请求参数
    mprpc::LoginRequest request;
    request.set_name("zhang san");
    request.set_pwd("123456");
    // rpc方法的响应
    mprpc::LoginResponse response;
    // 发起rpc方法的调用  同步的rpc调用过程  MprpcChannel::callmethod
    MprpcController controller;
    stub.Login(&controller, &request, &response, nullptr); // RpcChannel->RpcChannel::callMethod 集中来做所有rpc方法调用的参数序列化和网络发送
    
    if ( controller.Failed() ) {
        //如果Login在执行过程中失败了
        std::cout << controller.ErrorText() << std::endl;
        return 0;
    }
    
    //执行到这里说明Login是执行成功的，收到了服务器发送的response
    if (0 == response.result().errcode())
    {
        std::cout << "rpc login response success:" << response.sucess() << std::endl;
    }
    else
    {
        std::cout << "rpc login response error : " << response.result().errmsg() << std::endl;
    }
    
    return 0;
}