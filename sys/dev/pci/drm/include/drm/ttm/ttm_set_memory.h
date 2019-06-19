/**************************************************************************
 *
 * Copyright (c) 2018 Advanced Micro Devices, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Huang Rui <ray.huang@amd.com>
 */

#ifndef TTM_SET_MEMORY
#define TTM_SET_MEMORY

#include <linux/mm.h>

#if defined(__amd64__) || defined(__i386__)
#define CONFIG_X86
#endif

#ifdef CONFIG_X86

#include <asm/set_memory.h>


static inline int ttm_set_pages_array_wb(struct vm_page **pages, int addrinarray)
{
	return set_pages_array_wb(pages, addrinarray);
}

static inline int ttm_set_pages_array_wc(struct vm_page **pages, int addrinarray)
{
	return set_pages_array_wc(pages, addrinarray);
}

static inline int ttm_set_pages_array_uc(struct vm_page **pages, int addrinarray)
{
	return set_pages_array_uc(pages, addrinarray);
}

static inline int ttm_set_pages_wb(struct vm_page *page, int numpages)
{
	return set_pages_wb(page, numpages);
}

static inline int ttm_set_pages_wc(struct vm_page *page, int numpages)
{
#ifdef __linux__
	unsigned long addr = (unsigned long)page_address(page);

	return set_memory_wc(addr, numpages);
#else
	KASSERT(numpages == 1);
	atomic_setbits_int(&page->pg_flags, PG_PMAP_WC);
	return 0;
#endif
}

static inline int ttm_set_pages_uc(struct vm_page *page, int numpages)
{
	return set_pages_uc(page, numpages);
}

#else /* for CONFIG_X86 */

#if IS_ENABLED(CONFIG_AGP) && defined(__linux__)

#include <asm/agp.h>

static inline int ttm_set_pages_array_wb(struct vm_page **pages, int addrinarray)
{
	int i;

	for (i = 0; i < addrinarray; i++)
		unmap_page_from_agp(pages[i]);
	return 0;
}

static inline int ttm_set_pages_array_wc(struct vm_page **pages, int addrinarray)
{
	int i;

	for (i = 0; i < addrinarray; i++)
		map_page_into_agp(pages[i]);
	return 0;
}

static inline int ttm_set_pages_array_uc(struct vm_page **pages, int addrinarray)
{
	int i;

	for (i = 0; i < addrinarray; i++)
		map_page_into_agp(pages[i]);
	return 0;
}

static inline int ttm_set_pages_wb(struct vm_page *page, int numpages)
{
	int i;

	for (i = 0; i < numpages; i++)
		unmap_page_from_agp(page++);
	return 0;
}

#else /* for CONFIG_AGP */

static inline int ttm_set_pages_array_wb(struct vm_page **pages, int addrinarray)
{
	return 0;
}

static inline int ttm_set_pages_array_wc(struct vm_page **pages, int addrinarray)
{
	return 0;
}

static inline int ttm_set_pages_array_uc(struct vm_page **pages, int addrinarray)
{
	return 0;
}

static inline int ttm_set_pages_wb(struct vm_page *page, int numpages)
{
	return 0;
}

#endif /* for CONFIG_AGP */

static inline int ttm_set_pages_wc(struct vm_page *page, int numpages)
{
	return 0;
}

static inline int ttm_set_pages_uc(struct vm_page *page, int numpages)
{
	return 0;
}

#endif /* for CONFIG_X86 */

#endif
