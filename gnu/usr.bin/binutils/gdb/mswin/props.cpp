#if 0
#include "stdafx.h"
#include "srcwin.h"
#include "colinfo.h"

CSrcState::CSrcState()
{
  addresses = FALSE;
  breakpoint_ok = FALSE;
  disassembly = FALSE;
  instruction_data = FALSE;
  linenumbers = TRUE;
  source = TRUE;
}

CSrcState defs;

extern  CColorInfo  colinfo_s;
extern  CColorInfo  colinfo_a ;
extern CColorInfo  colinfo_b;
extern  CFontInfo srcfontinfo;

/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CSrcWinP dialog

class CSrcWinP : public CPropertyPage
{
	DECLARE_DYNCREATE(CSrcWinP)

// Construction
public:
	CSrcWinP();
	~CSrcWinP();

// Dialog Data
	//{{AFX_DATA(CSrcWinP)
	enum { IDD = ID_SYM_DIALOG_SRCWINIP };
	    class CSrcState state;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSrcWinP)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSrcWinP)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


/////////////////////////////////////////////////////////////////////////////
// CSrcWinFC dialog

class CSrcWinFC : public CPropertyPage
{
	DECLARE_DYNCREATE(CSrcWinFC)

// Construction
public:
	CSrcWinFC();
	~CSrcWinFC();

// Dialog Data
	//{{AFX_DATA(CSrcWinFC)
	enum { IDD = ID_SYM_DIALOG_SRCWINF };
		// NOTE - ClassWizard will add data members here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSrcWinFC)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CSrcWinFC)
	afx_msg void OnSetAsmColor();
	afx_msg void OnSetSrcColor();
	afx_msg void OnSetBckColor();
	afx_msg void OnSetFont();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};


/////////////////////////////////////////////////////////////////////////////
// CSrcWinPr

class CSrcWinPr : public CPropertySheet
{
  DECLARE_DYNAMIC(CSrcWinPr)
    CSrcWinP instance;
CSrcWinFC color;

  // Construction
public:
  //	CSrcWinPr(UINT nIDCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
  //	CSrcWinPr(LPCTSTR pszCaption, CWnd* pParentWnd = NULL, UINT iSelectPage = 0);
  CSrcWinPr();
  // Attributes
public:
  
  // Operations
public:
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSrcWinPr)
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CSrcWinPr();
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CSrcWinPr)
  // NOTE - the ClassWizard will add and remove member functions here.
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

/////////////////////////////////////////////////////////////////////////////
// CSrcWinP property page

IMPLEMENT_DYNCREATE(CSrcWinP, CPropertyPage)
     
     CSrcWinP::CSrcWinP() : CPropertyPage(CSrcWinP::IDD)
{
  //{{AFX_DATA_INIT(CSrcWinP)
  //}}AFX_DATA_INIT
}

CSrcWinP::~CSrcWinP()
{
}

void CSrcWinP::DoDataExchange(CDataExchange* pDX)
{
  CPropertyPage::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CSrcWinP)
  DDX_Check(pDX, ID_CMD_BUTTON_ADDRESSES, defs.addresses);
  DDX_Check(pDX, ID_CMD_BUTTON_BREAKPOINT_OK,defs.breakpoint_ok);
  DDX_Check(pDX, ID_CMD_BUTTON_DISASSEMBLY, defs.disassembly);
  DDX_Check(pDX, ID_REAL_CMD_BUTTON_INSTRUCTION_DATA, defs.instruction_data);
  DDX_Check(pDX, ID_CMD_BUTTON_LINENUMBERS, defs.linenumbers);
  DDX_Check(pDX, ID_REAL_CMD_BUTTON_SOURCE, defs.source);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSrcWinP, CPropertyPage)
     //{{AFX_MSG_MAP(CSrcWinP)
     // NOTE: the ClassWizard will add message map macros here
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CSrcWinP message handlers
     
     // CSrcWinPr
     
     IMPLEMENT_DYNAMIC(CSrcWinPr, CPropertySheet)
     
     CSrcWinPr::CSrcWinPr() : CPropertySheet("Properties",0,0)
{
  AddPage(&instance);
  AddPage(&color);
}
CSrcWinPr::~CSrcWinPr()
{
}


BEGIN_MESSAGE_MAP(CSrcWinPr, CPropertySheet)
     //{{AFX_MSG_MAP(CSrcWinPr)
     // NOTE - the ClassWizard will add and remove mapping macros here.
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CSrcWinPr message handlers
     
     /////////////////////////////////////////////////////////////////////////////
     // CSrcWinF message handlers
     /////////////////////////////////////////////////////////////////////////////
     // CSrcWinFC property page
     
     IMPLEMENT_DYNCREATE(CSrcWinFC, CPropertyPage)
     
     CSrcWinFC::CSrcWinFC() : CPropertyPage(CSrcWinFC::IDD)
{
  //{{AFX_DATA_INIT(CSrcWinFC)
  // NOTE: the ClassWizard will add member initialization here
  //}}AFX_DATA_INIT
}

CSrcWinFC::~CSrcWinFC()
{
}

void CSrcWinFC::DoDataExchange(CDataExchange* pDX)
{
  CPropertyPage::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CSrcWinFC)
  // NOTE: the ClassWizard will add DDX and DDV calls here
  //}}AFX_DATA_MAP
}



BEGIN_MESSAGE_MAP(CSrcWinFC, CPropertyPage)
     //{{AFX_MSG_MAP(CSrcWinFC)
     ON_BN_CLICKED(ID_REAL_CMD_BUTTON_SET_ASM_COLOR, OnSetAsmColor)
     ON_BN_CLICKED(ID_REAL_CMD_BUTTON_SET_SRC_COLOR, OnSetSrcColor)
     ON_BN_CLICKED(ID_REAL_CMD_BUTTON_SET_BCK_COLOR, OnSetBckColor)
     ON_BN_CLICKED(ID_REAL_CMD_BUTTON_SET_FONT, OnSetFont)
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CSrcWinFC message handlers
     
void CSrcWinFC::OnSetAsmColor() 
{
  // TODO: Add your control notification handler code here
  
  colinfo_a.OnChooseColor();
  redraw_allsrcwins();
}
void CSrcWinFC::OnSetSrcColor() 
{
  // TODO: Add your control notification handler code here
  
  colinfo_s.OnChooseColor();
  redraw_allsrcwins();
}
void CSrcWinFC::OnSetBckColor() 
{
  // TODO: Add your control notification handler code here
  
  colinfo_b.OnChooseColor();
  redraw_allsrcwins();
}

void CSrcWinFC::OnSetFont() 
{
  srcfontinfo.OnChooseFont();
  redraw_allsrcwins();
  
}

void props()
{
  CSrcWinPr p;
  if (p.DoModal() == IDOK) 
    {
      redraw_allsrcwins();
    }
}
#endif
