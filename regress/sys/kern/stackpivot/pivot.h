#ifndef REGRESS_PIVOT_H
#define REGRESS_PIVOT_H

static void pivot(size_t *newstack) {
#if defined(__amd64__)
    asm("mov %0, %%rsp; retq;" ::"r"(newstack));
#elif defined(__i386__)
    asm("mov %0, %%esp; retl;" ::"r"(newstack));
#endif
}

#endif
