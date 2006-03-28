/* Workaround for CR JAGab60546 setjmp/longjmp and
   JAGad55982 sigsetjmp/siglongjmp from shared libraries. */

/*
  * tabstop=4
  *
  * _setjmp/setjmp/sigsetjmp and
  *_longjmp/longjmp/siglongjmp.
  *
  * Written by Mark Klein, 10 October, 2000
  * Updated for gcc 3.x 6 October, 2005
  *
  * These routines are GCC specific and MUST BE COMPILED
  * WITH -O2
  *
  * The existing setjmp/longjmp code in both libc.a and XL.PUB.SYS
  * are not SR4 aware and cause problems when working with shared
  * libraries (XLs), especially when executing a longjmp between
  * XLs. This code preserves SR4 and will successfully handle
  * a cross space longjmp. However, the setjmp code must be
  * bound into each XL from which it will be called as well as
  * being bound into the main program.
  */

/*
  * The following macro takes the contents of the jmpbuf and
  * restores the registers from them. There is other code
  * elsewhere that ensures that __jmpbuf is %r26 at this
  * point in time. If it becomes some other register, that
  * register must be the last restored. At the end will
  * be a branch external that will cause a cross space
  * return if needed.
  */
#define RESTORE_REGS_AND_RETURN(__jmpbuf, __retval)                  \
({                                                                   \
         __asm__ __volatile__ (                                      \
             "   ldw    0(%%sr0, %0), %%rp\n"                        \
             "\t   ldw    4(%%sr0, %0), %%sp\n"                      \
             "\t   ldw   16(%%sr0, %0), %%r3\n"                      \
             "\t   ldw   20(%%sr0, %0), %%r4\n"                      \
             "\t   ldw   24(%%sr0, %0), %%r5\n"                      \
             "\t   ldw   28(%%sr0, %0), %%r6\n"                      \
             "\t   ldw   32(%%sr0, %0), %%r7\n"                      \
             "\t   ldw   36(%%sr0, %0), %%r8\n"                      \
             "\t   ldw   40(%%sr0, %0), %%r9\n"                      \
             "\t   ldw   44(%%sr0, %0), %%r10\n"                     \
             "\t   ldw   48(%%sr0, %0), %%r11\n"                     \
             "\t   ldw   52(%%sr0, %0), %%r12\n"                     \
             "\t   ldw   56(%%sr0, %0), %%r13\n"                     \
             "\t   ldw   60(%%sr0, %0), %%r14\n"                     \
             "\t   ldw   64(%%sr0, %0), %%r15\n"                     \
             "\t   ldw   68(%%sr0, %0), %%r16\n"                     \
             "\t   ldw   72(%%sr0, %0), %%r17\n"                     \
             "\t   ldw   76(%%sr0, %0), %%r18\n"                     \
             "\t   ldw   80(%%sr0, %0), %%r19\n"                     \
             "\t   ldw   84(%%sr0, %0), %%r20\n"                     \
             "\t   ldw   88(%%sr0, %0), %%r21\n"                     \
             "\t   ldw   92(%%sr0, %0), %%r22\n"                     \
             "\t   ldw   96(%%sr0, %0), %%r23\n"                     \
             "\t   ldw  100(%%sr0, %0), %%r24\n"                     \
             "\t   ldw  104(%%sr0, %0), %%r25\n"                     \
             "\t   ldw  112(%%sr0, %0), %%r27\n"                     \
             "\t   ldw  116(%%sr0, %0), %%r1\n"                      \
             "\t   mtsp %%r1, %%sr3\n"                               \
             "\t   ldw  120(%%sr0, %0), %%r1\n"                      \
             "\t   mtsp %%r1, %%sr1\n"                               \
             "\t   or,<>   %%r0, %1, %%r0\n"                         \
             "\t     ldi 1, %%r28\n"                                 \
             "\t   ldw  108(%%sr0, %0), %%r26\n"                     \
             "\t   be       0(%%sr1, %%rp)\n"                        \
             "\t   mtsp %%r1, %%sr4\n"                               \
                 : \
                 : "r" (__jmpbuf),                                   \
                   "r" (__retval));                                  \
})

/*
  * The following macro extracts the signal mask
  * from  __jmpbuf from the 3rd and 4th words and
  * if non-zero, calls sigprocmask with that value
  * to set the signal mask. This macro is usually
  * invoked before the registers are restored in
  * the longjmp routines and it can clobber things
  * without needing to spill them as a result.
  * A quick frame is built before making the
  * call and cut back just afterwards.
  * The ldi 2, %r26 is actually SIG_SETMASK from
  * /usr/include/signal.h.
  */
#define RESTORE_SIGNAL_MASK(__jmpbuf)                                \
({                                                                   \
   __asm__ __volatile__ (                                            \
              "  ldw 8(%0), %%r26\n"                                 \
              "\t  comibt,=,n 0,%%r26,.+36\n"                        \
              "\t    ldo 64(%%sp), %%sp\n"                           \
              "\t    stw %0, -28(%%sp)\n"                            \
              "\t    ldi 0, %%r24\n"                                 \
              "\t    ldo 8(%0), %%r25\n"                             \
              "\t    .import sigprocmask,code\n"                     \
              "\t    bl sigprocmask,%%rp\n"                          \
              "\t    ldi 2, %%r26\n"                                 \
              "\t    ldw -28(%%sr0, %%sp), %0\n"                     \
              "\t    ldo -64(%%sp), %%sp\n"                          \
                     :                                               \
                     : "r" (__jmpbuf));                              \
})

/*
  * This macro saves the current contents of the
  * registers to __jmpbuf. Note that __jmpbuf is
  * guaranteed elsewhere to be in %r26. We do not
  * want it spilled, nor do we want a new frame
  * built.
  */
#define SAVE_REGS(__jmpbuf)                                          \
({                                                                   \
   __asm__ __volatile__ (                                            \
              "  stw %%rp,     0(%%sr0, %0)\n"                       \
              "\t  stw %%sp,     4(%%sr0, %0)\n"                     \
              "\t  stw %%r0,     8(%%sr0, %0)\n"                     \
              "\t  stw %%r3,    16(%%sr0, %0)\n"                     \
              "\t  stw %%r4,    20(%%sr0, %0)\n"                     \
              "\t  stw %%r5,    24(%%sr0, %0)\n"                     \
              "\t  stw %%r6,    28(%%sr0, %0)\n"                     \
              "\t  stw %%r7,    32(%%sr0, %0)\n"                     \
              "\t  stw %%r8,    36(%%sr0, %0)\n"                     \
              "\t  stw %%r9,    40(%%sr0, %0)\n"                     \
              "\t  stw %%r10,   44(%%sr0, %0)\n"                     \
              "\t  stw %%r11,   48(%%sr0, %0)\n"                     \
              "\t  stw %%r12,   52(%%sr0, %0)\n"                     \
              "\t  stw %%r13,   56(%%sr0, %0)\n"                     \
              "\t  stw %%r14,   60(%%sr0, %0)\n"                     \
              "\t  stw %%r15,   64(%%sr0, %0)\n"                     \
              "\t  stw %%r16,   68(%%sr0, %0)\n"                     \
              "\t  stw %%r17,   72(%%sr0, %0)\n"                     \
              "\t  stw %%r18,   76(%%sr0, %0)\n"                     \
              "\t  stw %%r19,   80(%%sr0, %0)\n"                     \
              "\t  stw %%r20,   84(%%sr0, %0)\n"                     \
              "\t  stw %%r21,   88(%%sr0, %0)\n"                     \
              "\t  stw %%r22,   92(%%sr0, %0)\n"                     \
              "\t  stw %%r23,   96(%%sr0, %0)\n"                     \
              "\t  stw %%r24,  100(%%sr0, %0)\n"                     \
              "\t  stw %%r25,  104(%%sr0, %0)\n"                     \
              "\t  stw %%r26,  108(%%sr0, %0)\n"                     \
              "\t  stw %%r27,  112(%%sr0, %0)\n"                     \
              "\t  mfsp %%sr3, %%r1\n"                               \
              "\t  stw %%r1,   116(%%sr0, %0)\n"                     \
              "\t  mfsp %%sr4, %%r1\n"                               \
              "\t  stw %%r1,   120(%%sr0, %0)\n"                     \
                   :                                                 \
                   : "r" (__jmpbuf));                                \
})

/*
  * This macro will save the signal mask to the
  * __jmpbuf if __savemask is non-zero. By this
  * point in time, the other resisters have been
  * saved into the __jmpbuf.
  * The ldi 0, %r26 is actually SIG_BLOCK from
  * /usr/include/signal.h. Since the block is
  * an OR of the bits, this does not change the
  * mask, but returns it into the double word at
  * the address in %r24.
  */
#define SAVE_SIGNAL_MASK(__jmpbuf,__savemask)                        \
({                                                                   \
   __asm__ __volatile__ (                                            \
              "  comibt,=,n 0,%1,.+36\n"                             \
              "\t    stw %%rp, -20(%%sr0, %%sp)\n"                   \
              "\t    ldo 64(%%sp), %%sp\n"                           \
              "\t    ldo 8(%0), %%r24\n"                             \
              "\t    ldi 0, %%r25\n"                                 \
              "\t    .import sigprocmask,code\n"                     \
              "\t    bl sigprocmask,%%rp\n"                          \
              "\t    ldi 0, %%r26\n"                                 \
              "\t    ldo -64(%%sp), %%sp\n"                          \
              "\t    ldw -20(%%sr0, %%sp), %%rp\n"                   \
                     :                                               \
                     : "r" (__jmpbuf),                               \
                       "r" (__savemask));                            \
})

/*
  * Construct a jump buffer and unconditionally save
  * the signal mask. Return a 0 unconditinoally.
  * Care is taken here and in the macros to assume
  * the __jumpbuf is in %r26 and that the return
  * value will be in %r28. It is done this way to
  * prevent a frame from being built and any registers
  * from being spilled.
  */
int setjmp(register void *jmpbuf)
{
   register int __jmpbuf asm ("%r26");

   SAVE_REGS(__jmpbuf);
   SAVE_SIGNAL_MASK(__jmpbuf, 1);
   return 0;
}

/*
  * Construct a jump buffer but do not save the
  * signal mask.
  */
int _setjmp(register void *jmpbuf)
{
   register int __jmpbuf asm ("%r26");

   SAVE_REGS(__jmpbuf);
   return 0;
}

/*
  * Construct a jump buffer and conditionally save
  * the signal mask. The mask is saved if the
  * savemask parameter is non-zero.
  */
int sigsetjmp(register void *jmpbuf, register int savemask)
{
   register int __jmpbuf   asm ("%r26");
   register int __savemask asm ("%r25");

   SAVE_REGS(__jmpbuf);
   SAVE_SIGNAL_MASK(__jmpbuf, __savemask);
   return 0;
}

/*
  * Return to the location established in the jmpbuf,
  * and place the value in i2 in %r28. Registers
  * %r4 and %r5 are co-opted to save the address and
  * value of jmpbuf and the return value. The signal
  * mask is re-established if needed, then the
  * address of jmpbuf and value of retval are placed
  * into %r26 and %r28 correspondingly. This routine
  * will never return to its caller and the stack
  * will be cut back to whatever exists in the jmpbuf.
  */
void longjmp(register void *jmpbuf, register int i2)
{
   register int __jmpbuf        asm ("%r26");
   register int __retval        asm ("%r28");

   __asm__ __volatile__ (
              "  copy %0, %%r4\n"
              "\t  copy %1, %%r5\n"
                     :
                     : "r" (jmpbuf),
                       "r" (i2));

   RESTORE_SIGNAL_MASK (__jmpbuf);

   __asm__ __volatile__ (
              "  copy %%r4, %0\n"
              "\t  copy %%r5, %1\n"
                     : "=r" (__jmpbuf),
                       "=r" (__retval));

   RESTORE_REGS_AND_RETURN (__jmpbuf, __retval);
}

/*
  * Return to the location established in the jmpbuf,
  * but do not restore the signal mask.
  */
void _longjmp(register void *jmpbuf, register int i2)
{
   register int __retval         asm ("%r28");
   register int __jmpbuf         asm ("%r26");

   __jmpbuf = (int)jmpbuf;
   __retval = i2;

   RESTORE_REGS_AND_RETURN (__jmpbuf, __retval);
}

/*
  * Return to the location establsihed in the jmpbuf,
  * and conditionally re-establish the signal mask.
  */
void siglongjmp(register void *jmpbuf, register int i2)
{
   register int __jmpbuf        asm ("%r26");
   register int __retval        asm ("%r28");

   __asm__ __volatile__ (
              "  copy %0, %%r4\n"
              "\t  copy %1, %%r5\n"
                     :
                     : "r" (jmpbuf),
                       "r" (i2));

   RESTORE_SIGNAL_MASK (__jmpbuf);

   __asm__ __volatile__ (
              "  copy %%r4, %0\n"
              "\t  copy %%r5, %1\n"
                     : "=r" (__jmpbuf),
                       "=r" (__retval));

   RESTORE_REGS_AND_RETURN (__jmpbuf, __retval);
}

#ifdef TEST
int buf1[50];
int buf2[50];

foo() {
   printf("In routine foo(). Doing Longjmp.\n");
   longjmp(buf1, 123);
   printf("This is in foo after the longjmp() call. Should not reach here.\n");
}

bar(int ret) {
   printf("In routine bar(%d). Doing siglongjmp.\n",ret);
   siglongjmp(buf2, ret);
   printf("This is in bar after the siglongjmp() call. Should not reach here.\n");
}

main() {
   int i;
   if ((i = setjmp(buf1)))
     {
           printf("This is the return from the longjmp. i: %d\n",i);
         }
   else
     {
           printf("Jump buffer established, i: %d. Calling foo()\n",i);
           foo();
           printf("This is in main after the foo() call. Should not reach here.\n ");
         }

   if ((i = sigsetjmp(buf2,0)))
     {
           printf("This is the return from the longjmp. i: %d\n",i);
         }
   else
     {
           printf("Jump buffer established, i: %d. Calling bar(456)\n",i);
           bar(456);
           printf("This is in main after the bar(456) call. Should not reach here.\n");
         }

   if ((i = sigsetjmp(buf2,1)))
     {
           printf("This is the return from the longjmp. i: %d\n",i);
         }
   else
     {
           printf("Jump buffer established, i: %d. Calling bar(789)\n",i);
           bar(789);
           printf("This is in main after the bar(789) call. Should not reach here.\n");
         }
}
#endif
