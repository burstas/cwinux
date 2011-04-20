#include "CwxAppHandler4Channel.h"
#include "CwxAppChannel.h"

CWINUX_BEGIN_NAMESPACE


CwxAppHandler4Channel::CwxAppHandler4Channel(CwxAppChannel *channel)
:CwxAppHandler4Base(NULL)
{
    m_uiSendByte = 0;
    m_curSndingMsg = 0;
    m_waitSendMsgHead = NULL;
    m_waitSendMsgTail = NULL;
    m_channel = channel;
}

///��������
CwxAppHandler4Channel::~CwxAppHandler4Channel()
{
    clear();
    if (getHandle() != CWX_INVALID_HANDLE)
    {
        ::close(getHandle());
        setHandle(CWX_INVALID_HANDLE);
    }
}


int CwxAppHandler4Channel::open (void * )
{
    if (0 != channel()->registerHandler(getHandle(),
        this,
        CwxAppHandler4Base::READ_MASK))
    {
        CWX_ERROR(("Failure to register conn[%d] for read_mask",
            getHandle()));
        return -1;
    }
    return 0;
}

int CwxAppHandler4Channel::close(CWX_HANDLE)
{
    int ret = onConnClosed();
    if (-1 == ret)
    {
        channel()->removeHandler(this);
        delete this;
    }
    else if (0 == ret)
    {
        channel()->removeHandler(this);
    }
    return 0;
}


/**
@brief ���������ϵĵ�������
@param [in] handle ���ӵ�handle
@return -1���رյ����ӣ� 0���������ݳɹ�
*/
int CwxAppHandler4Channel::handle_event(int event, CWX_HANDLE)
{
    if (CWX_CHECK_ATTR(event, CwxAppHandler4Base::WRITE_MASK))
    {
        if (0 != handle_output()) return -1;
    }
    if (CWX_CHECK_ATTR(event, CwxAppHandler4Base::READ_MASK))
    {
        if (0 != handle_input()) return -1;
    }
    if (CWX_CHECK_ATTR(event, CwxAppHandler4Base::TIMEOUT_MASK))
    {
        if (0 != handle_timeout()) return -1;
    }
    return 0;
}


int CwxAppHandler4Channel::handle_output ()
{
    int result = 0;
    bool bCloseConn = false;
    // The list had better not be empty, otherwise there's a bug!
    if (NULL == this->m_curSndingMsg)
    {
        CWX_ASSERT(m_uiSendByte==0);
        while(1)
        {
            result = this->getNextMsg();
            if (0 == result) return this->cancelWakeup();
            if (CWX_CHECK_ATTR(this->m_curSndingMsg->send_ctrl().getMsgAttr(), CwxMsgSendCtrl::BEGIN_NOTICE))
            {
                if (-1 == onStartSendMsg(m_curSndingMsg))
                {
                    CwxMsgBlockAlloc::free(this->m_curSndingMsg);
                    this->m_curSndingMsg = NULL;
                    continue; //next msg;
                }
            }//end if (this->m_curSndingMsg->IsBeginNotice())
            //it's a msg which need be sent.
            break; //break while
        }//end while
    }
    //has message
    result = this->nonBlockSend();
    if (0 == result)
    {// Partial send.
        CWX_ASSERT (errno == EWOULDBLOCK);
        return 0;
    }
    else if (1 == result)
    {//finish
        bCloseConn = CWX_CHECK_ATTR(this->m_curSndingMsg->send_ctrl().getMsgAttr(), CwxMsgSendCtrl::CLOSE_NOTICE);
        this->m_uiSendByte = 0;
        if (CWX_CHECK_ATTR(this->m_curSndingMsg->send_ctrl().getMsgAttr(), CwxMsgSendCtrl::FINISH_NOTICE)){
            CWX_UINT32 uiResume = onEndSendMsg(m_curSndingMsg);
            if (CwxMsgSendCtrl::RESUME_CONN == uiResume)
            {
                if (0 != channel()->resumeHandler(this, CwxAppHandler4Base::READ_MASK))
                {
                    CWX_ERROR(("Failure to resume handler with conn_id[%d]", getHandle()));
                    return -1;
                }
            }
            else if (CwxMsgSendCtrl::SUSPEND_CONN == uiResume)
            {
                if (0 != channel()->suspendHandler(this, CwxAppHandler4Base::READ_MASK))
                {
                    CWX_ERROR(("Failure to suspend handler with conn_id[%d]", getHandle()));
                    return -1;
                }
            }
        }
        if (bCloseConn)
        {
            return -1;
        }
        if (m_curSndingMsg)
        {
            CwxMsgBlockAlloc::free(m_curSndingMsg);
            this->m_curSndingMsg = NULL;
        }
        return 0;
    }
    else
    {//failure
        if (m_curSndingMsg->send_ctrl().isFailNotice())
        {
            onFailSendMsg(m_curSndingMsg);
        }
        if (m_curSndingMsg) CwxMsgBlockAlloc::free(m_curSndingMsg);
        this->m_curSndingMsg = NULL;
        return -1;
    }
    return -1;
}

/***
desc:
	recv data from socket.
param: 
	handle of the event.
return:
	-1 : failure, handle_close is invoked.
	0  : success.
***/
int CwxAppHandler4Channel::handle_input ()
{
    return -1;
}

/***
desc:
	timeout evnet, it exec the noticeReconnect operation.
return:
	0  : success.
***/
int CwxAppHandler4Channel::handle_timeout()
{
    return 0;
}

int CwxAppHandler4Channel::handle_redo()
{
    return 0;
}

//return -1��ȡ����Ϣ�ķ��͡� 0��������Ϣ��
int CwxAppHandler4Channel::onStartSendMsg(CwxMsgBlock* )
{
    return 0;
}
/*return 
CwxMsgSendCtrl::UNDO_CONN�����޸����ӵĽ���״̬
CwxMsgSendCtrl::RESUME_CONN�������Ӵ�suspend״̬��Ϊ���ݽ���״̬��
CwxMsgSendCtrl::SUSPEND_CONN�������Ӵ����ݽ���״̬��Ϊsuspend״̬
*/
CWX_UINT32 CwxAppHandler4Channel::onEndSendMsg(CwxMsgBlock*& )
{
    return CwxMsgSendCtrl::UNDO_CONN;
}

void CwxAppHandler4Channel::onFailSendMsg(CwxMsgBlock*&)
{

}
//return �����������ӣ�1������engine���Ƴ�ע�᣻0������engine���Ƴ�ע�ᵫ��ɾ��handler��-1����engine�н�handle�Ƴ���ɾ����
int CwxAppHandler4Channel::onConnClosed()
{
    return -1;
}


///����û����Ϣ���ͣ�ʹ���ӵķ��ͼ������.����ֵ�� -1: failure, 0: success
int CwxAppHandler4Channel::cancelWakeup()
{
    if(-1 == channel()->suspendHandler(this, CwxAppHandler4Base::WRITE_MASK)){
        CWX_ERROR(("Failure to cancel wakeup a connection. conn[%d]", getHandle()));
        return -1;
    }
    return 0;
}

///�������ӵĿ�д��أ��Է���δ������ϵ�����.����ֵ�� -1:failure�� 0:success��
int CwxAppHandler4Channel::wakeUp()
{
    if(-1 == channel()->resumeHandler(this, CwxAppHandler4Base::WRITE_MASK)){
        CWX_ERROR(("Failure to wakeup a connection. conn[%d]", getHandle()));
        return -1;
    }
    return 0;
}

CWINUX_END_NAMESPACE
