/* Native-dependent definitions for Sparc running OpenBSD, for GDB.
   Copyright (C) 1986, 1987, 1989, 1992, 1995, 1996
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define SVR4_SHARED_LIBS

/* Get generic OpenBSD native definitions. */
#include "nm-obsd.h"

/* Before storing, read all the registers. (see inftarg.c) */
#define CHILD_PREPARE_TO_STORE() \
    read_register_bytes (0, NULL, REGISTER_BYTES)

#include "solib.h"     /* Support for shared libraries. */

/* The Net- and OpenBSD link.h structure definitions have different names
   than the SunOS version, but the structures are very similar,
   so we can use solib.c by defining the SunOS names.  */
#define link_object	sod
#define lo_name		sod_name
#define lo_library	sod_library
#define lo_unused	sod_reserved
#define lo_major	sod_major
#define lo_minor	sod_minor
#define lo_next		sod_next

#define l_addr		som_addr
#define l_name		som_path
#define l_next		som_next
#define lm_lop		som_sod
#define lm_lob		som_sodbase
#define l_prev		som_sodbase
#define lm_rwt		som_write
#define lm_ld		som_dynamic
#define lm_lpd		som_spd

#define link_dynamic_2	section_dispatch_table
#define ld_loaded	sdt_loaded
#define ld_need		sdt_sods
#define ld_rules	sdt_rules
#define ld_got		sdt_got
#define ld_plt		sdt_plt
#define ld_rel		sdt_rel
#define ld_hash		sdt_hash
#define ld_stab		sdt_nzlist
#define ld_stab_hash	sdt_filler2
#define ld_buckets	sdt_buckets
#define ld_symbols	sdt_strings
#define ld_symb_size	sdt_str_sz
#define ld_text		sdt_text_sz
#define ld_plt_sz	sdt_plt_sz
#define rtc_symb	rt_symbol
#define rtc_sp		rt_sp
#define rtc_next	rt_next

#define r_debug		so_debug
#define ldd_version	dd_version
#define ldd_in_debugger	dd_in_debugger
#define ldd_sym_loaded	dd_sym_loaded
#define ldd_bp_addr	dd_bpt_addr
#define ldd_bp_inst	dd_bpt_shadow
#define ldd_cp		dd_cc

#define link_dynamic	_dynamic
#define ld_version	d_version
#define ldd		d_debug
#define ld_un		d_un
#define ld_2		d_sdt
