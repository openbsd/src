#objdump: -dr
#name: sparc64 synth64

.*: +file format .*sparc.*

Disassembly of section .text:
0+0000 <foo-4> iprefetch  0+0004 <foo>
0+0004 <foo> signx  %g1, %g2
0+0008 <foo\+4> clruw  %g1, %g2
0+000c <foo\+8> cas  \[ %g1 \], %g2, %g3
0+0010 <foo\+c> casl  \[ %g1 \], %g2, %g3
0+0014 <foo\+10> casx  \[ %g1 \], %g2, %g3
0+0018 <foo\+14> casxl  \[ %g1 \], %g2, %g3
0+001c <foo\+18> clrx  \[ %g1 \+ %g2 \]
0+0020 <foo\+1c> clrx  \[ %g1 \]
0+0024 <foo\+20> clrx  \[ %g1 \+ 1 \]
0+0028 <foo\+24> clrx  \[ %g1 \+ 0x2a \]
0+002c <foo\+28> clrx  \[ 0x42 \]
