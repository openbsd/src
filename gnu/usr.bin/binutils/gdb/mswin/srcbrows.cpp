#if 0
// srcbrows.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "browserl.h"
#include "srcbrows.h"

/////////////////////////////////////////////////////////////////////////////
// CSrcBrowser

IMPLEMENT_DYNCREATE(CSrcBrowser, CFormView)

CSrcBrowser::CSrcBrowser()
	: CFormView(CSrcBrowser::IDD)
{
  //{{AFX_DATA_INIT(CSrcBrowser)
  m_filter = _T("");
  //}}AFX_DATA_INIT
  m_explode.SetWindowText("&Implode");
}

CSrcBrowser::~CSrcBrowser()
{
}

void CSrcBrowser::DoDataExchange(CDataExchange* pDX)
{
  CFormView::DoDataExchange(pDX);
  //{{AFX_DATA_MAP(CSrcBrowser)
  DDX_Control(pDX, ID_CMD_BUTTON_EXPLODE, m_explode);
  DDX_Control(pDX, ID_CMD_BUTTON_BROWSE_LIST, m_listbox);
  DDX_Text(pDX, ID_CMD_BUTTON_FILTER, m_filter);
  DDV_MaxChars(pDX, m_filter, 40);
  //}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSrcBrowser, CFormView)
     //{{AFX_MSG_MAP(CSrcBrowser)
     ON_LBN_DBLCLK(ID_CMD_BUTTON_BROWSE_LIST, OnDblclkBrowseList)
     ON_BN_CLICKED(ID_CMD_BUTTON_GOTO, OnGoto)
     ON_BN_CLICKED(ID_CMD_BUTTON_BREAKPOINT, OnBreakpoint)
     ON_BN_CLICKED(ID_REAL_CMD_BUTTON_SET_FONT, OnSetFont)
     ON_BN_CLICKED(ID_CMD_BUTTON_EXPLODE, OnExplode)
     ON_EN_CHANGE(ID_CMD_BUTTON_FILTER, OnChangeFilter)
     ON_WM_DESTROY()
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CSrcBrowser message handlers
     void CSrcBrowser::Initialize()
{
}
void CSrcBrowser::Terminate()
{
}
void CSrcBrowser::OnInitialUpdate() 
{
  // TODO: Add your specialized code here and/or call the base class
  ResizeParentToFit();	
  CFormView::OnInitialUpdate();
  Rethink();
  load_where(GetParentFrame(),"SrcBrowser");
}

void CSrcBrowser::Rethink()
{
  CString bname;
  static   struct gui_symtab_file *p = 0;
  int prev_top = m_listbox.GetTopIndex();
  int prev_sel = m_listbox.GetCurSel();

  
  const char *fs;
  if (m_filter.IsEmpty())
    fs = 0;
  else 
    fs = (const char *)m_filter;
  if (!p)  
    p = gdbwin_list_symbols(0, 1);
  
  int regex = fs ? ! re_comp (fs) : 0;


  m_explode.GetWindowText(bname);
  int  m_showall = bname[1] == 'I';
  m_explode.EnableWindow(m_filter.IsEmpty());
  struct gui_symtab_file *i ;
  m_listbox.ResetContent();
  struct gui_symtab_item *items ;  
  int line_count = 0;
  
  if (regex) 
    {
      for (i = p; i; i = i->next_file) 
	{	
	  int donefile = 0;
	  for (items = i->items; items; items = items->next_item)
	    {
	      if (re_exec(items->sym->GetName()))
		{
		  if (!donefile)
		    {
		      /* Only print the owning file if selected */
		      m_listbox.AddString ((char *)i);
		      line_count ++;
		      donefile = 1;
		    }
		  
		  m_listbox.AddString((char *)items);
		  line_count ++;
		}
	    }
	}
    }
  else 
    {
      for (i = p; i; i = i->next_file) 
	{
	  m_listbox.AddString ((char *)i);
	  line_count ++;
	  if (i->opened || m_showall)
	    {
	      for (items = i->items; items; items = items->next_item)
		{
		  m_listbox.AddString((char *)items);
		  line_count ++;
		}
	    }
	}
    }
  m_listbox.SetTopIndex(line_count < prev_top ? line_count -1 : prev_top);
  m_listbox.SetCurSel(line_count < prev_sel ? line_count -1 : prev_sel);
}

void CSrcBrowser::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
  
  Rethink();
}

void CSrcBrowser::OnDblclkBrowseList() 
{
  // Find what was clicked
  int sel = m_listbox.GetCurSel();
  if (sel >= 0) {
    union gui_symtab *p = (union gui_symtab *) m_listbox.GetItemData(sel);
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
	theApp_show_at (p->as_item.sym->GetValue());
      }
  }
}
void CSrcBrowser::OnGoto()
{
  OnDblclkBrowseList();
}


extern CGuiApp theApp;

void CSrcBrowser::OnBreakpoint() 
{
  // Find what was clicked
  int sel = m_listbox.GetCurSel();
  if (sel >= 0) 
    {
      union gui_symtab *p = (union gui_symtab *) m_listbox.GetItemData(sel);
      if (p->type == GUI_ITEM)
	{
	  char buf[200];
	  sprintf(buf,"%s:%s",
		  p->as_item.parent->tab->filename,
		  p->as_item.sym->GetName());
	  togdb_bpt_set(buf);
	}
    }
}





static void redraw_allsnoopwins()
{ 
  redraw_allwins(theApp.m_srcbrowserTemplate);
}

CFontInfo  browser_fontinfo ("Snoop",redraw_allsnoopwins);
void CSrcBrowser::OnSetFont() 
{
  
  browser_fontinfo.OnChooseFont();
  
}

void CSrcBrowser::OnExplode() 
{
  Rethink();
}


void CSrcBrowser::OnChangeFilter() 
{
  Rethink();
}

void CSrcBrowser::OnDestroy() 
{
  save_where(GetParentFrame(),"SrcBrowser");
  CFormView::OnDestroy();
  
}
#endif
