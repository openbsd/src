// infodoc.cpp : implementation of the CGnuInfoDoc class
//

#include "stdafx.h"
#include "ginfodoc.h"


/////////////////////////////////////////////////////////////////////////////
// CGnuInfoDoc

IMPLEMENT_DYNCREATE (CGnuInfoDoc, CDocument)

BEGIN_MESSAGE_MAP (CGnuInfoDoc, CDocument)
	//{{AFX_MSG_MAP(CGnuInfoDoc)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP ()

//////////////////////////////////////////////////////////////////////
// CGnuInfoFile


CGnuInfoFile::CGnuInfoFile()
{


  idx = 0;
  len = 0;
  idx = 0;
  off = 0;
  next = 0;

}
void CGnuInfoFile::set(long where)
{

  idx = 0;
  len = 0;
  off = 0;
  idx =0;
  len = 0;
  off = where;
  file.Seek(off, CFile::begin);

}
long CGnuInfoFile::where()
{
  return off;

}

int CGnuInfoFile::gc()
{ 

  if (idx >= len) {
    /* Refill buffer */
    len = file.Read(buf, BSIZE);
    
    if (len == 0)
      return -1;
    
    idx = 0;
  }
  off++;

  return buf[idx++];
}




//////////////////////////////////////////////////////////////////////
// CGnuInfoNode

CGnuInfoNode::CGnuInfoNode(const char *n)
{

  name = n;
  prev = 0;
  next = 0;
  up = 0;

file = 0;

}

int CGnuInfoNode::GetPageDepth()
{


  int depth = 0;
  CGnuInfoNode *n = this;
  while (n && depth < 7) {
    depth++;
    n = n->up;
  }

  return depth - 2;
}


//////////////////////////////////////////////////////////////////////
// CGnuInfoDoc

CGnuInfoNode *CGnuInfoDoc::LookupNode (const char *name)
{

  void *p;
  if (!map.Lookup(name, p))
    {
      CGnuInfoNode *  node = new CGnuInfoNode(name);
      map.SetAt (name, node);
      return node;
    }
  else
    {
      return (CGnuInfoNode *)p;
    }


}




CGnuInfoDoc::~CGnuInfoDoc ()
{ 
}

BOOL CGnuInfoDoc::OnNewDocument ()
{ 

  if (!CDocument::OnNewDocument ())
    return FALSE;
  
  // TODO: add reinitialization code here
  // (SDI documents will reuse this document)
  

  return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CGnuInfoDoc serialization

void CGnuInfoDoc::
Serialize (CArchive & ar)
{

  if (ar.IsStoring ())
    {
      // TODO: add storing code here
    }
  else
    {
      // TODO: add loading code here
    }

}


/////////////////////////////////////////////////////////////////////////////
// CGnuInfoDoc commands

static int get(char *p, int i, char *want, char **dst)
{ 

  *dst = "*empty*";
  while (p[i] == ' ' || p[i] == '\t')
    i++;
  int j;
  for (j = 0; want[j]; j++)
    if(p[i+j] != want[j])
      return i;
  
  i+=j+1;
  
  while (p[i] == ' ')
    i++;
  *dst = p+i;
  while (p[i] != 0 && p[i] != '\n' && p[i] != ',')
    i++;
  p[i] = 0;
  i++;

  return i;
}


/////////////////////////////////////////////////////////////////////////////
// CGnuInfoDoc construction/destruction

CGnuInfoDoc::CGnuInfoDoc ()
{

  // TODO: add one-time construction code here
  open = 0;
  npages = 0;


  // Initial size of 128 entries, and
  // grow by same 
  nodes = 0;
  head = 0;
  vector.SetSize(128,128);

}

void CGnuInfoDoc::look(char *name, int *ptr)
{

  void *res;
  if (map.Lookup(name, res)) {
    *ptr = (int )res;
  }
  else
    *ptr = -1;

}

/* read all the stuff from the file and build a node list */

void CGnuInfoNode::init(CGnuInfoNode *u, 
		     CGnuInfoNode *p,
		     CGnuInfoNode *n,
		     CGnuInfoFile *f,
		     int o,
		     int pa)
{

  up= u;
  prev = p;
  next = n;

  file = f;
  offset = o;
  pagenumber= pa;

}


BOOL CGnuInfoDoc::OnOpenDocument(LPCTSTR lpszPathName) 
{
  char *res;
  if (togdb_searchpath(togdb_get_info_path(),
		       0, lpszPathName, &res))
    lpszPathName = res;

  if (!CDocument::OnOpenDocument(lpszPathName))
    return FALSE;
  
  head = new CGnuInfoFile();
  scanfile (head, lpszPathName, lpszPathName);

  return TRUE;
}


int CGnuInfoDoc::scanfile(CGnuInfoFile *file,
		       const char *name,
		       const char *parent)
{
  if (!file->file.Open(name, CFile::modeRead| CFile::typeBinary))
    {
      /* Cant find this file, try again with the parents root */
      CString trypath ;
      trypath = parent;
      int l = trypath.ReverseFind('\\');
      l ++;
      trypath = trypath.Left(l);
      trypath += name;
      if (!file->file.Open(trypath, CFile::modeRead| CFile::typeBinary))
	return FALSE;
    }
  int  c = file->gc();
  while (c > 0) 
    {
      if (c != 037)
	c = file->gc();
      else 
	{
	  long off = file->where();
	  int len = 0;
	  char buf1[200];
	  c = file->gc(); 
	  c = file->gc();
	  while (c != '\n' && len < sizeof(buf1)-1) {
	    buf1[len++] = c;
	    c = file->gc();
	  }
	  buf1[len]=0;
	  /* Scan out the node if there is one */
	  if (buf1[0] == 'I') 
	    {

	      c = file->gc();
	      /* This is the start of an indirect file list, process
		 each sub file too until another esc is reached */
	      while (c != 037)
		{
		  int  i = 0; 
		  while (c != ':')  {
		    buf1[i++] = c;
		    c = file->gc();
		  }
		  buf1[i++] = 0;
		  while (c != '\n')
		    c = file->gc();
		  c = file->gc();
		  CGnuInfoFile *n = new CGnuInfoFile();
		  if (!	scanfile (n,buf1, parent)) {
		    char buf[20];
		    sprintf(buf,"not found %s", buf1);
		    CGnuInfoNode *node = new CGnuInfoNode(buf);
		    node->init (0,0,0, file, off, npages);
		    vector.SetAtGrow(npages, node);
		    npages++;
		  }
		  else {
		    n->next = head;
		    head = n;
		  }
		}
	    }
	  else
	    if (buf1[0] == 'F') 
	      {
		char *f;
		char *next;
		char *prev;
		char *nodename;
		char *up;
		int i =0;
		i = get(buf1, i, "File:",  &f);
		i = get(buf1, i, "Node:", &nodename);
		i = get(buf1, i, "Next:", &next);
		i = get(buf1, i, "Prev:", &prev);
		i = get(buf1, i, "Up:", &up);

		CGnuInfoNode *node = LookupNode(nodename);
		node->init (LookupNode(up),
			    LookupNode(prev),
			    LookupNode(next), file, file->where(), npages);
		if (!nodes)
		  nodes = node;
		vector.SetAtGrow(npages, node);
		npages++;
	      }
	}
    }
  open = 1;

  return TRUE;
}

void CGnuInfoDoc::GetPage(class CGnuInfoNode *want, CStringArray &p)
{

  if (want == 0 || !open || want->file == 0) 
    {
      p.RemoveAll();
      p.Add ("Closed");
    }
  else {
    /* Build a string array in the target space,
       I tried using CString but it was too slow  */
    char buf[200];
    p.RemoveAll();
    want->file->set (want->offset);
    int  c = want->file->gc(); 
    while (c > 0 && c != 037) {
      int i = 0;
      while (c != '\n' && i < sizeof(buf)-1) {
	buf[i++] = (char)c;
	c = want->file->gc();
      }
      buf[i] = 0;
      c = want->file->gc();
      p.Add (buf);
    }
  }

}

CString CGnuInfoDoc::GetPageName(int n)
{ 
  return ((CGnuInfoNode *)vector[n])->name;
}

CGnuInfoNode *CGnuInfoDoc::GetNode (int i)
{ 
  return (CGnuInfoNode *)vector[i];
}
void CGnuInfoDoc::OnCloseDocument() 
{

  CGnuInfoFile *p,*q;
  
  /* close and delete all the files */
  if (p = head) 
    do {
      q = p->next;
      p->file.Close();
      delete p;
      p = q;
    } while (p);

  /* Kill all the nodes - have to look through the map
     rather than the vector, because some nodes don't get pages ma*/
  POSITION i = map.GetStartPosition(); 
  while (i )
    {
      CString key;
      void *value;
      map.GetNextAssoc(i, key, value);
      delete ((CGnuInfoNode *)value);
    }
  CDocument::OnCloseDocument();
}



