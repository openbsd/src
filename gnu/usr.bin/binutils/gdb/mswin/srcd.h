// srcdoc.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSrcD document

class CSrcLine 
{
 public:
  int isasm;			/* Set if this is an assembly line */
  int is_source_line() { return !isasm;}
  int is_assembly_line() { return isasm;}
  int line;			/* line number of this line (based at 0) */
  int bpt_ok() ;


  CORE_ADDR from;
  CORE_ADDR to;
  const char *text;
  CSrcLine(CSrcLine **next, int,CORE_ADDR, CORE_ADDR ,const char *text);
  CSrcLine *next;		/* Thread them to make them easy to delete */
  ~CSrcLine();
};

class CSrcFrom
{
  int maxlines;
  int maxwidth;
  CPtrArray lines;
 public:
  CSrcLine *get_line(int n);
  CSrcFrom ();
  ~CSrcFrom();
  int get_height();
  int get_width();
  void add(CSrcLine *p);
  void reset_contents();
};

class CSrcFile  : public CObject
{
 public:
  CSrcFile (const char *);	/* Tab name */
  CSrcFrom src;
  CSrcFrom asml;
  CSrcFrom both; 
  CSrcLine *head;

  int source_available;		
  int assembly_available;	

  int notthere;			/* Set if the source is unavailable,
				   but we have line number debug info */
  int func_only;		/* Set if we've got just enough info for 
				   the range of the function */

  CString title;		/* tab title */
  CORE_ADDR first_pc;
  CORE_ADDR last_pc;
  void delete_lines();
  virtual ~CSrcFile();

  void read(CFile &file);


  void cantfind();
  virtual void toggle_breakpoint(CSrcLine *)  = 0;
  virtual CSymtab *get_symtab() = 0;
  virtual class CSrcFileBySymtab *get_CSrcFileBySymtab() = 0;
  virtual class CSrcFileByFilename *get_CSrcFileByFilename() = 0;
  virtual  void preparedisassemble() = 0;
};



class CSrcFileBySymtab : public CSrcFile
{
 public:
  virtual ~CSrcFileBySymtab();
  class  CSymtab *symtab;	/* Then point to owner */
  CSrcFileBySymtab (CSymtab *symtab);
  void read_src_by_symtab(CSymtab *);
  void preparedisassemble();
  void disassemble(CORE_ADDR from, int length);
  void toggle_breakpoint(CSrcLine *)  ;
  BOOL suck_document();
  virtual CSymtab *get_symtab();
  virtual CSrcFileBySymtab *get_CSrcFileBySymtab();
  virtual CSrcFileByFilename *get_CSrcFileByFilename() ;
  void reread();
}				  ;

class CSrcFileByFilename : public CSrcFile
{
 public:
  virtual  ~CSrcFileByFilename();
  CString filename;
  CSrcFileByFilename(const char *);
  virtual CSymtab *get_symtab();
  virtual CSrcFileBySymtab *get_CSrcFileBySymtab();
  virtual CSrcFileByFilename *get_CSrcFileByFilename() ;
  void preparedisassemble();
  void toggle_breakpoint(CSrcLine *)  ;
}			;

class CSrcFileByAddress : public CSrcFile
{
 public:
  virtual  ~CSrcFileByAddress();
  CORE_ADDR address;
  CSrcFileByAddress(CORE_ADDR address);
  virtual CSymtab *get_symtab();
  virtual CSrcFileBySymtab *get_CSrcFileBySymtab();
  virtual CSrcFileByFilename *get_CSrcFileByFilename() ;
  void preparedisassemble();
  void toggle_breakpoint(CSrcLine *)  ;
}			;

class CSrcD : public CDocument
{
 protected:
 public:
  void read_src_by_filename(const char *path);
  void read_src_by_symtab(CSymtab *tab);
  void func(const char *f, CORE_ADDR from, CORE_ADDR to);
  const char *getrecentname;
  
  CObList list;			/* list of CSrcFiles */
  void delete_list();
  CSrcFile *lookup_title(const char *n, int *p);
  CSrcFile *lookup_symtab(class CSymtab *, int *p);
  CSrcFile *lookup_by_index(int n);

  void makesure_fresh() ;	/* checks to see if source should be re-read */
  static int path_changed;	/* incremented each time the source file path
				   changes */
  int path_now;			/* when it's different to this, re-read */

  void remove_file(CSrcFile *);
  void sync_bpts();
  void sync_pc();
  void show_at(CORE_ADDR x);
 protected:
  CSrcD();			
  DECLARE_DYNCREATE(CSrcD)
    
    // Attributes
  public:
  
  
  // Operations
public:
  // Overrides
  // ClassWizard generated virtual function overrides
  //{{AFX_VIRTUAL(CSrcD)
public:
  virtual BOOL OnOpenDocument(const char *path);
  
protected:
  virtual BOOL OnNewDocument();
  //}}AFX_VIRTUAL
  
  // Implementation
public:
  virtual ~CSrcD();
  virtual void Serialize(CArchive& ar);	// overridden for document i/o
  
  // Generated message map functions
protected:
  //{{AFX_MSG(CSrcD)
  //}}AFX_MSG
  DECLARE_MESSAGE_MAP()
  };

