
class CGnuInfoNode
{

 public:
  CString name;			/* name of node */
  CGnuInfoNode *prev;
  CGnuInfoNode *next;
  CGnuInfoNode *up;
  class CGnuInfoFile *file;	/* Which file is the node in */
  int offset;
  int pagenumber;
  void init (CGnuInfoNode *up, CGnuInfoNode *prev, CGnuInfoNode *next,
	     class  CGnuInfoFile *file, int off, int pagenumber);

  CGnuInfoNode(const char *n);
  int GetPageDepth();

};

#define BSIZE 512

class CGnuInfoFile
{

public:
CGnuInfoFile();
  CGnuInfoFile *next;
  CFile file;			
  char buf[BSIZE];
  int gc();
  long where();
  void set(long x);

  int off;
  int idx;
  int len;

};



class CGnuInfoDoc : public CDocument
{


 protected:			// create from serialization only
  CGnuInfoDoc();
  DECLARE_DYNCREATE(CGnuInfoDoc)
    CGnuInfoNode *LookupNode(const char *name);
  // Attributes
public:
  CGnuInfoFile *head;  
  // Operations
public:
  int npages;
  void Next();
  void Prev();
  void info(char *);
  void Up();
  CGnuInfoNode *nodes;
  void look(char *what, int *ptr);
  void GetPage(class CGnuInfoNode *want, CStringArray &p);
  CString GetPageName(int n);
  
  CGnuInfoNode *GetNode (int i);  
  int scanfile (CGnuInfoFile *, const char *, const char *);
  CMapStringToPtr map;
  CPtrArray vector;
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CGnuInfoDoc)
public:
  virtual BOOL OnNewDocument();
  virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
  virtual void OnCloseDocument();
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  /* pointer to last place history read from, or hindex if not */
  
  virtual ~CGnuInfoDoc();
  virtual void Serialize(CArchive& ar);	// overridden for document i/o
  
  
protected:
  void GotoPage(int n);
  void RememberPage(int n);
#define HISTORY 20
  
  
  
  // transaction basis.
  int open;
  
  void InfoWorker (char *&dst, int val, char*name);  
  
  // Generated message map functions

protected:
  //{{AFX_MSG(CGnuInfoDoc)
  
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()



  };

/////////////////////////////////////////////////////////////////////////////
