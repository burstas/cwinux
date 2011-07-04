#include "CwxBinLogMgr.h"

CWINUX_BEGIN_NAMESPACE
/***********************************************************************
                    CwxBinLogCursor  class
***********************************************************************/
CwxBinLogCursor::CwxBinLogCursor()
{
    m_fd = -1;
    m_bDangling = true;
    m_szHeadBuf[0] = 0x00;
    m_szErr2K[0] = 0x00;
	m_ullBlockNo = 0;
	m_uiBlockDataOffset = 0;
    m_ullSid = 0; ///<seek��sid
    m_ucSeekState = 0; ///<seek��״̬
    m_pBinLogFile = NULL;

}

CwxBinLogCursor::~CwxBinLogCursor()
{
    if (-1 != m_fd) ::close(m_fd);
}

int CwxBinLogCursor::open(char const* szFileName)
{
    if (-1 != this->m_fd)
	{
		::close(m_fd);
		m_fd = -1;
	}
    m_fd = ::open(szFileName, O_RDONLY);
    if (-1 == m_fd)
    {
        CwxCommon::snprintf(this->m_szErr2K, 2047, "Can't open file:%s.", szFileName);
        return -1;
    }
    m_strFileName = szFileName;
    m_bDangling = true;
    m_curLogHeader.reset();
    m_szErr2K[0] = 0x00;
	m_ullBlockNo = 0;
	m_uiBlockDataOffset = 0;
    return 0;
}

/**
@brief ��ȡ��ǰlog��data
@return -1��ʧ�ܣ�>=0����ȡ���ݵĳ���
*/
int CwxBinLogCursor::data(char * szBuf, CWX_UINT32& uiBufLen)
{
    int iRet;
    if (-1 == m_fd)
    {
        CwxCommon::snprintf(this->m_szErr2K, 2047, "Cursor's file handle is invalid");
        return -1;
    }
    if (m_bDangling)
    {
        CwxCommon::snprintf(this->m_szErr2K, 2047, "Cursor is dangling.");
        return -1;
    }
    if (m_curLogHeader.getLogLen())
    {
        if (uiBufLen < m_curLogHeader.getLogLen())
        {
            CwxCommon::snprintf(this->m_szErr2K, 2047, "Buf is too small, buf-size[%u], data-size[%u]",
                uiBufLen, m_curLogHeader.getLogLen());
            return -1;
        }
        iRet = pread(this->m_fd,
            szBuf,
            m_curLogHeader.getLogLen(),
            m_curLogHeader.getOffset() + CwxBinLogHeader::BIN_LOG_HEADER_SIZE);
        if (iRet != (int)m_curLogHeader.getLogLen())
        {
            if (-1 == iRet)
				return -1;
            uiBufLen = iRet;
            CwxCommon::snprintf(this->m_szErr2K, 2047, "Log's dat is less, log-data-size[%u], read-size[%u]",
                m_curLogHeader.getLogLen(), iRet);
            return -2;
        }
    }
    uiBufLen = m_curLogHeader.getLogLen();
    return uiBufLen;
}

void CwxBinLogCursor::close()
{
    if (-1 != m_fd) ::close(m_fd);
    m_fd = -1;
    m_bDangling = true;
}


///-2����������ɵļ�¼ͷ��-1��ʧ�ܣ�0��������1����ȡһ��
int CwxBinLogCursor::header(CWX_UINT64 ullOffset)
{
    int iRet;
    iRet = pread(this->m_fd, m_szHeadBuf, CwxBinLogHeader::BIN_LOG_HEADER_SIZE, ullOffset);
    if (iRet != CwxBinLogHeader::BIN_LOG_HEADER_SIZE)
    {
        if (0 == iRet)
        {
            return 0;
        }
        else if(-1 == iRet)
        {
            return -1;
        }
        CwxCommon::snprintf(this->m_szErr2K, 2047, "Log is incomplete");
        return -2;
    }
    m_curLogHeader.unserialize(m_szHeadBuf);
    if (ullOffset != m_curLogHeader.getOffset())
    {
        char szBuf1[64];
        char szBuf2[64];
        CwxCommon::snprintf(this->m_szErr2K, 2047, "Invalid binlog, offset of header[%s] is different with it's file-offset[%s].",
            CwxCommon::toString(ullOffset, szBuf1),
            CwxCommon::toString(m_curLogHeader.getOffset(), szBuf2));
        return -1;
    }
    return 1;
}

inline bool CwxBinLogCursor::preadPage(int fildes, CWX_UINT64 ullBlockNo, CWX_UINT32 uiOffset)
{
	CWX_ASSERT(uiOffset<=BINLOG_READ_BLOCK_SIZE);
	ssize_t ret = 0;
	if (ullBlockNo != m_ullBlockNo)
	{
		m_ullBlockNo = ullBlockNo;
		m_uiBlockDataOffset = 0;
	}
	if ((ullBlockNo == m_ullBlockNo)&&(uiOffset <= m_uiBlockDataOffset)) return true;
	do 
	{
		ret = ::pread(fildes, m_szReadBlock + m_uiBlockDataOffset, BINLOG_READ_BLOCK_SIZE - m_uiBlockDataOffset, (ullBlockNo<<BINLOG_READ_BLOCK_BIT) + m_uiBlockDataOffset);
		if (-1 != ret)
		{
			m_uiBlockDataOffset += ret;
			return true;
		}
		if (EOVERFLOW == errno) return true;
		if (EINTR == errno) continue;
		break;
	} while(1);
	CwxCommon::snprintf(this->m_szErr2K, 2047, "Failure to read bin-log file:%s, errno=%d.", m_strFileName.c_str(), errno);
	return false;
}

//��ȡ����
ssize_t CwxBinLogCursor::pread(int fildes, void *buf, size_t nbyte, CWX_UINT64 offset)
{
	size_t pos = 0;
	if (nbyte)
	{
		CWX_UINT64 ullStartBlock = offset>>BINLOG_READ_BLOCK_BIT;
		CWX_UINT64 ullEndBlock = (offset + nbyte - 1)>>BINLOG_READ_BLOCK_BIT;
		CWX_UINT64 ullBlockNum = ullEndBlock - ullStartBlock + 1;
		CWX_UINT32 uiBlockStartOffset = 0;
		CWX_UINT32 uiBlockEndOffset = 0;

		//get first block
		uiBlockStartOffset = offset - (ullStartBlock << BINLOG_READ_BLOCK_BIT);
		uiBlockEndOffset = ullBlockNum==1?uiBlockStartOffset + nbyte:(CWX_UINT32)BINLOG_READ_BLOCK_SIZE;
		if (!preadPage(fildes, ullStartBlock, uiBlockEndOffset)) return -1;
		if (uiBlockEndOffset <= m_uiBlockDataOffset)
		{//�����㹻
			memcpy(buf, m_szReadBlock + uiBlockStartOffset, uiBlockEndOffset - uiBlockStartOffset);
			pos = uiBlockEndOffset - uiBlockStartOffset;
			if (BINLOG_READ_BLOCK_SIZE != uiBlockEndOffset) return pos;
		}
		else
		{//not enough data
			if (uiBlockStartOffset < m_uiBlockDataOffset)
			{
				memcpy(buf, m_szReadBlock + uiBlockStartOffset, m_uiBlockDataOffset - uiBlockStartOffset);
				pos = m_uiBlockDataOffset - uiBlockStartOffset;
				return pos;
			}
			return 0;
		}
		//get middle block
		if (ullBlockNum > 2)
		{//read middle
			ssize_t ret = 0;
			do 
			{
				ret = ::pread(fildes, (char*)buf+pos, (ullBlockNum - 2)<<BINLOG_READ_BLOCK_BIT, (ullStartBlock + 1)<<BINLOG_READ_BLOCK_BIT);
				if (-1 != ret)
				{
					pos += ret;
					if (ret != ((ullBlockNum - 2)<<BINLOG_READ_BLOCK_BIT)) //finish
						return pos;
					break;
				}
				if (EOVERFLOW == errno) return pos;
				if (EINTR == errno) continue;
				CwxCommon::snprintf(this->m_szErr2K, 2047, "Failure to read bin-log file:%s, errno=%d.", m_strFileName.c_str(), errno);
				return -1;
			} while(1);
		}		
		//get last block
		{
			CWX_ASSERT(nbyte > pos);
			uiBlockEndOffset = nbyte - pos;
			if (!preadPage(fildes, ullEndBlock, uiBlockEndOffset)) return -1;
			if (uiBlockEndOffset <= m_uiBlockDataOffset)
			{//�����㹻
				memcpy((char*)buf + pos, m_szReadBlock, uiBlockEndOffset);
				pos += uiBlockEndOffset;
			}
			else
			{//not enough data
				memcpy((char*)buf + pos, m_szReadBlock, m_uiBlockDataOffset);
				pos += m_uiBlockDataOffset;
			}

		}
	}
	return pos;
}


/***********************************************************************
                    CwxBinLogFile  class
***********************************************************************/
CwxBinLogFile::CwxBinLogFile(CWX_UINT32 ttDay, CWX_UINT32 uiFileNo, CWX_UINT64 ullMaxFileSize)
{
    m_bValid = false;
    if (ullMaxFileSize<MIN_BINLOG_FILE_SIZE)
    {
        m_ullMaxFileSize = MIN_BINLOG_FILE_SIZE;
    }
    else
    {
        m_ullMaxFileSize = ullMaxFileSize;
    }
    m_ullMinSid = 0;
    m_ullMaxSid = 0;
    m_ttMinTimestamp = 0;
    m_ttMaxTimestamp = 0;
    m_uiLogNum = 0;
    m_bReadOnly = true;
    m_fd = -1;
    m_indexFd = -1;
    m_ullFileSize = 0;
    m_ullIndexFileSize = 0;
    m_ullPrevLogOffset = 0;
    m_ttDay = ttDay;
    m_uiFileNo = uiFileNo;
    m_prevBinLogFile = NULL;
    m_nextBinLogFile = NULL;
}

CwxBinLogFile::~CwxBinLogFile()
{
    close();
}


//0: success, 
//-1:failure,
int CwxBinLogFile::open(char const* szPathFile,
                        bool bReadOnly,
                        bool bCreate,
                        char* szErr2K)
{
    string strIndexPathFileName;
    //�رն���
    close();
    m_bReadOnly = bCreate?false:bReadOnly;
    m_strPathFileName = szPathFile;
    m_strIndexFileName = m_strPathFileName + ".idx";

    //��ȡbinlog�ļ��Ĵ�С
    if (CwxFile::isFile(m_strPathFileName.c_str()))
    {
        m_ullFileSize = CwxFile::getFileSize(m_strPathFileName.c_str());
        if (-1 == (CWX_INT64)m_ullFileSize)
        {
            if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to get binlog file's size, file:%s, errno=%d", m_strPathFileName.c_str(), errno);
            return -1;
        }
    }
    else
    {
        m_ullFileSize = -1;
    }
    //��ȡbinlog�����ļ��Ĵ�С
    if (CwxFile::isFile(m_strIndexFileName.c_str()))
    {
        m_ullIndexFileSize = CwxFile::getFileSize(m_strIndexFileName.c_str());
        if (-1 == (CWX_INT64)m_ullIndexFileSize)
        {
            if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to get binlog index file's size, file:%s, errno=%d", m_strIndexFileName.c_str(), errno);
            return -1;
        }
    }
    else
    {
        m_ullIndexFileSize = -1;
    }

    //����binlog�ļ����������ļ�
    if (bCreate)
    {
        if (-1 == mkBinlog(szErr2K)) return -1;
    }
    else
    {
        if (-1 == (CWX_INT64)m_ullFileSize)
        {//binlog �ļ�������
            if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Binlog file doesn't exist, file:%s", m_strPathFileName.c_str());
            return -1;
        }
    }
    //��binlog�ļ�
    m_fd = ::open(m_strPathFileName.c_str(),  O_RDWR);
    if (-1 == m_fd)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Can't open binlog file:%s", m_strPathFileName.c_str());
        return -1;
    }
    //�������ļ�
    if (-1 != (CWX_INT64)m_ullIndexFileSize)
    {//�����ļ�����
        m_indexFd = ::open(m_strIndexFileName.c_str(),  O_RDWR);
    }
    else
    {//�����ļ�������
        m_indexFd = ::open(m_strIndexFileName.c_str(),  O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        //����index�ļ��Ĵ�СΪ0.
        m_ullIndexFileSize = 0;
    }
    if (-1 == m_indexFd)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Can't open binlog's index file:%s", m_strIndexFileName.c_str());
        return -1;
    }
    //��֤binlog�ļ��������ļ�һ��
    if (-1 == prepareFile(szErr2K)) return -1;
    //�����ֻ������ر�binlog��������io handle
    if (m_bReadOnly)
    {
        if (-1 != m_fd) ::close(m_fd);
        m_fd = -1;
        if (-1 != m_indexFd) ::close(m_indexFd);
        m_indexFd = -1;
    }

    m_bValid = true;
    return 0;
}

//-1��ʧ�ܣ�0����־�ļ����ˣ�1���ɹ���
int CwxBinLogFile::append(CWX_UINT64 ullSid,
                          CWX_UINT32 ttTimestamp,
                          CWX_UINT32 uiGroup,
                          CWX_UINT32 uiType,
                          CWX_UINT32 uiAttr,
                          char const* szData,
                          CWX_UINT32 uiDataLen,
                          char* szErr2K)
{
    char szBuf[CwxBinLogHeader::BIN_LOG_HEADER_SIZE];
    if (m_ullFileSize + FREE_BINLOG_FILE_SIZE + CwxBinLogHeader::BIN_LOG_HEADER_SIZE + uiDataLen >= m_ullMaxFileSize) return 0; //it's full.

    if (this->m_bReadOnly)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "The log is opened in read mode, can't append record.file:%s", m_strPathFileName.c_str());
        return -1;
    }
    if (!m_bValid)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "The log is not valid, can't append record. file:%s", m_strPathFileName.c_str());
        return -1;
    }
    if (ttTimestamp < m_ttMaxTimestamp) ttTimestamp = m_ttMaxTimestamp;
    //sid��������
    if (!m_uiLogNum)
    {
        if (ullSid <= m_ullMaxSid)
        {
            char szBuf1[64];
            char szBuf2[64];
            if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "binlog[%s]'s sid [%s] is more than the appending binlog's sid[%s]",
                m_strPathFileName.c_str(),
                CwxCommon::toString(m_ullMaxSid, szBuf1),
                CwxCommon::toString(ullSid, szBuf2));
            return -1;
        }
    }
    CwxBinLogHeader header(ullSid,
        (CWX_UINT32)ttTimestamp,
        m_ullFileSize,
        uiDataLen,
        m_uiLogNum,
        m_ullPrevLogOffset,
        uiGroup,
        uiType,
        uiAttr);
    //���������ļ�
    header.serialize(szBuf);
    if (CwxBinLogHeader::BIN_LOG_HEADER_SIZE != pwrite(m_fd, szBuf, CwxBinLogHeader::BIN_LOG_HEADER_SIZE, m_ullFileSize))
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to write log header to binlog:%s, errno=%d", m_strPathFileName.c_str(), errno);
        return -1;
    }
    if (uiDataLen)
    {
        if ((int)uiDataLen != pwrite(m_fd, szData, uiDataLen, m_ullFileSize + CwxBinLogHeader::BIN_LOG_HEADER_SIZE))
        {
            if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to write log data to binlog:%s, errno=%d", m_strPathFileName.c_str(), errno);
            return -1;
        }
    }
    //���������ļ�
    CwxBinLogIndex index(header);
    if (0 != writeIndex(m_indexFd, index, m_ullIndexFileSize, szErr2K)) return -1;
    ///����ǰһ��binlog���ļ�offset
    m_ullPrevLogOffset = m_ullFileSize;
    ///�޸��ļ��Ĵ�С
    m_ullFileSize += CwxBinLogHeader::BIN_LOG_HEADER_SIZE + uiDataLen;
    ///�޸������ļ��Ĵ�С
    m_ullIndexFileSize += CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
    //����sid��ʱ���
    m_ullMaxSid = ullSid;
    m_ttMaxTimestamp = ttTimestamp;
    if (!m_uiLogNum)
    {
        m_ullMinSid = ullSid;
        m_ttMinTimestamp = ttTimestamp;
    }
    //��¼����1
    m_uiLogNum ++;
    return 1;
}

// -1��ʧ�ܣ�0���ɹ���
int CwxBinLogFile::commit(char* szErr2K)
{
    if (this->m_bReadOnly)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "The log is opened in read mode, can't commit.file:%s", m_strPathFileName.c_str());
        return -1;
    }
    if (!m_bValid)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "The log is not valid, can't commit. file:%s", m_strPathFileName.c_str());
        return -1;
    }
    fsync(m_fd);
    fsync(m_indexFd);
    return 0;
}

int CwxBinLogFile::upper(CWX_UINT64 ullSid, CwxBinLogIndex& item, CWX_UINT32& index, char* szErr2K)
{
	CWX_ASSERT(m_bValid);
	if (!m_bValid)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "CwxBinlogFile is invalid, file:%s", m_strPathFileName.c_str());
		return -1;
	}
	if (!m_uiLogNum) return 0;

	//����ָ����SID��λ
	int fd = -1;
	CWX_UINT64 ullOffset = 0;
	//�۰����
	fd = ::open(m_strIndexFileName.c_str(), O_RDONLY);
	if (-1 == fd)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to open index file:%s, errno=%d", m_strIndexFileName.c_str(), errno);
		return -1;
	}
	if (ullSid >= m_ullMaxSid)
	{
		::close(fd);
		return 0;///������
	}
	if (ullSid < m_ullMinSid)
	{
		index = 0;
		if (0 != readIndex(fd, item, 0, szErr2K))
		{
			::close(fd);
			return -1;
		}
		::close(fd);
		return 1;
	}

	CWX_UINT32 uiStart = 0;
	CWX_UINT32 uiEnd = m_uiLogNum - 1;
	CWX_UINT32 uiMid = 0;
	while(uiEnd >= uiStart)
	{
		uiMid = (uiStart + uiEnd)/2;
		ullOffset = uiMid;
		ullOffset *= CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
		if (0 != readIndex(fd, item, ullOffset, szErr2K))
		{
			::close(fd);
			return -1;
		}
		if (ullSid == item.getSid())
		{
			break;
		}
		else if (ullSid < item.getSid())
		{
			uiEnd = uiMid-1;
		}
		else
		{
			uiStart = uiMid+1;
		}
	}
	index = uiMid;
	if (ullSid >= item.getSid())
	{//next item
		ullOffset += CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
		index++;
		if (0 != readIndex(fd, item, ullOffset, szErr2K))
		{
			::close(fd);
			return -1;
		}
	}
	::close(fd);
	return 1;
}

// -1��ʧ�ܣ�0�������ڣ�1������
int CwxBinLogFile::lower(CWX_UINT64 ullSid, CwxBinLogIndex& item, CWX_UINT32& index, char* szErr2K)
{
	CWX_ASSERT(m_bValid);
	if (!m_bValid)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "CwxBinlogFile is invalid, file:%s", m_strPathFileName.c_str());
		return -1;
	}
	if (!m_uiLogNum) return 0;

	//����ָ����SID��λ
	int fd = -1;
	CWX_UINT64 ullOffset = 0;
	//�۰����
	fd = ::open(m_strIndexFileName.c_str(), O_RDONLY);
	if (-1 == fd)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to open index file:%s, errno=%d", m_strIndexFileName.c_str(), errno);
		return -1;
	}
	if (ullSid < m_ullMinSid)
	{
		::close(fd);
		return 0;///������
	}
	if (ullSid >= m_ullMaxSid)
	{
		//��ȡ���һ��
		index = m_uiLogNum - 1;
		if (0 != readIndex(fd, item, index * CwxBinLogIndex::BIN_LOG_INDEX_SIZE, szErr2K))
		{
			::close(fd);
			return -1;
		}
		::close(fd);
		return 1;
	}

	CWX_UINT32 uiStart = 0;
	CWX_UINT32 uiEnd = m_uiLogNum - 1;
	CWX_UINT32 uiMid = 0;
	while(uiEnd >= uiStart)
	{
		uiMid = (uiStart + uiEnd)/2;
		ullOffset = uiMid;
		ullOffset *= CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
		if (0 != readIndex(fd, item, ullOffset, szErr2K))
		{
			::close(fd);
			return -1;
		}
		if (ullSid == item.getSid())
		{
			break;
		}
		else if (ullSid < item.getSid())
		{
			uiEnd = uiMid-1;
		}
		else
		{
			uiStart = uiMid+1;
		}
	}
	index = uiMid;
	if (ullSid < item.getSid())
	{//next item
		ullOffset -= CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
		index--;
		if (0 != readIndex(fd, item, ullOffset, szErr2K))
		{
			::close(fd);
			return -1;
		}
	}
	::close(fd);
	return 1;
}

/// -1��ʧ�ܣ�0���ɹ�
int CwxBinLogFile::truncate(CWX_UINT64 ullSid, char* szErr2K)
{
	CWX_ASSERT(m_bValid);
	if (!m_bValid)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "CwxBinlogFile is invalid, file:%s", m_strPathFileName.c_str());
		return -1;
	}
	if (m_bReadOnly)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "CwxBinlogFile is readonly, file:%s", m_strPathFileName.c_str());
		return -1;
	}
	CwxBinLogIndex index;
	CWX_UINT32 pos=0;
	int ret = upper(ullSid, index, pos, szErr2K);
	if (-1 == ret) return -1;
	if (0 == ret) return 0; ///���м�¼��С��ullSid
	//truncate index �ļ�
	off_t offset = pos;
	offset *= CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
	if (0 != ftruncate(m_indexFd, offset))
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to truncate index file:%s, errno=%d", m_strIndexFileName.c_str(), errno);
		close();
		return -1;
	}
	offset = index.getOffset();
	if (0 != ftruncate(m_fd, offset))
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to truncate data file:%s, errno=%d", m_strPathFileName.c_str(), errno);
		close();
		return -1;
	}
	//���´��ļ�
	string strFileName = m_strPathFileName;
	return open(strFileName.c_str(), false, false, szErr2K);
}

//-2����������ɵļ�¼ͷ��-1��ʧ�ܣ�0�������ڣ�1����λ��ָ����λ��
int CwxBinLogFile::seek(CwxBinLogCursor& cursor, CWX_UINT8 ucMode)
{
    CWX_ASSERT(m_bValid);
	if (!m_bValid)
	{
		CwxCommon::snprintf(cursor.m_szErr2K, 2047, "CwxBinlogFile is invalid, file:%s", m_strPathFileName.c_str());
		return -1;
	}
    int iRet = cursor.open(m_strPathFileName.c_str());
    if (-1 == iRet) return -1;
    if (SEEK_START == ucMode)  return cursor.seek(0);
    if (SEEK_TAIL == ucMode) return cursor.seek(m_ullPrevLogOffset);
    
    //����ָ����SID��λ
	CwxBinLogIndex item;
	CWX_UINT32 index;
	iRet = upper(cursor.m_ullSid, item, index, cursor.m_szErr2K);
	if (1 != iRet) return iRet;
    return cursor.seek(item.getOffset());
}


void CwxBinLogFile::reset()
{
    m_bValid = false;
    m_strPathFileName.erase();
    m_strIndexFileName.erase();
    m_ullMinSid = 0;
    m_ullMaxSid = 0;
    m_ttMinTimestamp = 0;
    m_ttMaxTimestamp = 0;
    m_uiLogNum = 0;
    m_bReadOnly = true;
    m_fd = -1;
    m_indexFd = -1;
    m_ullFileSize = 0;
    m_ullIndexFileSize = 0;
    m_ullPrevLogOffset = 0;
    m_prevBinLogFile = NULL;
    m_nextBinLogFile = NULL;
}

void CwxBinLogFile::remove(char const* szPathFileName)
{
    //ɾ��binlog�ļ�
    CwxFile::rmFile(szPathFileName);
    //ɾ�������ļ�
    string strIndexFile=szPathFileName;
    strIndexFile += ".idx";
    CwxFile::rmFile(strIndexFile.c_str());
}

//�ر�
void CwxBinLogFile::close()
{
    if (-1 != m_fd) ::close(m_fd);
    m_fd = -1;
    if (-1 != m_indexFd) ::close(m_indexFd);
    m_indexFd = -1;
    reset();
}


int CwxBinLogFile::mkBinlog(char* szErr2K)
{
    //pretect the sync-log file
    if ((-1 != (CWX_INT64)m_ullFileSize) && (m_ullFileSize > CwxBinLogHeader::BIN_LOG_HEADER_SIZE * 2))
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "binlog file [%s] exists, can't create.", m_strPathFileName.c_str());
        return -1;
    }
    int fd=-1;
    //������ļ����ݵķ�ʽ���ļ�
    fd = ::open(m_strPathFileName.c_str(),  O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if (-1 == fd)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to create binlog file [%s] .", m_strPathFileName.c_str());
        return -1;
    }
    //���������ļ���СΪ0
    m_ullFileSize = 0;
    ::close(fd);

    //������ļ����ݵķ�ʽ���ļ�
    fd = ::open(m_strIndexFileName.c_str(),  O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if (-1 == fd)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to create binlog's index file [%s] .", m_strIndexFileName.c_str());
        return -1;
    }
    //���������ļ���СΪ0
    m_ullIndexFileSize = 0; 
    ::close(fd);
    return 0;
}

//-1��ʧ�ܣ�0���ɹ���
int CwxBinLogFile::prepareFile(char* szErr2K)
{
    int iRet = isRebuildIndex(szErr2K);
    if (-1 == iRet) return -1;
    if (1 == iRet)
    {//rebuilt index
        if (0 != createIndex(szErr2K)) return -1;
    }
    //��ȡbinlog����
    CWX_ASSERT(!(m_ullIndexFileSize%CwxBinLogIndex::BIN_LOG_INDEX_SIZE));
    m_uiLogNum = m_ullIndexFileSize /CwxBinLogIndex::BIN_LOG_INDEX_SIZE;

    //�������һ����¼�Ŀ�ʼλ��
    m_ullPrevLogOffset = 0;

    if (m_uiLogNum)
    {//��¼����
        //��ȡ��С��sid��timestamp
        CwxBinLogIndex index;
        if (0 != readIndex(m_indexFd, index, 0, szErr2K)) return -1;
        m_ullMinSid = index.getSid();
        m_ttMinTimestamp = index.getDatetime();
        //��ȡ����sid, timestamp
        if (0 != readIndex(m_indexFd, index, m_ullIndexFileSize - CwxBinLogIndex::BIN_LOG_INDEX_SIZE)) return -1;
        m_ullMaxSid = index.getSid();
        m_ttMaxTimestamp = index.getDatetime();
        m_ullPrevLogOffset = index.getOffset();
        if (m_ullMinSid > m_ullMaxSid)
        {
            char szBuf1[64];
            char szBuf2[64];
            if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Binlog file's min-sid[%s] more than max sid[%s], file:%s",
                CwxCommon::toString(m_ullMinSid, szBuf1),
                CwxCommon::toString(m_ullMaxSid, szBuf2),
                m_strPathFileName.c_str());
            return -1;
        }
    }
    else
    {
        m_ullMinSid = 0;
        m_ullMaxSid = 0;
        m_ttMinTimestamp = 0;
        m_ttMaxTimestamp = 0;
    }
    return 0;
}

//-1��ʧ�ܣ�0������Ҫ��1����Ҫ��
int CwxBinLogFile::isRebuildIndex(char* szErr2K)
{
    //������������
    if (m_ullIndexFileSize%CwxBinLogIndex::BIN_LOG_INDEX_SIZE) return 1;

    //����Ϊ�գ������ݲ�Ϊ��
    if (!m_ullIndexFileSize) return 1;

    //���������¼��binlog�ļ���С��binlog�ļ�����Ĵ�С��ϵ
    char szBuf[CwxBinLogIndex::BIN_LOG_INDEX_SIZE];
    CwxBinLogIndex index;
    //��ȡ����sid, timestamp
    if (CwxBinLogIndex::BIN_LOG_INDEX_SIZE != pread(m_indexFd,
        &szBuf,
        CwxBinLogIndex::BIN_LOG_INDEX_SIZE,
        m_ullIndexFileSize - CwxBinLogIndex::BIN_LOG_INDEX_SIZE))
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to read binlog index, file:%s, errno=%d", this->m_strIndexFileName.c_str(), errno);
        return -1;
    }
    
    index.unserialize(szBuf);
    //�������Ҫ�ؽ�
    if (index.getOffset() + index.getLogLen() + CwxBinLogHeader::BIN_LOG_HEADER_SIZE != m_ullFileSize) return 1;
    //����Ҫ�ؽ�
    return 0;
}

//-1��ʧ�ܣ�0���ɹ���
int CwxBinLogFile::createIndex(char* szErr2K)
{
    CwxBinLogIndex index;
    CwxBinLogCursor cursor;
    int iRet = cursor.open(m_strPathFileName.c_str());
    m_ullIndexFileSize = 0;
    if (-1 == iRet)
    {
        if (szErr2K) strcpy(szErr2K, cursor.getErrMsg());
        return -1;
    }

    while(1 == (iRet = cursor.next()))
    {
        index = cursor.getHeader();
        if (0 != writeIndex(m_indexFd, index, m_ullIndexFileSize, szErr2K)) return -1;
        m_ullIndexFileSize += CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
    }
    if (-1 == iRet)
    {
        if (szErr2K) strcpy(szErr2K, cursor.getErrMsg());
        return -1;
    }
    if (m_ullIndexFileSize)
    {//������Ч��binlog
        m_ullFileSize = index.getOffset() + index.getLogLen() + CwxBinLogHeader::BIN_LOG_HEADER_SIZE;
    }
    else
    {//��������Ч��binlog
        m_ullFileSize = 0;
    }
    //truncate binlog �ļ�
    ftruncate(m_fd, m_ullFileSize);
    //truncate index �ļ�
    ftruncate(m_indexFd, m_ullIndexFileSize);
    return 0;
}


/***********************************************************************
                    CwxBinLogMgr  class
***********************************************************************/

CwxBinLogMgr::CwxBinLogMgr(char const* szLogPath, char const* szFilePrex, CWX_UINT64 ullMaxFileSize, bool bDelOutManageLogFile)
{
    m_bValid = false;
    strcpy(m_szErr2K, "Not init.");
    m_pCurBinlog = NULL;
    m_strLogPath = szLogPath;
    if ('/' !=m_strLogPath[m_strLogPath.length() - 1]) m_strLogPath += "/";
    m_strPrexLogPath = m_strLogPath + szFilePrex + "/";
    m_strFilePrex = szFilePrex;
    m_ullMaxFileSize = ullMaxFileSize;
    m_uiMaxDay = DEF_MANAGE_MAX_DAY;
    m_bDelOutManageLogFile = bDelOutManageLogFile;
    m_fdLock = -1;
    m_ullMinSid = 0; ///<binlog�ļ�����Сsid
    m_ullMaxSid = 0; ///<binlog�ļ������sid
    m_ttMinTimestamp = 0; ///<binlog�ļ���log��ʼʱ��
    m_ttMaxTimestamp = 0; ///<binlog�ļ���log����ʱ��

}

CwxBinLogMgr::~CwxBinLogMgr()
{
    if (m_pCurBinlog) delete m_pCurBinlog;
    CWX_UINT32 i = 0;
    for (i=0; i<m_arrBinlog.size(); i++)
    {
        delete m_arrBinlog[i];
    }
    m_arrBinlog.clear();
	set<CwxBinLogCursor*>::iterator iter =  m_cursorSet.begin();
	while(iter != m_cursorSet.end())
	{
		delete *iter;
		iter++;
	}
}

// -1��ʧ�ܣ�0���ɹ���
int CwxBinLogMgr::init(CWX_UINT32 uiMaxDay, char* szErr2K)
{
    ///д������
    CwxWriteLockGuard<CwxRwLock> lock(&m_rwLock);
    this->_clear();
    m_bValid = false;
    strcpy(m_szErr2K, "Not init.");
    if (uiMaxDay < MIN_MANAGE_DAY) m_uiMaxDay = MIN_MANAGE_DAY;
    if (uiMaxDay > MAX_MANAGE_DAY) m_uiMaxDay = MAX_MANAGE_DAY;
    m_uiMaxDay = uiMaxDay;

    //���binlog��Ŀ¼�����ڣ��򴴽���Ŀ¼
    if (!CwxFile::isDir(m_strLogPath.c_str()))
    {
        if (!CwxFile::createDir(m_strLogPath.c_str()))
        {
            CwxCommon::snprintf(m_szErr2K, 2047, "Failure to create binlog path:%s, errno=%d", m_strLogPath.c_str(), errno);
            if (szErr2K) strcpy(szErr2K, m_szErr2K);
            return -1;
        }
    }
    if (!CwxFile::isDir(m_strPrexLogPath.c_str()))
    {
        if (!CwxFile::createDir(m_strPrexLogPath.c_str()))
        {
            CwxCommon::snprintf(m_szErr2K, 2047, "Failure to create binlog path:%s, errno=%d", m_strPrexLogPath.c_str(), errno);
            if (szErr2K) strcpy(szErr2K, m_szErr2K);
            return -1;
        }
    }
    //��ȡϵͳ���ļ�
    string strLockFile=m_strLogPath + m_strFilePrex + ".lock";
    if (!CwxFile::isFile(strLockFile.c_str()))
    {///�������ļ�
        m_fdLock = ::open(strLockFile.c_str(),  O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    }
    else
    {
        m_fdLock = ::open(strLockFile.c_str(),  O_RDWR);
    }
    if (-1 == m_fdLock)
    {
        CwxCommon::snprintf(m_szErr2K, 2047, "Failure to open  binlog lock file:%s, errno=%d", strLockFile.c_str(), errno);
        if (szErr2K) strcpy(szErr2K, m_szErr2K);
        return -1;
    }
    if (!CwxFile::lock(m_fdLock))
    {
        CwxCommon::snprintf(m_szErr2K, 2047, "Failure to lock  binlog lock file:%s, errno=%d", strLockFile.c_str(), errno);
        if (szErr2K) strcpy(szErr2K, m_szErr2K);
        return -1;
    }

    //��ȡĿ¼�µ������ļ�
    list<string> files;
    if (!CwxFile::getDirFile(m_strPrexLogPath, files))
    {
        CwxCommon::snprintf(m_szErr2K, 2047, "Failure to create binlog path:%s, errno=%d", m_strPrexLogPath.c_str(), errno);
        if (szErr2K) strcpy(szErr2K, m_szErr2K);
        return -1;
    }
    //��ȡĿ¼�µ�����binlog�ļ������ŵ�map�У�����map������������ļ�
    string strPathFile;
    list<string>::iterator iter=files.begin();
    map<CwxBinLogFileItem, string> fileMap;
    CWX_UINT32 ttDay = 0;
    CWX_UINT32 uiFileNo = 0;
    while(iter != files.end())
    {
        strPathFile = m_strPrexLogPath + *iter;
        if (isBinLogFile(strPathFile))
        {
            uiFileNo = getBinLogFileNo(strPathFile, ttDay);
            CwxBinLogFileItem item(ttDay, uiFileNo);
            fileMap[item] = strPathFile;
        }
        iter++;
    }
    map<CwxBinLogFileItem, string>::reverse_iterator map_iter = fileMap.rbegin();

    CwxBinLogFile* pBinLogFile = NULL;
    while(map_iter != fileMap.rend())
    {
        pBinLogFile = new CwxBinLogFile(map_iter->first.getDay(), map_iter->first.getFileNo(), m_ullMaxFileSize);
        if (0 != pBinLogFile->open(map_iter->second.c_str(),
            m_pCurBinlog?true:false,
            false,
            m_szErr2K))
        {
            delete pBinLogFile;
            if (szErr2K) strcpy(szErr2K, m_szErr2K);
            return -1;
        }
        if (!m_pCurBinlog)
        {
            m_ttMaxTimestamp = pBinLogFile->getMaxTimestamp();
            m_ttMinTimestamp = pBinLogFile->getMinTimestamp();
        }
        else
        {
            m_ttMinTimestamp = pBinLogFile->getMinTimestamp();
        }
        //���binlog��������ķ�Χ����break
        if (!_isManageBinLogFile(pBinLogFile))
        {
            CwxFile::rmFile(pBinLogFile->getDataFileName().c_str());
            CwxFile::rmFile(pBinLogFile->getIndexFileName().c_str());
            delete pBinLogFile;
            break;
        }
        pBinLogFile->m_nextBinLogFile = NULL;
        pBinLogFile->m_prevBinLogFile = NULL;
        if (!m_pCurBinlog)
        {
            m_pCurBinlog = pBinLogFile;
            m_ullMaxSid = pBinLogFile->getMaxSid();
            m_ullMinSid = pBinLogFile->getMinSid();
        }
        else
        {
            ///���ս�����ȡ�ļ�����Ϊ���������binlog�ļ���sidӦ��С�����е�sid
            if (pBinLogFile->getMaxSid() >= getMinSid())
            {
                char szBuf1[64];
                char szBuf2[64];
                CwxCommon::snprintf(m_szErr2K, 2047, "BinLog file[%s]'s max sid[%s] is more than the existing min sid[%s]",
                    pBinLogFile->getDataFileName().c_str(),
                    CwxCommon::toString(pBinLogFile->getMaxSid(), szBuf1),
                    CwxCommon::toString(getMinSid(), szBuf2)
                    );
                delete pBinLogFile;
                if (szErr2K) strcpy(szErr2K, m_szErr2K);
                return -1;
            }
            if (0 == m_arrBinlog.size())
            {
                m_pCurBinlog->m_prevBinLogFile = pBinLogFile;
                pBinLogFile->m_nextBinLogFile = m_pCurBinlog;
            }
            else
            {
                m_arrBinlog[0]->m_prevBinLogFile = pBinLogFile;
                pBinLogFile->m_nextBinLogFile = m_arrBinlog[0];
            }
            m_arrBinlog.insert(m_arrBinlog.begin(), pBinLogFile);
            m_ullMinSid = pBinLogFile->getMinSid();
        }
        map_iter++;
    }
    
    _outputManageBinLog();
    m_bValid = true;
    m_szErr2K[0] = 0x00;

    return 0;
}

//-1��ʧ�ܣ�0���ɹ���
int CwxBinLogMgr::append(CWX_UINT64 ullSid,
                         CWX_UINT32 ttTimestamp,
                         CWX_UINT32 uiGroup,
                         CWX_UINT32 uiType,
                         CWX_UINT32 uiAttr,
                         char const* szData,
                         CWX_UINT32 uiDataLen,
                         char* szErr2K)
{
	if(!m_bValid)
	{
		if (szErr2K) strcpy(szErr2K, m_szErr2K);
		return -1;
	}
    ///д������
    CwxWriteLockGuard<CwxRwLock> lock(&m_rwLock);
    //���û��binlog�ļ����򴴽���ʼbinlog�ļ�����ʼ�ļ����Ϊ0
    if (ttTimestamp < m_ttMaxTimestamp) ttTimestamp = m_ttMaxTimestamp;
    if (!m_pCurBinlog)
    {
        string strPathFile;
        m_pCurBinlog = new CwxBinLogFile(CwxDate::trimToDay(ttTimestamp), 0, m_ullMaxFileSize);
        getFileNameByFileNo(0, m_pCurBinlog->getFileDay(), strPathFile);
        if (0 != m_pCurBinlog->open(strPathFile.c_str(),
            false,
            true,
            m_szErr2K))
        {
            delete m_pCurBinlog;
            m_pCurBinlog = NULL;
            m_bValid = false;
            if (szErr2K) strcpy(szErr2K, m_szErr2K);
            return -1;
        }
    }

    if (!m_bValid)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Binlog manage is invalid, error:%s", m_szErr2K);
        return -1;
    }
    if (ullSid <= m_ullMaxSid)
    {
        char szBuf1[64];
        char szBuf2[64];
        CwxCommon::snprintf(m_szErr2K, 2047, "Log's sid[%s] is no more than max sid[%s]", CwxCommon::toString(ullSid, szBuf1), CwxCommon::toString(m_ullMaxSid, szBuf2));
        if (szErr2K) strcpy(szErr2K, m_szErr2K);
        return -1;
    }
    if (MIN_SID_NO > ullSid)
    {
        char szBuf1[64];
        char szBuf2[64];
        CwxCommon::snprintf(m_szErr2K, 2047, "Log's sid[%s] is less than min sid[%s]", CwxCommon::toString(ullSid, szBuf1), CwxCommon::toString((CWX_UINT64)MIN_SID_NO, szBuf2));
        if (szErr2K) strcpy(szErr2K, m_szErr2K);
        return -1;
    }

    if ((CWX_UINT32)CwxDate::trimToDay(ttTimestamp) > m_pCurBinlog->getFileDay())///new day
    {
        string strPathFile;
        getFileNameByFileNo(0, CwxDate::trimToDay(ttTimestamp), strPathFile);
        CwxBinLogFile* pBinLogFile = new CwxBinLogFile(CwxDate::trimToDay(ttTimestamp), 0, m_ullMaxFileSize);
        if (0 != pBinLogFile->open(strPathFile.c_str(),
            false,
            true,
            m_szErr2K))
        {
            delete pBinLogFile;
            m_bValid = false;
            if (szErr2K) strcpy(szErr2K, m_szErr2K);
            return -1;
        }
        ///���õ�ǰ��binlogΪֻ��
        m_pCurBinlog->setReadOnly();
        ///����ǰ��binlog���ŵ�m_arrBinlog������
        m_arrBinlog.push_back(m_pCurBinlog);
        pBinLogFile->m_nextBinLogFile = NULL;
        pBinLogFile->m_prevBinLogFile = m_pCurBinlog;
        m_pCurBinlog->m_nextBinLogFile = pBinLogFile;
        m_pCurBinlog = pBinLogFile;
        //����Ƿ��г�������Χ��binlog�ļ�
        while(m_arrBinlog.size())
        {
            if (_isManageBinLogFile(m_arrBinlog[0])) break;
            CwxFile::rmFile(m_arrBinlog[0]->getDataFileName().c_str());
            CwxFile::rmFile(m_arrBinlog[0]->getIndexFileName().c_str());
            delete m_arrBinlog[0];
            m_arrBinlog.erase(m_arrBinlog.begin());
        }
        if (m_arrBinlog.size())
        {
            m_arrBinlog[0]->m_prevBinLogFile = NULL;
            m_ullMinSid = m_arrBinlog[0]->getMinSid();
            m_ttMinTimestamp = m_arrBinlog[0]->getMinTimestamp();
        }
        else
        {
            m_ullMinSid = 0;
            m_ttMinTimestamp = 0;
        }
        ///������������binlog�ļ���Ϣ
        _outputManageBinLog();
    }

    int iRet = m_pCurBinlog->append(ullSid,
        ttTimestamp,
        uiGroup,
        uiType,
        uiAttr,
        szData,
        uiDataLen,
        m_szErr2K);
    if (-1 == iRet)
    {
        m_bValid = false;
        if (szErr2K) strcpy(szErr2K, m_szErr2K);
        return -1;
    }
    if (1 == iRet)
    {
        m_ullMaxSid = ullSid;
        m_ttMaxTimestamp = ttTimestamp;
        if ( 0 == m_ullMinSid)
        {
            m_ullMinSid = ullSid;
            m_ttMinTimestamp = ttTimestamp;
        }
        return 0;
    }
    if (0 == iRet)
    {
        string strPathFile;
        getFileNameByFileNo(m_pCurBinlog->getFileNo() + 1,
            m_pCurBinlog->getFileDay(),
            strPathFile);
        CwxBinLogFile* pBinLogFile = new CwxBinLogFile(m_pCurBinlog->getFileDay(),
            m_pCurBinlog->getFileNo() + 1, 
            m_ullMaxFileSize);
        if (0 != pBinLogFile->open(strPathFile.c_str(),
            false,
            true,
            m_szErr2K))
        {
            delete pBinLogFile;
            m_bValid = false;
            if (szErr2K) strcpy(szErr2K, m_szErr2K);
            return -1;
        }
        ///���õ�ǰ��binlogΪֻ��
        m_pCurBinlog->setReadOnly();
        ///����ǰ��binlog���ŵ�m_arrBinlog������
        m_arrBinlog.push_back(m_pCurBinlog);
        pBinLogFile->m_nextBinLogFile = NULL;
        pBinLogFile->m_prevBinLogFile = m_pCurBinlog;
        m_pCurBinlog->m_nextBinLogFile = pBinLogFile;
        m_pCurBinlog = pBinLogFile;
    }
    ///����¼������ӵ���binlog�ļ���
    iRet = m_pCurBinlog->append(ullSid,
        ttTimestamp,
        uiGroup,
        uiType,
        uiAttr,
        szData,
        uiDataLen,
        m_szErr2K);
    if (-1 == iRet)
    {
        m_bValid = false;
        if (szErr2K) strcpy(szErr2K, m_szErr2K);
        return -1;
    }
    if (1 == iRet)
    {
        m_ullMaxSid = ullSid;
        m_ttMaxTimestamp = ttTimestamp;
        if ( 0 == m_ullMinSid)
        {
            m_ullMinSid = ullSid;
            m_ttMinTimestamp = ttTimestamp;
        }
        return 0;
    }
    ///���ļ��޷�����һ����¼
    CwxCommon::snprintf(m_szErr2K, 2047, "Binlog's length[%d] is too large, can't be put binlog file", uiDataLen);
    if (szErr2K) strcpy(szErr2K, m_szErr2K);
    m_bValid = false;
    return -1;
}


//-1��ʧ�ܣ�0���ɹ���
int CwxBinLogMgr::commit(char* szErr2K)
{
    int iRet = 0;
	if(!m_bValid)
	{
		if (szErr2K) strcpy(szErr2K, m_szErr2K);
		return -1;
	}
    if (m_pCurBinlog) iRet = m_pCurBinlog->commit(m_szErr2K);
    if (0 != iRet)
    {
        if (szErr2K) strcpy(szErr2K, m_szErr2K);
    }
    return iRet;
}


///���binlog������
void CwxBinLogMgr::clear()
{
    ///д������
    CwxWriteLockGuard<CwxRwLock> lock(&m_rwLock);
    _clear();
}

void CwxBinLogMgr::_clear()
{
	set<CwxBinLogCursor*>::iterator iter = m_cursorSet.begin();
	while(iter != m_cursorSet.end())
	{
		delete *iter;
		iter++;
	}
	m_cursorSet.clear();

    if (m_pCurBinlog) delete m_pCurBinlog;
    m_pCurBinlog = NULL;
    CWX_UINT32 i = 0;
    for (i=0; i<m_arrBinlog.size(); i++)
    {
        delete m_arrBinlog[i];
    }
    if (-1 != m_fdLock)
    {
        CwxFile::unlock(m_fdLock);
        ::close(m_fdLock);
        m_fdLock = -1;
    }
    m_arrBinlog.clear();
    m_uiMaxDay = DEF_MANAGE_MAX_DAY;
    m_ullMinSid = 0; ///<binlog�ļ�����Сsid
    m_ullMaxSid = 0; ///<binlog�ļ������sid
    m_ttMinTimestamp = 0; ///<binlog�ļ���log��ʼʱ��
    m_ttMaxTimestamp = 0; ///<binlog�ļ���log����ʱ��
    m_bValid = false;
    strcpy(m_szErr2K, "Not init.");
}


//NULL ʧ�ܣ����򷵻��α�����ָ�롣
CwxBinLogCursor* CwxBinLogMgr::createCurser()
{
	///��������
	CwxWriteLockGuard<CwxRwLock> lock(&m_rwLock);
    CwxBinLogCursor* pCursor =  new CwxBinLogCursor();
    pCursor->m_ullSid = 0;
    pCursor->m_ucSeekState = CURSOR_STATE_UNSEEK;
    pCursor->m_pBinLogFile = NULL;
	m_cursorSet.insert(pCursor);
    return pCursor;
}

/**
@brief ��ȡ��С��ullSid����Сbinlog header
@param [in] ullSid Ҫ���ҵ�sid��
@param [out] index ����������binlog index��
@return -1��ʧ�ܣ�0�������ڣ�1������
*/
int CwxBinLogMgr::upper(CWX_UINT64 ullSid, CwxBinLogIndex& index, char* szErr2K)
{
	///��������
	CwxReadLockGuard<CwxRwLock> lock(&m_rwLock);
	return _upper(ullSid, index, szErr2K);
}

///-1��ʧ�ܣ�0�������ڣ�1�����֡�
int CwxBinLogMgr::_upper(CWX_UINT64 ullSid, CwxBinLogIndex& index, char* szErr2K)
{
	CwxBinLogFile* pBinLogFile = NULL;
	int iRet = 0;

	if(!m_bValid)
	{
		if (szErr2K) strcpy(szErr2K, m_szErr2K);
		return -1;
	}
	//��λsid���ڵ�binlog�ļ�
	///��С���������ʷ����
	for (CWX_UINT32 i=0; i<m_arrBinlog.size(); i++)
	{
		if (ullSid < m_arrBinlog[i]->getMaxSid()) ///���С�����ֵ����һ������
		{
			pBinLogFile = m_arrBinlog[i];
			break;
		}
	}
	if (!pBinLogFile)
	{
		if (m_pCurBinlog && (ullSid < m_pCurBinlog->getMaxSid())) ///��ǰbinglog���ڶ���С�����ֵ
			pBinLogFile = m_pCurBinlog;
	}
	if (!pBinLogFile)
	{///�������ֵ
		return 0;
	}
	//��λcursor
	CWX_UINT32 pos=0;
	iRet = pBinLogFile->upper(ullSid, index, pos, szErr2K);
	return iRet;
}

/**
@brief ��ȡ������ullSid�����binlog header
@param [in] ullSid Ҫ���ҵ�sid��
@param [out] index ����������binlog index��
@return -1��ʧ�ܣ�0�������ڣ�1������
*/
int CwxBinLogMgr::lower(CWX_UINT64 ullSid, CwxBinLogIndex& index, char* szErr2K)
{
	///��������
	CwxReadLockGuard<CwxRwLock> lock(&m_rwLock);
	return _lower(ullSid, index, szErr2K);
}

///-1��ʧ�ܣ�0�������ڣ�1�����֡�
int CwxBinLogMgr::_lower(CWX_UINT64 ullSid, CwxBinLogIndex& index, char* szErr2K)
{
	CwxBinLogFile* pBinLogFile = NULL;
	int iRet = 0;
	if(!m_bValid)
	{
		if (szErr2K) strcpy(szErr2K, m_szErr2K);
		return -1;
	}

	//��λsid���ڵ�binlog�ļ����Ӵ�С����
	if (m_pCurBinlog && (ullSid>=m_pCurBinlog->getMinSid())) ///<�����С����Сֵ
	{///�ڵ�ǰbinlog�ļ���
		pBinLogFile = m_pCurBinlog;
	}
	else
	{///������ʷ����
		for (CWX_UINT32 i=m_arrBinlog.size(); i>0; i--)
		{
			if (ullSid >= m_arrBinlog[i-1]->getMinSid()) ///<�����С����Сֵ
			{
				pBinLogFile = m_arrBinlog[i-1];
				break;
			}
		}
	}
	if (!pBinLogFile)
	{///û�м�¼
		return 0;
	}

	//��λcursor
	CWX_UINT32 pos=0;
	iRet = pBinLogFile->lower(ullSid, index, pos, szErr2K);
	return iRet;
}

/// -1��ʧ�ܣ�0���޷���λ��ullSid��һ��binlog��1����λ��ullSid��һ����binlog�ϡ�
int CwxBinLogMgr::seek(CwxBinLogCursor* pCursor, CWX_UINT64 ullSid)
{
    ///��������
    CwxReadLockGuard<CwxRwLock> lock(&m_rwLock);
    return _seek(pCursor, ullSid);
}

/// -1��ʧ�ܣ�0���޷���λ��ullSid��һ��binlog��1����λ��ullSid��һ����binlog�ϡ�
int CwxBinLogMgr::_seek(CwxBinLogCursor* pCursor, CWX_UINT64 ullSid)
{
    CwxBinLogFile* pBinLogFile = NULL;
    int iRet = 0;
    pCursor->m_ullSid = ullSid;
    pCursor->m_pBinLogFile = NULL;
	if(!m_bValid)
	{
		strcpy(pCursor->m_szErr2K, m_szErr2K);
		return -1;
	}
    if (!m_pCurBinlog || 
         (ullSid >= m_pCurBinlog->getMaxSid()))
    {///�������ֵ
        pCursor->m_ucSeekState = CURSOR_STATE_UNSEEK;
        return 0;
    }

    //��λsid���ڵ�binlog�ļ�
    if (ullSid>=m_pCurBinlog->getMinSid())
    {///�ڵ�ǰbinlog�ļ���
        pBinLogFile = m_pCurBinlog;
    }
    else
    {///������ʷ����
        for (CWX_UINT32 i=0; i<m_arrBinlog.size(); i++)
        {
            if (ullSid < m_arrBinlog[i]->getMaxSid())
            {
                pBinLogFile = m_arrBinlog[i];
                break;
            }
        }
    }
    if (!pBinLogFile) pBinLogFile = m_pCurBinlog;

    //��λcursor
    iRet = pBinLogFile->seek(*pCursor, CwxBinLogFile::SEEK_SID);
    if (1 == iRet)
    {
        pCursor->m_ucSeekState = CURSOR_STATE_READY;
        pCursor->m_pBinLogFile = pBinLogFile;
        return 1;
    }

    pCursor->m_ucSeekState = CURSOR_STATE_ERROR;
   
    if ((-1 == iRet) || (-2 == iRet)) return -1; 

    //iRet == 0
    char szBuf1[64], szBuf2[64];
    CwxCommon::snprintf(pCursor->m_szErr2K, 2047, "Sid[%s] should be found, but not. binlog-file's max-sid[%s], binlog file:%s",
        CwxCommon::toString(ullSid, szBuf1),
        CwxCommon::toString(pBinLogFile->getMaxSid(), szBuf2),
        pBinLogFile->getDataFileName().c_str());
    return -1;
}


//-1��ʧ�ܣ�0���Ƶ����1���ɹ��Ƶ���һ��binlog��
int CwxBinLogMgr::next(CwxBinLogCursor* pCursor)
{
    ///��������
    CwxReadLockGuard<CwxRwLock> lock(&m_rwLock);
	if(!m_bValid)
	{
		strcpy(pCursor->m_szErr2K, m_szErr2K);
		return -1;
	}
    if (!isCursorValid(pCursor))
    {
        if (isUnseek(pCursor)) strcpy(pCursor->m_szErr2K, "Cursor is unseek.");
        return -1;
    }
    if (CURSOR_STATE_UNSEEK == pCursor->m_ucSeekState)
    {
        if (!m_pCurBinlog ||
            (pCursor->m_ullSid >= m_pCurBinlog->getMaxSid())) return 0;
        if (0 != _seek(pCursor, pCursor->m_ullSid)) return -1;
        CWX_ASSERT(CURSOR_STATE_READY == pCursor->m_ucSeekState);
    }
    int iRet = 0;
    if (_isOutRange(pCursor))
    {
        pCursor->m_pBinLogFile = _getMinBinLogFile();
        CWX_ASSERT(pCursor->m_pBinLogFile->getLogNum());
        iRet = pCursor->m_pBinLogFile->seek(*pCursor, CwxBinLogFile::SEEK_START);
        //iRet -2����������ɵļ�¼ͷ��-1��ʧ�ܣ�0�������ڣ�1����λ��ָ����λ��
        if (1 == iRet) return 1;
        pCursor->m_pBinLogFile = NULL;
        pCursor->m_ucSeekState = CURSOR_STATE_ERROR;
        if (0 == iRet)
        {
            CwxCommon::snprintf(pCursor->m_szErr2K, 2047, "Seek to binlog file's start, but return 0, LogNum=%d, file=%s",
                pCursor->m_pBinLogFile->getLogNum(),
                pCursor->m_pBinLogFile->getDataFileName().c_str());
        }
        return -1;
    }
    iRet = pCursor->next();//-2��log��header��������-1����ȡʧ�ܣ�0����ǰlogΪ���һ��log��1���Ƶ���һ��log
    if (1 == iRet) return 1;
    if ((-1==iRet) || (-2==iRet))
    {
        pCursor->m_pBinLogFile = NULL;
        pCursor->m_ucSeekState = CURSOR_STATE_ERROR;
        return -1;
    }
    //Now, iRet=0
    CWX_ASSERT(pCursor->getHeader().getSid() == pCursor->m_pBinLogFile->getMaxSid());
    if (pCursor->m_pBinLogFile->m_nextBinLogFile)
    {
        pCursor->m_pBinLogFile = pCursor->m_pBinLogFile->m_nextBinLogFile;
        CWX_ASSERT(pCursor->m_pBinLogFile->getLogNum());
        iRet = pCursor->m_pBinLogFile->seek(*pCursor, CwxBinLogFile::SEEK_START);
        //iRet -2����������ɵļ�¼ͷ��-1��ʧ�ܣ�0�������ڣ�1����λ��ָ����λ��
        if (1 == iRet) return 1;
        pCursor->m_pBinLogFile = NULL;
        pCursor->m_ucSeekState = CURSOR_STATE_ERROR;
        if (0 == iRet)
        {
            CwxCommon::snprintf(pCursor->m_szErr2K, 2047, "Seek to binlog file's start, but return 0, LogNum=%d, file=%s",
                pCursor->m_pBinLogFile->getLogNum(),
                pCursor->m_pBinLogFile->getDataFileName().c_str());
        }
        return -1;
    }
    //the end
    return 0;
}

// -1��ʧ�ܣ�0���Ƶ��ʼ��1���ɹ��Ƶ�ǰһ��binlog��
int CwxBinLogMgr::prev(CwxBinLogCursor* pCursor)
{
    ///��������
    CwxReadLockGuard<CwxRwLock> lock(&m_rwLock);
	if(!m_bValid)
	{
		strcpy(pCursor->m_szErr2K, m_szErr2K);
		return -1;
	}
    if (!isCursorValid(pCursor))
    {
        if (isUnseek(pCursor)) strcpy(pCursor->m_szErr2K, "Cursor is unseek.");
        return -1;
    }
    if (CURSOR_STATE_UNSEEK == pCursor->m_ucSeekState)
    {
        if (!m_pCurBinlog ||
            (pCursor->m_ullSid <= m_pCurBinlog->getMinSid())) return 0;
        if (pCursor->m_ullSid >= m_pCurBinlog->getMaxSid())
        {
            if (m_pCurBinlog->getMaxSid())
            {
                pCursor->m_ullSid = m_pCurBinlog->getMaxSid() - 1;
                if (0 != _seek(pCursor, pCursor->m_ullSid)) return -1;
            }
            else
            {
                return 0;
            }
        }
        CWX_ASSERT(CURSOR_STATE_READY == pCursor->m_ucSeekState);
    }

    int iRet = 0;
    if (_isOutRange(pCursor))
    {
        pCursor->m_pBinLogFile = _getMinBinLogFile();
        CWX_ASSERT(pCursor->m_pBinLogFile->getLogNum());
        iRet = pCursor->m_pBinLogFile->seek(*pCursor, CwxBinLogFile::SEEK_START);
        //iRet -2����������ɵļ�¼ͷ��-1��ʧ�ܣ�0�������ڣ�1����λ��ָ����λ��
        if (1 == iRet) return 0;
        pCursor->m_pBinLogFile = NULL;
        pCursor->m_ucSeekState = CURSOR_STATE_ERROR;
        if (0 == iRet)
        {
            CwxCommon::snprintf(pCursor->m_szErr2K, 2047, "Seek to binlog file's start, but return 0, LogNum=%d, file=%s",
                pCursor->m_pBinLogFile->getLogNum(),
                pCursor->m_pBinLogFile->getDataFileName().c_str());
        }
        return -1;
    }

    iRet = pCursor->prev();//-2��log��header��������-1����ȡʧ�ܣ�0����ǰlogΪ���һ��log��1���Ƶ���һ��log

    if (1 == iRet) return 1;
    if ((-1==iRet) || (-2==iRet))
    {
        pCursor->m_pBinLogFile = NULL;
        pCursor->m_ucSeekState = CURSOR_STATE_ERROR;
        return -1;
    }
    //Now, iRet=0
    CWX_ASSERT(pCursor->getHeader().getSid() == pCursor->m_pBinLogFile->getMinSid());
    if (pCursor->m_pBinLogFile->m_prevBinLogFile)
    {
        pCursor->m_pBinLogFile = pCursor->m_pBinLogFile->m_prevBinLogFile;
        CWX_ASSERT(pCursor->m_pBinLogFile->getLogNum());
        iRet = pCursor->m_pBinLogFile->seek(*pCursor, CwxBinLogFile::SEEK_TAIL);
        //iRet -2����������ɵļ�¼ͷ��-1��ʧ�ܣ�0�������ڣ�1����λ��ָ����λ��
        if (1 == iRet) return 1;
        pCursor->m_pBinLogFile = NULL;
        pCursor->m_ucSeekState = CURSOR_STATE_ERROR;
        if (0 == iRet)
        {
            CwxCommon::snprintf(pCursor->m_szErr2K, 2047, "Seek to binlog file's end, but return 0, LogNum=%d, file=%s",
                pCursor->m_pBinLogFile->getLogNum(),
                pCursor->m_pBinLogFile->getDataFileName().c_str());
        }
        return -1;
    }
    //the end
    return 0;
}

//-1��ʧ�ܣ�0���ɹ���ȡ��һ��binlog��
int CwxBinLogMgr::fetch(CwxBinLogCursor* pCursor, char* szData, CWX_UINT32& uiDataLen)
{
	if(!m_bValid)
	{
		strcpy(pCursor->m_szErr2K, m_szErr2K);
		return -1;
	}
    if (!isCursorValid(pCursor))
    {
        if (isUnseek(pCursor)) strcpy(pCursor->m_szErr2K, "Cursor is unseek.");
        return -1;
    }
    if (CURSOR_STATE_UNSEEK == pCursor->m_ucSeekState)
    {
        strcpy(pCursor->m_szErr2K, "the cursor doesn't seek.");
        return -1;
    }
    int iRet = pCursor->data(szData, uiDataLen);
    //iRet: -2�����ݲ���ɣ�-1��ʧ�ܣ�>=0����ȡ���ݵĳ���
    if (iRet >= 0) return 0;
    pCursor->m_pBinLogFile = NULL;
    pCursor->m_ucSeekState = CURSOR_STATE_ERROR;
    return -1;
}

// -1��ʧ�ܣ�0���Ƶ����1���ɹ���ȡ��һ��binlog��
int CwxBinLogMgr::next(CwxBinLogCursor* pCursor, char* szData, CWX_UINT32& uiDataLen)
{
    int iRet = next(pCursor);
    //iRet -1��ʧ�ܣ�0���Ƶ����1���ɹ��Ƶ���һ��binlog��
    if (1 == iRet)
    {
        iRet = fetch(pCursor, szData, uiDataLen);
        //iRet -1��ʧ�ܣ�0���ɹ���ȡ��һ��binlog��
        if (0 == iRet) return 1;
        return -1;
    }
    if (0 == iRet) return 0;
    return -1;
}

// -1��ʧ�ܣ�0���Ƶ��ʼ��1���ɹ���ȡǰһ��binlog��
int CwxBinLogMgr::prev(CwxBinLogCursor* pCursor, char* szData, CWX_UINT32& uiDataLen)
{
    int iRet = prev(pCursor);
    //iRet -1��ʧ�ܣ�0���Ƶ���ʼ��1���ɹ��Ƶ���һ��binlog��
    if (1 == iRet)
    {
        iRet = fetch(pCursor, szData, uiDataLen);
        //iRet -1��ʧ�ܣ�0���ɹ���ȡ��һ��binlog��
        if (0 == iRet) return 1;
        return -1;
    }
    if (0 == iRet) return 0;
    return -1;
}

//-1��ʧ�ܣ�0���ɹ���
int CwxBinLogMgr::destoryCurser(CwxBinLogCursor*& pCursor)
{
	CwxWriteLockGuard<CwxRwLock> lock(&m_rwLock);
	set<CwxBinLogCursor*>::iterator iter =  m_cursorSet.find(pCursor);
	if (iter != m_cursorSet.end())
	{
		m_cursorSet.erase(iter);
		delete pCursor;
		pCursor = NULL;
		return 0;
	}
	return -1;
}

CWX_INT64 CwxBinLogMgr::leftLogNum(CwxBinLogCursor const* pCursor)
{
    ///��������
    CwxReadLockGuard<CwxRwLock> lock(&m_rwLock);
    if (!pCursor) return -1;
    if (CURSOR_STATE_READY != pCursor->m_ucSeekState) return -1;
    CwxBinLogFile* pFile = pCursor->m_pBinLogFile;
    CWX_INT64 num = pFile->getLogNum() - pCursor->m_curLogHeader.getLogNo();
    pFile = pFile->m_nextBinLogFile;
    while(pFile)
    {
        num += pFile->getLogNum();
        pFile = pFile->m_nextBinLogFile;
    }
    return num;
}

CWX_UINT64 CwxBinLogMgr::getFileStartSid(CWX_UINT32 ttTimestamp)
{
    CwxReadLockGuard<CwxRwLock> lock(&m_rwLock);
    for (CWX_UINT32 i=0; i<m_arrBinlog.size(); i++)
    {
        if (m_arrBinlog[i]->getMaxTimestamp() > ttTimestamp)
            return m_arrBinlog[i]->getMinSid();
    }
    if (m_pCurBinlog) return m_pCurBinlog->getMinSid();
    return 0;
}

//void
void CwxBinLogMgr::_outputManageBinLog()
{
    char szBuf[2048];
    char szBuf1[64];
    char szBuf2[64];
    string strTimeStamp1;
    string strTimeStamp2;
    string strFileName = m_strPrexLogPath + m_strFilePrex + "_mgr.inf";
    FILE* fd = fopen(strFileName.c_str(), "wb");
    if (fd)
    {
        CwxCommon::snprintf(szBuf, 2047, "FileNo\t\tMinSid\t\tMaxSid\t\tMinTimestamp\t\tMaxTimestamp\t\tLogNo\t\tFile\n");
        fwrite(szBuf, 1, strlen(szBuf), fd);
        if (m_pCurBinlog)
        {
            CwxCommon::snprintf(szBuf,
                2047,
                "%d\t\t%s\t\t%s\t\t%s\t\t%s\t\t%u\t\t%s\n",
                m_pCurBinlog->getFileNo(),
                CwxCommon::toString(m_pCurBinlog->getMinSid(), szBuf1),
                CwxCommon::toString(m_pCurBinlog->getMaxSid(), szBuf2),
                CwxDate::getDate(m_pCurBinlog->getMinTimestamp(), strTimeStamp1).c_str(),
                CwxDate::getDate(m_pCurBinlog->getMaxTimestamp(), strTimeStamp2).c_str(),
                m_pCurBinlog->getLogNum(),
                m_pCurBinlog->getDataFileName().c_str()
                );
            fwrite(szBuf, 1, strlen(szBuf), fd);
        }
        for (CWX_UINT32 i=m_arrBinlog.size(); i>0; i--)
        {
            CwxCommon::snprintf(szBuf,
                2047,
                "%d\t\t%s\t\t%s\t\t%s\t\t%s\t\t%u\t\t%s\n",
                m_arrBinlog[i-1]->getFileNo(),
                CwxCommon::toString(m_arrBinlog[i-1]->getMinSid(), szBuf1),
                CwxCommon::toString(m_arrBinlog[i-1]->getMaxSid(), szBuf2),
                CwxDate::getDate(m_arrBinlog[i-1]->getMinTimestamp(), strTimeStamp1).c_str(),
                CwxDate::getDate(m_arrBinlog[i-1]->getMaxTimestamp(), strTimeStamp2).c_str(),
                m_arrBinlog[i-1]->getLogNum(),
                m_arrBinlog[i-1]->getDataFileName().c_str()
                );
            fwrite(szBuf, 1, strlen(szBuf), fd);
        }
    }
    fclose(fd);
}




CWINUX_END_NAMESPACE
