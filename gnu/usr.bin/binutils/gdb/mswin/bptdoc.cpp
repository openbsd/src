// bptdoc.cpp : implementation file
//

#include "stdafx.h"
#include "bptdoc.h"
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBptDoc

IMPLEMENT_DYNCREATE(CBptDoc, CDocument)

CBptDoc *p = 0;
CBptDoc::CBptDoc()
{
p = this;
}

void CBptDoc::AllSync()
{
  if (p)
    p->Sync();
}

BOOL CBptDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	SetTitle("Breakpoints");
	return TRUE;
}

CBptDoc::~CBptDoc()
{ p = 0;
}


BEGIN_MESSAGE_MAP(CBptDoc, CDocument)
	//{{AFX_MSG_MAP(CBptDoc)
	ON_COMMAND(ID_REAL_CMD_BUTTON_SYNC, OnSync)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()




/////////////////////////////////////////////////////////////////////////////
// CBptDoc serialization

void CBptDoc::Serialize(CArchive& ar)
{

}

/////////////////////////////////////////////////////////////////////////////
// CBptDoc commands



void CBptDoc::Sync()
{
    UpdateAllViews(0);
}


void CBptDoc::OnSync() 
{
  // TODO: Add your command handler code here
  Sync();	
}


