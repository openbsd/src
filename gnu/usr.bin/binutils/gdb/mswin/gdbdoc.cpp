// gdbdoc.cpp : implementation file
//

#include "stdafx.h"
#include "gdbdoc.h"
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGdbDoc

IMPLEMENT_DYNCREATE(CGdbDoc, CDocument)

CGdbDoc::CGdbDoc()
{
}

BOOL CGdbDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;
	return TRUE;
}

CGdbDoc::~CGdbDoc()
{
}


BEGIN_MESSAGE_MAP(CGdbDoc, CDocument)
	//{{AFX_MSG_MAP(CGdbDoc)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()




/////////////////////////////////////////////////////////////////////////////
// CGdbDoc serialization

void CGdbDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

/////////////////////////////////////////////////////////////////////////////
// CGdbDoc commands

