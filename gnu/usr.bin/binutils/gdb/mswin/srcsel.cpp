// Srcsel.cpp : implementation file
//

#include "stdafx.h"
#ifdef __MFC4__			/* FIXME */
    #ifdef PTR
	//#error PTR 2
	#undef PTR	/* FIXME: new mfc has typedef for PTR */
	//#define PTR void *	/* FIXME: new mfc has typedef for PTR */
    #endif
#endif
#include "srcsel.h"
#include "srcd.h"
#ifdef __MFC4__			/* FIXME */
    #include "afxdlgs.h"	/* FIXME: use new mfc header */
    //#include "afxdlgs2.h"	/* FIXME: use old mfc header */
#else
    #include "afxdlgs.h"
#endif
#include "srcwin.h"
#include <../src/afximpl.h>	/* FIXME: this is a private header! */

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

static CFontInfo fontinfo ("TabFont");
/////////////////////////////////////////////////////////////////////////////
// CSrcSel

CSrcSel::CSrcSel()
{
m_hWnd = 0;
}

CSrcSel::~CSrcSel()
{
}


BEGIN_MESSAGE_MAP(CSrcSel, CTabControl)
	//{{AFX_MSG_MAP(CSrcSel)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CSrcSel message handlers







int CSrcSel::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  static	LOGFONT lf; 
  if (CTabControl::OnCreate(lpCreateStruct) == -1)
    return -1;
  lf.lfHeight = -12;
  lf.lfWeight = FW_LIGHT;

  m_hThinFont = CreateFontIndirect(&lf);

							//m_hThinFont = fontinfo.m_font;

  lf.lfWeight = FW_BOLD;
  m_hBoldFont = CreateFontIndirect(&lf);

 
  return 0;
}


void CSrcSel::Initialize()
{
  fontinfo.Initialize();
}

void CSrcSel::Terminate()
{
  fontinfo.Terminate();
}

 
class CTabItem : public CObject 
{ 
public: 
	CTabItem(LPCTSTR szCaption, int nWidth); 
	void Draw(CDC* pDC, HFONT hFont, BOOL bCurTab); 
 
	CString m_strCaption; 
	CRect   m_rect; 
	CRect   m_rectPrev; 
	int     m_nWidth; 
}; 
 
// amount to inflate the selected tab 
static const CSize sizeSelTab(2, 2); 

		  void mb(char*);
void CSrcSel::OnPaint() 
{
	CPaintDC dc(this); 
	dc.SetBkMode(TRANSPARENT); 
						 


HPEN prev = afxData.hpenBtnHilite;
CPen pe;
pe.CreateStockObject(BLACK_PEN);
afxData.hpenBtnHilite = (HPEN)dc.SelectObject(&pe);
 
//	CTabControl::OnPaint();
#ifndef __MFC4__	// FIXME: ??
#if 1

	// Draw all the tabs that are currently within view 
	for (int i = 0 ; i < GetItemCount() ; i++) 
	{ 
		if (IsTabVisible(i) && (i != m_nCurTab)) 
			GetTabItem(i)->Draw(&dc, m_hThinFont, FALSE); 
	} 
 
	// Draw the current tab last so that it gets drawn on "top" 
	if (IsTabVisible(m_nCurTab)) 
		GetTabItem(m_nCurTab)->Draw(&dc, m_hBoldFont, TRUE); 
 
 
	// Draw the line underneath all the tabs 
	CRect rectItem = GetTabItem(m_nCurTab)->m_rect; 
	CPen pen;
 
	pen.CreateStockObject(BLACK_PEN);
 
	CPen *old = dc.SelectObject(&pen);
  
	CRect rect; 
	GetWindowRect(&rect); 
	rect.OffsetRect(-rect.left, -rect.top); 
 	 
	dc.MoveTo(0, rect.bottom - 1); 
	if (!rectItem.IsRectNull()) 
	{ 
		// this leaves a gap in the line if the currently selected 
		// tab is within view. 
		dc.LineTo(rectItem.left - sizeSelTab.cx, rect.bottom - 1); 
		dc.MoveTo(rectItem.right + sizeSelTab.cx + 1, rect.bottom - 1); 
	} 
	dc.LineTo(rect.right + 1, rect.bottom - 1); 
 
	dc.SelectObject(old);
 
	if (CanScroll()) 
		DrawScrollers(&dc); 
 
	if (GetFocus() == this) 
		DrawFocusRect(&dc); 
#endif
#endif		// FIXME: ??
 afxData.hpenBtnHilite = prev;
 
	

}

void CSrcSel::OnSize(UINT nType, int cx, int cy) 
{
	CTabControl::OnSize(nType, cx, cy);
	
	// TODO: Add your message handler code here
	
}

/////////////////////////////////////////////////////////////////////////////
// CSrcSelWrap

CSrcSelWrap::CSrcSelWrap()
{
}

CSrcSelWrap::~CSrcSelWrap()
{
}


BEGIN_MESSAGE_MAP(CSrcSelWrap, CWnd)
	//{{AFX_MSG_MAP(CSrcSelWrap)
	ON_WM_PAINT()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CSrcSelWrap message handlers


void CSrcSelWrap::AddTab(const char *x)
{
sel.AddTab(x);
}


BOOL CSrcSelWrap::Create(int flags, CRect &r, CWnd *p, int i)
{ 
 CWnd::Create(0,0, WS_CHILD|WS_VISIBLE, r, p, i,0);
sel.Create(flags,r,this, i);
return TRUE;
}

void CSrcSelWrap::OnPaint() 
{
 CPaintDC dc(this);		// device context for painting

 int i;

#if 0
 for (i = 0; i < 1000; i+=10)
   {
     dc.MoveTo(0,i);
     dc.LineTo(1000,i);
     dc.MoveTo(i,0);
     dc.LineTo(i,1000);
   }
#endif
}

void CSrcSelWrap::OnSize(UINT nType, int cx, int cy) 
{
 CWnd::OnSize(nType, cx, cy);
 
}


const char * CSrcSelWrap::gettext(int n)
{
return sel.gettext(n);
 }

const char *CSrcSel::gettext(int n)
{
#ifdef __MFC4__
    return "FIXME!!";
#else
    return GetTabItem(n)->m_strCaption;
#endif
}
/////////////////////////////////////////////////////////////////////////////
// CTabView

IMPLEMENT_DYNCREATE(CTabView, CView)

CTabView::CTabView()
{
}

CTabView::~CTabView()
{

}


BEGIN_MESSAGE_MAP(CTabView, CView)
	//{{AFX_MSG_MAP(CTabView)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_NOTIFY(TCN_TABCHANGING, AFX_IDC_TAB_CONTROL, OnTabChanged) 
	ON_NOTIFY(TCN_TABCHANGED, AFX_IDC_TAB_CONTROL, OnTabChanged) 
	ON_WM_KEYDOWN()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CTabView drawing

void CTabView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
// sel.
}


/////////////////////////////////////////////////////////////////////////////
// CTabView message handlers


int CTabView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;
  CRect r(0,0,0,0);
  tabs.AddTab("<no source>");
  tabs.Create(WS_VISIBLE|WS_CHILD|WS_SYSMENU,r,this,  AFX_IDC_TAB_CONTROL);
	return 0;
}

void CTabView::OnInitialUpdate() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	CView::OnInitialUpdate();
}

void CTabView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
  static int depth;

#if 1
  if (!depth) {
		depth++;
		if (lHint != 0 
		    && lHint != -1
		    && lHint != -2)
		  {
		    parent->new_pc((CORE_ADDR)lHint);
		  }
		depth--;
	      }
#endif

  // Only do this for a major update
  if (lHint == 0) 
    {
      // Add all the tabs we need.
      CSrcD *doc = (CSrcD *)GetDocument();
      int newones = 0;
      int x = tabs.GetItemCount();
      
      POSITION p;
      p = doc->list.GetHeadPosition();
      while (p)
	{															 
	  CSrcFile * f = (CSrcFile *)doc->list.GetNext(p);
	  tabs.AddTab(f->title);
	  newones++;
	}
      // Delete original tabs if we've put more up
      int i;
      if (newones)
	for (i =0; i < x; i++)
	  tabs.RemoveTab(0);
    }
}

void CTabView::OnSize(UINT nType, int cx, int cy) 
{
  
  tabs.SetWindowPos(0,0,0,cx,cy,SWP_NOZORDER|SWP_NOMOVE);
}




void CTabView::OnTabChanged(NMHDR*, LRESULT*pResult) 
{
  parent->select_title(tabs.gettext(tabs.GetCurSel()));
  *pResult = 0;
}

void CTabView::deletecur()
{
  if (tabs.GetItemCount() > 1) 
    {
      CSrcD* pDoc = (CSrcD *)GetDocument();
      int i = tabs.GetCurSel();
      CSrcFile *p  = pDoc->lookup_by_index(i);
      if (p) {
	pDoc->remove_file(p);
	tabs.RemoveTab(i);
	parent->select_title(tabs.gettext(tabs.GetCurSel()));	
      }
    }
}

