
// infofram.cpp : implementation file
//

#include "stdafx.h"
//#include "info.h"
#include "resource.h" /* dawn: don't think this one is needed! */

#include "ginfodoc.h"

#include "infofram.h"
#define BVIEWDEPTH 29
#define BVIEWWIDTH 200

/////////////////////////////////////////////////////////////////////////////
// CGnuInfoFrame


IMPLEMENT_DYNCREATE(CGnuInfoFrame, CMDIChildWnd)
IMPLEMENT_DYNCREATE(CGnuInfoSView, CScrollView)
IMPLEMENT_DYNCREATE(CGnuInfoList, CScrollView)

//#define new DEBUG_NEW

#if 0
static CFont info_font;
static int info_font_height;


static  LOGFONT m_lfFont;
static  LOGFONT m_lfFontOrig;

static char *szHeight = "Height";
static char *szWeight = "Weight";
static char *szItalic = "Italic";
static char *szUnderline = "Underline";
static char *szPitchAndFamily = "PitchAndFamily";
static char *szFaceName = "FaceName";
static char *szSystem = "System";


static void GetProfileFont(LPCSTR szSec, LOGFONT* plf)
{

  CWinApp* pApp = AfxGetApp();
  plf->lfHeight = pApp->GetProfileInt(szSec, szHeight, 0);
  if (plf->lfHeight != 0)
    {
      plf->lfWeight = pApp->GetProfileInt(szSec, szWeight, 0);
      plf->lfItalic = (BYTE)pApp->GetProfileInt(szSec, szItalic, 0);
      plf->lfUnderline = (BYTE)pApp->GetProfileInt(szSec, szUnderline, 0);
      plf->lfPitchAndFamily = (BYTE)pApp->GetProfileInt(szSec, szPitchAndFamily, 0);
     CString strFont = pApp->GetProfileString(szSec, szFaceName, szSystem);
      strncpy((char*)plf->lfFaceName, strFont, sizeof plf->lfFaceName);
      plf->lfFaceName[sizeof plf->lfFaceName-1] = 0;
    }
}
static void WriteProfileFont(LPCSTR szSec, 
			     const LOGFONT* plf,	
			     const LOGFONT *plfOrig)
{

  CWinApp* pApp = AfxGetApp();
  if (plf->lfHeight != plfOrig->lfHeight)
    pApp->WriteProfileInt(szSec, szHeight, plf->lfHeight);
  
  {
    if (plf->lfHeight != plfOrig->lfHeight)
      if (plf->lfHeight != plfOrig->lfHeight)
	pApp->WriteProfileInt(szSec, szHeight, plf->lfHeight);
    if (plf->lfWeight != plfOrig->lfWeight)
      pApp->WriteProfileInt(szSec, szWeight, plf->lfWeight);
    if (plf->lfItalic != plfOrig->lfItalic)
      pApp->WriteProfileInt(szSec, szItalic, plf->lfItalic);
    if (plf->lfUnderline != plfOrig->lfUnderline)
      pApp->WriteProfileInt(szSec, szUnderline, plf->lfUnderline);
    if (plf->lfPitchAndFamily != plfOrig->lfPitchAndFamily)
      pApp->WriteProfileInt(szSec, szPitchAndFamily, plf->lfPitchAndFamily);
    if (strcmp(plf->lfFaceName, plfOrig->lfFaceName) != 0)
      pApp->WriteProfileString(szSec, szFaceName, (LPCSTR)plf->lfFaceName);
  }
}

#endif
extern CGuiApp theApp;
void redraw_allinfowins()
{ 
redraw_allwins(theApp.m_infoTemplate);
}


CFontInfo infobrowser_fontinfo ("InfoBrowser", redraw_allinfowins);

CGnuInfoFrame::CGnuInfoFrame()
{
  current = 0;
  history_head = 0;
  history_ptr = 0;
  history_root = 0;
}

CGnuInfoFrame::~CGnuInfoFrame()
{

  CGnuInfoHist *p;
  CGnuInfoHist *q;
  for(p = history_root; p; p = q)
    {
      q = p->next;
      if (p == q)
	abort();
      delete p;
    }
}


BEGIN_MESSAGE_MAP(CGnuInfoFrame, CMDIChildWnd)
	//{{AFX_MSG_MAP(CGnuInfoFrame)
	ON_COMMAND(ID_REAL_CMD_BUTTON_BACKWARD, OnActionsBackward)
	ON_COMMAND(ID_REAL_CMD_BUTTON_ACTIONS_FORWARD, OnActionsForward)
	ON_COMMAND(ID_REAL_CMD_BUTTON_ACTIONS_NEXT, OnActionsNext)
	ON_COMMAND(ID_REAL_CMD_BUTTON_NEXT_PAG, OnActionsNextPage)
	ON_COMMAND(ID_REAL_CMD_BUTTON_PREV, OnActionsPrev)
	ON_COMMAND(ID_REAL_CMD_BUTTON_PREV_PAGE, OnActionsPrevPage)
	ON_COMMAND(ID_REAL_CMD_BUTTON_UP, OnActionsUp)
	ON_COMMAND(ID_REAL_CMD_BUTTON_VIEW_BOTH, OnActionsViewBoth)
	ON_COMMAND(ID_REAL_CMD_BUTTON_VIEW_CONTENTS, OnActionsViewContents)
	ON_COMMAND(ID_REAL_CMD_BUTTON_VIEW_PAGE, OnActionsViewPage)
//	ON_COMMAND(ID_REAL_CMD_BUTTON_VIEW_SET_FONT, OnViewSetFont)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_BACKWARD, OnUpdateBackward)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_ACTIONS_FORWARD, OnUpdateForward)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_ACTIONS_NEXT, OnUpdateNext)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_NEXT_PAG, OnUpdateActionsNextPage)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_PREV, OnUpdatePrev)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_PREV_PAGE, OnUpdateActionsPrevPage)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_UP, OnUpdateUp)
	ON_WM_CREATE()
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CGnuInfoFrame message handlers

static UINT BASED_CODE buttons[] =
{
	  ID_REAL_CMD_BUTTON_BACKWARD,
	  ID_REAL_CMD_BUTTON_ACTIONS_FORWARD,
  ID_SEPARATOR,
	  ID_REAL_CMD_BUTTON_UP,
	  ID_REAL_CMD_BUTTON_PREV,
	  ID_REAL_CMD_BUTTON_ACTIONS_NEXT,
  ID_SEPARATOR,
	  ID_REAL_CMD_BUTTON_PREV_PAGE,
	  ID_REAL_CMD_BUTTON_NEXT_PAG,
};

static UINT BASED_CODE split_buttons[] =
{
  ID_REAL_CMD_BUTTON_VIEW_BOTH,
  ID_REAL_CMD_BUTTON_VIEW_CONTENTS,
  ID_REAL_CMD_BUTTON_VIEW_PAGE
  };


BOOL CGnuInfoFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext) 
{

  CRect rectClient_text;

  GetClientRect(rectClient_text);	
  rectClient_text.top = 40;
  
  text_view.CreateStatic(this, 1,2);
  text_view.inited = 1;  
  text_view.CreateView (0,
			1,
			RUNTIME_CLASS(CGnuInfoSView),
			CSize(150,100),
			pContext);

  text_view.CreateView (0,
			0,
			RUNTIME_CLASS(CGnuInfoList),
			CSize(150,100), 
			pContext);
  
  text_view.inited = 1;
  list_view = (CGnuInfoList *)(text_view.GetPane(0,0));
  list_view->frame = this;
  
  scroll_view = (CGnuInfoSView *)(text_view.GetPane(0,1));
  scroll_view->frame = this;
  
  
  
  toolbar.Create(this, WS_CHILD|WS_VISIBLE|CBRS_BOTTOM);
  toolbar.LoadBitmap(ID_SYM_BITMAP_FRAMEINFO);
  toolbar.SetButtons(buttons, sizeof(buttons)/sizeof(buttons[0]));
  
  toolbar1.Create(this, WS_CHILD|WS_VISIBLE|CBRS_BOTTOM);
  toolbar1.LoadBitmap(ID_SYM_BITMAP_SPLIT);
  toolbar1.SetButtons(split_buttons, 3);
  
  return CMDIChildWnd::OnCreateClient(lpcs, pContext);
}

int CGnuInfoFrame::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{

  if (CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  return 0;
}


void CGnuInfoFrame::RecalcLayout(BOOL bNotify) 
{

  // TODO: Add your specialized code here and/or call the base class
  // split into two parts
  
  CRect rectClient;
  GetClientRect(rectClient);  
  int ything =  HIWORD(GetDialogBaseUnits());
  
  int gap = rectClient.bottom - BVIEWDEPTH;
  
  int indent = rectClient.right / 2 - BVIEWWIDTH / 2;
  text_view.SetWindowPos(&wndTop, 0, 0,
			 rectClient.right,
			 gap , SWP_SHOWWINDOW);
  
  toolbar.MoveWindow(indent, gap, rectClient.right, rectClient.bottom - gap);
  toolbar1.MoveWindow(0, gap, indent, rectClient.bottom - gap);
  
  Invalidate();
  

}

static void doit(CCmdUI *p, char *name, int val, int *prev)
{

  char buf[20];
  if (val != *prev||1) {
    if (val == -1)
      p->Enable(FALSE);
    else {
      p->Enable();
      sprintf(buf,"%s %d",name, val);
      p->SetText(buf);
    }
    *prev = val;
  }
}  
extern CGuiApp theApp;
void redraw_alldocwins()
{ 
 class CMultiDocTemplate *p = theApp.m_infoTemplate;
 POSITION pos = p->GetFirstDocPosition();

 while (pos)
   {
     CGnuInfoDoc *doc = (CGnuInfoDoc *)(p->GetNextDoc(pos));
     doc->UpdateAllViews(NULL);
   }
}

int CGnuInfoFrame::OkNode (CGnuInfoNode *n)
{

  if (n == 0) return 0;
  if (n->file == 0) return 0;
  return 1;
}

void CGnuInfoFrame::Initialize()
{
	infobrowser_fontinfo.Initialize();
#if 0
  GetProfileFont ("INFO", &m_lfFont);
  m_lfFontOrig = m_lfFont;
  info_font.CreateFontIndirect(&m_lfFont);
 #endif
}

void CGnuInfoFrame::Terminate()
{
infobrowser_fontinfo.Terminate();
#if 0
  WriteProfileFont ("INFO", &m_lfFont, &m_lfFontOrig);
#endif


}


CGnuInfoNode * CGnuInfoFrame::GetCurrentNode()
{
if(doc()) {
  if (!OkNode(current)) 
    {
      current = doc()->nodes;
      if (!current)
	{
	  /* Invent a node */

	  current = new CGnuInfoNode("EMPTY");

	  
	}
    }
	}
  return current;
}


void CGnuInfoFrame::OnActionsNext() 
{
  SetCurrent(GetCurrentNode()->next, 1);
}

void CGnuInfoFrame::OnActionsPrev() 
{
  SetCurrent(GetCurrentNode()->prev, 1);
  
}


void CGnuInfoFrame::OnActionsUp() 
{
  SetCurrent(GetCurrentNode()->up, 1);  
}

void CGnuInfoFrame::OnActionsForward()
{
  if (history_ptr && history_ptr->next)
    {
      history_ptr = history_ptr->next;
      SetCurrent (history_ptr->node, 0);
    }
}

void CGnuInfoFrame::OnActionsBackward()
{
  if (history_ptr && history_ptr->prev)
    {
      history_ptr = history_ptr->prev;
      SetCurrent (history_ptr->node, 0);
    }
  
}
CGnuInfoHist::CGnuInfoHist(CGnuInfoNode *n, CGnuInfoHist *p)
{
  if (p)
    p->next = this;
  node = n;
  prev = p;
  next = 0;
}
void CGnuInfoFrame::RememberPage(CGnuInfoNode *v)
{
  history_ptr = history_head = new CGnuInfoHist (v, history_head);
  if (!history_root)
    history_root = history_head;
}


void CGnuInfoFrame::GotoNode(CString name)
{
  if (!name.GetLength() == 0)
    {
      if (name[0] == '(')
	{
	  /* This is a file node */
	  extern CWinApp *GetTheApp();
	  CString buf = name.Mid(1);
	  buf = name.Mid(1, name.GetLength() -2);
	  /* Stick on a suffix if not there */
	  if (buf.Find(".inf")< 0)
	    buf += ".inf";
	  GetTheApp()->OpenDocumentFile (buf);
	}
      else {

	CGnuInfoNode *x = doc()->LookupNode(name);
	SetCurrent (x, 1);


      }
    }
}

void CGnuInfoFrame::SetCurrent(CGnuInfoNode *ptr, int remember)
{
  if (OkNode(ptr)){
    current = ptr;
    if (remember)
      RememberPage(current);
    list_view->Sync();
    Invalidate();
  }
  
}
void CGnuInfoFrame::OnUpdateBackward(CCmdUI *p)
{
  p->Enable(history_ptr && history_ptr->prev);
}


void CGnuInfoFrame::OnUpdateForward(CCmdUI *p)
{


  p->Enable(history_ptr && history_ptr->next);
}

CGnuInfoDoc *CGnuInfoFrame::doc()
{
  return scroll_view->doc();
}

void CGnuInfoFrame::OnUpdateUp(CCmdUI *p)
{
if (doc()) 
  p->Enable(OkNode(GetCurrentNode()->up));
}


void CGnuInfoFrame::OnUpdateNext(CCmdUI *p)
{
if (doc()) 
  p->Enable(OkNode(GetCurrentNode()->next));
}


void CGnuInfoFrame::OnUpdatePrev(CCmdUI *p)
{
if (doc()) 
  p->Enable(OkNode(GetCurrentNode()->prev));
}

void CGnuInfoFrame::splitdo(int x)
{
  CRect r;
  GetClientRect(r);
  
  int width = r.Width() ;
  int wsplit = x * width / 8;
  int wpage = (8-x) * width / 8;
  
  text_view.SetColumnInfo(0, wsplit, 0);
  text_view.SetColumnInfo(1, wpage, 0);
  text_view.RecalcLayout();
}

void CGnuInfoFrame::OnActionsViewBoth() 
{
  splitdo(2);
}

void CGnuInfoFrame::OnActionsViewPage() 
{
  splitdo(0);
}

void CGnuInfoFrame::OnActionsViewContents() 
{
  splitdo(8);
}

int CGnuInfoFrame::okpage(int x)
{
  int want = GetCurrentNode()->pagenumber + x;
  return  (want >= 0 && want < doc()->npages);
  
}

void CGnuInfoFrame::wantpage(int x)
{
  int want = GetCurrentNode()->pagenumber + x;
  if (want >= 0 && want < doc()->npages)
    {

      GotoNode (doc()->GetPageName(want));

    }
}
void CGnuInfoFrame::OnActionsNextPage() 
{
  wantpage (1);
}

void CGnuInfoFrame::OnUpdateActionsNextPage(CCmdUI* pCmdUI) 
{ 
if (doc()) 
  pCmdUI->Enable (okpage(1));
}

void CGnuInfoFrame::OnActionsPrevPage() 
{ 
  wantpage(-1);
}

void CGnuInfoFrame::OnUpdateActionsPrevPage(CCmdUI* pCmdUI) 
{ 
if (doc())
  pCmdUI->Enable (okpage(-1));
}



 CGnuInfoSView::CGnuInfoSView()
{ 
  intext_node = 0;
  frame = (CGnuInfoFrame *)GetParentFrame();
}
CGnuInfoSView::~CGnuInfoSView()
{ 
  
}


BEGIN_MESSAGE_MAP(CGnuInfoSView, CScrollView)
     //{{AFX_MSG_MAP(CGnuInfoSView)
     ON_WM_KEYDOWN()
     ON_WM_LBUTTONDOWN() 
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CGnuInfoSView drawing
     
     void CGnuInfoSView::OnInitialUpdate()
{ 
  CScrollView::OnInitialUpdate();
  
  CSize sizeTotal;
  // TODO: calculate the total size of this view
  sizeTotal.cx = 100;
  sizeTotal.cy = 1000;
  if (doc()) {
    if (doc()->npages <3) {
      sizeTotal.cx = 0;
      frame = (CGnuInfoFrame *)GetParentFrame();
      frame->splitdo(0);
    }
    
  }
  SetScrollSizes(MM_TEXT, sizeTotal);
}

void CGnuInfoSView::AddSpot(int line, const char *nodename)
{ 
  if (n_hotspots < MAXHOTS) 
    {
      hotspots[n_hotspots].line = line;
      hotspots[n_hotspots].name = nodename;
      n_hotspots++;
    }
}
/*
   
   A hotspot is a 
   <nodename>::<text> thing
   or
   <text>:<nodename> thing
   
   <nodename>:= textofnode
   <nodename>:= ( textofnodefile )
   
   remember nodename, print in green  and print text in original color */


void CGnuInfoSView::ParseNode (const char *src, 
			       int line,
			       char *before,
			       char *after,
			       char *node,
			       int *type)
{ 
  char *onode = node;
  char *oafter = after;
  while (*src == ' ' 
	 || *src == '\t')
    *before = *src++;
  *before = 0;
  
  while (*src != ':' && *src)
    *node++ = *src++;
  *node++ = 0;
  
  if (*src == 0) {
    /* ahh, got to the end of the line, lets read the next one ... */
    src = text.ElementAt(line+1);
  }
  if (src[0] == ':' && src[1] == ':') 
    {
      /* We guessed right, we had the node:: text form */
      src+=2; // skip the colons 
      while (*src) 
	*after++ = *src++;
      *after++= 0;
      *type = 2;
    }
  else 
    {
      /* Oops, only had the single colon form, so the string
	 was text and the node follows */
      strcpy(after, onode);
      after += node -onode -1 ;
      node = onode;
      src++;
      while (*src == ' '
	     || *src == '\t')
	src++;
      while (*src && *src != '.') {
	*node++ = *src++;
	*after++ = ' ';
      }
      src++;
      while (*src)
	{
	  *after++ = *src++;
	}
      *after = 0;
      *node = 0;
      *type = 1;
    }
  
}

void CGnuInfoSView::PrintAHotSpot (int li, 
				   CDC *pDC, 
				   int x, 
				   int y, 
				   const CString &line)
{ 
  char after[200];
  char before[200];
  char node[200];
  int type;
  
  ParseNode(line, li, before, after, node, &type);
  
  AddSpot(li, node);
  
  x += pDC->TabbedTextOut (x, y, before, strlen(before), 0,0,0).cx;
  if (type == 2) 
    {
      COLORREF old = pDC->SetTextColor (RGB(50,100,50));
      x += pDC->TabbedTextOut (x, y, node, strlen(node), 0,0,0).cx;
      pDC->SetTextColor (old);
      pDC->TabbedTextOut (x, y, after, strlen(after), 0,0,0);
    }
  else if (type == 1)
    {
      COLORREF old = pDC->SetTextColor (RGB(50,100,50));
      pDC->TabbedTextOut (x, y, after, strlen(after), 0,0,0);
      pDC->SetTextColor (old);
    }
  
  
}
void CGnuInfoSView::OnDraw(CDC* pDC)
{ 
  
  CDocument* pDoc = GetDocument();
  CRect rect;
  CPoint tlc = GetScrollPosition();
  int y;
  int indent = 5;
  CGnuInfoDoc *id = (CGnuInfoDoc *)pDoc;
  
  GetClientRect(&rect);
  CSize sizeTotal;
#if 0
  pDC->SelectObject(&info_font);
#else
  pDC->SelectObject(&infobrowser_fontinfo.m_font);  
#endif
  CSize one_char= pDC->GetTextExtent("*", 1);
  
  if (GetCurrentNode() != intext_node) {
    id->GetPage(GetCurrentNode(), text);
    intext_node = GetCurrentNode();
    tlc.x = 0;
    tlc.y = 0;
    ScrollToPosition (tlc);
    
    y = 0;
    
    maxcx = 0;
    maxcy = 0;
    /* Calc max text width */
    int i;
    
    for (i = 0; i < text.GetSize(); i++)
      {
	CString &line = text.ElementAt(i);
	int llen = line.GetLength();
	CSize  one_line = pDC->GetTextExtent (line,llen);
	one_line.cx += indent;
	if (one_line.cx > maxcx)
	  maxcx = one_line.cx;
	maxcy += one_line.cy;
      }
  }
  
  font_height = one_char.cy;
  sizeTotal.cx = maxcx;
  sizeTotal.cy = (text.GetSize()+10) * one_char.cy;
  SetScrollSizes(MM_TEXT, sizeTotal);
  
  int from = tlc.y / one_char.cy - 2;
  int to =   (tlc.y + rect.Height()) / one_char.cy + 1;
  
  if (from < 0)
    from = 0;
  if (to > text.GetSize())
    to = text.GetSize();
  
  y = from * one_char.cy;
  int fy = 0;
  n_hotspots= 0;
  
  for (int i = from ; i < to; i++)
    {
      CString &line = text.ElementAt(i);
      int llen = line.GetLength();
      int notex;
      if ((notex = line.Find ("* Menu:")) == 0)
	{
	  pDC->TextOut(indent/2,y,"Menu");
	}
      else if ((notex = line.Find ("* ")) == 0)
	{
	  /* lines starting with this are node things */
	  PrintAHotSpot (i, pDC, indent, y, line.Mid (2));
	}
      else if ((notex = line.Find("*note ")) >= 0
	       || (notex = line.Find("*Note ")) >= 0)
	{
	  /* Hmm, the line has  a *note inside it, which will
	     be a real pain to emit */
	  CSize upto;
	  upto = pDC->TabbedTextOut(indent, y, line, notex,0,0,0);
	  /* Change the color and print the string */
	  PrintAHotSpot(i, pDC, upto.cx, y, line.Mid(notex+5));
	}
      else
	{
	  pDC->TabbedTextOut(indent,y,line,llen,0,0,0);
	}
      y += one_char.cy	;
      fy += one_char.cy	;
    }
  
}




/////////////////////////////////////////////////////////////////////////////
// CGnuInfoSView message handlers

void CGnuInfoSView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{ 
  
  int i;
  switch (nChar)
    {
    case VK_HOME:
      i = SB_TOP;
      break;
    case VK_END:
      i = SB_BOTTOM;
      break;
    case VK_PRIOR:
      i = SB_PAGEUP;
      break;
      
    case VK_NEXT:
      i = SB_PAGEDOWN;
      break;
    default:
      CScrollView::OnKeyDown(nChar, nRepCnt, nFlags);
      return ;
    }
  OnVScroll(i,   0,	   GetScrollBarCtrl(SB_VERT));
}


CString CGnuInfoSView::FindSpot(int n)
{ 
  int i;
  for (i= 0; i < n_hotspots; i++)
    {
      if (      n ==hotspots[i].line) return
	hotspots[i].name;
      
    }
  return "";
}
void CGnuInfoSView::OnLButtonDown(UINT nFlags, CPoint point) 
{ 
  
  CPoint lhs = GetScrollPosition();
  point.y += lhs.y;
  CString name = FindSpot (point.y/ font_height);
  frame->GotoNode(name);
  
}

CGnuInfoNode *CGnuInfoSView::GetCurrentNode() 
{ 
  frame = (CGnuInfoFrame *)GetParentFrame();
  return frame->GetCurrentNode();
}


/////////////////////////////////////////////////////////////////////////////
// CGnuInfoList



CGnuInfoList::CGnuInfoList()
{ 
  
}

CGnuInfoList::~CGnuInfoList()
{ 
}


BEGIN_MESSAGE_MAP(CGnuInfoList, CScrollView)
     //{{AFX_MSG_MAP(CGnuInfoList)
     // NOTE - the ClassWizard will add and remove mapping macros here.
     ON_WM_LBUTTONDOWN()
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CGnuInfoList drawing
     
     void CGnuInfoList::OnInitialUpdate()
{ 
  CScrollView::OnInitialUpdate();
  
  CSize sizeTotal;
  if (doc()) 
    {
      // TODO: calculate the total size of this view
      sizeTotal.cx = sizeTotal.cy = 100;
      sizeTotal.cy = 	doc()->npages * 20;
      
      SetScrollSizes(MM_TEXT, sizeTotal);
    }
}

void CGnuInfoList::OnDraw(CDC* pDC)
{ 
  int i;
  
  CGnuInfoDoc *d = doc();
  CSize sizeTotal;
  pDC->SelectObject(&infobrowser_fontinfo.m_font);
  CSize one_char= pDC->GetTextExtent("*", 1);
  sizeTotal.cx = sizeTotal.cy = 100;
  sizeTotal.cy = d->npages * one_char.cy;
  SetScrollSizes(MM_TEXT, sizeTotal);  
  font_height = one_char.cy;
  CGnuInfoNode *cnode = GetCurrentNode();
  for (i = 0; i < d->npages; i++) {
    CString name;
    CGnuInfoNode *n = d->GetNode (i);
    if (n) 
      {
	COLORREF old;
	if (cnode == n)
	  old = pDC->SetTextColor (RGB(200,50,50));
	
	pDC->TextOut(one_char.cx *2* n->GetPageDepth(),
		     i*one_char.cy,
		     n->name);
	if (cnode == n)
	  pDC->SetTextColor(old);
      }
  }
  
}

/////////////////////////////////////////////////////////////////////////////
// CGnuInfoList diagnostics




/////////////////////////////////////////////////////////////////////////////
// CGnuInfoList message handlers

class CGnuInfoNode *CGnuInfoList::GetCurrentNode()
{ 
  frame = (CGnuInfoFrame *)GetParentFrame();
  return  frame->GetCurrentNode(); 
}

void CGnuInfoList::OnLButtonDown(UINT nFlags, CPoint point) 
{ 
  
  CPoint lhs = GetScrollPosition();
  point.y += lhs.y;
  int line = point.y / font_height;
  frame->GotoNode(doc()->GetNode(line)->name);
  
}


/* Make sure that the current node is visible */
void CGnuInfoList::Sync()
{ 
  
  CRect screen;
  GetClientRect (screen);
  CPoint tlc = GetScrollPosition();
  int where = GetCurrentNode()->pagenumber  * font_height;
  
  if (where < tlc.y || where > tlc.y + screen.Height())
    {
      CPoint x;
      where -= screen.Height() /2 ;
      x.x = 0;
      x.y =  where;
      ScrollToPosition (x);
    }
  
}

/////////////////////////////////////////////////////////////////////////////
// CMySplitterWnd

IMPLEMENT_DYNCREATE(CMySplitterWnd, CSplitterWnd)
     
     CMySplitterWnd::CMySplitterWnd()
{
  /* The inited thing is 'cause the framework calls OnSize before everything's 
     been initialzed, which blows chunks */
  inited = 0;
}

CMySplitterWnd::~CMySplitterWnd()
{
}



BEGIN_MESSAGE_MAP(CMySplitterWnd, CSplitterWnd)
     //{{AFX_MSG_MAP(CMySplitterWnd)
     ON_WM_SIZE()
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CMySplitterWnd message handlers
     
     void CMySplitterWnd::OnSize(UINT nType, int cx, int cy) 
{
  if (inited) {
    CSplitterWnd::OnSize(nType, cx, cy);
  }
}

