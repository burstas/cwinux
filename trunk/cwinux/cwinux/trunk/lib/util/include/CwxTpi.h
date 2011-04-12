#ifndef __CWX_APP_TPI_H__
#define __CWX_APP_TPI_H__
/*
��Ȩ������
    ��������ѭGNU LGPL��http://www.gnu.org/copyleft/lesser.html����
    ��ϵ��ʽ��email:cwinux@gmail.com��΢��:http://t.sina.com.cn/cwinux
*/

/**
@file CwxAppTpi.h
@brief �̳߳ؽӿڶ���
@author cwinux@gmail.com
@version 0.1
@date 2009-07-20
@warning
@bug
*/

#include "CwxPre.h"
#include "CwxGlobalMacro.h"
#include "CwxType.h"
#include "CwxAppMacro.h"
#include "CwxTss.h"

CWINUX_BEGIN_NAMESPACE

class CwxAppFramework;
/**
@class CwxAppTpi
@brief �̳߳صĽӿڶ���
*/

class CWX_API CwxAppTpi
{
public :
    ///���캯��
    CwxAppTpi(CWX_UINT16 unGroup, CWX_UINT16 unThreadNum)
        :m_unGroup(unGroup), m_unThreadNum(unThreadNum)
    {
    }
    ///��������
    virtual ~CwxAppTpi()
    {

    }
public:
    /**
    @brief �����̳߳أ�Ϊ����ӿ�
    @param [in] pThrEnv �̳߳ص��߳�Tss�����飬����ָ������ͨ��onThreadCreated������
    @param [in] stack_size �̶߳�ջ�Ĵ�С����Ϊ0�������ϵͳĬ�ϴ�С��
    @return -1��ʧ�ܣ� 0���ɹ�
    */
    virtual int start(CwxTss** pThrEnv=NULL, size_t stack_size= 0) = 0;
    ///ֹͣ�̳߳�
    virtual void stop() = 0;
    ///check thread �Ƿ�����������Ҫ�ı���Ĺ��������ش�API
    virtual bool isDeath() = 0;
    ///check thread �Ƿ�ֹͣ������Ҫ�ı���Ĺ��������ش�API
    virtual bool isStop() = 0;
    ///��ȡ�̵߳�TSS����Thread env
    virtual CwxTss* getTss(CWX_UINT16 unThreadIndex)=0;
    ///��ס�����̳߳ء�����ֵ0���ɹ���-1��ʧ��
    virtual int lock()=0;
    ///��������̳߳ء�����ֵ0���ɹ���-1��ʧ��
    virtual int unlock()=0;
public:
    ///��ȡ�̳߳����̵߳�����
    inline CWX_UINT16 getThreadNum() const
    {
        return m_unThreadNum;
    }
    ///��ȡ�̳߳ص��߳���ID
    inline CWX_UINT16 getGroupId() const 
    {
        return m_unGroup;
    }
private:
    CWX_UINT16             m_unGroup;///<�߳���id
    CWX_UINT16             m_unThreadNum;///<�̳߳����̵߳�����
};

CWINUX_END_NAMESPACE

#include "CwxPost.h"
#endif
