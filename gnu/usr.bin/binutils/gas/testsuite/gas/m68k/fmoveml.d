#objdump: -d
#name: fmoveml

# Test handling of fmoveml and fmovemx instructions.

.*: +file format .*

Disassembly of section .text:
0+000 <foo> fmovel %fpcr,%a0@
0+004 <foo\+4> fmovel %fpsr,%a0@
0+008 <foo\+8> fmovel %fpiar,%a0@
0+00c <foo\+c> fmoveml %fpsr/%fpcr,%a0@
0+010 <foo\+10> fmoveml %fpiar/%fpcr,%a0@
0+014 <foo\+14> fmoveml %fpiar/%fpsr,%a0@
0+018 <foo\+18> fmoveml %fpiar/%fpsr/%fpcr,%a0@
0+01c <foo\+1c> fmovel %fpcr,%d0
0+020 <foo\+20> fmovel %fpsr,%d0
0+024 <foo\+24> fmovel %fpiar,%d0
0+028 <foo\+28> fmovel %fpiar,%a0
0+02c <foo\+2c> fmovel %a0@,%fpcr
0+030 <foo\+30> fmovel %a0@,%fpsr
0+034 <foo\+34> fmovel %a0@,%fpiar
0+038 <foo\+38> fmoveml %a0@,%fpsr/%fpcr
0+03c <foo\+3c> fmoveml %a0@,%fpiar/%fpcr
0+040 <foo\+40> fmoveml %a0@,%fpiar/%fpsr
0+044 <foo\+44> fmoveml %a0@,%fpiar/%fpsr/%fpcr
0+048 <foo\+48> fmovel %d0,%fpcr
0+04c <foo\+4c> fmovel %d0,%fpsr
0+050 <foo\+50> fmovel %d0,%fpiar
0+054 <foo\+54> fmovel %a0,%fpiar
0+058 <foo\+58> fmovel #1,%fpcr
0+060 <foo\+60> fmovel #1,%fpsr
0+068 <foo\+68> fmovel #1,%fpiar
0+070 <foo\+70> fmoveml #1,%fpsr/%fpcr
0+078 <foo\+78> fmoveml #1,%fpiar/%fpcr
0+080 <foo\+80> fmoveml #1,%fpiar/%fpsr
0+088 <foo\+88> fmoveml #1,%fpiar/%fpsr/%fpcr
0+090 <foo\+90> fmovemx %fp1,%a0@
0+094 <foo\+94> fmovemx %fp4,%a0@
0+098 <foo\+98> fmovemx %fp7,%a0@
0+09c <foo\+9c> fmovemx %fp1/%fp3,%a0@
0+0a0 <foo\+a0> fmovemx %fp1-%fp4,%a0@
0+0a4 <foo\+a4> fmovemx %fp0/%fp7,%a0@
0+0a8 <foo\+a8> fmovemx %fp0-%fp7,%a0@
0+0ac <foo\+ac> fmovemx %a0@,%fp0
0+0b0 <foo\+b0> fmovemx %a0@,%fp1
0+0b4 <foo\+b4> fmovemx %a0@,%fp7
0+0b8 <foo\+b8> fmovemx %a0@,%fp0/%fp3
0+0bc <foo\+bc> fmovemx %a0@,%fp0/%fp4
0+0c0 <foo\+c0> fmovemx %a0@,%fp2-%fp4
0+0c4 <foo\+c4> fmovemx %a0@,%fp1-%fp7
0+0c8 <foo\+c8> fmovemx %fp0,%a0@-
0+0cc <foo\+cc> fmovemx %fp0-%fp7,%a0@-
0+0d0 <foo\+d0> fmovemx %fp0/%fp4,%a0@-
0+0d4 <foo\+d4> fmovemx %a0@\+,%fp7
0+0d8 <foo\+d8> fmovemx %a0@\+,%fp0-%fp7
0+0dc <foo\+dc> fmovemx %a0@\+,%fp3/%fp7
0+0e0 <foo\+e0> fmovemx %d0,%a0@-
0+0e4 <foo\+e4> fmovemx %a0@\+,%d0
0+0e8 <foo\+e8> fmovemx %fp1/%fp5,%a0@-
