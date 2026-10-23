#include "StdAfx.h"
#include "X64Compat.h"
#include "FileLoader.h"
#include <cassert>

CMemoryTextFileLoader::CMemoryTextFileLoader() = default;

CMemoryTextFileLoader::~CMemoryTextFileLoader() = default;

bool CMemoryTextFileLoader::SplitLineByTab(uint32_t dwLine, CTokenVector * pstTokenVector)
{
	pstTokenVector->reserve(10);
	pstTokenVector->clear();

	const std::string & c_rstLine = GetLineString(dwLine);
	const std::string::size_type c_iLineLength = c_rstLine.length();

	if (0 == c_iLineLength)
		return false;

	std::string::size_type basePos = 0;

	do
	{
		const auto beginPos = c_rstLine.find_first_of('\t', basePos);

		pstTokenVector->emplace_back(c_rstLine.substr(basePos, beginPos - basePos));

		basePos = beginPos + 1;
	} while (basePos < c_iLineLength && basePos > 0);

	return true;
}

int CMemoryTextFileLoader::SplitLine2(uint32_t dwLine, CTokenVector * pstTokenVector, const char * c_szDelimeter)
{
	pstTokenVector->reserve(10);
	pstTokenVector->clear();

	std::string stToken;
	const std::string & c_rstLine = GetLineString(dwLine);

	std::string::size_type basePos = 0;

	do
	{
		auto beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);

		if (beginPos == std::string::npos)
			return -1;

		std::string::size_type endPos;

		if (c_rstLine[beginPos] == '"')
		{
			++beginPos;
			endPos = c_rstLine.find_first_of('"', beginPos);

			if (endPos == std::string::npos)
				return -2;

			basePos = endPos + 1;
		}
		else
		{
			endPos = c_rstLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pstTokenVector->emplace_back(c_rstLine.substr(beginPos, endPos - beginPos));

		// �߰� �ڵ�. �ǵڿ� ���� �ִ� ��츦 üũ�Ѵ�. - [levites]
		if (c_rstLine.find_first_not_of(c_szDelimeter, basePos) == std::string::npos)
			break;
	} while (basePos < c_rstLine.length());

	return 0;
}

#include "../UserInterface/Locale_inc.h"	//ENABLE_DS_SET
bool CMemoryTextFileLoader::SplitLine(uint32_t dwLine, CTokenVector * pstTokenVector, const char * c_szDelimeter)
{
	pstTokenVector->reserve(10);
	pstTokenVector->clear();

	std::string stToken;
	const std::string & c_rstLine = GetLineString(dwLine);

	std::string::size_type basePos = 0;

	do
	{
		auto beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);
		if (beginPos == std::string::npos)
			return false;

		std::string::size_type endPos;

#ifdef ENABLE_DS_SET
		if (c_rstLine[beginPos] == '#')
		{
			if (c_rstLine.compare(beginPos, 4, "#--#"))
				return false;
		}
#endif

		if (c_rstLine[beginPos] == '"')
		{
			++beginPos;
			endPos = c_rstLine.find_first_of('"', beginPos);

			if (endPos == std::string::npos)
				return false;

			basePos = endPos + 1;
		}
		else
		{
			endPos = c_rstLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pstTokenVector->emplace_back(c_rstLine.substr(beginPos, endPos - beginPos));

		// �߰� �ڵ�. �ǵڿ� ���� �ִ� ��츦 üũ�Ѵ�. - [levites]
		if (c_rstLine.find_first_not_of(c_szDelimeter, basePos) == std::string::npos)
			break;
	} while (basePos < c_rstLine.length());

	return true;
}

uint32_t CMemoryTextFileLoader::GetLineCount() const
{
	return m2::narrow<uint32_t>(m_stLineVector.size());
}

bool CMemoryTextFileLoader::CheckLineIndex(uint32_t dwLine) const
{
	return dwLine < m_stLineVector.size();
}

const std::string & CMemoryTextFileLoader::GetLineString(uint32_t dwLine)
{
	assert(CheckLineIndex(dwLine));
	return m_stLineVector[dwLine];
}

void CMemoryTextFileLoader::Bind(int bufSize, const void * c_pvBuf)
{
	m_stLineVector.reserve(128);
	m_stLineVector.clear();

	const auto * c_pcBuf = static_cast<const char *>(c_pvBuf);
	std::string stLine;
	int pos = 0;

	while (pos < bufSize)
	{
		const char c = c_pcBuf[pos++];

		if ('\n' == c || '\r' == c)
		{
			if (pos < bufSize)
				if ('\n' == c_pcBuf[pos] || '\r' == c_pcBuf[pos])
					++pos;

			m_stLineVector.emplace_back(stLine);
			stLine = "";
		}
#ifndef ENABLE_MULTI_LANGUAGE_SYSTEM
		else if (c < 0)
		{
			stLine.append(c_pcBuf + (pos - 1), 2);
			++pos;
		}
#endif
		else
			stLine += c;
	}

	m_stLineVector.emplace_back(stLine);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
int CMemoryFileLoader::GetSize() const
{
	return m_size;
}

int CMemoryFileLoader::GetPosition() const
{
	return m_pos;
}

bool CMemoryFileLoader::IsReadableSize(int size) const
{
	return m_pos + size <= m_size;
}

bool CMemoryFileLoader::Read(int size, void * pvDst)
{
	if (!IsReadableSize(size))
		return false;

	memcpy(pvDst, GetCurrentPositionPointer(), size);
	m_pos += size;
	return true;
}

const char * CMemoryFileLoader::GetCurrentPositionPointer() const
{
	assert(m_pcBase != nullptr);
	return (m_pcBase + m_pos);
}

CMemoryFileLoader::CMemoryFileLoader(int size, const void * c_pvMemoryFile)
{
	assert(c_pvMemoryFile != nullptr);

	m_pos = 0;
	m_size = size;
	m_pcBase = static_cast<const char *>(c_pvMemoryFile);
}

CMemoryFileLoader::~CMemoryFileLoader() = default;

//////////////////////////////////////////////////////////////////////////////////////////////////
int CDiskFileLoader::GetSize() const
{
	return m_size;
}

bool CDiskFileLoader::Read(int size, void * pvDst) const
{
	assert(m_fp != nullptr);

	const size_t ret = fread(pvDst, size, 1, m_fp);

	return ret > 0;
}

bool CDiskFileLoader::Open(const char * c_szFileName)
{
	Close();

	if (!c_szFileName[0])
		return false;

	m_fp = fopen(c_szFileName, "rb");

	if (!m_fp)
		return false;

	fseek(m_fp, 0, SEEK_END);
	m_size = ftell(m_fp);
	fseek(m_fp, 0, SEEK_SET);
	return true;
}

void CDiskFileLoader::Close()
{
	if (m_fp)
		fclose(m_fp);

	Initialize();
}

void CDiskFileLoader::Initialize()
{
	m_fp = nullptr;
	m_size = 0;
}

CDiskFileLoader::CDiskFileLoader()
{
	Initialize();
}

CDiskFileLoader::~CDiskFileLoader()
{
	Close();
}
