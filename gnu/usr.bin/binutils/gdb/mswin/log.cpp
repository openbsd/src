// logview.cpp : implementation file

/* We save the logview as a standard editview, and we buffer
   up text from gdb until we get to an idle.  If we see a return
   from the user, then we suck up that line and treat it as input */

#include "stdafx.h"
#include "log.h"

extern CGuiApp theApp;

static void redraw_allcmdlogwins()
{ 
  redraw_allwins(theApp.m_CmdLogTemplate);
}
static void redraw_alliologwins()
{ 
  redraw_allwins(theApp.m_IOLogTemplate);
}

CFontInfo cmdlogview_fontinfo ("Command", redraw_allcmdlogwins);
CFontInfo iologview_fontinfo  ("IO", redraw_alliologwins);

IMPLEMENT_DYNCREATE(CGenericLogView, CEditView)



CIOLogView *iowinptr;
CCmdLogView *cmdwinptr;


CGenericLogView::CGenericLogView()
{
  *(getptr) = this;
  getfontinfo->MakeFont();
}

CGenericLogView::~CGenericLogView()
{
  *(getptr) = 0;
}


BEGIN_MESSAGE_MAP(CGenericLogView, CEditView)
	//{{AFX_MSG_MAP(CGenericLogView)
	ON_COMMAND(ID_REAL_CMD_BUTTON_SET_FONT, OnSetFont)
	ON_EN_CHANGE(AFX_IDW_PANE_FIRST, OnEditChange)
	ON_WM_SHOWWINDOW()
	ON_WM_KEYDOWN()
	ON_WM_DESTROY()
     ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()



void CGenericLogView::add(const char *s)
{
pending += s;
}

void CGenericLogView::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
}



void CGenericLogView::OnSetFont() 
{
  getfontinfo ->OnChooseFont();	
  SetFont(&(getfontinfo ->m_font));

}

BOOL CGenericLogView::PreCreateWindow(CREATESTRUCT& cs) 
{
  return CEditView::PreCreateWindow(cs);
}

void CGenericLogView::OnInitialUpdate() 
{
  SetFont(&(getfontinfo ->m_font));
  CEditView::OnInitialUpdate();
  load_where(GetParentFrame(), getfontinfo->windowname);
}


void CGenericLogView::OnDestroy() 
{
	save_where(GetParentFrame(), getfontinfo->windowname);
	CEditView::OnDestroy();
}

void CGenericLogView::OnShowWindow(BOOL bShow, UINT nStatus) 
{
  CEditView::OnShowWindow(bShow, nStatus);
}

static char line[100];
int lc;
int nl = 0;

void CGenericLogView::OnEditChange()
{
  CEditView::OnEditChange();
  if (nl) {
    char buf[200];
    static int j;
    nl = 0;
    lc = 0;
    sprintf(buf,"(gdb)  ", j++);
    togdb_command_from_tty(line+6);


    add (buf);
//	togdb_force_update ();
  }
}

void CGenericLogView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
  doidle();
  int i = GetEditCtrl().GetLineCount();
  int z = GetEditCtrl().GetLine(i-1,line, sizeof(line));
  
  if (isalpha(nChar))
    {
      int idx = GetEditCtrl().LineIndex(i-1);
      GetEditCtrl(). SetSel (idx + z, idx+z, TRUE);
    }
  CEditView::OnKeyDown(nChar, nRepCnt, nFlags);
  if (nChar == VK_RETURN)
    {
      /* Suck off last line */
      line[z] = 0;
      nl = 1;
    }
}

void CGenericLogView::OnIdle() 
{
/*	if (*getptr())
	getptr()->doidle();*/
}

void CGenericLogView::doidle()
{
  if (pending.GetLength()) 
    { 
      WINDOWPLACEMENT wnp;
      GetWindowPlacement (&wnp); 
      if (wnp.showCmd != SW_SHOWMINIMIZED) 
	{ 
	  int i;
	  while (pending.GetLength()) {
	    i = pending.Find('\n');
	    if (i >= 0)
	      {
		CString l = pending.Left(i);
		GetEditCtrl().SetSel(-1,-1,FALSE); 
		GetEditCtrl().ReplaceSel(l);
		GetEditCtrl().ReplaceSel("\r"); 
		GetEditCtrl().ReplaceSel("\n"); 
		pending = pending.Mid(i+1);
	      }
	    else {
	      /* Scan look for the newlines and change them */
	      GetEditCtrl().ReplaceSel(pending); 
	      pending ="";
	      return ;
	    }
	  }
	} 
    }
}


IMPLEMENT_DYNCREATE(CIOLogView, CGenericLogView)
IMPLEMENT_DYNCREATE(CCmdLogView, CGenericLogView)


void CCmdLogView::Initialize()
{
  cmdlogview_fontinfo.Initialize();
}
void CCmdLogView::Terminate() 
{
  cmdlogview_fontinfo.Terminate();
}

void CIOLogView::Initialize()
{
  iologview_fontinfo.Initialize();
}
void CIOLogView::Terminate() 
{
  iologview_fontinfo.Terminate();
}
		  


CGenericLogView::CGenericLogView(CGenericLogView **p, CFontInfo *f) 

{ 
*p = this; 
getptr = p; 
getfontinfo = f;}

CIOLogView::CIOLogView() 
: CGenericLogView ((CGenericLogView **)&iowinptr,   &iologview_fontinfo) 
{}
CCmdLogView::CCmdLogView() : CGenericLogView ((CGenericLogView **)&cmdwinptr, &cmdlogview_fontinfo)  {}



void CIOLogView_output (const char *s)
{
  if (!iowinptr)
    {
      /* Window doesn't exist!! make one */
	theApp.OnNewIOLogWin(); 
    }

  if (iowinptr)
    {
      iowinptr->add (s);
      iowinptr->doidle ();
    }
}
void CGenericLogView::OnSize(UINT nType, int cx, int cy) 
{
//  ::OnSize(nType, cx, cy);
  // TODO: Add your message handler code here
  GetWindowRect (&getfontinfo->where);
  SetFont(&(getfontinfo ->m_font));
}

