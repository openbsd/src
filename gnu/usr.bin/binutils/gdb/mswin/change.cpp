// change.cpp : implementation file
//

#include "stdafx.h"
//#include "gui.h"
#include "change.h"

void theApp_add_watch(const char *);
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CChange dialog


CChange::CChange(CWnd* pParent /*=NULL*/)
	: CDialog(CChange::IDD, pParent)
{
	//{{AFX_DATA_INIT(CChange)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}


void CChange::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CChange)
	DDX_Control(pDX, IDC_VAR_NAME, name);
	DDX_Control(pDX, IDC_VALUE, value);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CChange, CDialog)
	//{{AFX_MSG_MAP(CChange)
	ON_BN_CLICKED(IDC_WATCH, OnWatch)
	ON_BN_CLICKED(IDC_SET, OnSet)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CChange message handlers

void CChange::change(const char *name)
{
CChange dialog;
dialog.sname = name;
dialog.DoModal();
}

void CChange::think()
{
CString exp;
  name.GetWindowText(exp);
  CString b = togdb_eval_as_string (exp);
  value.SetWindowText(b);

}
BOOL CChange::OnInitDialog() 
{
 CDialog::OnInitDialog();
 
 // TODO: Add extra initialization here
 name.SetWindowText(sname);	
 think();
 return TRUE;			// return TRUE unless you set the focus to a control
 // EXCEPTION: OCX Property Pages should return FALSE
}

void CChange::OnOK() 
{
	// TODO: Add extra validation here
	CDialog::OnOK();
}

void CChange::OnWatch() 
{
CString exp;
name.GetWindowText(exp)		  ;
  theApp_add_watch((const char *)exp);
}

void CChange::OnSet() 
{
 CString dst ;
 CString val;
 name.GetWindowText(dst);
 value.GetWindowText(val);
 CString ass = dst + "=" + val;
 CString b = togdb_eval_as_string ((const char *)ass);
 think();
}
