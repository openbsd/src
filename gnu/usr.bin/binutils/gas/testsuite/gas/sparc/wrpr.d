#objdump: -dr
#name: sparc64 wrpr

.*: +file format .*sparc.*

No symbols in .*
Disassembly of section .text:
0+0000 wrpr  %g1, %tpc
0+0004 wrpr  %g2, %tnpc
0+0008 wrpr  %g3, %tstate
0+000c wrpr  %g4, %tt
0+0010 wrpr  %g5, %tick
0+0014 wrpr  %g6, %tba
0+0018 wrpr  %g7, %pstate
0+001c wrpr  %o0, %tl
0+0020 wrpr  %o1, %pil
0+0024 wrpr  %o2, %cwp
0+0028 wrpr  %o3, %cansave
0+002c wrpr  %o4, %canrestore
0+0030 wrpr  %o5, %cleanwin
0+0034 wrpr  %sp, %otherwin
0+0038 wrpr  %o7, %wstate
