// regdoc.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CRegDoc document

class CRegDoc : public CDocument
{
 public:
  void Sync();
 protected:
  CRegDoc();			// protected constructor used by dynamic creation
  DECLARE_DYNCREATE(CRegDoc)
    
    class CRegDoc *m_next;
  // Attributes
public:
  
  // Operations
public:

#if defined(TARGET_SH)
#define MAXREGS NUM_REGS
#elif defined(TARGET_H8300)
#define MAXREGS 18	  
#elif defined(TARGET_M68K)
#define MAXREGS 30
#elif defined(TARGET_SPARCLITE)
#define MAXREGS NUM_REGS
#elif defined(TARGET_SPARCLET)
#define MAXREGS NUM_REGS
#elif defined(TARGET_MIPS)
#define MAXREGS NUM_REGS
#elif defined(TARGET_A29K)
#define MAXREGS NUM_REGS
#elif defined(TARGET_I386)
#define MAXREGS NUM_REGS
#elif defined(TARGET_V850)
#define MAXREGS NUM_REGS
#else
#error HEY!  no target defined!
#endif

  int m_regs[MAXREGS];
  int m_regchanged[MAXREGS];
  int m_regsinit[MAXREGS];
  char *memwnd_get_memory(CORE_ADDR off);
  int m_init;
  void prepare();
  void ChangeReg(int rn, int val);
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CRegDoc)
	public:
	virtual void OnCloseDocument();
	protected:
  virtual BOOL OnNewDocument();
	virtual BOOL SaveModified();
	//}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CRegDoc();
  virtual void Serialize(CArchive& ar);	// overridden for document i/o
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CRegDoc)
  afx_msg void OnSync();
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };
