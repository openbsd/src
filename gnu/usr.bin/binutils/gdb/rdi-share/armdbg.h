/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * ARM symbolic debugger toolbox interface
 */

/*
 * RCS $Revision: 1.3 $
 * Checkin $Date: 2004/12/27 14:00:54 $
 */

/* Minor points of uncertainty are indicated by a question mark in the
   LH margin.

   Wherever an interface is required to iterate over things of some class,
   I prefer something of the form  EnumerateXXs(..., XXProc *p, void *arg)
   which results in a call of p(xx, arg) for each xx, rather than something
   of the form
     for (xxh = StartIterationOverXXs(); (xx = Next(xxh)) != 0; ) { ... }
     EndIterationOverXXs(xxh);
   Others may disagree.
   (Each XXProc returns an Error value: if this is not Err_OK, iteration
   stops immediately and the EnumerateXXs function returns that value).

   ptrace has been retired as of insufficient utility.  If such fuctionality is
   required, it can be constructed using breakpoints.

   The file form of all name fields in debug areas is in-line, with a length
   byte and no terminator.  The debugger toolbox makes an in-store translation,
   where the strings are out of line (the namep variant in asdfmt.h) and have a
   terminating zero byte: the pointer is to the first character of the string
   with the length byte at ...->n.namep[-1].
 */

#ifndef armdbg__h
#define armdbg__h

#include <stddef.h>

#include "host.h"
#include "msg.h"

typedef unsigned32 ARMaddress;
typedef unsigned32 ARMword;
typedef unsigned16 ARMhword;

#include "dbg_conf.h"
#include "dbg_rdi.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef unsigned char Dbg_Byte;

typedef int Dbg_Error;

typedef struct Dbg_MCState Dbg_MCState;
/* A representation of the state of the target.  The structure is not revealed.
   A pointer to one of these is returned by Dbg_Initialise(), is passed to all
   toolbox calls which refer to the state of the target, and is discarded
   by Dbg_Finalise().
   Nothing in the toolbox itself precludes there being multiple targets, each
   with its own state.
 */

/* Most toolbox interfaces return an error status.  Only a few of the status
   values are expected to be interesting to clients and defined here; the
   rest are private (but a textual form can be produced by ErrorToChars()).
 */

#define Error_OK 0

/* Partitioning of the error code space: errors below Dbg_Error_Base are RDI
   errors (as defined in dbg_rdi.h). Codes above Dbg_Error_Limit are
   available to clients, who may impose some further structure.
 */
#define Dbg_Error_Base 0x1000
#define Dbg_Error_Limit 0x2000

#define DbgError(n) ((Dbg_Error)(Dbg_Error_Base+(n)))

#define Dbg_Err_OK Error_OK
#define Dbg_Err_Interrupted             DbgError(1)
#define Dbg_Err_Overflow                DbgError(2)
#define Dbg_Err_FileNotFound            DbgError(3)
#define Dbg_Err_ActivationNotPresent    DbgError(4)
#define Dbg_Err_OutOfHeap               DbgError(5)
#define Dbg_Err_TypeNotSimple           DbgError(6)
#define Dbg_Err_BufferFull              DbgError(7)
#define Dbg_Err_AtStackBase             DbgError(8)
#define Dbg_Err_AtStackTop              DbgError(9)
#define Dbg_Err_DbgTableFormat          DbgError(10)
#define Dbg_Err_NotVariable             DbgError(11)
#define Dbg_Err_NoSuchBreakPoint        DbgError(12)
#define Dbg_Err_NoSuchWatchPoint        DbgError(13)
#define Dbg_Err_FileLineNotFound        DbgError(14)
#define Dbg_Err_DbgTableVersion         DbgError(15)
#define Dbg_Err_NoSuchPath              DbgError(16)
#define Dbg_Err_StateChanged            DbgError(17)
#define Dbg_Err_SoftInitialiseError     DbgError(18)
#define Dbg_Err_CoProRegNotWritable     DbgError(19)
#define Dbg_Err_NotInHistory            DbgError(20)
#define Dbg_Err_ContextSyntax           DbgError(21)
#define Dbg_Err_ContextNoLine           DbgError(22)
#define Dbg_Err_ContextTwoLines         DbgError(23)
#define Dbg_Err_VarReadOnly             DbgError(24)
#define Dbg_Err_FileNewerThanImage      DbgError(25)
#define Dbg_Err_NotFound                DbgError(26)

   /* functions which evaluate expressions may return this value, to indicate
      that execution became suspended within a function called in the debugee */

/* Functions returning characters take a BufDesc argument, with fields buffer
   and bufsize being input arguments describing the buffer to be filled, and
   filled being set on return to the number of bytes written to the buffer
   (omitting the terminating 0).
 */

typedef struct Dbg_BufDesc Dbg_BufDesc;

typedef void Dbg_BufferFullProc(Dbg_BufDesc *bd);

struct Dbg_BufDesc {
    char *buffer;
    size_t size,
           filled;
    Dbg_BufferFullProc *p;
    void *arg;
};

#define Dbg_InitBufDesc(bd, buf, bytes) \
    ((bd).buffer = (buf), (bd).size = (bytes), (bd).filled = 0,\
     (bd).p = NULL, (bd).arg = NULL)

#define Dbg_InitBufDesc_P(bd, buf, bytes, fn, a) \
    ((bd).buffer = (buf), (bd).size = (bytes), (bd).filled = 0,\
     (bd).p = (fn), (bd).arg = (a))

Dbg_Error Dbg_StringToBuf(Dbg_BufDesc *buf, char const *s);
Dbg_Error Dbg_BufPrintf(Dbg_BufDesc *buf, char const *form, ...);
#ifdef NLS
Dbg_Error Dbg_MsgToBuf(Dbg_BufDesc *buf, msg_t t);
Dbg_Error Dbg_BufMsgPrintf(Dbg_BufDesc *buf, msg_t form, ...);
#else
#define Dbg_MsgToBuf Dbg_StringToBuf
#define Dbg_BufMsgPrintf Dbg_BufPrintf
#endif
Dbg_Error Dbg_CharToBuf(Dbg_BufDesc *buf, int ch);

int Dbg_CIStrCmp(char const *s1, char const *s2);
/* Case-independent string comparison, interface as for strcmp */

int Dbg_CIStrnCmp(char const *s1, char const *s2, size_t n);
/* Case-independent string comparison, interface as for strncmp */

void Dbg_ErrorToChars(Dbg_MCState *state, Dbg_Error err, Dbg_BufDesc *buf);

typedef int Dbg_RDIResetCheckProc(int);
/* Type of a function to be called after each RDI operation performed by the
   toolbox, with the status from the operation as argument.  The value returned
   is treated as the status.  (The intent is to allow the toolbox's client to
   take special action to handle RDIDbg_Error_Reset).
 */

typedef struct Dbg_CoProDesc Dbg_CoProDesc;

typedef Dbg_Error Dbg_CoProFoundProc(Dbg_MCState *state, int cpno, Dbg_CoProDesc const *cpd);
/* Type of a function to be called when the shape of a coprocessor is discovered
   by enquiry of the target or agent (via RequestCoProDesc)
 */

typedef struct RDIProcVec RDIProcVec;

Dbg_Error Dbg_RequestReset(Dbg_MCState *);

Dbg_Error Dbg_Initialise(
    Dbg_ConfigBlock *config, Dbg_HostosInterface const *i,
    Dbg_RDIResetCheckProc *checkreset, Dbg_CoProFoundProc *coprofound,
    RDIProcVec const *rdi, Dbg_MCState **statep);
/* values in config are updated if they call for default values */

void Dbg_Finalise(Dbg_MCState *state, bool targetdead);

typedef struct {
    char name[16];
    RDI_MemDescr md;
    RDI_MemAccessStats a;
} Dbg_MemStats;

/*--------------------------------------------------------------------------*/

/* Symbol table management.
   The structure of a Dbg_SymTable is not revealed.  It is created by
   Dbg_ReadSymbols() or by Dbg_LoadFile(), and associated with the argument
   Dbg_MCState.
   Many symbol tables may be concurrently active.
   A Dbg_SymTable is removed either explicitly (by call to Dbg_DeleteSymbols)
   or implicitly when a symbol table for an overlapping address range is read.

   There is a pre-defined symbol table containing entries for ARM registers,
   co-processor registers and the like.
 */

typedef struct Dbg_SymTable Dbg_SymTable;

typedef struct Dbg_ImageFragmentDesc {
    ARMaddress base, limit;
} Dbg_ImageFragmentDesc;

typedef enum {
    Dbg_Lang_None,
    Dbg_Lang_C,
    Dbg_Lang_Pascal,
    Dbg_Lang_Fortran,
    Dbg_Lang_Asm,
    Dbg_Lang_Cpp
} Dbg_Lang;

typedef struct Dbg_ImageDesc {
    Dbg_Lang lang;
    int executable;
    ARMaddress robase, rolimit, rwbase, rwlimit;
    int nfrags;
    Dbg_ImageFragmentDesc *fragments;
    char *name;
} Dbg_ImageDesc;

Dbg_ImageDesc *Dbg_ImageAreas(Dbg_SymTable *st);

Dbg_SymTable *Dbg_LastImage(Dbg_MCState *state);

Dbg_SymTable *Dbg_NewSymTable(Dbg_MCState *state, const char *name);

Dbg_Error Dbg_ReadSymbols(Dbg_MCState *state, const char *filename, Dbg_SymTable **st);
/* Just read the symbols from the named image.  <st> is set to the allocated
   symbol table.
?  Maybe we could usefully allow other formats than AIF images to describe
   the symbols (eg) of shared libraries
 */

typedef struct Dbg_SymInfo {
    int isize;
    ARMaddress addr;
    char *name;
} Dbg_SymInfo;

Dbg_SymInfo *Dbg_AsmSym(Dbg_SymTable *st, ARMaddress addr);
int32 Dbg_AsmAddr(Dbg_SymTable *st, int32 line);
int32 Dbg_AsmLine(Dbg_SymTable *st, ARMaddress addr);
int32 Dbg_AsmLinesInRange(Dbg_SymTable *st, ARMaddress start, ARMaddress end);

Dbg_Error Dbg_LoadFile(Dbg_MCState *state, const char *filename, Dbg_SymTable **st);
/* load the image into target memory, and read its symbols.  <st> is set to
   the allocated symbol table.
   A null filename reloads the most recently loaded file (and rereads its
   symbols).
   Loading an image leaves breakpoints unchanged.  If a client wishes
   otherwise, it must remove the breakpoints explicitly.
*/

Dbg_Error Dbg_CallGLoadFile(Dbg_MCState *state, const char *filename, Dbg_SymTable **st);

typedef void Dbg_ImageLoadProc(Dbg_MCState *, Dbg_SymTable *);
Dbg_Error Dbg_OnImageLoad(Dbg_MCState *, Dbg_ImageLoadProc *);
/* Register function to be called back whenever an image is loaded, or symbols
 * for an image read. (To allow multiple toolbox clients to coexist).
 */

Dbg_Error Dbg_LoadAgent(Dbg_MCState *state, const char *filename);
/* Load a debug agent, and start it.
   Symbols in the image for the agent are ignored.
*/

Dbg_Error Dbg_RelocateSymbols(Dbg_SymTable *st, ARMaddress reloc);
/* add <reloc> to the value of all symbols in <st> describing absolute memory
   locations.  The intent is to allow the symbols in a load-time relocating
   image (for example) to be useful.
 */

Dbg_Error Dbg_DeleteSymbols(Dbg_MCState *state, Dbg_SymTable **st);

typedef enum Dbg_LLSymType {
    llst_code,
    llst_code16,
    llst_data,
    llst_const,
    llst_unknown,
    llst_max
} Dbg_LLSymType;
/* Since AIF images contain no type information for low-level symbols, this
   classification is only a guess, and some symbols describing constants will
   incorrectly be described as code or data.
 */

typedef Dbg_Error Dbg_SymProc(
    Dbg_MCState *state,
    const char *symbol, Dbg_LLSymType symtype, ARMaddress value,
    void *arg);

Dbg_Error Dbg_EnumerateLowLevelSymbols(
    Dbg_MCState *state, const char *match, Dbg_SymProc *p,
    void *arg);
/* Call  p(name, value)  for each low level symbol in the tables of <state>
   whose name matches the regular expression <match> (a NULL <match> matches
   any name).  Symbols are enumerated in no particular order.
 */

/*--------------------------------------------------------------------------*/

/* Functions are provided here to allow quick mass access to register values
   for display.  There is no comparable need for quick mass update, so writing
   should be via Assign().
 */

typedef struct Dbg_RegSet {
    ARMword
        user[15],
        pc,
        psr,
        fiq[7],
        spsr_fiq,
        irq[2],
        spsr_irq,
        svc[2],
        spsr_svc,
        abort[2],
        spsr_abort,
        undef[2],
        spsr_undef;
} Dbg_RegSet;

/* bits in the modemask argument for ReadRegisters */

#define Dbg_MM_User     1
#define Dbg_MM_FIQ      2
#define Dbg_MM_IRQ      4
#define Dbg_MM_SVC      8
#define Dbg_MM_Abort 0x10
#define Dbg_MM_Undef 0x20
#define Dbg_MM_System 0x40

Dbg_Error Dbg_ReadRegisters(Dbg_MCState *state, Dbg_RegSet *regs, int modemask);

Dbg_Error Dbg_WriteRegister(Dbg_MCState *state, int rno, int modemask, ARMword val);

int Dbg_StringToMode(const char *name);

int Dbg_ModeToModeMask(ARMword mode);

/* Some implementations of the FP instruction set keep FP values loaded into
   registers in their unconverted format, converting only when necessary.
   Some RDI implementations deliver these values uninterpreted.
   (For the rest, register values will always have type F_Extended).
 */

typedef enum { F_Single, F_Double, F_Extended, F_Packed, /* fpe340 values */
               F_Internal,                               /* new fpe : mostly as extended */
               F_None } Dbg_FPType;

typedef struct { ARMword w[3]; } Dbg_TargetExtendedVal;
typedef struct { ARMword w[3]; } Dbg_TargetPackedVal;
typedef struct { ARMword w[2]; } Dbg_TargetDoubleVal;
typedef struct { ARMword w[1]; } Dbg_TargetFloatVal;

typedef union { Dbg_TargetExtendedVal e; Dbg_TargetPackedVal p;
                Dbg_TargetDoubleVal d; Dbg_TargetFloatVal f; } Dbg_TargetFPVal;

#define TargetSizeof_Extended 12
#define TargetSizeof_Packed   12
#define TargetSizeof_Double   8
#define TargetSizeof_Float    4

typedef struct Dbg_FPRegVal {
     Dbg_FPType type;
     Dbg_TargetFPVal v;
} Dbg_FPRegVal;

typedef struct Dbg_FPRegSet {
    Dbg_FPRegVal f[8];
    ARMword fpsr, fpcr;
} Dbg_FPRegSet;

Dbg_Error Dbg_ReadFPRegisters(Dbg_MCState *state, Dbg_FPRegSet *regs);

Dbg_Error Dbg_WriteFPRegisters(Dbg_MCState *state, int32 mask, Dbg_FPRegSet *regs);

Dbg_Error Dbg_FPRegToDouble(DbleBin *d, Dbg_FPRegVal const *f);
/* Converts from a FP register value (in any format) to a double with
   approximately the same value (or returns Dbg_Err_Overflow)
 */

void Dbg_DoubleToFPReg(Dbg_FPRegVal *f, DbleBin const *d);
/* Converts the double <d> to a Dbg_FPRegVal with type F_Extended */

/*--------------------------------------------------------------------------*/

#include "dbg_cp.h"

Dbg_Error Dbg_DescribeCoPro(Dbg_MCState *state, int cpnum, Dbg_CoProDesc *p);

Dbg_Error Dbg_DescribeCoPro_RDI(Dbg_MCState *state, int cpnum, Dbg_CoProDesc *p);

Dbg_Error Dbg_ReadCPRegisters(Dbg_MCState *state, int cpnum, ARMword *regs);

Dbg_Error Dbg_WriteCPRegisters(Dbg_MCState *state, int cpnum, int32 mask, ARMword *regs);

/*--------------------------------------------------------------------------*/

Dbg_Error Dbg_ReadWords(
    Dbg_MCState *state,
    ARMword *words, ARMaddress addr, unsigned count);
/* Reads a number of (32-bit) words from target store.  The values are in host
   byte order; if they are also to be interpreted as bytes Dbg_SwapByteOrder()
   must be called to convert to target byte order.
 */

Dbg_Error Dbg_WriteWords(
    Dbg_MCState *state,
    ARMaddress addr, const ARMword *words, unsigned count);
/* Writes a number of (32-bit) words to target store.  The values are in host
   byte order (if what is being written is actually a byte string it must be
   converted by Dbg_SwapByteOrder()).
 */

Dbg_Error Dbg_ReadHalf(Dbg_MCState *state, ARMhword *val, ARMaddress addr);
Dbg_Error Dbg_WriteHalf(Dbg_MCState *state, ARMaddress addr, ARMword val);

Dbg_Error Dbg_ReadBytes(Dbg_MCState *state, Dbg_Byte *val, ARMaddress addr, unsigned count);
Dbg_Error Dbg_WriteBytes(Dbg_MCState *state, ARMaddress addr, const Dbg_Byte *val, unsigned count);

void Dbg_HostWords(Dbg_MCState *state, ARMword *words, unsigned wordcount);
/* (A noop unless host and target bytesexes differ) */

ARMword Dbg_HostWord(Dbg_MCState *state, ARMword v);

ARMhword Dbg_HostHalf(Dbg_MCState *state, ARMword v);

/*--------------------------------------------------------------------------*/

/* Types describing various aspects of position within code.
   There are rather a lot of these, in the interests of describing precisely
   what fields must be present (rather than having a single type with many
   fields which may or may not be valid according to context).
 */

typedef struct Dbg_LLPos {
    Dbg_SymTable *st;
    char *llsym;
    ARMaddress offset;
} Dbg_LLPos;

typedef struct Dbg_File {
    Dbg_SymTable *st;
    char *file;
} Dbg_File;

typedef struct Dbg_Line {
    unsigned32 line;      /* linenumber in the file */
    unsigned16 statement, /* within the line (1-based) */
               charpos;   /* ditto */
} Dbg_Line;
/* As an output value from toolbox functions, both statement and charpos are set
   if the version of the debugger tables for the section concerned permits.
   On input, <charpos> is used only if <statement> is 0 (in which case, if
   <charpos> is non-0, Dbg_Err_DbgTableVersion is returned if the version of
   the debugger tables concerned is too early.
 */

typedef struct Dbg_FilePos {
    Dbg_File f;
    Dbg_Line line;
} Dbg_FilePos;

typedef struct Dbg_ProcDesc {
    Dbg_File f;
    char *name;
} Dbg_ProcDesc;

typedef struct Dbg_ProcPos {
    Dbg_ProcDesc p;
    Dbg_Line line;
} Dbg_ProcPos;

/* Support for conversions between position representations */

Dbg_Error Dbg_ProcDescToLine(Dbg_MCState *state, Dbg_ProcDesc *proc, Dbg_Line *line);
/* If proc->f.file is null (and there is just one function proc->name), it is
   updated to point to the name of the file containing (the start of)
   proc->name.
 */

Dbg_Error Dbg_FilePosToProc(Dbg_MCState *state, const Dbg_FilePos *pos, char **procname);

/* Conversions from position representations to and from code addresses */

Dbg_Error Dbg_AddressToProcPos(
    Dbg_MCState *state, ARMaddress addr,
    Dbg_ProcPos *pos);
Dbg_Error Dbg_AddressToLLPos(
    Dbg_MCState *state, ARMaddress addr,
    Dbg_LLPos *pos, Dbg_LLSymType *res_type, int system_names);

Dbg_Error Dbg_ProcPosToAddress(
    Dbg_MCState *state, const Dbg_ProcPos *pos,
    ARMaddress *res);
Dbg_Error Dbg_LLPosToAddress(
    Dbg_MCState *state, const Dbg_LLPos *pos,
    ARMaddress *res);

typedef struct {
    ARMaddress start, end;
} Dbg_AddressRange;

typedef Dbg_Error Dbg_AddressRangeProc(void *arg, int32 first, int32 last, Dbg_AddressRange const *range);

Dbg_Error Dbg_MapAddressRangesForFileRange(
    Dbg_MCState *state,
    Dbg_SymTable *st, const char *f, int32 first, int32 last, Dbg_AddressRangeProc *p, void *arg);

typedef struct Dbg_Environment Dbg_Environment;
/* A Dbg_Environment describes the context required to make sense of a variable
   name and access its value.  Its format is not revealed.  Dbg_Environment
   values are allocated by Dbg_NewEnvironment() and discarded by
   Dbg_DeleteEnvironment().
 */

Dbg_Environment *Dbg_NewEnvironment(Dbg_MCState *state);
void Dbg_DeleteEnvironment(Dbg_MCState *state, Dbg_Environment *env);

Dbg_Error Dbg_StringToEnv(
    Dbg_MCState *state, char *str, Dbg_Environment *resenv,
    int forcontext, Dbg_Environment const *curenv);

Dbg_Error Dbg_ProcPosToEnvironment(
    Dbg_MCState *state, const Dbg_ProcPos *pos, int activation,
    const Dbg_Environment *current, Dbg_Environment *res);

/* Conversion from a position representation to an Dbg_Environment (as required
   to access variable values).  Only a Dbg_ProcPos argument here; other
   representations need to be converted first.

   Returns <res> describing the <activation>th instance of the function
   described by <pos>, up from the stack base if <activation> is negative,
   else down from <current>.
   If this function returns Dbg_Err_ActivationNotPresent, the result
   Dbg_Environment is still valid for accessing non-auto variables.
 */

typedef struct Dbg_DeclSpec Dbg_DeclSpec;

Dbg_Error Dbg_EnvToProcItem(
    Dbg_MCState *state, Dbg_Environment const *env, Dbg_DeclSpec *proc);

Dbg_Error Dbg_ContainingEnvironment(
    Dbg_MCState *state, const Dbg_Environment *context, Dbg_Environment *res);
/* Set <res> to describe the containing function, file if <context> is within
   a top-level function (or error if <context> already describes a file).
 */

/*--------------------------------------------------------------------------*/

/* ASD debug table pointers are not by themselves sufficient description,
   since there's an implied section context.  Hence the DeclSpec and TypeSpec
   structures.
 */


#ifndef Dbg_TypeSpec_Defined

struct Dbg_DeclSpec { void *a; };

#ifdef CALLABLE_COMPILER
typedef void *Dbg_TypeSpec;

/* The intention here is to give an alternative definition for Dbg_BasicType
   which follows.
 */

#define Dbg_T_Void      xDbg_T_Void
#define Dbg_T_Bool      xDbg_T_Bool
#define Dbg_T_SByte     xDbg_T_SByte
#define Dbg_T_SHalf     xDbg_T_Half
#define Dbg_T_SWord     xDbg_T_SWord
#define Dbg_T_UByte     xDbg_T_UByte
#define Dbg_T_UHalf     xDbg_T_UHalf
#define Dbg_T_UWord     xDbg_T_UWord
#define Dbg_T_Float     xDbg_T_Float
#define Dbg_T_Double    xDbg_T_Double
#define Dbg_T_LDouble   xDbg_T_LDouble
#define Dbg_T_Complex   xDbg_T_Complex
#define Dbg_T_DComplex  xDbg_T_DComplex
#define Dbg_T_String    xDbg_T_String
#define Dbg_T_Function  xDbg_T_Function

#define Dbg_BasicType   xDbg_BaiscType
#define Dbg_PrimitiveTypeToTypeSpec xDbg_PrimitiveTypeToTypeSpec

#else
/* We want a Dbg_TypeSpec to be a an opaque type, but of known size (so the
   toolbox's clients can allocate the store to hold one); unfortunately, we
   can do this only by having one definition for the toolbox's clients and
   one (elsewhere) for the toolbox itself.
 */

typedef struct Dbg_TypeSpec Dbg_TypeSpec;
struct Dbg_TypeSpec { void *a; int32 b; };
#endif /* CALLABLE_COMPILER */

typedef enum {
    Dbg_T_Void,

    Dbg_T_Bool,

    Dbg_T_SByte,
    Dbg_T_SHalf,
    Dbg_T_SWord,

    Dbg_T_UByte,
    Dbg_T_UHalf,
    Dbg_T_UWord,

    Dbg_T_Float,
    Dbg_T_Double,
    Dbg_T_LDouble,

    Dbg_T_Complex,
    Dbg_T_DComplex,

    Dbg_T_String,

    Dbg_T_Function
} Dbg_BasicType;

#endif

void Dbg_PrimitiveTypeToTypeSpec(Dbg_TypeSpec *ts, Dbg_BasicType t);

bool Dbg_TypeIsIntegral(Dbg_TypeSpec const *ts);

bool Dbg_TypeIsPointer(Dbg_TypeSpec const *ts);

bool Dbg_TypeIsFunction(Dbg_TypeSpec const *ts);

bool Dbg_PruneType(Dbg_TypeSpec *tsres, Dbg_TypeSpec const *ts);
/* Return to tsres a version of ts which has been pruned by the removal of all
   toplevel typedefs. Result is YES if the result has changed.
 */

typedef Dbg_Error Dbg_FileProc(Dbg_MCState *state, const char *name, const Dbg_DeclSpec *procdef, void *arg);

Dbg_Error Dbg_EnumerateFiles(Dbg_MCState *state, Dbg_SymTable *st, Dbg_FileProc *p, void *arg);
/* The top level for a high level enumerate.  Lower levels are performed by
   EnumerateDeclarations (below).
 */

typedef enum {
    ds_Invalid,
    ds_Type,
    ds_Var,
    ds_Proc,
    ds_Enum,
    ds_Function,
    ds_Label
} Dbg_DeclSort;

Dbg_DeclSort Dbg_SortOfDeclSpec(Dbg_DeclSpec const *decl);

char *Dbg_NameOfDeclSpec(Dbg_DeclSpec const *decl);

Dbg_TypeSpec Dbg_TypeSpecOfDeclSpec(Dbg_DeclSpec const *decl);

typedef enum {
    cs_None,
    cs_Extern,
    cs_Static,
    cs_Auto,
    cs_Reg,
    cs_Var,
    cs_Farg,
    cs_Fcarg,
    cs_Local,
    cs_Filtered,
    cs_Globalreg
} Dbg_StgClass;

Dbg_StgClass Dbg_StgClassOfDeclSpec(Dbg_DeclSpec const *decl);

bool Dbg_VarsAtSameAddress(Dbg_DeclSpec const *d1, Dbg_DeclSpec const *d2);

bool Dbg_VarsDecribedForDeclSpec(Dbg_DeclSpec const *decl);

int Dbg_ArgCountOfDeclSpec(Dbg_DeclSpec const *decl);

typedef struct Dbg_DComplex { DbleBin r, i; } Dbg_DComplex;

typedef union Dbg_ConstantVal {
    int32 l;
    unsigned32 u;
    DbleBin d;
    Dbg_DComplex fc;
    ARMaddress a;
    char *s;
} Dbg_ConstantVal;

typedef struct Dbg_Constant {
    Dbg_TypeSpec type;
    Dbg_ConstantVal val;
} Dbg_Constant;

typedef enum Dbg_ValueSort {
    vs_register,
    vs_store,
    vs_constant,
    vs_local,
    vs_filtered,
    vs_none,
    vs_error
} Dbg_ValueSort;

/* vs_local allows the use of symbol table entries to describe entities within
   the debugger's own address space, accessed in the same way as target
   variables.
   vs_filtered describes entities which may be read or written only via an
   access function (eg r15)
 */

#define fpr_base 16
/* There's only one register ValueSort (reflecting asd table StgClass);
   fp register n is encoded as register n+fpr_base.
 */

typedef struct Dbg_Value Dbg_Value;

typedef Dbg_Error Dbg_AccessFn(Dbg_MCState *state, int write, Dbg_Value *self, Dbg_Constant *c);
/* <write> == 0: read a vs_filtered value, updating the value self.
   <write> == 1: update a vs_filtered value, with the value described by c.
   <self> allows use of the same Dbg_AccessFn for several different entities
   (using different val.f.id fields).
 */

typedef Dbg_Error Dbg_FormatFn(int decode, char *b, ARMword *valp, void *formatarg);

typedef struct { Dbg_AccessFn *f; int id; } Dbg_AccessFnRec;

struct Dbg_Value {
    Dbg_TypeSpec type;
    Dbg_ValueSort sort;
    Dbg_FormatFn *formatp;
    void *formatarg;
    int f77csize;
    union {
        struct { int no; ARMaddress frame; } r;
        ARMaddress ptr;
        Dbg_ConstantVal c;
        void *localp;
        Dbg_AccessFnRec f;
        Dbg_Error err;
    } val;
};

Dbg_Error Dbg_AddLLSymbol(Dbg_SymTable *st, char const *name, Dbg_LLSymType type, ARMword val);

Dbg_Error Dbg_AddSymbol(Dbg_SymTable *st, char const *name, Dbg_Value const *val);

typedef struct Dbg_DeclSpecF {
  Dbg_DeclSpec decl;
  Dbg_FormatFn *formatp;
  void *formatarg;
} Dbg_DeclSpecF;

typedef Dbg_Error Dbg_DeclProc(Dbg_MCState *state, Dbg_Environment const *context,
                       Dbg_DeclSpecF const *var, Dbg_DeclSort sort, int masked,
                       void *arg);

Dbg_Error Dbg_EnumerateDeclarations(Dbg_MCState *state, Dbg_Environment const *context,
                            Dbg_DeclProc *p, void *arg);
/* call p once for every declaration local to the function described by
   <context> (or file if <context> describes a place outside a function).
   p's argument <masked> is true if the variable is not visible, thanks to
   a declaration in an inner scope.
 */

Dbg_Error Dbg_ValueOfVar(Dbg_MCState *state, const Dbg_Environment *context,
                 const Dbg_DeclSpec *var, Dbg_Value *val);
/* Different from Dbg_EvalExpr() in that the thing being evaluated is described
   by a Dbg_DeclSpec (which must be for a variable), rather than a string
   needing to be decoded and associated with a symbol-table item.  Intended to
   be called from a Dbg_DeclProc called from Dbg_EnumerateDeclarations.
 */

Dbg_Error Dbg_EvalExpr(Dbg_MCState *state, Dbg_Environment const *context,
               char const *expr, int flags, Dbg_Value *val);

Dbg_Error Dbg_EvalExpr_ep(Dbg_MCState *state, Dbg_Environment const *context,
                  char const *expr, char **exprend, int flags, Dbg_Value *val);

/* Both Dbg_ValueOfVar and Dbg_EvalExpr mostly deliver a value still containing
   an indirection (since it may be wanted as the lhs of an assignment)
 */

void Dbg_RealLocation(Dbg_MCState *state, Dbg_Value *val);
/* If val describes a register, this may really be a register, or a place on
   the stack where the register's value is saved. In the latter case, val
   is altered to describe the save place. (In all others, it remains
   unchanged).
 */

Dbg_Error Dbg_DereferenceValue(Dbg_MCState *state, const Dbg_Value *value, Dbg_Constant *c);
/* This fails if <value> describes a structure or array, returning
   Dbg_Err_TypeNotSimple
 */

typedef struct Dbg_Expr Dbg_Expr;
/* The result of parsing an expression in an environment: its structure is not
   revealed.  (Clients may wish to parse just once an expression which may be
   evaluated often).  In support of which, the following two functions partition
   the work of Dbg_EvalExpr().
 */

#define Dbg_exfl_heap 1    /* allocate Expr on the heap (FreeExpr must then be
                              called to discard it).  Otherwise, it goes in a
                              place overwritten by the next call to ParseExpr
                              or EvalExpr
                            */
#define Dbg_exfl_needassign 2
#define Dbg_exfl_lowlevel   4

int Dbg_SetInputRadix(Dbg_MCState *state, int radix);
char *Dbg_SetDefaultIntFormat(Dbg_MCState *state, char *format);

Dbg_Error Dbg_ParseExpr(
    Dbg_MCState *state, Dbg_Environment const *env, char *string,
    char **end, Dbg_Expr **res, int flags);
/* Just parse the argument string, returning a pointer to a parsed expression
   and a pointer to the first non-white space character in the input string
   which is not part of the parsed expression. (If macro expansion has taken
   place, the returned pointer will not be into the argument string at all,
   rather into the expanded version of it).
 */

Dbg_Error Dbg_ParseExprCheckEnd(
    Dbg_MCState *state, Dbg_Environment const *env, char *string,
    Dbg_Expr **res, int flags);
/* As Dbg_ParseExpr, but the parsed expression is required completely to fill
   the argument string (apart possibly for trailing whitespace), and an error
   is returned if it does not.
 */

Dbg_Error Dbg_ParsedExprToValue(
    Dbg_MCState *state, const Dbg_Environment *env, Dbg_Expr *expr, Dbg_Value *v);

Dbg_Error Dbg_ReadDecl(
    Dbg_MCState *state, Dbg_Environment const *env, char *string,
    Dbg_TypeSpec *p, char **varp, int flags);
/* Read a variable declaration, returing a description of the type of the
   variable to p, and a pointer to its name to varp.
 */

bool Dbg_IsCastToArrayType(Dbg_MCState *state, Dbg_Expr *expr);

void Dbg_FreeExpr(Dbg_Expr *expr);

Dbg_Error Dbg_CopyType(Dbg_TypeSpec *tdest, Dbg_TypeSpec const *tsource);
Dbg_Error Dbg_FreeCopiedType(Dbg_TypeSpec *ts);

Dbg_Error Dbg_TypeOfExpr(Dbg_MCState *state, Dbg_Expr *tree, Dbg_TypeSpec *restype);

Dbg_Error Dbg_ExprToVar(const Dbg_Expr *expr, Dbg_DeclSpec *var, Dbg_Environment *env);

Dbg_Error Dbg_Assign(Dbg_MCState *state, const Dbg_Value *lv, const Dbg_Value *rv);

typedef enum Dbg_TypeSort {
  ts_simple,
  ts_union,
  ts_struct,
  ts_array
} Dbg_TypeSort;

Dbg_TypeSort Dbg_TypeSortOfValue(Dbg_MCState *state, const Dbg_Value *val, int *fieldcount);

Dbg_Error Dbg_TypeToChars(const Dbg_TypeSpec *var, Dbg_BufDesc *buf);

Dbg_Error Dbg_TypeSize(Dbg_MCState *state, const Dbg_TypeSpec *type, unsigned32 *res);

typedef int Dbg_ValToChars_cb(Dbg_MCState *state, Dbg_Value *subval, const char *fieldname,
                          Dbg_BufDesc *buf, void *arg);

Dbg_Error Dbg_ValToChars(Dbg_MCState *state, Dbg_Value *val, int base,
                 Dbg_ValToChars_cb *cb, void *arg,
                 const char *form, Dbg_BufDesc *buf);
/*
   <base> is used for (any size) integer values.
   If <val> is of an array or structure type, <cb> is called for each element,
   with <arg> as its last parameter, and <subbuf> describing the space remaining
   in <buf>.  If <cb> returns 0, conversion ceases.
 */

Dbg_Error Dbg_NthElement(
    Dbg_MCState *state,
    const Dbg_Value *val, unsigned32 n, char **fieldname, Dbg_Value *subval);

typedef Dbg_Error Dbg_HistoryProc(void *, int, Dbg_Value *);

Dbg_Error Dbg_RegisterHistoryProc(Dbg_MCState *state, Dbg_HistoryProc *p, void *arg);

typedef enum {
  ls_cpu,
  ls_store,
  ls_copro,
  ls_local,
  ls_filtered
} Dbg_LocSort;

typedef struct {
  Dbg_LocSort sort;
  union {
    struct { ARMaddress addr, size; } store;
    struct { int modemask; int r; } cpu;
    struct { int no; int r; } cp;
    void *localp;
    Dbg_AccessFnRec f;
  } loc;
} Dbg_Loc;

typedef Dbg_Error Dbg_ObjectWriteProc(Dbg_MCState *state, Dbg_Loc const *loc);
Dbg_Error Dbg_OnObjectWrite(Dbg_MCState *state, Dbg_ObjectWriteProc *p);
/* Register function to be called back whenever the toolbox has written to any
 * object accessible by the debuggee (or to local variables belonging to a
 * toolbox client). The write has already been done.
 * (To allow multiple toolbox clients to coexist).
 */

Dbg_Error Dbg_ObjectWritten(Dbg_MCState *state, Dbg_Loc const *loc);

/*--------------------------------------------------------------------------*/

/* Control of target program execution.
   Currently, only synchronous operation is provided.
   Execution could possibly be asynchronous where the target is a seperate
   processor, but is necessarily synchronous if the target is Armulator.
   Unfortunately, this may require modification to the RDI implementation
   if multitasking is required but the the host system provides it only
   cooperatively, or if there is no system-provided way to generate SIGINT.
 */

Dbg_Error Dbg_SetCommandline(Dbg_MCState *state, const char *args);
/* Set the argument string to the concatenation of the name of the most
   recently loaded image and args.
 */

typedef enum Dbg_ProgramState {
    ps_notstarted,
    /* Normal ways of stopping */
    ps_atbreak, ps_atwatch, ps_stepdone,
    ps_interrupted,
    ps_stopped,
    /* abnormal (but unsurprising) ways of stopping */
    ps_lostwatch,
    ps_branchthrough0, ps_undef, ps_caughtswi, ps_prefetch,
    ps_abort, ps_addrexcept, ps_caughtirq, ps_caughtfiq,
    ps_error,
    /* only as a return value from Call() */
    ps_callfailed, ps_callreturned,
    /* internal inconsistencies */
    ps_broken,                  /* target has "broken" */
    ps_unknownbreak,
    ps_unknown
} Dbg_ProgramState;

int Dbg_IsCallLink(Dbg_MCState *state, ARMaddress pc);

typedef struct {
    Dbg_FPRegVal fpres;
    ARMword intres;
} Dbg_CallResults;

Dbg_CallResults *Dbg_GetCallResults(Dbg_MCState *state);

#define Dbg_S_STATEMENTS 0
#define Dbg_S_INSTRUCTIONS 1
#define Dbg_S_STEPINTOPROCS 2

Dbg_Error Dbg_Step(Dbg_MCState *state, int32 stepcount, int stepby, Dbg_ProgramState *status);
/*  <stepby> is a combination of the Dbg_S_... values above */

Dbg_Error Dbg_StepOut(Dbg_MCState *state, Dbg_ProgramState *status);

bool Dbg_CanGo(Dbg_MCState *state);

bool Dbg_IsExecuting(Dbg_MCState *state);

Dbg_Error Dbg_Go(Dbg_MCState *state, Dbg_ProgramState *status);

Dbg_Error Dbg_Stop(Dbg_MCState *state);
/* Asynchronous Stop request, for call from SIGINT handler.  On return to the
   caller, the call of Dbg_Go, Dbg_Step or Dbg_Call which started execution
   should return ps_interrupted.
 */

typedef void Dbg_ExecuteProc(Dbg_MCState *state, Dbg_ProgramState status);
Dbg_Error Dbg_OnExecute(Dbg_MCState *, Dbg_ExecuteProc *);
/* Register function to be called back whenever execution stops.
 * (To allow multiple toolbox clients to coexist).
 */

Dbg_Error Dbg_SetReturn(Dbg_MCState *state,
                const Dbg_Environment *context, const Dbg_Value *value);
/* Prepare continuation by returning <value> from the function activation
   described by <context>.  (Dbg_Go() or Dbg_Step() actually perform the
   continuation).
 */

Dbg_Error Dbg_SetExecution(Dbg_MCState *state, Dbg_Environment *context);
/* Set the pc in a high-level fashion */

Dbg_Error Dbg_ProgramStateToChars(Dbg_MCState *state, Dbg_ProgramState event, Dbg_BufDesc *buf);
/* This is guaranteed to give a completely accurate description of <event> if
   this was the value returned by the most recent call of Dbg_Go, Dbg_Step,
   or Dbg_Call.
 */

/*--------------------------------------------------------------------------*/

Dbg_Error Dbg_CurrentEnvironment(Dbg_MCState *state, Dbg_Environment *context);

Dbg_Error Dbg_PrevFrame(Dbg_MCState *state, Dbg_Environment *context);
/* towards the base of the stack */

Dbg_Error Dbg_NextFrame(Dbg_MCState *state, Dbg_Environment *context);
/* away from the base of the stack */

typedef struct Dbg_AnyPos {
    enum { pos_source, pos_ll, pos_none } postype;
    ARMaddress pc;
    union {
        Dbg_ProcPos source;
        Dbg_LLPos ll;
        ARMaddress none;
    } pos;
} Dbg_AnyPos;

Dbg_Error Dbg_EnvironmentToPos(Dbg_MCState *state, const Dbg_Environment *context, Dbg_AnyPos *pos);
/* <pos> is set to a Dbg_ProcPos if these is one corresponding to <context>
   else a Dbg_LLPos if there is one.
 */

/*--------------------------------------------------------------------------*/

/* Source file management.
   Pretty vestigial.  Handles source path (per loaded image),
   and translates from line-number (as given in debugger tables) to character
   position (as required to access files)
 */

Dbg_Error Dbg_ClearPaths(Dbg_MCState *state, Dbg_SymTable *st);
Dbg_Error Dbg_AddPath(Dbg_MCState *state, Dbg_SymTable *st, const char *path);
Dbg_Error Dbg_DeletePath(Dbg_MCState *state, Dbg_SymTable *st, const char *path);

typedef enum {
  Dbg_PathsCleared,
  Dbg_PathAdded,
  Dbg_PathDeleted
} Dbg_PathAlteration;

typedef void Dbg_PathAlteredProc(
    Dbg_MCState *state, Dbg_SymTable *st, char const *path,
    Dbg_PathAlteration sort);

Dbg_Error Dbg_OnPathAlteration(Dbg_MCState *state, Dbg_PathAlteredProc *p);
/* Register function to be called back whenever one of the source path
 * modification functions above is called. (To allow multiple toolbox
 * clients to coexist).
 */

typedef struct Dbg_FileRec Dbg_FileRec;
typedef struct {
  unsigned32 linecount;
  Dbg_FileRec *handle;
  char *fullname;
} Dbg_FileDetails;

Dbg_Error Dbg_GetFileDetails(
    Dbg_MCState *state, const Dbg_File *fname, Dbg_FileDetails *res);
Dbg_Error Dbg_FinishedWithFile(Dbg_MCState *state, Dbg_FileRec *handle);

Dbg_Error Dbg_GetFileDetails_fr(
    Dbg_MCState *state, Dbg_FileRec *handle, Dbg_FileDetails *res);
/* Refresh details about the file associated with <handle> (in particular,
 * its linecount).
 */

Dbg_Error Dbg_FileLineLength(
    Dbg_MCState *state, Dbg_FileRec *handle, int32 lineno, int32 *len);
/* Return to <len> the length of line <lineno> of the file associated with
 * <handle> (without necessarily reading from the file).
 */

Dbg_Error Dbg_GetFileLine_fr(
    Dbg_MCState *state, Dbg_FileRec *handle, int32 lineno, Dbg_BufDesc *buf);
/* Return to <buf> the contents of line <lineno> of the file associated with
 * <handle> (including its terminating newline).
 */

Dbg_Error Dbg_StartFileAccess(Dbg_MCState *state, Dbg_FileRec *handle);
Dbg_Error Dbg_EndFileAccess(Dbg_MCState *state, Dbg_FileRec *handle);
/* These two calls bracket a sequence of calls to GetFileLine. Between the
 * calls, the toolbox is permitted to retain state allowing more rapid
 * access to text on the file associated with <handle>.
 */

Dbg_Error Dbg_ControlSourceFileAccess(
    Dbg_MCState *state, uint32 cachesize, bool closefiles);
/* Control details of how the toolbox manages source files.
 *   If <cachesize> is non-zero, the text from the most recently accessed
 *     source files (of total size not to exceed <cachesize>) is saved in
 *     store on first access to the file; subsequent access to the text of
 *     the file uses this copy.
 *   If <closefiles> is true, no stream is left attached to uncached source
 *     files after Dbg_EndFileAccess has been closed. Otherwise, the toolbox
 *     may retain such streams, in order to improve access.
 */

/*--------------------------------------------------------------------------*/

/* disassembly */

/*
 ? More exact control is wanted here, but that requires a more complicated
 ? disass callback interface.
 */

typedef const char *Dbg_SWI_Decode(Dbg_MCState *state, ARMword swino);

Dbg_Error Dbg_InstructionAt(Dbg_MCState *state, ARMaddress addr,
                    int isize, ARMhword *inst, Dbg_SymTable *st,
                    Dbg_SWI_Decode *swi_name, Dbg_BufDesc *buf, int *length);
/* <isize> describes the form of disassembly wanted: 2 for 16-bit, 4 for 32-bit,
 * 0 for 16- or 32-bit depending whether addr addresses 16- or 32-bit code.
 * <inst> is a pointer to a pair of halfwords *in target byte order*
 * Possibly only the first halfword will be consumed: the number of bytes used
 * is returned via <length>.
 */

/*--------------------------------------------------------------------------*/

int Dbg_RDIOpen(Dbg_MCState *state, unsigned type);
int Dbg_RDIInfo(Dbg_MCState *state, unsigned type, ARMword *arg1, ARMword *arg2);

/*--------------------------------------------------------------------------*/

typedef enum {
    Dbg_Point_Toolbox,
    Dbg_Point_RDI_Unknown,
    Dbg_Point_RDI_SW,
    Dbg_Point_RDI_HW
} Dbg_PointType;

/* breakpoint management
   Associated with a breakpoint there may be any of
     a count
     an expression
     a function
   the breakpoint is activated if
        the expression evaluates to a non-zero value (or fails to evaluate).
     && decrementing the count reaches zero (the count is then reset to its
        initial value).
     && the function, called with the breakpoint address as argument, returns
        a non-zero value.
?  (The order here may be open to debate.  Note that the first two are in
    the opposite order in armsd, but I think this order more rational)
 */

typedef enum Dbg_BreakPosType {
    bt_procpos,
    bt_procexit,
    bt_address
} Dbg_BreakPosType;

typedef union {
      Dbg_ProcPos procpos;
      Dbg_ProcDesc procexit;
      ARMaddress address;
} Dbg_BreakPosPos;

typedef struct Dbg_BreakPos {
    Dbg_BreakPosType sort;
    Dbg_BreakPosPos loc;
} Dbg_BreakPos;

typedef int Dbg_BPProc(Dbg_MCState *state, void *BPArg, Dbg_BreakPos *where);

typedef struct Dbg_BreakStatus {
    int index;
    int initcount, countnow;
    Dbg_BreakPos where;
    char *expr;
    Dbg_BPProc *p; void *p_arg;
    int incomplete;
    Dbg_PointType type;
    ARMword hwresource;
} Dbg_BreakStatus;

Dbg_Error Dbg_StringToBreakPos(
    Dbg_MCState *state, Dbg_Environment *env, char const *str, size_t len,
    Dbg_BreakPos *bpos, char *b);

Dbg_Error Dbg_SetBreakPoint(Dbg_MCState *state, Dbg_BreakPos *where,
                    int count,
                    const char *expr,
                    Dbg_BPProc *p, void *arg);
Dbg_Error Dbg_SetBreakPoint16(Dbg_MCState *state, Dbg_BreakPos *where,
                    int count,
                    const char *expr,
                    Dbg_BPProc *p, void *arg);
Dbg_Error Dbg_SetBreakPointNaturalSize(Dbg_MCState *state, Dbg_BreakPos *where,
                    int count,
                    const char *expr,
                    Dbg_BPProc *p, void *arg);
/* Setting a breakpoint at the same address as a previous breakpoint
   completely removes the previous one.
 */

Dbg_Error Dbg_DeleteBreakPoint(Dbg_MCState *state, Dbg_BreakPos *where);

Dbg_Error Dbg_SuspendBreakPoint(Dbg_MCState *state, Dbg_BreakPos *where);
/* Temporarily remove the break point (until Reinstated) but leave intact
   its associated expr, the value its count has reached, etc.
?  The debugger toolbox itself wants this, but I'm not sure what use a client
   could have for it.  Ditto Reinstate...
 */

Dbg_Error Dbg_ReinstateBreakPoint(Dbg_MCState *state, Dbg_BreakPos *where);
/* Undo the effect of Dbg_SuspendBreakPoint
 */

Dbg_Error Dbg_DeleteAllBreakPoints(Dbg_MCState *state);

Dbg_Error Dbg_SuspendAllBreakPoints(Dbg_MCState *state);

Dbg_Error Dbg_ReinstateAllBreakPoints(Dbg_MCState *state);

typedef Dbg_Error Dbg_BPEnumProc(Dbg_MCState *state, Dbg_BreakStatus *status, void *arg);

Dbg_Error Dbg_EnumerateBreakPoints(Dbg_MCState *state, Dbg_BPEnumProc *p, void *arg);

Dbg_Error Dbg_BreakPointStatus(Dbg_MCState *state,
                       const Dbg_BreakPos *where, Dbg_BreakStatus *status);

typedef void Dbg_BreakAlteredProc(Dbg_MCState *state, ARMaddress addr, bool set);
Dbg_Error Dbg_OnBreak(Dbg_MCState *state, Dbg_BreakAlteredProc *p);
/* Register function to be called back whenever a breakpoint is set or
 * cleared. (To allow multiple toolbox clients to coexist).
 */

bool Dbg_StoppedAtBreakPoint(Dbg_MCState *state, const Dbg_BreakPos *where);
/* Called after execution which resulted in ps_atbreak, to find out whether
   the specified breakpoint was hit (could be >1, eg. exit break and another
   high-level breakpoint at the same position).
   Returns NO if specified breakpoint not found, or execution didn't stop
   with ps_atbreak status.
 */

/*--------------------------------------------------------------------------*/

typedef struct {
  Dbg_Value val;
  char *name;
} Dbg_WatchPos;

typedef int Dbg_WPProc(Dbg_MCState *state, void *WPArg, Dbg_WatchPos *where);

typedef struct Dbg_WPStatus {
    int index;
    int initcount, countnow;
    Dbg_WatchPos what, target;
    char *expr;
    Dbg_WPProc *p; void *p_arg;
    Dbg_PointType type;
    ARMword hwresource;
    int skip;
} Dbg_WPStatus;

Dbg_Error Dbg_SetWatchPoint(
    Dbg_MCState *state, Dbg_Environment *context, char const *watchee,
    char const *target,
    int count,
    char const *expr,
    Dbg_WPProc *p, void *arg);

/* Cause a watchpoint event if the value of <watchee> changes to the value of
   <target> (or changes at all if <target> is NULL).  <watchee> should
   evaluate either to an L-value (when the size of the object being watched is
   determined by its type) or to an integer constant (when the word with this
   address is watched).
 */

Dbg_Error Dbg_DeleteWatchPoint(Dbg_MCState *state, Dbg_Environment *context, char const *watchee);


Dbg_Error Dbg_SetWatchPoint_V(
    Dbg_MCState *state,
    char const *name, Dbg_Value const *val, char const *tname, Dbg_Value const *tval,
    int count,
    char const *expr,
    Dbg_WPProc *p, void *arg);

Dbg_Error Dbg_DeleteWatchPoint_V(Dbg_MCState *state, Dbg_Value const *val);


Dbg_Error Dbg_DeleteAllWatchPoints(Dbg_MCState *state);

typedef Dbg_Error Dbg_WPEnumProc(Dbg_MCState *state, Dbg_WPStatus const *watchee, void *arg);

Dbg_Error Dbg_EnumerateWatchPoints(Dbg_MCState *state, Dbg_WPEnumProc *p, void *arg);

Dbg_Error Dbg_WatchPointStatus(Dbg_MCState *state,
                       Dbg_WatchPos const *where, Dbg_WPStatus *status);

typedef void Dbg_WPRemovedProc(void *arg, Dbg_WPStatus const *wp);
Dbg_Error Dbg_RegisterWPRemovalProc(Dbg_MCState *state, Dbg_WPRemovedProc *p, void *arg);
/* When a watchpoint goes out of scope it is removed by the toolbox, and the
   function registered here gets called back to adjust its view
 */

typedef void Dbg_WatchAlteredProc(Dbg_MCState *state, Dbg_Value const *where, bool set);
Dbg_Error Dbg_OnWatch(Dbg_MCState *state, Dbg_WatchAlteredProc *p);
/* Register function to be called back whenever a watchpoint is set or
 * cleared. (To allow multiple toolbox clients to coexist).
 */

/*--------------------------------------------------------------------------*/

Dbg_Error Dbg_ProfileLoad(Dbg_MCState *state);

Dbg_Error Dbg_ProfileStart(Dbg_MCState *state, ARMword interval);
Dbg_Error Dbg_ProfileStop(Dbg_MCState *state);

Dbg_Error Dbg_ProfileClear(Dbg_MCState *state);

Dbg_Error Dbg_WriteProfile(Dbg_MCState *state, char const *filename,
                           char const *toolid, char const *arg);

/*--------------------------------------------------------------------------*/

Dbg_Error Dbg_ConnectChannel_ToHost(Dbg_MCState *state, RDICCProc_ToHost *p, void *arg);
Dbg_Error Dbg_ConnectChannel_FromHost(Dbg_MCState *state, RDICCProc_FromHost *p, void *arg);

/*--------------------------------------------------------------------------*/

/* Configuration data management */

Dbg_Error Dbg_LoadConfigData(Dbg_MCState *state, char const *filename);

Dbg_Error Dbg_SelectConfig(
    Dbg_MCState *state,
    RDI_ConfigAspect aspect, char const *name, RDI_ConfigMatchType matchtype,
    unsigned versionreq, unsigned *versionp);

Dbg_Error Dbg_ParseConfigVersion(
    char const *s, RDI_ConfigMatchType *matchp, unsigned *versionp);

typedef Dbg_Error Dbg_ConfigEnumProc(Dbg_MCState *state, RDI_ConfigDesc const *desc, void *arg);

Dbg_Error Dbg_EnumerateConfigs(Dbg_MCState *state, Dbg_ConfigEnumProc *p, void *arg);

/*--------------------------------------------------------------------------*/

/* Angel OS support */

Dbg_Error Dbg_CreateTask(Dbg_MCState **statep, Dbg_MCState *parent, bool inherit);
/* This is called when a task is to be debugged which has not been debugged
   before - ie. there is no existing Dbg_MCState for this task. It
   initialises a new Dbg_MCState and returns a pointer to it.
   <parent> is any valid previously-created MCCState. If <inherit> is TRUE,
   the new MCState inherits certain features from it (eg. symbols).
   Otherwise, only features which are the same across all tasks are inherited,
   (eg. global breakpoints).
 */

Dbg_Error Dbg_DeleteTask(Dbg_MCState *state);
/* This is called when a task dies, and frees up everything which relates to that
   task which is controlled by armdbg.
 */

Dbg_Error Dbg_DetachTask(Dbg_MCState *state);

Dbg_Error Dbg_AttachTask(Dbg_MCState *state);
/* These are called to request a switch of the current task.  First
   Dbg_DetachTask should be called with the state of the old task.
   Dbg_DetachTask will ensure that any cached state held by armdbg for
   the old task is immediately written out to the target.

   After Dbg_DetachTask is called and before Dbg_AttachTask is called
   the OS channel manager should tell the target that any future
   requests from the debugger will be fore the new task.

   If the new task does not have an armdbg state structure
   already, then Dbg_CreateTask should be called to create one (see
   above).  Then Dbg_AttachTask is called to tell armdbg to treat the
   new armdbg state as the current task.
 */

typedef Dbg_Error Dbg_TaskSwitchProc(void *arg, Dbg_MCState *newstate);

Dbg_Error Dbg_OnTaskSwitch(Dbg_MCState *state, Dbg_TaskSwitchProc *fn, void *arg);
/* The front end may register a callback which gets called by armdbg whenever
   Dbg_AttachTask is called.  This callback tells the front end the new current
   Dbg_MCState it should use to call armdbg.
   [Note that this is only useful if there is one front end shared between all
    tasks rather than one front end per task]
   The value of <arg> passed to Dbg_OnTaskSwitch is passed to <fn>
   when it is called.
 */

typedef Dbg_Error Dbg_RestartProc(
    void *arg, Dbg_MCState *curstate, Dbg_MCState **newstate);

Dbg_Error Dbg_OnRestart(Dbg_MCState *state, Dbg_RestartProc *fn, void *arg);
/* This is used by the OS channels layer to register a callback which
   will be made by the debugger toolbox early in the process of resuming
   execution.

   This callback must determine which task will be resumed when the target
   restarts execution.  If this is not already the current task then it must
   call Dbg_DetachTask and Dbg_AttachTask as decribed above to switch to the
   task about to be resumed and return the state for the new task in
   <newstate>.

   This will ensure that armdbg updates the correct task on execution as well
   as ensuring that stepping over a breakpointed instruction on restarting
   happens correctly.

   The value of <arg> passed to Dbg_OnRestart is passed to <fn>
   when it is called.
 */


#ifdef __cplusplus
}
#endif

#endif

/* End of armdbg.h */
