#include "mprpcprovider.hpp"
#include "mprpcapplication.hpp"
#include "rpcheader.pb.h"
#include "logger.hpp"
#include "zookeeperutil.hpp"

/*
service_name =>  service描述   
                        =》 service* 记录服务对象
                        method_name  =>  method方法对象
json   protobuf
*/
// 这里是框架提供给外部使用的，可以发布rpc方法的函数接口
void MprpcProvider::NotifyService(google::protobuf::Service *service)
{
    ServiceInfo service_info;

    // 获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor *pserviceDesc = service->GetDescriptor();
    // 获取服务的名字
    std::string service_name = pserviceDesc->name();
    // 获取服务对象service的方法的数量
    int methodCnt = pserviceDesc->method_count();

    // std::cout << "service_name:" << service_name << std::endl;
    LOG_INFO("service_name:%s", service_name.c_str());

    for (int i=0; i < methodCnt; ++i)
    {
        // 获取了服务对象指定下标的服务方法的描述（抽象描述） UserService   Login
        const google::protobuf::MethodDescriptor* pmethodDesc = pserviceDesc->method(i);
        std::string method_name = pmethodDesc->name();
        service_info.m_methodMap.insert({method_name, pmethodDesc});

        // LOG_INFO("method_name:%s", method_name.c_str());
    }
    service_info.m_service = service;
    m_serviceMap.insert({service_name, service_info});
}

// 启动rpc服务节点，开始提供rpc远程网络调用服务
void MprpcProvider::Run()
{
    // 读取配置文件rpcserver的信息
    std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    muduo::net::InetAddress address(ip, port);

    // 创建TcpServer对象
    muduo::net::TcpServer server(&m_eventLoop, address, "RpcProvider");

    // 绑定连接回调和消息读写回调方法  分离了网络代码和业务代码
    server.setConnectionCallback(std::bind(&MprpcProvider::OnConnection, this, std::placeholders::_1));
    server.setMessageCallback(std::bind(&MprpcProvider::OnMessage, this, std::placeholders::_1, 
            std::placeholders::_2, std::placeholders::_3));

    // 设置muduo库的线程数量
    server.setThreadNum(4);
    //在连接网络之前
    // 把当前rpc节点上要发布的服务全部注册到zk上面，让rpc client可以从zk上发现服务
    // session timeout   30s     zkclient 网络I/O线程  1/3 * timeout 时间发送ping消息
    //rpc服务结点发送心跳消息，zookeeperserver重置超时时间
    // service_name为永久性节点    method_name为临时性节点
    ZkClient zkCli;
    zkCli.start();
    //有若干个服务
    for ( auto& sp : m_serviceMap ) {
        std::string service_path = "/" + sp.first;
        //节点只能一级一级创建，且从上往下创建
        //e.g. /UserService
        zkCli.create(service_path.c_str(),nullptr,0);
        for (auto& mp : sp.second.m_methodMap ) {
            //e.e. /UserService/Login
            std::string method_path = service_path + "/"+ mp.first;
            char method_path_data[128];
            //该结点为最底层节点，故创建节点数据
            //这个rpc服务结点所在的ip地址和端口号
            snprintf(method_path_data,128,"%s:%d",ip.c_str(),port);
            //ZOO_EPHEMERAL表示znode是一个临时性节点(ZOO_EPHEMERAL)
            zkCli.create(method_path.c_str(),method_path_data,strlen(method_path_data),ZOO_EPHEMERAL);
        }
    }

    // rpc服务端准备启动，打印信息
    std::cout << "RpcProvider start getting ready to connecting, service at ip:" << ip << " port:" << port << std::endl;

    // 启动网络服务
    server.start();
    m_eventLoop.loop(); 
}
// 注册socket连接回调，当有连接信息时调用该函数
void MprpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        // 和rpc client的连接断开了
        conn->shutdown();
    }
}
//注册消息回调，当有消息从客户端发来时，做相应的处理
void MprpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn, 
                            muduo::net::Buffer *buffer, 
                            muduo::Timestamp)
{
    // 网络上接收的远程rpc调用请求的字符流    Login args
    std::string recv_buf = buffer->retrieveAllAsString();

    // 从字符流中读取前4个字节的内容
    uint32_t header_size = 0;
    //这一行有待商榷
    recv_buf.copy((char*)&header_size, 4, 0);


    // 根据header_size读取数据头的原始字符流，反序列化数据，得到rpc请求的详细信息
    std::string rpc_header_str = recv_buf.substr(4, header_size);
    mprpc::RpcHeader rpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size;
    if (rpcHeader.ParseFromString(rpc_header_str))
    {
        // 数据头反序列化成功
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
        LOG_INFO("rpc_header_str: %s parse success",rpc_header_str.c_str());
    }
    else
    {
        // 数据头反序列化失败
        std::cout << "rpc_header_str:" << rpc_header_str << " parse error!" << std::endl;
        LOG_ERR("%s:%03d -%s  rpc_header_str: %s parse error",__func__,__LINE__,__FILE__,rpc_header_str.c_str());
        return;
    }

    // 获取rpc方法参数的字符流数据
    std::string args_str = recv_buf.substr(4 + header_size, args_size);

    // 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl; 
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl; 
    std::cout << "service_name: " << service_name << std::endl; 
    std::cout << "method_name: " << method_name << std::endl; 
    std::cout << "args_str: " << args_str << std::endl; 
    std::cout << "============================================" << std::endl;

    // 获取service对象和method对象
    auto it = m_serviceMap.find(service_name);
    if (it == m_serviceMap.end())
    {
        std::cout << "service_name: " << service_name << " is not exist!" << std::endl;
        LOG_ERR("%s:%03d -%s  service_name: %s is not exist!",__func__,__LINE__,__FILE__,service_name.c_str());
        return;
    }

    auto mit = it->second.m_methodMap.find(method_name);
    if (mit == it->second.m_methodMap.end())
    {
        std::cout << service_name << ":" << method_name << " is not exist!" << std::endl;
        LOG_ERR("%s:%03d -%s  service_name: %s is exist,but method_name: %s is not exist!",__func__,__LINE__,__FILE__,service_name.c_str(),method_name.c_str());
        return;
    }

    google::protobuf::Service *service = it->second.m_service; // 获取service对象  new UserService
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取method对象  Login

    // 生成rpc方法调用的请求request和响应response参数
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    if (!request->ParseFromString(args_str))
    {
        std::cout << "request parse error, content:" << args_str << std::endl;
        LOG_ERR("%s:%03d -%s  request parse error,content: %s",__func__,__LINE__,__FILE__,args_str.c_str());
        return;
    }
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给下面的method方法的调用，绑定一个Closure的回调函数
    google::protobuf::Closure *done = google::protobuf::NewCallback<MprpcProvider, 
                                                                    const muduo::net::TcpConnectionPtr&, 
                                                                    google::protobuf::Message*>
                                                                    (this, 
                                                                    &MprpcProvider::SendRpcResponse, 
                                                                    conn, response);

    // 在框架上根据远端rpc请求，调用当前rpc节点上发布的方法
    // new UserService().Login(controller, request, response, done)
    service->CallMethod(method, nullptr, request, response, done);
}
// Closure的回调操作，用于序列化rpc的响应和网络发送
void MprpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str)) // response进行序列化
    {
        // 序列化成功后，通过网络把rpc方法执行的结果发送会rpc的调用方
        conn->send(response_str);
        LOG_INFO("server send response_str: %s success!",response_str.c_str());
    }
    else
    {
        LOG_ERR("%s:%03d -%s serialize response_str: %s error!",__func__,__LINE__,__FILE__, response_str.c_str());

    }
    conn->shutdown(); // 模拟http的短链接服务，由rpcprovider主动断开连接
}