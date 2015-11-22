
/*
 * Copyright (c) 2015 Mike Larkin <mlarkin@openbsd.org>
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

#ifndef __VMD_H__
#define __VMD_H__

#define VMD_USER		"_vmd"
#define SOCKET_NAME		"/var/run/vmd.sock"
#define VMM_NODE		"/dev/vmm"
#define VM_NAME_MAX		256

/* #define VMD_DEBUG */

#ifdef VMD_DEBUG
#define dprintf(x...)   do { fprintf(stderr, x); } while(0)
#else
#define dprintf(x...)
#endif /* VMM_DEBUG */


enum imsg_type {
        IMSG_NONE,
	IMSG_VMDOP_DISABLE_VMM_REQUEST,
	IMSG_VMDOP_DISABLE_VMM_RESPONSE,
	IMSG_VMDOP_ENABLE_VMM_REQUEST,
	IMSG_VMDOP_ENABLE_VMM_RESPONSE,
        IMSG_VMDOP_START_VM_REQUEST,
	IMSG_VMDOP_START_VM_RESPONSE,
        IMSG_VMDOP_TERMINATE_VM_REQUEST,
	IMSG_VMDOP_TERMINATE_VM_RESPONSE,
	IMSG_VMDOP_GET_INFO_VM_REQUEST,
	IMSG_VMDOP_GET_INFO_VM_DATA,
	IMSG_VMDOP_GET_INFO_VM_END_DATA
};

int write_page(uint32_t dst, void *buf, uint32_t, int);
int read_page(uint32_t dst, void *buf, uint32_t, int);

#endif /* __VMD_H__ */
