#objdump: -d
#name: fmoveml

# Test handling of fmoveml instruction.

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
