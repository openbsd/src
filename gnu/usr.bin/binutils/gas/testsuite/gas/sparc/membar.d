#objdump: -dr
#name: sparc64 membar

.*: +file format .*sparc.*

No symbols in .*
Disassembly of section .text:
0+0000 membar  0
0+0004 membar  #Sync|#MemIssue|#Lookaside|#StoreStore|#LoadStore|#StoreLoad|#LoadLoad
0+0008 membar  #Sync|#MemIssue|#Lookaside|#StoreStore|#LoadStore|#StoreLoad|#LoadLoad
0+000c membar  #Sync
0+0010 membar  #MemIssue
0+0014 membar  #Lookaside
0+0018 membar  #StoreStore
0+001c membar  #LoadStore
0+0020 membar  #StoreLoad
0+0024 membar  #LoadLoad
