#include "StdAfx.h"
#include "X64Compat.h"
#include "FileDir.h"
#include <string>

CDir::CDir()
{
	Initialize();
}

CDir::~CDir()
{
	Destroy();
}

void CDir::Destroy()
{
	if (m_hFind)
		FindClose(m_hFind);

	Initialize();
}

bool CDir::Create(const char * c_szFilter, const char * c_szPath, BOOL bCheckedExtension)
{
	Destroy();

	std::string stPath = c_szPath;

	if (stPath.length())
	{
		char end = stPath[stPath.length() - 1];

		if (end != '\\')
			stPath += '\\';
	}

	std::string stQuery;
	stQuery += stPath;
	stQuery += "*.*";

	m_wfd.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
	m_hFind = FindFirstFile(stQuery.c_str(), &m_wfd);

	if (m_hFind == INVALID_HANDLE_VALUE)
		return true;

	do
	{
		if (*m_wfd.cFileName == '.')
			continue;

		if (IsFolder())
		{
			if (!OnFolder(c_szFilter, stPath.c_str(), m_wfd.cFileName))
				return false;
		}
		else
		{
			const char * c_szExtension = strchr(m_wfd.cFileName, '.');
			if (!c_szExtension)
				continue;

			// NOTE : �ӽ� ���� - [levites]
			//        ���������δ� ������ TRUE ���·� �����.
			//        ������ �� ������Ʈ�� CDir�� ����ϴ� ������ Extension�� "wav", "gr2" �̷������� �ְԲ� �Ѵ�. - [levites]
			if (bCheckedExtension)
			{
				std::string strFilter = c_szFilter;
				const auto iPos = strFilter.find_first_of(';', 0);
				if (iPos != std::string::npos && iPos > 0)
				{
					std::string strFirstFilter = std::string(c_szFilter).substr(0, iPos);
					std::string strSecondFilter = std::string(c_szFilter).substr(iPos + 1, strlen(c_szFilter));
					if (c_szExtension + 1 != strFirstFilter && c_szExtension + 1 != strSecondFilter)
						continue;
				}
				else
				{
					if (0 != stricmp(c_szExtension + 1, c_szFilter))
						continue;
				}
			}

			if (!OnFile(stPath.c_str(), m_wfd.cFileName))
				return false;
		}
	} while (FindNextFile(m_hFind, &m_wfd));

	return true;
}

bool CDir::IsFolder() const
{
	return (m_wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

void CDir::Initialize()
{
	m_wfd = {};
	m_hFind = nullptr;
}
