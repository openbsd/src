#if 0
// mem.cpp : implementation file
//

#include "stdafx.h"
#include "regdoc.h"
#include "mem.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern CGuiApp theApp;
void redraw_allmemwins()
{ 
redraw_allwins(theApp.m_memTemplate);
}

 CFontInfo mem_fontinfo  ("MemFont", redraw_allmemwins);

/////////////////////////////////////////////////////////////////////////////
// CMem

IMPLEMENT_DYNCREATE(CMem, CView)

CMem::CMem()
{
}

CMem::~CMem()
{
}


BEGIN_MESSAGE_MAP(CMem, CView)
	//{{AFX_MSG_MAP(CMem)
	ON_EN_CHANGE(IDC_EDIT1, OnChangeEdit)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CMem drawing

void CMem::OnDraw(CDC* pDC)
{
  CRegDoc* pDoc = (CRegDoc *)GetDocument();
  CRect rect;
  GetClientRect (rect);
  /* Work out how many bytes we can go across */
  int wid = 40;

  int off = 0x100;
  int yco;
  CORE_ADDR addr = off;

  for (yco = 0; yco < rect.bottom; yco += mem_fontinfo.dunits.cy)
    {
      char abuf[20];
      sprintf(abuf,"%s ", paddr(addr)); /* FIXME - this bit's not done yet */

      pDC->TextOut(0,yco,abuf);
     addr += off;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CMem message handlers
/////////////////////////////////////////////////////////////////////////////
// CMemFrame

IMPLEMENT_DYNCREATE(CMemFrame, CMiniMDIChildWnd)

CMemFrame::CMemFrame()
{
}

CMemFrame::~CMemFrame()
{
}


BEGIN_MESSAGE_MAP(CMemFrame, CMiniMDIChildWnd)
	//{{AFX_MSG_MAP(CMemFrame)
		// NOTE - the ClassWizard will add and remove mapping macros here.

    // ON_EN_CHANGE(ID_CMD_BUTTON_BREAKPOINT, on_edit_changed)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CMemFrame message handlers

BOOL CMemFrame::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
}

BOOL CMemFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class
#if 1
bar.Create(this, IDD_DIALOG3, WS_CHILD|WS_VISIBLE|CBRS_TOP, 0);
bar.ShowWindow(1);
bar.SetWindowPos(0,0,0,0,0,SWP_NOSIZE);
#endif
#if 0
CRect x;
GetClientRect(x);
mini.Create(0, "memory", WS_CAPTION|WS_POPUP|WS_SYSMENU|WS_VISIBLE|MFS_THICKFRAME|MFS_SYNCACTIVE|MFS_MOVEFRAME, x, this, 0);
mini.SetWindowPos(0,40,40,100,100,0);
#endif
SetWindowText("memory");
	return CMiniMDIChildWnd::OnCreateClient(lpcs, pContext);
}

BOOL CMemFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	// TODO: Add your specialized code here and/or call the base class
//	cs.style &= ~(WS_THICKFRAME|WS_CAPTION);
//	cs.style |= WS_BORDER | WS_THICKFRAME;

	return CMiniMDIChildWnd::PreCreateWindow(cs);
}


/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// CMemMini

IMPLEMENT_DYNCREATE(CMemMini, CMiniFrameWnd)

CMemMini::CMemMini()
{
}

CMemMini::~CMemMini()
{
}


BEGIN_MESSAGE_MAP(CMemMini, CMiniFrameWnd)
	//{{AFX_MSG_MAP(CMemMini)
		// NOTE - the ClassWizard will add and remove mapping macros here.
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CMemMini message handlers


void CMem::Initialize()
{
  mem_fontinfo.Initialize();
}

void CMem::Terminate()
{
  mem_fontinfo.Terminate();
}

void CMem::OnChangeEdit() 
{
	// TODO: Add your control notification handler code here
	
}

void CMem::OnInitialUpdate() 
{
 // TODO: Add your specialized code here and/or call the base class
 

// edit.Create(WS_TABSTOP|WS_CHILD|WS_VISIBLE|WS_BORDER, CRect(0,0,20,20), this,ID_EDIT1);
 //edit.SetWindowPos(0,0,0,20,20,0);

 CView::OnInitialUpdate();
}
#endif
