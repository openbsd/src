{
char buf[4];
switch (jump_table[iword]) {
/* stc SPC,<REG_N> 0000nnnn01000010 */
case 1:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=SPC;
CDEF(n);
break;
}
/* stc SSR,<REG_N> 0000nnnn00110010 */
case 2:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=SSR;
CDEF(n);
break;
}
/* stc VBR,<REG_N> 0000nnnn00100010 */
case 3:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=VBR;
CDEF(n);
break;
}
/* stc GBR,<REG_N> 0000nnnn00010010 */
case 4:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=GBR;
CDEF(n);
break;
}
/* stc SR,<REG_N> 0000nnnn00000010 */
case 5:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=GET_SR();
CDEF(n);
break;
}
/* braf <REG_N> 0000nnnn00100011 */
case 6:      
{
int n =  (iword >>8) & 0xf;
TL(n);ult = PC; PC+=R[n]-2;SL(ult+2);
CREF(n);
break;
}
/* bsrf <REG_N> 0000nnnn00000011 */
case 7:      
{
int n =  (iword >>8) & 0xf;
TL(n);PR = PC + 4; PC+=R[n]-2;SL(PR-2);
CREF(n);
break;
}
/* mov.b <REG_M>,@(R0,<REG_N>) 0000nnnnmmmm0100 */
case 8:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();WBAT(R[n]+R0, R[m]);
CREF(m);
CREF(n);
CREF(0);
break;
}
/* mov.w <REG_M>,@(R0,<REG_N>) 0000nnnnmmmm0101 */
case 9:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();WWAT(R0+R[n],R[m]);
CREF(m);
CREF(0);
CREF(n);
break;
}
/* mov.l <REG_M>,@(R0,<REG_N>) 0000nnnnmmmm0110 */
case 10:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();WLAT(R0+R[n],R[m]);
CREF(n);
CREF(m);
CREF(0);
break;
}
/* mul.l <REG_M>,<REG_N> 0000nnnnmmmm0111 */
case 11:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n); MACL = R[n] * R[m];
CREF(n);
CREF(m);
break;
}
/* mul.l <REG_M>,<REG_N> 0000nnnnmmmm0111 */
case 12:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MACL=((int)R[n])*((int)R[m]);
CREF(m);
CREF(n);
break;
}
/* sets 0000000001011000 */
case 13:      
{
S=1;
break;
}
/* clrmac 0000000000101000 */
case 14:      
{
MACH = MACL = 0;
break;
}
/* clrs 0000000001001000 */
case 15:      
{
S= 0;
break;
}
/* clrt 0000000000001000 */
case 16:      
{
T= 0;
break;
}
/* sett 0000000000011000 */
case 17:      
{
T=1;
break;
}
/* ldtlb 0000000000111000 */
case 18:      
{
/*XXX*/ abort();
break;
}
/* nop 0000000000001001 */
case 19:      
{

break;
}
/* movt <REG_N> 0000nnnn00101001 */
case 20:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=T;
CDEF(n);
break;
}
/* div0u 0000000000011001 */
case 21:      
{
M=Q=T=0;
break;
}
/* sts MACL,<REG_N> 0000nnnn00011010 */
case 22:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=MACL;
CDEF(n);
break;
}
/* sts PR,<REG_N> 0000nnnn00101010 */
case 23:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=PR;
CDEF(n);
break;
}
/* sts FPUL,<REG_N> 0000nnnn01011010 */
case 24:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n] = FPUL;
break;
}
/* sts MACH,<REG_N> 0000nnnn00001010 */
case 25:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=MACH;
CDEF(n);
break;
}
/* sts FPSCR,<REG_N> 0000nnnn01101010 */
case 26:      
{
int n =  (iword >>8) & 0xf;
TL(n);*(float *)buf = FPSCR; R[n] = *(int *)buf;
break;
}
/* sleep 0000000000011011 */
case 27:      
{
trap(0xc3,R0,memory,maskl,maskw,little_endian);PC-=2;
break;
}
/* rte 0000000000101011 */
case 28:      
{
{ int tmp = PC; PC=RLAT(R[15])+2;R[15]+=4;SET_SR(RLAT(R[15]) & 0x3f3);R[15]+=4;SL(tmp+2);}
break;
}
/* rts 0000000000001011 */
case 29:      
{
ult=PC;PC=PR-2;SL(ult+2);
break;
}
/* mov.b @(R0,<REG_M>),<REG_N> 0000nnnnmmmm1100 */
case 30:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]=RSBAT(R0+R[m]);L(n);
CREF(0);
CREF(m);
CDEF(n);
break;
}
/* mov.w @(R0,<REG_M>),<REG_N> 0000nnnnmmmm1101 */
case 31:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]=RSWAT(R0+R[m]);L(n);
CREF(m);
CREF(0);
CDEF(n);
break;
}
/* mov.l @(R0,<REG_M>),<REG_N> 0000nnnnmmmm1110 */
case 32:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]=RLAT(R0+R[m]);L(n);
CREF(m);
CDEF(n);
break;
}
/* mac.l @<REG_M>+,@<REG_N>+ 0000nnnnmmmm1111 */
case 33:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);trap(255,R0,memory,maskl,maskw,little_endian);
CREF(n);
CREF(m);
break;
}
/* mov.l <REG_M>,@(<disp>,<REG_N>) 0001nnnnmmmmi4*4 */
case 34:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
int i = (iword & 0xf)<<2;
TB(m,n);MA();WLAT(i+R[n],R[m]);
CREF(n);
CREF(m);
break;
}
/* mov.b <REG_M>,@<REG_N> 0010nnnnmmmm0000 */
case 35:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();WBAT(R[n], R[m]);
CREF(m);
CREF(n);
break;
}
/* mov.w <REG_M>,@<REG_N> 0010nnnnmmmm0001 */
case 36:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();WWAT(R[n],R[m]);
CREF(n);
CREF(m);
break;
}
/* mov.l <REG_M>,@<REG_N> 0010nnnnmmmm0010 */
case 37:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();WLAT(R[n], R[m]);
CREF(n);
CREF(m);
break;
}
/* mov.b <REG_M>,@-<REG_N> 0010nnnnmmmm0100 */
case 38:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]--; WBAT(R[n],R[m]);
CREF(n);
CREF(m);
break;
}
/* mov.w <REG_M>,@-<REG_N> 0010nnnnmmmm0101 */
case 39:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]-=2;WWAT(R[n],R[m]);
CREF(m);
CREF(n);
CDEF(n);
break;
}
/* mov.l <REG_M>,@-<REG_N> 0010nnnnmmmm0110 */
case 40:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]-=4;WLAT(R[n],R[m]);
CREF(n);
CREF(m);
break;
}
/* div0s <REG_M>,<REG_N> 0010nnnnmmmm0111 */
case 41:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);Q=(R[n]&sbit)!=0; M=(R[m]&sbit)!=0; T=M!=Q;;
CREF(m);
CREF(n);
break;
}
/* tst <REG_M>,<REG_N> 0010nnnnmmmm1000 */
case 42:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);T=(R[n]&R[m])==0;
CREF(m);
CREF(n);
break;
}
/* and <REG_M>,<REG_N> 0010nnnnmmmm1001 */
case 43:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n); R[n]&=R[m];
CREF(n);
CREF(m);
CDEF(n);
break;
}
/* xor <REG_M>,<REG_N> 0010nnnnmmmm1010 */
case 44:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n]^=R[m];
CREF(m);
CREF(n);
CDEF(n);
break;
}
/* or <REG_M>,<REG_N> 0010nnnnmmmm1011 */
case 45:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n]|=R[m];
CREF(m);
CDEF(n);
break;
}
/* cmp/str <REG_M>,<REG_N> 0010nnnnmmmm1100 */
case 46:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);ult = R[n] ^ R[m]; T=((ult&0xff000000)==0) |((ult&0xff0000)==0) |((ult&0xff00)==0) |((ult&0xff)==0); 
CREF(m);
CREF(n);
break;
}
/* xtrct <REG_M>,<REG_N> 0010nnnnmmmm1101 */
case 47:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n]=((R[n]>>16)&0xffff)|((R[m]<<16)&0xffff0000);
CREF(n);
CREF(m);
CDEF(n);
break;
}
/* mulu <REG_M>,<REG_N> 0010nnnnmmmm1110 */
case 48:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MACL=((unsigned int)(unsigned short)R[n])*((unsigned int)(unsigned short)R[m]);
CREF(m);
CREF(n);
break;
}
/* muls <REG_M>,<REG_N> 0010nnnnmmmm1111 */
case 49:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MACL=((int)(short)R[n])*((int)(short)R[m]);
CREF(m);
CREF(n);
break;
}
/* cmp/eq <REG_M>,<REG_N> 0011nnnnmmmm0000 */
case 50:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);T=R[n]==R[m];
CREF(m);
CREF(n);
break;
}
/* cmp/hs <REG_M>,<REG_N> 0011nnnnmmmm0010 */
case 51:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);T=UR[n]>=UR[m];
CREF(m);
CREF(n);
break;
}
/* cmp/ge <REG_M>,<REG_N> 0011nnnnmmmm0011 */
case 52:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);T=R[n]>=R[m];
CREF(m);
CREF(n);
break;
}
/* div1 <REG_M>,<REG_N> 0011nnnnmmmm0100 */
case 53:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);T=div1(R,m,n,T);
break;
}
/* dmulu.l <REG_M>,<REG_N> 0011nnnnmmmm0101 */
case 54:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);dmul(0,R[n],R[m]);
CREF(n);
CREF(m);
break;
}
/* cmp/hi <REG_M>,<REG_N> 0011nnnnmmmm0110 */
case 55:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);T=UR[n]>UR[m];
CREF(m);
CREF(n);
break;
}
/* cmp/gt <REG_M>,<REG_N> 0011nnnnmmmm0111 */
case 56:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);T=R[n]>R[m];
CREF(m);
CREF(n);
break;
}
/* sub <REG_M>,<REG_N> 0011nnnnmmmm1000 */
case 57:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n]-=R[m];
CREF(n);
CREF(m);
CDEF(n);
break;
}
/* subc <REG_M>,<REG_N> 0011nnnnmmmm1010 */
case 58:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);ult=R[n]-T;T=ult>R[n];R[n]=ult-R[m];T|=R[n]>ult;
CREF(n);
CREF(m);
CDEF(n);
break;
}
/* subv <REG_M>,<REG_N> 0011nnnnmmmm1011 */
case 59:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);ult = R[n] - R[m]; T = (((R[n] ^ R[m]) & (ult ^ R[n])) >> 31); R[n] = ult;
CREF(n);
CREF(m);
CDEF(n);
break;
}
/* add <REG_M>,<REG_N> 0011nnnnmmmm1100 */
case 60:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n] += R[m];
CREF(m);
CREF(n);
CDEF(n);
break;
}
/* dmuls.l <REG_M>,<REG_N> 0011nnnnmmmm1101 */
case 61:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);dmul(1,R[n],R[m]);
CREF(n);
CREF(m);
break;
}
/* addc <REG_M>,<REG_N> 0011nnnnmmmm1110 */
case 62:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);ult=R[n]+T;T=ult<R[n];R[n]=ult+R[m];T|=R[n]<ult;
CREF(m);
CREF(n);
CDEF(n);
break;
}
/* addv <REG_M>,<REG_N> 0011nnnnmmmm1111 */
case 63:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);ult = R[n] + R[m]; T = ((~(R[n] ^ R[m]) & (ult ^ R[n])) >> 31); R[n] = ult;
CREF(m);
CREF(n);
CDEF(n);
break;
}
/* shal <REG_N> 0100nnnn00100000 */
case 64:      
{
int n =  (iword >>8) & 0xf;
TL(n);T=R[n]<0; R[n]<<=1;
CREF(n);
CDEF(n);
break;
}
/* shll <REG_N> 0100nnnn00000000 */
case 65:      
{
int n =  (iword >>8) & 0xf;
TL(n);T=R[n]<0; R[n]<<=1;
CREF(n);
CDEF(n);
break;
}
/* dt <REG_N> 0100nnnn00010000 */
case 66:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]--; T=R[n] == 0;
CREF(n);
CDEF(n);
break;
}
/* cmp/pz <REG_N> 0100nnnn00010001 */
case 67:      
{
int n =  (iword >>8) & 0xf;
TL(n);T = R[n]>=0;
CREF(n);
break;
}
/* shar <REG_N> 0100nnnn00100001 */
case 68:      
{
int n =  (iword >>8) & 0xf;
TL(n);T=R[n]&1; R[n] = R[n] >> 1;
CREF(n);
CDEF(n);
break;
}
/* shlr <REG_N> 0100nnnn00000001 */
case 69:      
{
int n =  (iword >>8) & 0xf;
TL(n);T=R[n]&1;R[n]=UR[n]>>1;
CREF(n);
CDEF(n);
break;
}
/* sts.l FPUL,@-<REG_N> 0100nnnn01010010 */
case 70:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;WLAT(R[n],FPUL);
break;
}
/* sts.l FPSCR,@-<REG_N> 0100nnnn01100010 */
case 71:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;*(float *)buf = FPSCR; WLAT(R[n],*(int *)buf);
break;
}
/* sts.l PR,@-<REG_N> 0100nnnn00100010 */
case 72:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;WLAT(R[n],PR);
CREF(n);
CDEF(n);
break;
}
/* sts.l MACH,@-<REG_N> 0100nnnn00000010 */
case 73:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;WLAT(R[n],MACH);
CREF(n);
CDEF(n);
break;
}
/* sts.l MACL,@-<REG_N> 0100nnnn00010010 */
case 74:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;WLAT(R[n],MACL);
CREF(n);
CDEF(n);
break;
}
/* stc.l SR,@-<REG_N> 0100nnnn00000011 */
case 75:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;WLAT(R[n],GET_SR());
CREF(n);
CDEF(n);
break;
}
/* stc.l GBR,@-<REG_N> 0100nnnn00010011 */
case 76:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;WLAT(R[n],GBR);;
CREF(n);
CDEF(n);
break;
}
/* stc.l VBR,@-<REG_N> 0100nnnn00100011 */
case 77:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;WLAT(R[n],VBR);
CREF(n);
CDEF(n);
break;
}
/* stc.l SSR,@-<REG_N> 0100nnnn00110011 */
case 78:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;WLAT(R[n],SSR);
CREF(n);
CDEF(n);
break;
}
/* stc.l SPC,@-<REG_N> 0100nnnn01000011 */
case 79:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();R[n]-=4;WLAT(R[n],SPC);
CREF(n);
CDEF(n);
break;
}
/* rotl <REG_N> 0100nnnn00000100 */
case 80:      
{
int n =  (iword >>8) & 0xf;
TL(n);T=R[n]<0;R[n]<<=1;R[n]|=T;
CREF(n);
CDEF(n);
break;
}
/* rotcl <REG_N> 0100nnnn00100100 */
case 81:      
{
int n =  (iword >>8) & 0xf;
TL(n);ult=R[n]<0;R[n]=(R[n]<<1)|T;T=ult;
CREF(n);
CDEF(n);
break;
}
/* rotr <REG_N> 0100nnnn00000101 */
case 82:      
{
int n =  (iword >>8) & 0xf;
TL(n);T=R[n]&1;R[n] = UR[n]>> 1;R[n]|=(T<<31);
CREF(n);
CDEF(n);
break;
}
/* rotcr <REG_N> 0100nnnn00100101 */
case 83:      
{
int n =  (iword >>8) & 0xf;
TL(n);ult=R[n]&1;R[n]=(UR[n]>>1)|(T<<31);T=ult;
CREF(n);
CDEF(n);
break;
}
/* cmp/pl <REG_N> 0100nnnn00010101 */
case 84:      
{
int n =  (iword >>8) & 0xf;
TL(n);T = R[n]>0;
CREF(n);
break;
}
/* lds.l @<REG_N>+,FPSCR 0100nnnn01100110 */
case 85:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();*(int *)buf = RLAT(R[n]); FPSCR = *(float *)buf; R[n]+=4;
break;
}
/* lds.l @<REG_N>+,FPUL 0100nnnn01010110 */
case 86:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();FPUL = RLAT(R[n]);R[n]+=4;
break;
}
/* lds.l @<REG_N>+,MACH 0100nnnn00000110 */
case 87:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();MACH = SEXT(RLAT(R[n]));R[n]+=4;
CREF(n);
break;
}
/* lds.l @<REG_N>+,PR 0100nnnn00100110 */
case 88:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();PR = RLAT(R[n]);R[n]+=4;;
CREF(n);
break;
}
/* lds.l @<REG_N>+,MACL 0100nnnn00010110 */
case 89:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();MACL = RLAT(R[n]);R[n]+=4;
CREF(n);
break;
}
/* ldc.l @<REG_N>+,VBR 0100nnnn00100111 */
case 90:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();VBR=RLAT(R[n]);R[n]+=4;;
CREF(n);
break;
}
/* ldc.l @<REG_N>+,SPC 0100nnnn01000111 */
case 91:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();SPC=RLAT(R[n]);R[n]+=4;;
CREF(n);
break;
}
/* ldc.l @<REG_N>+,SR 0100nnnn00000111 */
case 92:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();SET_SR(RLAT(R[n]));R[n]+=4;;
CREF(n);
break;
}
/* ldc.l @<REG_N>+,GBR 0100nnnn00010111 */
case 93:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();GBR=RLAT(R[n]);R[n]+=4;;
CREF(n);
break;
}
/* ldc.l @<REG_N>+,SSR 0100nnnn00110111 */
case 94:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();SSR=RLAT(R[n]);R[n]+=4;;
CREF(n);
break;
}
/* shll2 <REG_N> 0100nnnn00001000 */
case 95:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]<<=2;
CREF(n);
CDEF(n);
break;
}
/* shll8 <REG_N> 0100nnnn00011000 */
case 96:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]<<=8;
CREF(n);
CDEF(n);
break;
}
/* shll16 <REG_N> 0100nnnn00101000 */
case 97:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]<<=16;
CREF(n);
CDEF(n);
break;
}
/* shlr2 <REG_N> 0100nnnn00001001 */
case 98:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=UR[n]>>2;
CREF(n);
CDEF(n);
break;
}
/* shlr8 <REG_N> 0100nnnn00011001 */
case 99:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=UR[n]>>8;
CREF(n);
CDEF(n);
break;
}
/* shlr16 <REG_N> 0100nnnn00101001 */
case 100:      
{
int n =  (iword >>8) & 0xf;
TL(n);R[n]=UR[n]>>16;
CREF(n);
CDEF(n);
break;
}
/* lds <REG_N>,MACL 0100nnnn00011010 */
case 101:      
{
int n =  (iword >>8) & 0xf;
TL(n);MACL= R[n];
CREF(n);
break;
}
/* lds <REG_N>,MACH 0100nnnn00001010 */
case 102:      
{
int n =  (iword >>8) & 0xf;
TL(n);MACH = R[n];
CREF(n);
break;
}
/* lds <REG_N>,FPUL 0100nnnn01011010 */
case 103:      
{
int n =  (iword >>8) & 0xf;
TL(n);FPUL = R[n];
break;
}
/* lds <REG_N>,FPSCR 0100nnnn01101010 */
case 104:      
{
int n =  (iword >>8) & 0xf;
TL(n);*(int *)buf = R[n]; FPSCR = *(float *)buf;
break;
}
/* lds <REG_N>,PR 0100nnnn00101010 */
case 105:      
{
int n =  (iword >>8) & 0xf;
TL(n);PR = R[n];
CREF(n);
break;
}
/* jsr @<REG_N> 0100nnnn00001011 */
case 106:      
{
int n =  (iword >>8) & 0xf;
TL(n);PR = PC + 4; PC=R[n]-2; if (~doprofile) gotcall(PR,PC+2);SL(PR-2);
CREF(n);
break;
}
/* jmp @<REG_N> 0100nnnn00101011 */
case 107:      
{
int n =  (iword >>8) & 0xf;
TL(n);ult = PC; PC=R[n]-2; SL(ult+2);
CREF(n);
break;
}
/* tas.b @<REG_N> 0100nnnn00011011 */
case 108:      
{
int n =  (iword >>8) & 0xf;
TL(n);MA();ult=RBAT(R[n]);T=ult==0;WBAT(R[n],ult|0x80);
CREF(n);
break;
}
/* shad <REG_M>,<REG_N> 0100nnnnmmmm1100 */
case 109:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n] = (R[m] < 0) ? (R[n] >> ((-R[m])&0x1f)) : (R[n] << (R[m] & 0x1f));
CREF(m);
CREF(n);
CDEF(n);
break;
}
/* shld <REG_M>,<REG_N> 0100nnnnmmmm1101 */
case 110:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n] = (R[m] < 0) ? (UR[n] >> ((-R[m])&0x1f)): (R[n] << (R[m] & 0x1f));
CREF(m);
CREF(n);
CDEF(n);
break;
}
/* ldc <REG_N>,SSR 0100nnnn00111110 */
case 111:      
{
int n =  (iword >>8) & 0xf;
TL(n);SSR=R[n];
CREF(n);
break;
}
/* ldc <REG_N>,GBR 0100nnnn00011110 */
case 112:      
{
int n =  (iword >>8) & 0xf;
TL(n);GBR=R[n];
CREF(n);
break;
}
/* ldc <REG_N>,SR 0100nnnn00001110 */
case 113:      
{
int n =  (iword >>8) & 0xf;
TL(n);SET_SR(R[n]);
CREF(n);
break;
}
/* ldc <REG_N>,VBR 0100nnnn00101110 */
case 114:      
{
int n =  (iword >>8) & 0xf;
TL(n);VBR=R[n];
CREF(n);
break;
}
/* ldc <REG_N>,SPC 0100nnnn01001110 */
case 115:      
{
int n =  (iword >>8) & 0xf;
TL(n);SPC=R[n];
CREF(n);
break;
}
/* mac.w @<REG_M>+,@<REG_N>+ 0100nnnnmmmm1111 */
case 116:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);macw(R0,memory,n,m);
CREF(n);
break;
}
/* mov.l @(<disp>,<REG_M>),<REG_N> 0101nnnnmmmmi4*4 */
case 117:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
int i = (iword & 0xf)<<2;
TB(m,n);MA();R[n]=RLAT(i+R[m]);L(n);
CREF(m);
CDEF(n);
break;
}
/* mov.b @<REG_M>,<REG_N> 0110nnnnmmmm0000 */
case 118:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]=RSBAT(R[m]);L(n);
CREF(m);
CDEF(n);
break;
}
/* mov.w @<REG_M>,<REG_N> 0110nnnnmmmm0001 */
case 119:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]=RSWAT(R[m]);L(n);
CREF(m);
CDEF(n);
break;
}
/* mov.l @<REG_M>,<REG_N> 0110nnnnmmmm0010 */
case 120:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]=RLAT(R[m]);L(n);
CREF(m);
CDEF(n);
break;
}
/* mov <REG_M>,<REG_N> 0110nnnnmmmm0011 */
case 121:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n] = R[m];
CREF(m);
CDEF(n);
break;
}
/* mov.b @<REG_M>+,<REG_N> 0110nnnnmmmm0100 */
case 122:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n] = RSBAT(R[m]);L(n);R[m]++;
CREF(m);
CDEF(n);
break;
}
/* mov.w @<REG_M>+,<REG_N> 0110nnnnmmmm0101 */
case 123:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]=RSWAT(R[m]);R[m]+=2;L(n);
CREF(n);
CDEF(n);
CDEF(m);
break;
}
/* mov.l @<REG_M>+,<REG_N> 0110nnnnmmmm0110 */
case 124:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n]=RLAT(R[m]);R[m]+=4;L(n);
CREF(m);
CDEF(n);
CDEF(m);
break;
}
/* not <REG_M>,<REG_N> 0110nnnnmmmm0111 */
case 125:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n]=~R[m];
CREF(m);
CDEF(n);
break;
}
/* swap.b <REG_M>,<REG_N> 0110nnnnmmmm1000 */
case 126:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n]=((R[m]<<8)&0xff00)|((R[m]>>8)&0x00ff);
CREF(n);
CREF(m);
CDEF(n);
break;
}
/* swap.w <REG_M>,<REG_N> 0110nnnnmmmm1001 */
case 127:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n]=((R[m]<<16)&0xffff0000)|((R[m]>>16)&0x00ffff);
CREF(n);
CREF(m);
CDEF(n);
break;
}
/* negc <REG_M>,<REG_N> 0110nnnnmmmm1010 */
case 128:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);ult=-T;T=ult>0;R[n]=ult-R[m];T|=R[n]>ult;
CREF(m);
CDEF(n);
break;
}
/* neg <REG_M>,<REG_N> 0110nnnnmmmm1011 */
case 129:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n] = - R[m];
CREF(m);
CDEF(n);
break;
}
/* extu.b <REG_M>,<REG_N> 0110nnnnmmmm1100 */
case 130:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n] = R[m] & 0xff;
CREF(m);
CDEF(n);
break;
}
/* extu.w <REG_M>,<REG_N> 0110nnnnmmmm1101 */
case 131:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n] = R[m] & 0xffff;
CREF(m);
CDEF(n);
break;
}
/* exts.b <REG_M>,<REG_N> 0110nnnnmmmm1110 */
case 132:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n] = SEXT(R[m]);
CREF(m);
CDEF(n);
break;
}
/* exts.w <REG_M>,<REG_N> 0110nnnnmmmm1111 */
case 133:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);R[n] = SEXTW(R[m]);
CREF(m);
CDEF(n);
break;
}
/* add #<imm>,<REG_N> 0111nnnni8*1.... */
case 134:      
{
int n =  (iword >>8) & 0xf;
int i = (iword & 0xff);
TL(n);R[n] += SEXT(i);if (i == 0) { UNDEF(n); break; } 
CDEF(n);
break;
}
/* bt.s <bdisp8> 10001101i8p1.... */
case 135:      
{
int i = (iword & 0xff);
if(T) {ult = PC; PC+=(SEXT(i)<<1)+2;C+=2;SL(ult+2);}
break;
}
/* bf.s <bdisp8> 10001111i8p1.... */
case 136:      
{
int i = (iword & 0xff);
if(!T) {ult = PC; PC+=(SEXT(i)<<1)+2;C+=2;SL(ult+2);}
break;
}
/* cmp/eq #<imm>,R0 10001000i8*1.... */
case 137:      
{
int i = (iword & 0xff);
;T = R0 == SEXT(i);
CREF(0);
break;
}
/* bf <bdisp8> 10001011i8p1.... */
case 138:      
{
int i = (iword & 0xff);
if(T==0) {PC+=(SEXT(i)<<1)+2;C+=2;}
break;
}
/* bt <bdisp8> 10001001i8p1.... */
case 139:      
{
int i = (iword & 0xff);
if(T) {PC+=(SEXT(i)<<1)+2;C+=2;}
break;
}
/* mov.b @(<disp>,<REG_M>),R0 10000100mmmmi4*1 */
case 140:      
{
int m =  (iword >>4) & 0xf;
int i = (iword & 0xf);
TL(m);MA();R0=RSBAT(i+R[m]);L(0);
CREF(m);
CDEF(0);
break;
}
/* mov.b R0,@(<disp>,<REG_M>) 10000000mmmmi4*1 */
case 141:      
{
int m =  (iword >>4) & 0xf;
int i = (iword & 0xf);
TL(m);MA();WBAT(i+R[m],R0);
CREF(m);
CREF(0);
break;
}
/* mov.w R0,@(<disp>,<REG_M>) 10000001mmmmi4*2 */
case 142:      
{
int m =  (iword >>4) & 0xf;
int i = (iword & 0xf)<<1;
TL(m);MA();WWAT(i+R[m],R0);
CREF(0);
CREF(m);
break;
}
/* mov.w @(<disp>,<REG_M>),R0 10000101mmmmi4*2 */
case 143:      
{
int m =  (iword >>4) & 0xf;
int i = (iword & 0xf)<<1;
TL(m);MA();R0=RSWAT(i+R[m]);L(0);
CREF(m);
CDEF(0);
break;
}
/* mov.w @(<disp>,PC),<REG_N> 1001nnnni8p2.... */
case 144:      
{
int n =  (iword >>8) & 0xf;
int i = (iword & 0xff)<<1;
TL(n);MA();R[n]=RSWAT(PC+i+4);L(n);
CDEF(n);
break;
}
/* bra <bdisp12> 1010i12......... */
case 145:      
{
int i = (iword & 0xfff);
i = (i ^ (1<<11))-(1<<11);
ult = PC; PC=PC+(i<<1)+2;SL(ult+2);
break;
}
/* bsr <bdisp12> 1011i12......... */
case 146:      
{
int i = (iword & 0xfff);
i = (i ^ (1<<11))-(1<<11);
PR = PC + 4; PC=PC+(i<<1)+2;SL(PR-2);
break;
}
/* trapa #<imm> 11000011i8*1.... */
case 147:      
{
int i = (iword & 0xff);
{ long imm = 0xff & i; if (i==0xc3) PC-=2; if (i<20||i==34||i==0xc3) trap(i,R,memory,maskl,maskw,little_endian); else { R[15]-=4; WLAT(R[15],GET_SR()); R[15]-=4;WLAT(R[15],PC+2); PC=RLAT(VBR+(imm<<2))-2;}}
CDEF(0);
break;
}
/* tst.b #<imm>,@(R0,GBR) 11001100i8*1.... */
case 148:      
{
int i = (iword & 0xff);
MA();T=(RBAT(GBR+R0)&i)==0;
CREF(0);
break;
}
/* mov.w @(<disp>,GBR),R0 11000101i8*2.... */
case 149:      
{
int i = (iword & 0xff)<<1;
MA();R0=RSWAT(i+GBR);L(0);
CDEF(0);
break;
}
/* mov.b R0,@(<disp>,GBR) 11000000i8*1.... */
case 150:      
{
int i = (iword & 0xff);
MA();WBAT(i+GBR,R0);
CREF(0);
break;
}
/* mov.l @(<disp>,GBR),R0 11000110i8*4.... */
case 151:      
{
int i = (iword & 0xff)<<2;
MA();R0=RLAT(i+GBR);L(0);
CDEF(0);
break;
}
/* mov.w R0,@(<disp>,GBR) 11000001i8*2.... */
case 152:      
{
int i = (iword & 0xff)<<1;
MA();WWAT(i+GBR,R0);
CREF(0);
break;
}
/* mova @(<disp>,PC),R0 11000111i8p4.... */
case 153:      
{
int i = (iword & 0xff)<<2;
R0=((i+4+PC) & ~0x3);
CDEF(0);
break;
}
/* or #<imm>,R0 11001011i8*1.... */
case 154:      
{
int i = (iword & 0xff);
R0|=i;
CDEF(0);
break;
}
/* xor #<imm>,R0 11001010i8*1.... */
case 155:      
{
int i = (iword & 0xff);
R0^=i;
CREF(0);
break;
}
/* mov.b @(<disp>,GBR),R0 11000100i8*1.... */
case 156:      
{
int i = (iword & 0xff);
MA();R0=RSBAT(i+GBR);L(0);
CDEF(0);
break;
}
/* mov.l R0,@(<disp>,GBR) 11000010i8*4.... */
case 157:      
{
int i = (iword & 0xff)<<2;
MA();WLAT(i+GBR,R0);
CREF(0);
break;
}
/* xor.b #<imm>,@(R0,GBR) 11001110i8*1.... */
case 158:      
{
int i = (iword & 0xff);
MA();ult=RBAT(GBR+R0);ult^=i;WBAT(GBR+R0,ult);
CREF(0);
break;
}
/* tst #<imm>,R0 11001000i8*1.... */
case 159:      
{
int i = (iword & 0xff);
T=(R0&i)==0;
CREF(0);
break;
}
/* or.b #<imm>,@(R0,GBR) 11001111i8*1.... */
case 160:      
{
int i = (iword & 0xff);
MA();WBAT(R0+GBR,RBAT(R0+GBR)|i);
CREF(0);
break;
}
/* and #<imm>,R0 11001001i8*1.... */
case 161:      
{
int i = (iword & 0xff);
;R0&=i;
CDEF(0);
break;
}
/* and.b #<imm>,@(R0,GBR) 11001101i8*1.... */
case 162:      
{
int i = (iword & 0xff);
MA();WBAT(GBR+R0, RBAT(GBR+R0) & i);
CREF(0);
break;
}
/* mov.l @(<disp>,PC),<REG_N> 1101nnnni8p4.... */
case 163:      
{
int n =  (iword >>8) & 0xf;
int i = (iword & 0xff)<<2;
TL(n);MA();R[n]=RLAT((i+4+PC) & ~3);L(n);
CDEF(n);
break;
}
/* mov #<imm>,<REG_N> 1110nnnni8*1.... */
case 164:      
{
int n =  (iword >>8) & 0xf;
int i = (iword & 0xff);
TL(n);R[n] = SEXT(i);
CDEF(n);
break;
}
/* fadd <FREG_M>,<FREG_N> 1111nnnnmmmm0000 */
case 165:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);F[n] = F[n] + F[m];
break;
}
/* fsub <FREG_M>,<FREG_N> 1111nnnnmmmm0001 */
case 166:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);F[n] = F[n] - F[m];
break;
}
/* fmul <FREG_M>,<FREG_N> 1111nnnnmmmm0010 */
case 167:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);F[n] = F[n] * F[m];
break;
}
/* fdiv <FREG_M>,<FREG_N> 1111nnnnmmmm0011 */
case 168:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);F[n] = F[n] / F[m];
break;
}
/* fcmp/eq <FREG_M>,<FREG_N> 1111nnnnmmmm0100 */
case 169:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);T = F[n] == F[m] ? 1 : 0;
break;
}
/* fcmp/gt <FREG_M>,<FREG_N> 1111nnnnmmmm0101 */
case 170:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);T = F[n] > F[m] ? 1 : 0;
break;
}
/* fmov.s @(R0,<REG_M>),<FREG_N> 1111nnnnmmmm0110 */
case 171:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();*(int *)buf = RLAT((R[0]+R[m]));F[n] = *(float *)buf;
break;
}
/* fmov.s <FREG_M>,@(R0,<REG_N>) 1111nnnnmmmm0111 */
case 172:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();*(float *)buf = F[m]; WLAT((R[0]+R[n]), *(int *)buf);
break;
}
/* fmov.s @<REG_M>,<FREG_N> 1111nnnnmmmm1000 */
case 173:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();*(int *)buf = RLAT(R[m]);F[n] = *(float *)buf;
break;
}
/* fmov.s @<REG_M>+,<FREG_N> 1111nnnnmmmm1001 */
case 174:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();*(int *)buf = RLAT(R[m]); F[n] = *(float *)buf; R[m] += 4;
break;
}
/* fmov.s <FREG_M>,@<REG_N> 1111nnnnmmmm1010 */
case 175:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();*(float *)buf = F[m]; WLAT(R[n], *(int *)buf);
break;
}
/* fmov.s <FREG_M>,@-<REG_N> 1111nnnnmmmm1011 */
case 176:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);MA();R[n] -= 4; *(float *)buf = F[m]; WLAT(R[n], *(int *)buf);
break;
}
/* fmov <FREG_M>,<FREG_N> 1111nnnnmmmm1100 */
case 177:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);F[n] = F[m];
break;
}
/* ftrc <FREG_N>, FPUL 1111nnnn00111101 */
case 178:      
{
int n =  (iword >>8) & 0xf;
TL(n);if (F[n] != F[n]) /* NaN */ FPUL = 0x80000000; else FPUL = (int)F[n];
break;
}
/* ftst/nan <FREG_N> 1111nnnn01111101 */
case 179:      
{
int n =  (iword >>8) & 0xf;
TL(n);T = isnan (F[n]);
break;
}
/* float FPUL,<FREG_N> 1111nnnn00101101 */
case 180:      
{
int n =  (iword >>8) & 0xf;
TL(n);F[n] = (float)FPUL;
break;
}
/* flds <FREG_N>,FPUL 1111nnnn00011101 */
case 181:      
{
int n =  (iword >>8) & 0xf;
TL(n);*(float *)buf = F[n]; FPUL = *(int *)buf;
break;
}
/* fneg <FREG_N> 1111nnnn01001101 */
case 182:      
{
int n =  (iword >>8) & 0xf;
TL(n);F[n] = -F[n];
break;
}
/* fsts FPUL,<FREG_N> 1111nnnn00001101 */
case 183:      
{
int n =  (iword >>8) & 0xf;
TL(n);*(int *)buf = FPUL; F[n] = *(float *)buf;
break;
}
/* fabs <FREG_N> 1111nnnn01011101 */
case 184:      
{
int n =  (iword >>8) & 0xf;
TL(n);F[n] = fabs (F[n]);
break;
}
/* fldi1 <FREG_N> 1111nnnn10011101 */
case 185:      
{
int n =  (iword >>8) & 0xf;
TL(n);F[n] = (float)1.0;
break;
}
/* fldi0 <FREG_N> 1111nnnn10001101 */
case 186:      
{
int n =  (iword >>8) & 0xf;
TL(n);F[n] = (float)0.0;
break;
}
/* fsqrt <FREG_N> 1111nnnn01101101 */
case 187:      
{
int n =  (iword >>8) & 0xf;
TL(n);F[n] = sqrt (F[n]);
break;
}
/* fmac <FREG_0>,<FREG_M>,<FREG_N> 1111nnnnmmmm1110 */
case 188:      
{
int n =  (iword >>8) & 0xf;
int m =  (iword >>4) & 0xf;
TB(m,n);F[n] = F[m] * F[0] + F[n];
break;
}
default:
{
saved_state.asregs.exception = SIGILL;
}
}
}
