#objdump: -dr
#name: sparc64 rdpr

.*: +file format .*sparc.*

No symbols in .*
Disassembly of section .text:
0+0000 rdpr  %tpc, %g1
0+0004 rdpr  %tnpc, %g2
0+0008 rdpr  %tstate, %g3
0+000c rdpr  %tt, %g4
0+0010 rdpr  %tick, %g5
0+0014 rdpr  %tba, %g6
0+0018 rdpr  %pstate, %g7
0+001c rdpr  %tl, %o0
0+0020 rdpr  %pil, %o1
0+0024 rdpr  %cwp, %o2
0+0028 rdpr  %cansave, %o3
0+002c rdpr  %canrestore, %o4
0+0030 rdpr  %cleanwin, %o5
0+0034 rdpr  %otherwin, %sp
0+0038 rdpr  %wstate, %o7
0+003c rdpr  %fq, %l0
0+0040 rdpr  %ver, %l1
