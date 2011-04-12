#ifndef __CWX_THREAD_H__
#define __CWX_THREAD_H__
/*
��Ȩ������
    ��������ѭGNU LGPL��http://www.gnu.org/copyleft/lesser.html����
    ��ϵ��ʽ��email:cwinux@gmail.com��΢��:http://t.sina.com.cn/cwinux
*/

/**
@file CwxThread.h
@brief �̶߳�����
@author cwinux@gmail.com
@version 0.1
@date 2009-07-20
@warning
@bug
*/
#include "CwxPre.h"
#include "CwxGlobalMacro.h"
#include "CwxType.h"
#include "CwxLockGuard.h"
#include "CwxMutexLock.h"
#include "CwxMsgQueue.h"
#include "CwxMsgBlock.h"
#include "CwxAppMacro.h"
#include "CwxAppConfig.h"
#include "CwxAppLogger.h"
#include "CwxTss.h"

CWINUX_BEGIN_NAMESPACE

class CwxAppFramework;

typedef void* (*CWX_THR_FUNC)(void *);

#define THREAD_BOUND               0x00000001
#define THREAD_NEW_LWP             0x00000002
#define THREAD_DETACHED            0x00000040
#define THREAD_SUSPENDED           0x00000080
#define THREAD_DAEMON              0x00000100
#define THREAD_SCHED_FIFO          0x00020000
#define THREAD_SCHED_RR            0x00040000
#define THREAD_SCHED_DEFAULT       0x00080000
#define THREAD_JOINABLE            0x00010000
#define THREAD_SCOPE_SYSTEM        THREAD_BOUND
#define THREAD_SCOPE_PROCESS       0x00200000
#define THREAD_INHERIT_SCHED       0x00400000
#define THREAD_EXPLICIT_SCHED      0x00800000
#define THREAD_SCHED_IO            0x01000000
#define CWX_DEFAULT_THREAD_PRIORITY (-0x7fffffffL - 1L)

class CWX_API CwxThread
{
public:
    ///�����̳߳���
    enum{
        DEATH_CHECK_MSG_WATER_MASK = 5, ///<�̵߳�״̬�����Ŷ���Ϣ����
        DEATH_CHECK_UPDATE_WATER_MASK = 30 ///<�߳�ʧЧ����״̬���µ�ʱ������
    };
public :
    ///���캯��
    CwxThread(CwxAppFramework* pApp,///<app����
        CWX_UINT16 unGroupId,///<�̵߳�group id
        CWX_UINT16 unThreadId=0,///<�߳����߳�group�е����
        CWX_UINT32 uiDeathCheckMsgWaterMask=DEATH_CHECK_MSG_WATER_MASK,///<�̵߳�״̬�����Ŷ���Ϣ����
        CWX_UINT32 uiDeathCheckUpdateWaterMask=DEATH_CHECK_UPDATE_WATER_MASK///<�߳�ʧЧ����״̬���µ�ʱ������
        );
    ///��������
    virtual ~CwxThread();
public:
    /**
    @brief �����߳�
    @param [in] pThrEnv �̵߳�Tss������ָ������ͨ��onThreadCreated������
    @param [in] stack_size �̶߳�ջ�Ĵ�С����Ϊ0�������ϵͳĬ�ϴ�С��
    @return -1��ʧ�ܣ� 0���ɹ�
    */
    virtual int start(CwxTss* pThrEnv=NULL, size_t stack_size = 0);
    ///ֹͣ�߳�
    virtual void stop();
    /**
    @brief ֪ͨ�̴߳�������Ҫ�����Լ���Thread-Env�������ش�API
    @param [in] unGroup �̵߳��߳��顣
    @param [in] unThreadId �߳����߳����е���š�
    @param [out] pThrEnv ������Thread��Thread Env��
    @return -1��ʧ�ܣ� 0���ɹ�
    */
    virtual int onThreadCreated(CWX_UINT16 unGroup, CWX_UINT16 unThreadId, CwxTss*& pThrEnv);
    /**
    @brief ֪ͨ�߳��˳�
    @param [out] pThrEnv Thread��Thread Env��
    @return void
    */
    virtual void onThreadClosed(CwxTss*& pThrEnv);
    /**
    @brief �̵߳�body������Ҫ�ı��̵߳�������Ϊ�������ش�API
    @param [out] pThrEnv Thread��Thread Env��
    @return void
    */
    virtual void threadMain(CwxTss* pThrEnv);
    ///check thread �Ƿ�����������Ҫ�ı���Ĺ��������ش�API
    virtual bool isDeath();
    ///check thread �Ƿ�ֹͣ������Ҫ�ı���Ĺ��������ش�API
    virtual bool isStop();
    ///��ȡ�̵߳�TSS����Thread env
    virtual CwxTss* getTss();
    ///��ס�̡߳�����ֵ0���ɹ���-1��ʧ��
    virtual int lock();
    ///�����̡߳�����ֵ0���ɹ���-1��ʧ��
    virtual int unlock();
public:
    ///��ȡ�̵߳���Ϣ�����Ŷ���Ϣ��
    size_t getQueuedMsgNum();
    /**
    @brief ���̵߳���Ϣ��������һ������Ϣ��
    @param [in] pMsg append����Ϣ
    @return -1��ʧ�ܣ�>=0�������Ŷӵ���Ϣ����
    */
    int  append(CwxMsgBlock* pMsg);
    /**
    @brief ���̵߳���Ϣ���л�ȡһ���Ŷ���Ϣ��������Ϊ�գ���������
    @param [out] pMsg pop����Ϣ��
    @return -1��ʧ�ܣ�>=0�������Ŷӵ���Ϣ����
    */
    int  pop(CwxMsgBlock*& pMsg);
    ///��ȡ�̵߳�group id
    CWX_UINT16 getThreadId() const;
    ///��ȡ�������߳����е����
    CWX_UINT16 getGroupId() const;
public:
    /**
    @brief ����һ���̡߳�
    @param [in] func �̵߳�main������
    @param [in] args �̵߳�main�����Ĳ�����
    @param [in] flags �̵߳����Ա�־��
    @param [out] thr_id �̵߳�id��
    @param [in] priority �̵߳����ȼ���
    @param [in] stack �̵߳Ķ�ջ��
    @param [in] stacksize �̵߳Ķ�ջ�Ĵ�С��
    @return -1��ʧ�ܣ�����ԭ����errno��0�ɹ���
    */
    static int spawn(CWX_THR_FUNC func,
        void *args=NULL,
        long flags=THREAD_NEW_LWP | THREAD_JOINABLE,
        pthread_t *thr_id=NULL,
        long priority = CWX_DEFAULT_THREAD_PRIORITY,
        void *stack = NULL,
        size_t stacksize = 0);
    ///��os��pthread_join�ķ�װ��0���ɹ���-1��ʧ�ܣ�errno��ʧ�ܵĴ���
    static int join(pthread_t thread, void **value_ptr);
    ///��os��pthread_kill�ķ�װ��0���ɹ���-1��ʧ�ܣ�errno��ʧ�ܵĴ���
    static int kill(pthread_t thread, int sig);
    ///��os��pthread_self�ķ�װ������thread id��
    static pthread_t self();
    ///��os��pthread_exit�ķ�װ
    static void exit(void *value_ptr);
    ///��os��pthread_equal�ķ�װ
    static bool equal(pthread_t t1, pthread_t t2);
    ///��os��pthread_cancel�ķ�װ��0���ɹ���-1��ʧ�ܣ�errno��ʧ�ܵĴ���
    static int cancel(pthread_t thread);
private:
    static void* threadFunc(void *);
private:
    CwxAppFramework*        m_pApp;///<�ܹ���APP
    CwxTss*             m_pTssEnv;///<�̵߳�tss
    CWX_UINT16             m_unGroupId;///<�̵߳��߳���id
    CWX_UINT16             m_unThreadId;///<�߳����߳����е����
    CWX_UINT32             m_uiTheadDeathMsgWaterMask;///<�߳�death���Ķ�����Ϣ�Ŷӷ�ֵ
    CWX_UINT32             m_uiThreadDeathUpdateWaterMask;///<�߳�death�����߳���״̬���µ�ʱ�䷧ֵ
    CwxMsgQueue*         m_msgQueue;
    pthread_t              m_tid;
};

CWINUX_END_NAMESPACE

#include "CwxThread.inl"
#include "CwxPost.h"
#endif