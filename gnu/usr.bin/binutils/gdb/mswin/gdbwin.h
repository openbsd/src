#ifdef __cplusplus
extern "C" {
#endif
#define atof atof
#undef const
#define const const
#undef max
#undef min
#include "../defs.h"
extern char gdb_dirbuf[];


#include "../gdbtypes.h"
#include "../symtab.h"
#include "../breakpoint.h"
#include "../value.h"
#include "../frame.h"
#include "../serial.h"

// Stuff in gdb we need 

struct block *togdb_get_frame_block(struct frame_info *s);
const int togdb_pcreg();
const int togdb_ccrreg();
const int togdb_maxregs();
void togdb_eval_as_string_worker(const char *);
void togdb_command(const char *);

void togdb_command_from_tty (const char *)		 ;
void togdb_force_update();
const char*togdb_symtab_to_filename(struct symtab *);
const char*togdb_symtab_to_fullname(struct symtab *);
void togdb_symtab_search_for_fullname(struct symtab *);
struct symbol *togdb_block_getblocksym(struct block *, int i);
struct symbol *togdb_block_getfunction(struct block *p);
struct block *togdb_block_getsuperblock(struct block *p);
int togdb_block_getnsyms(struct block *p);
void togdb_do_cleanups_ALL_CLEANUPS();
void togdb_restore_cleanups(void *p);
const char *doing_something (const char *s);
int togdb_target_has_registers();
struct symbol *togdb_get_block_sym(struct frame_info *, int i);
int togdb_find_pc_partial_function (CORE_ADDR pc,
				     char **name,
				     CORE_ADDR *low,
				     CORE_ADDR *high);

int togdb_searchpath (char *path, int try_cwd_first, const char *string, char **filename_opened);

const char*togdb_get_source_path();
void togdb_set_source_path(const char *);

char *togdb_get_info_path (void);
void togdb_set_info_path (const char *);

extern struct frame_info *selected_frame;
struct symbol *my_lookup_symbol(const char *);

struct tregs 
  {
      int regs[18];
      int changed[18];
  };

struct lineinfo 
  {
  int line;
  CORE_ADDR pc;
  };



struct frame_annotated_info {
  int called_by;
#define CALLED_BY_GDB 1
#define CALLED_BY_SIGNAL 2
#define CALLED_BY_PROG 3
  char *funcname;
  char *filename;
  int line;
  struct symtab *symtab;

};

void togdb_annotate_info(struct frame_info *, struct frame_annotated_info*);
struct gui_symtab_file *gdbwin_list_symbols (const char *, int);
void gdbwin_list_symbols_free(struct gui_symtab_file *);

int catch_errors (int (*)(char *), void *, char *, int);

struct lineinfo *togdb_lineinfo(const char *filename);
void togdb_free_lineinfo (struct lineinfo *p);
CORE_ADDR togdb_fetchreg(int rn);
CORE_ADDR togdb_fetchpc();
int togdb_target_has_execution();
void togdb_fetchloc(CORE_ADDR pc, char ** filename, int *line,
		    struct symtab **s);
void togdb_set_breakpoint_sal (struct symtab *, int);
void *togdb_breakinfo_i_next(void*);
void *togdb_breakinfo_i_init();
int togdb_disassemble (CORE_ADDR, char *);
void togdb_bpt_set(const char *s);

int  bi_type(void *);
int  bi_disposition(void *);
int  bi_enable(void *);
CORE_ADDR  bi_address(void *);
int  bi_linenumber(void *);
const char*  bi_sourcefile(void *);
const char*  bi_condstring(void *);
const char*  bi_expstring(void *);
const char*  bi_addrstring(void *);
int bi_number(void *);
int bi_hitcount(void *);
void bi_delete_breakpoint(void *);
void bi_delete_all();
void bi_disable_bpt(void *);
void bi_enable_bpt(void *);

const char *togdb_symtab_filename(struct symtab *);
//const char *togdb_symbol_name(struct symbol *);
char *const togdb_symbol_value(struct symbol *);
const int  togdb_symbol_line(struct symbol *);

CORE_ADDR togdb_lineinfo_getpc(struct lineinfo *);
int  togdb_lineinfo_getline(struct lineinfo *);
CORE_ADDR togdb_frameinfo_frameaddr(struct frame_info *);
CORE_ADDR togdb_frameinfo_framepc(struct frame_info *);
struct frame_info *togdb_frameinfo_prevframe(struct frame_info *);
struct frame_info *togdb_frameinfo_getcurrentframe();
/* Other crap */

int re_exec(const char *);
#ifdef __cplusplus
};

class CBreakInfo : public breakpoint
{
  void *m_fake;
 public:
    int GetNumber() { return number;}
  enum bptype GetType() { return type;}
  enum bpdisp GetDisposition() { return disposition;}
  enum enable GetEnable() { return enable;}
  CORE_ADDR GetAddress() { return address;}
  int GetLineNumber() { return line_number;}
  const char *GetSourceFile() { return source_file;}
  const char *GetCondString() { return cond_string;}
  const char *GetExpString() { return exp_string;}
  const char *GetAddrString() { return addr_string;}
  int GetHitCount() {  return hit_count; }


  CBreakInfo() {};
  void DoDelete();
  void DoDisable();
  void togdb_bpt_set (const char *p);

};

#if 1
class CBreakInfoList {
 public:
    CPtrArray m_info;
    CBreakInfoList();
    CBreakInfo *GetAt(int idx);
    int GetSize();
    int GetPCIdx(  CORE_ADDR addr);
    void Delete(int idx);
    void Disable(int idx);
    void Enable(int idx);
    void Update();
    void DeleteAll();
};

#endif

class CBlock : public block
{
 public:

  class CSymbol *GetBlockSym(int i)  { return (CSymbol *)BLOCK_SYM(this,i);}
  class CSymbol *GetFunction() { return (CSymbol *)BLOCK_FUNCTION(this);}
  class CBlock *GetSuperBlock() { return (CBlock *)BLOCK_SUPERBLOCK(this);}
  int GetNSyms() { return BLOCK_NSYMS(this);}
};
class CFrameInfo : public frame_info {

 public:

  class CFrameInfo *get_prev_frame() { return (CFrameInfo *)::get_prev_frame(this);}
  CORE_ADDR GetFrameAddr();
    CORE_ADDR GetFramePC() { return pc;}

  class CBlock *GetFrameBlock()  { return (CBlock *) togdb_get_frame_block(this);}
  class CSymbol *GetBlockSym(int i) { return (CSymbol *) togdb_get_block_sym(this, i);}
  static CFrameInfo *GetCurrentFrame() { return (CFrameInfo *)get_current_frame();}
};

class CSymbol :public symbol 
{
 public:
   enum address_class GetSymbolClass() { return aclass;}
     const  char *GetName()  { return SYMBOL_NAME(this);}
const   int GetLine();
CORE_ADDR  GetValue();
   CORE_ADDR GetSymbolValueAddress() {return SYMBOL_VALUE_ADDRESS(this);}

   class CType *GetType() { return (CType *) type;}
     static CSymbol *Lookup(const char *name);
       class CValue *ReadVarValue(class CFrameInfo *frame)
	 { return (CValue *)read_var_value(this, frame);}
 };
class CLineInfo:  public lineinfo
 {
 public:
  CORE_ADDR getpc();
  int getline();
};

class CLineInfoArray {
 CString filename;
 public:
  CLineInfoArray (CString name);
  class CLineInfo *ptr;
};

class CSymtab 	  : public symtab
{

 public:

 void search_for_fullname() { togdb_symtab_search_for_fullname(this);}
  const char *to_fullname() { return togdb_symtab_to_fullname(this);}
  const char *to_filename() { return togdb_symtab_to_filename(this);}
  const  char *get_filename()  { return filename;}
  CLineInfo *calc_lineinfo();

};
int togdb_updatebptinfo(CBreakInfoList *);
void togdb_fillbptlistbox(CListBox &listbox);
CString togdb_eval_as_string (const char *);

class CValue : public value
{
 public:
  CORE_ADDR GetAddress() { return VALUE_ADDRESS(this);}
  LONGEST GetInt() { return unpack_long (VALUE_TYPE(this), VALUE_CONTENTS(this));}
  const char *GetEnumName ();
  int GetRegno() { return (int)VALUE_ADDRESS(this); }
  enum lval_type  GetLVal() { return VALUE_LVAL(this);}
};


class CType  : public type
{
 public:	 ;

};


#endif


#if 1
/* Communication between gdb symtabs and gui */
#define GUI_FILE 1
#define GUI_ITEM 2
struct gui_symtab_item
{
  int type;
  struct gui_symtab_item *next_item;
#ifdef __cplusplus
  CSymbol *sym;
#else
  struct symbol *sym;
#endif
  struct gui_symtab_file *parent;
};
struct gui_symtab_file
{
  int type;
  struct gui_symtab_file *next_file;
  struct gui_symtab_item *items;
#ifdef __cplusplus
  CSymtab *tab;
#else
  struct symtab *tab;
#endif
  int opened;
};
union gui_symtab
{
  int type;
  struct gui_symtab_file as_file;
  struct gui_symtab_item as_item;
};








/*struct cleanup *my_make_cleanup  (void *fptr, void *arg);*/
#endif
