--- arla-0.23/lwp/process.S~    Sun Mar 28 16:13:33 1999
+++ arla-0.23/lwp/process.S     Fri Aug 13 09:35:56 1999
@@ -207,7 +207,7 @@
 #
 */
 
-       .globl  _PRE_Block
+       .globl  PRE_Block
 
 topstack =     0
 
@@ -225,26 +225,26 @@
 area1  =       12
 newsp  =       16
 
-       .globl  _savecontext
-_savecontext:
-       movb    #1,_PRE_Block           | Dont allow any interrupt finagling
-       link    a6,#-(nregs*4)          | Save frame pointer & ...
+       .globl  savecontext
+savecontext:
+       movb    #1,PRE_Block            | Dont allow any interrupt finagling
+       link    %a6,#-(nregs*4)         | Save frame pointer & ...
                                        | ... allocate space for nregs registers
 /* Save registers */
-       moveml  #regs,sp@
+       moveml  #regs,%sp@
 
-       movl    a6@(area1),a0           | a0 = base of savearea
-       movl    sp,a0@(topstack)        | area->topstack = sp
-       movl    a6@(newsp),d0           | Get new sp
+       movl    %a6@(area1),%a0         | a0 = base of savearea
+       movl    %sp,%a0@(topstack)      | area->topstack = sp
+       movl    %a6@(newsp),%d0         | Get new sp
        jeq     forw1                   | If newsp == 0, no stack switch
-       movl    d0,sp                   | Switch to new stack
+       movl    %d0,%sp                 | Switch to new stack
 forw1:
-       movl    a6@(f),a0               | a0 = f
-       jbsr    a0@                     | f()
+       movl    %a6@(f),%a0             | a0 = f
+       jbsr    %a0@                    | f()
 
 /* It is impossible to be here, so abort() */
 
-       jbsr    _abort
+       jbsr    abort
 
 /*
 # returnto(area2)
@@ -254,18 +254,18 @@
 /* Stack offset of argument */
 area2  =       8
 
-       .globl _returnto
-_returnto:
-       link    a6,#0
-       movl    a6@(area2),a0           | Base of savearea
-       movl    a0@(topstack),sp        | Restore sp
+       .globl returnto
+returnto:
+       link    %a6,#0
+       movl    %a6@(area2),%a0         | Base of savearea
+       movl    %a0@(topstack),%sp      | Restore sp
 /* Restore registers */
-       moveml  sp@,#regs
+       moveml  %sp@,#regs
 
-       addl    #(nregs*4),sp
-       movl    sp,a6                   | Argghh...be careful here
-       unlk    a6
-       clrb    _PRE_Block
+       addl    #(nregs*4),%sp
+       movl    %sp,%a6                 | Argghh...be careful here
+       unlk    %a6
+       clrb    PRE_Block
        rts                             | Return to previous process
 #endif /* mc68000 */
        

