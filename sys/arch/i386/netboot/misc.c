/*	$NetBSD: misc.c,v 1.3 1994/10/27 04:21:18 cgd Exp $	*/

#include "proto.h"

#if defined(DEBUG)

void
DUMP_STRUCT(char *s, u_char *p, u_int ps) {
  int i;
  printf("struct %s (@0x%x %d bytes): ", s, p, ps);
  for (i=0; i<ps; i++)
    printf("%x ", *(p+i));
  printf("\n");
}

#else

void
DUMP_STRUCT(char *s, u_char *p, u_int ps) {
}


#endif


char *
strncpy(char *dst, const char *src, int len) {
  char *p=dst;
  while (*src && len--)
    *p++ = *src++;
  *p = 0;
  return dst;
}


int
strlen(const char *s) {
  int len = 0;
  while (*s++)
    len++;
  return len;
}


char *
strncat(char *s, const char *append, int len) {
  int offset = strlen(s);
  strncpy(s+offset, append, len);
  return s;
}


int
strcmp(const char *s, const char *t) {
  while (*s == *t++)
    if (*s++ == '\0')
      return 0;
  return *s - *--t;
}


int
bcmp(const void *p, const void *q, int len) {
  while (len--)
    if (*((const char *)p)++ != *((const char *)q)++)
      return 1;
  return 0;
}


void volatile exit(int v) {
#ifdef DEBUG
  L: goto L;
#else
  ExitToBios();
#endif
}


#define RTC_ADDR 0x70
#define RTC_DATA 0x71

#define RTC_SECONDS 0x00
#define RTC_MINUTES 0x02
#define RTC_HOURS 0x04
#define RTC_STATUS_A 0x0a
#define RTC_BASEMEM_LO 0x15
#define RTC_BASEMEM_HI 0x16
#define RTC_EXPMEM_LO 0x30
#define RTC_EXPMEM_HI 0x31

static u_char
ReadRtcRam(u_short addr) {
  for (;;) {
    /* wait if updating */
    outb(RTC_ADDR, RTC_STATUS_A);
    if (!(inb(RTC_DATA) & 0x80))
      break;
  }
  outb(RTC_ADDR, addr);
  return inb(RTC_DATA);
}

static void
getrtc(u_long *hrs, u_long *mins, u_long *secs) {
/* TBD - replace args with single arg - struct or sec of day */
  /*
   * Get real time clock values (they are in BCD)
   */
#ifdef USE_BIOS
  asm("
	call	_prot_to_real
	movb	$0x02,%ah		# read real time clock
	int	$0x1a
	.byte	0x66
	call	_real_to_prot

	xor	%eax, %eax
	movb	%dh, %al
	mov	%%eax, %0
	movb	%cl, %al
	mov	%eax, %1
	movb	%ch, %al
	mov	%eax, %2
	" : "=g" (secs), "=g" (mins), "=g" (hrs));
#else
  *secs = ReadRtcRam(RTC_SECONDS);
  *mins = ReadRtcRam(RTC_MINUTES);
  *hrs = ReadRtcRam(RTC_HOURS);
#endif
}

static inline u_long
bcd2dec(u_long arg) {
  return ((arg & 0xF0) >> 4) * 10 + (arg & 0x0F);
}


u_long
timer(void) {
/* TBD - replace with StartCountdown()/CountdownAtZero() routines,
   isolate the span-midnight problem to inside these routines
   */
  /*
   * Return the current time in seconds
   */

  u_long sec, min, hour;

  /* BIOS time is stored in bcd */
  getrtc(&hour, &min, &sec);
  sec = bcd2dec(sec);
  min = bcd2dec(min);
  hour = bcd2dec(hour);
#if 0
printe("time h%d m%d s%d = sum%d\n", hour, min, sec, hour * 3600L + min * 60L + sec);
#endif
  return hour * 3600L + min * 60L + sec;
}

/*
 * Simple random generator
 */
static u_long next = 1;

void
srand(u_int seed) {
  next = seed;
}

int
rand(void) {
  next = next * 1103515245L + 12345;
  return (u_int)(next / 65536) % 32768;
}

u_short
GetMemSize(u_short s) {
#ifdef USE_BIOS
  u_short result;
  asm("
	push	%%ebx
	mov	%1, %%ebx
	call	_prot_to_real
	cmpb	$0x1, %%bl
	.byte	0x66
	je	1f	
	sti
	int	$0x12
	cli
	.byte	0x66
	jmp	2f
1:	movb	$0x88, %%ah
	sti
	int	$0x15
	cli
2:	mov	%%eax, %%ebx
	.byte	0x66
	call	_real_to_prot
	mov	%%bx, %0
	pop	%%ebx
	" : "=g" (result) : "g" (s));
  return result;
#else
  u_long result;
  if (s)
    result = ((u_long)ReadRtcRam(RTC_EXPMEM_HI)<<8) + ReadRtcRam(RTC_EXPMEM_LO);
  else
    result = ((u_long)ReadRtcRam(RTC_BASEMEM_HI)<<8) + ReadRtcRam(RTC_BASEMEM_LO);
  return result;
#endif
}

void
ResetCpu(void) {
#ifdef USE_BIOS
  asm("
	call	_prot_to_real		# enter real mode
	int	$0x19
	" );
#else
  while (inb(0x64)&2);	/* wait input ready */
  outb(0x64, 0xFE);	/* Reset Command */
#endif
}
