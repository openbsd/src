#objdump: -dr
#name: sparc64 prefetch

.*: +file format .*sparc.*

No symbols in .*
Disassembly of section .text:
0+0000 prefetch  \[ %g1 \], #n_reads
0+0004 prefetch  \[ %g1 \], 31
0+0008 prefetch  \[ %g1 \], #n_reads
0+000c prefetch  \[ %g1 \], #one_read
0+0010 prefetch  \[ %g1 \], #n_writes
0+0014 prefetch  \[ %g1 \], #one_write
0+0018 prefetcha  \[ %g1 \] #ASI_AIUP, #n_reads
0+001c prefetcha  \[ %g1 \] %asi, 31
0+0020 prefetcha  \[ %g1 \] #ASI_AIUS, #n_reads
0+0024 prefetcha  \[ %g1 \] %asi, #one_read
