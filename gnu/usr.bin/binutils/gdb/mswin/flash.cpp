#if 0
// flash.cpp : implementation file
//

#include "stdafx.h"
#include "flash.h"
#include "aboutbox.h"
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

static CFontInfo fontinfo  ("Flash");
/////////////////////////////////////////////////////////////////////////////
// CFlash

IMPLEMENT_DYNCREATE(CFlash, CView)

static char *string = 0;
CFlash *last = 0;
CFlash::CFlash()
{
last = this;
}

CFlash::~CFlash()
{
last = 0;
}


BEGIN_MESSAGE_MAP(CFlash, CView)
	//{{AFX_MSG_MAP(CFlash)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_COMMAND(ID_REAL_CMD_BUTTON_SET_FONT, OnSetFont)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CFlash drawing

void CFlash::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();

if (string) {
pDC->TextOut(0,0,string);
}
}


/////////////////////////////////////////////////////////////////////////////
// CFlash message handlers

void CFlash::Initialize()
{
  fontinfo.Initialize();
}

void CFlash::Terminate()
{
  fontinfo.Terminate();
}
BOOL CFlash::PreCreateWindow(CREATESTRUCT& cs) 
{
 // TODO: Add your specialized code here and/or call the base class

  return CView::PreCreateWindow(cs);
}

void CFlash::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// TODO: Add your specialized code here and/or call the base class

}

extern CGuiApp theApp;
void CFlash::Flash(const char *name)
{
  if (string)
    free(string);
  string = strdup (name);
  if (!last) {
    theApp.m_flashTemplate->OpenDocumentFile(NULL);
  }
  /* Trim off any non symbol stuff */
  char *p = string;
  const char *q;
  
  while (*p && !isalpha(*p))
    p++;
  char*s = p;
  
  /* Then go till it's no longer an id */
  while (*s) 
    {
      if (!isalpha(*s) 	&& ! isdigit(*s) && *s != '_'&& *s != '.') {
	*s = 0;
	break;
      }
      s++;
    }
  
  
  q = p;
  last->m_data.AddString(strdup(q));
  last->Invalidate();
}

BOOL CFlash::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
  // TODO: Add your specialized code here and/or call the base class
  
  return CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
}

int CFlash::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  if (CView::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  m_data.Create(LBS_OWNERDRAWFIXED|WS_HSCROLL|WS_BORDER|WS_CHILD|WS_VISIBLE|WS_VSCROLL,
		CRect(0,0,0,0), this, 0);
  return 0;
}

void CFlash::OnSize(UINT nType, int cx, int cy) 
{
  CView::OnSize(nType, cx, cy);
  

  CRect cl;
  GetClientRect(cl);
  m_data.SetWindowPos(NULL,
		      cl.left, cl.top ,
		      cl.right - cl.left,
		      cl.bottom - cl.top ,
		      SWP_DRAWFRAME|SWP_NOZORDER);
}
/////////////////////////////////////////////////////////////////////////////
// CFlashItem

CFlashItem::CFlashItem()
{
}

CFlashItem::~CFlashItem()
{
}


BEGIN_MESSAGE_MAP(CFlashItem, CListBox)
     //{{AFX_MSG_MAP(CFlashItem)
     // NOTE - the ClassWizard will add and remove mapping macros here.
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CFlashItem message handlers
void showsymbol(CDC *p,int x, int y, GdbSymbol *symbol);     
 void CFlashItem::DrawItem(LPDRAWITEMSTRUCT lp)
{
  char bu[200];	
  union gui_symtab *ppp;
  CDC* pDC = CDC::FromHandle(lp->hDC);	
  pDC->SelectObject(&fontinfo.m_font);
  int x = lp->rcItem.left;
  int y = lp->rcItem.top;
  CString p = (char  *)GetItemData(lp->itemID);	
  pDC->TextOut(x,y,p);
   const char *s = p;
  GdbSymbol *sym = GdbSymbol::Lookup(s);
if (sym) {
showsymbol(pDC,x,y,sym);
}
else {
  sprintf(bu,"%s not available", s);
  pDC->TextOut(x,y,bu);
}
}

void CFlashItem::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) 
{
  // TODO: Add your code to determine the size of specified item
  
}

void CFlash::OnInitialUpdate() 
{
  // TODO: Add your specialized code here and/or call the base class
  //  fontinfo.HadInitialUpdate(this);
  CView::OnInitialUpdate();
}

void CFlash::OnSetFont() 
{
  fontinfo.OnChooseFont();
}
/////////////////////////////////////////////////////////////////////////////
// CFlashFrame

IMPLEMENT_DYNCREATE(CFlashFrame, CMDIChildWnd)

CFlashFrame::CFlashFrame()
{
}

CFlashFrame::~CFlashFrame()
{
}


BEGIN_MESSAGE_MAP(CFlashFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CFlashFrame)
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CFlashFrame message handlers

BOOL CFlashFrame::PreCreateWindow(CREATESTRUCT& cs) 
{
	// TODO: Add your specialized code here and/or call the base class
  fontinfo.SetUp(cs);			
	return CMDIChildWnd::PreCreateWindow(cs);
}

void CFlashFrame::OnSize(UINT nType, int cx, int cy) 
{
	CMDIChildWnd::OnSize(nType, cx, cy);
    fontinfo.RememberSize(this);
	// TODO: Add your message handler code here
	
}
#endif
