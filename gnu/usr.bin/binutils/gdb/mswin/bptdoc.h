// CBptDoc document

class CBptDoc : public CDocument
{
 public:
  void Sync();
 protected:
  CBptDoc();			// protected constructor used by dynamic creation
  DECLARE_DYNCREATE(CBptDoc)
    // Attributes
  public:
  
  // Operations
public:
  static void AllSync();
  
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CBptDoc)
protected:
  virtual BOOL OnNewDocument();
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CBptDoc();
  virtual void Serialize(CArchive& ar);	// overridden for document i/o
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CBptDoc)
  afx_msg void OnSync();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };
