#if 0
#include <stdio.h>

typedef void *CORE_ADDR;
typedef FILE GDB_FILE;
typedef long LONGEST;
#define PARAMS(x) x

extern "C" 
{
#include "tm.h" 
//#include "frame.h"
#include "breakpoint.h"
#include "symtab.h"
#include "gdbtypes.h"
};


#include "gdbwrap.h"

//SYNTAX ERROR

///////////////////////////////////////////////////////////////////////
// symbol wrappers
GdbAddressClass GdbSymbol::GetSymbolClass()
{
  return (GdbAddressClass) (ingdb()->aclass) ;
}

GdbType *GdbSymbol::GetType() 
{
return (GdbType*)(ingdb()->type);
}
char *GdbSymbol::GetName()
{
return ingdb()->ginfo.name;
}

GdbSymbol *GdbSymbol::Lookup(const char *name)
{
#if 0
return (GdbSymbol*)lookup_symbol (p, gename);
#else
return (GdbSymbol*)lookup_symbol (name,get_current_block(), VAR_NAMESPACE, NULL, NULL);
#endif
}
//////////////////////////////////////////////////////////////////////
// frame_info wrappers 

GdbBlock *GdbFrameInfo::GetFrameBlock()
{
  return (GdbBlock *)get_frame_block(ingdb());
}

GdbFrameInfo *GdbFrameInfo::GetCurrentFrame()
{
return (GdbFrameInfo *)(get_current_frame());
}

GdbFrameInfo *GdbFrameInfo::GetPrevFrame()
{
return (GdbFrameInfo *)(get_prev_frame(ingdb()));
}


//////////////////////////////////////////////////////////////////////
// block wrappers

int GdbBlock::GetNSyms()
{
return BLOCK_NSYMS(ingdb());
}

GdbSymbol *GdbBlock::GetBlockSym(int i)
{
  return (GdbSymbol *)(BLOCK_SYM(ingdb(),i));
}
GdbBlock *GdbBlock::GetSuperBlock()
{
  return (GdbBlock *)BLOCK_SUPERBLOCK(ingdb());
}

GdbSymbol *GdbBlock::GetFunction()
{
return (GdbSymbol *)(BLOCK_FUNCTION(ingdb()));
}
//////////////////////////////////////////////////////////////////////
// gdbvalue


GdbValue *GdbValue::ReadVarValue(GdbSymbol *symbol,
	GdbFrameInfo *frame)
{
return (GdbValue *)(read_var_value (symbol->ingdb(), frame->ingdb()));
}

GdbLValType GdbValue::GetLVal()
{
return (GdbLValType)(VALUE_LVAL(ingdb()));
}

int GdbValue::GetAddress()
{
return (int)VALUE_ADDRESS(ingdb());
}
int GdbValue::GetRegno()
{
return VALUE_REGNO(ingdb());
}
int GdbValue::GetFrame()
{
return (int)VALUE_FRAME(ingdb());
}

LONGEST GdbValue::GetInt()
{
return unpack_long (VALUE_TYPE(ingdb()), VALUE_CONTENTS(ingdb()));
}

char *GdbValue::GetEnumName()
{
  struct type *typ =VALUE_TYPE(ingdb());
  int len = TYPE_NFIELDS ( typ);
  int val = unpack_long (typ, VALUE_CONTENTS(ingdb()));
  int i;
  for (i = 0; i < len; i++)
    {
      if (val == TYPE_FIELD_BITPOS (typ, i))
	{
	  return TYPE_FIELD_NAME (typ, i);
	}
    }
  return "*";
}
//////////////////////////////////////////////////////////////////////

GdbTypeCode GdbType::GetTypeCode()
{
return (GdbTypeCode)(ingdb()->code);
}
#endif
