/*      $OpenBSD: codepatch.c,v 1.1 2015/01/16 10:17:51 sf Exp $    */
/*
 * Copyright (c) 2014-2015 Stefan Fritsch <sf@sfritsch.de>
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

#include <sys/param.h>
#include <machine/codepatch.h>
#include <uvm/uvm_extern.h>

#ifdef CODEPATCH_DEBUG
#define DBGPRINT(fmt, args...)	printf("%s: " fmt "\n", __func__, ## args)
#else
#define DBGPRINT(fmt, args...)	do {} while (0)
#endif

struct codepatch {
	uint32_t offset;
	uint16_t len;
	uint16_t tag;
};

extern struct codepatch codepatch_begin;
extern struct codepatch codepatch_end;

#define NOP_LEN_MAX	9

static const unsigned char nops[][NOP_LEN_MAX] = {
	{ 0x90 },
	{ 0x66, 0x90 },
	{ 0x0F, 0x1F, 0x00 },
	{ 0x0F, 0x1F, 0x40, 0x00 },
	{ 0x0F, 0x1F, 0x44, 0x00, 0x00 },
	{ 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 },
	{ 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00 },
	{ 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00},
};

void
codepatch_fill_nop(void *caddr, uint16_t len)
{
	unsigned char *addr = caddr;
	uint16_t nop_len;

	while (len > 0) {
		if (len <= NOP_LEN_MAX)
			nop_len = len;
		else
			nop_len = NOP_LEN_MAX;
		memcpy(addr, nops[nop_len-1], nop_len);
		addr += nop_len;
		len -= nop_len;
	}
}

/*
 * Create writeable aliases of memory we need
 * to write to as kernel is mapped read-only
 */
void *codepatch_maprw(vaddr_t *nva, vaddr_t dest)
{
	paddr_t kva = trunc_page((paddr_t)dest);
	paddr_t po = (paddr_t)dest & PAGE_MASK;
	paddr_t pa1, pa2;

	if (*nva == 0)
		*nva = (vaddr_t)km_alloc(2 * PAGE_SIZE, &kv_any, &kp_none,
					&kd_waitok);

	pmap_extract(pmap_kernel(), kva, &pa1);
	pmap_extract(pmap_kernel(), kva + PAGE_SIZE, &pa2);
	pmap_kenter_pa(*nva, pa1, PROT_READ | PROT_WRITE);
	pmap_kenter_pa(*nva + PAGE_SIZE, pa2, PROT_READ | PROT_WRITE);
	pmap_update(pmap_kernel());

	return (void *)(*nva + po);
}

void codepatch_unmaprw(vaddr_t nva)
{
	if (nva != 0)
		km_free((void *)nva, 2 * PAGE_SIZE, &kv_any, &kp_none);
}

/* Patch with NOPs */
void
codepatch_nop(uint16_t tag)
{
	struct codepatch *patch;
	unsigned char *rwaddr;
	vaddr_t addr, rwmap = 0;
	int i = 0;

	DBGPRINT("patching tag %u", tag);

	for (patch = &codepatch_begin; patch < &codepatch_end; patch++) {
		if (patch->tag != tag)
			continue;
		addr = KERNBASE + patch->offset;
		rwaddr = codepatch_maprw(&rwmap, addr);
		codepatch_fill_nop(rwaddr, patch->len);
		i++;
	}
	codepatch_unmaprw(rwmap);
	DBGPRINT("patched %d places", i);
}

/* Patch with alternative code */
void
codepatch_replace(uint16_t tag, void *code, size_t len)
{
	struct codepatch *patch;
	unsigned char *rwaddr;
	vaddr_t addr, rwmap = 0;
	int i = 0;

	DBGPRINT("patching tag %u with %p", tag, code);

	for (patch = &codepatch_begin; patch < &codepatch_end; patch++) {
		if (patch->tag != tag)
			continue;
		addr = KERNBASE + patch->offset;

		if (len > patch->len) {
			panic("%s: can't replace len %u with %zu at %#lx",
			    __func__, patch->len, len, addr);
		}
		rwaddr = codepatch_maprw(&rwmap, addr);
		memcpy(rwaddr, code, len);
		codepatch_fill_nop(rwaddr + len, patch->len - len);
		i++;
	}
	codepatch_unmaprw(rwmap);
	DBGPRINT("patched %d places", i);
}

/* Patch with calls to func */
void
codepatch_call(uint16_t tag, void *func)
{
	struct codepatch *patch;
	unsigned char *rwaddr;
	int32_t offset;
	int i = 0;
	vaddr_t addr, rwmap = 0;

	DBGPRINT("patching tag %u with call %p", tag, func);

	for (patch = &codepatch_begin; patch < &codepatch_end; patch++) {
		if (patch->tag != tag)
			continue;
		addr = KERNBASE + patch->offset;
		if (patch->len < 5)
			panic("%s: can't replace len %u with call at %#lx",
			    __func__, patch->len, addr);

		offset = (vaddr_t)func - (addr + 5);
		rwaddr = codepatch_maprw(&rwmap, addr);
		rwaddr[0] = 0xe8; /* call near */
		memcpy(rwaddr + 1, &offset, sizeof(offset));
		codepatch_fill_nop(rwaddr + 5, patch->len - 5);
		i++;
	}
	codepatch_unmaprw(rwmap);
	DBGPRINT("patched %d places", i);
}
