/* ====================================================================
 * Copyright (c) 1999-2000 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 */

/*
**  ap_mm.c -- wrapper for MM shared memory library
**
**  This file has two reason:
**
**  1. Under DSO context we need stubs inside the Apache core code
**     to make sure the MM library's code is actually available
**     to the module DSOs.
**
**  2. When the MM library cannot be built on the current platform
**     still provide dummy stubs so modules using the ap_mm_xxx()
**     functions can be still built. But modules should use
**     ap_mm_useable() to find out whether they really can use
**     the MM stuff.
*/
                                       /*
                                        * "What you see is all you get."
                                        *     -- Brian Kernighan
                                        */
#include "httpd.h"
#include "ap_mm.h"

#ifdef EAPI_MM
#include "mm.h"
API_EXPORT(int) ap_mm_useable(void) { return TRUE;  }
#define STUB(val,nul)               { return val;   }
#define STUB_STMT(stmt)             { stmt; return; }
#else
API_EXPORT(int) ap_mm_useable(void) { return FALSE; }
#define STUB(val,nul)               { return nul;   }
#define STUB_STMT(stmt)             { return;       }
#endif

API_EXPORT(int) ap_MM_create(size_t size, char *file) 
    STUB(MM_create(size, file), FALSE)
API_EXPORT(int) ap_MM_permission(mode_t mode, uid_t owner, gid_t group) 
    STUB(MM_permission(mode, owner, group), -1)
API_EXPORT(void) ap_MM_destroy(void)
    STUB_STMT(MM_destroy())
API_EXPORT(int) ap_MM_lock(ap_mm_lock_mode mode)
    STUB(MM_lock(mode), FALSE)
API_EXPORT(int) ap_MM_unlock(void)
    STUB(MM_unlock(), FALSE)
API_EXPORT(void *) ap_MM_malloc(size_t size)
    STUB(MM_malloc(size), NULL)
API_EXPORT(void *) ap_MM_realloc(void *ptr, size_t size)
    STUB(MM_realloc(ptr, size), NULL)
API_EXPORT(void) ap_MM_free(void *ptr)
    STUB_STMT(MM_free(ptr))
API_EXPORT(void *) ap_MM_calloc(size_t number, size_t size)
    STUB(MM_calloc(number, size), NULL)
API_EXPORT(char *) ap_MM_strdup(const char *str)
    STUB(MM_strdup(str), NULL)
API_EXPORT(size_t) ap_MM_sizeof(void *ptr)
    STUB(MM_sizeof(ptr), 0)
API_EXPORT(size_t) ap_MM_maxsize(void)
    STUB(MM_maxsize(), 0)
API_EXPORT(size_t) ap_MM_available(void)
    STUB(MM_available(), 0)
API_EXPORT(char *) ap_MM_error(void)
    STUB(MM_error(), NULL)

API_EXPORT(AP_MM *) ap_mm_create(size_t size, char *file)
    STUB(mm_create(size, file), NULL)
API_EXPORT(int) ap_mm_permission(AP_MM *mm, mode_t mode, uid_t owner, gid_t group) 
    STUB(mm_permission(mm, mode, owner, group), -1)
API_EXPORT(void) ap_mm_destroy(AP_MM *mm)
    STUB_STMT(mm_destroy(mm))
API_EXPORT(int) ap_mm_lock(AP_MM *mm, ap_mm_lock_mode mode)
    STUB(mm_lock(mm, mode), FALSE)
API_EXPORT(int) ap_mm_unlock(AP_MM *mm)
    STUB(mm_unlock(mm), FALSE)
API_EXPORT(void *) ap_mm_malloc(AP_MM *mm, size_t size)
    STUB(mm_malloc(mm, size), NULL)
API_EXPORT(void *) ap_mm_realloc(AP_MM *mm, void *ptr, size_t size)
    STUB(mm_realloc(mm, ptr, size), NULL)
API_EXPORT(void) ap_mm_free(AP_MM *mm, void *ptr)
    STUB_STMT(mm_free(mm, ptr))
API_EXPORT(void *) ap_mm_calloc(AP_MM *mm, size_t number, size_t size)
    STUB(mm_calloc(mm, number, size), NULL)
API_EXPORT(char *) ap_mm_strdup(AP_MM *mm, const char *str)
    STUB(mm_strdup(mm, str), NULL)
API_EXPORT(size_t) ap_mm_sizeof(AP_MM *mm, void *ptr)
    STUB(mm_sizeof(mm, ptr), 0)
API_EXPORT(size_t) ap_mm_maxsize(void)
    STUB(mm_maxsize(), 0)
API_EXPORT(size_t) ap_mm_available(AP_MM *mm)
    STUB(mm_available(mm), 0)
API_EXPORT(char *) ap_mm_error(void)
    STUB(mm_error(), NULL)
API_EXPORT(void) ap_mm_display_info(AP_MM *mm)
    STUB_STMT(mm_display_info(mm))

API_EXPORT(void *) ap_mm_core_create(size_t size, char *file)
    STUB(mm_core_create(size, file), NULL)
API_EXPORT(int) ap_mm_core_permission(void *core, mode_t mode, uid_t owner, gid_t group) 
    STUB(mm_core_permission(core, mode, owner, group), -1)
API_EXPORT(void) ap_mm_core_delete(void *core)
    STUB_STMT(mm_core_delete(core))
API_EXPORT(size_t) ap_mm_core_size(void *core)
    STUB(mm_core_size(core), 0)
API_EXPORT(int) ap_mm_core_lock(void *core, ap_mm_lock_mode mode)
    STUB(mm_core_lock(core, mode), FALSE)
API_EXPORT(int) ap_mm_core_unlock(void *core)
    STUB(mm_core_unlock(core), FALSE)
API_EXPORT(size_t) ap_mm_core_maxsegsize(void)
    STUB(mm_core_maxsegsize(), 0)
API_EXPORT(size_t) ap_mm_core_align2page(size_t size)
    STUB(mm_core_align2page(size), 0)
API_EXPORT(size_t) ap_mm_core_align2word(size_t size)
    STUB(mm_core_align2word(size), 0)

API_EXPORT(void) ap_mm_lib_error_set(unsigned int type, const char *str)
    STUB_STMT(mm_lib_error_set(type, str))
API_EXPORT(char *) ap_mm_lib_error_get(void)
    STUB(mm_lib_error_get(), NULL)
API_EXPORT(int) ap_mm_lib_version(void)
    STUB(mm_lib_version(), 0)
