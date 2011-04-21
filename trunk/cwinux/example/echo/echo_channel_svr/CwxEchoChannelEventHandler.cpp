#include "CwxEchoChannelEventHandler.h"

/**
@brief ���ӿɶ��¼�������-1��close()�ᱻ����
@return -1������ʧ�ܣ������close()�� 0������ɹ�
*/
int CwxEchoChannelEventHandler::onInput()
{
    ssize_t recv_size = 0;
    ssize_t need_size = 0;
    need_size = CwxMsgHead::MSG_HEAD_LEN - this->m_uiRecvHeadLen;
    if (need_size > 0 )
    {//not get complete head
        recv_size = CwxSocket::recv(getHandle(), this->m_szHeadBuf + this->m_uiRecvHeadLen, need_size);
        if (recv_size <=0 )
        { //error or signal
            if ((0==recv_size) || ((errno != EWOULDBLOCK) && (errno != EINTR)))
            {
                return -1; //error
            }
            else
            {//signal or no data
                return 0;
            }
        }
        this->m_uiRecvHeadLen += recv_size;
        if (recv_size < need_size)
        {
            return 0;
        }
        this->m_szHeadBuf[this->m_uiRecvHeadLen] = 0x00;
        if (!m_header.fromNet(this->m_szHeadBuf))
        {
            CWX_ERROR(("Msg header is error."));
            return -1;
        }
        if (m_header.getDataLen() > 0) this->m_recvMsgData = CwxMsgBlockAlloc::malloc(m_header.getDataLen());
        CWX_ASSERT(this->m_uiRecvDataLen==0);
    }//end  if (need_size > 0)
    //recv data
    need_size = m_header.getDataLen() - this->m_uiRecvDataLen;
    if (need_size > 0)
    {//not get complete data
        recv_size = CwxSocket::recv(getHandle(), this->m_recvMsgData->wr_ptr(), need_size);
        if (recv_size <=0 )
        { //error or signal
            if ((errno != EWOULDBLOCK)&&(errno != EINTR))
            {
                return -1; //error
            }
            else
            {//signal or no data
                return 0;
            }
        }
        //move write pointer
        this->m_recvMsgData->wr_ptr(recv_size);
        this->m_uiRecvDataLen += recv_size;
        if (recv_size < need_size)
        {
            return 0;
        }
    }
    replyMessage();
    //notice recieving a msg.
    if (!this->m_recvMsgData) this->m_recvMsgData = CwxMsgBlockAlloc::malloc(0);
    
    CwxMsgBlockAlloc::free(m_recvMsgData);
    this->m_recvMsgData = NULL;
    this->m_uiRecvHeadLen = 0;
    this->m_uiRecvDataLen = 0;
    return 0;
}
/**
@brief ֪ͨ���ӹرա�
@return �����������ӣ�1������engine���Ƴ�ע�᣻0������engine���Ƴ�ע�ᵫ��ɾ��handler��-1����engine�н�handle�Ƴ���ɾ����
*/
int CwxEchoChannelEventHandler::onConnClosed()
{
    return -1;
}

void CwxEchoChannelEventHandler::replyMessage()
{
    ///����echo�ظ�����Ϣ���ͣ�Ϊ�������Ϣ����+1
    m_recvMsgData->event().getMsgHeader().setMsgType(m_recvMsgData->event().getMsgHeader().getMsgType() + 1);
    ///����echo�ظ������ݰ�����
    m_recvMsgData->event().getMsgHeader().setDataLen(m_recvMsgData->length());
    ///�����ظ������ݰ�
    CwxMsgBlock* pBlock = CwxMsgBlockAlloc::malloc(m_recvMsgData->length() + CwxMsgHead::MSG_HEAD_LEN);
    ///�������ݰ��İ�ͷ
    memcpy(pBlock->wr_ptr(), m_recvMsgData->event().getMsgHeader().toNet(), CwxMsgHead::MSG_HEAD_LEN);
    ///����block��дָ��
    pBlock->wr_ptr(CwxMsgHead::MSG_HEAD_LEN);
    ///�������ݰ�������
    memcpy(pBlock->wr_ptr(), m_recvMsgData->rd_ptr(), m_recvMsgData->length());
    ///����block��дָ��
    pBlock->wr_ptr(m_recvMsgData->length());
    if (!putMsg(pBlock))
    {
        CWX_ERROR(("Failure to put message"));
        CwxMsgBlockAlloc::free(pBlock);
    }
    m_ullMsgNum ++;
    if (m_ullMsgNum && !(m_ullMsgNum%10000))
    {
        char szBuf[64];
        CwxCommon::toString(m_ullMsgNum, szBuf, 10);
        CWX_INFO(("Recv echo message num:%s", szBuf));
    }


}
