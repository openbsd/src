#define DEBUG

#define MSIZE (8*64*1024)
#define CSIZE 1000


union rtype 
    {
      unsigned long l;
      unsigned short s[2];
      unsigned char *c;
    };


/* Local register names */
typedef enum
{
  R0, R1, R2, R3, R4, R5, R6, R7,
  R_SR,				/* 8 */
  R_PC,				/* 9 */
  R_BR,				/* 10 */
  R_BP,				/* 11 */
  R_CP,				/* 14 */
  R_DP,				/* 13 */
  R_EP,				/* 12 */
  R_TP,				/* 15 */
  R_HARD_0,			/* 16 */
  R_HARD8_0,			/* 17 */
  R_LAST,
} reg_type;




typedef struct
{
  fastref type;
  union
    {
      int code;
      unsigned char *bptr;
      unsigned short *wptr;
      unsigned long *lptr;
      unsigned char **segptr;
      union rtype *rptr;
      
    }
  reg;
  int literal;
  union
    {
      unsigned char **segreg;
      unsigned short *wptr;
      union rtype *rptr;
    }
  r2;
}

ea_type;



typedef struct
{
  ea_type srca;
  ea_type srcb;
  ea_type dst;
  fastref opcode;
  fastref flags;
  int next_pc;
  int oldpc;
  int cycles;
#ifdef DEBUG
  h8500_opcode_info *op;
#endif
}

decoded_inst;



typedef struct
{
  int exception;
  union rtype   regs[20];
  


  unsigned char *memory;
  unsigned short *cache_idx;
  int cache_top;
  int maximum;
  int csize;
  decoded_inst *cache;
  int cycles;
  int insts;
  int ticks;
  int compiles;
}

cpu_state_type;
