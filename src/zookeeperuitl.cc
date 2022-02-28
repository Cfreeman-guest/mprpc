#include "zookeeperutil.hpp"
#include "mprpcapplication.hpp"
#include "logger.hpp"

// 全局的watcher观察器   zkserver给zkclient的通知
void global_watcher(zhandle_t *zh, int type,
                   int state, const char *path, void *watcherCtx)
{
    if (type == ZOO_SESSION_EVENT)  // 回调的消息类型是和会话相关的消息类型
	{
		if (state == ZOO_CONNECTED_STATE)  // zkclient和zkserver连接成功
		{
			sem_t *sem = (sem_t*)zoo_get_context(zh);
            sem_post(sem);
		}
	}
}

//默认初始化客户端的句柄
ZkClient::ZkClient()
    : m_zhandle(nullptr)
{}

ZkClient::~ZkClient()
{
    if (m_zhandle != nullptr)
    {
        // 关闭句柄，释放资源  MySQL_Conn
        zookeeper_close(m_zhandle); 
        LOG_INFO("zookeeper close success!");
    }
}

void ZkClient::start()
{
    //获取ip地址和端口号，连接至zkServer
    std::string host = MprpcApplication::GetConfig().Load("zookeeperip");
    std::string port = MprpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    //按照zookep要求的格式去封装ip地址和端口
    std::string connstr = host+":"+port;
    /*
    zookeeper_mt:多线程版本
    zookeeper的初始化函数内部创建了两个线程，分别是
    网络I/O线程
    watcher回调线程
    加上api所在的线程共有三个线程
    ZOOAPI zhandle_t *zookeeper_init(const char *host, watcher_fn fn,
  int recv_timeout, const clientid_t *clientid, void *context, int flags);
    */
    //这里是一个异步的连接，zookeeper_init只做数据的初始化，连接成功后会去通知global-watcher
    m_zhandle = zookeeper_init(connstr.c_str(),global_watcher,30000,nullptr,nullptr,0);
    if ( nullptr == m_zhandle ) {
        LOG_ERR("%s:%03d %s zookeeper_init error!",__func__,__LINE__,__FILE__);
        exit(EXIT_FAILURE);
    }
    //创建信号量，并且初始化，信号量用于决定运行顺序，
    sem_t sem;
    sem_init(&sem, 0, 0);
    zoo_set_context(m_zhandle, &sem);
    //这里必须让watcher回调线程执行完watcher才可继续执行
    sem_wait(&sem);
    LOG_INFO("zookeeper_init success!");
}

void ZkClient::create(const char *path, const char *data, int datalen, int state)
{
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag;
	// 先判断path表示的znode节点是否存在，如果存在，就不再重复创建了
	flag = zoo_exists(m_zhandle, path, 0, nullptr);
	if (ZNONODE == flag) // 表示path的znode节点不存在
	{
		// 创建指定path的znode节点了
		flag = zoo_create(m_zhandle, path, data, datalen,
			&ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
		if (flag == ZOK)
		{
            LOG_INFO("znode create success... path: %s",path);
		}
		else
		{
            LOG_ERR("%s:%03d %s znode create error... path: %s, flag: %d",__func__,__LINE__,__FILE__,path,flag);
			exit(EXIT_FAILURE);
		}
	}
}
// 根据指定的path，获取znode节点的值
std::string ZkClient::getData(const char *path)
{
    char buffer[64];
	int bufferlen = sizeof(buffer);
	int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
	if (flag != ZOK)
	{
        LOG_ERR("%s:%03d %s get znode data error... path: %s",__func__,__LINE__,__FILE__,path);
		return "";
	}
	else
	{
        LOG_INFO("get znode data success... path: %s",path);
		return buffer;
	}
}







