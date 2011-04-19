#ifndef __CWX_APP_HANDLER_4_CHANNEL_H__
#define __CWX_APP_HANDLER_4_CHANNEL_H__
/*
��Ȩ������
    �������ѭGNU LGPL��http://www.gnu.org/copyleft/lesser.html����
    ��ϵ��ʽ��email:cwinux@gmail.com��΢��:http://t.sina.com.cn/cwinux
*/

/**
@file CwxAppHandler4Channel.h
@brief Channel��ioͨ�Ż��ࡣ
@author cwinux@gmail.com
@version 1.0
@date 2011-04-17
@warning
@bug
*/

#include "CwxPre.h"
#include "CwxGlobalMacro.h"
#include "CwxMsgBlock.h"
#include "CwxMsgHead.h"
#include "CwxSocket.h"
#include "CwxAppConfig.h"
#include "CwxAppMacro.h"
#include "CwxLogger.h"
#include "CwxAppHandler4Base.h"
#include "CwxDTail.h"
#include "CwxMsgBlock.h"

CWINUX_BEGIN_NAMESPACE
/**
@class CwxAppHandler4Channel
@brief Channel��ioͨ�Ż���
*/

class CwxAppChannel;

class CWX_API CwxAppHandler4Channel:public CwxAppHandler4Base
{
public:
    ///���캯��
    CwxAppHandler4Channel(CwxAppChannel *channel);
    ///��������
    ~CwxAppHandler4Channel();
public:
    /**
    @brief ��ʼ�����������ӣ�����Reactorע������
    @param [in] arg �������ӵ�acceptor��ΪNULL
    @return -1���������������ӣ� 0�����ӽ����ɹ�
    */
    virtual int open (void * arg= 0);
    /**
    @brief ���������ϵ��¼�
    @param [in] event ���ӵ�handle�ϵ��¼�
    @param [in] handle  �������¼���handle��
    @return -1������ʧ�ܣ� 0������ɹ�
    */
    virtual int handle_event(int event, CWX_HANDLE handle=CWX_INVALID_HANDLE);
    /**
    @brief ���ӿ�д�¼�������-1��handle_close()�ᱻ����
    @return -1������ʧ�ܣ������handle_close()�� 0������ɹ�
    */
    virtual int handle_output();
    /**
    @brief ���ӿɶ��¼�������-1��handle_close()�ᱻ����
    @return -1������ʧ�ܣ������handle_close()�� 0������ɹ�
    */
    virtual int handle_input();
    /**
    @brief ���ӳ�ʱ�¼�������-1��handle_close()�ᱻ����
    @return -1������ʧ�ܣ������handle_close()�� 0������ɹ�
    */
    virtual int handle_timeout();
    /**
    @brief Handler���ӳ�ִ���¼�����ÿ��dispatchʱִ�С�
    @return -1������ʧ�ܣ������handle_close()�� 0������ɹ�
    */
    virtual int handle_defer();
    /**
    @brief ���ӹر�
    @param [in] handle  ����handle��
    @return -1������ʧ�ܣ� 0������ɹ�
    */
    virtual int close(CWX_HANDLE handle=CWX_INVALID_HANDLE);
    /**
    @brief ֪ͨ���ӿ�ʼ����һ����Ϣ��<br>
    ֻ����MSGָ��BEGIN_NOTICE��ʱ��ŵ���.
    @param [in] msg Ҫ���͵���Ϣ��
    @return -1��ȡ����Ϣ�ķ��͡� 0��������Ϣ��
    */
    virtual int onStartSendMsg(CwxMsgBlock* msg);
    /**
    @brief ֪ͨ�������һ����Ϣ�ķ��͡�<br>
    ֻ����Msgָ��FINISH_NOTICE��ʱ��ŵ���.
    @param [in,out] msg ���뷢����ϵ���Ϣ��������NULL����msg���ϲ��ͷţ�����ײ��ͷš�
    @return 
    CwxMsgSendCtrl::UNDO_CONN�����޸����ӵĽ���״̬
    CwxMsgSendCtrl::RESUME_CONN�������Ӵ�suspend״̬��Ϊ���ݽ���״̬��
    CwxMsgSendCtrl::SUSPEND_CONN�������Ӵ����ݽ���״̬��Ϊsuspend״̬
    */
    virtual CWX_UINT32 onEndSendMsg(CwxMsgBlock*& msg);

    /**
    @brief ֪ͨ�����ϣ�һ����Ϣ����ʧ�ܡ�<br>
    ֻ����Msgָ��FAIL_NOTICE��ʱ��ŵ���.
    @param [in,out] msg ����ʧ�ܵ���Ϣ��������NULL����msg���ϲ��ͷţ�����ײ��ͷš�
    @return void��
    */
    virtual void onFailSendMsg(CwxMsgBlock*& msg);
    /**
    @brief ֪ͨ���ӹرա�
    @return �����������ӣ�1������engine���Ƴ�ע�᣻0������engine���Ƴ�ע�ᵫ��ɾ��handler��-1����engine�н�handle�Ƴ���ɾ����
    */
    virtual int onConnClosed();

public:
    ///��ն���
    void clear();
    ///��ȡ��һ�������͵���Ϣ������ֵ��0��û�д�������Ϣ��1,�����һ����������Ϣ
    inline int getNextMsg();
    ///��Ҫ���͵���Ϣ�Ŷӣ�����ֵ��true���ɹ���false��ʧ�ܣ�ʧ��ʱ��Ϊ��������
    inline bool putMsg(CwxMsgBlock* msg);
    ///����û����Ϣ���ͣ�ʹ���ӵķ��ͼ������.����ֵ�� -1: failure, 0: success
    inline int cancelWakeup();
    ///�������ӵĿ�д��أ��Է���δ������ϵ�����.����ֵ�� -1:failure�� 0:success��
    inline int wakeUp();
    ///�Ƿ��пɷ��͵����ݰ�
    bool isEmpty() const;
    ///��ȡchannel
    CwxAppChannel* channel();
private:
    ///�Է������ķ�ʽ��������Ϣ������ֵ,-1: failure; 0: not send all;1:send a msg
    inline int nonBlockSend();
protected:
    CWX_UINT32             m_uiSendByte; ///the sent bytes number for current message.
    CwxMsgBlock*           m_curSndingMsg; ///<current sending msg;
    CwxMsgBlock*           m_waitSendMsgHead; ///<The header for wait to be sent msg.
    CwxMsgBlock*           m_waitSendMsgTail;   ///<The tail for wait to be sent msg.
    CWX_UINT32             m_uiRecvHeadLen; ///<recieved msg header's byte number.
    CWX_UINT32             m_uiRecvDataLen; ///<recieved data's byte number.
    CwxMsgBlock*           m_recvMsgData; ///<the recieved msg data
    CwxAppChannel*         m_channel;
};
CWINUX_END_NAMESPACE

#include "CwxAppHandler4Channel.inl"

#include "CwxPost.h"

#endif
