void togdb_command(const char *string);

class Credirect 
{
  CString *ptr;
  Credirect *prev;
 public:
  Credirect(CString *p);
  Credirect();
  ~Credirect();
  void add(const char *p) { if(ptr) *ptr += p;}
};

class CErrorWrap {
  struct cleanup *saved_cleanup_chain;
  void (*prev_error_hook)();
  int firsttime;
  int passed;
public: 
CErrorWrap();
~CErrorWrap();
  int gdb_try() ;
  int gdb_catch() ;
};
#define TRYCATCH(x,y) { extern jmp_buf gobuf;if (setjmp(gobuf)==0) { x; } else {y;} }
