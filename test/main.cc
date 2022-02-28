#include "test.pb.h"
#include <iostream>
#include <string>

using namespace csq;

int main()
{
    //封装了请求对象的数据，并将其序列化成字符串
    LoginRequest login;
    login.set_name("chensongqing");
    login.set_pwd("666");
    std::string str;
    //对象序列化成string,str为传出参数，故传入地址
    if ( login.SerializeToString(&str) ) {
        std::cout << str << std::endl;
    }
    LoginRequest req;
    //反序列化，
    if ( req.ParseFromString(str) ) {
        std::cout <<req.name() << req.pwd() << std::endl;
    }
    GetFriendListResponse rsp;
    //对组合类对象访问的方法是，返回该对象的地址，然后对该对象进行操作
    ResultCode* rc = rsp.mutable_result();
    rc->set_errcode(0);
    rc->set_errmsg("ok!");
    //对组合类对象访问的方法是，返回该对象的地址，然后对该对象进行操作
    User* user = rsp.add_friend_list();
    user->set_name("csf");
    user->set_age(18);
    user->set_sex(User::MAN);
    //获取列表当前的大小
    std::cout << rsp.friend_list_size();
    
    User usr = rsp.friend_list(0);
    std::cout << usr.name();
    std::cout << usr.age();
    std::cout << usr.sex();
    std::string rspstr;
    if (rc->SerializeToString(&rspstr) ) {
        std::cout << rspstr << std::endl;
    }
    







    return 0;
}