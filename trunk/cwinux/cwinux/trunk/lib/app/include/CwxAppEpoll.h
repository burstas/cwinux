#ifndef __CWX_APP_EPOLL_H__
#define __CWX_APP_EPOLL_H__

/*
��Ȩ������
    �������ѭGNU LGPL��http://www.gnu.org/copyleft/lesser.html����
    ��ϵ��ʽ��email:cwinux@gmail.com��΢��:http://t.sina.com.cn/cwinux
*/

/**
@file CwxAppEpoll.h
@brief �ܹ���epoll�¼��������
@author cwinux@gmail.com
@version 0.1
@date 2011-04-13
@warning
@bug
*/
#include "CwxPre.h"
#include "CwxGlobalMacro.h"
#include "CwxTimeValue.h"
#include "CwxAppConfig.h"
#include "CwxAppMacro.h"
#include "CwxAppHandler4Base.h"
#include "CwxThread.h"
#include "CwxLogger.h"
#include "CwxMinHeap.h"

CWINUX_BEGIN_NAMESPACE

/**
@class CwxAppEpoll
@brief �ܹ���epoll�¼�����
*/

class CWX_API CwxAppEpoll
{
public:
    enum
    {
        CWX_EPOLL_INIT_HANDLE = 81920
    };
public:
    ///���캯��
    CwxAppEpoll();
    ///��������
    ~CwxAppEpoll();
public:
    /**
    @brief ��ʼ��epoll���档
    @return -1��ʧ�ܣ�0���ɹ�
    */
    int init();
    /**
    @brief ע��IO�¼�����handle��
    @param [in] io_handle ����IO handle
    @param [in] event_handler io handle��Ӧ��event handler��
    @param [in] mask ע����¼����룬ΪREAD_MASK��WRITE_MASK��PERSIST_MASK��TIMEOUT_MASK���
    @return -1��ʧ�ܣ�0���ɹ���
    */
    int registerHandler(CWX_HANDLE io_handle,
        CwxAppHandler4Base *event_handler,
        int mask);
    /**
    @brief ɾ��io�¼�����handle��
    @param [in] event_handler �Ƴ���event-handler
    @return -1��ʧ�ܣ�
    0���ɹ���
    */
    int removeHandler (CwxAppHandler4Base *event_handler);
    /**
    @brief suspend io�¼�����handle��
    @param [in] event_handler suspend��event-handler
    @param [in] suspend_mask suspend���¼�,ΪREAD_MASK��WRITE_MASK���
    @return -1��ʧ�ܣ�
    0���ɹ���
    */
    int suspendHandler (CwxAppHandler4Base *event_handler,
        int suspend_mask);
    /**
    @brief resume io�¼�����handle��
    @param [in] event_handler resume��event-handler
    @param [in] resume_mask resume���¼�,ΪREAD_MASK��WRITE_MASK���
    @return -1��ʧ�ܣ�
    0���ɹ���
    */
    int resumeHandler (CwxAppHandler4Base *event_handler,
        int resume_mask);

    /**
    @brief ɾ��io�¼�����handle��
    @param [in] handle �Ƴ��� io handle
    @return NULL�������ڣ����򣺳ɹ���
    */
    CwxAppHandler4Base* removeHandler (CWX_HANDLE handle);
    /**
    @brief suspend io�¼�����handle��
    @param [in] handle suspend io handle
    @param [in] suspend_mask suspend���¼�,ΪREAD_MASK��WRITE_MASK���
    @return -1��ʧ�ܣ�0���ɹ�
    */
    int suspendHandler (CWX_HANDLE handle,
        int suspend_mask);
    /**
    @brief resume io�¼�����handle��
    @param [in] handle resume io handle
    @param [in] resume_mask resume���¼�,ΪREAD_MASK��WRITE_MASK���
    @return -1��ʧ�ܣ�0���ɹ���
    */
    int resumeHandler (CWX_HANDLE handle,
        int resume_mask);
    /**
    @brief ע��signal�¼�����handle���źž���PERSIST���ԡ�
    @param [in] signum �ź�
    @param [in] event_handler signal��event handler
    @return -1��ʧ�ܣ� 0���ɹ���
    */
    int registerSignal(int signum,
        CwxAppHandler4Base *event_handler);
    /**
    @brief ɾ��signal�¼�����handle��
    @param [in] event_handler signal��event handler
    @return -1��ʧ�ܣ�0���ɹ���
    */
    int removeSignal(CwxAppHandler4Base *event_handler);
    /**
    @brief ɾ��signal�¼�����handle�����̰߳�ȫ�������̶߳����Ե��á�
    @param [in] sig signal
    @return NULL�������ڣ����򷵻�signal��handler
    */
    CwxAppHandler4Base* removeSignal(int sig);

    /**
    @brief ���ö�ʱ����handle��timeout������persist���ԡ�
    @param [in] event_handler timer��event handler
    @param [in] interval ��ʱ�ļ��
    @return -1��ʧ�ܣ�0���ɹ���
    */
    int scheduleTimer (CwxAppHandler4Base *event_handler,
        CwxTimeValue const &interval);
    ///ȡ����ʱ����handle��
    int cancelTimer (CwxAppHandler4Base *event_handler);
    ///fork��re-init����������ֵ��0���ɹ���-1��ʧ��
    int forkReinit();
    /**
    @brief ����¼���
    @return -1��ʧ�ܣ�0���ɹ�
    */
    int poll();
private:
    ///��ȡ��һ����epoll�ĳ�ʱʱ��
    void timeout(CWX_UINT64& ullTime);
    ///��� epoll���,maskΪREAD_MASK��WRITE_MASK����ϡ�
    int addEvent(int fd, int mask);
    ///ɾ�����ڵ�mask��maskΪREAD_MASK��WRITE_MASK����ϡ�
    int delEvent(int fd, int mask);
private:
    class EventHandle
    {
    public:
        EventHandle()
        {
            m_mask = 0;
            m_handler = NULL;
        }
        inline bool isReg() { return (m_mask&CwxAppHandler4Base::RW_MASK) != 0;}
    public:
        int         m_mask;
        CwxAppHandler4Base* m_handler;
    };
private:
    int                             m_epfd;     ///<epoll��fd
    struct epoll_event              m_events[CWX_APP_MAX_IO_NUM]; ///<epoll��event ����
    EventHandle                     m_eHandler[CWX_APP_MAX_IO_NUM]; ///<epoll��event handler
    int                             m_signalFd[2]; ///<�źŵĶ�дhandle
    sig_atomic_t                    m_arrSignals[CWX_APP_MAX_SIGNAL_ID + 1];///<signal������
    CwxAppHandler4Base*             m_sHandler[CWX_APP_MAX_SIGNAL_ID + 1];///<signal handler������
    volatile sig_atomic_t           m_bSignal; ///<�Ƿ����ź�
    CwxMinHeap<CwxAppHandler4Base>  m_timeHeap; ///<ʱ���
};


CWINUX_END_NAMESPACE
#include "CwxAppReactor.inl"
#include "CwxPost.h"

#endif
