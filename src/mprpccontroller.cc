#include "mprpccontroller.hpp"

MprpcController::MprpcController()
{
    m_failed = false;//默认rpc过程调用成功
    m_errText = "";//空
}


void MprpcController::Reset()
{
    //返回默认设置 
    m_failed = false;
    m_errText = "";
}
bool MprpcController::Failed() const
{
    return m_failed;
}
std::string MprpcController::ErrorText() const
{
    //返回错误的原因
    return m_errText;
}
void MprpcController::SetFailed(const std::string& reason)
{
    m_failed = true;
    m_errText = reason;
}
//功能为用到，不实现
void MprpcController::StartCancel()
{
}
bool MprpcController::IsCanceled() const
{
}
void MprpcController::NotifyOnCancel(google::protobuf::Closure* callback)
{
}