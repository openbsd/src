// browserl.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CBrowserList window

class CBrowserList : public CListBox
{
 public:
  static void Initialize();
  static void Terminate();
  // Construction
  public:
  CBrowserList();

  // Attributes
  public:

  // Operations
  public:
  int m_show_address;
  int m_show_lines;
  // Overrides
    // ClassWizard generated virtual function overrides
      //{{AFX_VIRTUAL(CBrowserList)
	public:
	    virtual int CompareItem(LPCOMPAREITEMSTRUCT lpCompareItemStruct);
	    virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);
	virtual void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct);
	//}}AFX_VIRTUAL

	      // Implementation
	      public:
  virtual ~CBrowserList();

  // Generated message map functions
  protected:
  //{{AFX_MSG(CBrowserList)
	//}}AFX_MSG

	  DECLARE_MESSAGE_MAP()
	  };

/////////////////////////////////////////////////////////////////////////////
