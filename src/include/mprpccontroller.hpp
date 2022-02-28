#pragma once
#include <string>
#include <google/protobuf/service.h>

class MprpcController : public google::protobuf::RpcController
{
public:
    MprpcController();
    virtual void Reset() override;
    virtual bool Failed() const override;
    virtual std::string ErrorText() const override;
    virtual void StartCancel() override;
    virtual void SetFailed(const std::string& reason) override;
    virtual bool IsCanceled() const override;
    virtual void NotifyOnCancel(google::protobuf::Closure* callback) override;

private:
    bool m_failed; // RPC方法执行过程中的状态
    std::string m_errText; // RPC方法执行过程中的错误信息
};