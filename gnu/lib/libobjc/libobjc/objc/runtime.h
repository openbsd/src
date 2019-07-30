/* GNU Objective C Runtime internal declarations
   Copyright (C) 1993, 1995, 1996, 1997, 2002 Free Software Foundation, Inc.
   Contributed by Kresten Krab Thorup

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2, or (at your option) any later version.

GNU CC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with
GNU CC; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* As a special exception, if you link this library with files compiled with
   GCC to produce an executable, this does not cause the resulting executable
   to be covered by the GNU General Public License. This exception does not
   however invalidate any other reasons why the executable file might be
   covered by the GNU General Public License.  */

#ifndef __objc_runtime_INCLUDE_GNU
#define __objc_runtime_INCLUDE_GNU

#include <stdarg.h>		/* for varargs and va_list's */

#include <stdio.h>
#include <ctype.h>

#include <stddef.h>		/* so noone else will get system versions */
#include "assert.h"

#include "objc/objc.h"		/* core data types */
#include "objc/objc-api.h"	/* runtime api functions */

#include "objc/thr.h"		/* thread and mutex support */

#include "objc/hash.h"		/* hash structures */
#include "objc/objc-list.h"	/* linear lists */

extern void __objc_add_class_to_hash(Class);   /* (objc-class.c) */
extern void __objc_init_selector_tables(void); /* (objc-sel.c) */
extern void __objc_init_class_tables(void);    /* (objc-class.c) */
extern void __objc_init_dispatch_tables(void); /* (objc-dispatch.c) */
extern void __objc_install_premature_dtable(Class); /* (objc-dispatch.c) */
extern void __objc_resolve_class_links(void);  /* (objc-class.c) */
extern void __objc_register_selectors_from_class(Class); /* (objc-sel.c) */
extern void __objc_update_dispatch_table_for_class (Class);/* (objc-msg.c) */

extern int  __objc_init_thread_system(void);    /* thread.c */
extern int  __objc_fini_thread_system(void);    /* thread.c */
extern void __objc_print_dtable_stats(void);    /* sendmsg.c */

extern void class_add_method_list(Class, MethodList_t);

/* Registering instance methods as class methods for root classes */
extern void __objc_register_instance_methods_to_class(Class);
extern Method_t search_for_method_in_list(MethodList_t list, SEL op);

/* True when class links has been resolved */     
extern BOOL __objc_class_links_resolved;

/* Number of selectors stored in each of the selector  tables */
extern unsigned int __objc_selector_max_index;

/* Mutex locking __objc_selector_max_index and its arrays. */
extern objc_mutex_t __objc_runtime_mutex;

/* Number of threads which are alive. */
extern int __objc_runtime_threads_alive;

#ifdef DEBUG
#define DEBUG_PRINTF(format, args...) printf (format, ## args)
#else
#define DEBUG_PRINTF(format, args...)
#endif 

BOOL __objc_responds_to (id object, SEL sel); /* for internal use only! */
SEL  __sel_register_typed_name (const char*, const char*, 
				struct objc_selector*, BOOL is_const);
extern void __objc_generate_gc_type_description (Class);

#endif /* not __objc_runtime_INCLUDE_GNU */


