#if 0
typedef enum 
{
  GDB_LOC_UNDEF,
  GDB_LOC_CONST,
  GDB_LOC_STATIC,
  GDB_LOC_REGISTER,
  GDB_LOC_ARG,
  GDB_LOC_REF_ARG,
  GDB_LOC_REGPARM,
  GDB_LOC_REGPARM_ADDR,
  GDB_LOC_LOCAL,
  GDB_LOC_TYPEDEF,
  GDB_LOC_LABEL,
  GDB_LOC_BLOCK,
  GDB_LOC_CONST_BYTES,
  GDB_LOC_LOCAL_ARG,
  GDB_LOC_BASEREG,
  GDB_LOC_BASEREG_ARG,
  GDB_LOC_OPTIMIZED_OUT
  } GdbAddressClass ;

typedef enum
{
  GDB_TYPE_CODE_UNDEF,		/* Not used; catches errors */
  GDB_TYPE_CODE_PTR,		/* Pointer type */
  GDB_TYPE_CODE_ARRAY,		/* Array type with lower & upper bounds. */
  GDB_TYPE_CODE_STRUCT,		/* C struct or Pascal record */
  GDB_TYPE_CODE_UNION,		/* C union or Pascal variant part */
  GDB_TYPE_CODE_ENUM,		/* Enumeration type */
  GDB_TYPE_CODE_FUNC,		/* Function type */
  GDB_TYPE_CODE_INT,		/* Integer type */

  /* Floating type.  This is *NOT* a complex type.  Beware, there are parts
     of GDB which bogusly assume that TYPE_CODE_FLT can mean complex.  */
  GDB_TYPE_CODE_FLT,

  /* Void type.  The length field specifies the length (probably always
     one) which is used in pointer arithmetic involving pointers to
     this type, but actually dereferencing such a pointer is invalid;
     a void type has no length and no actual representation in memory
     or registers.  A pointer to a void type is a generic pointer.  */
  GDB_TYPE_CODE_VOID,

  GDB_TYPE_CODE_SET,		/* Pascal sets */
  GDB_TYPE_CODE_RANGE,		/* Range (integers within spec'd bounds) */

  /* A string type which is like an array of character but prints
     differently (at least for CHILL).  It does not contain a length
     field as Pascal strings (for many Pascals, anyway) do; if we want
     to deal with such strings, we should use a new type code.  */
  GDB_TYPE_CODE_STRING,

  /* String of bits; like TYPE_CODE_SET but prints differently (at least
     for CHILL).  */
  GDB_TYPE_CODE_BITSTRING,

  /* Unknown type.  The length field is valid if we were able to
     deduce that much about the type, or 0 if we don't even know that.  */
  GDB_TYPE_CODE_ERROR,

  /* C++ */
  GDB_TYPE_CODE_MEMBER,		/* Member type */
  GDB_TYPE_CODE_METHOD,		/* Method type */
  GDB_TYPE_CODE_REF,		/* C++ Reference types */

  GDB_TYPE_CODE_CHAR,		/* *real* character type */

  /* Boolean type.  0 is false, 1 is true, and other values are non-boolean
     (e.g. FORTRAN "logical" used as unsigned int).  */
  GDB_TYPE_CODE_BOOL,

  /* Fortran */
  GDB_TYPE_CODE_COMPLEX,		/* Complex float */
  GDB_TYPE_CODE_LITERAL_COMPLEX,	/* */
  GDB_TYPE_CODE_LITERAL_STRING	/* */
}  GdbTypeCode;
class GdbSymbol 
{
  int dummy;
 public:
};

class GdbFrameInfo
{
  int dummy;
 public:
  class GdbBlock *GetFrameBlock();
  static GdbFrameInfo *GetCurrentFrame();
  GdbFrameInfo *GetPrevFrame();
  struct frame_info *ingdb() { return (struct frame_info *)(&dummy); }
  GdbFrameInfo *cast(struct frame_info *x) { return (GdbFrameInfo *)x;};
};

class GdbBlock
{
  int dummy;
 public:
  int GetNSyms();

  class GdbSymbol *GetFunction();
  class GdbBlock *GetSuperBlock();
  class GdbSymbol *GetBlockSym(int i);
  struct block *ingdb() { return (struct block *)(&dummy); }
  GdbBlock *cast(struct block *x) { return (GdbBlock *)x;};
};



 
typedef enum {
  /* Not an lval.  */
GDB_NOT_LVAL,
  /* In memory.  Could be a saved register.  */
GDB_LVAL_MEMORY,
  /* In a register.  */
GDB_LVAL_REGISTER,
  /* In a gdb internal variable.  */
GDB_LVAL_INTERNALVAR,
  /* Part of a gdb internal variable (structure field).  */
GDB_LVAL_INTERNALVAR_COMPONENT,
  /* IN A REGISTER SERIES IN A FRAME NOT THE CURRENT ONE, WHICH MAY HAVE been
     partially saved or saved in different places (otherwise would be
     lval_register or lval_memory).  */
GDB_LVAL_REG_FRAME_RELATIVE
} GdbLValType;

class GdbValue 
{
  int dummy;
 public:
  static  GdbValue *ReadVarValue(GdbSymbol *, GdbFrameInfo*);
  GdbLValType GetLVal();
  int GetAddress();
  int GetRegno();
  int GetFrame();
  int GetInt();
char *GetEnumName();
  struct value *ingdb() { return (struct value *)(&dummy); }
};

class GdbType 
{
int dummy;
 public:
  struct type *ingdb() { return (struct type *)(&dummy); }
GdbTypeCode GetTypeCode() ;
};
#endif
