/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2004,2005,2006,2007, Keir Fraser <keir@xensource.com>
 */

#ifndef _XENREG_H_
#define _XENREG_H_

/*
 * Hypercall interface defines
 */

#if defined(__amd64__)
# define HYPERCALL_ARG1(_i1)	ulong _a1 = (ulong)(_i1)
# define HYPERCALL_ARG2(_i2)	ulong _a2 = (ulong)(_i2)
# define HYPERCALL_ARG3(_i3)	ulong _a3 = (ulong)(_i3)
# define HYPERCALL_ARG4(_i4)	register ulong _a4 __asm__("r10") = (ulong)(_i4)
# define HYPERCALL_ARG5(_i5)	register ulong _a5 __asm__("r8") = (ulong)(_i5)
# define HYPERCALL_RES1		ulong _r1
# define HYPERCALL_RES2		ulong _r2
# define HYPERCALL_RES3		ulong _r3
# define HYPERCALL_RES4		ulong _r4
# define HYPERCALL_RES5		/* empty */
# define HYPERCALL_RES6		/* empty */
# define HYPERCALL_RET(_rv)	(_rv) = _r1
# define HYPERCALL_LABEL	"call *%[hcall]"
# define HYPERCALL_PTR(_ptr)	[hcall] "a" (_ptr)
# define HYPERCALL_OUT1		"=a" (_r1)
# define HYPERCALL_OUT2		, "=D" (_r2)
# define HYPERCALL_OUT3		, "=S" (_r3)
# define HYPERCALL_OUT4		, "=d" (_r4)
# define HYPERCALL_OUT5		, "+r" (_a4)
# define HYPERCALL_OUT6		, "+r" (_a5)
# define HYPERCALL_IN1		"1" (_a1)
# define HYPERCALL_IN2		, "2" (_a2)
# define HYPERCALL_IN3		, "3" (_a3)
# define HYPERCALL_IN4		/* empty */
# define HYPERCALL_IN5		/* empty */
# define HYPERCALL_CLOBBER	"memory"
#elif defined(__i386__)
# define HYPERCALL_ARG1(_i1)	ulong _a1 = (ulong)(_i1)
# define HYPERCALL_ARG2(_i2)	ulong _a2 = (ulong)(_i2)
# define HYPERCALL_ARG3(_i3)	ulong _a3 = (ulong)(_i3)
# define HYPERCALL_ARG4(_i4)	ulong _a4 = (ulong)(_i4)
# define HYPERCALL_ARG5(_i5)	ulong _a5 = (ulong)(_i5)
# define HYPERCALL_RES1		ulong _r1
# define HYPERCALL_RES2		ulong _r2
# define HYPERCALL_RES3		ulong _r3
# define HYPERCALL_RES4		ulong _r4
# define HYPERCALL_RES5		ulong _r5
# define HYPERCALL_RES6		ulong _r6
# define HYPERCALL_RET(_rv)	(_rv) = _r1
# define HYPERCALL_LABEL	"call *%[hcall]"
# define HYPERCALL_PTR(_ptr)	[hcall] "a" (_ptr)
# define HYPERCALL_OUT1		"=a" (_r1)
# define HYPERCALL_OUT2		, "=b" (_r2)
# define HYPERCALL_OUT3		, "=c" (_r3)
# define HYPERCALL_OUT4		, "=d" (_r4)
# define HYPERCALL_OUT5		, "=S" (_r5)
# define HYPERCALL_OUT6		, "=D" (_r6)
# define HYPERCALL_IN1		"1" (_a1)
# define HYPERCALL_IN2		, "2" (_a2)
# define HYPERCALL_IN3		, "3" (_a3)
# define HYPERCALL_IN4		, "4" (_a4)
# define HYPERCALL_IN5		, "5" (_a5)
# define HYPERCALL_CLOBBER	"memory"
#else
# error "Not implemented"
#endif

#define CPUID_OFFSET_XEN_HYPERCALL		0x2

/*
 * interface/xen.h
 */

typedef uint16_t domid_t;

/* DOMID_SELF is used in certain contexts to refer to oneself. */
#define DOMID_SELF		(0x7FF0U)

/*
 * interface/features.h
 *
 * Feature flags, reported by XENVER_get_features.
 */

/*
 * If set, the guest does not need to write-protect its pagetables, and can
 * update them via direct writes.
 */
#define XENFEAT_writable_page_tables		0
/*
 * If set, the guest does not need to write-protect its segment descriptor
 * tables, and can update them via direct writes.
 */
#define XENFEAT_writable_descriptor_tables	1
/*
 * If set, translation between the guest's 'pseudo-physical' address space
 * and the host's machine address space are handled by the hypervisor. In this
 * mode the guest does not need to perform phys-to/from-machine translations
 * when performing page table operations.
 */
#define XENFEAT_auto_translated_physmap		2
/* If set, the guest is running in supervisor mode (e.g., x86 ring 0). */
#define XENFEAT_supervisor_mode_kernel		3
/*
 * If set, the guest does not need to allocate x86 PAE page directories
 * below 4GB. This flag is usually implied by auto_translated_physmap.
 */
#define XENFEAT_pae_pgdir_above_4gb		4
/* x86: Does this Xen host support the MMU_PT_UPDATE_PRESERVE_AD hypercall? */
#define XENFEAT_mmu_pt_update_preserve_ad	5
/* x86: Does this Xen host support the MMU_{CLEAR,COPY}_PAGE hypercall? */
#define XENFEAT_highmem_assist			6
/*
 * If set, GNTTABOP_map_grant_ref honors flags to be placed into guest kernel
 * available pte bits.
 */
#define XENFEAT_gnttab_map_avail_bits		7
/* x86: Does this Xen host support the HVM callback vector type? */
#define XENFEAT_hvm_callback_vector		8
/* x86: pvclock algorithm is safe to use on HVM */
#define XENFEAT_hvm_safe_pvclock		9
/* x86: pirq can be used by HVM guests */
#define XENFEAT_hvm_pirqs			10
/* operation as Dom0 is supported */
#define XENFEAT_dom0				11


/*
 * interface/version.h
 *
 * Xen version, type, and compile information.
 */

/* arg == NULL; returns major:minor (16:16). */
#define XENVER_version		0

/* arg == 16 bytes buffer. */
#define XENVER_extraversion	1

/* arg == xen_compile_info. */
#define XENVER_compile_info	2
struct xen_compile_info {
	char compiler[64];
	char compile_by[16];
	char compile_domain[32];
	char compile_date[32];
};

#define XENVER_get_features	6
struct xen_feature_info {
	unsigned int submap_idx;	/* IN: which 32-bit submap to return */
	uint32_t submap;		/* OUT: 32-bit submap */
};

/* arg == NULL; returns host memory page size. */
#define XENVER_pagesize		7

/* arg == xen_domain_handle_t. */
#define XENVER_guest_handle	8

#define XENVER_commandline	9
typedef char xen_commandline_t[1024];

#endif /* _XENREG_H_ */
