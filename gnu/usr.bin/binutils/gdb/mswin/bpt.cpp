// bpt.cpp : implementation file
//

#include "stdafx.h"
 
#include "bpt.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CBpt

IMPLEMENT_DYNCREATE(CBpt, CView)
/* There is only one of these, perhaps it should belong
   to the app  ? */
CBreakInfoList the_breakinfo_list;

extern CGuiApp theApp;
void redraw_allbptwins()
{ 
  redraw_allwins(theApp.m_bptTemplate);
}


CFontInfo bptfontinfo ("BptFont", redraw_allbptwins);
CFontInfo buttonfontinfo ("ButtonFont", redraw_allbptwins);
CBpt::CBpt()
{
 inchanged = 0;
}

CBpt::~CBpt()
{
}


BEGIN_MESSAGE_MAP(CBpt, CView)
	//{{AFX_MSG_MAP(CBpt)
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_COMMAND(ID_REAL_CMD_BUTTON_SET_BREAKPOINT, OnSetBreak)
	ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_SET_BREAKPOINT, OnUpdateSetBreak)
     ON_BN_CLICKED(ID_CMD_BUTTON_CLEAR_ALL, OnClearAll)
     ON_LBN_SELCHANGE(ID_CMD_BUTTON_BREAKPOINT_LIST, on_list_changed)
     ON_LBN_DBLCLK(ID_CMD_BUTTON_BREAKPOINT_LIST, OnDblclkBreakpointList)
     ON_BN_CLICKED(ID_CMD_BUTTON_CLEAR_BREAKPOINT, OnClearBreakpoint)
     ON_EN_CHANGE(ID_CMD_BUTTON_BREAKPOINT, on_edit_changed)
     ON_BN_CLICKED(ID_CMD_BUTTON_DISABLE, OnDisable)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CBpt drawing

void CBpt::OnDraw(CDC* pDC)
{
  CDocument* pDoc = GetDocument();
  list.SetFont(&bptfontinfo.m_font, TRUE);
  edit.SetFont(&bptfontinfo.m_font, TRUE);
  for (int i= 0; i < 4; i++)
    buttons[i].SetFont(&buttonfontinfo.m_font, TRUE);
}


/////////////////////////////////////////////////////////////////////////////
// CBpt message handlers

BOOL CBpt::Create(LPCTSTR lpszClassName, LPCTSTR lpszWindowName, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID, CCreateContext* pContext) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CWnd::Create(lpszClassName, lpszWindowName, dwStyle, rect, pParentWnd, nID, pContext);
}

BOOL CBpt::DestroyWindow() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CView::DestroyWindow();
}

BOOL CBpt::OnEraseBkgnd(CDC* pDC) 
{
 // TODO: Add your message handler code here and/or call default
 CBrush b (RGB(192,192,192));
 CBrush *old = pDC->SelectObject(&b);
 CRect rect;
 pDC->GetClipBox(&rect);
 pDC->PatBlt(rect.left, rect.top,rect.Width(), rect.Height(), PATCOPY);
 pDC->SelectObject(old);
 return TRUE;	
}

#define R(x) x.left, x.top, x.Width(), x.Height()
void CBpt::OnSize(UINT nType, int cx, int cy) 
{
  CRect wholebox;
  CRect shrunkbox;
  int dx = bptfontinfo.dunits.cx;
  int dy = bptfontinfo.dunits.cy;
 
  int bx = buttonfontinfo.dunits.cx;
  int by = buttonfontinfo.dunits.cy;
 
  int hgap = dy ;
  int indent = dx;
  GetClientRect(wholebox);
  shrunkbox = wholebox;
  shrunkbox.InflateRect(-indent, -hgap);
 
  CRect editbox = shrunkbox;
  editbox.bottom = editbox.top + dy *2;
  edit.SetWindowPos (NULL, R(editbox), 0);
 
  CRect listbox = shrunkbox;
  int bwidth = bx * 14;
  listbox.top    = editbox.bottom + hgap;
  listbox.right = shrunkbox.right - bwidth;
  listbox.bottom = shrunkbox.bottom;
  list.SetWindowPos (NULL, R(listbox), 0);
 
  /* Set, clear, disable, clearall on the bottom row */
  CRect boxx = listbox;
  boxx.left = shrunkbox.right - bwidth +hgap;
  boxx.right = shrunkbox.right;
  int boxheight = by * 2;
  int boxgap = by * 3;
  for (int i = 0; i < 4; i++) 
    {
      boxx.top = listbox.top + i * boxgap;
      boxx.bottom = boxx.top + boxheight;
      buttons[i].SetWindowPos(0,R(boxx),0);
    }
}

int CBpt::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  CRect rc (10,10,100,100);
 
  if (CView::OnCreate(lpCreateStruct) == -1)
    return -1;
 
  int i;
  buttons[0].Create("&Set",WS_TABSTOP| BS_DEFPUSHBUTTON|WS_CHILD|WS_VISIBLE,	rc, this, ID_REAL_CMD_BUTTON_SET_BREAKPOINT);	
  buttons[1].Create("&Clear",WS_TABSTOP|WS_CHILD|WS_VISIBLE,	rc, this, ID_CMD_BUTTON_CLEAR_BREAKPOINT);	
  buttons[2].Create("&Disable",WS_TABSTOP|WS_CHILD|WS_VISIBLE,	rc, this, ID_CMD_BUTTON_DISABLE);	
  buttons[3].Create("Clear &All",WS_TABSTOP|WS_CHILD|WS_VISIBLE ,	rc, this, ID_CMD_BUTTON_CLEAR_ALL);
 
  list.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL
	      |WS_BORDER
	      | LBS_DISABLENOSCROLL | LBS_USETABSTOPS | WS_TABSTOP
	      | LBS_NOTIFY,
	      rc,
	      this,
	      ID_CMD_BUTTON_BREAKPOINT_LIST);
 
  edit.Create(WS_TABSTOP|WS_CHILD|WS_VISIBLE|WS_BORDER, rc, this, ID_CMD_BUTTON_BREAKPOINT);  
 
  return 0;
}

void CBpt::OnSetBreak() 
{
  CString c;
  edit.GetWindowText(c);
  togdb_bpt_set ((const char *)c);
	  edit.SetWindowText("");
  update_list();
  update_buttons();
}

void CBpt::OnUpdateSetBreak(CCmdUI* pCmdUI) 
{
  pCmdUI->Enable(togdb_target_has_execution());	
}

void CBpt::Initialize()
{
  bptfontinfo.Initialize();
  buttonfontinfo.Initialize();
}
void CBpt::Terminate()
{
  bptfontinfo.Terminate();
  buttonfontinfo.Terminate();
}

void CBpt::update_list()
{
  char buf[200];
  
  int cursel = list.GetCurSel();

  static int tstops[] = {10,20,30,40};

  list.ResetContent();	
  list.SetTabStops (4,tstops);
  int i;
  int size = the_breakinfo_list.GetSize();
  for (i = 0; i < size; i++) {
    char buf[200];
    CBreakInfo *bi = the_breakinfo_list.GetAt(i);
    if (bi->GetAddress()) {
      sprintf(buf,"%s\t%d\t%s\t%s",
	      bi->GetEnable() ? "+" : "-",
	      i,
	      paddr(bi->GetAddress()),
	      bi->GetAddrString());
      list.AddString(buf);
    }
  }
  list.SetCurSel(cursel);  

  if (cursel >= 0 && cursel < list.GetCount())
    {
      CBreakInfo *bi = the_breakinfo_list.GetAt(cursel);
      sprintf(buf,"%s:%d",
	      the_breakinfo_list.GetAt(cursel)->GetSourceFile(),
	      the_breakinfo_list.GetAt(cursel)->GetLineNumber());
      edit.SetWindowText(buf);
    }
}

void CBpt::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
  update_list();
  update_buttons();
}

void CBpt::update_buttons()
{
  /* Set the set button correctly */
  CString c;
  int cursel = list.GetCurSel();
  edit.GetWindowText(c);
  const char *s = (const char *)c;
  buttons[B_SETBUTTON].EnableWindow (strlen(s));
  buttons[B_DISABLE].EnableWindow(cursel >= 0);
  buttons[B_CLEAR].EnableWindow(cursel >= 0);
  buttons[B_CLEARALL].EnableWindow(the_breakinfo_list.GetSize() > 0);

  if (cursel >= 0) {
    CBreakInfo *bi = the_breakinfo_list.GetAt(cursel);
    if (bi->GetEnable())
      buttons[B_DISABLE].SetWindowText("&Disable");
    else
      buttons[B_DISABLE].SetWindowText("&Enable");
  }
}

void CBpt::on_edit_changed()
{
  update_buttons();
}
void CBpt::on_list_changed()
{
update_list();
  update_buttons();
}


void CBpt::OnDblclkBreakpointList() 
{
  int cursel = list.GetCurSel();
  if (cursel >= 0) {
    CBreakInfo *bi = the_breakinfo_list.GetAt(cursel);
    /* goto where this line is */
    theApp_show_at (bi->GetAddress());
  }
}
void CBpt::OnClearAll() 
{
  the_breakinfo_list.DeleteAll();
  update_list();
  update_buttons();
}

void CBpt::OnClearBreakpoint() 
{
  int cursel = list.GetCurSel();
  if (cursel >= 0) {
    the_breakinfo_list.Delete(cursel);
    if (cursel < list.GetCount()) {
    list.SetCurSel(cursel);
  }
  }
  update_list();
  update_buttons();
}

void CBpt::OnDisable() 
{
  int cursel =list.GetCurSel();
  if (cursel >= 0) 
    {
      CBreakInfo *bi = the_breakinfo_list.GetAt(cursel);
      if (bi->GetEnable())
	the_breakinfo_list.Disable(cursel);
      else
	the_breakinfo_list.Enable(cursel);
    }
  update_list();
  update_buttons();
}

void CBpt::OnInitialUpdate() 
{
 GetParentFrame()->SetWindowPos(0,0,0,
				bptfontinfo.dunits.cx * 45,
				bptfontinfo.dunits.cy * 30,
				SWP_NOMOVE);
 CView::OnInitialUpdate();
}

