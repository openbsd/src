/*	$NetBSD: proto.h,v 1.3 1994/10/27 04:21:24 cgd Exp $	*/

/*
 * TBD - need for common typedefs - rethink?
 * TBD - move config include into the source files - this is just expedient
 */

#include "config.h"
#include "nbtypes.h"

#define TRUE 1
#define FALSE 0
#define MDUMP 100 /* TBD - remove */
#define MAX_FILE_NAME_LEN 128
#define CRTBASE ((char *)0xb8000)
#define CHECKPOINT(x) (CRTBASE[0] = x)
#define nelt(x) (sizeof(x)/sizeof((x)[0]))

void ENTER(char *); /* remove TBD */
int IsKbdCharReady(void);
u_char GetKbdChar(void);
void HandleKbdAttn(void);
void KbdWait(int n);

int bcmp(const void *, const void *, int);
void volatile exit(int);
void volatile ExitToBios(void);
void gateA20(void);
int getc(void);
int getchar(void);
u_short GetMemSize(u_short);
int gets(char *);
int ischar(void);
void longjmp(jmp_buf env, int val);
void printf( /* const char *, ... */ );
void putc(int);
void putchar(int);
int rand(void);
void ResetCpu(void);
int setjmp(jmp_buf env);
void srand(u_int);
void StartProg(u_long phyaddr, u_long *args);
int strlen(const char *);
char *strncat(char *, const char *, int len);
char *strncpy(char *, const char *, int len);
int strcmp(const char *, const char *);
u_long timer(void);

/* macros in pio.h, rewritten as in-line functions */
/* TBD - define addr arg as long - short might result in extra
bit masking whereas longs simply get plonked down in %edx */
#undef inb
#undef outb
#undef inw
#undef outw
#undef inl
#undef outl

static inline u_char
inb(u_short addr) {
  u_char datum;
  asm volatile("inb %1, %0" : "=a" (datum) : "d" (addr));
  return datum;
}

static inline void
outb(u_short addr, u_char datum) {
  asm volatile("outb %0, %1" : : "a" (datum), "d" (addr));
}

static inline u_short
inw(u_short addr) {
  u_short datum;
  asm volatile(".byte 0x66; inl %1, %0" : "=a" (datum) : "d" (addr));
  return datum;
}

static inline void
outw(u_short addr, u_short datum) {
  asm volatile(".byte 0x66; outw %0, %1" : : "a" (datum), "d" (addr));
}

static inline u_long
inl(u_short addr) {
  u_long datum;
  asm volatile("inw %1, %0" : "=a" (datum) : "d" (addr));
  return datum;
}

static inline void
outl(u_short addr, u_long datum) {
  asm volatile("outw %0, %1" : : "a" (datum), "d" (addr));
}

#if __GCC__ >= 2
/* fast versions of bcopy(), bzero() */
static inline void
bcopy(const void *from, void *to, int len) {
  /* assumes %es == %ds */
  asm("
	mov	%0, %%esi
	mov	%1, %%edi
	mov	%2, %%ecx
	cld
	rep
	movsb
	" : : "g" (from), "g" (to), "g" (len) : "esi", "edi", "ecx");
}

static inline void
bzero(void *dest, int len) {
  /* assumes %es == %ds */
  asm("
	mov	%0, %%edi
	mov	%1, %%ecx
	xor	%%eax, %%eax
	cld
	rep
	stosb
	" : : "g" (dest), "g" (len) : "edi", "ecx", "eax");
}
#else

static inline void
bcopy(char *from, char *to, int len)
{
	while (len-- > 0)
		*to++ = *from++;
}

static inline void
bzero(char *to, int len)
{
	while (len-- > 0)
		*to++ = '\0';
}

#endif

static inline void PhysBcopy(u_long src, u_long dest, u_long nbytes) {
  bcopy((void *)src, (void *)dest, nbytes);
}

static inline void PhysBzero(u_long dest, u_long nbytes) {
  bzero((void *)dest, nbytes);
}
