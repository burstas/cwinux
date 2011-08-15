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
	m_uiBlockNo = 0;
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
	m_uiBlockNo = 0;
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
int CwxBinLogCursor::header(CWX_UINT32 uiOffset)
{
    int iRet;
    iRet = pread(this->m_fd, m_szHeadBuf, CwxBinLogHeader::BIN_LOG_HEADER_SIZE, uiOffset);
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
    if (uiOffset != m_curLogHeader.getOffset())
    {
        CwxCommon::snprintf(this->m_szErr2K, 2047, "Invalid binlog, offset of header[%u] is different with it's file-offset[%u].",
            uiOffset, m_curLogHeader.getOffset());
        return -1;
    }
    return 1;
}

inline bool CwxBinLogCursor::preadPage(int fildes, CWX_UINT32 uiBlockNo, CWX_UINT32 uiOffset)
{
	CWX_ASSERT(uiOffset<=BINLOG_READ_BLOCK_SIZE);
	ssize_t ret = 0;
	if (uiBlockNo != m_uiBlockNo)
	{
		m_uiBlockNo = uiBlockNo;
		m_uiBlockDataOffset = 0;
	}
	if ((uiBlockNo == m_uiBlockNo)&&(uiOffset <= m_uiBlockDataOffset)) return true;
	do 
	{
		ret = ::pread(fildes, m_szReadBlock + m_uiBlockDataOffset, BINLOG_READ_BLOCK_SIZE - m_uiBlockDataOffset, (uiBlockNo<<BINLOG_READ_BLOCK_BIT) + m_uiBlockDataOffset);
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
ssize_t CwxBinLogCursor::pread(int fildes, void *buf, size_t nbyte, CWX_UINT32 offset)
{
	size_t pos = 0;
	if (nbyte)
	{
		CWX_UINT32 uiStartBlock = offset>>BINLOG_READ_BLOCK_BIT;
		CWX_UINT32 uiEndBlock = (offset + nbyte - 1)>>BINLOG_READ_BLOCK_BIT;
		CWX_UINT32 uiBlockNum = uiEndBlock - uiStartBlock + 1;
		CWX_UINT32 uiBlockStartOffset = 0;
		CWX_UINT32 uiBlockEndOffset = 0;

		//get first block
		uiBlockStartOffset = offset - (uiStartBlock << BINLOG_READ_BLOCK_BIT);
		uiBlockEndOffset = uiBlockNum==1?uiBlockStartOffset + nbyte:(CWX_UINT32)BINLOG_READ_BLOCK_SIZE;
		if (!preadPage(fildes, uiStartBlock, uiBlockEndOffset)) return -1;
		if (uiBlockEndOffset <= m_uiBlockDataOffset)
		{//�����㹻
			memcpy(buf, m_szReadBlock + uiBlockStartOffset, uiBlockEndOffset - uiBlockStartOffset);
			pos = uiBlockEndOffset - uiBlockStartOffset;
			if (uiStartBlock == uiEndBlock) return pos;
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
		if (uiBlockNum > 2)
		{//read middle
			ssize_t ret = 0;
			do 
			{
				ret = ::pread(fildes, (char*)buf+pos, (uiBlockNum - 2)<<BINLOG_READ_BLOCK_BIT, (uiStartBlock + 1)<<BINLOG_READ_BLOCK_BIT);
				if (-1 != ret)
				{
					pos += ret;
					if ((CWX_UINT32)ret != ((uiBlockNum - 2)<<BINLOG_READ_BLOCK_BIT)) //finish
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
			if (!preadPage(fildes, uiEndBlock, uiBlockEndOffset)) return -1;
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
                    CwxBinLogWriteCache  class
***********************************************************************/
CwxBinLogWriteCache::CwxBinLogWriteCache(int indexFd,
					int dataFd,
					CWX_UINT32 uiIndexOffset,
					CWX_UINT32 uiDataOffset,
					CWX_UINT64 ullSid,
					bool bCacheData)
{
	m_indexFd = indexFd;
	m_dataFd = dataFd;
	m_bCacheData = bCacheData;
	m_ullPrevIndexSid = ullSid;
	m_ullMinIndexSid = 0;
	m_uiIndexFileOffset = uiIndexOffset;
	m_indexBuf = new unsigned char[BINLOG_WRITE_INDEX_CACHE_RECORD_NUM * CwxBinLogIndex::BIN_LOG_INDEX_SIZE];
	m_uiIndexLen = 0;
	m_ullPrevDataSid = ullSid;
	m_ullMinDataSid = 0;
	m_uiDataFileOffset = uiDataOffset;
	m_dataBuf = new unsigned char[BINLOG_WRITE_DATA_CACHE_SIZE];
	m_uiDataLen = 0;
	m_ullMaxSid = ullSid;
}

CwxBinLogWriteCache::~CwxBinLogWriteCache()
{
	if (m_indexBuf) delete [] m_indexBuf;
	if (m_dataBuf) delete [] m_dataBuf;
}

///1:�ɹ���0��szData̫�󣬳���buf�����ߴ磻-1��д����ʧ�ܣ�-2��д����ʧ�ܡ�
int CwxBinLogWriteCache::append(CwxBinLogHeader const& header, char const* szData, char* szErr2K)
{
	unsigned char* pos = NULL;
	//д��data��cache
	{
		CWX_UINT32 uiLen = CwxBinLogHeader::BIN_LOG_HEADER_SIZE + header.getLogLen();
		if (uiLen > BINLOG_WRITE_DATA_CACHE_SIZE)
		{
			if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Data is too len, max is:%u, current is %u", BINLOG_WRITE_DATA_CACHE_SIZE - CwxBinLogHeader::BIN_LOG_HEADER_SIZE, header.getLogLen());
			return 0;
		}
		if (uiLen + m_uiDataLen > BINLOG_WRITE_DATA_CACHE_SIZE)
		{
			if (0 != flushData(szErr2K)) return -1;
		}
		pos = m_dataBuf + m_uiDataLen;
		//copy header
		header.serialize((char*)m_dataBuf + m_uiDataLen);
		m_uiDataLen += CwxBinLogHeader::BIN_LOG_HEADER_SIZE;
		memcpy(m_dataBuf + m_uiDataLen, szData, header.getLogLen());
		m_uiDataLen += header.getLogLen();
		if (!m_bCacheData)
		{
			if (0 != flushData(szErr2K)) return -1;
		}
		else
		{
			if (0 == m_ullMinDataSid) m_ullMinDataSid = header.getSid();
			m_dataSidMap[header.getSid()] = pos;
		}
	}
	//д��index��cache
	CwxBinLogIndex index(header);
	if (m_uiIndexLen + CwxBinLogIndex::BIN_LOG_INDEX_SIZE > CwxBinLogIndex::BIN_LOG_INDEX_SIZE * BINLOG_WRITE_INDEX_CACHE_RECORD_NUM)
	{
		if (0 != flushIndex(szErr2K)) return -1;
	}
	pos = m_indexBuf + m_uiIndexLen;
	index.serialize((char*)pos);
	m_uiIndexLen += CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
	if (0 == m_ullMinIndexSid) m_ullMinIndexSid =  header.getSid();
	m_indexSidMap[header.getSid()] = pos;
	m_ullMaxSid = header.getSid();
	return 1;
}


/***********************************************************************
                    CwxBinLogFile  class
***********************************************************************/
CwxBinLogFile::CwxBinLogFile(CWX_UINT32 ttDay, CWX_UINT32 uiFileNo, CWX_UINT32 uiMaxFileSize)
{
    m_bValid = false;
    if (uiMaxFileSize<MIN_BINLOG_FILE_SIZE)
    {
        m_uiMaxFileSize = MIN_BINLOG_FILE_SIZE;
    }
    else
    {
        m_uiMaxFileSize = uiMaxFileSize;
    }
    m_ullMinSid = 0;
    m_ullMaxSid = 0;
    m_ttMinTimestamp = 0;
    m_ttMaxTimestamp = 0;
    m_uiLogNum = 0;
    m_bReadOnly = true;
    m_fd = -1;
    m_indexFd = -1;
    m_uiFileSize = 0;
    m_uiIndexFileSize = 0;
    m_uiPrevLogOffset = 0;
    m_ttDay = ttDay;
    m_uiFileNo = uiFileNo;
	m_writeCache = NULL;
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
						bool bCacheData,
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
        m_uiFileSize = CwxFile::getFileSize(m_strPathFileName.c_str());
        if (-1 == (CWX_INT32)m_uiFileSize)
        {
            if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to get binlog file's size, file:%s, errno=%d", m_strPathFileName.c_str(), errno);
            return -1;
        }
    }
    else
    {
        m_uiFileSize = -1;
    }
    //��ȡbinlog�����ļ��Ĵ�С
    if (CwxFile::isFile(m_strIndexFileName.c_str()))
    {
        m_uiIndexFileSize = CwxFile::getFileSize(m_strIndexFileName.c_str());
        if (-1 == (CWX_INT32)m_uiIndexFileSize)
        {
            if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to get binlog index file's size, file:%s, errno=%d", m_strIndexFileName.c_str(), errno);
            return -1;
        }
    }
    else
    {
        m_uiIndexFileSize = -1;
    }

    //����binlog�ļ����������ļ�
    if (bCreate)
    {
        if (-1 == mkBinlog(szErr2K)) return -1;
    }
    else
    {
        if (-1 == (CWX_INT32)m_uiFileSize)
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
    if (-1 != (CWX_INT32)m_uiIndexFileSize)
    {//�����ļ�����
        m_indexFd = ::open(m_strIndexFileName.c_str(),  O_RDWR);
    }
    else
    {//�����ļ�������
        m_indexFd = ::open(m_strIndexFileName.c_str(),  O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
        //����index�ļ��Ĵ�СΪ0.
        m_uiIndexFileSize = 0;
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
		m_writeCache = NULL;
    }
	else
	{
		m_writeCache = new CwxBinLogWriteCache(m_indexFd, m_fd, m_uiIndexFileSize, m_uiFileSize, getMaxSid(), bCacheData);
	}
    m_bValid = true;
    return 0;
}

//-1��ʧ�ܣ�0����־�ļ����ˣ�1���ɹ���
int CwxBinLogFile::append(CWX_UINT64 ullSid,
                          CWX_UINT32 ttTimestamp,
                          CWX_UINT32 uiGroup,
                          CWX_UINT32 uiType,
                          char const* szData,
                          CWX_UINT32 uiDataLen,
                          char* szErr2K)
{
//    char szBuf[CwxBinLogHeader::BIN_LOG_HEADER_SIZE];
    if (m_uiFileSize + FREE_BINLOG_FILE_SIZE + CwxBinLogHeader::BIN_LOG_HEADER_SIZE + uiDataLen >= m_uiMaxFileSize) return 0; //it's full.

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
		m_uiLogNum,
        (CWX_UINT32)ttTimestamp,
        m_uiFileSize,
        uiDataLen,
        m_uiPrevLogOffset,
        uiGroup,
        uiType);
	int ret = m_writeCache->append(header, szData, szErr2K);
	if (0 == ret) return -1;
	if (0 > ret)
	{
		m_bValid = false;
		return -1;		
	}
    ///����ǰһ��binlog���ļ�offset
    m_uiPrevLogOffset = m_uiFileSize;
    ///�޸��ļ��Ĵ�С
    m_uiFileSize += CwxBinLogHeader::BIN_LOG_HEADER_SIZE + uiDataLen;
    ///�޸������ļ��Ĵ�С
    m_uiIndexFileSize += CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
    //����sid��ʱ���
    m_ullMaxSid = ullSid;
    m_ttMaxTimestamp = ttTimestamp; ///ʱ��Ӧ��������ģ�����������˵��ʱ�����˵���������������ֵ�����
    if (!m_uiLogNum)
    {
        m_ullMinSid = ullSid;
        m_ttMinTimestamp = ttTimestamp;
    }
	if (m_ttMinTimestamp > ttTimestamp) ///����ǰʱ��С����Сʱ�䣬���޸���Сʱ��
	{
		m_ttMinTimestamp = ttTimestamp;
	}
    //��¼����1
    m_uiLogNum ++;
    return 1;
}

int CwxBinLogFile::flush_cache(bool bFlushAll, char* szErr2K)
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
	//flush data 
	if (0 != m_writeCache->flushData(szErr2K))
	{
		m_bValid = false;
		return -1;
	}
	if (bFlushAll)
	{
		if (0 != m_writeCache->flushIndex(szErr2K))
		{
			m_bValid = false;
			return -1;
		}
	}
	return 0;
}

int CwxBinLogFile::fsync_data(bool bFlushAll, char*)
{
	::fsync(m_fd);
	if (bFlushAll)
	{
		::fsync(m_indexFd);
	}
    return 0;
}

int CwxBinLogFile::upper(CWX_UINT64 ullSid, CwxBinLogIndex& item, char* szErr2K)
{
	CWX_ASSERT(m_bValid);
	if (!m_bValid)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "CwxBinlogFile is invalid, file:%s", m_strPathFileName.c_str());
		return -1;
	}
	if (!m_uiLogNum) return 0;

	if (ullSid >= m_ullMaxSid)
	{
		return 0;///������
	}
	if (m_writeCache && m_writeCache->m_indexSidMap.size())
	{
		if (m_writeCache->m_ullPrevIndexSid <= ullSid)
		{//��ullSid���sid��һ����write cache�С�
			map<CWX_UINT64/*sid*/, unsigned char*>::const_iterator iter = m_writeCache->m_indexSidMap.upper_bound(ullSid);
			CWX_ASSERT((iter != m_writeCache->m_indexSidMap.end()));
			item.unserialize((char const*)iter->second);
			return 1;
		}
	}
	//һ�����ļ���
	//����ָ����SID��λ
	int fd = -1;
	CWX_UINT32 uiOffset = 0;
	//�۰����
	fd = ::open(m_strIndexFileName.c_str(), O_RDONLY);
	if (-1 == fd)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to open index file:%s, errno=%d", m_strIndexFileName.c_str(), errno);
		return -1;
	}
	if (ullSid < m_ullMinSid)
	{
		if (0 != readIndex(fd, item, 0, szErr2K))
		{
			::close(fd);
			return -1;
		}
		::close(fd);
		return 1;
	}

	CWX_UINT32 uiStart = 0;
	CWX_UINT32 uiEnd = m_uiLogNum - 1 - (m_writeCache?m_writeCache->m_indexSidMap.size():0);
	CWX_UINT32 uiMid = 0;
	while(uiEnd >= uiStart)
	{
		uiMid = (uiStart + uiEnd)/2;
		uiOffset = uiMid;
		uiOffset *= CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
		if (0 != readIndex(fd, item, uiOffset, szErr2K))
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
	if (ullSid >= item.getSid())
	{//next item
		uiOffset += CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
		if (0 != readIndex(fd, item, uiOffset, szErr2K))
		{
			::close(fd);
			return -1;
		}
	}
	::close(fd);
	return 1;
}

// -1��ʧ�ܣ�0�������ڣ�1������
int CwxBinLogFile::lower(CWX_UINT64 ullSid, CwxBinLogIndex& item, char* szErr2K)
{
	CWX_ASSERT(m_bValid);
	if (!m_bValid)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "CwxBinlogFile is invalid, file:%s", m_strPathFileName.c_str());
		return -1;
	}
	if (!m_uiLogNum) return 0;
	if (ullSid < m_ullMinSid)
	{
		return 0;///������
	}

	if (m_writeCache && m_writeCache->m_indexSidMap.size())
	{
		if (m_writeCache->m_ullMinIndexSid <= ullSid)
		{//������ullSid���sid��һ����write cache�С�
			map<CWX_UINT64/*sid*/, unsigned char*>::const_iterator iter = m_writeCache->m_indexSidMap.lower_bound(ullSid);
			CWX_ASSERT((iter != m_writeCache->m_indexSidMap.end()));
			item.unserialize((char const*)iter->second);
			return 1;
		}
	}

	//һ�����ļ���
	//����ָ����SID��λ
	int fd = -1;
	CWX_UINT32 uiOffset = 0;
	//�۰����
	fd = ::open(m_strIndexFileName.c_str(), O_RDONLY);
	if (-1 == fd)
	{
		if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to open index file:%s, errno=%d", m_strIndexFileName.c_str(), errno);
		return -1;
	}
	if (ullSid >= m_ullMaxSid)
	{
		//��ȡ���һ��
		if (0 != readIndex(fd, item, (m_uiLogNum - 1) * CwxBinLogIndex::BIN_LOG_INDEX_SIZE, szErr2K))
		{
			::close(fd);
			return -1;
		}
		::close(fd);
		return 1;
	}

	CWX_UINT32 uiStart = 0;
	CWX_UINT32 uiEnd = m_uiLogNum - 1 - (m_writeCache?m_writeCache->m_indexSidMap.size():0);
	CWX_UINT32 uiMid = 0;
	while(uiEnd >= uiStart)
	{
		uiMid = (uiStart + uiEnd)/2;
		uiOffset = uiMid;
		uiOffset *= CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
		if (0 != readIndex(fd, item, uiOffset, szErr2K))
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
	if (ullSid < item.getSid())
	{//next item
		uiOffset -= CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
		if (0 != readIndex(fd, item, uiOffset, szErr2K))
		{
			::close(fd);
			return -1;
		}
	}
	::close(fd);
	return 1;
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

	if (SEEK_START == ucMode)
	{
		if (!m_writeCache || m_writeCache->m_uiDataFileOffset/*�����ļ�������*/)
		{
			return cursor.seek(0);
		}
		else
		{
			if (m_writeCache->m_dataSidMap.size())
			{//�����ݣ�ȡ��һ��
				map<CWX_UINT64/*sid*/, unsigned char*>::const_iterator iter = m_writeCache->m_dataSidMap.begin();
				return cursor.seek((char const*)iter->second, getMinSid(), false);
			}
			//û�����ݣ�Ϊdangling״̬
			return 0;
		}
	}
	if (SEEK_TAIL == ucMode)
	{
		if (!m_writeCache || !m_writeCache->m_uiDataLen/*û��cache����*/)
		{
			return cursor.seek(m_uiPrevLogOffset);
		}
		else
		{//ȡ���һ����¼
			map<CWX_UINT64/*sid*/, unsigned char*>::const_reverse_iterator iter = m_writeCache->m_dataSidMap.rbegin();
			return cursor.seek((char const*)iter->second, getMaxSid(), false);
		}
	}

	//����ָ����SID��λ
	if (cursor.m_ullSid >= getMaxSid()) return 0; ///������
	if (!m_writeCache || (m_writeCache->m_ullPrevDataSid > cursor.m_ullSid) /*�ļ��д��ڱȶ�λsid�����Ϣ*/)
	{
		CwxBinLogIndex item;
		iRet = upper(cursor.m_ullSid, item, cursor.m_szErr2K);
		if (1 != iRet) return iRet;
		return cursor.seek(item.getOffset());
	}
	//��ǰ��sidһ����m_writeCache�д���
	map<CWX_UINT64/*sid*/, unsigned char*>::const_iterator iter = m_writeCache->m_dataSidMap.upper_bound(cursor.m_ullSid);
	CWX_ASSERT(iter != m_writeCache->m_dataSidMap.end());
	return cursor.seek((char const*)iter->second, iter->first, false);
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
    m_uiFileSize = 0;
    m_uiIndexFileSize = 0;
    m_uiPrevLogOffset = 0;
    m_prevBinLogFile = NULL;
    m_nextBinLogFile = NULL;
	if (m_writeCache) delete m_writeCache;
	m_writeCache = NULL;
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
	if (!m_bReadOnly)
	{
		flush_cache(true, NULL);
		fsync_data(true, NULL);
	}
    if (-1 != m_fd) ::close(m_fd);
    m_fd = -1;
    if (-1 != m_indexFd) ::close(m_indexFd);
    m_indexFd = -1;
    reset();
}


int CwxBinLogFile::mkBinlog(char* szErr2K)
{
    //pretect the sync-log file
    if ((-1 != (CWX_INT32)m_uiFileSize) && (m_uiFileSize > CwxBinLogHeader::BIN_LOG_HEADER_SIZE * 2))
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
    m_uiFileSize = 0;
    ::close(fd);

    //������ļ����ݵķ�ʽ���ļ�
    fd = ::open(m_strIndexFileName.c_str(),  O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if (-1 == fd)
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to create binlog's index file [%s] .", m_strIndexFileName.c_str());
        return -1;
    }
    //���������ļ���СΪ0
    m_uiIndexFileSize = 0; 
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
    CWX_ASSERT(!(m_uiIndexFileSize%CwxBinLogIndex::BIN_LOG_INDEX_SIZE));
    m_uiLogNum = m_uiIndexFileSize /CwxBinLogIndex::BIN_LOG_INDEX_SIZE;

    //�������һ����¼�Ŀ�ʼλ��
    m_uiPrevLogOffset = 0;

    if (m_uiLogNum)
    {//��¼����
        //��ȡ��С��sid��timestamp
        CwxBinLogIndex index;
        if (0 != readIndex(m_indexFd, index, 0, szErr2K)) return -1;
        m_ullMinSid = index.getSid();
        m_ttMinTimestamp = index.getDatetime();
        //��ȡ����sid, timestamp
        if (0 != readIndex(m_indexFd, index, m_uiIndexFileSize - CwxBinLogIndex::BIN_LOG_INDEX_SIZE)) return -1;
        m_ullMaxSid = index.getSid();
        m_ttMaxTimestamp = index.getDatetime();
        m_uiPrevLogOffset = index.getOffset();
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
    if (m_uiIndexFileSize%CwxBinLogIndex::BIN_LOG_INDEX_SIZE) return 1;

    //����Ϊ�գ������ݲ�Ϊ��
    if (!m_uiIndexFileSize) return 1;

    //���������¼��binlog�ļ���С��binlog�ļ�����Ĵ�С��ϵ
    char szBuf[CwxBinLogIndex::BIN_LOG_INDEX_SIZE];
    CwxBinLogIndex index;
    //��ȡ����sid, timestamp
    if (CwxBinLogIndex::BIN_LOG_INDEX_SIZE != pread(m_indexFd,
        &szBuf,
        CwxBinLogIndex::BIN_LOG_INDEX_SIZE,
        m_uiIndexFileSize - CwxBinLogIndex::BIN_LOG_INDEX_SIZE))
    {
        if (szErr2K) CwxCommon::snprintf(szErr2K, 2047, "Failure to read binlog index, file:%s, errno=%d", this->m_strIndexFileName.c_str(), errno);
        return -1;
    }
    
    index.unserialize(szBuf);
    //�������Ҫ�ؽ�
    if (index.getOffset() + index.getLogLen() + CwxBinLogHeader::BIN_LOG_HEADER_SIZE != m_uiFileSize) return 1;
    //����Ҫ�ؽ�
    return 0;
}

//-1��ʧ�ܣ�0���ɹ���
int CwxBinLogFile::createIndex(char* szErr2K)
{
    CwxBinLogIndex index;
    CwxBinLogCursor* cursor = new CwxBinLogCursor();
    int iRet = cursor->open(m_strPathFileName.c_str());
    m_uiIndexFileSize = 0;
    if (-1 == iRet)
    {
        if (szErr2K) strcpy(szErr2K, cursor->getErrMsg());
		delete cursor;
        return -1;
    }

    while(1 == (iRet = cursor->next()))
    {
        index = cursor->getHeader();
        if (0 != writeIndex(m_indexFd, index, m_uiIndexFileSize, szErr2K)) return -1;
        m_uiIndexFileSize += CwxBinLogIndex::BIN_LOG_INDEX_SIZE;
    }
    if (-1 == iRet)
    {
        if (szErr2K) strcpy(szErr2K, cursor->getErrMsg());
		delete cursor;
        return -1;
    }
    if (m_uiIndexFileSize)
    {//������Ч��binlog
        m_uiFileSize = index.getOffset() + index.getLogLen() + CwxBinLogHeader::BIN_LOG_HEADER_SIZE;
    }
    else
    {//��������Ч��binlog
        m_uiFileSize = 0;
    }
    //truncate binlog �ļ�
	CWX_INFO(("Truncate file %s to size %u", m_strPathFileName.c_str(), m_uiFileSize));
    ftruncate(m_fd, m_uiFileSize);
    //truncate index �ļ�
	CWX_INFO(("Truncate file %s to size %u", m_strIndexFileName.c_str(), m_uiIndexFileSize));
    ftruncate(m_indexFd, m_uiIndexFileSize);
	delete cursor;
    return 0;
}


/***********************************************************************
                    CwxBinLogMgr  class
***********************************************************************/

CwxBinLogMgr::CwxBinLogMgr(char const* szLogPath, char const* szFilePrex, CWX_UINT32 uiMaxFileSize, bool bDelOutManageLogFile)
{
    m_bValid = false;
    strcpy(m_szErr2K, "Not init.");
    m_pCurBinlog = NULL;
    m_strLogPath = szLogPath;
    if ('/' !=m_strLogPath[m_strLogPath.length() - 1]) m_strLogPath += "/";
    m_strPrexLogPath = m_strLogPath + szFilePrex + "/";
    m_strFilePrex = szFilePrex;
    m_uiMaxFileSize = uiMaxFileSize;
	if (m_uiMaxFileSize > MAX_BINLOG_FILE_SIZE) m_uiMaxFileSize = MAX_BINLOG_FILE_SIZE;
    m_uiMaxFileNum = DEF_MANAGE_FILE_NUM;
	m_bCache = true;
    m_bDelOutManageLogFile = bDelOutManageLogFile;
    m_fdLock = -1;
    m_ullMinSid = 0; ///<binlog�ļ�����Сsid
    m_ullMaxSid = 0; ///<binlog�ļ������sid
    m_ttMinTimestamp = 0; ///<binlog�ļ���log��ʼʱ��
    m_ttMaxTimestamp = 0; ///<binlog�ļ���log����ʱ��
}

CwxBinLogMgr::~CwxBinLogMgr()
{
    if (m_pCurBinlog)
	{
		m_pCurBinlog->flush_cache(true, NULL);
		m_pCurBinlog->fsync_data(true, NULL);
		delete m_pCurBinlog;
	}
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
int CwxBinLogMgr::init(CWX_UINT32 uiMaxFileNum, bool bCache, char* szErr2K)
{
    ///д������
    CwxWriteLockGuard<CwxRwLock> lock(&m_rwLock);
    this->_clear();
    m_bValid = false;
    strcpy(m_szErr2K, "Not init.");
    if (uiMaxFileNum < MIN_MANAGE_FILE_NUM) m_uiMaxFileNum = MIN_MANAGE_FILE_NUM;
    if (uiMaxFileNum > MAX_MANAGE_FILE_NUM) m_uiMaxFileNum = MAX_MANAGE_FILE_NUM;
    m_uiMaxFileNum = uiMaxFileNum;

	m_bCache = bCache;

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
        pBinLogFile = new CwxBinLogFile(map_iter->first.getDay(), map_iter->first.getFileNo(), m_uiMaxFileSize);
        if (0 != pBinLogFile->open(map_iter->second.c_str(),
            m_pCurBinlog?true:false,
            false,
			m_bCache,
            m_szErr2K))
        {
            delete pBinLogFile;
            if (szErr2K) strcpy(szErr2K, m_szErr2K);
            return -1;
        }
		if (!pBinLogFile->getLogNum())
		{///�������ļ���ɾ��
			CWX_INFO(("Remove binlog file for empty, file:%s", pBinLogFile->getDataFileName().c_str()));
			CwxBinLogFile::remove(pBinLogFile->getDataFileName().c_str());
			delete pBinLogFile;
			map_iter++;
			continue;
		}
        if (!m_pCurBinlog)
        {
            m_ttMaxTimestamp = pBinLogFile->getMaxTimestamp();
            m_ttMinTimestamp = pBinLogFile->getMinTimestamp();
        }
		else
		{
			//������С��ʱ���
			m_ttMinTimestamp = pBinLogFile->getMinTimestamp();
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
    if (!m_pCurBinlog)
    {
        string strPathFile;
        m_pCurBinlog = new CwxBinLogFile(CwxDate::trimToDay(ttTimestamp), START_FILE_NUM, m_uiMaxFileSize);
        getFileNameByFileNo(START_FILE_NUM, m_pCurBinlog->getFileDay(), strPathFile);
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
		CWX_UINT32 uiFileNum = m_pCurBinlog->getFileNo() + 1; 
        getFileNameByFileNo(uiFileNum, CwxDate::trimToDay(ttTimestamp), strPathFile);
        CwxBinLogFile* pBinLogFile = new CwxBinLogFile(CwxDate::trimToDay(ttTimestamp), uiFileNum, m_uiMaxFileSize);
        if (0 != pBinLogFile->open(strPathFile.c_str(),
            false,
            true,
			m_bCache,
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
			CWX_INFO(("Remove binlog file for outdate, file:%s", m_arrBinlog[0]->getDataFileName().c_str()));
			CwxBinLogFile::remove(m_arrBinlog[0]->getDataFileName().c_str());
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
			m_ullMinSid = m_pCurBinlog->getMinSid();
			m_ttMinTimestamp =m_pCurBinlog->getMinTimestamp();
		}

		///������������binlog�ļ���Ϣ
        _outputManageBinLog();
    }

    int iRet = m_pCurBinlog->append(ullSid,
        ttTimestamp,
        uiGroup,
        uiType,
        szData,
        uiDataLen,
        m_szErr2K);
    if (-1 == iRet)
    {
		if (!m_pCurBinlog->m_bValid)
		{
	        m_bValid = false;
		}
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
		//���ܻ����ʱ��
		if (m_ttMinTimestamp > ttTimestamp) m_ttMinTimestamp = ttTimestamp;
        return 0;
    }
    if (0 == iRet)
    {
        string strPathFile;
		CWX_UINT32 uiFileNum = m_pCurBinlog->getFileNo() + 1;
        getFileNameByFileNo(uiFileNum,
            m_pCurBinlog->getFileDay(),
            strPathFile);
        CwxBinLogFile* pBinLogFile = new CwxBinLogFile(m_pCurBinlog->getFileDay(),
            uiFileNum, 
            m_uiMaxFileSize);
        if (0 != pBinLogFile->open(strPathFile.c_str(),
            false,
            true,
			m_bCache,
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
			CWX_INFO(("Remove binlog file for outdate, file:%s", m_arrBinlog[0]->getDataFileName().c_str()));
			CwxBinLogFile::remove(m_arrBinlog[0]->getDataFileName().c_str());
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
			m_ullMinSid = m_pCurBinlog->getMinSid();
			m_ttMinTimestamp =m_pCurBinlog->getMinTimestamp();
		}
		///������������binlog�ļ���Ϣ
		_outputManageBinLog();
    }
    ///����¼������ӵ���binlog�ļ���
    iRet = m_pCurBinlog->append(ullSid,
        ttTimestamp,
        uiGroup,
        uiType,
        szData,
        uiDataLen,
        m_szErr2K);
    if (-1 == iRet)
    {
		if (!m_pCurBinlog->m_bValid)
		{
	        m_bValid = false;
		}
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
		//���ܻ����ʱ��
		if (m_ttMinTimestamp > ttTimestamp) m_ttMinTimestamp = ttTimestamp;
        return 0;
    }
    ///���ļ��޷�����һ����¼
    CwxCommon::snprintf(m_szErr2K, 2047, "Binlog's length[%d] is too large, can't be put binlog file", uiDataLen);
    if (szErr2K) strcpy(szErr2K, m_szErr2K);
    m_bValid = false;
    return -1;
}


//-1��ʧ�ܣ�0���ɹ���
int CwxBinLogMgr::commit(bool bAlL, char* szErr2K)
{
    int iRet = 0;
	if(!m_bValid)
	{
		if (szErr2K) strcpy(szErr2K, m_szErr2K);
		return -1;
	}
	if (!m_pCurBinlog) return 0;
	{
		///д������
		CwxWriteLockGuard<CwxRwLock> lock(&m_rwLock);
		iRet = m_pCurBinlog->flush_cache(bAlL, m_szErr2K);
		if (0 != iRet)
		{
			if (szErr2K) strcpy(szErr2K, m_szErr2K);
			return iRet;
		}
	}
	iRet = m_pCurBinlog->fsync_data(bAlL, m_szErr2K); 
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

    if (m_pCurBinlog)
	{
		m_pCurBinlog->flush_cache(true, NULL);
		m_pCurBinlog->fsync_data(true, NULL);
		delete m_pCurBinlog;
	}
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
    m_uiMaxFileNum = DEF_MANAGE_FILE_NUM;
    m_ullMinSid = 0; ///<binlog�ļ�����Сsid
    m_ullMaxSid = 0; ///<binlog�ļ������sid
    m_ttMinTimestamp = 0; ///<binlog�ļ���log��ʼʱ��
    m_ttMaxTimestamp = 0; ///<binlog�ļ���log����ʱ��
    m_bValid = false;
    strcpy(m_szErr2K, "Not init.");
}


//NULL ʧ�ܣ����򷵻��α�����ָ�롣
CwxBinLogCursor* CwxBinLogMgr::createCurser(CWX_UINT64 ullSid, CWX_UINT8 ucState)
{
	///��������
	CwxWriteLockGuard<CwxRwLock> lock(&m_rwLock);
    CwxBinLogCursor* pCursor =  new CwxBinLogCursor();
    pCursor->m_ullSid = ullSid;
    pCursor->m_ucSeekState = ucState;
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
	iRet = pBinLogFile->upper(ullSid, index,szErr2K);
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
	iRet = pBinLogFile->lower(ullSid, index, szErr2K);
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
	//��ȡ��һ�����ȼ�����ڴ����Ƿ����
	if (pCursor->m_pBinLogFile->m_writeCache && ///����cache
		pCursor->m_pBinLogFile->m_writeCache->m_dataSidMap.size()&&
		(pCursor->m_curLogHeader.getSid()  >= pCursor->m_pBinLogFile->m_writeCache->m_ullPrevDataSid))
	{
		map<CWX_UINT64/*sid*/, unsigned char*>::const_iterator iter = pCursor->m_pBinLogFile->m_writeCache->m_dataSidMap.upper_bound(pCursor->m_curLogHeader.getSid());
		if (iter == pCursor->m_pBinLogFile->m_writeCache->m_dataSidMap.end())
		{//û��next
			iRet = 0;
		}
		else
		{
			iRet = pCursor->seek((char const*)iter->second, iter->first, false);
		}
	}
	else
	{
		iRet = pCursor->next();//-2��log��header��������-1����ȡʧ�ܣ�0����ǰlogΪ���һ��log��1���Ƶ���һ��log
	}
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

	//��ȡ��һ�����ȼ�����ڴ����Ƿ����
	if (pCursor->m_pBinLogFile->m_writeCache && ///����cache
		pCursor->m_pBinLogFile->m_writeCache->m_dataSidMap.size()&&
		(pCursor->m_curLogHeader.getSid() >= pCursor->m_pBinLogFile->m_writeCache->m_ullMinDataSid))
	{
		map<CWX_UINT64/*sid*/, unsigned char*>::const_iterator iter = pCursor->m_pBinLogFile->m_writeCache->m_dataSidMap.lower_bound(pCursor->m_curLogHeader.getSid());
		if (iter->first == pCursor->m_curLogHeader.getSid()) iter--;
		iRet = pCursor->seek((char const*)iter->second, iter->first, false);
	}
	else
	{
		iRet = pCursor->prev();//-2��log��header��������-1����ȡʧ�ܣ�0����ǰlogΪ���һ��log��1���Ƶ���һ��log
	}

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
	///��������
	{
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
			strcpy(pCursor->m_szErr2K, "the cursor doesn't seek.");
			return -1;
		}
		//����Ƿ����ڴ�
		if (pCursor->m_pBinLogFile->m_writeCache &&
			pCursor->m_pBinLogFile->m_writeCache->m_dataSidMap.size()&&
			(pCursor->m_curLogHeader.getSid() >= pCursor->m_pBinLogFile->m_writeCache->m_ullMinDataSid))
		{
			map<CWX_UINT64/*sid*/, unsigned char*>::const_iterator iter = pCursor->m_pBinLogFile->m_writeCache->m_dataSidMap.find(pCursor->m_curLogHeader.getSid());
			CWX_ASSERT(iter != pCursor->m_pBinLogFile->m_writeCache->m_dataSidMap.end());
			if (uiDataLen < pCursor->m_curLogHeader.getLogLen())
			{
				CwxCommon::snprintf(pCursor->m_szErr2K, 2047, "Buf is too small, buf-size[%u], data-size[%u]",
					uiDataLen, pCursor->m_curLogHeader.getLogLen());
				pCursor->m_pBinLogFile = NULL;
				pCursor->m_ucSeekState = CURSOR_STATE_ERROR;
				return -1;
			}
			memcpy(szData, iter->second + CwxBinLogHeader::BIN_LOG_HEADER_SIZE, pCursor->m_curLogHeader.getLogLen());
			uiDataLen = pCursor->m_curLogHeader.getLogLen();
			return 1;
		}
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
