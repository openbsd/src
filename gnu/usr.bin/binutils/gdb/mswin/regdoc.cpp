// regdoc.cpp : implementation file
//

#include "stdafx.h"
#include "regdoc.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRegDoc

IMPLEMENT_DYNCREATE(CRegDoc, CDocument)



static CRegDoc *doclist = 0;

CRegDoc::CRegDoc()
{
  int i;
 
  for (i= 0; i < MAXREGS; i++)
    m_regsinit[i] = 0;

  m_next = doclist;

  doclist = this;
//Sync();	
		m_init=0;
}

BOOL CRegDoc::OnNewDocument()
{

  if (!CDocument::OnNewDocument())
    return FALSE;


  return TRUE;
}

CRegDoc::~CRegDoc()
{

}


BEGIN_MESSAGE_MAP(CRegDoc, CDocument)
	//{{AFX_MSG_MAP(CRegDoc)
	ON_COMMAND(ID_REAL_CMD_BUTTON_SYNC, OnSync)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



/////////////////////////////////////////////////////////////////////////////
// CRegDoc serialization

void CRegDoc::Serialize(CArchive& ar)
{

}

void CRegDoc::ChangeReg(int rn, int val)
{
m_regs[rn] = val;
}
/////////////////////////////////////////////////////////////////////////////
// CRegDoc commands



void CRegDoc::Sync()
{
  int rn;
  int hadchange = 0;
  const	int nregs = togdb_maxregs();
  long copy[MAXREGS];
  /* Do a pass to see if anything changed */
  for (rn = 0; rn < nregs; rn++) 
    {
      copy[rn] = togdb_fetchreg(rn);
      if (copy[rn] != m_regs[rn]) 
	hadchange = 1;
      m_regsinit[rn] = 1;	
    }
  
  if (hadchange) 
    {
      for (rn = 0; rn < nregs; rn++) 
	{

	  if (copy[rn] != m_regs[rn])
	    {

	      m_regs[rn] = copy[rn];
	      m_regchanged[rn] = 1;
	    }
	  else 
	    {
	      m_regchanged[rn] = 0;
	    }
	}
    UpdateAllViews(0);
    }
}


void CRegDoc::OnSync() 
{
  // TODO: Add your command handler code here
  Sync();	
}


void CRegDoc::OnCloseDocument() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	m_bModified = FALSE; 
	CDocument::OnCloseDocument();
}


void CRegDoc::prepare()
{
 if (!m_init) {
 	Sync();
	m_init =1 ;
	}


}

BOOL CRegDoc::SaveModified() 
{
	// TODO: Add your specialized code here and/or call the base class
		
	return TRUE; // CDocument::SaveModified();
}
