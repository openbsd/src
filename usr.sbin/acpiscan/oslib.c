/* $OpenBSD: oslib.c,v 1.1 2006/11/03 19:33:56 marco Exp $ */
/*
 * Copyright (c) 2006 Jordan Hargrave <jordan@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <oslib.h>

#define need_pciio   1
#define need_physmem 1
#define need_portio  1
//#define need_pthread 1

/*=====================================================================
 *
 *                          OpenBSD User Mode
 *
 *=====================================================================*/
#ifdef __OpenBSD__
void set_iopl(int x) 
{
	i386_iopl(x);
}
#endif

/*=====================================================================
 *
 *                          Solaris User Mode
 *
 *=====================================================================*/
#if defined(__sun__)
void set_iopl(int x)
{
	sysi86(SI86V86,V86SC_IOPL,0x3000);
}
#endif

/*=====================================================================
 *
 *                          UnixWare User Mode
 *
 *=====================================================================*/
#if defined(SCO)
void set_iopl(int x)
{
	sysi86(SI86IOPL, x);
}
#endif

/*=====================================================================
 *
 *                          Linux Kernel Mode
 *
 *=====================================================================*/
#if defined(linux) && defined(__KERNEL__)

#if (LINUX_KERNEL_VERSION >= 0x020400)
MODULE_LICENSE("GPL");
#endif

char kernel_version[] = UTS_RELEASE;

#define printf printk

#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

void ZeroMem(void *a, int c)
{
	uint8_t *b = (uint8_t *)a;
	while(c--) {
		*(b++) = 0;
	}
}

void CopyMem(void *d, const void *s, int c)
{
	uint8_t *a = (uint8_t *)d;
	uint8_t *b = (uint8_t *)s;

	while(c--) {
		*(a++) = *(b++);
	}
}

#undef need_pthread
#undef need_physmem
int physmemcpy(void *dest, uint32_t src, size_t len)
{
	memcpy(dest, phys_to_virt(src), len);
	return 0;
}

int physmemcmp(const void *dest, uint32_t src, size_t len)
{
	uint8_t buf[len];

	physmemcpy(buf, src, len);
	return memcmp(dest, buf, len);
}
#endif

/*=====================================================================
 *
 *                          Linux User Mode
 *
 *=====================================================================*/
#if defined(linux) && !defined(__KERNEL__)
void set_iopl(int x)
{
	iopl(x);
}

#define need_mmap 1

#undef need_pciio
int pci_read_n(int b, int d, int f, int reg, int len, void *buf)
{
	char dev[32];
	int  fd;

	memset(buf, 0, len);
	sprintf(dev,"/proc/bus/pci/%.02x/%.02x.%x", b,d,f);
	if ((fd = open(dev,O_RDWR)) >= 0) {
		pread(fd,buf,len,reg);
		close(fd);
		return 0;
	}
	return -1;
}
#endif

/*=====================================================================
 *
 *                          DOS 16-bit Mode
 *
 *=====================================================================*/
#ifdef MSDOS

/* Converts linear address to 16:16 pointer */
void FAR *lintoseg(uint32_t v);
#pragma aux lintoseg = "shl dx, 12" parm caller [dx ax] value [dx ax];

void set_iopl(int x)
{
}

#undef need_portio
uint8_t os_inb(uint16_t port)
{
	return inb(port);
}
uint16_t os_inw(uint16_t port)
{
	return inw(port);
}
uint32_t os_inl(uint16_t port)
{
	return inl(port);
}
void os_outb(uint16_t port, uint8_t val)
{
	outb(port, val);
}
void os_outw(uint16_t port, uint16_t val)
{
	outw(port, val);
}
void os_outl(uint16_t port, uint32_t val)
{
	outl(port, val);
}

#ifdef WATCOM
#pragma aux os_inb = "in al, dx" value [al] parms [dx];
#pragma aux os_inw = "in ax, dx" value [ax] parms [dx];
#pragma aux os_inl = \
		     "in   eax, dx" \
"mov  dx, ax" \
"shr  eax, 16", \
"xchg dx, ax" value [dx ax] parms [dx];

#pragma aux os_outb = "out dx, al" parms [dx] [al];
#pragma aux os_outw = "out dx, ax" parms [dx] [ax];
#pragma aux os_outl = \
		      "mov ax, cx" \
"shl eax, 16" \
"mov ax, bx" \
"out dx, eax" parm [dx] [cx bx] modifies [ax];
#endif

#undef need_physmem
int physmemcpy(void FAR *dest, uint32_t src, int len)
{
	_fmemcpy(dest, lintoseg(src), len);
	return 0;
}

int physmemcmp(const void FAR *dest, uint32_t src, int len)
{
	return _fmemcmp(dest, lintoseg(src), len);
}
#endif  /* end dos */

/*=====================================================================
 *
 *                          Common Physmem code
 *
 *=====================================================================*/
#ifdef need_physmem

#define PAGE_OFFSET(x) (uint64_t)((x) & ~PAGE_MASK)

static const char  *memfile = "/dev/mem";
static uint64_t  memoff;

int physmemcpy(void FAR *dest, uint32_t src, size_t len)
{
	int fd,rc;
	uint64_t memlimit;

	memset(dest, 0, len);
	if ((fd = open(memfile, O_RDONLY)) < 0) {
		printf("Can't open: %s\n", memfile);
		exit(0);
	}
#if 0
	memlimit = lseek(fd, 0, SEEK_END);
	if (src < (unsigned)memoff || src+len > memlimit + memoff) {
		close(fd);
		return -1;
	}
#endif
	src -= memoff;
#ifdef need_mmap
	{
		uint64_t  offset;
		void     *ptr;

		offset = PAGE_OFFSET(src);
		ptr = mmap(NULL, offset+len, PROT_READ, MAP_PRIVATE, fd, src-offset);
		if (ptr == (void *)-1L) {
			printf("Can't mmap @ 0x%.08lx! %d\n", src, errno);
		}
		else {
			memcpy(dest, ptr+offset, len);
			munmap(ptr, offset+len);
		}
	}
#else
	rc=pread(fd, dest, len, (off_t)(src - memoff));
	if (rc < 0) {
		printf("Can't pread64 @ 0x%.08x %d\n", src, errno);
	}
#endif
	close(fd);
	return 0;
}

int physmemcmp(const void FAR *dest, uint32_t src, size_t len)
{
	unsigned char bufr[len];

	physmemcpy(bufr, src, len);
	return memcmp(bufr, dest, len);
}
#endif

/*=====================================================================
 *
 *                          Common PCI Access code
 *
 *=====================================================================*/
#ifdef need_pciio
#define PCI_ADDR(b,d,f,r) (0x80000000+((b)<<16)+((d)<<10)+((f)<<8)+(r))
int pci_read_n(int b, int d, int f, int reg, int len, void *buf)
{
	uint8_t *ptr = (uint8_t *)buf;
	while(len--) {
		os_outl(0xCF8, PCI_ADDR(b,d,f,reg++));
		*(ptr++) = os_inb(0xCF8);
	}
}
#endif

/*=====================================================================
 *
 *                          Common Port code
 *
 *=====================================================================*/
#ifdef need_portio
uint8_t os_inb(uint16_t port)
{
	uint8_t v;

	__asm__ __volatile__ ("inb %w1,%b0" : "=a" (v) : "Nd" (port));
	return v;
}
uint16_t os_inw(uint16_t port)
{
	uint16_t v;

	__asm__ __volatile__ ("inw %w1,%w0" : "=a" (v) : "Nd" (port));
	return v;
}
uint32_t os_inl(uint16_t port)
{
	uint32_t v;

	__asm__ __volatile__ ("inl %w1,%0" : "=a" (v) : "Nd" (port));
	return v;
}
void os_outb(uint16_t port, uint8_t v)
{
	__asm__ __volatile__ ("outb %b0,%w1" : "=a" (v) : "Nd" (port));
}
void os_outw(uint16_t port, uint16_t v)
{
	__asm__ __volatile__ ("outw %w0,%w1" : "=a" (v) : "Nd" (port));
}
void os_outl(uint16_t port, uint32_t v)
{
	__asm__ __volatile__ ("outl %0,%w1" : "=a" (v) : "Nd" (port));
}
#endif


/*==================================================================
 * common pthread routines
 *==================================================================*/
#ifdef need_pthread
#include <pthread.h>

typedef int (*threadproc_t)(void *);

typedef struct
{
	pthread_t    tid;
	threadproc_t proc;
	void         *arg;
} osthread_t;

static void *os_threadhelper(void *arg)
{
	osthread_t *thrd = (osthread_t *)arg;

	thrd->proc(thrd->arg);
	return NULL;
}

osthread_t *os_create_thread(threadproc_t threadproc, void *arg)
{
	osthread_t *thrd;

	if ((thrd = (osthread_t *)malloc(sizeof(osthread_t))) == NULL) {
		return NULL;
	}
	thrd->proc = threadproc;
	thrd->arg  = arg;
	pthread_create(&thrd->tid, NULL, os_threadhelper, thrd);
	return thrd;
}

int os_destroy_thread(osthread_t *thrd)
{
	void *rcval;

	pthread_join(thrd->tid, &rcval);
	free(thrd);
}
#endif

/*====================================================================*
 * Common code
 *====================================================================*/
uint32_t scanmem(uint32_t src, uint32_t end, int step, int sz, 
		const void FAR *mem)
{
	while(src < end) {
		if (physmemcmp(mem, src, sz) == 0) {
			return src;
		}
		src += step;
	}
	return 0;
}

void set_physmemfile(const char *name, uint64_t offset)
{
#ifdef need_physmem
	memfile = name;
	memoff  = offset;
#endif
}

char dxc(char v, int t)
{
	if (v < ' ' || v > 'z' || t) {
		return '.';
	}
	return v;
}
uint8_t dxb(uint8_t v, int t)
{
	return (t) ? 0xFF : v;
}

void dump(void FAR *dest, int len)
{
	uint8_t FAR *b = (uint8_t FAR *)dest;
	int i,j;

	for(i=0;i<len;i+=16) {
		printf("%.06x: ", i);
		for(j=0;j<16;j++) {
			if (i+j >= len) {
				printf("-- ");
			}
			else {
				printf("%.02x ", dxb(b[i+j],i+j>=len));
			}
		}
		printf(" ");
		for(j=0;j<16;j++) {
			printf("%c", dxc(b[i+j],i+j>=len));
		}
		printf("\n");
	}
}
