/* mainfrm for WINGDB, the GNU debugger.
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


#include <direct.h>
#include "stdafx.h"
#include "regdoc.h"
#include "regview.h"
#include "log.h"
#include "mainfrm.h"
#include "srcsel.h"
#include "srcwin.h"


IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
	//{{AFX_MSG_MAP(CMainFrame)

        ON_COMMAND(ID_CMD_REMOTE, OnTargetRemote)
        ON_COMMAND(ID_CMD_SIMULATOR, OnTargetSimulator)
        ON_COMMAND(ID_CMD_READ_SCRIPT, OnReadScript)
        ON_COMMAND(ID_REAL_CMD_BUTTON_CONT, OnCont)
        ON_COMMAND(ID_REAL_CMD_BUTTON_FINISH, OnFinish)
        ON_COMMAND(ID_REAL_CMD_BUTTON_IN, OnIn) 
        ON_COMMAND(ID_REAL_CMD_BUTTON_N, OnN)
        ON_COMMAND(ID_REAL_CMD_BUTTON_OUT, OnOut)       
        ON_COMMAND(ID_REAL_CMD_BUTTON_REGISTER, OnRegister)
        ON_COMMAND(ID_REAL_CMD_BUTTON_RUN, OnRun)
        ON_COMMAND(ID_REAL_CMD_BUTTON_S, OnS) 
        ON_COMMAND(ID_REAL_CMD_BUTTON_SYNC, OnSync)
        ON_COMMAND(ID_REAL_CMD_BUTTON_VIEW_PROPERTIES, OnProperties)
        ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_CONT, OnUpdateCont)
        ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_FINISH, OnUpdateFinish)
        ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_IN, OnUpdateIn)
        ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_N, OnUpdateN)
        ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_RUN, OnUpdateRun)
        ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_S, OnUpdateS)
        ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_WHAT, OnUpdateWhat)
        ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_SYNC, OnUpdateSync)
        ON_WM_CLOSE()
        ON_WM_CREATE()
        ON_WM_SIZE()
       ON_UPDATE_COMMAND_UI(ID_REAL_CMD_BUTTON_OUT, OnUpdateOut)
 	//}}AFX_MSG_MAP
	// Global help commands
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// arrays of IDs used to initialize control bars
// toolbar buttons - IDs are command buttons
static UINT BASED_CODE buttons[] =
{
	// same order as in the bitmap 'toolbar.bmp'
	ID_FILE_NEW,
	ID_FILE_OPEN,
	ID_FILE_SAVE,
		ID_SEPARATOR,
	ID_EDIT_CUT,
	ID_EDIT_COPY,
	ID_EDIT_PASTE,
};



// toolbar buttons - IDs are command buttons
static UINT BASED_CODE gdbbuttons[] =
{
	ID_REAL_CMD_BUTTON_S,
	ID_REAL_CMD_BUTTON_N,
		ID_SEPARATOR,
	ID_REAL_CMD_BUTTON_FINISH,
	ID_REAL_CMD_BUTTON_CONT,
	ID_REAL_CMD_BUTTON_RUN,
		ID_SEPARATOR,
	ID_REAL_CMD_BUTTON_IN,
	ID_REAL_CMD_BUTTON_OUT,
		ID_SEPARATOR,
	ID_REAL_CMD_BUTTON_SYNC
};



// window buttons
static UINT BASED_CODE winbuttons[] =
{
	ID_REAL_CMD_BUTTON_NEW_CMDWIN,
	ID_REAL_CMD_BUTTON_NEW_REGWIN,
	ID_REAL_CMD_BUTTON_NEW_BPTWIN,
	ID_REAL_CMD_BUTTON_NEW_SRCBROWSER_WIN,
	ID_REAL_CMD_BUTTON_NEW_LOCAL_WIN,
	ID_REAL_CMD_BUTTON_NEW_EXPRESSION_WIN

};

// source window buttons
static UINT BASED_CODE srcwinbuttons [] = 
{
  ID_REAL_CMD_BUTTON_SRCWIN_SHOWSOURCE,
  ID_REAL_CMD_BUTTON_SRCWIN_SHOWASM,
  ID_REAL_CMD_BUTTON_SRCWIN_SHOWLINE,
  ID_REAL_CMD_BUTTON_SRCWIN_SHOWBPT,
};

static UINT BASED_CODE indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_SEPARATOR,
	 
};


/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CWnd *theframe;
CMainFrame::CMainFrame()
{
  theframe = this;
  
}

CMainFrame::~CMainFrame()
{
}

void CMainFrame::InitBar(CToolBar *p, int id, const unsigned int *buttons, int size)
{
  if (!p->Create(this, WS_CHILD|WS_VISIBLE|CBRS_TOP|CBRS_TOOLTIPS|CBRS_FLYBY) ||
      !p->LoadBitmap(id) ||
      !p->SetButtons(buttons, size))
    {
      TRACE0("Failed to create toolbar\n");
      
    }
  
  // TODO: Delete these three lines if you don't want the toolbar to
  //  be dockable
  p->EnableDocking(CBRS_ALIGN_ANY);
  
}



void CMainFrame::DockControlBarLeftOf(CControlBar* Bar,CControlBar* LeftOf)
{
  CRect rect;
  DWORD dw;
  UINT n;
  
  // get MFC to adjust the dimensions of all docked ToolBars
  // so that GetWindowRect will be accurate
  RecalcLayout();
  LeftOf->GetWindowRect(&rect);
  rect.OffsetRect(1,0);
  dw=LeftOf->GetBarStyle();
  n = 0;
  n = (dw&CBRS_ALIGN_TOP) ? AFX_IDW_DOCKBAR_TOP : n;
  n = (dw&CBRS_ALIGN_BOTTOM && n==0) ? AFX_IDW_DOCKBAR_BOTTOM : n;
  n = (dw&CBRS_ALIGN_LEFT && n==0) ? AFX_IDW_DOCKBAR_LEFT : n;
  n = (dw&CBRS_ALIGN_RIGHT && n==0) ? AFX_IDW_DOCKBAR_RIGHT : n;
  
  // When we take the default parameters on rect, DockControlBar will dock
  // each Toolbar on a seperate line.  By calculating a rectangle, we in effect
  // are simulating a Toolbar being dragged to that location and docked.
  DockControlBar(Bar,n,&rect);
}

void CMainFrame::OnClose()
{
  SaveBarState("General");
  CMDIFrameWnd::OnClose();
}
CMainFrame *the_cover;
static int depth;
//CLogView logview;
extern CWnd *top;
extern CWnd *bottom;
CStatusBar *status;	extern "C" {
  extern char *target_name;
};

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
  
  if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
    return -1;
  
  
  CRect rect;
  GetClientRect(&rect);
  CRect tr;
  CRect br;
  CRect foo = rect;
  tr = rect;
  the_cover = this;
  br = rect;
  
  int dwStyle = WS_BORDER|WS_CHILD|WS_VISIBLE;
  tr.top += 50;
  tr.bottom = tr.bottom / 2;
  br.top = tr.bottom;
  char * m = MAKEINTRESOURCE(ID_SYM_FRAME_MAINFRAME);
  
  EnableDocking(CBRS_ALIGN_ANY);
  
  InitBar (&m_wndToolBar, 
	   ID_SYM_BITMAP_MAINFRAME, 
	   buttons, 
	   sizeof(buttons)/sizeof(UINT));
  DockControlBar(&m_wndToolBar, AFX_IDW_DOCKBAR_TOP);	
  
  InitBar ( &m_wndWinBar, 
	   ID_SYM_BITMAP_WINTOOLBAR, 
	   winbuttons,
	   sizeof(winbuttons)/sizeof(UINT));	
  DockControlBarLeftOf(&m_wndWinBar, &m_wndToolBar);
  
  
  InitBar ( &src_win_bar,
	   ID_SYM_BITMAP_SRCWIN,
	   srcwinbuttons,
	   sizeof(srcwinbuttons)/sizeof(UINT));	
  DockControlBarLeftOf(&src_win_bar, &m_wndWinBar);
  
  
  InitBar (&m_wndGdbBar, 
	   ID_SYM_BITMAP_GDBTOOLBAR, 
	   gdbbuttons, 
	   sizeof(gdbbuttons)/sizeof(UINT));	
  DockControlBarLeftOf(&m_wndGdbBar, &src_win_bar);
  
  if (!m_wndStatusBar.Create(this) || 
      !m_wndStatusBar.SetIndicators(indicators, 
				    sizeof(indicators)/sizeof(UINT))) 
    { 
      TRACE("Failed to create status bar\n"); 
      return -1;			// fail to create 
    } 
  status = &m_wndStatusBar;
  //  m_wndStatusBar.EnableDocking(CBRS_ALIGN_ANY);    
  //  DockControlBarLeftOf(&m_wndStatusBar, &m_wndGdbBar);
  {
    CString x;
    
    x = "gdb ";
    x += target_name;
    SetWindowText(x); 
  }
#if 0  
  doingbar.Create(
		  
		  doingbar.EnableDocking(CBRS_ALIGN_ANY);  
		  DockControlBarLeftOf(&doingbar, &m_wndGdbBar);
		  doing = &doingbar;  
#endif
		  return 0;
		}
  
  // CMainFrame message handlers
  
  void CMainFrame::OnRegister() 
    {
      
    }
  
  
  
  
  BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)  
    {
      // don't muck with the window title 
      cs.style &= ~(FWS_ADDTOTITLE);	
      return CMDIFrameWnd::PreCreateWindow(cs);
    }
  
  
  void CMainFrame::OnUpdateFinish(CCmdUI* pCmdUI) 
    {
      pCmdUI->Enable( togdb_target_has_execution());
      
    }
  
  void CMainFrame::OnSync()
    {
      togdb_force_update();
      togdb_force_update();
    }
  void CMainFrame::OnS() 
    {
      Credirect dummy;
      togdb_command("s");	
	togdb_force_update();
    }
  
  
  void CMainFrame::OnFinish() 
    {
      Credirect dummy;
      togdb_command("finish");	
	togdb_force_update();
    }
  
  void CMainFrame::OnUpdateS(CCmdUI* pCmdUI) 
    {
      pCmdUI->Enable( togdb_target_has_execution());
      
    }
  
  
  void CMainFrame::OnUpdateSync(CCmdUI* pCmdUI) 
    {
      pCmdUI->Enable( togdb_target_has_execution());
    }
  
  void CMainFrame::OnN() 
    {
      Credirect dummy;
      togdb_command("n");	
	togdb_force_update();
      
    }
  
  void CMainFrame::OnUpdateN(CCmdUI* pCmdUI) 
    {
      pCmdUI->Enable( togdb_target_has_execution());
      
    }
  
  void CMainFrame::OnCont() 
    {
      Credirect dummy;
      togdb_command("c");
      OnSync();
    }
  
  void CMainFrame::OnUpdateCont(CCmdUI* pCmdUI) 
    {
      pCmdUI->Enable( togdb_target_has_execution());	
    }
  
  void CMainFrame::OnUpdateRun(CCmdUI* pCmdUI) 
    {
      pCmdUI->Enable( togdb_target_has_execution());
      
    }
  
  void CMainFrame::OnRun() 
    {
      Credirect dummy;
      togdb_command("r");
      
	togdb_force_update();
    }
  
  
  
  
  
  
  
  /////////////////////////////////////////////////////////////////////////////
    // CMyToolBar
    
    CMyToolBar::CMyToolBar()
      {
      }
  
  CMyToolBar::~CMyToolBar()
    {
    }
  
  
  BOOL CMyToolBar::PreCreateWindow(CREATESTRUCT& cs) 
    {
      // TODO: Add your specialized code here and/or call the base class
      cs.x = 200;
      cs.y = 200;
      
      return CToolBar::PreCreateWindow(cs);
    }
  
  BEGIN_MESSAGE_MAP(CMyToolBar, CToolBar)
    //{{AFX_MSG_MAP(CMyToolBar)
    //}}AFX_MSG_MAP
    END_MESSAGE_MAP()
      
      
      
      
      void CMainFrame::OnSize(UINT nType, int cx, int cy) 
	{
	  
	  CMDIFrameWnd::OnSize(nType, cx, cy);
	  
	  
	}
  
  void CMainFrame::RecalcLayout(BOOL bNotify) 
    {
      // TODO: Add your specialized code here and/or call the base class
#if 0
      
      CRect tr;
      CRect br;
      tr = rect;
      br = rect;
      tr.top += 50;
      tr.bottom = tr.bottom /2;
      br.top = tr.bottom;
      top.SetWindowPos(0,tr.left, tr.top, tr.Width(), tr.Height(), SWP_NOZORDER);
      bottom.SetWindowPos(0,br.left, br.top, br.Width(), br.Height(), SWP_NOZORDER);
      top.RecalcLayout(bNotify);
      bottom.RecalcLayout(bNotify);
#endif
      CRect rect;
      GetClientRect(rect);
      CMDIFrameWnd::RecalcLayout(bNotify);
    }
  
  
  
  void CMainFrame::OnIn() 
    {
      Credirect dummy;
      togdb_command("down");	
      if (selected_frame)
	{
	  theApp_show_at(selected_frame->pc);
	}
    }
  
  void CMainFrame::OnUpdateIn(CCmdUI* pCmdUI) 
    {
      pCmdUI->Enable( togdb_target_has_execution());
      
    }
  
  
  void CMainFrame::OnOut() 
    {
      Credirect dummy;
      togdb_command("up");	
      if (selected_frame)
	{
	  theApp_show_at(selected_frame->pc);
	}
    }
  
  void CMainFrame::OnUpdateOut(CCmdUI* pCmdUI) 
    {
      pCmdUI->Enable( togdb_target_has_execution());
      
    }
  void nprops();
  
  void CMainFrame::OnProperties()
    {
      nprops();
    }
  int what;
  void CMainFrame::OnUpdateWhat(CCmdUI *pCmdUI)
    {
      char b[100];
      sprintf(b,"what %d", what);
      pCmdUI->SetText(b);
    } 
  
  
  extern "C" 
    {
      char  serial_port[];
    };
  
  
  void CMainFrame::OnTargetRemote()
    {
      CString x ("target remote ");
      Credirect dummy;
      x += serial_port;
      togdb_command(x);
	togdb_force_update ();
    }
  
  void CMainFrame::OnTargetSimulator()
    {
      Credirect dummy;
      togdb_command("target sim");	
    }
  
  
  void cd (const char *name)
    {
      if (name[1]== ':')
	{
	  _chdrive (toupper(name[0]) - 'A' + 1);
	  name += 2;
	}
  _chdir (name);
}

void CMainFrame::OnReadScript()
{
  /* The FileDialog will change the working directory - we fix that here.. */
  CString old_dir = gdb_dirbuf;
 
  CFileDialog x(TRUE, NULL, NULL, 0,"Script files | *.gdb | Script files || *.* ||");
 
  if (x.DoModal()) 
    {
      CString b ("source ");
      b += x.GetPathName();
      togdb_command (b);
    }

  cd (old_dir);
  strcpy (gdb_dirbuf, old_dir);
  current_directory = gdb_dirbuf;
}
