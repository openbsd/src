/* The source doc..

   There is only one source document for all the stuff that's going on
   at one time.  Perhaps when we can deal with more than one session,
   then each session will have it's own doc.

   Each doc has a list of CSrcFile derived things, which are created when gdb
   wants to show something.  When created they are either by wanting
   to show the file associcated with a symtab, or when someone wants
   to open a file which doesn't have a symtab attached.
*/

   

#include "stdafx.h"
#include "srcd.h"

#ifdef _DEBUG														  
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CSrcD

IMPLEMENT_DYNCREATE(CSrcD, CDocument)

int   CSrcD::path_changed = 0;

CSrcFrom::CSrcFrom()
 {
  maxlines =0;
  maxwidth = 0;
}

int CSrcFrom::get_height()
{
return maxlines;
}
int CSrcFrom::get_width()
{
return maxwidth;
}
CSrcFrom::~CSrcFrom()
{

}
CSrcLine *CSrcFrom::get_line(int i)
{
if (i >= 0 && i < maxlines)
  {
    return (CSrcLine *)lines[i];
  }
return 0;
}
void
CSrcFrom::add(CSrcLine *p)
{
  maxlines++;
  lines.Add(p);
  int n = strlen(p->text);
  if (n > maxwidth)
    maxwidth = n;
}

CSrcFile::CSrcFile(const char *t)
{
  first_pc = 0;
  last_pc = 0;
   
  func_only = 0;
  notthere = 0;
  head = 0;
  source_available = 0;
  assembly_available = 0;
  title = t;
} 

CSrcFileBySymtab::CSrcFileBySymtab(CSymtab *s) : CSrcFile(s->to_filename())
{
  symtab = s;
  assembly_available = 1;
}

CSrcFileByFilename::CSrcFileByFilename(const char *path) : CSrcFile(path)
{
filename = path;
}

CSrcD::CSrcD()
{



}

void CSrcFrom::reset_contents()
{
  maxlines = 0;
  maxwidth = 0;
  lines.RemoveAll();
}

void CSrcFile::delete_lines()
{
  CSrcLine *p = head;
  while (p) {
    CSrcLine *n = p->next;
    delete p;
    p = n;
  }
  head = 0;

  src.reset_contents();
  asml.reset_contents();
  both.reset_contents();

}

CSrcFile::~CSrcFile()
{
  delete_lines();
}

CSrcFileBySymtab::~CSrcFileBySymtab()
{
}

CSrcFileByFilename::~CSrcFileByFilename()
{
}

void 
CSrcD::delete_list()
{
  POSITION p;
  p = list.GetHeadPosition();
  while (p)
    {
      CSrcFile *f = (CSrcFile *)list.GetNext(p);
      delete f;
    }
}

CSrcD::~CSrcD()
{
  delete_list();
}



BOOL CSrcD::OnNewDocument()
{
  if (!CDocument::OnNewDocument())
    return FALSE;
  return TRUE;
}



BEGIN_MESSAGE_MAP(CSrcD, CDocument)
     //{{AFX_MSG_MAP(CSrcD)
     //}}AFX_MSG_MAP
     END_MESSAGE_MAP()
     
     
     
     /////////////////////////////////////////////////////////////////////////////
     // CSrcD diagnostics
     

void CSrcD::Serialize(CArchive& ar)
{
  
}


void CSrcFile::cantfind()
{
  int i;
  for (i = 0; i < 1000; i++) 
    {
      CSrcLine *n = new CSrcLine(&head, 0,0,0,"");
      src.add(n);
      n->line = i;
    }
  assembly_available = 1;	
  source_available = 0;	
  notthere = 1;
}
void CSrcFile::read(CFile &file)
{
  char inbuf[1024];
  char buf[200+2];
  int idx =0;
  int len;
  char l;
  source_available = 1;
  while (len = file.Read (inbuf, sizeof(inbuf)))
    { 
      int j;
      for (j = 0; j < len; j++)	 
	{
	  l = inbuf[j];
	  if (l == '\t') 
	    {
	      buf[idx++] = ' ';
	      while ((idx & 7) && (idx < sizeof(buf)-5))
		buf[idx++] = ' ';		
	    }
	  else if (l == '\n') 
	    {
	      buf[idx++] = ' ';
	      buf[idx++] = 0;
	      CSrcLine *p = new CSrcLine(&head, 0,0,0,buf);
	      p->line = src.get_height();
	      src.add(p);
	      idx = 0;
	    }
	  else if (l == '\r')
	    {
	    }
	  else 
	    {
	      if (idx < sizeof(buf)-5)
		{
		  buf[idx] = l;
		  idx++;	
		  buf[idx] = 0;
		}
	    }
	}
    }
  for (int i = 0; i < 2; i++) 
    {
      CSrcLine *n = new CSrcLine(&head, 0,0,0,""); 
      n->line = src.get_height();
      src.add(n);
    }
}

/////////////////////////////////////////////////////////////////////////////
// CSrcD commands



BOOL CSrcFileBySymtab::suck_document()
{
  CString fname = symtab->to_fullname();

  /* Look up the line number info too */
  struct lineinfo *qorig = symtab->calc_lineinfo();
  int   i = 0;
  CSrcLine *prev = 0;
  if (qorig)
    {
      struct lineinfo *q = qorig;
      
      while (q->pc)
	{
	  if (q->line >= 0 
	      && 
	      (q->line < src.get_height()))
	    {
	      CSrcLine *sr = src.get_line(q->line-1);
	      if (sr && sr->from == 0) 
		{
		  sr->from = q->pc;
		  if (prev)
		    prev->to = sr->from -1;
		  prev = sr	   ;
		};
	      if (q->pc < first_pc
		  || first_pc == 0)
		first_pc = q->pc;
	      if (q->pc > last_pc)
		last_pc = q->pc;
	    }
	  q++;
	}
    }
  if (prev)
    prev->to = last_pc + 10;
  togdb_free_lineinfo (qorig);
  
  return TRUE;
}

CSrcLine::~CSrcLine()
{
  free ((void *)text);
}
CSrcLine::CSrcLine(CSrcLine **n, int aisasm, CORE_ADDR apc, CORE_ADDR aendpc, const char *p)
{
  char tb[200];
  const  char *src  = p;
  char *dst = tb;
  next = *n;
  *n = this;
  int i = 0;
  while (*src && i < sizeof(tb)-9) 
    {
      if (*src == '\t') 
	{
	  dst[i++] = ' ';
	  while (i & 7)
	    dst[i++] = ' ';
	}
      else 
	dst[i++] = *src;
      src++;
    }
  dst[i++] = 0;
  isasm = aisasm;
  text = strdup (tb);
  from = apc;
  to = aendpc;
}



void CSrcFileBySymtab::disassemble (CORE_ADDR from, int length)
{
  int its = 0;
  int done = 0;
  char buf[200];

  while (done < length && its < 2000) 
    {
      CORE_ADDR x = from + done;
      int len= togdb_disassemble (x, buf);
      done += len;
      CSrcLine *p = new CSrcLine(&head, 1,x,  x + len -1, buf);
      asml.add(p);
      both.add(p);
      its++;
    }
}
void CSrcFileByFilename::preparedisassemble()
{
}
void
CSrcFileBySymtab::preparedisassemble()
{
  /* run though the source lines and build a new list of lines
     with source and assembly mingled */
  int i;
  if (asml.get_height())
    return;
  
  for (i= 0; i < src.get_height(); )
    {
      CSrcLine *p = src.get_line(i);
      both.add(p);
      if (p->from== 0)
	i++;
      else {
	/* Run forward and see how many bytes this line takes */
	int j = i + 1;
	CSrcLine *n = src.get_line(j);
	while (n && n->from == 0)
	  {
	    j++;
	    n = src.get_line(j);
	  }
	/* All the bytes from p->pc to n->pc-1 belong to source line j */
	/* Build the disassembly */
	if (n)
	  disassemble (p->from, n->from - p->from);
	i = j;
      }
    }
} 

extern CBreakInfoList the_breakinfo_list;

void CSrcFileByFilename::toggle_breakpoint(CSrcLine *p) {}

void CSrcFileBySymtab::toggle_breakpoint(CSrcLine *p)
{
  Credirect dummy;
  if (p->bpt_ok()) 
    {
      int bindex = the_breakinfo_list.GetPCIdx(p->from);
      if (bindex>=0) 
	{
	  /* already there, so delete it */
	  CBreakInfo *bi = the_breakinfo_list.GetAt(bindex);
	  the_breakinfo_list.Delete(bindex);
	}
      else
	{
	  char buf[200];
	  if (p->isasm) 
	    {
	      sprintf(buf, "*0x%s",paddr(p->from));
	      togdb_bpt_set(buf);
	    }
	  else 
	    {
	      int x = p->line+1;
	      int j;
	      j = sprintf(buf, "%s:", title);
	      sprintf(buf+j,"%d",x);
	      togdb_bpt_set(buf);
	    }
	}
    }
  //  UpdateAllViews(NULL, (LPARAM)(p->from));
}

int CSrcLine::bpt_ok()
{
  return from != 0;
}

void CSrcD::sync_bpts()
{
  UpdateAllViews(NULL, (LPARAM)-1);
}


void CSrcD::sync_pc()
{
  /* There are two views in every doc, the tab along the
     top and the split.  */
  UpdateAllViews(NULL, (LPARAM)togdb_fetchpc());
}


void CSrcD::show_at(CORE_ADDR x)
{
  UpdateAllViews(NULL, (LPARAM)x);
  
}


void CSrcD::read_src_by_filename(const char *path)
{
  CSrcFileByFilename *p = new CSrcFileByFilename(path);
  CFile file;
  
  if (file.Open(path, CFile::modeRead)) 
    {
      p->read(file);
    }
  else
    {
      p->cantfind();
    }
//  p->suck_document ();
  list.AddTail(p);
}

void CSrcFileBySymtab::read_src_by_symtab(CSymtab *symtab)
{
  symtab->search_for_fullname();
  if (symtab->to_fullname() == NULL)
    {
      cantfind();
    }
  else 
    {
      CFile file;
      if (file.Open(symtab->to_fullname(), CFile::modeRead)) 
	{
	  read(file);
	}
      else
	{
	  cantfind();
	}
    }
  suck_document ();
}

void CSrcD::read_src_by_symtab(CSymtab *symtab)
{
  CSrcFileBySymtab *p = new CSrcFileBySymtab(symtab);
  p->read_src_by_symtab(symtab);
  list.AddTail(p);
}

void CSrcD::func(const char *path,
		 CORE_ADDR from,
		 CORE_ADDR to)
{
  CSrcFileByFilename *p = new CSrcFileByFilename(path);
  p->func_only = 1;
  p->assembly_available = 1;	
//  p->disassemble(from, to-from);
  list.AddTail(p);
}



#if 1
CSrcFile *CSrcD::lookup_title(const char * title, int *in)
{
  *in = 0;
  POSITION p;
  CSrcFile *f = 0;
  p = list.GetHeadPosition();
  while (p)
    {
      f = (CSrcFile *)list.GetNext(p);
      if (strcmp(f->title, title) == 0)
	return f;
      (*in)++;
    }
  return 0;
}
#endif


CSymtab *CSrcFileBySymtab::get_symtab()
{
  return symtab;
}

CSymtab *CSrcFileByFilename::get_symtab()
{
  return 0;
}
CSrcFile *CSrcD::lookup_symtab(CSymtab *tab, int *in)
{
  *in = 0;
  POSITION p;
  CSrcFile *f = 0;
  p = list.GetHeadPosition();
  while (p)
    {
      f = (CSrcFile *)list.GetNext(p);	
	  if (f->title == tab->filename)
	  	return f;
      if (f->get_symtab() == tab)
	return f;
      (*in)++;
    }
  return 0;
}
#if 1
CSrcFile *CSrcD::lookup_by_index(int n)
{
  POSITION p;
  CSrcFile *f = 0;
  p = list.GetHeadPosition();
  while (p)
    {
      f = (CSrcFile *)list.GetNext(p);
      if (n == 0)
	return f;
      n--;
    }
  return 0;
}
#endif


BOOL CSrcD::OnOpenDocument(const char *p)
{
#if 0
  read_src_by_filename(p,p);
  getrecentname = strdup(p);
  //  UpdateAllViews(NULL, 0);
#endif
  return TRUE;
}



void CSrcD::remove_file(CSrcFile *f)
{
  POSITION pos =  list.Find(f);
  if (pos) {
    list.RemoveAt(pos);
    delete f;
  }
}

void CSrcFileBySymtab::reread()
{
  delete_lines();
  read_src_by_symtab(symtab);
}

void CSrcD::makesure_fresh()
{
  if (CSrcD::path_changed != path_now)
    {
      path_now = path_changed;
      POSITION p;
      p = list.GetHeadPosition();
      while (p)
	{
	  CSrcFile *f = (CSrcFile *)list.GetNext(p);
	  CSrcFileBySymtab *p = f->get_CSrcFileBySymtab();
	  if (p)
	    p->reread();

	}
    
  UpdateAllViews(NULL,0);
  }
}


CSrcFileBySymtab *
CSrcFileBySymtab::get_CSrcFileBySymtab() { return this; }
CSrcFileBySymtab *
CSrcFileByFilename::get_CSrcFileBySymtab() { return 0; }



CSrcFileByFilename *
CSrcFileBySymtab::get_CSrcFileByFilename() { return 0; }
CSrcFileByFilename *
CSrcFileByFilename::get_CSrcFileByFilename() { return this; }
