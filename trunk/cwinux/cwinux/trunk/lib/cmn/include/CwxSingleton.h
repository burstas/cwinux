#ifndef __CWX_SINGLETON_H__
#define __CWX_SINGLETON_H__
/*
��Ȩ������
    �������ѭGNU LGPL��http://www.gnu.org/copyleft/lesser.html��
*/

/**
@file CwxSingleton.h
@brief ��ʵ������ӿڵĶ��塣
@author cwinux@gmail.com
@version 0.1
@date 2009-10-02
@warning
@bug
*/
#include "CwxPre.h"
#include "CwxGlobalMacro.h"
#include "CwxType.h"
#include "CwxStl.h"

CWINUX_BEGIN_NAMESPACE
class CwxSingletonMgr;
/**
@class CwxSingleton
@brief ��ʵ������ӿڡ�
*/
class CWX_API CwxSingleton
{
public:
    inline string const& getName() const
    {
        return m_strName;
    }
protected:
    ///���캯��������CwxSingletonMgr����ע��, strNameΪ��������֣��Ա����
    CwxSingleton(string const& strName);
    ///���������������ʵ������CwxSingletonMgr�ͷ�
    virtual ~CwxSingleton();
    friend class CwxSingletonMgr;
private:
    CwxSingleton*      m_next;
    string             m_strName;

};

CWINUX_END_NAMESPACE


#include "CwxPost.h"

#endif
