#objdump: -dr
#name: sparc64 asi

.*: +file format .*sparc.*

No symbols in .*
Disassembly of section .text:
0+0000 lda  \[ %g1 \] \(0\), %g2
0+0004 lda  \[ %g1 \] \(255\), %g2
0+0008 lda  \[ %g1 \] #ASI_AIUP, %g2
0+000c lda  \[ %g1 \] #ASI_AIUS, %g2
0+0010 lda  \[ %g1 \] #ASI_AIUP_L, %g2
0+0014 lda  \[ %g1 \] #ASI_AIUS_L, %g2
0+0018 lda  \[ %g1 \] #ASI_P, %g2
0+001c lda  \[ %g1 \] #ASI_S, %g2
0+0020 lda  \[ %g1 \] #ASI_PNF, %g2
0+0024 lda  \[ %g1 \] #ASI_SNF, %g2
0+0028 lda  \[ %g1 \] #ASI_P_L, %g2
0+002c lda  \[ %g1 \] #ASI_S_L, %g2
0+0030 lda  \[ %g1 \] #ASI_PNF_L, %g2
0+0034 lda  \[ %g1 \] #ASI_SNF_L, %g2
0+0038 lda  \[ %g1 \] #ASI_AIUP, %g2
0+003c lda  \[ %g1 \] #ASI_AIUS, %g2
0+0040 lda  \[ %g1 \] #ASI_AIUP_L, %g2
0+0044 lda  \[ %g1 \] #ASI_AIUS_L, %g2
0+0048 lda  \[ %g1 \] #ASI_P, %g2
0+004c lda  \[ %g1 \] #ASI_S, %g2
0+0050 lda  \[ %g1 \] #ASI_PNF, %g2
0+0054 lda  \[ %g1 \] #ASI_SNF, %g2
0+0058 lda  \[ %g1 \] #ASI_P_L, %g2
0+005c lda  \[ %g1 \] #ASI_S_L, %g2
0+0060 lda  \[ %g1 \] #ASI_PNF_L, %g2
0+0064 lda  \[ %g1 \] #ASI_SNF_L, %g2
