#define fetch8(x) fetch8func((cycles++,(x)))
#define NFAKES 16
typedef struct
  {
    unsigned  a;
    unsigned  x;
    unsigned  y;
    unsigned  pc;		/* Keep pbr in there too */
    unsigned  dbr;
    unsigned  d;
    unsigned  s;
    unsigned  p;
    unsigned  e;
    unsigned  char *memory;
    unsigned  int exception;
    unsigned  int ticks;
    unsigned  int cycles;
    unsigned  int insts;
    unsigned  int r[NFAKES];
  }
saved_state_type;



#define GET_P      \
  ((GET_NBIT << 7) \
 | (GET_VBIT << 6) \
 | (GET_MBIT << 5) \
 | (GET_XBIT << 4) \
 | (GET_DBIT << 3) \
 | (GET_IBIT << 2) \
 | (GET_ZBIT << 1) \
 | (GET_CBIT << 0))

#define SET_P(_bits)   \
{ int bits = _bits;    \
SET_NBIT((bits>>7)&1); \
SET_VBIT((bits>>6)&1); \
SET_MBIT((bits>>5)&1); \
SET_XBIT((bits>>4)&1); \
SET_DBIT((bits>>3)&1); \
SET_IBIT((bits>>2)&1); \
SET_ZBIT((bits>>1)&1); \
SET_CBIT((bits>>0)&1);  }

#define BFLAG		(1<<4)
#define DFLAG 		(1<<3)

#define GET_A 		(the_a)
#define GET_E		(the_e)
#define GET_B 		(the_b)
#define GET_CBIT 	(the_cbit)
#define GET_D		(the_d)
#define GET_DBIT 	(the_dbit)
#define GET_DBR_HIGH 	(the_dbr)
#define GET_DBR_LOW 	(the_dbr >> 16)
#define GET_DPR 	(the_dpr)
#define GET_IBIT	(the_ibit)
#define GET_MBIT	(the_mbit)
#define SET_XBIT(x)     { the_xbit = x; }       
#define GET_NBIT 	(the_nbit)
#define GET_PBRPC 	(the_pc)
#define GET_PBR_HIGH 	(the_pc & 0xff0000)
#define GET_PBR_LOW 	(the_pc >> 16)
#define GET_PC 		(the_pc & 0xffff)
#define GET_S 		(the_s)
#define GET_VBIT 	(the_vbit)
#define GET_X 		(the_x)
#define GET_XBIT 	(the_xbit)
#define GET_Y 		(the_y)
#define GET_ZBIT 	(the_zbit)
#define IFLAG 		(1<<2)
#define INC_PC(x) 	{the_pc += x;}
#define POP16(x)        { int _xx; POP8(x); POP8(_xx); x+= _xx<<8;}
#define POP24(x)  	{ int _xx; POP8(x); POP8(_xx); x += _xx << 8; POP8(_xx); x += _xx << 16; }
#define POP8(x)   	{ SET_S(GET_S +1); x =  fetch8(GET_S);}
#define PUSH16(x) 	{ int _xx = x;PUSH8(_xx >> 8); PUSH8(_xx);}
#define PUSH8(x)  	{ store8(GET_S, x); SET_S(GET_S-1);}
#define SET_A(x)  	{ the_a = x & AMASK; }
#define SET_B(x)  	{ the_b = x;}
#define SET_CBIT(x)  	{ the_cbit = x;}
#define SET_CBIT_M(x) 	{ the_cbit = (x & (GET_MBIT ? 0x100: 0x10000)) != 0;}
#define SET_CBIT_X(x) 	{ the_cbit = (x & (GET_XBIT ? 0x100: 0x10000)) != 0;}
#define SET_D(x)	{the_d = x;}
#define SET_DBIT(x) 	{ the_dbit= x;}
#define SET_DBR_LOW(x) 	{the_dbr = (x<<16);}
#define SET_DPR(x) 	{ the_dpr = x;}
#define SET_E(x) 	{ the_e = x;}
#define SET_IBIT(x) 	{ the_ibit = x;}
#define SET_MBIT(x) 	{ the_mbit = x; }
#define SET_NBIT(x)     { the_nbit = x;}
#define SET_NBIT_16(x)  { the_nbit= ( ((x) & (0x8000)) != 0);}
#define SET_NBIT_8(x)  { the_nbit= ( ((x) & (0x80)) != 0);}
#define SET_NBIT_M(x)   { if (GET_MBIT) { SET_NBIT_8(x); } else { SET_NBIT_16(x);}}
#define SET_NBIT_X(x)   { if (GET_XBIT) { SET_NBIT_8(x); } else { SET_NBIT_16(x);}}
#define SET_PBR(x)      { the_pc = (the_pc & 0xffff) + ((x)<<16);}
#define SET_PBRPC(x)    { the_pc = x;}
#define SET_ONLY_PC(x)  { the_pc = (the_pc & 0xff0000) + ((x ) & 0xffff);}
#define SET_S(x)	{the_s = x; }
#define SET_VBIT_16(x)  {the_vbit = ((((int)x) < -0x8000) || (((int)x) > 0x7fff));}
#define SET_VBIT_8(x)   {the_vbit = ((((int)x) < -0x80) || (((int)x) > 0x7f));}
/*#define SET_VBIT_M(x)   { if(GET_MBIT) {SET_VBIT_8(x);}else {SET_VBIT_16(x);}}*/
#define SET_ZBIT_16(x)  { the_zbit = ((x & 0xffff) == 0);}
#define SET_ZBIT_8(x)   { the_zbit = ((x & 0xff) == 0);}
#define SET_ZBIT_M(x)   { if(GET_MBIT) {SET_ZBIT_8(x);}else {SET_ZBIT_16(x);}}
#define SET_ZBIT_X(x)   { if(GET_XBIT) {SET_ZBIT_8(x);}else {SET_ZBIT_16(x);}}
#define SET_VBIT(x)     { the_vbit = x; }
#define SET_ZBIT(x)     { the_zbit = x; }
#define SET_X(x)        { the_x = (x) & (GET_XBIT ? 0xff : 0xffff);}
#define SET_Y(x)        { the_y = (x) & (GET_XBIT ? 0xff : 0xffff);}

#define AMASK            ( GET_MBIT ? 0xff : 0xffff)
#define SMASK             ( GET_MBIT ? 0x80 : 0x8000)
#define SET_VBIT_M(s1,acc,d) ( the_vbit = ((s1 ^ acc) & (acc ^ d) & SMASK )!=0)

/*#define fetch8(x) (memory[x&MMASK])*/
#define fetch16(x) (fetch8(x) + (fetch8((x)+1)<<8))
#define fetch24(x) (fetch8(x) + (fetch8((x)+1)<<8) + (fetch8((x)+2)<<16))
#define fetch8sext(x) ((char)fetch8(x))
#define fetch16sext(x) ((short)fetch16(x))
#define store8(x,y) {memory[x&MMASK]=y;}
#define store16(x,y) { store8(x,y); store8(x+1,y>>8);}
#define SEXTM(x)  (GET_MBIT ? ((char)x): ((short)x))


#define STARTFUNC() 	\
  register unsigned char *memory;\
  int the_s;		\
  int the_b;		\
  int the_x;		\
  int the_d;		\
  int the_y;		\
  int the_dbr;		\
  int the_pc;		\
  int the_nbit;		\
  int the_vbit;		\
  int the_z;		\
  int the_mbit;		\
  int the_ibit;		\
  int the_xbit;		\
  int the_zbit;		\
  int the_cbit;		\
  int the_dbit;		\
  int the_dpr;		\
  int the_e;            \
  int the_a;		\
  int tick_start = get_now ();\
  int cycles = 0;	      \
  int insts = 0;	      \
				    \
  SET_E (saved_state.e);	    \
  SET_P (saved_state.p);	    \
  SET_A (saved_state.a);	    \
  SET_X (saved_state.x);	    \
  SET_Y (saved_state.y);	    \
  SET_ONLY_PC (saved_state.pc);	    \
  SET_PBR (saved_state.pc >> 16);   \
  SET_DBR_LOW (saved_state.dbr);    \
  SET_D (saved_state.d);	    \
  SET_S (saved_state.s);	    \
  memory = saved_state.memory ;     \
{ int k; for (k = 0; k < NFAKES; k++) 	   \
 store16(0x10 + k * 2, saved_state.r[k]); }\
				    \
    top:			      \



#define ENDFUNC() \
 rethink:          \
  saved_state.ticks += get_now () - tick_start;\
  saved_state.cycles += cycles;   \
  saved_state.insts += insts;  	  \
{ int k; for (k = 0; k < NFAKES; k++) 	   \
saved_state.r[k] = fetch16(0x10 + k * 2); }\
			       	  \
  saved_state.e = GET_E;	  \
  saved_state.p = GET_P;	  \
  saved_state.a = GET_A;	  \
  saved_state.x = GET_X;	  \
  saved_state.y = GET_Y;	  \
  saved_state.pc = GET_PBRPC;	  \
  saved_state.dbr = GET_DBR_LOW;  \
  saved_state.d = GET_D;	  \
  saved_state.s = GET_S;	  \
				  \
  return 0;			  \


extern saved_state_type saved_state;

#define MMASK 0xfffff
#define NUMSEGS 16
#define RETHINK goto rethink;
