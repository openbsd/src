/* Program to write out opcode tables for the W65816 and friends
   Copyright (C) 1995 Free Software Foundation, Inc.
   Written by Steve Chamberlain sac@cygnus.com


GDB is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GDB is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#include <stdio.h>


struct opinfo
  {
    int code;
    char *opcode;
    char *mode;
    int clocks;
    int cpu;
    struct ainfo *ai;
    struct oinfo *oi;
  };

#define W16_ONLY 1
#define C02_ONLY 2

struct ainfo
  {
    char *name;
    char *enumname;
    char *disasmstring;
    char *sizer;
    char *reloc0;
    char *howlval;

    /* If addr val could be reg addr  - used for disasssmbly of 
       args into reg names - you want lda <0x10 to turn into lda <r0
       but you don't want lda #0x10 to do the same. */
    char regflag; 
  };

#define GET_M 	1
#define SPECIAL_CASE 	2
#define COP_GET 	3
#define G2_GET 		4
#define BRANCH		5
#define GET_X 6
#define STANDARD_PC_GET 	7
#define PUSH_16 8
#define PUSH_8 9
#define PUSH_X 10
#define PUSH_M 11
#define POP_16 12
#define POP_8 13
#define POP_X 14
#define POP_M 15
#define STORE_M  16
#define STORE_X  17
struct oinfo
  {
    char *name;
    int howsrc;
    char *howto;
  };
struct oinfo olist[] =
{
  {"adc", GET_M, "{ int old_acc = GET_A; int old_src =src; src = old_src + old_acc + GET_CBIT; SET_NBIT_M(src); SET_VBIT_M(old_src, old_acc, src); SET_CBIT_M(src); SET_ZBIT_M(src); SET_A(src);}"},
  {"and", GET_M, "src = GET_A & src; SET_NBIT_M(src); SET_ZBIT_M(src);SET_A(src);"},
  {"asl", G2_GET, " src <<=1; SET_CBIT_M(src); SET_ZBIT_M(src);SET_NBIT_M(src);"},
  {"bcc", BRANCH, "GET_CBIT==0"},
  {"bcs", BRANCH, "GET_CBIT==1"},
  {"beq", BRANCH, "GET_ZBIT==1"},
  {"bit", GET_M, "SET_NBIT_M(src); SET_VBIT((src >> (GET_MBIT ? 6:14 ) &1)); SET_ZBIT_M (GET_A & src);"},
  {"bmi", BRANCH, "GET_NBIT==1"},
  {"bne", BRANCH, "GET_ZBIT==0"},
  {"bpl", BRANCH, "GET_NBIT==0"},
  {"bra", BRANCH, "1"},
  {"brk", SPECIAL_CASE,
   "\
{\
   if (GET_E == 1) \
     { PUSH16(GET_PC + 1); PUSH8 (GET_P | BFLAG); SET_P (GET_P | IFLAG); SET_ONLY_PC(fetch16 (0xfffe));}\
   else \
     { PUSH8 (GET_PBR_LOW); PUSH16 (GET_PC); PUSH8(GET_P); SET_P(GET_P |IFLAG); SET_PBRPC(fetch16 (0xffe6));};\
     }"},
  {"brl", BRANCH, "1"},
  {"bvc", BRANCH, "GET_VBIT==0"},
  {"bvs", BRANCH, "GET_VBIT==1"},
  {"clc", SPECIAL_CASE, "SET_CBIT(0);"},
  {"cld", SPECIAL_CASE, "SET_DBIT(0);"},
  {"cli", SPECIAL_CASE, "SET_IBIT(0);"},
  {"clv", SPECIAL_CASE, "SET_VBIT(0);"},
  {"cmp", GET_M, "src = GET_A - src; SET_ZBIT_M(src); SET_CBIT_M(~src); SET_NBIT_M(src);"},
  {"cop", COP_GET,
   "\
{\
   if (GET_E == 1) \
     { PUSH16(GET_PC + 1); PUSH8 (GET_P | BFLAG); SET_P ((GET_P | IFLAG) & ~DFLAG); SET_ONLY_PC(fetch16 (0xfff4));}\
   else \
     { PUSH8 (GET_PBR_LOW); PUSH16 (GET_PC); PUSH8(GET_P); SET_P((GET_P |IFLAG) & ~DFLAG); SET_PBRPC(fetch16 (0xffe4));};\
     }"},
  {"cpx", GET_X, "src = GET_X - src; SET_NBIT_X(src); SET_ZBIT_X(src); SET_CBIT_X(~src);"},
  {"cpy", GET_X, "src = GET_Y - src; SET_NBIT_X(src); SET_ZBIT_X(src); SET_CBIT_X(~src);"},
  {"dec", G2_GET, "src --; SET_NBIT_M(src); SET_ZBIT_M(src); "},
  {"dex", SPECIAL_CASE, "SET_X(GET_X -1); SET_NBIT_X(GET_X); SET_ZBIT_X(GET_X);"},
  {"dey", SPECIAL_CASE, "SET_Y(GET_Y -1); SET_NBIT_X(GET_Y); SET_ZBIT_X(GET_Y);"},
  {"eor", GET_M, "src = GET_A ^ src;  SET_NBIT_M(src); SET_ZBIT_M(src);SET_A(src); "},
  {"inc", G2_GET, "src ++; SET_NBIT_M(src); SET_ZBIT_M(src); "},
  {"inx", SPECIAL_CASE, "SET_X(GET_X +1); SET_NBIT_X(GET_X); SET_ZBIT_X(GET_X);"},
  {"iny", SPECIAL_CASE, "SET_Y(GET_Y +1); SET_NBIT_X(GET_Y); SET_ZBIT_X(GET_Y);"},
  {"jmp", STANDARD_PC_GET, "SET_ONLY_PC(lval);"},
  {"jsr", STANDARD_PC_GET, "if (l) { PUSH8(GET_PBR_LOW);} PUSH16(GET_PC); SET_ONLY_PC(lval);\n"},
  {"lda", GET_M, "SET_A(src); SET_NBIT_M(GET_A); SET_ZBIT_M(GET_A);"},
  {"ldx", GET_X, "SET_X(src);SET_NBIT_X(GET_X); SET_ZBIT_X(GET_X);"},
  {"ldy", GET_X, "SET_Y(src);SET_NBIT_X(GET_Y); SET_ZBIT_X(GET_Y);"},
  {"lsr", G2_GET, 
       "SET_CBIT(src & 1); \
        SET_NBIT(0);       \
         src = src >> 1;   \
         SET_ZBIT_M(src);"},
  {"mvn", SPECIAL_CASE,
   "{ int dst_bank; int src_bank; dst_bank = fetch8 ( GET_PC)<<16; INC_PC(1); src_bank = fetch8 (GET_PC)<<16; INC_PC(1);\
       do {  store8 ( dst_bank + GET_X, fetch8 (src_bank +  GET_Y)); SET_X(GET_X+1); SET_Y(GET_Y+1); SET_A((GET_A-1) & 0xffff); } while (GET_A != 0xffff);}"},
  {"mvp", SPECIAL_CASE,
   "{ int dst_bank; int src_bank; dst_bank = fetch8 ( GET_PBRPC)<<16; INC_PC(1); src_bank = fetch8 (GET_PBRPC)<<16; INC_PC(1);\
       do {  store8 ( dst_bank + GET_X, fetch8 (src_bank + GET_Y)); SET_X(GET_X-1); SET_Y(GET_Y-1); SET_A((GET_A-1) & 0xffff); } while (GET_A != 0xffff);}"},

  {"nop", SPECIAL_CASE, ""},
{"ora", GET_M, "SET_A(GET_A | src); SET_NBIT_M(GET_A); SET_ZBIT_M(GET_A);"},
  {"pea", PUSH_16, "src = fetch16(GET_PBRPC); INC_PC(2);"},
  {"pei", PUSH_16, "src = fetch16(fetch8(GET_PBRPC) + GET_D); INC_PC(1);"},
  {"per", PUSH_16, "src = fetch16(GET_PBRPC) + GET_PC+2; INC_PC(2);"},
  {"pha", PUSH_M, "src = GET_A;"},
  {"phb", PUSH_8, "src = GET_DBR_LOW;"},
  {"phd", PUSH_16, "src = GET_DPR;"},
  {"phk", PUSH_8, "src = GET_PBR_LOW;"},
  {"php", PUSH_8, "src = GET_P;"},
  {"phx", PUSH_X, "src = GET_X;"},
  {"phy", PUSH_X, "src = GET_Y;"},
  {"pla", POP_M, "SET_A( src); SET_NBIT_M(GET_A);SET_ZBIT_M(GET_A);"},
  {"plb", POP_8, "SET_DBR_LOW(src);SET_NBIT_8(src); SET_ZBIT_8(src);"},
  {"pld", POP_16, "SET_DPR(src);SET_NBIT_16(src); SET_ZBIT_16(src);"},
  {"plp", POP_8, "SET_P(src); RETHINK;"},
  {"plx", POP_X, "SET_X(src);SET_NBIT_X(src);SET_ZBIT_X(src);"},
  {"ply", POP_X, "SET_Y(src);SET_NBIT_X(src);SET_ZBIT_X(src);"},
  {"rep", COP_GET, "SET_P(GET_P & ~src); RETHINK;"},
  {"rol", G2_GET, "src = (src << 1) | GET_CBIT; SET_CBIT((src >> (GET_MBIT ? 7 : 15)) &1); SET_ZBIT_M(src);"},
  {"ror", G2_GET, "{ int t = src; src  = (src>>1) | (GET_CBIT<<((GET_MBIT ? 8:16)-1));SET_NBIT_M(src);SET_ZBIT_M(src); SET_CBIT(t&1);}"},
  {"rti", SPECIAL_CASE, "{ int t; POP16(t); SET_ONLY_PC(t); if (GET_E==0) { POP8(t); SET_PBR(t);}  POP8(t);SET_P(t);}"},
  {"rtl", SPECIAL_CASE, "{ int t; POP24(t); SET_PBRPC(t);}"},
  {"rts", SPECIAL_CASE, "{ int t; POP16(t); SET_ONLY_PC(t);}"},
  {"sbc", GET_M,
   "{ int old_acc = GET_A & AMASK; int old_src = src & AMASK;  src = old_acc - old_src - !GET_CBIT; SET_A(src);\
    SET_CBIT(!(src>>(GET_MBIT?8:16) &1)); SET_VBIT_M(old_src,old_acc, src); SET_ZBIT_M(src); SET_NBIT_M(src);}"},
  {"sec", SPECIAL_CASE, "SET_CBIT(1);"},
  {"sed", SPECIAL_CASE, "SET_DBIT(1);"},
  {"sei", SPECIAL_CASE, "SET_IBIT(1);"},
  {"sep", COP_GET, "SET_P(GET_P | src);RETHINK;"},
  {"sta", STORE_M, "src = GET_A;"},
  {"stp", SPECIAL_CASE, "abort();"},
  {"stx", STORE_X, "src = GET_X;"},
  {"sty", STORE_X, "src = GET_Y;"},
  {"stz", STORE_M, "src = 0;"},
{"tax", SPECIAL_CASE, "SET_X(GET_A); SET_NBIT_X(GET_A);SET_ZBIT_X(GET_A);"},
{"tay", SPECIAL_CASE, "SET_Y(GET_A); SET_NBIT_X(GET_A);SET_ZBIT_X(GET_A);"},
  {"tcd", SPECIAL_CASE, "SET_DPR(GET_A); SET_NBIT_X(GET_A); SET_ZBIT_X(GET_A);"},
  {"tcs", SPECIAL_CASE, "SET_S(GET_A);"},
  {"tdc", SPECIAL_CASE, "SET_A(GET_DPR); SET_NBIT_M(GET_A); SET_ZBIT_M(GET_A);"},
  {"trb", G2_GET, "SET_ZBIT_M(src & GET_A); src = src & ~GET_A; "},
  {"tsb", G2_GET, "SET_ZBIT_M(src & GET_A); src = src | GET_A;"},
{"tsc", SPECIAL_CASE, "SET_A(GET_S); SET_NBIT_16(GET_A); SET_ZBIT_16(GET_A);"},
{"tsx", SPECIAL_CASE, "SET_X(GET_S); SET_NBIT_X(GET_X); SET_ZBIT_X(GET_X);"},
{"txa", SPECIAL_CASE, "SET_A(GET_X); SET_NBIT_M(GET_A); SET_ZBIT_M(GET_A);"},
  {"txs", SPECIAL_CASE, "SET_S(GET_X);"},
{"txy", SPECIAL_CASE, "SET_Y(GET_X); SET_NBIT_X(GET_Y); SET_ZBIT_X(GET_Y);"},
{"tya", SPECIAL_CASE, "SET_A(GET_Y); SET_NBIT_M(GET_A); SET_ZBIT_M(GET_A);"},
{"tyx", SPECIAL_CASE, "SET_X(GET_Y); SET_NBIT_X(GET_X); SET_ZBIT_X(GET_X);"},
  {"wai", SPECIAL_CASE, "wai();INC_PC(-1);"},
  {"wdm", SPECIAL_CASE, "SET_A(wdm(GET_A, GET_X));"},
  {"xba", SPECIAL_CASE,
   "if (GET_XBIT==0) { SET_A(((GET_A >> 8) & 0xff) | ((GET_A & 0xff)<<8));} else { int t = GET_A; SET_A(GET_B); SET_B(t);}; SET_NBIT_8(GET_A); SET_ZBIT_8(GET_A);"},
  {"xce", SPECIAL_CASE, "{ int t = GET_E; SET_E(GET_CBIT); SET_CBIT(GET_E);if (GET_E) { SET_MBIT(1); SET_XBIT(1);}}; goto top;"},
  0};

struct ainfo alist[] =
{
  {"#a", "IMMTOA", "#$0", "M==0 ? 2:1", "M==0 ? %sR_W65_ABS16 : %sR_W65_ABS8",   "lval = GET_PBRPC; INC_PC(GET_MBIT ? 1:2); ", 0},
  {"#c", "IMMCOP", "#$0", "1", "%sR_W65_ABS8",   "lval = GET_PBRPC; INC_PC(1); ", 0},
  {"#i", "IMMTOI", "#$0", "X==0 ? 2:1", "X==0 ? %sR_W65_ABS16 : %sR_W65_ABS8",   "lval = GET_PBRPC; INC_PC(GET_XBIT ? 1:2);", 0},
  {"A", "ACC", "a", "0", 0, "*FAIL**", 0},
  {"r", "PC_REL", "$0", "1", "%sR_W65_PCR8",   "lval = GET_PBR_HIGH + (0xffff &(fetch8sext(GET_PBRPC) + GET_PC + 1)); INC_PC(1);", 0},
  {"rl", "PC_REL_LONG", "$0", "2", "%sR_W65_PCR16",   "lval = GET_PBR_HIGH + (0xffff & (fetch16 (GET_PBRPC) + GET_PC + 2)); INC_PC(2);", 0},
  {"i", "IMPLIED", "", "0", "", 0},
  {"s", "STACK", "", "0", "", 0},
  {"d", "DIR", "<$0", "1", "%sR_W65_ABS8",   "lval = fetch8(GET_PBRPC) + GET_D; INC_PC(1);", 1},
  {"d,x", "DIR_IDX_X", "<$0,x", "1", "%sR_W65_ABS8",   "lval = fetch8(GET_PBRPC) + GET_X+ GET_D; INC_PC(1);", 1},
  {"d,y", "DIR_IDX_Y", "<$0,y", "1", "%sR_W65_ABS8",   "lval = fetch8(GET_PBRPC) + GET_Y + GET_D; INC_PC(1);", 1},
  {"(d)", "DIR_IND", "(<$0)", "1", "%sR_W65_ABS8",   "lval = GET_DBR_HIGH + (0xffff & fetch16(fetch8(GET_PBRPC) + GET_D));INC_PC(1);",1},
  {"(d,x)", "DIR_IDX_IND_X", "(<$0,x)", "1", "%sR_W65_ABS8",   "lval = GET_DBR_HIGH + (0xffff & (fetch8 (GET_PBRPC) + GET_D + GET_X)) ; INC_PC(1);",1},
  {"(d),y", "DIR_IND_IDX_Y", "(<$0),y", "1", "%sR_W65_ABS8",   "lval = fetch16(fetch8(GET_PBRPC) + GET_D) + GET_Y  + GET_DBR_HIGH;INC_PC(1);",1},
  {"[d]", "DIR_IND_LONG", "[$0]", "1", "%sR_W65_ABS8",   "lval = fetch24(GET_D + fetch8(GET_PBRPC));INC_PC(1);",1},

  {"[d],y", "DIR_IND_IDX_Y_LONG", "[$0],y", "1", "%sR_W65_ABS8",   "lval = fetch24(fetch8(GET_PBRPC) + GET_D) + GET_Y;INC_PC(1);",1},

  {"a", "ABS", "!$0", "2", "%sR_W65_ABS16",   "lval = fetch16(GET_PBRPC) + GET_DBR_HIGH; INC_PC(2) ; ",1},
  {"a,x", "ABS_IDX_X", "!$0,x", "2", "%sR_W65_ABS16",   "lval = fetch16(GET_PBRPC) + GET_DBR_HIGH + GET_X; INC_PC(2); ",1},
  {"a,y", "ABS_IDX_Y", "!$0,y", "2", "%sR_W65_ABS16",   "lval = fetch16(GET_PBRPC) + GET_DBR_HIGH + GET_Y; INC_PC(2); ", 1},
  {"al", "ABS_LONG", ">$0", "3", "%sR_W65_ABS24",   "lval = fetch24(GET_PBRPC); INC_PC(3);\nl=1;\n", 1},
  {"[a]", "ABS_IND_LONG", "[>$0]", "2", "%sR_W65_ABS16",   "lval = fetch24(fetch16(GET_PBRPC)); INC_PC(2);", 1},
  {"al,x", "ABS_LONG_IDX_X", ">$0,x", "3", "%sR_W65_ABS24",   "lval = fetch24(GET_PBRPC) + GET_X; INC_PC(3);", 1},
  {"d,s", "STACK_REL", "$0,s", "1", "%sR_W65_ABS8",   "lval = fetch8(GET_PBRPC) + GET_S; INC_PC(1);", 0},
  {"(d,s),y", "STACK_REL_INDX_IDX", "($0,s),y", "1", "%sR_W65_ABS8",   "lval = fetch16(fetch8(GET_PBRPC) + GET_S) + GET_DBR_HIGH + GET_Y;INC_PC(1);",0},
  {"(a)", "ABS_IND", "($0)", "2", "%sR_W65_ABS16",   "lval = fetch16(GET_PBRPC) + GET_DBR_HIGH; INC_PC(2);", 1},
  {"(a,x)", "ABS_IND_IDX", "($0,x)", "2", "%sR_W65_ABS16",
     "lval = fetch16((0xffff & (fetch16(GET_PBRPC) + GET_X )) + GET_PBR_HIGH) + GET_PBR_HIGH;INC_PC(2);",1},
  {"xyz", "BLOCK_MOVE", "", "2", "", 0},	
  0};


struct opinfo optable[257] =
{
  {0x00, "brk", "s"},
  {0x01, "ora", "(d,x)"},
  {0x02, "cop", "#c"},
  {0x03, "ora", "d,s"},
  {0x04, "tsb", "d"},
  {0x05, "ora", "d"},
  {0x06, "asl", "d"},
  {0x07, "ora", "[d]"},
  {0x08, "php", "s"},
  {0x09, "ora", "#a"},
  {0x0a, "asl", "A"},
  {0x0b, "phd", "s"},
  {0x0c, "tsb", "a"},
  {0x0d, "ora", "a"},
  {0x0e, "asl", "a"},
  {0x0f, "ora", "al"},
  {0x10, "bpl", "r"},
  {0x11, "ora", "(d),y"},
  {0x12, "ora", "(d)"},
  {0x13, "ora", "(d,s),y"},
  {0x14, "trb", "d"},
  {0x15, "ora", "d,x"},
  {0x16, "asl", "d,x"},
  {0x17, "ora", "[d],y"},
  {0x18, "clc", "i"},
  {0x19, "ora", "a,y"},
  {0x1a, "inc", "A"},
  {0x1b, "tcs", "i"},
  {0x1c, "trb", "a"},
  {0x1d, "ora", "a,x"},
  {0x1e, "asl", "a,x"},
  {0x1f, "ora", "al,x"},
  {0x20, "jsr", "a"},
  {0x21, "and", "(d,x)"},
  {0x22, "jsr", "al"},
  {0x23, "and", "d,s"},
  {0x24, "bit", "(d)"},
  {0x25, "and", "d"},
  {0x26, "rol", "d"},
  {0x27, "and", "[d]"},
  {0x28, "plp", "s"},
  {0x29, "and", "#a"},
  {0x2a, "rol", "A"},
  {0x2b, "pld", "s"},
  {0x2c, "bit", "a"},
  {0x2d, "and", "a"},
  {0x2e, "rol", "a"},
  {0x2f, "and", "al"},
  {0x30, "bmi", "r"},
  {0x31, "and", "(d),y"},
  {0x32, "and", "(d)"},
  {0x33, "and", "(d,s),y"},
  {0x34, "bit", "(d,x)"},
  {0x35, "and", "d,x"},
  {0x36, "rol", "d,x"},
  {0x37, "and", "[d],y"},
  {0x38, "sec", "i"},
  {0x39, "and", "a,y"},
  {0x3a, "dec", "A"},
  {0x3b, "tsc", "i"},
  {0x3c, "bit", "a,x"},
  {0x3d, "and", "a,x"},
  {0x3e, "rol", "a,x"},
  {0x3f, "and", "al,x"},
  {0x40, "rti", "s"},
  {0x41, "eor", "(d,x)"},
  {0x42, "wdm", "i"},
  {0x43, "eor", "d,s"},
  {0x44, "mvp", "xyz"},
  {0x45, "eor", "d"},
  {0x46, "lsr", "d"},
  {0x47, "eor", "[d]"},
  {0x48, "pha", "s"},
  {0x49, "eor", "#a"},
  {0x4a, "lsr", "A"},
  {0x4b, "phk", "s"},
  {0x4c, "jmp", "a"},
  {0x4d, "eor", "a"},
  {0x4e, "lsr", "a"},
  {0x4f, "eor", "al"},
  {0x50, "bvc", "r"},
  {0x51, "eor", "(d),y"},
  {0x52, "eor", "(d)"},
  {0x53, "eor", "(d,s),y"},
  {0x54, "mvn", "xyz"},
  {0x55, "eor", "d,x"},
  {0x56, "lsr", "d,x"},
  {0x57, "eor", "[d],y"},
  {0x58, "cli", "i"},
  {0x59, "eor", "a,y"},
  {0x5a, "phy", "s"},
  {0x5b, "tcd", "i"},
  {0x5c, "jmp", "al"},
  {0x5d, "eor", "a,x"},
  {0x5e, "lsr", "a,x"},
  {0x5f, "eor", "al,x"},
  {0x60, "rts", "s"},
  {0x61, "adc", "(d,x)"},
  {0x62, "per", "rl"},
  {0x63, "adc", "d,s"},
  {0x64, "stz", "d"},
  {0x65, "adc", "d"},
  {0x66, "ror", "d"},
  {0x67, "adc", "[d]"},
  {0x68, "pla", "s"},
  {0x69, "adc", "#a"},
  {0x6a, "ror", "A"},
  {0x6b, "rtl", "s"},
  {0x6c, "jmp", "(a)"},
  {0x6d, "adc", "a"},
  {0x6e, "ror", "a"},
  {0x6f, "adc", "al"},
  {0x70, "bvs", "r"},
  {0x71, "adc", "(d),y"},
  {0x72, "adc", "(d)"},
  {0x73, "adc", "(d,s),y"},
  {0x74, "stz", "d,x"},
  {0x75, "adc", "d,x"},
  {0x76, "ror", "d,x"},
  {0x77, "adc", "[d],y"},
  {0x78, "sei", "i"},
  {0x79, "adc", "a,y"},
  {0x7a, "ply", "s"},
  {0x7b, "tdc", "i"},
  {0x7c, "jmp", "(a,x)"},
  {0x7d, "adc", "a,x"},
  {0x7e, "ror", "a,x"},
  {0x7f, "adc", "al,x"},
  {0x80, "bra", "r"},
  {0x81, "sta", "(d,x)"},
  {0x82, "brl", "rl"},
  {0x83, "sta", "d,s"},
  {0x84, "sty", "d"},
  {0x85, "sta", "d"},
  {0x86, "stx", "d"},
  {0x87, "sta", "[d]"},
  {0x88, "dey", "i"},
  {0x89, "bit", "#a"},
  {0x8a, "txa", "i"},
  {0x8b, "phb", "s"},
  {0x8c, "sty", "a"},
  {0x8d, "sta", "a"},
  {0x8e, "stx", "a"},
  {0x8f, "sta", "al"},
  {0x90, "bcc", "r"},
  {0x91, "sta", "(d),y"},
  {0x92, "sta", "(d)"},
  {0x93, "sta", "(d,s),y"},
  {0x94, "sty", "d,x"},
  {0x95, "sta", "d,x"},
  {0x96, "stx", "d,x"},
  {0x97, "sta", "[d],y"},
  {0x98, "tya", "i"},
  {0x99, "sta", "a,y"},
  {0x9a, "txs", "i"},
  {0x9b, "txy", "i"},
  {0x9c, "stz", "a"},
  {0x9d, "sta", "a,x"},
  {0x9e, "stz", "a,x"},
  {0x9f, "sta", "al,x"},
  {0xa0, "ldy", "#i"},
  {0xa1, "lda", "(d,x)"},
  {0xa2, "ldx", "#i"},
  {0xa3, "lda", "d,s"},
  {0xa4, "ldy", "d"},
  {0xa5, "lda", "d"},
  {0xa6, "ldx", "d"},
  {0xa7, "lda", "[d]"},
  {0xa8, "tay", "i"},
  {0xa9, "lda", "#a"},
  {0xaa, "tax", "i"},
  {0xab, "plb", "s"},
  {0xac, "ldy", "a"},
  {0xad, "lda", "a"},
  {0xae, "ldx", "a"},
  {0xaf, "lda", "al"},
  {0xb0, "bcs", "r"},
  {0xb1, "lda", "(d),y"},
  {0xb2, "lda", "(d)"},
  {0xb3, "lda", "(d,s),y"},
  {0xb4, "ldy", "d,x"},
  {0xb5, "lda", "d,x"},
  {0xb6, "ldx", "d,y"},
  {0xb7, "lda", "[d],y"},
  {0xb8, "clv", "i"},
  {0xb9, "lda", "a,y"},
  {0xba, "tsx", "i"},
  {0xbb, "tyx", "i"},
  {0xbc, "ldy", "a,x"},
  {0xbd, "lda", "a,x"},
  {0xbe, "ldx", "a,y"},
  {0xbf, "lda", "al,x"},
  {0xc0, "cpy", "#i"},
  {0xc1, "cmp", "(d,x)"},
  {0xc2, "rep", "#c"},
  {0xc3, "cmp", "d,s"},
  {0xc4, "cpy", "d"},
  {0xc5, "cmp", "d"},
  {0xc6, "dec", "d"},
  {0xc7, "cmp", "[d]"},
  {0xc8, "iny", "i"},
  {0xc9, "cmp", "#a"},
  {0xca, "dex", "i"},
  {0xcb, "wai", "i"},
  {0xcc, "cpy", "a"},
  {0xcd, "cmp", "a"},
  {0xce, "dec", "a"},
  {0xcf, "cmp", "al"},
  {0xd0, "bne", "r"},
  {0xd1, "cmp", "(d),y"},
  {0xd2, "cmp", "(d)"},
  {0xd3, "cmp", "(d,s),y"},
  {0xd4, "pei", "d"},
  {0xd5, "cmp", "d,x"},
  {0xd6, "dec", "d,x"},
  {0xd7, "cmp", "[d],y"},
  {0xd8, "cld", "i"},
  {0xd9, "cmp", "a,y"},
  {0xda, "phx", "s"},
  {0xdb, "stp", "i"},
  {0xdc, "jmp", "[a]"},
  {0xdd, "cmp", "a,x"},
  {0xde, "dec", "a,x"},
  {0xdf, "cmp", "al,x"},
  {0xe0, "cpx", "#i"},
  {0xe1, "sbc", "(d,x)"},
  {0xe2, "sep", "#c"},
  {0xe3, "sbc", "d,s"},
  {0xe4, "cpx", "d"},
  {0xe5, "sbc", "d"},
  {0xe6, "inc", "d"},
  {0xe7, "sbc", "[d]"},
  {0xe8, "inx", "i"},
  {0xe9, "sbc", "#a"},
  {0xea, "nop", "i"},
  {0xeb, "xba", "i"},
  {0xec, "cpx", "a"},
  {0xed, "sbc", "a"},
  {0xee, "inc", "a"},
  {0xef, "sbc", "al"},
  {0xf0, "beq", "r"},
  {0xf1, "sbc", "(d),y"},
  {0xf2, "sbc", "(d)"},
  {0xf3, "sbc", "(d,s),y"},
  {0xf4, "pea", "a"},
  {0xf5, "sbc", "d,x"},
  {0xf6, "inc", "d,x"},
  {0xf7, "sbc", "[d],y"},
  {0xf8, "sed", "i"},
  {0xf9, "sbc", "a,y"},
  {0xfa, "plx", "s"},
  {0xfb, "xce", "i"},
  {0xfc, "jsr", "(a,x)"},
  {0xfd, "sbc", "a,x"},
  {0xfe, "inc", "a,x"},
  {0xff, "sbc", "al,x"},
  0};


int pfunc(a,b)
struct opinfo *a;
struct opinfo *b;
{
return strcmp(a->mode, b->mode);

}
static void
dump_table ()
{
  int x;
  int y;
  printf ("  |");
  for (x = 0; x < 16; x++)
    {
      printf ("   %x   |", x);
    }
  printf ("\n");
  printf ("  |");
  for (x = 0; x < 16; x++)
    {
      printf ("-------|");
    }
  printf ("\n");

  for (y = 0; y < 16; y++)
    {
      printf ("%x |", y);
      for (x = 0; x < 16; x++)
	{
	  struct opinfo *p = &optable[y * 16 + x];
	  if (p->opcode)
	    {
	      printf ("%-7s", p->opcode);
	    }
	  else
	    {
	      printf ("*******");
	    }
	  printf ("|");
	}
      printf ("\n");
      printf ("  |");

      for (x = 0; x < 16; x++)
	{
	  struct opinfo *p = &optable[y * 16 + x];
	  if (p->mode)
	    {
	      printf ("%-7s", p->mode);
	    }
	  else
	    {
	      printf ("*******");
	    }
	  printf ("|");
	}
      printf ("\n");
      printf ("  |");
      for (x = 0; x < 16; x++)
	{
	  printf ("-------|");
	}


      printf ("\n");
    }
}

dt ()
{
#if 0
  int i;
  for (i = 0; i < 256; i++)
    {
      struct opinfo *p = &optable[i];
      printf ("/* %02x */ ", i);
      if (p->opcode)
	printf ("{\"%s\",\"%s\",%d,%d},", p->opcode->name, p->addr->name);

      printf ("\n");
    }
#endif

}
static
void
init_table ()
{
  int i;
  for (i = 0; i < 256; i++)
    {
      struct opinfo *p = optable + i;
      struct ainfo *a;
      struct oinfo *o;
      for (a = alist; a->name; a++)
	{
	  if (strcmp (a->name, p->mode) == 0)
	    {
	      p->ai = a;
	      goto done;
	    }
	}
      printf ("bad %x\n", i);
    done:;
      for (o = olist; o->name; o++)
	{
	  if (strcmp (o->name, p->opcode) == 0)
	    {
	      p->oi = o;
	      goto doneo;
	    }
	}
      printf ("bad %x\n", i);
    doneo:;

    }
}

/* Dump the opcodes sorted by name */
static
void
assembler_table (as)
{
  int i;
  struct oinfo *o;
  struct ainfo *a;
  int n = 0;
  /* Step through the sorted list of opnames */
  printf ("			/* WDC 65816 Assembler opcode table */\n");
  printf ("			/*   (generated by the program sim/w65/gencode -a) */\n");

  for (a = alist; a->name; a++)
    {
      printf ("#define ADDR_%-20s%d\t /* %-7s */\n", a->enumname, ++n, a->name);
    }

  printf ("struct opinfo {\n\tint val;\n\tint code;\n\tchar *name;\n\tint amode;\n};\n");

  printf ("struct opinfo optable[257]={\n");
  if (as)
    {
      i = 1;
      for (o = olist; o->name; o++)
	{
	  printf ("#define O_%s %d\n", o->name, i++);
	}
      
      qsort (optable, 256, sizeof (struct opinfo), pfunc);

      printf ("#ifdef DEFINE_TABLE\n");
      for (o = olist; o->name; o++)
	{

	  for (i = 0; i < 256; i++)
	    {
	      struct opinfo *p = optable + i;

	      if (p->oi == o)
		{
		  /* This opcode is of the right name */
		  printf ("\t{0x%02X,\tO_%s,\t\"%s\",\tADDR_%s},\n", p->code, p->oi->name, p->oi->name, p->ai->enumname);
		}
	    }
	}
    }
  else
    {
      for (i = 0; i < 256; i++)
	{
	  struct opinfo *p = optable + i;
	  printf ("\t{0x%02X,\t\"%s\",\tADDR_%s},\n", i, p->oi->name, p->ai->enumname);
	}
    }

  printf ("0};\n");
  printf ("#endif\n");


  /* Generate the operand disassembly case list */

  printf ("#define DISASM()\\\n");
  {
    struct ainfo *a;
    for (a = alist; a->name; a++)
      {
	printf ("  case ADDR_%s:\\\n\t", a->enumname);
	if (strcmp (a->enumname, "BLOCK_MOVE") == 0)
	  {
	    printf ("args[0] = (asR_W65_ABS16 >>8) &0xff;\\\n");
	    printf ("\targs[1] = ( asR_W65_ABS16 & 0xff);\\\n");
	    printf ("\tprint_operand (0,\"\t$0,$1\",args);\\\n");
	  }
	else if (a->reloc0 == 0) 
	  {
	    printf ("print_operand (0, \"\t%s\", 0);\\\n", a->disasmstring );
	  }
	else if (strlen (a->reloc0))
	  {
	    printf ("args[0] = ");
	    printf (a->reloc0, "as","as");
	    printf (";\\\n");
	    printf ("\tprint_operand (%d, \"\t%s\", args);\\\n",
		    a->regflag,
		    a->disasmstring);
	  }
	
	printf ("\tsize += %s;\\\n\tbreak;\\\n", a->sizer);
      }
  }

  printf ("\n");

  /* Generate the operand size and type case list */

  printf ("#define GETINFO(size,type,pcrel)\\\n");
  {
    struct ainfo *a;
    for (a = alist; a->name; a++)
      {
	printf ("\tcase ADDR_%s: ", a->enumname);
	printf ("size = %s;type=", a->sizer);
	if (a->reloc0 && strlen (a->reloc0))
	  {
	    printf (a->reloc0, "", "");
	  }
	else
	  printf ("-1");
	printf (";pcrel=%d;", a->name[0] == 'P');
	printf ("break;\\\n");
      }
  }
  printf ("\n");
}


/* Write out examples of each opcode */
static
void
test_table ()
{
  struct opinfo *o;
  for (o = optable; o->opcode; o++)
    {
      printf ("\t%s\t", o->opcode);
      printf (o->ai->disasmstring, 0x6543210, 0x6543210);
      printf ("\n");
    }

}

static void
op_table ()
{
  struct opinfo *o;
  int i = 0;
  /* Write four optables, M=0,1 X=0,1 */

  for (o = optable; o->opcode; o++)
    {
      printf ("{0x%02x, \"%s\", \"%s\"\t},\n",
	      i++,
	      o->opcode,
	      o->mode);
    }
}

int worked_out_lval;
static void
genfetch (amode, size)
     struct ainfo *amode;
     int size;
{
  if (amode->howlval)
    {
      if (strcmp (amode->name, "A") == 0)
	{
	  /* Can't get the lval for the accumulator */
	  printf ("src = GET_A;\n");
	}
      else
	{
	  printf ("%s\n", amode->howlval);
worked_out_lval = 1;
	  if (size == 0)
	    {
	      printf ("src = fetch16 (lval);\n");
	    }
	  else
	    {
	      printf ("src = fetch8 (lval);\n");
	    }
	}
    }

}

static void
genstore (amode, size)
     struct ainfo *amode;
     int size;
{
  if (amode->howlval)
    {
      if (strcmp (amode->name, "A") == 0)
	{
	  /* Can't get the lval for the accumulator */
	  printf ("SET_A (src);\n");
	}
      else
	{
	  if (!worked_out_lval)
	    printf ("%s\n", amode->howlval);
	  if (size == 0)
	    {
	      printf ("store16(lval, src);\n");
	    }
	  else
	    {
	      printf ("store8(lval, src);\n");
	    }
	}
    }
}
/* Generate the code to simulate the instructions */
static void
code_table ()
{
  struct opinfo *o;
  int x, m;
  printf("#include \"interp.h\"\n");

  for (x = 0; x < 2; x++) {
    for (m = 0; m < 2; m++) {
      printf("ifunc_X%d_M%d() {\n",x,m);
      printf("#undef GET_MBIT\n");
      printf("#undef GET_XBIT\n");
      printf("#define GET_XBIT %d\n", x);
      printf("#define GET_MBIT %d\n", m);
      printf("STARTFUNC();\n");
      printf("do { register opcode = fetch8(GET_PBRPC); insts++; INC_PC(1);\n");
      printf ("switch (opcode) {\n");
      for (o = optable; o->opcode; o++)
	{
	  printf ("		/* %s %s */\n", o->opcode, o->ai->name);
	  printf ("case 0x%02x:\n", o->code);
	  printf ("{\n");
	  printf ("int l ;\n");
	  printf ("register int src;\n");
	  printf ("register int lval;\n");
worked_out_lval = 0;
	  switch (o->oi->howsrc)
	    {
	    case POP_M:
	      if (m == 0)
		printf ("POP16 (src);");
	      else
		printf ("POP8(src);");
	      break;
	    case POP_X:
	      if (x == 0)
		printf ("POP16 (src);");
	      else
		printf ("POP8 (src);");
	      break;
	    case POP_8:
	      printf ("POP8 (src);");
	      break;
	    case POP_16:
	      printf ("POP16 (src);");
	      break;

	    case STANDARD_PC_GET:
	      printf ("%s\n", o->ai->howlval);
	      break;

	    case GET_M:
	      genfetch (o->ai, m);
	      break;
	    case G2_GET:
	      genfetch (o->ai, m);
	      break;
	    case GET_X:
	      genfetch (o->ai, x);
	      break;
	    case BRANCH:
	      printf ("%s", o->ai->howlval);
	      break;
	    case COP_GET:
	      genfetch(o->ai,1);
	      break;
	    case STORE_X:
	    case STORE_M:
	      ;
	    }

	  switch (o->oi->howsrc)
	    {
	    case BRANCH:
	      printf ("if (%s) { SET_ONLY_PC(lval);} ", o->oi->howto);
	      break;
	    case SPECIAL_CASE:
	    case GET_M:
	    case GET_X:
	    case STORE_X:
	    case STANDARD_PC_GET:
	    case G2_GET:
	    case PUSH_16:
	    case PUSH_8:
	    case PUSH_M:
	    case PUSH_X:
	    case POP_16:
	    case POP_8:
	    case POP_M:
	    case POP_X:
	    case COP_GET:
	    case STORE_M:
	      printf ("%s", o->oi->howto);
	      break;
	    }

	  switch (o->oi->howsrc)
	    {
	    case STORE_M:
	      genstore (o->ai, m);
	      break;
	    case STORE_X:
	      genstore (o->ai, x);
	      break;
	    case PUSH_M:
	      if (m == 0)
		printf ("PUSH16 (src);");
	      else
		printf ("PUSH8(src);");
	      break;
	    case PUSH_X:
	      if (x == 0)
		printf ("PUSH16 (src);");
	      else
		printf ("PUSH8 (src);");
	      break;
	    case PUSH_8:
	      printf ("PUSH8 (src);");
	      break;
	    case PUSH_16:
	      printf ("PUSH16 (src);");
	      break;
	    case G2_GET:
	      genstore (o->ai, x, m);
	      break;
	    }
	  printf ("}\n");

	  printf ("break;\n");
	}
      printf ("}\n}\n");
      printf("while (!saved_state.exception);\n");
      printf("#undef GET_MBIT\n");
      printf("#undef GET_XBIT\n");
      printf("#define GET_MBIT (the_mbit)\n");
      printf("#define GET_XBIT (the_xbit)\n");
      
      printf("ENDFUNC();\n");
      printf("}");
    }
  }
}

int
main (ac, av)
     char **av;
{
  init_table ();

  if (ac > 1)
    {
      if (av[1][1] == 'a')
	{
	  assembler_table (1);
	}
      if (av[1][1] == 't')
	{
	  test_table ();
	}
      if (av[1][1] == 'o')
	{
	  op_table ();
	}
      if (av[1][1] == 'c')
	{
	  code_table ();
	}
    }
  else
    dump_table ();

  dt ();

  return 0;
}
