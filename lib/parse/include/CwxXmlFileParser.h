#ifndef __CWX_XML_FILE_PARSER_H__
#define __CWX_XML_FILE_PARSER_H__
/*
��Ȩ������
    �������ѭGNU GPL V3��http://www.gnu.org/licenses/gpl.html����
    ��ϵ��ʽ��email:cwinux@gmail.com��΢��:http://t.sina.com.cn/cwinux
*/

/**
@file CwxXmlFileParser.h
@brief ������CwxXmlFileParser�࣬�������expatʵ��XML�ļ�����������
@author cwinux@gmail.com
@version 0.1
@date 2009-10-10
@warning
@bug
*/
#include "CwxPre.h"
#include "CwxStl.h"
#include "CwxCommon.h"
#include "CwxParseMacro.h"

#include "CwxXmlParser.h"

CWINUX_BEGIN_NAMESPACE
/**
@class CwxXmlFileParser
@brief ����CwxXmlParser����ʵ��XML�ļ���������
*/
class  CWX_API CwxXmlFileParser : public CwxXmlParser
{
public:
    ///�����ļ���strFileNameΪҪ������XML���ļ���
    CwxXmlFileParser(string const& strFileName);
    ///��������
    virtual ~CwxXmlFileParser();
protected:
    ///XML�ļ�������׼��
    virtual bool prepare();
    ///��XML�ļ��ж�ȡ��һ�������������ݿ飬-1����ʾ�ļ�β���ļ�������ͨ��status��ʶ��>=0����ȡ�����ݵĳ���
    virtual ssize_t readBlock(void);
private:
    FILE *  m_fd; ///<XML�ļ��ľ��
    string  m_strFileName;///<XML�ļ�������

};
#include "CwxPost.h"

CWINUX_END_NAMESPACE;

#endif /* _EXPATMM_EXPATXMLFILEPARSER_H */

