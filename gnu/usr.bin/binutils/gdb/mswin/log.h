/* stuff for GDB, the GNU debugger.
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

#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus

// CGenericLogView view

class CGenericLogView : public CEditView
{
 protected:
  CGenericLogView();		// protected constructor used by dynamic creation
  DECLARE_DYNCREATE(CGenericLogView);
  

  CGenericLogView **getptr;
  CFontInfo *getfontinfo;
  CGenericLogView(CGenericLogView **, CFontInfo*);		// protected constructor used by dynamic creation
  CString pending;
  
  static buffer_push (CString *);	  
  static buffer_pop ();
virtual int   isiolog() { return 0;};
  // Attributes
public:
  int shown;

  // Operations
public:
  
static  void OnIdle();
  void doidle();
  void add(const char *);
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CGenericLogView)
public:
  virtual void OnInitialUpdate();
protected:
  virtual void OnDraw(CDC* pDC); // overridden to draw this view
  afx_msg void OnSize(UINT nType, int cx, int cy);
  virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  virtual ~CGenericLogView();
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CGenericLogView)
  afx_msg void OnSetFont();
  afx_msg void OnEditChange();
  afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
  afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
  afx_msg void OnDestroy();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };



class CIOLogView : public CGenericLogView
{
 protected:
  CIOLogView();	

  DECLARE_DYNCREATE(CIOLogView);
 public:
  static void Initialize();
  static void Terminate();
int isiolog () { return 1; }

};

extern "C" 
{
  void CIOLogView_output (const char *);
};

class CCmdLogView : public CGenericLogView
{
 protected:

  CCmdLogView();
  DECLARE_DYNCREATE(CCmdLogView);
 public:
  static void Initialize();
  static void Terminate();
int isiolog () { return 0; }

};



extern CIOLogView *iowinptr;
extern CCmdLogView *cmdwinptr;


#else /* __cplusplus */
  void CIOLogView_output (const char *);
#endif /* __cplusplus */

#endif /* LOG_H */
