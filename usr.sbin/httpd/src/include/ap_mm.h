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
**
** ap_mm.h -- wrapper code for MM shared memory library
**
*/

#ifndef AP_MM_H 
#define AP_MM_H 1

#ifndef FALSE
#define FALSE 0
#define TRUE  !FALSE
#endif

API_EXPORT(int) ap_mm_useable(void);

typedef void AP_MM;
typedef enum { AP_MM_LOCK_RD, AP_MM_LOCK_RW } ap_mm_lock_mode;

/* Global Malloc-Replacement API */
API_EXPORT(int)     ap_MM_create(size_t size, char *file);
API_EXPORT(int)     ap_MM_permission(mode_t mode, uid_t owner, gid_t group);
API_EXPORT(void)    ap_MM_destroy(void);
API_EXPORT(int)     ap_MM_lock(ap_mm_lock_mode mode);
API_EXPORT(int)     ap_MM_unlock(void);
API_EXPORT(void *)  ap_MM_malloc(size_t size);
API_EXPORT(void *)  ap_MM_realloc(void *ptr, size_t size);
API_EXPORT(void)    ap_MM_free(void *ptr);
API_EXPORT(void *)  ap_MM_calloc(size_t number, size_t size);
API_EXPORT(char *)  ap_MM_strdup(const char *str);
API_EXPORT(size_t)  ap_MM_sizeof(void *ptr);
API_EXPORT(size_t)  ap_MM_maxsize(void);
API_EXPORT(size_t)  ap_MM_available(void);
API_EXPORT(char *)  ap_MM_error(void);

/* Standard Malloc-Style API */
API_EXPORT(AP_MM *) ap_mm_create(size_t size, char *file);
API_EXPORT(int)     ap_mm_permission(AP_MM *mm, mode_t mode, uid_t owner, gid_t group);
API_EXPORT(void)    ap_mm_destroy(AP_MM *mm);
API_EXPORT(int)     ap_mm_lock(AP_MM *mm, ap_mm_lock_mode mode);
API_EXPORT(int)     ap_mm_unlock(AP_MM *mm);
API_EXPORT(void *)  ap_mm_malloc(AP_MM *mm, size_t size);
API_EXPORT(void *)  ap_mm_realloc(AP_MM *mm, void *ptr, size_t size);
API_EXPORT(void)    ap_mm_free(AP_MM *mm, void *ptr);
API_EXPORT(void *)  ap_mm_calloc(AP_MM *mm, size_t number, size_t size);
API_EXPORT(char *)  ap_mm_strdup(AP_MM *mm, const char *str);
API_EXPORT(size_t)  ap_mm_sizeof(AP_MM *mm, void *ptr);
API_EXPORT(size_t)  ap_mm_maxsize(void);
API_EXPORT(size_t)  ap_mm_available(AP_MM *mm);
API_EXPORT(char *)  ap_mm_error(void);
API_EXPORT(void)    ap_mm_display_info(AP_MM *mm);

/* Low-Level Shared Memory API */
API_EXPORT(void *)  ap_mm_core_create(size_t size, char *file);
API_EXPORT(int)     ap_mm_core_permission(void *core, mode_t mode, uid_t owner, gid_t group);
API_EXPORT(void)    ap_mm_core_delete(void *core);
API_EXPORT(size_t)  ap_mm_core_size(void *core);
API_EXPORT(int)     ap_mm_core_lock(void *core, ap_mm_lock_mode mode);
API_EXPORT(int)     ap_mm_core_unlock(void *core);
API_EXPORT(size_t)  ap_mm_core_maxsegsize(void);
API_EXPORT(size_t)  ap_mm_core_align2page(size_t size);
API_EXPORT(size_t)  ap_mm_core_align2word(size_t size);

/* Internal Library API */
API_EXPORT(void)    ap_mm_lib_error_set(unsigned int, const char *str);
API_EXPORT(char *)  ap_mm_lib_error_get(void);
API_EXPORT(int)     ap_mm_lib_version(void);

#endif /* AP_MM_H */
