
// srcb.cpp : implementation file
//

#include "stdafx.h"

#include "browserl.h"
#include "srcb.h"
#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


extern CGuiApp theApp;
void redraw_allsrcbwins()
{ 
  redraw_allwins(theApp.m_srcbrowserTemplate);
}
CFontInfo srcbfontinfo ("SrcBrowserFont", redraw_allsrcbwins);
extern CFontInfo buttonfontinfo;
/////////////////////////////////////////////////////////////////////////////
// SrcB

IMPLEMENT_DYNCREATE(CSrcB, CView)

CSrcB::CSrcB()
{
showall = 1;
}

CSrcB::~CSrcB()
{
}


BEGIN_MESSAGE_MAP(CSrcB, CView)
	//{{AFX_MSG_MAP(CSrcB)
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_CREATE()
	ON_BN_CLICKED(ID_CMD_BUTTON_GOTO,on_goto)
	ON_LBN_DBLCLK(ID_CMD_BUTTON_BROWSE_LIST, on_dblclkbrowselist)
	ON_BN_CLICKED(ID_CMD_BUTTON_BREAKPOINT, on_breakpoint)
	ON_BN_CLICKED(ID_CMD_BUTTON_EXPLODE, on_explode)
	ON_EN_CHANGE(ID_CMD_BUTTON_FILTER, on_change_filter)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CSrcB drawing

void CSrcB::OnDraw(CDC* pDC)
{
	CDocument* pDoc = GetDocument();
	// TODO: add draw code here
}


/////////////////////////////////////////////////////////////////////////////
// CSrcB message handlers

void CSrcB::OnInitialUpdate() 
{
  // TODO: Add your specialized code here and/or call the base class
  GetParentFrame()->SetWindowPos(0,0,0,
				 srcbfontinfo.dunits.cx * 45,
				 srcbfontinfo.dunits.cy * 30,
				 SWP_NOMOVE);
  
  CView::OnInitialUpdate();
}
#define R(x) x.left, x.top, x.Width(), x.Height()
void CSrcB::OnSize(UINT nType, int cx, int cy) 
{
  CRect wholebox;
  CRect shrunkbox;
  int dx = srcbfontinfo.dunits.cx;
  int dy = srcbfontinfo.dunits.cy;
  
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
  
 int i = 0;
  boxx.top = listbox.top + i * boxgap;
  boxx.bottom = boxx.top + boxheight;
  view.SetWindowPos(0,R(boxx),0);
  
  i = 1;
  boxx.top = listbox.top + i * boxgap;
  boxx.bottom = boxx.top + boxheight;
  bpt.SetWindowPos(0,R(boxx),0);
  i =2;
  boxx.top = listbox.top + i * boxgap;
  boxx.bottom = boxx.top + boxheight;
  explode.SetWindowPos(0,R(boxx),0);
  
}

int CSrcB::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  CRect rc (10,10,100,100);
  if (CView::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  list.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL
	      |WS_BORDER
	      | LBS_DISABLENOSCROLL | LBS_USETABSTOPS | WS_TABSTOP
	      | LBS_NOTIFY | LBS_OWNERDRAWFIXED,
	      rc,
	      this,
	      ID_CMD_BUTTON_BROWSE_LIST);
  
  edit.Create(WS_TABSTOP|WS_CHILD|WS_VISIBLE|WS_BORDER, rc, this, ID_CMD_BUTTON_FILTER);  
  
  view.Create("&View",WS_TABSTOP| BS_DEFPUSHBUTTON|WS_CHILD|WS_VISIBLE,	rc, this, 
	      ID_CMD_BUTTON_GOTO);
  bpt.Create("&Break",WS_TABSTOP| BS_DEFPUSHBUTTON|WS_CHILD|WS_VISIBLE,	rc, this, 
	     ID_CMD_BUTTON_BREAKPOINT);
  explode.Create("&Explode",WS_TABSTOP| BS_DEFPUSHBUTTON|WS_CHILD|WS_VISIBLE,	rc, this,
		 ID_CMD_BUTTON_EXPLODE);
  
  return 0;
}
BOOL CSrcB::OnEraseBkgnd(CDC* pDC) 
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


void CSrcB::Initialize()
{
  srcbfontinfo.Initialize();
}

void CSrcB::Terminate()
{
  srcbfontinfo.Terminate();
}

void CSrcB::rethink()
{
 static   struct gui_symtab_file *p = 0;
 int prev_top = list.GetTopIndex();
 int prev_sel = list.GetCurSel();
int showall; 
 //  gdbwin_list_symbols_free(p);
 CString filter;
 edit.GetWindowText(filter);
 const char *fs;
 if (filter.IsEmpty())
   fs = 0;
 else 
   fs = filter;
 if (!p)  
   p = gdbwin_list_symbols(0, 1);
 
 int regex = fs ? ! re_comp (fs) : 0;
				 CString bname;
 explode.GetWindowText(bname);
   showall = bname[1] == 'I';
 
 struct gui_symtab_file *i ;
 list.ResetContent();
 struct gui_symtab_item *items ;  
 int line_count = 0;
 
 if (regex) 
   {
    for (i = p; i; i = i->next_file) 
      {	
	int donefile = 0;
	for (items = i->items; items; items = items->next_item)
	  {
	   if (re_exec(SYMBOL_NAME(items->sym)))
	     {
	      if (!donefile)
		{
		 /* Only print the owning file if selected */
		 list.AddString ((char *)i);
		 line_count ++;
		 donefile = 1;
	       }
	      
	      list.AddString((char *)items);
	      line_count ++;
	    }
	 }
      }
  }
 else 
   {
    for (i = p; i; i = i->next_file) 
      {
       list.AddString ((char *)i);
       line_count ++;
       if (i->opened || showall)
	 {
	  for (items = i->items; items; items = items->next_item)
	    {
	     list.AddString((char *)items);
	     line_count ++;
	   }
	}
     }
  }
 list.SetTopIndex(line_count < prev_top ? line_count -1 : prev_top);
 list.SetCurSel(line_count < prev_sel ? line_count -1 : prev_sel);
}


void CSrcB::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
	// TODO: Add your specialized code here and/or call the base class
	
rethink();
}



void CSrcB::on_dblclkbrowselist() 
{
  // Find what was clicked
  int sel = list.GetCurSel();
  if (sel >= 0) {
    union gui_symtab *p = (union gui_symtab *) list.GetItemData(sel);
	if (p) {
    if (p->type == GUI_FILE) 
      {
	p->as_file.opened = !p->as_file.opened;
	CDocument* pDoc = GetDocument();
	//	pDoc->SetModifiedFlag();
	pDoc->UpdateAllViews(NULL);
      }
    else 
      {
	/* Double click on item, goto it if possible */
	theApp_show_at(p->as_item.sym->GetValue());
      }
	  }
  }
}
void CSrcB::on_goto()
{
  on_dblclkbrowselist();
}




void CSrcB::on_breakpoint()
{
  // Find what was clicked
  int sel = list.GetCurSel();
  if (sel >= 0) 
    {
      union gui_symtab *p = (union gui_symtab *) list.GetItemData(sel);
      if (p->type == GUI_ITEM)
	{
	  char buf[200];
	  sprintf(buf,"%s:%s",
		  p->as_item.parent->tab->filename,
		  SYMBOL_NAME(p->as_item.sym));
	  togdb_bpt_set(buf);
	}
    }
}


void CSrcB::on_explode()
{
  CString bname;
  explode.GetWindowText(bname);
  if (bname[1] == 'I')
    explode.SetWindowText("&Explode");
  else
    explode.SetWindowText("&Implode");
  rethink();
}


void CSrcB::on_change_filter()
{
  CString x;
  edit.GetWindowText(x);
  explode.EnableWindow(x.IsEmpty());
  rethink();
}

