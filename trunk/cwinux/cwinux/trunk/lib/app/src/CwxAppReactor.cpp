#include "CwxAppFramework.h"
#include "CwxAppReactor.h"

CWINUX_BEGIN_NAMESPACE

CwxAppReactor::CwxAppReactor()
:m_connMap(CWX_APP_MAX_IO_NUM * 2)
{
    m_owner = CwxThread::self();
    m_bStop = true;
    ///��ʼ��IO handler������
    memset(m_connId, 0x00, sizeof(m_connId));
    ///����notice pipe����
    m_pNoticePipe = NULL;
    //�¼�����
    m_engine= NULL;
}


CwxAppReactor::~CwxAppReactor()
{
    close();
}

///fork��re-init����������ֵ��0���ɹ���-1��ʧ��
int CwxAppReactor::forkReinit()
{
    if (m_engine)
    {
        m_owner = CwxThread::self();
        int ret = m_engine->forkReinit();
        if (-1 == ret)
        {
            CWX_ERROR(("Failure to invoke libevent::event_reinit"));
            return -1;
        }
        return 0;
    }
    CWX_ERROR(("Epoll engine doesn't init"));
    return -1;
}

///��reactor��return -1��ʧ�ܣ�0���ɹ�
int CwxAppReactor::open()
{
    if (!m_bStop)
    {
        CWX_ERROR(("Can't re-open the openning reactor"));
        return -1;
    }
    ///����reactor��owner
    m_owner = CwxThread::self();
    ///����reactor
    this->close();
    ///����notice pipe����
    m_pNoticePipe = new CwxAppNoticePipe();
    ///����engine
    if (m_engine)
    {
        delete m_engine;
        m_engine = NULL;
    }
    m_engine = new CwxAppEpoll();
    if (0 != m_engine->init()) return -1;
    if (m_pNoticePipe->init() != 0)
    {
        CWX_ERROR(("Failure to invoke CwxAppNoticePipe::init()"));
        return -1;
    }
    CWX_DEBUG(("Success to open CwxAppReactor"));
    return 0;
}

///�ر�reactor��return -1��ʧ�ܣ�0���ɹ�
int CwxAppReactor::close()
{
    if (!CwxThread::equal(m_owner, CwxThread::self()))
    {
        CWX_ERROR(("CwxAppReactor::close must be invoked by owner thread"));
        return -1;
    }
    m_bStop = true;
    memset(m_connId, 0x00, sizeof(m_connId));
    if (m_pNoticePipe)
    {
        delete m_pNoticePipe;
        m_pNoticePipe = NULL;
    }
    int i =0;
    m_uiCurConnId = 0;
    m_connMap.clear();
    if (m_engine) delete m_engine;
    m_engine = NULL;
    return 0;
}

/**
@brief �ܹ��¼���ѭ������API��ʵ����Ϣ�ķַ���
@return -1��ʧ�ܣ�0�������˳�
*/
int CwxAppReactor::run(CwxAppHandler4Base* noticeHandler,
                       REACTOR_EVENT_HOOK hook,
                       void* arg)
{
    int ret = 0;
    if (!m_bStop || !m_engine)
    {
        CWX_ERROR(("CwxAppReactor::open() must be invoke before CwxAppReactor::run()"));
        return -1;
    }
    ///����reactor��owner
    m_owner = CwxThread::self();
    ///ע��notice handler
    noticeHandler->setHandle(m_pNoticePipe->getPipeReader());
    if (0 != this->registerHandler(m_pNoticePipe->getPipeReader(),
        noticeHandler,
        CwxAppHandler4Base::PREAD_MASK))
    {
        CWX_ERROR(("Failure to register CwxAppHandler4Notice notice handler"));
        return -1;
    }
    m_bStop = false;

    while(!m_bStop)
    {
        {
            ///����ִ��event-loop
            CwxMutexGuard<CwxMutexLock> lock(&m_lock);
            ret = m_engine->poll();
        }
        if (m_bStop)
        {
            CWX_DEBUG(("Stop running for stop"));
            break;
        }
        if (0 != ret)
        {
            if ((-1 == ret) && (EINTR != errno))
            {
                CWX_ERROR(("Failure to running epoll with -1, errno=%d", errno));
                break;
            }
        }
        ///����hook
        if (hook)
        {
            if (0 != hook(arg))
            {
                CWX_DEBUG(("Stop running for hook() without 0"));
                break;
            }
        }
        ///�ȴ��������߳�ִ�и��ֲ�����
        m_rwLock.acquire_write();
        m_rwLock.release();
    }
    return ret;
}

/**
@brief ֹͣ�ܹ��¼���ѭ������
@return -1��ʧ�ܣ�0�������˳�
*/
int CwxAppReactor::stop()
{
    if (!CwxThread::equal(m_owner, CwxThread::self()))
    {
        m_rwLock.acquire_read();
        this->notice(NULL);
        {
            CwxMutexGuard<CwxMutexLock> lock(&m_lock);
            return _stop();
        }
        m_rwLock.release();
    }
    return _stop();
}

void CwxAppReactor::callback(CwxAppHandler4Base* handler, int mask, void *arg)
{
    CwxAppHandler4Base* handler = (CwxAppHandler4Base*)arg;
    CwxAppReactor* reactor=handler->reactor();
    if (!handler->isPersistMask())
    {
        switch(handler->getRegType())
        {
        case REG_TYPE_IO:
            reactor->m_ioHandler[handler->getHandle()].m_pHandler = NULL;
            reactor->m_ioHandler[handler->getHandle()].m_uiConnId = CWX_APP_INVALID_CONN_ID;
            break;
        case REG_TYPE_TIMEOUT:
            reactor->m_timeouts.erase(handler);
            break;
        case REG_TYPE_SIG:
            reactor->m_arrSignals[handler->getHandle()] = NULL;
            break;
        default:
            CWX_ASSERT(0);
        }
        handler->setReg(false);
    }
    int ret = handler->handle_event(event, fd);
    if (-1 == ret)
    {
        handler->close(fd);
    }
}

CWINUX_END_NAMESPACE
