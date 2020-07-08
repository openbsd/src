#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include "x86emu.h"
#include <machine/vmmvar.h>

#define printf(x...) fprintf(stderr, x)

enum {
  REX_B    = 0x1,   // mrr.rrr or sib.bbb or op.ggg
  REX_X    = 0x2,   // sib.iii
  REX_R    = 0x4,   // mrr.ggg
  REX_W    = 0x8,   // opsize

  /* Prefix flags */
  FLG_REX  = 0x0001,
  FLG_SEG  = 0x0002,
  FLG_OSZ  = 0x0004,
  FLG_ASZ  = 0x0008,
  FLG_LOCK = 0x0010,
  FLG_REP  = 0x0020,

  /* Additional flags */
  FLG_MRR  = 0x0100,  // has mod-reg-rm byte
  FLG_GRP  = 0x0200,  // opcode based on mrr.reg
  FLG_D64  = 0x0400,  // default size = 64-bit
  FLG_NO64 = 0x0800,  // invalid in 64-bit mode
  FLG_MEM  = 0x1000,  // non-mrr memory
};

#define _(m, a...) { .mnem=#m, a }
#define _xxx { }
#define __ 0

struct opcode {
  const char *mnem;
  int arg0;
  int arg1;
  int arg2;
  int flag;
};

struct opcode hicodes[256] = {
  [0x30] =
   _(wrmsr),
   _(rdtsc),
   _(rdmsr),
   _(rdpmc),
   _(sysenter),
   _(sysexit),

   /* 0x40 */
   [0x40] =
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),
   _(cmovcc,  Gv,  Ev, __, FLG_MRR),

   /* 0x80 */
   [0x80] =
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),
   _(jcc,     Jv,  __, __, FLG_D64),

   /* 0x90 */
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),
   _(setcc,   Eb,  __, __, FLG_MRR),

   /* 0xa0 */
   _(push,    rFS, __, __, FLG_D64),
   _(pop,     rFS, __, __, FLG_D64),
   _(cpuid),
   _(bt,      Ev,  Gv, __, FLG_MRR),
   _(shld,    Ev,  Gv, Ib, FLG_MRR),
   _(shld,    Ev,  Gv, rCL,FLG_MRR),
   _xxx,
   _xxx,
   _(push,    rGS, __, __, FLG_D64),
   _(pop,     rGS, __, __, FLG_D64),
   _xxx,
   _(bts,     Ev,  Gv, __, FLG_MRR),
   _(shrd,    Ev,  Gv, Ib, FLG_MRR),
   _(shrd,    Ev,  Gv, rCL,FLG_MRR),
   _xxx,
   _(imul,    Gv,  Ev, __, FLG_MRR),

   /* 0xb0 */
   _(cmpxchg, Eb,  Gb, __, FLG_MRR),
   _(cmpxchg, Ev,  Gv, __, FLG_MRR),
   _(lss,     Gv,  Mp, __, FLG_MRR),
   _(btr,     Ev,  Gv, __, FLG_MRR),
   _(lfs,     Gv,  Mp, __, FLG_MRR),
   _(lgs,     Gv,  Mp, __, FLG_MRR),
   _(movzx,   Gv,  Eb, __, FLG_MRR),
   _(movzx,   Gv,  Ew, __, FLG_MRR),
   _xxx,
   _xxx,
   _xxx,
   _(btc,     Ev,  Gv, __, FLG_MRR),
   _(bsf,     Gv,  Ev, __, FLG_MRR),
   _(bsr,     Gv,  Ev, __, FLG_MRR),
   _(movsx,   Gv,  Eb, __, FLG_MRR),
   _(movsx,   Gv,  Ew, __, FLG_MRR),

   /* 0xc0 */
   _(xadd,    Eb,  Gb, __, FLG_MRR),
   _(xadd,    Ev,  Gv, __, FLG_MRR),
   _xxx,
   _xxx,
   _xxx,
   _xxx,
   _xxx,
   _xxx,
   _(bswap,   gv),
   _(bswap,   gv),
   _(bswap,   gv),
   _(bswap,   gv),
   _(bswap,   gv),
   _(bswap,   gv),
   _(bswap,   gv),
   _(bswap,   gv),
};

struct opcode locodes[256] = {
   _(add,     Eb,  Gb, __, FLG_MRR),
   _(add,     Ev,  Gv, __, FLG_MRR),
   _(add,     Gb,  Eb, __, FLG_MRR),
   _(add,     Gv,  Ev, __, FLG_MRR),
   _(add,     rAL, Ib),
   _(add,    rvAX, Iv),
   _(push,   rES,  __, __, FLG_NO64),
   _(pop,    rES,  __, __, FLG_NO64),
   _(or,      Eb,  Gb, __, FLG_MRR),
   _(or,      Ev,  Gv, __, FLG_MRR),
   _(or,      Gb,  Eb, __, FLG_MRR),
   _(or,      Gv,  Ev, __, FLG_MRR),
   _(or,      rAL, Ib),
   _(or,     rvAX, Iv),
   _(push,    rCS, __, __, FLG_NO64),
   _xxx,

   /* 0x10 */
   _(adc,     Eb,  Gb, __, FLG_MRR),
   _(adc,     Ev,  Gv, __, FLG_MRR),
   _(adc,     Gb,  Eb, __, FLG_MRR),
   _(adc,     Gv,  Ev, __, FLG_MRR),
   _(adc,     rAL, Ib),
   _(adc,    rvAX, Iv),
   _(push,    rSS, __, __, FLG_NO64),
   _(pop,     rSS, __, __, FLG_NO64),
   _(sbb,     Eb,  Gb, __, FLG_MRR),
   _(sbb,     Ev,  Gv, __, FLG_MRR),
   _(sbb,     Gb,  Eb, __, FLG_MRR),
   _(sbb,     Gv,  Ev, __, FLG_MRR),
   _(sbb,     rAL, Ib),
   _(sbb,    rvAX, Iv),
   _(push,    rDS, __, __, FLG_NO64),
   _(pop,     rDS, __, __, FLG_NO64),

   /* 0x20 */
   _(and,     Eb,  Gb, __, FLG_MRR),
   _(and,     Ev,  Gv, __, FLG_MRR),
   _(and,     Gb,  Eb, __, FLG_MRR),
   _(and,     Gv,  Ev, __, FLG_MRR),
   _(and,     rAL, Ib),
   _(and,    rvAX, Iv),
   _(pfx,     rES, __, __, FLG_SEG),
   _(daa,      __, __, __, FLG_NO64),
   _(sub,     Eb,  Gb, __, FLG_MRR),
   _(sub,     Ev,  Gv, __, FLG_MRR),
   _(sub,     Gb,  Eb, __, FLG_MRR),
   _(sub,     Gv,  Ev, __, FLG_MRR),
   _(sub,     rAL, Ib),
   _(sub,    rvAX, Iv),
   _(pfx,     rCS, __, __, FLG_SEG),
   _(das,     __,  __, __, FLG_NO64),

   /* 0x30 */
   _(xor,     Eb,  Gb, __, FLG_MRR),
   _(xor,     Ev,  Gv, __, FLG_MRR),
   _(xor,     Gb,  Eb, __, FLG_MRR),
   _(xor,     Gv,  Ev, __, FLG_MRR),
   _(xor,     rAL, Ib),
   _(xor,    rvAX, Iv),
   _(pfx,     rSS, __, __, FLG_SEG),
   _(aaa,      __, __, __, FLG_NO64),
   _(cmp,      Eb, Gb, __, FLG_MRR),
   _(cmp,      Ev, Gv, __, FLG_MRR),
   _(cmp,      Gb, Eb, __, FLG_MRR),
   _(cmp,      Gv, Ev, __, FLG_MRR),
   _(cmp,     rAL, Ib),
   _(cmp,    rvAX, Iv),
   _(pfx,     rDS, __, __, FLG_SEG),
   _(aas,      __, __, __, FLG_NO64),

   /* 0x40 */
   _(inc,     gv,  __, __, FLG_REX),
   _(inc,     gv,  __, __, FLG_REX),
   _(inc,     gv,  __, __, FLG_REX),
   _(inc,     gv,  __, __, FLG_REX),
   _(inc,     gv,  __, __, FLG_REX),
   _(inc,     gv,  __, __, FLG_REX),
   _(inc,     gv,  __, __, FLG_REX),
   _(inc,     gv,  __, __, FLG_REX),
   _(dec,     gv,  __, __, FLG_REX),
   _(dec,     gv,  __, __, FLG_REX),
   _(dec,     gv,  __, __, FLG_REX),
   _(dec,     gv,  __, __, FLG_REX),
   _(dec,     gv,  __, __, FLG_REX),
   _(dec,     gv,  __, __, FLG_REX),
   _(dec,     gv,  __, __, FLG_REX),
   _(dec,     gv,  __, __, FLG_REX),

   /* 0x50 */
   _(push,    gv,  __, __, FLG_D64), // 41.50 = r8
   _(push,    gv,  __, __, FLG_D64),
   _(push,    gv,  __, __, FLG_D64),
   _(push,    gv,  __, __, FLG_D64),
   _(push,    gv,  __, __, FLG_D64),
   _(push,    gv,  __, __, FLG_D64),
   _(push,    gv,  __, __, FLG_D64),
   _(push,    gv,  __, __, FLG_D64), // 41.57 = r15
   _(pop,     gv,  __, __, FLG_D64),
   _(pop,     gv,  __, __, FLG_D64),
   _(pop,     gv,  __, __, FLG_D64),
   _(pop,     gv,  __, __, FLG_D64),
   _(pop,     gv,  __, __, FLG_D64),
   _(pop,     gv,  __, __, FLG_D64),
   _(pop,     gv,  __, __, FLG_D64),
   _(pop,     gv,  __, __, FLG_D64),

   /* 0x60 */
   _(pusha,   __,  __, __, FLG_NO64),
   _(popa,    __,  __, __, FLG_NO64),
   _xxx,                                       // EVEX
   _xxx,                                       // movsxd Gv, Rd
   _(pfx,    rFS,  __, __, FLG_SEG),
   _(pfx,    rGS,  __, __, FLG_SEG),
   _(pfx,     __,  __, __, FLG_OSZ),
   _(pfx,     __,  __, __, FLG_ASZ),
   _(push,    Iz,  __, __, FLG_D64),
   _(imul,    Gv,  Ev, Iz, FLG_MRR),
   _(push,    Ib,  __, __, FLG_D64),
   _(imul,    Gv,  Ev, Ib, FLG_MRR),
   _(insb,    Yb, rDX, __, FLG_MEM),   // rep
   _(insv,    Yv, rDX, __, FLG_MEM),   // rep
   _(outsb,  rDX,  Xb, __, FLG_MEM),   // rep
   _(outsv,  rDX,  Xv, __, FLG_MEM),   // rep

   /* 0x70 */
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),
   _(jcc,     Jb,  __, __, FLG_D64),

   /* 0x80 */
   _(grp1,    Eb,  Ib, __, FLG_MRR|FLG_GRP),
   _(grp1,    Ev,  Iz, __, FLG_MRR|FLG_GRP),
   _(grp1,    Eb,  Ib, __, FLG_MRR|FLG_GRP|FLG_NO64),
   _(grp1,    Ev,  Ib, __, FLG_MRR|FLG_GRP),
   _(test,    Eb,  Gb, __, FLG_MRR),
   _(test,    Ev,  Gv, __, FLG_MRR),
   _(xchg,    Eb,  Gb, __, FLG_MRR),
   _(xchg,    Ev,  Gv, __, FLG_MRR),
   _(mov,     Eb,  Gb, __, FLG_MRR),
   _(mov,     Ev,  Gv, __, FLG_MRR),
   _(mov,     Gb,  Eb, __, FLG_MRR),
   _(mov,     Gv,  Ev, __, FLG_MRR),
   _(mov,     Ew,  Sw, __, FLG_MRR),
   _(lea,     Gv,  Mp, __, FLG_MRR),
   _(mov,     Sw,  Ew, __, FLG_MRR),
   _(pop,     Ev,  __, __, FLG_MRR), // GRP1a [pop]

   /* 0x90 */
   _(nop),
   _(xchg,  rvAX,  gv), // 49.91 = xchg rax,r9
   _(xchg,  rvAX,  gv),
   _(xchg,  rvAX,  gv),
   _(xchg,  rvAX,  gv),
   _(xchg,  rvAX,  gv),
   _(xchg,  rvAX,  gv),
   _(xchg,  rvAX,  gv),
   _(cbw), // AX=AL    / EAX=AX      / RAX=EAX
   _(cwd), // DX:AX=AX / EDX:EAX=EAX / RDX:RAX=RAX
   _(call,    Ap,  __, __, FLG_NO64),
   _(wait),
   _(pushf,   __,  __, __, FLG_D64),
   _(popf,    __,  __, __, FLG_D64),
   _(sahf),
   _(lahf),

   /* 0xa0 */
   _(mov,    rAL,  Ob, __, FLG_MEM),
   _(mov,   rvAX,  Ov, __, FLG_MEM),
   _(mov,     Ob, rAL, __, FLG_MEM),
   _(mov,     Ov,rvAX, __, FLG_MEM),
   _(movsb,   Xb,  Yb, __, FLG_MEM), //rep
   _(movsv,   Xv,  Yv, __, FLG_MEM), //rep
   _(cmpsb,   Xb,  Yb, __, FLG_MEM), //repz/repnz
   _(cmpsv,   Xb,  Yv, __, FLG_MEM), //repz/repnz
   _(test,   rAL,  Ib),
   _(test,  rvAX,  Iv),
   _(stosb,   Yb, rAL, __, FLG_MEM), //rep
   _(stosv,   Yv,rvAX, __, FLG_MEM), //rep
   _(lodsb,  rAL,  Xb, __, FLG_MEM),
   _(lodsv, rvAX,  Xv, __, FLG_MEM),
   _(scasb,  rAL,  Yb, __, FLG_MEM), //repz/repnz
   _(scasv, rvAX,  Yv, __, FLG_MEM), //repz/repnz

   /* 0xb0 */
   _(mov,     gb,  Ib),
   _(mov,     gb,  Ib),
   _(mov,     gb,  Ib),
   _(mov,     gb,  Ib),
   _(mov,     gb,  Ib), // spl
   _(mov,     gb,  Ib), // bpl
   _(mov,     gb,  Ib), // sil
   _(mov,     gb,  Ib), // dil
   _(mov,     gv,  Iv), // Iq
   _(mov,     gv,  Iv),
   _(mov,     gv,  Iv),
   _(mov,     gv,  Iv),
   _(mov,     gv,  Iv),
   _(mov,     gv,  Iv),
   _(mov,     gv,  Iv),
   _(mov,     gv,  Iv),

   /* 0xc0 */
   _(grp2,    Eb,  Ib, __, FLG_MRR|FLG_GRP),
   _(grp2,    Ev,  Ib, __, FLG_MRR|FLG_GRP),
   _(ret,     Iw,  __, __, FLG_D64),
   _(ret,     __,  __, __, FLG_D64),
   _(les,     Gv,  Mp, __, FLG_MRR|FLG_NO64), // VEX3
   _(lds,     Gv,  Mp, __, FLG_MRR|FLG_NO64), // VEX2
   _(mov,     Eb,  Ib, __, FLG_MRR),  // GRP11 [mov]
   _(mov,     Ev,  Iz, __, FLG_MRR),  // GRP11 [mov]
   _(enter,   Iw,  Ib, __, FLG_D64),
   _(leave,   __,  __, __, FLG_D64),
   _(retf,    Iw),
   _(retf),
   _(int,     i3),
   _(int,     Ib),
   _(into,    __,  __, __, FLG_NO64),
   _(iret),

   /* 0xd0 */
   _(grp2,   Eb,  i1,  __, FLG_MRR|FLG_GRP),
   _(grp2,   Ev,  i1,  __, FLG_MRR|FLG_GRP),
   _(grp2,   Eb,  rCL, __, FLG_MRR|FLG_GRP),
   _(grp2,   Eb,  rCL, __, FLG_MRR|FLG_GRP),
   _(aam,    Ib,  __,  __, FLG_NO64),
   _(aad,    Ib,  __,  __, FLG_NO64),
   _(salc,   __,  __,  __, FLG_NO64),
   _(xlat,   __,  __,  __, FLG_MEM),
   _xxx,
   _xxx,
   _xxx,
   _xxx,
   _xxx,
   _xxx,
   _xxx,
   _xxx,

   /* 0xe0 */
   _(loopnz,  Jb,  __, __, FLG_D64),
   _(loopz,   Jb,  __, __, FLG_D64),
   _(loop,    Jb,  __, __, FLG_D64),
   _(jcxz,    Jb,  __, __, FLG_D64),
   _(in,     rAL,  Ib),
   _(in,    rvAX,  Ib),
   _(out,     Ib,  rAL),
   _(out,     Ib,  rvAX),
   _(call,    Jv,  __, __, FLG_D64),
   _(jmp,     Jv,  __, __, FLG_D64),
   _(jmp,     Ap,  __, __, FLG_NO64),
   _(jmp,     Jb,  __, __, FLG_D64),
   _(in,     rAL, rDX),
   _(in,    rvAX, rDX),
   _(out,    rDX, rAL),
   _(out,    rDX, rvAX),

   /* 0xf0 */
   _(pfx,    __, __,  __, FLG_LOCK),
   _(int,    i1),
   _(pfx,    __, __,  __, FLG_REP),
   _(pfx,    __, __,  __, FLG_REP),
   _(hlt),
   _(cmc),
   _(grp3,   __, __,  __, FLG_MRR|FLG_GRP), // Eb
   _(grp3,   __, __,  __, FLG_MRR|FLG_GRP), // Ev
   _(clc),
   _(stc),
   _(cli),
   _(sti),
   _(cld),
   _(std),
   _(grp4,   __, __,  __, FLG_MRR|FLG_GRP),
   _(grp5,   __, __,  __, FLG_MRR|FLG_GRP),
};

struct istate {
  uint32_t op;
  uint8_t  rep;
  uint8_t  rex;
  uint8_t  mrr;
  uint8_t  sib;
  uint32_t seg;
  uint32_t flag;
  uint32_t osz;
  uint32_t asz;
  uint32_t mode;

  /* Instruction bytes */
  uint8_t  ib[16];
  int      nib;

  uint8_t *pc;
};

static uint64_t
getb(struct istate *i) {
  uint8_t c = *i->pc++;

  i->ib[i->nib++] = c;
  return c;
}

/* Get operand size */
static int
osize(struct istate *i) {
  switch (i->mode) {
  case SIZE_QWORD:
    if ((i->flag & FLG_D64) || (i->rex & REX_W))
      return SIZE_QWORD;
    return (i->flag & FLG_OSZ) ? SIZE_WORD : SIZE_DWORD;
  case SIZE_DWORD:
    return (i->flag & FLG_OSZ) ? SIZE_WORD : SIZE_DWORD;
  case SIZE_WORD:
    return (i->flag & FLG_OSZ) ? SIZE_DWORD : SIZE_WORD;
  }
  return 0;
}  

/* Get address size */
static int
asize(struct istate *i) {
  switch (i->mode) {
  case SIZE_QWORD:
    return (i->flag & FLG_ASZ) ? SIZE_DWORD : SIZE_QWORD;
  case SIZE_DWORD:
    return (i->flag & FLG_ASZ) ? SIZE_WORD : SIZE_DWORD;
  case SIZE_WORD:
    return (i->flag & FLG_ASZ) ? SIZE_DWORD : SIZE_WORD;
  }
  return 0;
}

/* f3 48 a5 = rep movsv
 * f3 48 ab = rep stosv
 */

/*============================*
 * Decode opcode
 *============================*/
static struct opcode
decodeop(struct istate *i)
{
  struct opcode o;
  int op;

  for(;;) {
    op = getb(i);
    if (op == 0x0f) {
	op = (op << 8) | getb(i);
	o  = hicodes[op & 0xFF];
    } else {
	o = locodes[op];
    }
    i->flag |= o.flag;
    i->op = op;

    if (o.flag == FLG_SEG)
      i->seg = o.arg0;
    else if (o.flag == FLG_REP)
      i->rep = op;
    else if (o.flag == FLG_REX && (i->mode == SIZE_QWORD))
      i->rex = op;
    else if (!(o.flag & (FLG_OSZ|FLG_ASZ|FLG_LOCK))) {
      if (i->flag & FLG_MRR)
	i->mrr = getb(i);
      /* Get operand and address size */
      i->osz = osize(i);
      i->asz = asize(i);
      if (!o.mnem)
	o.mnem = "---";
      return o;
    }
  }
}

/*=====================================================
 * Register names
 *=====================================================*/
static const char *bregs[] = { 
  "al", "cl", "dl",  "bl",  "ah",  "ch",  "dh",  "bh",
  "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b",
  "spl","bpl","sil", "dil"
};
static const char *wregs[] = {
  "ax", "cx", "dx",  "bx",  "sp",  "bp",  "si",  "di",
  "r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w",
};
static const char *dregs[] = {
  "eax","ecx","edx", "ebx", "esp", "ebp", "esi", "edi",
  "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d",
};
static const char *qregs[] = {
  "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
  "r8", "r9", "r10","r11","r12","r13","r14","r15",
};

static const char *
regname(int reg) {
  int vv = reg & VAL_MASK;

  if (vv > 20)
    return "xx";
  switch (reg & SIZE_MASK) {
  case SIZE_BYTE: return bregs[vv];
  case SIZE_WORD: return wregs[vv];
  case SIZE_DWORD:return dregs[vv];
  case SIZE_QWORD:return qregs[vv];
  }
  return "--";
}

/* Make register */
static uint32_t
mkreg(struct istate *i, int sz, int vv, int mask) {
  /* REX byte */
  if (mask & i->rex)
    vv += 8;
  /* Special case for spl/bpl/sil/dil */
  if (sz == SIZE_BYTE && i->rex && (vv >= 4 && vv <= 7))
    vv += 12;
  vv += TYPE_REG+sz;
  printf("%%%s ", regname(vv));
  return vv;
}

/* Get Embedded or Decoded immediate byte */
static uint64_t
mkimm(struct istate *i, int sz, uint64_t val, const char *fmt) {
  switch (sz) {
  case SIZE_BYTE:
    val = getb(i);
    break;
  case SIZE_WORD:
    val = getb(i);
    val |= (getb(i) << 8);
    break;
  case SIZE_DWORD:
    val = getb(i);
    val |= getb(i) << 8;
    val |= getb(i) << 16;
    val |= getb(i) << 24;
    break;
  case SIZE_QWORD:
    val = getb(i);
    val |= getb(i) << 8;
    val |= getb(i) << 16;
    val |= getb(i) << 24;
    val |= getb(i) << 32LL;
    val |= getb(i) << 40LL;
    val |= getb(i) << 48LL;
    val |= getb(i) << 56LL;
    break;
  }
  printf(fmt, val);
  return val;
}

/* Decode effective address */
static uint32_t
mkea(struct istate *i, int sz) {
  int mm, rrr;

  mm = mrr_mm(i->mrr);
  rrr = mrr_rrr(i->mrr);
  if (mm == 3) {
    // register encoding
    return mkreg(i, sz, rrr, REX_B);
  }
  switch (i->asz) {
  case SIZE_QWORD:
    printf("(");
    if (rrr == 4) {
      i->sib = getb(i);
      printf("%d,", 1 << sib_ss(i->sib));
      mkreg(i, SIZE_QWORD, sib_iii(i->sib), REX_X);
      mkreg(i, SIZE_QWORD, sib_bbb(i->sib), REX_B);
    }
    else {
      mkreg(i, SIZE_QWORD, rrr, REX_B);
    }
    if (mm == 1) {
      mkimm(i, SIZE_BYTE, 0, "b[$0x%llx]");
    }
    else if (mm == 2) {
      mkimm(i, SIZE_DWORD, 0, "d[$0x%llx]");
    }
    else if (rrr == 5) {
      /* Special case RIP-relative */
      mkimm(i, SIZE_DWORD, 0, "%%rip[$0x%llx]");
    }
    break;
  }
  printf(") ");
  return 0;
}

static uint32_t
decodearg(struct istate *i, int arg) {
  int tt, sz, vv;

  if (!arg)
    return 0;
  tt = arg & TYPE_MASK;
  sz = arg & SIZE_MASK;
  vv = arg & VAL_MASK;

  if (sz == SIZE_VWORD)
    sz = i->osz;
  if (sz == SIZE_ZWORD)
    sz = SIZE_DWORD;
  switch (tt) {
  case TYPE_REG:    // specific register
    return mkreg(i, sz, vv, 0);
  case TYPE_EMBREG: // embedded in opcode
    return mkreg(i, sz, i->op & 0x7, REX_B);
  case TYPE_EAREG:  // embedded in mrr
    return mkreg(i, sz, mrr_ggg(i->mrr), REX_R);
  case TYPE_EA:
  case TYPE_EAMEM:  // effective address
    return mkea(i, sz);
  case TYPE_IMM:    // immediate value
    return mkimm(i, sz, vv, "imm:$0x%llx ");
  default:
    printf("Unknown arg: %.8x ", arg);
    break;
  }
  return 0;
}

static int
sz(int arg) {
  switch (arg & SIZE_MASK) {
  case SIZE_BYTE: return 1;
  case SIZE_WORD: return 2;
  case SIZE_DWORD: return 4;
  case SIZE_QWORD: return 8;
  }
  return 0;
}

static int vmmreg[] = {
  VCPU_REGS_RAX,
  VCPU_REGS_RCX,
  VCPU_REGS_RDX,
  VCPU_REGS_RBX,
  VCPU_REGS_RSP,
  VCPU_REGS_RBP,
  VCPU_REGS_RSI,
  VCPU_REGS_RDI,
  VCPU_REGS_R8,
  VCPU_REGS_R9,
  VCPU_REGS_R10,
  VCPU_REGS_R11,
  VCPU_REGS_R12,
  VCPU_REGS_R13,
  VCPU_REGS_R14,
  VCPU_REGS_R15,
};

static int
Vreg(int arg) {
  if ((arg & VAL_MASK) < 16)
    return vmmreg[arg & VAL_MASK];
  printf("error bad reg: %x\n", arg);
  return VCPU_REGS_RAX;
}

int
dodis(uint8_t *ib, struct insn *ix) {
  struct istate i = { 0 };
  struct opcode o;
  int a0, a1;

  i.pc = ib;
  i.mode = SIZE_QWORD;
  o = decodeop(&i);
  printf("%c%c dis: %.2x %.2x %.2x %.2x | %-6s", 
	(i.osz >> 16), (i.asz >> 16), i.seg, i.rep, i.rex, i.op, o.mnem);
  a0 = decodearg(&i, o.arg0); 
  a1 = decodearg(&i, o.arg1); 
  decodearg(&i, o.arg2); 
  printf(" : %d\n", i.nib);

  ix->incr = 0;
  if (strncmp(o.mnem, "mov", 3))
    return 0;
  if ((a0 & TYPE_MASK) == TYPE_REG) {
    ix->dir = VEI_DIR_IN;
    ix->size = sz(a0);
    ix->reg  = Vreg(a0);
    ix->incr = i.nib;
  }
  else if ((a1 & TYPE_MASK) == TYPE_REG) {
    ix->dir  = VEI_DIR_OUT;
    ix->size = sz(a1);
    ix->reg  = Vreg(a1);
    ix->incr = i.nib;
  }
  printf("dir:%d size:%d reg:%d incr:%d\n", ix->dir, ix->size, ix->reg, ix->incr);
  return 0;
}

