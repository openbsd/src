/* Simulator/Opcode generator for the Hitachi Super-H architecture.

   Written by Steve Chamberlain of Cygnus Support.
   sac@cygnus.com

   This file is part of SH sim


		THIS SOFTWARE IS NOT COPYRIGHTED

   Cygnus offers the following for use in the public domain.  Cygnus
   makes no warranty with regard to the software or it's performance
   and the user accepts the software "AS IS" with all faults.

   CYGNUS DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED, WITH REGARD TO
   THIS SOFTWARE INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

*/

/* This program generates the opcode table for the assembler and
   the simulator code

   -t		prints a pretty table for the assembler manual
   -s		generates the simulator code jump table
   -d		generates a define table
   -x		generates the simulator code switch statement
   default 	generates the opcode tables

*/

#include <stdio.h>

typedef struct
{
char *defs;
char *refs;
  char *name;
  char *code;
  char *stuff[10];
  int index;
}

op;


op tab[] =
{

  {"n","","add #<imm>,<REG_N>", "0111nnnni8*1....", "R[n] += SEXT(i);if (i == 0) { UNDEF(n); break; } "},
  {"n","mn","add <REG_M>,<REG_N>", "0011nnnnmmmm1100", "R[n] += R[m];"},
  {"n","mn","addc <REG_M>,<REG_N>", "0011nnnnmmmm1110",
     "ult=R[n]+T;T=ult<R[n];R[n]=ult+R[m];T|=R[n]<ult;"},
  {"n","mn","addv <REG_M>,<REG_N>", "0011nnnnmmmm1111",
     "ult = R[n] + R[m]; T = ((~(R[n] ^ R[m]) & (ult ^ R[n])) >> 31); R[n] = ult;"},
  {"0","","and #<imm>,R0", "11001001i8*1....", ";R0&=i;"},
  {"n","nm","and <REG_M>,<REG_N>", "0010nnnnmmmm1001", " R[n]&=R[m];"},
  {"","0","and.b #<imm>,@(R0,GBR)", "11001101i8*1....", "MA();WBAT(GBR+R0, RBAT(GBR+R0) & i);"},

  {"","","bra <bdisp12>", "1010i12.........", "ult = PC; PC=PC+(i<<1)+2;SL(ult+2);"},
  {"","","bsr <bdisp12>", "1011i12.........", "PR = PC + 4; PC=PC+(i<<1)+2;SL(PR-2);"},
  {"","","bt <bdisp8>", "10001001i8p1....", "if(T) {PC+=(SEXT(i)<<1)+2;C+=2;}"},
  {"","","bf <bdisp8>", "10001011i8p1....", "if(T==0) {PC+=(SEXT(i)<<1)+2;C+=2;}"},
  {"","","bt.s <bdisp8>", "10001101i8p1....","if(T) {ult = PC; PC+=(SEXT(i)<<1)+2;C+=2;SL(ult+2);}"},
  {"","","bf.s <bdisp8>", "10001111i8p1....","if(!T) {ult = PC; PC+=(SEXT(i)<<1)+2;C+=2;SL(ult+2);}"},
  {"","","clrmac", "0000000000101000", "MACH = MACL = 0;"},
  {"","","clrs", "0000000001001000", "S= 0;"},
  {"","","clrt", "0000000000001000", "T= 0;"},
  {"","0","cmp/eq #<imm>,R0", "10001000i8*1....", ";T = R0 == SEXT(i);"},
  {"","mn","cmp/eq <REG_M>,<REG_N>", "0011nnnnmmmm0000", "T=R[n]==R[m];"},
  {"","mn","cmp/ge <REG_M>,<REG_N>", "0011nnnnmmmm0011", "T=R[n]>=R[m];"},
  {"","mn","cmp/gt <REG_M>,<REG_N>", "0011nnnnmmmm0111", "T=R[n]>R[m];"},
  {"","mn","cmp/hi <REG_M>,<REG_N>", "0011nnnnmmmm0110", "T=UR[n]>UR[m];"},
  {"","mn","cmp/hs <REG_M>,<REG_N>", "0011nnnnmmmm0010", "T=UR[n]>=UR[m];"},
  {"","n","cmp/pl <REG_N>", "0100nnnn00010101", "T = R[n]>0;"},
  {"","n","cmp/pz <REG_N>", "0100nnnn00010001", "T = R[n]>=0;"},
  {"","mn","cmp/str <REG_M>,<REG_N>", "0010nnnnmmmm1100",
   "ult = R[n] ^ R[m]; T=((ult&0xff000000)==0) |((ult&0xff0000)==0) |((ult&0xff00)==0) |((ult&0xff)==0); "},
  {"","mn","div0s <REG_M>,<REG_N>", "0010nnnnmmmm0111", "Q=(R[n]&sbit)!=0; M=(R[m]&sbit)!=0; T=M!=Q;;"},
  {"","","div0u", "0000000000011001", "M=Q=T=0;"},
  {"","","div1 <REG_M>,<REG_N>", "0011nnnnmmmm0100", "T=div1(R,m,n,T);"},
  {"n","m","exts.b <REG_M>,<REG_N>", "0110nnnnmmmm1110", "R[n] = SEXT(R[m]);"},
  {"n","m","exts.w <REG_M>,<REG_N>", "0110nnnnmmmm1111", "R[n] = SEXTW(R[m]);"},
  {"n","m","extu.b <REG_M>,<REG_N>", "0110nnnnmmmm1100", "R[n] = R[m] & 0xff;"},
  {"n","m","extu.w <REG_M>,<REG_N>", "0110nnnnmmmm1101", "R[n] = R[m] & 0xffff;"},
  {"","n","jmp @<REG_N>", "0100nnnn00101011", "ult = PC; PC=R[n]-2; SL(ult+2);"},
  {"","n","jsr @<REG_N>", "0100nnnn00001011", "PR = PC + 4; PC=R[n]-2; if (~doprofile) gotcall(PR,PC+2);SL(PR-2);"},
  {"","n","ldc <REG_N>,SR", "0100nnnn00001110", "SET_SR(R[n]);"},
  {"","n","ldc <REG_N>,GBR", "0100nnnn00011110", "GBR=R[n];"},
  {"","n","ldc <REG_N>,VBR", "0100nnnn00101110", "VBR=R[n];"},
  {"","n","ldc <REG_N>,SSR", "0100nnnn00111110", "SSR=R[n];"},
  {"","n","ldc <REG_N>,SPC", "0100nnnn01001110", "SPC=R[n];"},
  {"","n","ldc.l @<REG_N>+,SR", "0100nnnn00000111", "MA();SET_SR(RLAT(R[n]));R[n]+=4;;"},
  {"","n","ldc.l @<REG_N>+,GBR", "0100nnnn00010111", "MA();GBR=RLAT(R[n]);R[n]+=4;;"},
  {"","n","ldc.l @<REG_N>+,VBR", "0100nnnn00100111", "MA();VBR=RLAT(R[n]);R[n]+=4;;"},
  {"","n","ldc.l @<REG_N>+,SSR", "0100nnnn00110111", "MA();SSR=RLAT(R[n]);R[n]+=4;;"},
  {"","n","ldc.l @<REG_N>+,SPC", "0100nnnn01000111", "MA();SPC=RLAT(R[n]);R[n]+=4;;"},
  {"","n","lds <REG_N>,MACH", "0100nnnn00001010", "MACH = R[n];"},
  {"","n","lds <REG_N>,MACL", "0100nnnn00011010", "MACL= R[n];"},
  {"","n","lds <REG_N>,PR", "0100nnnn00101010", "PR = R[n];"},
  {"","n","lds.l @<REG_N>+,MACH", "0100nnnn00000110", "MA();MACH = SEXT(RLAT(R[n]));R[n]+=4;"},
  {"","n","lds.l @<REG_N>+,MACL", "0100nnnn00010110", "MA();MACL = RLAT(R[n]);R[n]+=4;"},
  {"","n","lds.l @<REG_N>+,PR", "0100nnnn00100110", "MA();PR = RLAT(R[n]);R[n]+=4;;"},
  {"","","ldtlb", "0000000000111000", "/*XXX*/ abort();"},
  {"","n","mac.w @<REG_M>+,@<REG_N>+", "0100nnnnmmmm1111", "macw(R0,memory,n,m);"},
  {"n","","mov #<imm>,<REG_N>", "1110nnnni8*1....", "R[n] = SEXT(i);"},
  {"n","m","mov <REG_M>,<REG_N>", "0110nnnnmmmm0011", "R[n] = R[m];"},
  {"","mn0","mov.b <REG_M>,@(R0,<REG_N>)", "0000nnnnmmmm0100", "MA();WBAT(R[n]+R0, R[m]);"},
  {"","nm","mov.b <REG_M>,@-<REG_N>", "0010nnnnmmmm0100", "MA();R[n]--; WBAT(R[n],R[m]);"},
  {"","mn","mov.b <REG_M>,@<REG_N>", "0010nnnnmmmm0000", "MA();WBAT(R[n], R[m]);"},
  {"0","m","mov.b @(<disp>,<REG_M>),R0", "10000100mmmmi4*1", "MA();R0=RSBAT(i+R[m]);L(0);"},
  {"0","","mov.b @(<disp>,GBR),R0", "11000100i8*1....", "MA();R0=RSBAT(i+GBR);L(0);"},
  {"n","0m","mov.b @(R0,<REG_M>),<REG_N>", "0000nnnnmmmm1100", "MA();R[n]=RSBAT(R0+R[m]);L(n);"},
  {"n","m","mov.b @<REG_M>+,<REG_N>", "0110nnnnmmmm0100", "MA();R[n] = RSBAT(R[m]);L(n);R[m]++;"},
  {"n","m","mov.b @<REG_M>,<REG_N>", "0110nnnnmmmm0000", "MA();R[n]=RSBAT(R[m]);L(n);"},
  {"","m0","mov.b R0,@(<disp>,<REG_M>)", "10000000mmmmi4*1", "MA();WBAT(i+R[m],R0);"},
  {"","0","mov.b R0,@(<disp>,GBR)", "11000000i8*1....", "MA();WBAT(i+GBR,R0);"},
  {"","nm","mov.l <REG_M>,@(<disp>,<REG_N>)", "0001nnnnmmmmi4*4",   "MA();WLAT(i+R[n],R[m]);"},
  {"","nm0","mov.l <REG_M>,@(R0,<REG_N>)", "0000nnnnmmmm0110", "MA();WLAT(R0+R[n],R[m]);"},
  {"","nm","mov.l <REG_M>,@-<REG_N>", "0010nnnnmmmm0110", "MA();R[n]-=4;WLAT(R[n],R[m]);"},
  {"","nm","mov.l <REG_M>,@<REG_N>", "0010nnnnmmmm0010", "MA();WLAT(R[n], R[m]);"},
  {"n","m","mov.l @(<disp>,<REG_M>),<REG_N>","0101nnnnmmmmi4*4", "MA();R[n]=RLAT(i+R[m]);L(n);"},
  {"0","","mov.l @(<disp>,GBR),R0", "11000110i8*4....", "MA();R0=RLAT(i+GBR);L(0);"},
  {"n","","mov.l @(<disp>,PC),<REG_N>", "1101nnnni8p4....", "MA();R[n]=RLAT((i+4+PC) & ~3);L(n);"},
  {"n","m","mov.l @(R0,<REG_M>),<REG_N>", "0000nnnnmmmm1110", "MA();R[n]=RLAT(R0+R[m]);L(n);"},
{"nm","m","mov.l @<REG_M>+,<REG_N>", "0110nnnnmmmm0110", "MA();R[n]=RLAT(R[m]);R[m]+=4;L(n);"},
  {"n","m","mov.l @<REG_M>,<REG_N>", "0110nnnnmmmm0010", "MA();R[n]=RLAT(R[m]);L(n);"},
  {"","0","mov.l R0,@(<disp>,GBR)", "11000010i8*4....", "MA();WLAT(i+GBR,R0);"},
  {"","m0n","mov.w <REG_M>,@(R0,<REG_N>)", "0000nnnnmmmm0101", "MA();WWAT(R0+R[n],R[m]);"},
{"n","mn","mov.w <REG_M>,@-<REG_N>", "0010nnnnmmmm0101", "MA();R[n]-=2;WWAT(R[n],R[m]);"},
  {"","nm","mov.w <REG_M>,@<REG_N>", "0010nnnnmmmm0001", "MA();WWAT(R[n],R[m]);"},
  {"0","m","mov.w @(<disp>,<REG_M>),R0", "10000101mmmmi4*2", "MA();R0=RSWAT(i+R[m]);L(0);"},
  {"0","","mov.w @(<disp>,GBR),R0", "11000101i8*2....", "MA();R0=RSWAT(i+GBR);L(0);"},
  {"n","","mov.w @(<disp>,PC),<REG_N>", "1001nnnni8p2....", "MA();R[n]=RSWAT(PC+i+4);L(n);"},
{"n","m0","mov.w @(R0,<REG_M>),<REG_N>", "0000nnnnmmmm1101", "MA();R[n]=RSWAT(R0+R[m]);L(n);"},
{"nm","n","mov.w @<REG_M>+,<REG_N>", "0110nnnnmmmm0101", "MA();R[n]=RSWAT(R[m]);R[m]+=2;L(n);"},
  {"n","m","mov.w @<REG_M>,<REG_N>", "0110nnnnmmmm0001", "MA();R[n]=RSWAT(R[m]);L(n);"},
  {"","0m","mov.w R0,@(<disp>,<REG_M>)", "10000001mmmmi4*2",   "MA();WWAT(i+R[m],R0);"},
  {"","0","mov.w R0,@(<disp>,GBR)", "11000001i8*2....", "MA();WWAT(i+GBR,R0);"},
  {"0","","mova @(<disp>,PC),R0", "11000111i8p4....", "R0=((i+4+PC) & ~0x3);"},
  {"n","","movt <REG_N>", "0000nnnn00101001", "R[n]=T;"},
  {"","mn","muls <REG_M>,<REG_N>", "0010nnnnmmmm1111","MACL=((int)(short)R[n])*((int)(short)R[m]);"},
  {"","mn","mul.l <REG_M>,<REG_N>","0000nnnnmmmm0111","MACL=((int)R[n])*((int)R[m]);"},
  {"","mn","mulu <REG_M>,<REG_N>", "0010nnnnmmmm1110","MACL=((unsigned int)(unsigned short)R[n])*((unsigned int)(unsigned short)R[m]);"},
  {"n","m","neg <REG_M>,<REG_N>", "0110nnnnmmmm1011", "R[n] = - R[m];"},
  {"n","m","negc <REG_M>,<REG_N>", "0110nnnnmmmm1010", "ult=-T;T=ult>0;R[n]=ult-R[m];T|=R[n]>ult;"},
  {"","","nop", "0000000000001001", ""},
  {"n","m","not <REG_M>,<REG_N>", "0110nnnnmmmm0111", "R[n]=~R[m];"},
  {"0","","or #<imm>,R0", "11001011i8*1....", "R0|=i;"},
  {"n","m","or <REG_M>,<REG_N>", "0010nnnnmmmm1011", "R[n]|=R[m];"},
  {"","0","or.b #<imm>,@(R0,GBR)", "11001111i8*1....", "MA();WBAT(R0+GBR,RBAT(R0+GBR)|i);"},
{"n","n","rotcl <REG_N>", "0100nnnn00100100", "ult=R[n]<0;R[n]=(R[n]<<1)|T;T=ult;"},
  {"n","n","rotcr <REG_N>", "0100nnnn00100101", "ult=R[n]&1;R[n]=(UR[n]>>1)|(T<<31);T=ult;"},
  {"n","n","rotl <REG_N>", "0100nnnn00000100", "T=R[n]<0;R[n]<<=1;R[n]|=T;"},
  {"n","n","rotr <REG_N>", "0100nnnn00000101", "T=R[n]&1;R[n] = UR[n]>> 1;R[n]|=(T<<31);"},
  {"","","rte", "0000000000101011", 
    "{ int tmp = PC; PC=RLAT(R[15])+2;R[15]+=4;SET_SR(RLAT(R[15]) & 0x3f3);R[15]+=4;SL(tmp+2);}"},
  {"","","rts", "0000000000001011", "ult=PC;PC=PR-2;SL(ult+2);"},
  {"","","sets", "0000000001011000", "S=1;"},
  {"","","sett", "0000000000011000", "T=1;"},
  {"n","mn","shad <REG_M>,<REG_N>", "0100nnnnmmmm1100",
    "R[n] = (R[m] < 0) ? (R[n] >> ((-R[m])&0x1f)) : (R[n] << (R[m] & 0x1f));"},
  {"n","mn","shld <REG_M>,<REG_N>", "0100nnnnmmmm1101",
    "R[n] = (R[m] < 0) ? (UR[n] >> ((-R[m])&0x1f)): (R[n] << (R[m] & 0x1f));"},
  {"n","n","shal <REG_N>", "0100nnnn00100000", "T=R[n]<0; R[n]<<=1;"},
  {"n","n","shar <REG_N>", "0100nnnn00100001", "T=R[n]&1; R[n] = R[n] >> 1;"},
  {"n","n","shll <REG_N>", "0100nnnn00000000", "T=R[n]<0; R[n]<<=1;"},
  {"n","n","shll16 <REG_N>", "0100nnnn00101000", "R[n]<<=16;"},
  {"n","n","shll2 <REG_N>", "0100nnnn00001000", "R[n]<<=2;"},
  {"n","n","shll8 <REG_N>", "0100nnnn00011000", "R[n]<<=8;"},
  {"n","n","shlr <REG_N>", "0100nnnn00000001", "T=R[n]&1;R[n]=UR[n]>>1;"},
  {"n","n","shlr16 <REG_N>", "0100nnnn00101001", "R[n]=UR[n]>>16;"},
  {"n","n","shlr2 <REG_N>", "0100nnnn00001001", "R[n]=UR[n]>>2;"},
  {"n","n","shlr8 <REG_N>", "0100nnnn00011001", "R[n]=UR[n]>>8;"},
  {"","","sleep", "0000000000011011", "trap(0xc3,R0,memory,maskl,maskw,little_endian);PC-=2;"},
  {"n","","stc SR,<REG_N>",  "0000nnnn00000010", "R[n]=GET_SR();"},
  {"n","","stc GBR,<REG_N>", "0000nnnn00010010", "R[n]=GBR;"},
  {"n","","stc VBR,<REG_N>", "0000nnnn00100010", "R[n]=VBR;"},
  {"n","","stc SSR,<REG_N>", "0000nnnn00110010", "R[n]=SSR;"},
  {"n","","stc SPC,<REG_N>", "0000nnnn01000010", "R[n]=SPC;"},
  {"n","n","stc.l SR,@-<REG_N>",  "0100nnnn00000011", "MA();R[n]-=4;WLAT(R[n],GET_SR());"},
  {"n","n","stc.l GBR,@-<REG_N>", "0100nnnn00010011", "MA();R[n]-=4;WLAT(R[n],GBR);;"},
  {"n","n","stc.l VBR,@-<REG_N>", "0100nnnn00100011", "MA();R[n]-=4;WLAT(R[n],VBR);"},
  {"n","n","stc.l SSR,@-<REG_N>", "0100nnnn00110011", "MA();R[n]-=4;WLAT(R[n],SSR);"},
  {"n","n","stc.l SPC,@-<REG_N>", "0100nnnn01000011", "MA();R[n]-=4;WLAT(R[n],SPC);"},
  {"n","","sts MACH,<REG_N>", "0000nnnn00001010", "R[n]=MACH;"},
  {"n","","sts MACL,<REG_N>", "0000nnnn00011010", "R[n]=MACL;"},
  {"n","","sts PR,<REG_N>", "0000nnnn00101010", "R[n]=PR;"},
  {"n","n","sts.l MACH,@-<REG_N>", "0100nnnn00000010", "MA();R[n]-=4;WLAT(R[n],MACH);"},
  {"n","n","sts.l MACL,@-<REG_N>", "0100nnnn00010010", "MA();R[n]-=4;WLAT(R[n],MACL);"},
  {"n","n","sts.l PR,@-<REG_N>", "0100nnnn00100010", "MA();R[n]-=4;WLAT(R[n],PR);"},
  {"n","nm","sub <REG_M>,<REG_N>", "0011nnnnmmmm1000", "R[n]-=R[m];"},
{"n","nm","subc <REG_M>,<REG_N>", "0011nnnnmmmm1010", "ult=R[n]-T;T=ult>R[n];R[n]=ult-R[m];T|=R[n]>ult;"},
  {"n","nm","subv <REG_M>,<REG_N>", "0011nnnnmmmm1011",
     "ult = R[n] - R[m]; T = (((R[n] ^ R[m]) & (ult ^ R[n])) >> 31); R[n] = ult;"},
  {"n","nm","swap.b <REG_M>,<REG_N>", "0110nnnnmmmm1000", "R[n]=((R[m]<<8)&0xff00)|((R[m]>>8)&0x00ff);"},
  {"n","nm","swap.w <REG_M>,<REG_N>", "0110nnnnmmmm1001", "R[n]=((R[m]<<16)&0xffff0000)|((R[m]>>16)&0x00ffff);"},
  {"","n","tas.b @<REG_N>", "0100nnnn00011011", "MA();ult=RBAT(R[n]);T=ult==0;WBAT(R[n],ult|0x80);"},
  {"0","","trapa #<imm>", "11000011i8*1....", 
     "{ long imm = 0xff & i; if (i==0xc3) PC-=2; if (i<20||i==34||i==0xc3) trap(i,R,memory,maskl,maskw,little_endian); else { R[15]-=4; WLAT(R[15],GET_SR()); R[15]-=4;WLAT(R[15],PC+2); PC=RLAT(VBR+(imm<<2))-2;}}"},
  {"","0","tst #<imm>,R0", "11001000i8*1....", "T=(R0&i)==0;"},
  {"","mn","tst <REG_M>,<REG_N>", "0010nnnnmmmm1000", "T=(R[n]&R[m])==0;"},
  {"","0","tst.b #<imm>,@(R0,GBR)", "11001100i8*1....", "MA();T=(RBAT(GBR+R0)&i)==0;"},
  {"","0","xor #<imm>,R0", "11001010i8*1....", "R0^=i;"},
  {"n","mn","xor <REG_M>,<REG_N>", "0010nnnnmmmm1010", "R[n]^=R[m];"},
  {"","0","xor.b #<imm>,@(R0,GBR)", "11001110i8*1....", "MA();ult=RBAT(GBR+R0);ult^=i;WBAT(GBR+R0,ult);"},
  {"n","nm","xtrct <REG_M>,<REG_N>", "0010nnnnmmmm1101", "R[n]=((R[n]>>16)&0xffff)|((R[m]<<16)&0xffff0000);"},
  {"","nm","mul.l <REG_M>,<REG_N>", "0000nnnnmmmm0111", " MACL = R[n] * R[m];"},
  {"n","n","dt <REG_N>", "0100nnnn00010000", "R[n]--; T=R[n] == 0;"},
  {"","nm","dmuls.l <REG_M>,<REG_N>", "0011nnnnmmmm1101", "dmul(1,R[n],R[m]);"},
  {"","nm","dmulu.l <REG_M>,<REG_N>", "0011nnnnmmmm0101", "dmul(0,R[n],R[m]);"},
  {"","nm","mac.l @<REG_M>+,@<REG_N>+", "0000nnnnmmmm1111", "trap(255,R0,memory,maskl,maskw,little_endian);"},
  {"","n","braf <REG_N>", "0000nnnn00100011", "ult = PC; PC+=R[n]-2;SL(ult+2);"},
  {"","n","bsrf <REG_N>", "0000nnnn00000011", "PR = PC + 4; PC+=R[n]-2;SL(PR-2);"},
#if 0
  {"divs.l <REG_M>,<REG_N>", "0100nnnnmmmm1110", "divl(0,R[n],R[m]);"},
  {"divu.l <REG_M>,<REG_N>", "0100nnnnmmmm1101", "divl(0,R[n],R[m]);"},
#endif

  {"", "", "fmov.s @<REG_M>,<FREG_N>", "1111nnnnmmmm1000", "MA();*(int *)buf = RLAT(R[m]);F[n] = *(float *)buf;"},
  {"", "", "fmov.s <FREG_M>,@<REG_N>", "1111nnnnmmmm1010", "MA();*(float *)buf = F[m]; WLAT(R[n], *(int *)buf);"},
  {"", "", "fmov.s @<REG_M>+,<FREG_N>", "1111nnnnmmmm1001", "MA();*(int *)buf = RLAT(R[m]); F[n] = *(float *)buf; R[m] += 4;"},
  {"", "", "fmov.s <FREG_M>,@-<REG_N>", "1111nnnnmmmm1011", "MA();R[n] -= 4; *(float *)buf = F[m]; WLAT(R[n], *(int *)buf);"},
  {"", "", "fmov.s @(R0,<REG_M>),<FREG_N>", "1111nnnnmmmm0110", "MA();*(int *)buf = RLAT((R[0]+R[m]));F[n] = *(float *)buf;"},
  {"", "", "fmov.s <FREG_M>,@(R0,<REG_N>)", "1111nnnnmmmm0111", "MA();*(float *)buf = F[m]; WLAT((R[0]+R[n]), *(int *)buf);"},
  {"", "", "fmov <FREG_M>,<FREG_N>", "1111nnnnmmmm1100", "F[n] = F[m];"},
  {"", "", "fldi0 <FREG_N>", "1111nnnn10001101", "F[n] = (float)0.0;"},
  {"", "", "fldi1 <FREG_N>", "1111nnnn10011101", "F[n] = (float)1.0;"},
  {"", "", "fadd <FREG_M>,<FREG_N>", "1111nnnnmmmm0000","F[n] = F[n] + F[m];"},
  {"", "", "fsub <FREG_M>,<FREG_N>", "1111nnnnmmmm0001","F[n] = F[n] - F[m];"},
  {"", "", "fmul <FREG_M>,<FREG_N>", "1111nnnnmmmm0010","F[n] = F[n] * F[m];"},
  {"", "", "fdiv <FREG_M>,<FREG_N>", "1111nnnnmmmm0011","F[n] = F[n] / F[m];"},
  {"", "", "fmac <FREG_0>,<FREG_M>,<FREG_N>", "1111nnnnmmmm1110", "F[n] = F[m] * F[0] + F[n];"},
  {"", "", "fcmp/eq <FREG_M>,<FREG_N>", "1111nnnnmmmm0100", "T = F[n] == F[m] ? 1 : 0;"},
  {"", "", "fcmp/gt <FREG_M>,<FREG_N>", "1111nnnnmmmm0101", "T = F[n] > F[m] ? 1 : 0;"},
  {"", "", "fneg <FREG_N>", "1111nnnn01001101", "F[n] = -F[n];"},
  {"", "", "fabs <FREG_N>", "1111nnnn01011101", "F[n] = fabs (F[n]);"},
  {"", "", "fsqrt <FREG_N>", "1111nnnn01101101", "F[n] = sqrt (F[n]);"},
  {"", "", "float FPUL,<FREG_N>", "1111nnnn00101101", "F[n] = (float)FPUL;"},
  {"", "", "ftrc <FREG_N>, FPUL", "1111nnnn00111101", "if (F[n] != F[n]) /* NaN */ FPUL = 0x80000000; else FPUL = (int)F[n];"},
  {"", "", "ftst/nan <FREG_N>", "1111nnnn01111101", "T = isnan (F[n]);"},
  {"", "", "fsts FPUL,<FREG_N>", "1111nnnn00001101", "*(int *)buf = FPUL; F[n] = *(float *)buf;"},
  {"", "", "flds <FREG_N>,FPUL", "1111nnnn00011101", "*(float *)buf = F[n]; FPUL = *(int *)buf;"},
  {"", "", "lds <REG_N>,FPUL", "0100nnnn01011010", "FPUL = R[n];"},
  {"", "", "sts FPUL,<REG_N>", "0000nnnn01011010", "R[n] = FPUL;"},
  {"", "", "lds <REG_N>,FPSCR", "0100nnnn01101010", "*(int *)buf = R[n]; FPSCR = *(float *)buf;"},
  {"", "", "sts FPSCR,<REG_N>", "0000nnnn01101010", "*(float *)buf = FPSCR; R[n] = *(int *)buf;"},
  {"","","lds.l @<REG_N>+,FPUL", "0100nnnn01010110", "MA();FPUL = RLAT(R[n]);R[n]+=4;"},
  {"","","lds.l @<REG_N>+,FPSCR", "0100nnnn01100110", "MA();*(int *)buf = RLAT(R[n]); FPSCR = *(float *)buf; R[n]+=4;"},
  {"","","sts.l FPUL,@-<REG_N>", "0100nnnn01010010", "MA();R[n]-=4;WLAT(R[n],FPUL);"},
  {"","","sts.l FPSCR,@-<REG_N>", "0100nnnn01100010", "MA();R[n]-=4;*(float *)buf = FPSCR; WLAT(R[n],*(int *)buf);"},

  {0, 0}};

/* Tables of things to put into enums for sh-opc.h */
static char *nibble_type_list[] =
{
  "HEX_0",
  "HEX_1",
  "HEX_2",
  "HEX_3",
  "HEX_4",
  "HEX_5",
  "HEX_6",
  "HEX_7",
  "HEX_8",
  "HEX_9",
  "HEX_A",
  "HEX_B",
  "HEX_C",
  "HEX_D",
  "HEX_E",
  "HEX_F",
  "REG_N",
  "REG_M",
  "BRANCH_12",
  "BRANCH_8",
  "DISP_8",
  "DISP_4",
  "IMM_4",
  "IMM_4BY2",
  "IMM_4BY4",
  "PCRELIMM_8BY2",
  "PCRELIMM_8BY4",
  "IMM_8",
  "IMM_8BY2",
  "IMM_8BY4",
  0
};
static
char *arg_type_list[] =
{
  "A_END",
  "A_BDISP12",
  "A_BDISP8",
  "A_DEC_M",
  "A_DEC_N",
  "A_DISP_GBR",
  "A_DISP_PC",
  "A_DISP_REG_M",
  "A_DISP_REG_N",
  "A_GBR",
  "A_IMM",
  "A_INC_M",
  "A_INC_N",
  "A_IND_M",
  "A_IND_N",
  "A_IND_R0_REG_M",
  "A_IND_R0_REG_N",
  "A_MACH",
  "A_MACL",
  "A_PR",
  "A_R0",
  "A_R0_GBR",
  "A_REG_M",
  "A_REG_N",
  "A_SR",
  "A_VBR",
  "A_SSR",
  "A_SPC",
  0,
};

static void
make_enum_list (name, s)
     char *name;
     char **s;
{
  int i = 1;
  printf ("typedef enum {\n");
  while (*s)
    {
      printf ("\t%s,\n", *s);
      s++;
      i++;
    }
  printf ("} %s;\n", name);
}

static int
qfunc (a, b)
     op *a;
     op *b;
{
  char bufa[9];
  char bufb[9];
  memcpy (bufa, a->code, 4);
  memcpy (bufa + 4, a->code + 12, 4);
  bufa[8] = 0;

  memcpy (bufb, b->code, 4);
  memcpy (bufb + 4, b->code + 12, 4);
  bufb[8] = 0;
  return (strcmp (bufa, bufb));
}

static void
sorttab ()
{
  op *p = tab;
  int len = 0;

  while (p->name)
    {
      p++;
      len++;
    }
  qsort (tab, len, sizeof (*p), qfunc);
}

static void
printonmatch (ptr, a, rep)
     char **ptr;
     char *a;
     char *rep;
{
  int l = strlen (a);
  if (strncmp (*ptr, a, l) == 0)
    {
      printf ("%s", rep);
      *ptr += l;
      if (**ptr)
	printf (",");
    }
}


static 
void
think (o)
     op *o;
{
  char *n;
  char *p;

  printf ("{\"");
  n = o->name;
  while (*n && *n != ' ')
    {
      printf ("%c", *n);
      n++;
    }
  printf ("\",{");

  p = n;

  if (!*p)
    {
      printf ("0");
    }
  while (*p)
    {
      while (*p == ',' || *p == ' ')
	p++;
      printonmatch (&p, "#<imm>", "A_IMM");
      printonmatch (&p, "R0", "A_R0");
      printonmatch (&p, "<REG_N>", "A_REG_N");
      printonmatch (&p, "@<REG_N>+", "A_INC_N");
      printonmatch (&p, "@<REG_N>", "A_IND_N");
      printonmatch (&p, "@-<REG_N>", "A_DEC_N");
      printonmatch (&p, "<REG_M>", " A_REG_M");
      printonmatch (&p, "@<REG_M>+", "A_INC_M");
      printonmatch (&p, "@<REG_M>", "A_IND_M");
      printonmatch (&p, "@-<REG_M>", "A_DEC_M");
      printonmatch (&p, "@(<disp>,PC)", "A_DISP_PC");
      printonmatch (&p, "@(<disp>,<REG_M>)", "A_DISP_REG_M");
      printonmatch (&p, "@(<disp>,<REG_N>)", "A_DISP_REG_N");
      printonmatch (&p, "@(R0,<REG_N>)", "A_IND_R0_REG_N");
      printonmatch (&p, "@(R0,<REG_M>)", "A_IND_R0_REG_M");
      printonmatch (&p, "@(<disp>,GBR)", "A_DISP_GBR");
      printonmatch (&p, "@(R0,GBR)", "A_R0_GBR");
      printonmatch (&p, "<bdisp8>", "A_BDISP8");
      printonmatch (&p, "<bdisp12>", "A_BDISP12");
      printonmatch (&p, "SR", "A_SR");
      printonmatch (&p, "GBR", "A_GBR");
      printonmatch (&p, "VBR", "A_VBR");
      printonmatch (&p, "SSR", "A_SSR");
      printonmatch (&p, "SPC", "A_SPC");
      printonmatch (&p, "MACH", "A_MACH");
      printonmatch (&p, "MACL", "A_MACL");
      printonmatch (&p, "PR", "A_PR");

    }
  printf ("},{");

  p = o->code;
  while (*p)
    {
      printonmatch (&p, "0000", "HEX_0");
      printonmatch (&p, "0001", "HEX_1");
      printonmatch (&p, "0010", "HEX_2");
      printonmatch (&p, "0011", "HEX_3");
      printonmatch (&p, "0100", "HEX_4");
      printonmatch (&p, "0101", "HEX_5");
      printonmatch (&p, "0110", "HEX_6");
      printonmatch (&p, "0111", "HEX_7");

      printonmatch (&p, "1000", "HEX_8");
      printonmatch (&p, "1001", "HEX_9");
      printonmatch (&p, "1010", "HEX_A");
      printonmatch (&p, "1011", "HEX_B");
      printonmatch (&p, "1100", "HEX_C");
      printonmatch (&p, "1101", "HEX_D");
      printonmatch (&p, "1110", "HEX_E");
      printonmatch (&p, "1111", "HEX_F");
      printonmatch (&p, "i8*1....", "IMM_8");
      printonmatch (&p, "i4*1", "IMM_4");
      printonmatch (&p, "i8p4....", "PCRELIMM_8BY4");
      printonmatch (&p, "i8p2....", "PCRELIMM_8BY2");
      printonmatch (&p, "i8*2....", "IMM_8BY2");
      printonmatch (&p, "i4*2", "IMM_4BY2");
      printonmatch (&p, "i8*4....", "IMM_8BY4");
      printonmatch (&p, "i4*4", "IMM_4BY4");
      printonmatch (&p, "i12.........", "BRANCH_12");
      printonmatch (&p, "i8p1....", "BRANCH_8");
      printonmatch (&p, "nnnn", "REG_N");
      printonmatch (&p, "mmmm", "REG_M");

    }
  printf ("}},\n");
}

static void
gengastab ()
{
  op *p;
  sorttab ();
  for (p = tab; p->name; p++)
    {
      printf ("%s %-30s\n", p->code, p->name);
    }


}


static void
genopc ()
{
  op *p;
  make_enum_list ("sh_nibble_type", nibble_type_list);
  make_enum_list ("sh_arg_type", arg_type_list);

  printf ("typedef struct {\n");
  printf ("char *name;\n");
  printf ("sh_arg_type arg[3];\n");
  printf ("sh_nibble_type nibbles[4];\n");
  printf ("} sh_opcode_info;\n");
  printf ("#ifdef DEFINE_TABLE\n");
  printf ("sh_opcode_info sh_table[]={\n");
  for (p = tab; p->name; p++)
    {
      printf ("\n/* %s %-20s*/", p->code, p->name);
      think (p);
    }
  printf ("0};\n");
  printf ("#endif\n");
}






/* Convert a string of 4 binary digits into an int */

static
int
bton (s)
     char *s;

{
  int n = 0;
  int v = 8;
  while (v)
    {
      if (*s == '1')
	n |= v;
      v >>= 1;
      s++;
    }
  return n;
}

static unsigned char table[1 << 16];

/* Take an opcode expand all varying fields in it out and fill all the
  right entries in 'table' with the opcode index*/

static void
expand_opcode (shift, val, i, s)
     int shift;
     int val;
     int i;
     char *s;
{
  int j;

  if (*s == 0)
    {
      table[val] = i;
    }
  else
    {
      switch (s[0])
	{

	case '0':
	case '1':
	  {

	    int n = bton (s);
	    if (n >= 0)
	      {
		expand_opcode (shift - 4, val | (n << shift), i, s + 4);
	      }
	    break;
	  }
	case 'n':
	case 'm':
	  for (j = 0; j < 16; j++)
	    {
	      expand_opcode (shift - 4, val | (j << shift), i, s + 4);

	    }
	  break;

	default:
	  for (j = 0; j < (1 << (shift + 4)); j++)
	    {
	      table[val | j] = i;
	    }
	}
    }
}

/* Print the jump table used to index an opcode into a switch
   statement entry. */

static void
dumptable ()
{
  int lump = 256;
  int online = 16;

  int i = 0;

  while (i < 1 << 16)
    {
      int j = 0;

      printf ("unsigned char sh_jump_table%x[%d]={\n", i, lump);

      while (j < lump)
	{
	  int k = 0;
	  while (k < online)
	    {
	      printf ("%2d", table[i + j + k]);
	      if (j + k < lump)
		printf (",");

	      k++;
	    }
	  j += k;
	  printf ("\n");
	}
      i += j;
      printf ("};\n");
    }

}


static void
filltable ()
{
  op *p;
  int index = 1;

  sorttab ();
  for (p = tab; p->name; p++)
    {
      p->index = index++;
      expand_opcode (12, 0, p->index, p->code);
    }
}

static void
gensim ()
{
  op *p;
  int j;

  printf ("{\n");
  printf("char buf[4];\n");
  printf ("switch (jump_table[iword]) {\n");

  for (p = tab; p->name; p++)
    {
      int sextbit = -1;
      int needm = 0;
      int needn = 0;
      
      char *s = p->code;

      printf ("/* %s %s */\n", p->name, p->code);
      printf ("case %d:      \n", p->index);

      printf ("{\n");
      while (*s)
	{
	  switch (*s)
	    {
	    case '0':
	    case '1':
	    case '.':
	      s += 4;
	      break;
	    case 'n':
	      printf ("int n =  (iword >>8) & 0xf;\n");
	      needn = 1;
	      s += 4;
	      break;
	    case 'm':
	      printf ("int m =  (iword >>4) & 0xf;\n");
	      needm = 1;
	      s += 4;

	      break;

	    case 'i':
	      printf ("int i = (iword & 0x");

	      switch (s[1])
		{
		case '4':
		  printf ("f");
		  break;
		case '8':
		  printf ("ff");
		  break;
		case '1':
		  sextbit = 12;

		  printf ("fff");
		  break;
		}
	      printf (")");

	      switch (s[3])
		{
		case '1':
		  break;
		case '2':
		  printf ("<<1");
		  break;
		case '4':
		  printf ("<<2");
		  break;
		}
	      printf (";\n");
	      s += 4;
	    }
	}
      if (sextbit > 0)
	{
	  printf ("i = (i ^ (1<<%d))-(1<<%d);\n", sextbit - 1, sextbit - 1);
	}
      if (needm && needn)
	printf("TB(m,n);");  
      else if (needm)
	printf("TL(m);");
      else if (needn)
	printf("TL(n);");
      for (j = 0; j < 10; j++)
	{
	  if (p->stuff[j])
	    {
	      printf ("%s\n", p->stuff[j]);
	    }
	}

      
      {
	/* Do the defs and refs */
	char *r;
	for (r = p->refs; *r; r++) {
	 if (*r == '0') printf("CREF(0);\n"); 
	 if (*r == 'n') printf("CREF(n);\n"); 
	 if (*r == 'm') printf("CREF(m);\n"); 
	 
	}
	for (r = p->defs; *r; r++) 
	  {
	 if (*r == '0') printf("CDEF(0);\n"); 
	 if (*r == 'n') printf("CDEF(n);\n"); 
	 if (*r == 'm') printf("CDEF(m);\n"); 
	 
	}

      }
      printf ("break;\n");
      printf ("}\n");
    }
  printf("default:\n{\nsaved_state.asregs.exception = SIGILL;\n}\n");
  printf ("}\n}\n");
}


static void
gendefines ()
{
  op *p;
  filltable();
  for (p = tab; p->name; p++)
    {
      char *s = p->name;
      printf ("#define OPC_");
      while (*s) {
	if (isupper(*s)) 
	  *s = tolower(*s);
	if (isalpha(*s)) printf("%c", *s);
	if (*s == ' ') printf("_");
	if (*s == '@') printf("ind_");
	if (*s == ',') printf("_");
	s++;
      }
      printf(" %d\n",p->index);
    }
}

int
main (ac, av)
     int ac;
     char **av;
{
  if (ac > 1)
    {
      if (strcmp (av[1], "-t") == 0)
	{
	  gengastab ();
	}
      else if (strcmp (av[1], "-d") == 0)
	{
	  gendefines ();
	}
      else if (strcmp (av[1], "-s") == 0)
	{
	  filltable ();
	  dumptable ();

	}
      else if (strcmp (av[1], "-x") == 0)
	{
	  filltable ();
	  gensim ();
	}
    }
  else
    {
      genopc ();
    }
  return 0;
}
