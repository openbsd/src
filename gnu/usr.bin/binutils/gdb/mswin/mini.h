class CMiniMDIChildWnd : public CMDIChildWnd 
{ 
	DECLARE_DYNCREATE(CMiniMDIChildWnd) 
 
// Constructors 
public: 
	CMiniMDIChildWnd(); 
	BOOL Create(LPCTSTR lpClassName, LPCTSTR lpWindowName, 
		DWORD dwStyle, const RECT& rect, 
		CWnd* pParentWnd = NULL, UINT nID = 0); 
 
// Implementation 
public: 
	~CMiniMDIChildWnd(); 
 
	//{{AFX_MSG(CMiniMDIChildWnd) 
	afx_msg BOOL OnNcActivate(BOOL bActive); 
	afx_msg void OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpParams); 
	afx_msg UINT OnNcHitTest(CPoint point); 
	afx_msg void OnNcPaint(); 
	afx_msg void OnNcLButtonDown(UINT nHitTest, CPoint pt ); 
	afx_msg void OnLButtonUp(UINT nFlags, CPoint pt ); 
	afx_msg void OnMouseMove(UINT nFlags, CPoint pt ); 
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam); 
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* pMMI); 
	afx_msg LRESULT OnGetText(WPARAM wParam, LPARAM lParam); 
	afx_msg LRESULT OnGetTextLength(WPARAM wParam, LPARAM lParam); 
	afx_msg LRESULT OnSetText(WPARAM wParam, LPARAM lParam); 
	afx_msg BOOL OnNcCreate(LPCREATESTRUCT lpcs); 
	//}}AFX_MSG 
	DECLARE_MESSAGE_MAP() 
 
public: 
	virtual void CalcWindowRect(LPRECT lpClientRect, 
		UINT nAdjustType = adjustBorder); 
 
protected: 
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs); 
 
protected: 
	BOOL m_bSysTracking; 
	BOOL m_bInSys; 
	BOOL m_bActive; 
	CString m_strCaption; 
 
	void InvertSysMenu(); 
}; 
