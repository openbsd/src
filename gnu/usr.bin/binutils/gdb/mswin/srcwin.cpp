/* srcwin.cpp for WINGDB, the GNU debugger.
   Copyright 1995
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* 
   A document may sometimes be unable to show the assembly contained within 
   (if there is no debug info for the file), and sometimes may be unable to
   show the source of the assembly (if the source file is unavailable).

   This information is stored in doc->source_available and doc->assembly_available.

   The view on the document is split into two panes by the splitter.
   Each pane can show the assembly and/or source representations of
   the file.  Of course this is moderated by the actual availability
   of the data (cv doc->source/assembly_available).

   So the buttons we show always reflect the current doc state as well as the
   what the user wants.

*/

   
#include "stdafx.h"
#include "srcsel.h"
#include "srcd.h"
#include "srcwin.h"
#include "colinfo.h"
#include "log.h"
#include "change.h"
#define ROUND(x)   ((((x - srcx)/ width) *width)+srcx)
#define E_SPLAT 0
#define NOTE_SPLAT 1
#define POS_SPLAT 2
#define PC_SPLAT 3
#define FRAME_SPLAT 4

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define ABS(a)   (((a) < 0) ? -(a) : (a))


void theApp_add_watch(const char *);
static int addresses = 0;


extern CGuiApp theApp;

void redraw_allsrcwins()
{ 
  redraw_allwins(theApp.m_srcTemplate);
}



CColorInfo  colinfo_s   ("SrcA", 8388608, redraw_allsrcwins);
CColorInfo  colinfo_a   ("SrcS", 255, redraw_allsrcwins);
CColorInfo  colinfo_b   ("SrcB", 12632256, redraw_allsrcwins);
CFontInfo srcfontinfo ("SrcFont", redraw_allsrcwins);




/////////////////////////////////////////////////////////////////////////////
// CSrcFrame message handlers
/////////////////////////////////////////////////////////////////////////////
// CSrcSplit

IMPLEMENT_DYNCREATE(CSrcSplit, CMDIChildWnd)

CSrcSplit *src;


CSrcSplit::CSrcSplit()
{
src = this;
panes[0] = 0;
panes[1] = 0;
visible_file = 0;
}

CSrcSplit::~CSrcSplit()
{ 
//  delete visible_file;
}





static HCURSOR bpt_cursor;
static HCURSOR notbpt_cursor;
static HCURSOR src_cursor;

void CSrcSplit::Initialize()
{
  bpt_cursor = AfxGetApp()->LoadCursor(ID_SYM_BPT_CURSOR);
  notbpt_cursor = AfxGetApp()->LoadCursor(ID_SYM_NOTBPT_CURSOR);
  src_cursor = AfxGetApp()->LoadCursor(ID_SYM_SRC_CURSOR);

  srcfontinfo.Initialize();
  colinfo_a.Initialize();
  colinfo_s.Initialize();
  colinfo_b.Initialize();
}


void CSrcSplit::Terminate()
{
   colinfo_s.Terminate();
   colinfo_b.Terminate();
   colinfo_a.Terminate();
   srcfontinfo.Terminate();
}

#define  GAP 26
BOOL CSrcSplit::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
  CRect r;
  GetClientRect(r);
  CRect t = r;
  r.bottom = GAP;
  CSrcD *doc = (CSrcD *)(pContext->m_pCurrentDoc);
  active = 0;

  sel = (CTabView *)(RUNTIME_CLASS(CTabView)->CreateObject());
  sel->Create(0,0,WS_CHILD|WS_VISIBLE, r, this, ID_SYM_FRAME_TABTYPE,pContext);
  sel->parent = this;
  split.CreateStatic(this, 2, 1);
  
  split.CreateView (0, 0,
		    RUNTIME_CLASS(CSrcScroll1), 
		    CSize(t.Width(),t.Height()),
		    pContext);
  
  split.CreateView (1, 0,
		    RUNTIME_CLASS(CSrcScroll1), 
		    CSize(0,0),
		    pContext);
  
  (panes[0]=((CSrcScroll1 *)(split.GetPane(0,0))))->parent = this;
  (panes[1]=((CSrcScroll1 *)(split.GetPane(1,0))))->parent = this;
  panes[0]->winindex = 0;
  panes[1]->winindex = 1;
  
  //::SetWindowLong(sheet.m_hWnd, GWL_ID, m_wndSplitter.IdFromRowCol(0,0));
  load_where(GetParentFrame(),"SrcFont");
  return TRUE;
}



BEGIN_MESSAGE_MAP(CSrcSplit, CMDIChildWnd)
     //{{AFX_MSG_MAP(CSrcSplit)
     ON_WM_SIZE()
     ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
     ON_COMMAND(ID_REAL_CMD_BUTTON_TAB_CLOSE, OnTabClose)
     ON_WM_DESTROY()
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CSrcSplit message handlers
     /////////////////////////////////////////////////////////////////////////////
     // CSrcScroll
     /////////////////////////////////////////////////////////////////////////////
     // CSrcScroll1
     
     IMPLEMENT_DYNCREATE(CSrcScroll1, CScrollView)
     
     CSrcScroll1::CSrcScroll1()
{
  init = 0;
  visible_buffer = 0;
  srcb = 1;
  asmb = 0;
  lineb = 0;
  bptb = 0;
}

CSrcScroll1::~CSrcScroll1()
{
  
}


BEGIN_MESSAGE_MAP(CSrcScroll1, CScrollView)
     //{{AFX_MSG_MAP(CSrcScroll1)
     ON_WM_LBUTTONDOWN()
     ON_WM_LBUTTONDBLCLK()
     ON_WM_RBUTTONDOWN()
     ON_WM_CREATE()
     ON_WM_SETCURSOR()
     ON_WM_MOUSEMOVE()
     ON_WM_KEYDOWN()
     ON_WM_LBUTTONUP()
     ON_WM_ERASEBKGND()
     ON_COMMAND(ID_REAL_CMD_BUTTON_S, OnS)
     ON_COMMAND(ID_REAL_CMD_BUTTON_N, OnN)
     
     
     ON_COMMAND(ID_REAL_CMD_BUTTON_SRCWIN_WATCH, OnWatch)
     ON_WM_SETFOCUS()
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     
void CSrcScroll1::OnInitialUpdate()
{
  workout_source();
  CSize sizeTotal;
  // TODO: calculate the total size of this view
  sizeTotal.cx = sizeTotal.cy = 1000;
  SetScrollSizes(MM_TEXT, sizeTotal);
  
  CScrollView::OnInitialUpdate();
  
}


/////////////////////////////////////////////////////////////////////////////
// CSrcScroll1 diagnostics


/////////////////////////////////////////////////////////////////////////////
// CSrcScroll1 message handlers

int CSrcScroll1::find_line_of_pc(CORE_ADDR pc)
{
  int i;
  int first_visible_line;
  int last_visible_line;
  calc_visible_lines (&first_visible_line, &last_visible_line);
  for (i = first_visible_line; i < last_visible_line; i++)
    {
      if (addr_in_line (i, pc, 1))
	return i ;
    }
  return -1;
}

void CSrcScroll1::calc_visible_lines(int*first_visible_line,
				     int *last_visible_line)
{
  CRect rect;
  
  GetClientRect(&rect);
  *first_visible_line = get_top_line() - 1;
  int n_visible_lines = (rect.Height() / depth)+3 ;  
  *last_visible_line =  *first_visible_line + n_visible_lines;
  
  if (visible_buffer && *last_visible_line > visible_buffer->get_height())
    *last_visible_line = visible_buffer->get_height();
  
}


void CSrcScroll1::splat(CDC *pDC, int type, int line)
{
  int circle_gap = depth / 8;
  if (circle_gap < 1) 
    circle_gap = 1;
  
  int circle_width = (depth - circle_gap * 2) ;
  
  int y = line *depth;
  int x1 = splatx ;
  int y1 = y + circle_gap ;
  int x2 = x1 + circle_width ;
  int y2 = y1 + circle_width ;
  CBrush br;
  CBrush *old;
  switch (type) { 
  case E_SPLAT:
    br.CreateSolidBrush(RGB(255,0,0));
    break;
  case POS_SPLAT:
    br.CreateSolidBrush( RGB(192,192,192));
    break;
  case NOTE_SPLAT:
    br.CreateSolidBrush( RGB(0,255,255));
    break;
  case PC_SPLAT:
  case FRAME_SPLAT:
    {
      CPoint point[3];
      
      
      x1 += 1;
      y1 += 1;
      x2 -= 1;
      y2 -= 1;
      point[0] = CPoint(x1,y1);
      point[1] = CPoint(x2,(y1+y2)/2);
      point[2] = CPoint(x1,y2);
      
      br.CreateSolidBrush(PC_SPLAT ?  RGB(255,255,0) : RGB(0,255,255));
      old = pDC->SelectObject(&br);
      pDC->Polygon (point,3);
      pDC->SelectObject(old);
      return;
    }
    break;
  }
  old = pDC->SelectObject(&br);
  pDC->RoundRect(CRect(x1,y1,x2,y2), CPoint(2,2));
  pDC->SelectObject(old);
}
int CSrcScroll1::get_scroll_y()
{
  return GetScrollPosition().y;
}
int CSrcScroll1::get_top_line()
{
  return get_scroll_y() / depth;
}

int CSrcScroll1::addr_in_line (int l, CORE_ADDR x, int splatish)
{
  register CSrcLine *p = visible_buffer ? visible_buffer->get_line(l) : 0;
  /* Never put a splat on a source line when in disassmbly mode */
  if (p && show_assembly()
      && p->is_source_line()
      && splatish)
    return 0;
  if (p 
      && p->from <= x 
      && p->to >= x)
    return 1;
  return 0;
}


int CSrcScroll1::show_assembly()
{
  if (!get_visible_file()) return 0;
  if (!get_visible_file()->source_available)
    return 1;
  return asmb;
}

int CSrcScroll1::show_source()
{
  if (!get_visible_file()) return 0;
  if (!get_visible_file()->source_available)
    return 0;
  return srcb;
}


/* Setup the pointer to the right visible buffer. */

void CSrcScroll1::workout_source()
{
  CSrcD *doc = getdoc();
  
  doc->makesure_fresh();
  /* Show the assembly if the source is not available */

  if (show_assembly())
    {
      if (show_source())
	{
	  visible_buffer =  &get_visible_file()->both ;
	}
      else 
	{
	  visible_buffer =  &get_visible_file()->asml;
	}
    }
  else if (show_source())
    {
      visible_buffer = &get_visible_file()->src;
    }
  else
    return;

  init = 1;
}


void CSrcScroll1::OnDraw(CDC *pDC)
{
  int sl = -1;
  CSrcD *doc = getdoc();
  if (!get_visible_file())
    return;
  workout_source();

  pDC->SelectObject (&srcfontinfo.m_font);	
  CSize s = pDC->GetTextExtent ("8",1);

  if (show_assembly()) 
    {
      if (show_source())
	{
	  splatx = s.cx * 8;
	}
      else  {
	      splatx = s.cx * 8;
	    }
    }
  else {
	 splatx = lineb ? s.cx * 4 : 0;
       }
  splatx += s.cy  / 4;
  srcx = splatx + (s.cy);
  depth = s.cy;
  width = s.cx;
  //rethink_scrollbar();
  CSize total((srcx + s.cx * visible_buffer->get_width()), depth * visible_buffer->get_height());
  
  SetScrollSizes(MM_TEXT, total, CSize(0,depth*10),CSize(50,50));
  
  int first_visible_line;
  int last_visible_line;
  
  calc_visible_lines (&first_visible_line, &last_visible_line);
  
  if (want_to_show_line >= 0
      && want_to_show_line < first_visible_line + 4
      || want_to_show_line > last_visible_line - 4)
    {
      scroll_to_show_line (want_to_show_line);
      calc_visible_lines (&first_visible_line, &last_visible_line);
      want_to_show_line = -1;
    }
  
  if (show_assembly()) 
    {
      CSrcFileBySymtab *p = get_visible_file()->get_CSrcFileBySymtab();
      if (p)
	p->preparedisassemble();
    }
  
  
  CORE_ADDR prevpc = ~0;  
  
  
  
  for (int i = first_visible_line - 10;
       i < last_visible_line + 10;
       i++)
    {	
      char buf[40];
      int cx = 0;
      CSrcLine *p = visible_buffer->get_line(i);
      int    y = i * depth;
      
      /* A line looks like
	 
	 aaaaaaaa * disassembly
	 llll     * source
	 
	 */
      
      if (p) 
	{
	  if (p->is_assembly_line()) 
	    {
	      pDC->SetTextColor(colinfo_a.m_value);
	      if (addresses && p->from != prevpc) 
		{
		  prevpc = p->from;
		  sprintf(buf,"%s ", paddr(p->from));
		  cx += pDC->TabbedTextOut(cx, y, buf, strlen(buf),0,0,0).cx;
		}
	      
	      if (show_assembly())
		{	
		  pDC->TabbedTextOut(srcx, y, p->text, strlen(p->text),0,0,0);
		}
	    }
	  else 
	    {
	      pDC->SetTextColor(colinfo_s.m_value);
	      if (lineb)
		{
		  sprintf(buf," %d", p->line + 1);
		  cx+=  pDC->TabbedTextOut(cx, y, buf,strlen(buf),0,0,cx).cx;
		}
	      if (show_source())
		{	
		  pDC->TabbedTextOut(srcx, y, p->text, strlen(p->text),0,0,cx);
		}
	    }
	  /* Don't put breakpoint splats on source lines when
	     we're disassembling */
	  if (bptb
	      && p->bpt_ok()
	      && (p->is_assembly_line() || !show_assembly()))
	    {
	      splat(pDC, POS_SPLAT, i);
	    }
	}
    }
  
  {
    /* Stick on the breakpoint splats.  We've got to take a bit of 
       care here, since sometimes breakpoints can be set within a source line.
       We deal by splatting down a dot if its the same pc, or just before the next
       pc */
    extern CBreakInfoList the_breakinfo_list;
    int i;
    int size = the_breakinfo_list.GetSize();
    for (i = 0; i < size; i++) 
      {
	CBreakInfo *bi = the_breakinfo_list.GetAt(i);
	CORE_ADDR x = bi->GetAddress();
	for (int l = first_visible_line-10; l < last_visible_line+10; l++) 
	  {
	    CSrcLine *p = visible_buffer->get_line(l);	    
	    /* Don't put breakpoint splats on source lines when
	       we're disassembling */

	    if (p && p->bpt_ok() && addr_in_line (l, x, 1))
	      {
		if (p->is_assembly_line()
		    || !show_assembly())
		  {
		    /* It's on this screen, index and splat the lines with it on */
		    splat(pDC, bi->GetEnable() ?
			  E_SPLAT : NOTE_SPLAT , l);
		  }
	      }
	  }
      }
  }
  
  
  if (togdb_target_has_registers())
    {  
      if (selected_frame)
	{
	  splat(pDC, FRAME_SPLAT, find_line_of_pc(selected_frame->pc));
	}
      splat(pDC, PC_SPLAT, line_with_pc = find_line_of_pc (togdb_fetchpc()));
    }
  
  
  
}
void CSrcScroll1::redraw_line (int line, int off)
{
  
  CRect r;
  GetClientRect (&r);
  r.top = (line - off -3 ) *depth;
  r.bottom = (line - off +3) *depth;
  r.left = splatx;
  r.right = r.left +depth;
  InvalidateRect(&r);
}

void CSrcScroll1::scroll_to_show_line (int i)
{
  /* Stop at first hit, scroll to 1/3 the way down */
  CRect x;
  GetClientRect(&x);
  int nlines = (x.Height() / depth) - 5;
  i -= nlines / 3;
  if (i < 0) i =0;
  Invalidate();
  ScrollToPosition(CPoint(0, i * depth));
}

/* This function is called whenever the pc changes,
   lHint is the pc */

void CSrcScroll1::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) 
{
  CSrcD *doc = getdoc();
  int first_visible_line;
  int last_visible_line;
  
  
  if (!get_visible_file())
    return;
  
  calc_visible_lines (&first_visible_line, &last_visible_line);
  
  if (get_visible_file())  
    {
      switch (lHint)
	{
	case 0:
	  Invalidate();
	  break;
	case -1:							    
	  /* Just change the breakpoint info colum */
	  {
	    CRect r;
	    GetClientRect (&r);
	    r.left = splatx;
	    r.right = splatx + depth;
	    InvalidateRect (&r);
	  }
	  break;
	default:
	  /* Asked to show at the visible_file address, could be a changed
	     pc or a browse request */
	  {
	    CORE_ADDR show = lHint;
	    CORE_ADDR pc = togdb_fetchpc();
	    
	    int line = find_line_of_pc (pc);
	    want_to_show_line = find_line_of_pc(lHint);
	    
	    redraw_line (line, first_visible_line);
	    
	    if (line != line_with_pc) 
	      {
		/* The line with pc has moved, so rub out old
		   pc and draw new one */
		/* first mark the old pc line for rubbing out */
		redraw_line (line_with_pc, first_visible_line);
	      }
	    if (line != line_with_show_line) 
	      {
		redraw_line (line_with_show_line, first_visible_line);
	      }
	    
	    /* Whatever happens we want to scroll to where show is */
	    
	    if (show >= get_visible_file()->first_pc
		&& show < get_visible_file()->last_pc) 
	      {
		int i;
		
		if (1) 
		  {
		    for (i = first_visible_line +2 ;i < last_visible_line - 2; i++) 
		      {
			if (addr_in_line (i, show, 1)) 
			  {
			    /* found the line with the pc on it, if it's 
			       near the center of the screen then we'll just
			       repaint the screen - remember we leave a bit
			       of slack at *_visible_line so we can scroll
			       without leaving turds. */
			    want_to_show_line = i;
			    return;
			  }
		      }
		  }
		/* can't see it without scrolling */
		
		/* This pc isn't on the screen, 
		   but is in the file, so make sure
		   that next time we draw the screen
		   we show the right bit */
		for (i= 0; i  < visible_buffer->get_height(); i++)
		  {
		    if (addr_in_line (i, show, 1))
		      {
			want_to_show_line = i;
			Invalidate();
			return ;
		      }
		  }
	      }
	  }
	  break;
	}
    }
}

static int downline;		/* Line where mouse button went down */
static int downx;		/* Where mouse button went down */
static int curx;		/* visible_file mouse x co */
static int bdown;		/* if button is down */
static int rdown;
static int inverted;		/* if downx..curx is inverted */
static char buf[200];		/* Visible_File highlighted word */

int CSrcScroll1::line_from_0basedy(int y)
{
  return (get_scroll_y() + y) / depth;
}

void CSrcScroll1::OnLButtonDown(UINT nFlags, CPoint point) 
{
  if (zone == BPT) 
    {
      int line =  line_from_0basedy(point.y);
      CSrcLine *l = visible_buffer ? visible_buffer->get_line(line) : 0;
      if (l) 
	{
	  parent->visible_file->toggle_breakpoint(l);
	}
    }
  if (zone == SRC) {
    if (!bdown) {
      set_invert(0);
      bdown = 1;
      curx = downx = ROUND(point.x);
      downline = srcline_index;
    }
  }
}

void CSrcScroll1::set_invert(int on)
{
  if (inverted !=on)
    {
      /* Turn off the drawing */
      CRect r;
      CClientDC dc(this);
      OnPrepareDC(&dc);
      r.top = downline * depth;
      r.bottom = r.top + depth;
      r.left = downx;
      r.right = curx;
      dc.InvertRect(r);
      inverted = on;
    }
}
void CSrcScroll1::OnLButtonUp(UINT nFlags, CPoint point) 
{
  bdown = 0;
}

void CSrcScroll1::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
  /* workout the string underneath the dot */
  set_invert(0);
  bdown = 0;
  CClientDC dc(this);
  dc.SelectObject (&srcfontinfo.m_font);	
  CSize s = dc.GetTextExtent ("8",1);
  CSrcLine *l = visible_buffer ? visible_buffer->get_line (downline) : 0;
  if (l) {
    const char *p = l->text;
    int len = strlen(p);
    int from;
    int to;
    
    from = downx;
    to = curx;
    
    from = MAX(0,from - srcx);
    to = MAX(0,to - srcx);
    
    int a = (from) / width;
    int b = (to)  / width;
    
    /* Double click on same place, we'll take a word */
    /* Scan back for the start of the word */
    while (a>=0
	   && (isalpha (p[a])
	       || isdigit (p[a])
	       || p[a] == '_'))
      a--;
    a++;      
    /* And forrwards to the end of the word */
    while (b < len
	   && (isalpha (p[b])
	       || isdigit (p[b])
	       || p[b] == '_'))
      b++;
    
    downx = a * width + srcx;
    curx = b * width + srcx;
    set_invert(1);
  }
}


void CSrcScroll1::OnRButtonDown(UINT nFlags, CPoint point) 
{
  /* If there's a highlighted area then use that text */
  /* workout the string underneath the dot */
  ClientToScreen (&point);
  set_invert(0);
  bdown = 0;
  rdown=1;
  
  CSrcLine *l = visible_buffer ? visible_buffer->get_line (downline) : 0;
  if (l) {
    const char *p = l->text;
    int len = strlen(p);
    int from;
    int to;
    
    from = downx;
    to = curx;
    
    from = MAX(0,from - srcx);
    to = MAX(0,to - srcx);
    
    int a = (from) / width;
    int b = (to )  / width;
    
    int i;
    int j;
    if (a != b) {
      for (i = 0, j = a; j < b && i < sizeof(buf)-1; j++, i++) 
	buf[i] = p[j];
      buf[i] = 0;
      
      CChange::change(buf);
      //  theApp_add_watch(buf);      
      
      curx = downx = 0;
      rdown = 0;
      return;
    }
  }
  theApp_add_watch(buf);
  
  rdown = 0;
  redraw_allsrcwins();
}



int CSrcScroll1::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
  if (CScrollView::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  // TODO: Add your specialized creation code here
  
  return 0;
}
static CPoint m;

BOOL CSrcScroll1::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
  int first_visible_line;
  int last_visible_line;
  if (!get_visible_file()) 
    return TRUE;
  calc_visible_lines (&first_visible_line, &last_visible_line);
  CSrcLine *p = visible_buffer->get_line(srcline_index = line_from_0basedy(m.y));
  CRect c;
  GetClientRect(c);
  
  if (rdown)
    SetCursor(bpt_cursor);
  else
    if (m.x < splatx + depth) 
      {
	if (p) {
	  if (p->bpt_ok())
	    ::SetCursor(bpt_cursor);
	  else
	    ::SetCursor(notbpt_cursor);
	}
	zone = BPT;
	return TRUE;
      }
    else if (m.x < c.right - 30)
      {
	::SetCursor(src_cursor);
	zone = SRC;
	return TRUE;
      }
  
  return CScrollView::OnSetCursor(pWnd, nHitTest, message);
}

void CSrcScroll1::OnMouseMove(UINT nFlags, CPoint point) 
{
  m = point;
  
  if (bdown) 
    {
      CClientDC dc(this);
      
      CSrcLine *l = visible_buffer ? visible_buffer->get_line (downline) : 0;
      
      set_invert(0);
      /* always make it a multiple of the char width */
      curx = ROUND(point.x);
      set_invert(1);
    }
  CScrollView::OnMouseMove(nFlags, point);
}


void CSrcScroll1::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
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
    case VK_DOWN:
      i = SB_LINEDOWN;
      break;
    case VK_UP:
      i = SB_LINEUP;
      break;
    case VK_NEXT:
      i = SB_PAGEDOWN;
      break;
    default:
      CScrollView::OnKeyDown(nChar, nRepCnt, nFlags);
      return ;
    }
  OnVScroll(i, 0, GetScrollBarCtrl(SB_VERT));
}



void CSrcScroll1::OnWatch()
{
  theApp_add_watch(buf);
}

void CSrcScroll1::OnS() 
{
  Credirect dummy;
  if (show_assembly())
    togdb_command("si");	
  else
    togdb_command("s");	
  togdb_force_update();
}  


void CSrcScroll1::OnN() 
{
  Credirect dummy;
  if (show_assembly())
    togdb_command("ni");	
  else
    togdb_command("n");	
  togdb_force_update();
}


void CSrcSplit::OnSize(UINT nType, int cx, int cy) 
{
  if (panes[0]) 
    {
      CMDIChildWnd::OnSize(nType, cx, cy); 
      CRect c;
      GetClientRect(c);
      CRect top= c;
      CRect bottom = c;
      top.bottom = GAP; 
      bottom.top = GAP;
      
      sel->SetWindowPos(0,top.left,top.top,top.Width(),top.Height(),0);
      split.SetWindowPos(0,bottom.left,bottom.top,bottom.Width(),bottom.Height(),0);  
    }
}

BOOL CSrcSplit::PreCreateWindow(CREATESTRUCT& cs) 
{
  return CMDIChildWnd::PreCreateWindow(cs);
}

BOOL CSrcScroll1::OnEraseBkgnd(CDC* pDC) 
{
  CRect rect; 
  GetClientRect(rect); 
  pDC->FillSolidRect(rect, colinfo_b.m_value);
  return TRUE; 
}


BOOL CSrcSelWrap::OnEraseBkgnd(CDC* pDC) 
{
  CRect rect; 
  GetClientRect(rect); 
  pDC->FillSolidRect(rect, colinfo_b.m_value);
  return TRUE; 
}

BOOL CSrcSel::OnEraseBkgnd(CDC* pDC) 
{
  CRect rect; 
  GetClientRect(rect); 
  pDC->FillSolidRect(rect, colinfo_b.m_value);
  return TRUE; 
}




void CSrcSplit::select_title(const char *title)
{
  CSrcD *doc = panes[0]->getdoc();
  int in;
  CSrcFile *t = doc->lookup_title(title,&in);
  
  if (!t && global_options.always_create)
    {
      theApp_show_file(title);
      t = doc->lookup_title(title, &in);
    }
  if (t && t != visible_file)
    {
      visible_file = t;
      panes[0]->workout_source();
      panes[1]->workout_source();
      sel->tabs.SetCurSel(in);
      SetWindowText(title);
      Invalidate();
    }
}


void CSrcSplit::select_symtab(CSymtab *symtab)
{
  CSrcD *doc = panes[0]->getdoc();
  int in;
  CSrcFile *t = doc->lookup_symtab(symtab,&in);
  
  if (!t && global_options.always_create)
    {
      theApp_show_with_symtab (symtab);
      t = doc->lookup_symtab(symtab, &in);
    }
  if (t && t != visible_file)
    {
      visible_file = t;
      panes[0]->workout_source();
      panes[1]->workout_source();
      sel->tabs.SetCurSel(in);
      //      SetWindowText(title);
      
      /* This is a different file, so force a redraw of the whole screen */
      Invalidate();
    }
}


void CSrcSplit::OnFileOpen() 
{
  CFileDialog dialog(TRUE, 
		     NULL,
		     NULL,
		     0,
		     "Source files | *.c; *.cpp; *.h | All Files | *.* ||", 
		     this);
  
  if (dialog.DoModal() == IDOK) 
    {
      theApp_show_file (dialog.GetPathName());
    }
}

void CSrcSplit::OnTabClose() 
{
  sel->deletecur();
}

void CSrcSplit::new_pc(CORE_ADDR pc)
{
  char *filename;
  CSymtab *symt;
  int l;
  togdb_fetchloc(pc, &filename, &l, ((struct symtab **)(&symt)));
  if (symt)
    select_symtab (symt);
  /*  sel->OnUpdate(0,pc,0);
      panes[0]->OnUpdate(0, pc, 0);
      panes[1]->OnUpdate(0,pc,0);*/
  panes[0]->want_to_show_line = l;
  panes[1]->want_to_show_line = l;
}



void CSrcScroll1:: OnUpdateShowSource(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(show_source())	 ;
  pCmdUI->Enable(get_visible_file() ? get_visible_file()->source_available : 0  ); 
}

void CSrcScroll1:: toggle(int *p)
{
  *p = !*p;
  workout_source();
  Invalidate();
}
void CSrcScroll1:: OnShowSource()
{
  toggle (&srcb);
  if (!show_source() && !show_assembly())
    OnShowAsm();
}


void CSrcScroll1:: OnShowBpts()
{
  toggle(&bptb);
}
void CSrcScroll1:: OnShowLine()
{
  toggle(&lineb);
}

void CSrcScroll1:: OnUpdateShowBpts(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(bptb);
  pCmdUI->Enable(get_visible_file() ? get_visible_file()->assembly_available :0);
}

void CSrcScroll1:: OnUpdateShowLine(CCmdUI* pCmdUI)
{
  pCmdUI->SetCheck(lineb);
  pCmdUI->Enable(get_visible_file() ? get_visible_file()->source_available: 0);
}

CSrcScroll1 *thesrcpane;
void CSrcScroll1::OnSetFocus(CWnd* pOldWnd) 
{
  CScrollView::OnSetFocus(pOldWnd);
  
  thesrcpane = this;
}
void CSrcScroll1:: OnShowAsm()
{
  toggle (&asmb);
  if (!show_source() && !show_assembly())
    OnShowSource();
}

void CSrcScroll1:: OnUpdateShowAsm(CCmdUI* pCmdUI) 
{
  pCmdUI->SetCheck(show_assembly());
  
  pCmdUI->Enable(get_visible_file() ? get_visible_file()->assembly_available : 0);
}


void CSrcSplit::OnDestroy() 
{
  save_where(GetParentFrame(),"SrcFont");	
  thesrcpane = 0;
  CMDIChildWnd::OnDestroy();
}

