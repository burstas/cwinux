#ifndef __CWX_MPROXY_MQ_HANDLER_H__
#define __CWX_MPROXY_MQ_HANDLER_H__
/*
版权声明：
    本软件遵循GNU LGPL（http://www.gnu.org/copyleft/lesser.html）
*/
#include "CwxAppCommander.h"
#include "CwxAppTss.h"
#include "CwxGlobalMacro.h"
#include "CwxMqTss.h"

CWINUX_USING_NAMESPACE

class CwxMproxyApp;
///请求的MQ消息回复的handle
class CwxMproxyMqHandler : public CwxAppCmdOp 
{
public:
    ///构造函数
    CwxMproxyMqHandler(CwxMproxyApp* pApp):m_pApp(pApp)
    {
    }
    ///析构函数
    virtual ~CwxMproxyMqHandler()
    {

    }
public:
    ///mq消息返回的处理函数
    virtual int onRecvMsg(CwxMsgBlock*& msg, CwxAppTss* pThrEnv);
    virtual int onConnClosed(CwxMsgBlock*& msg, CwxAppTss* pThrEnv);
    virtual int onEndSendMsg(CwxMsgBlock*& msg, CwxAppTss* pThrEnv);
    virtual int onFailSendMsg(CwxMsgBlock*& msg, CwxAppTss* pThrEnv);
public:
    static int sendMq(CwxMproxyApp* app, CWX_UINT32 uiTaskId, CwxMsgBlock*& msg);
private:
    CwxMproxyApp*     m_pApp;  ///<app对象
};

#endif 
