// VersionInfo.cpp: implementation of the CVersionInfo class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "bindinstall.h"
#include "VersionInfo.h"
#include <winver.h>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CVersionInfo::CVersionInfo(CString filename)
{
	HANDLE hFile;
	WIN32_FIND_DATA fd;
	memset(&fd, 0, sizeof(WIN32_FIND_DATA));

	m_status = ERROR_SUCCESS;
	m_isValid = FALSE;
	m_filename = filename;

	// See if the given file exists
	hFile = FindFirstFile(filename, &fd);
	if(hFile == INVALID_HANDLE_VALUE)
	{
		m_status = ERROR_FILE_NOT_FOUND;
		m_versionInfo = NULL;
		return;
	}
	FindClose(hFile);

	// Extract the file info 
	DWORD handle;
	DWORD viSize = GetFileVersionInfoSize((LPTSTR)(LPCTSTR)filename, &handle);
	m_versionInfo = NULL;

	if(viSize == 0)
	{
		m_status = GetLastError();
	}
	else
	{
		m_versionInfo = new char[viSize];

		// Get the block of version info from the file
		if(!GetFileVersionInfo((LPTSTR)(LPCTSTR)filename, handle, viSize, m_versionInfo))
		{
			if(m_versionInfo)
			{
				delete m_versionInfo;
				m_versionInfo = NULL;
			}
			return;
		}

		// Now extract the sub block we are interested in
		UINT versionLen = 0;
		LPVOID viBlob = NULL;
		if(!VerQueryValue(m_versionInfo, "\\", &viBlob, &versionLen))
		{
			if(m_versionInfo)
			{
				delete m_versionInfo;
				m_versionInfo = NULL;
			}
			return;
		}

		// And finally the version info is ours
		m_fixedInfo = (VS_FIXEDFILEINFO *)viBlob;

		UINT blobLen = 0;

		// If we got here, all is good
	}
	m_isValid = TRUE;
}

CVersionInfo::~CVersionInfo()
{
	m_fixedInfo = NULL;
	if(m_versionInfo)
	{
		delete m_versionInfo;
		m_versionInfo = NULL;
	}
}

CString CVersionInfo::GetFileVersionString()
{
	return(QueryStringValue("FileVersion"));
}

CString CVersionInfo::GetProductVersionString()
{
	return(QueryStringValue("ProductVersion"));
}

CString CVersionInfo::GetComments()
{
	return(QueryStringValue("Comments"));
}

CString CVersionInfo::GetFileDescription()
{
	return(QueryStringValue("FileDescription"));
}

CString CVersionInfo::GetInternalName()
{
	return(QueryStringValue("InternalName"));
}

CString CVersionInfo::GetLegalCopyright()
{
	return(QueryStringValue("LegalCopyright"));
}

CString CVersionInfo::GetLegalTrademarks()
{
	return(QueryStringValue("LegalTrademarks"));
}

CString CVersionInfo::GetOriginalFileName()
{
	return(QueryStringValue("OriginalFilename"));
}

CString CVersionInfo::GetProductName()
{
	return(QueryStringValue("ProductName"));
}

CString CVersionInfo::GetSpecialBuildString()
{
	return(QueryStringValue("SpecialBuild"));
}

CString CVersionInfo::GetPrivateBuildString()
{
	return(QueryStringValue("PrivateBuild"));
}

CString CVersionInfo::GetCompanyName()
{
	return(QueryStringValue("CompanyName"));
}

#ifdef NOTUSED
BOOL CVersionInfo::CopyFileCheckVersion(CVersionInfo &originalFile)
{
	_int64 myVer = GetFileVersion();
	_int64 origVer = originalFile.GetFileVersion();

	if(origVer > myVer)
	{
		CString msg;
		msg.Format(IDS_EXISTING_NEWER, m_filename);
		DWORD query = AfxMessageBox(msg, MB_YESNO);
		if(query == IDNO)
			return(TRUE);
	}
	
	return(CopyFileNoVersion(originalFile));
}
#endif

BOOL CVersionInfo::CopyFileNoVersion(CVersionInfo &originalFile)
{
	int x = 7;
	return(CopyFile(originalFile.GetFilename(), m_filename, FALSE));
}


_int64 CVersionInfo::GetFileVersion()
{
	_int64 ver = 0;
	
	if(m_versionInfo)
	{
		ver = m_fixedInfo->dwFileVersionMS;
		ver <<= 32;
		ver += m_fixedInfo->dwFileVersionLS;
	}
	return(ver);
}

_int64 CVersionInfo::GetProductVersion()
{
	_int64 ver = 0;

	if(m_versionInfo)
	{
		ver = m_fixedInfo->dwProductVersionMS;
		ver <<= 32;
		ver += m_fixedInfo->dwProductVersionLS;
	}
	return(ver);
}

_int64 CVersionInfo::GetFileDate()
{
	_int64 fDate = 0;

	if(m_versionInfo)
	{
		fDate = m_fixedInfo->dwFileDateMS;
		fDate <<= 32;
		fDate += m_fixedInfo->dwFileDateLS;
	}
	return(fDate);
}

DWORD CVersionInfo::GetFileFlagMask()
{
	if(m_versionInfo)
	{
		return(m_fixedInfo->dwFileFlagsMask);
	}
	return(0);
}

DWORD CVersionInfo::GetFileFlags()
{
	if(m_versionInfo)
	{
		return(m_fixedInfo->dwFileFlags);
	}
	return(0);
}

DWORD CVersionInfo::GetFileOS()
{
	if(m_versionInfo)
	{
		return(m_fixedInfo->dwFileOS);
	}
	return(VOS_UNKNOWN);
}

DWORD CVersionInfo::GetFileType()
{
	if(m_versionInfo)
	{
		return(m_fixedInfo->dwFileType);
	}
	return(VFT_UNKNOWN);
}

DWORD CVersionInfo::GetFileSubType()
{
	if(m_versionInfo)
	{
		return(m_fixedInfo->dwFileSubtype);
	}
	return(VFT2_UNKNOWN);
}

CString CVersionInfo::QueryStringValue(CString value)
{
	UINT blobLen = 0;
	LPVOID viBlob = NULL;

	if(m_versionInfo)
	{
		char queryString[256];

		// This code page value is for American English.  If you change the resources to be other than that
		// You probably should change this to match it.
		DWORD codePage = 0x040904B0;

		sprintf(queryString, "\\StringFileInfo\\%08X\\%s", codePage, value);

		if(VerQueryValue(m_versionInfo, queryString, &viBlob, &blobLen))
			return((char *)viBlob);
	}	
	return("Not Available");
}
