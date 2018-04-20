#ifndef REGRESS_PIVOT_H
#define REGRESS_PIVOT_H

/* Set the stack pointer to the memory pointed to by newstack and
 * return to the first address in that memory. The stack pointer
 * must point to the next address in newstack after return. */
void pivot(size_t *newstack)
{
#if defined(__amd64__)
	__asm volatile("mov %0, %%rsp; retq;"
	    ::"r"(newstack));
#elif defined(__i386__)
	__asm volatile("mov %0, %%esp; retl;"
	    ::"r"(newstack));
#elif defined(__arm__)
	__asm volatile("mov sp, %0; ldmfd sp!, {lr}; mov pc, lr"
	    ::"r"(newstack));
#elif defined(__aarch64__)
	__asm volatile("mov sp, %0; ldr x30, [sp]; add sp, sp, #16; ret"
	    :"+r"(newstack));
#elif defined(__alpha__)
	__asm volatile("mov %0,$sp\n ret $31,($26),1"
	    :"+r"(newstack));
#elif defined(__mips64__)
	__asm volatile("move $sp, %0\n ld $ra, ($sp)\n addiu $sp, $sp, 8\n jr $ra"
	    :"+r"(newstack));
#elif defined(__hppa__)
	__asm volatile("copy %0,%%r30\n\tldwm 4(%%r30),%%r2\n\tbv %%r0(%%r2)\n\tnop"
	    ::"r"(newstack));
#elif defined(__powerpc__)
	__asm volatile("li 1, %0; lwz 0, 0(1); addi 1, 4, 4; mtlr 0; blr"
	    :"+r"(newstack));
#elif defined(__m88k__)
	__asm volatile("ldcr %%r31, %0; ld %%r1, %%r31, 0; addu %%r31, %%r31, 4; jmp %%r1"
	    ::"r"(newstack));
#elif defined(__sh__)
	__asm volatile("mov %0, sp\n\trts\n\tnop"
	    :"+r"(newstack));
#elif defined(__sparc64__)
	__asm volatile("mov %0, %%fp; ldx [%%fp+0], %%i7; ret"
	    ::"r"(newstack));
#endif
}

#endif
