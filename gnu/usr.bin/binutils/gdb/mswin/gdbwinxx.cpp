/* C++ wrappers for gdb structures. */


#include <afxwin.h>
#include <setjmp.h>
#include "stdafx.h"

/* Rebuild the CBreakInfo structure from inside gdb */
void CBreakInfoList::Update()
{
  void *i;
  int idx;


  for (i = togdb_breakinfo_i_init(), idx = 0;
       i;
       i = togdb_breakinfo_i_next(i), idx++)
    {
	m_info.SetAtGrow(idx, i);
    }
	m_info.SetSize(idx,20);

}
 
#if 0
int CBreakInfo::GetNumber() {	return bi_number((void *)this);}
int CBreakInfo::GetType() { return bi_type((void *)this); }
int CBreakInfo::GetDisposition() { return bi_disposition((void *)this); }
int CBreakInfo::GetEnable() { return bi_enable((void *)this); }
int CBreakInfo::GetHitCount() { return bi_hitcount((void *)this); }
CORE_ADDR CBreakInfo::GetAddress() { return bi_address((void *)this); }
int CBreakInfo::GetLineNumber() { return bi_linenumber((void *)this); }
const char * CBreakInfo::GetSourceFile() { return bi_sourcefile((void *)this); }
const char * CBreakInfo::GetCondString() { return bi_condstring((void *)this); }
const char * CBreakInfo::GetAddrString() { return bi_addrstring((void *)this); }
const char * CBreakInfo::GetExpString() { return bi_expstring((void *)this); }

#endif

void CBreakInfoList::Delete(int idx)
{
	bi_delete_breakpoint ((void *)GetAt(idx));
}

void CBreakInfoList::DeleteAll()
{
	bi_delete_all();
}


void CBreakInfoList::Disable(int idx)
{
	bi_disable_bpt ((void *)GetAt(idx));
}


void CBreakInfoList::Enable(int idx)
{
	bi_enable_bpt ((void *)GetAt(idx));
}



CBreakInfoList::CBreakInfoList()
{
  m_info.SetSize(0,20);
}

CBreakInfo *CBreakInfoList::GetAt(int idx)
{
  return (CBreakInfo *)(m_info.GetAt(idx));
}

int CBreakInfoList::GetSize()
{
return m_info.GetSize();
}

/* Find the breakpoint info at the given address */
int CBreakInfoList::GetPCIdx(CORE_ADDR pc)
{
int i;
int max = m_info.GetSize();

for (i = 0; i < max; i++) {	
	if (GetAt(i)->GetAddress() == pc)
	return i;
}
return -1;
}


#if 0
CFrameInfo * CFrameInfo::GetPrevFrame()
{
  return (CFrameInfo *)togdb_frameinfo_prevframe(castme());
}

CORE_ADDR CFrameInfo::GetFrameAddr()
{
  return togdb_frameinfo_frameaddr(castme());
}

CORE_ADDR CFrameInfo::GetFramePC()
{
  return togdb_frameinfo_framepc(castme());
}

CFrameInfo *CFrameInfo::GetCurrentFrame()
{
  return (CFrameInfo *)(togdb_frameinfo_getcurrentframe());
}
#endif
	 static int top_level_val;

/* Do a setjmp on error_return and quit_return.  catch_errors is
   generally a cleaner way to do this, but main() would look pretty
   ugly if it had to use catch_errors each time.  */
	extern "C" {
	extern	jmp_buf error_return;
extern	jmp_buf quit_return;
		}
		;
#define SET_TOP_LEVEL() \
  (((top_level_val = setjmp (error_return)) \
    ? (void *) 0 : (void *) memcpy (quit_return, error_return, sizeof (jmp_buf))) \
   , top_level_val)
//////////////////////////////////////////////////////////////////////
CString togdb_eval_as_string(const char *p)
{
  CString x;
  Credirect j(&x);
  {
 			  if (!SET_TOP_LEVEL()) {
    			togdb_eval_as_string_worker(p);
    			}
				else { x="unavailable";}
  }
  return x;
}	



const char *CValue::GetEnumName() 
{
  int len = TYPE_NFIELDS ( type);
  LONGEST val = unpack_long (type, VALUE_CONTENTS(this));
  int i;
  for (i = 0; i < len; i++)
    {
     if (val == TYPE_FIELD_BITPOS (type, i))
       {
	return TYPE_FIELD_NAME (type, i);
      }
   }
  return "*";
}

CLineInfo *CSymtab::calc_lineinfo()
{
  struct lineinfo *n 
    = (struct lineinfo *) (malloc (sizeof (struct lineinfo) 
				   * (linetable->nitems + 2)));
  int i;
	  int inc = 0;
  /* attribute all the code to before the first line
     to the line before it */
  if (blockvector) 
    {
      struct block *bl = blockvector->block[0];
      if (bl) 
	{
	  n[0].pc =   bl->startaddr;
	  n[0].line = linetable->item[0].line -3 ;
	  inc = 1;
	}
    }

  for  (i = 0; i <linetable->nitems ; i++) 
    {
      n[i+inc].line = linetable->item[i].line;
      n[i+inc].pc   = (CORE_ADDR) linetable->item[i].pc;
    }



  n[i+inc].pc = 0;
  n[i+inc].line = 0;
  return (CLineInfo *)n;
}


const int CSymbol::GetLine()
{
 struct symtab_and_line sal;
 if (! SYMBOL_LINE(this)) 
   {
    sal = find_pc_line(GetValue(), 1);
    SYMBOL_LINE(this) = sal.line;
  }
 return SYMBOL_LINE(this);
}

CORE_ADDR CSymbol::GetValue()
{
 if (aclass == LOC_BLOCK) 
   {
    return SYMBOL_BLOCK_VALUE(this)->startaddr;
  }
 return (~0);
}


void togdb_command (CString x)
{

}
