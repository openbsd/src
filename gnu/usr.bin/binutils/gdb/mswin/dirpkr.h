/////////////////////////////////////////////////////////////////////////////
// CMyFileDlg dialog 


class CMyFileDlg : public CFileDialog
{
public:
    
// Public data members

   BOOL m_bDlgJustCameUp;
    
// Constructors

    CMyFileDlg(BOOL bOpenFileDialog, // TRUE for FileOpen, FALSE for FileSaveAs
               LPCSTR lpszDefExt = NULL,
               LPCSTR lpszFileName = NULL,
               DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
               LPCSTR lpszFilter = NULL,
               CWnd* pParentWnd = NULL);
                                          
// Implementation
protected:
    //{{AFX_MSG(CMyFileDlg)
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};


int dirpick(CString &, CWnd *);
