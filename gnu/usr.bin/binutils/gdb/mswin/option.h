// option.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionsSheet

/////////////////////////////////////////////////////////////////////////////
// COptionsSerial dialog

class COptionsSerial : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionsSerial)


// Construction
public:
	COptionsSerial();
	~COptionsSerial();
    static void Initialize();
  static void Terminate();
  void  from_gdb();
void to_gdb();

// Dialog Data
	//{{AFX_DATA(COptionsSerial)
	enum { IDD = ID_SYM_DIALOG_SER_DIALOG };
	int		m_baud;
	int		m_parity;
	int		m_com;
	int		m_databits;
	int		m_stopbits;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionsSerial)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionsSerial)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
// COptionsColor dialog

class CMyBitmapButton : public CBitmapButton
{
  COLORREF col;
 public:
  void set_color(COLORREF c) {col = c;}
  virtual void DrawItem(LPDRAWITEMSTRUCT lpDIS); 
};
class COptionsColor : public CPropertyPage
{
  DECLARE_DYNCREATE(COptionsColor)
#define NC 20
    CMyBitmapButton bs[NC];

  // Construction
public:
  COptionsColor();
  ~COptionsColor();
  void c(int x);
  int sel;  
  void highlight(int x);
  int findb(int sel);
  // Dialog Data
  //{{AFX_DATA(COptionsColor)
	enum { IDD = ID_SYM_DIALOG_COLOR_DIALOG };
	CListBox	m_list;
	//}}AFX_DATA
  
  
  // Overrides
  // ClassWizard generate virtual function overrides
  //{{AFX_VIRTUAL(COptionsColor)
protected:
  virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  // Generated message map functions
  //{{AFX_MSG(COptionsColor)
  afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
  afx_msg void OnC0();
  afx_msg void OnC1();
  afx_msg void OnC2();
  afx_msg void OnC3();
  afx_msg void OnC4();
  afx_msg void OnC5();
  afx_msg void OnC6();
  afx_msg void OnC7();
  afx_msg void OnC8();
  afx_msg void OnC9();
  afx_msg void OnC10();
  afx_msg void OnC11();
  afx_msg void OnC12();
  afx_msg void OnC13();
  afx_msg void OnC14();
  afx_msg void OnC15();
  afx_msg void OnC16();
  afx_msg void OnC17();
  afx_msg void OnC18();
  afx_msg void OnC19();
  virtual BOOL OnInitDialog();
  afx_msg void OnDblclkWinds();
  afx_msg void OnSetfocusWinds();
	afx_msg void OnSelchangeWinds();
	afx_msg void OnEditupdateWinds();
	//}}AFX_MSG
  DECLARE_MESSAGE_MAP()
    
  };

/////////////////////////////////////////////////////////////////////////////
// COptionsSer

/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// COptionsFont dialog

class COptionsFont : public CPropertyPage
{
  DECLARE_DYNCREATE(COptionsFont)
    // Construction
  public:
  COptionsFont();
  ~COptionsFont();
  char *tfont;
  LOGFONT showing;
  LOGFONT *showup;
  void show_fonts();
  void show_styles(const char*);
  void highlight_font();
  int first_call ;
  int pass; 
  int iteration;
  void enumerate_sizes( );
  void show_one_size();
  int doshow;
  
  void enable_okones();
  CFont curfont;
  // Dialog Data
  //{{AFX_DATA(COptionsFont)
  enum { IDD = ID_SYM_DIALOG_FONT_DIALOG };
  CListBox	font_style;
  CListBox	font_size;
  CListBox	font_name;
  int prev_font_name_index;

  int prev_font_window_index;
  int prev_font_style_index;
  int prev_font_size_index;
  BOOL	show_unfixed;

  CListBox	font_window;

  CEdit	font_demo;
  //}}AFX_DATA

  // Overrides
  // ClassWizard generate virtual function overrides
  //{{AFX_VIRTUAL(COptionsFont)
protected:
  virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  // Generated message map functions
  //{{AFX_MSG(COptionsFont)

  afx_msg void OnSelchangeFontName();
  afx_msg void OnSelchangeFontSize();
  afx_msg void OnSelchangeFontStyle();


  afx_msg void OnSelchangeFontWindow();
  virtual BOOL OnInitDialog();
  afx_msg void OnShowUnfixed();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()

  };


// options.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// COptionsDir dialog

class COptionsDir : public CPropertyPage
{
  DECLARE_DYNCREATE(COptionsDir)
    static void Initialize();
  static void Terminate();
  void move(int);

  // Construction
public:
  void fillup(const char *s, CListBox *p);
  int prev_sel;
  void remember(int );
  COptionsDir();
  ~COptionsDir();
  
  // Dialog Data
  //{{AFX_DATA(COptionsDir)
	enum { IDD = ID_SYM_DIALOG_DIR_DIALOG };
	CListBox	forwhat;
  CButton	up;
  CButton	remove;
  CButton	down;
  CButton	add;
  CListBox	path_list;
	//}}AFX_DATA


  // Overrides
  // ClassWizard generate virtual function overrides
  //{{AFX_VIRTUAL(COptionsDir)
protected:
  virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support
  virtual void OnOK();
  //}}AFX_VIRTUAL
  
  // Implementation
protected:
  // Generated message map functions
  //{{AFX_MSG(COptionsDir)
  virtual BOOL OnInitDialog();
  afx_msg void OnAdd();
  afx_msg void OnDown();
  afx_msg void OnRemove();
  afx_msg void OnUp();
  afx_msg void OnSelchangePath();
  afx_msg void OnSelchangePathForWhat();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()

  };

class COptionsSheet : public CPropertySheet
{
  DECLARE_DYNAMIC(COptionsSheet)
    static void Initialize();
  static void Terminate();
  COptionsColor colors;
  COptionsFont fonts;
  COptionsDir dirs;  
  COptionsSerial ser;
  // Construction
  public:
  COptionsSheet();
  
  // Attributes
  public:
  
  // Operations
  public:
  
  // Overrides
    // ClassWizard generated virtual function overrides
      //{{AFX_VIRTUAL(COptionsSheet)
	    //}}AFX_VIRTUAL
  
	      // Implementation
	      public:
  virtual ~COptionsSheet();
  
  // Generated message map functions
  protected:
  //{{AFX_MSG(COptionsSheet)
	// NOTE - the ClassWizard will add and remove member functions here.
  
	  //}}AFX_MSG
	    DECLARE_MESSAGE_MAP()
};
/////////////////////////////////////////////////////////////////////////////
// COptionsAddPath dialog

class COptionsAddPath : public CDialog
{
 // Construction
 public:
 COptionsAddPath(CWnd* pParent = NULL);	// standard constructor
 
 // Dialog Data
 //{{AFX_DATA(COptionsAddPath)
 enum { IDD = ID_SYM_DIALOG_ADD_PATH };
 CString	path;
 //}}AFX_DATA
 
 
 // Overrides
 // ClassWizard generated virtual function overrides
 //{{AFX_VIRTUAL(COptionsAddPath)
 protected:
 virtual void DoDataExchange(CDataExchange* pDX); // DDX/DDV support
 //}}AFX_VIRTUAL
 
 // Implementation
 protected:
 
 // Generated message map functions
 //{{AFX_MSG(COptionsAddPath)
 afx_msg void OnBrowse();
 //}}AFX_MSG
 DECLARE_MESSAGE_MAP()
 };
