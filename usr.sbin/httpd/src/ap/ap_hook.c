/* ====================================================================
 * Copyright (c) 1998-2000 The Apache Group.  All rights reserved.
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
 *
 */

/*
**  Implementation of a Generic Hook Interface for Apache
**  Written by Ralf S. Engelschall <rse@engelschall.com> 
**
**  See POD document at end of ap_hook.h for description.
**  View it with the command ``pod2man ap_hook.h | nroff -man | more''
**
*/

                                      /*
                                       * Premature optimization is 
                                       * the root of all evil.
                                       *       -- D. E. Knuth
                                       */

#include "httpd.h"
#include "http_log.h"
#include "ap_config.h"
#include "ap_hook.h"

/* 
 * the internal hook pool
 */
static ap_hook_entry **ap_hook_pool = NULL;

/* 
 * forward prototypes for internal functions
 */
static int            ap_hook_call_func(va_list ap, ap_hook_entry *he, ap_hook_func *hf);
static ap_hook_entry *ap_hook_create(char *hook);
static ap_hook_entry *ap_hook_find(char *hook);
static void           ap_hook_destroy(ap_hook_entry *he);

/*
 * Initialize the hook mechanism
 */
API_EXPORT(void) ap_hook_init(void)
{
	int i;

	if (ap_hook_pool != NULL)
		return;
	ap_hook_pool = (ap_hook_entry **)malloc(sizeof(ap_hook_entry *)
	    *(AP_HOOK_MAX_ENTRIES+1));
	for (i = 0; i < AP_HOOK_MAX_ENTRIES; i++)
		ap_hook_pool[i] = NULL;
	return;
}

/*
 * Kill the hook mechanism
 */
API_EXPORT(void) ap_hook_kill(void)
{
	int i;

	if (ap_hook_pool == NULL)
		return;
	for (i = 0; ap_hook_pool[i] != NULL; i++)
		ap_hook_destroy(ap_hook_pool[i]);
	free(ap_hook_pool);
	ap_hook_pool = NULL;
	return;
}
    
/*
 * Smart creation of a hook (when it exist this is the same as
 * ap_hook_find, when it doesn't exists it is created)
 */
static ap_hook_entry *ap_hook_create(char *hook)
{
	int i;
	ap_hook_entry *he;

	for (i = 0; ap_hook_pool[i] != NULL; i++)
		if (strcmp(ap_hook_pool[i]->he_hook, hook) == 0)
			return ap_hook_pool[i];

	if (i >= AP_HOOK_MAX_ENTRIES)
		return NULL;

	if ((he = (ap_hook_entry *)malloc(sizeof(ap_hook_entry))) == NULL)
		return NULL;
	ap_hook_pool[i] = he;

	he->he_hook          = strdup(hook);
	he->he_sig           = AP_HOOK_SIG_UNKNOWN;
	he->he_modeid        = AP_HOOK_MODE_UNKNOWN;
	he->he_modeval.v_int = 0;

	he->he_func = (ap_hook_func **)malloc(sizeof(ap_hook_func *)
	    *(AP_HOOK_MAX_FUNCS+1));
	if (he->he_func == NULL)
		return FALSE;

	for (i = 0; i < AP_HOOK_MAX_FUNCS; i++)
		he->he_func[i] = NULL;
	return he;
}

/*
 * Find a particular hook
 */
static ap_hook_entry *ap_hook_find(char *hook)
{
	int i;

	for (i = 0; ap_hook_pool[i] != NULL; i++)
		if (strcmp(ap_hook_pool[i]->he_hook, hook) == 0)
			return ap_hook_pool[i];
	return NULL;
}

/*
 * Destroy a particular hook
 */
static void ap_hook_destroy(ap_hook_entry *he)
{
	int i;

	if (he == NULL)
		return;
	free(he->he_hook);
	for (i = 0; he->he_func[i] != NULL; i++)
		free(he->he_func[i]);
	free(he->he_func);
	free(he);
	return;
}

/*
 * Configure a particular hook, 
 * i.e. remember its signature and return value mode
 */
API_EXPORT(int) ap_hook_configure(char *hook, ap_hook_sig sig, ap_hook_mode modeid, ...)
{
	ap_hook_entry *he;
	va_list ap;
	int rc;

	va_start(ap, modeid);
	if ((he = ap_hook_create(hook)) == NULL)
	rc = FALSE;
	else {
		he->he_sig = sig;
		he->he_modeid = modeid;
		if (modeid == AP_HOOK_MODE_DECLINE || modeid == AP_HOOK_MODE_DECLTMP) {
			if (AP_HOOK_SIG_HAS(sig, RC, char))
				he->he_modeval.v_char = va_arg(ap, va_type(char));
			else if (AP_HOOK_SIG_HAS(sig, RC, int))
				he->he_modeval.v_int = va_arg(ap, va_type(int));
			else if (AP_HOOK_SIG_HAS(sig, RC, long))
				he->he_modeval.v_long = va_arg(ap, va_type(long));
			else if (AP_HOOK_SIG_HAS(sig, RC, float))
				he->he_modeval.v_float = va_arg(ap, va_type(float));
			else if (AP_HOOK_SIG_HAS(sig, RC, double))
				he->he_modeval.v_double = va_arg(ap, va_type(double));
			else if (AP_HOOK_SIG_HAS(sig, RC, ptr))
				he->he_modeval.v_ptr = va_arg(ap, va_type(ptr));
		}
		rc = TRUE;
	}
	va_end(ap);
	return rc;
}

/*
 * Register a function to call for a hook
 */
API_EXPORT(int) ap_hook_register_I(char *hook, void *func, void *ctx)
{
	int i, j;
	ap_hook_entry *he;
	ap_hook_func *hf;

	if ((he = ap_hook_create(hook)) == NULL)
		return FALSE;

	for (i = 0; he->he_func[i] != NULL; i++)
		if (he->he_func[i]->hf_ptr == func)
			return FALSE;

	if (i == AP_HOOK_MAX_FUNCS)
		return FALSE;

	if ((hf = (ap_hook_func *)malloc(sizeof(ap_hook_func))) == NULL)
		return FALSE;

	for (j = i; j >= 0; j--)
		he->he_func[j+1] = he->he_func[j];
	he->he_func[0] = hf;

	hf->hf_ptr = func;
	hf->hf_ctx = ctx;

	return TRUE;
}

/*
 * Unregister a function to call for a hook
 */
API_EXPORT(int) ap_hook_unregister_I(char *hook, void *func)
{
	int i, j;
	ap_hook_entry *he;

	if ((he = ap_hook_find(hook)) == NULL)
		return FALSE;
	for (i = 0; he->he_func[i] != NULL; i++) {
		if (he->he_func[i]->hf_ptr == func) {
			free(he->he_func[i]);
			for (j = i; he->he_func[j] != NULL; j++)
				he->he_func[j] = he->he_func[j+1];
			return TRUE;
			}
	}
	return FALSE;
}

/*
 * Retrieve the status of a particular hook
 */
API_EXPORT(ap_hook_state) ap_hook_status(char *hook)
{
	ap_hook_entry *he;

	if ((he = ap_hook_find(hook)) == NULL)
		return AP_HOOK_STATE_NOTEXISTANT;
	if (   he->he_func[0] != NULL
	    && he->he_sig != AP_HOOK_SIG_UNKNOWN
	    && he->he_modeid != AP_HOOK_MODE_UNKNOWN)
		return AP_HOOK_STATE_REGISTERED;
	if (   he->he_sig != AP_HOOK_SIG_UNKNOWN
	    && he->he_modeid != AP_HOOK_MODE_UNKNOWN)
		return AP_HOOK_STATE_CONFIGURED;
	return AP_HOOK_STATE_ESTABLISHED;
}

/*
 * Use a hook, i.e. optional on-the-fly configure it before calling it
 */
API_EXPORT(int) ap_hook_use(char *hook, ap_hook_sig sig, ap_hook_mode modeid, ...)
{
	int i;
	ap_hook_value modeval;
	ap_hook_entry *he;
	va_list ap;
	int rc;

	va_start(ap, modeid);

	if (modeid == AP_HOOK_MODE_DECLINE || modeid == AP_HOOK_MODE_DECLTMP) {
		if (AP_HOOK_SIG_HAS(sig, RC, char))
			modeval.v_char = va_arg(ap, va_type(char));
		else if (AP_HOOK_SIG_HAS(sig, RC, int))
			modeval.v_int = va_arg(ap, va_type(int));
		else if (AP_HOOK_SIG_HAS(sig, RC, long))
			modeval.v_long = va_arg(ap, va_type(long));
		else if (AP_HOOK_SIG_HAS(sig, RC, float))
			modeval.v_float = va_arg(ap, va_type(float));
		else if (AP_HOOK_SIG_HAS(sig, RC, double))
			modeval.v_double = va_arg(ap, va_type(double));
		else if (AP_HOOK_SIG_HAS(sig, RC, ptr))
			modeval.v_ptr = va_arg(ap, va_type(ptr));
	}

	if ((he = ap_hook_create(hook)) == NULL)
		return FALSE;

	if (he->he_sig == AP_HOOK_SIG_UNKNOWN)
		he->he_sig = sig;
	if (he->he_modeid == AP_HOOK_MODE_UNKNOWN) {
		he->he_modeid  = modeid;
		he->he_modeval = modeval;
	}

	for (i = 0; he->he_func[i] != NULL; i++)
		if (ap_hook_call_func(ap, he, he->he_func[i]))
			break;

	if (i > 0 && he->he_modeid == AP_HOOK_MODE_ALL)
		rc = TRUE;
	else if (i == AP_HOOK_MAX_FUNCS || he->he_func[i] == NULL)
		rc = FALSE;
	else
		rc = TRUE;

	va_end(ap);
	return rc;
}

/*
 * Call a hook
 */
API_EXPORT(int) ap_hook_call(char *hook, ...)
{
	int i;
	ap_hook_entry *he;
	va_list ap;
	int rc;

	va_start(ap, hook);

	if ((he = ap_hook_find(hook)) == NULL) {
		va_end(ap);
		return FALSE;
	}
	if (   he->he_sig == AP_HOOK_SIG_UNKNOWN
	    || he->he_modeid == AP_HOOK_MODE_UNKNOWN) {
		va_end(ap);
		return FALSE;
	}

	for (i = 0; he->he_func[i] != NULL; i++)
		if (ap_hook_call_func(ap, he, he->he_func[i]))
			break;

	if (i > 0 && he->he_modeid == AP_HOOK_MODE_ALL)
		rc = TRUE;
	else if (i == AP_HOOK_MAX_FUNCS || he->he_func[i] == NULL)
		rc = FALSE;
	else
		rc = TRUE;

	va_end(ap);
	return rc;
}

static int ap_hook_call_func(va_list ap, ap_hook_entry *he, ap_hook_func *hf)
{
	void *v_rc;
	ap_hook_value v_tmp;
	int rc;

	/*
	* Now we dispatch the various function calls. We support function
	* signatures with up to 9 types (1 return type, 8 argument types) where
	* each argument can have 7 different types (ctx, char, int, long, float,
	* double, ptr), so theoretically there are 9^7 (=4782969) combinations
	* possible.  But because we don't need all of them, of course, we
	* implement only the following well chosen subset (duplicates are ok):
	*
	* 1. `The basic hook'.
	*
	*    void func()
	*
	* 2. The standard set of signatures which form all combinations of
	*    int&ptr based signatures for up to 3 arguments. We provide
	*    them per default for module authors.
	*
	*    int func()
	*    ptr func()
	*    int func(int)
	*    int func(ptr)
	*    ptr func(int)
	*    ptr func(ptr)
	*    int func(int,int)
	*    int func(int,ptr)
	*    int func(ptr,int)
	*    int func(ptr,ptr)
	*    ptr func(int,int)
	*    ptr func(int,ptr)
	*    ptr func(ptr,int)
	*    ptr func(ptr,ptr)
	*    int func(int,int,int)
	*    int func(int,int,ptr)
	*    int func(int,ptr,int)
	*    int func(int,ptr,ptr)
	*    int func(ptr,int,int)
	*    int func(ptr,int,ptr)
	*    int func(ptr,ptr,int)
	*    int func(ptr,ptr,ptr)
	*    ptr func(int,int,int)
	*    ptr func(int,int,ptr)
	*    ptr func(int,ptr,int)
	*    ptr func(int,ptr,ptr)
	*    ptr func(ptr,int,int)
	*    ptr func(ptr,int,ptr)
	*    ptr func(ptr,ptr,int)
	*    ptr func(ptr,ptr,ptr)
	*
	* 3. Actually currently used hooks.
	*
	*    int   func(ptr)                          [2x]
	*    int   func(ptr,ptr)                      [2x]
	*    int   func(ptr,ptr,int)                  [5x]
	*    int   func(ptr,ptr,ptr,int)              [1x]
	*    int   func(ptr,ptr,ptr,int,ptr)          [1x]
	*    int   func(ptr,ptr,ptr,ptr,int)          [1x]
	*    int   func(ptr,ptr,ptr,ptr,int,ptr)      [1x]
	*    ptr   func(ptr,ptr)                      [3x]
	*    ptr   func(ptr,ptr,ptr,ptr,ptr)          [1x]
	*    void  func(ptr)                          [2x]
	*    void  func(ptr,int,int)                  [1x]
	*    void  func(ptr,ptr)                      [5x]
	*    void  func(ptr,ptr,ptr)                  [3x]
	*    void  func(ptr,ptr,ptr,ptr)              [2x]
	*
	* To simplify the programming task we generate the actual dispatch code
	* for these calls via the embedded Perl script at the end of this source
	* file. This script parses the above lines and generates the section
	* below.  So, when you need more signature variants just add them to the
	* above list and run
	*
	*     $ perl ap_hook.c
	*
	* This automatically updates the above code.
	*/

	rc = TRUE;
	v_rc = NULL;
	if (!AP_HOOK_SIG_HAS(he->he_sig, RC, void)) {
		if (he->he_modeid == AP_HOOK_MODE_DECLTMP) {
			/* the return variable is a temporary one */ 
			if (AP_HOOK_SIG_HAS(he->he_sig, RC, char))
				v_rc = &v_tmp.v_char;
			else if (AP_HOOK_SIG_HAS(he->he_sig, RC, int))
				v_rc = &v_tmp.v_int;
			else if (AP_HOOK_SIG_HAS(he->he_sig, RC, long))
				v_rc = &v_tmp.v_long;
			else if (AP_HOOK_SIG_HAS(he->he_sig, RC, float))
				v_rc = &v_tmp.v_float;
			else if (AP_HOOK_SIG_HAS(he->he_sig, RC, double))
				v_rc = &v_tmp.v_double;
			else if (AP_HOOK_SIG_HAS(he->he_sig, RC, ptr))
				v_rc = &v_tmp.v_ptr;
		}
		else {
			/* the return variable is provided by caller */ 
			v_rc = va_arg(ap, void *);
		}
	}

	/* ----BEGIN GENERATED SECTION-------- */
	if (he->he_sig == AP_HOOK_SIG1(void)) {
		/* Call: void func() */
		((void(*)())(hf->hf_ptr))();
	}
	else if (he->he_sig == AP_HOOK_SIG1(int)) {
		/* Call: int func() */
		*((int *)v_rc) = ((int(*)())(hf->hf_ptr))();
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG1(ptr)) {
		/* Call: ptr func() */
		*((void * *)v_rc) = ((void *(*)())(hf->hf_ptr))();
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG2(int, int)) {
		/* Call: int func(int) */
		int   v1 = va_arg(ap, va_type(int));
		*((int *)v_rc) = ((int(*)(int))(hf->hf_ptr))(v1);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG2(int, ptr)) {
		/* Call: int func(ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(void *))(hf->hf_ptr))(v1);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG2(ptr, int)) {
		/* Call: ptr func(int) */
		int   v1 = va_arg(ap, va_type(int));
		*((void * *)v_rc) = ((void *(*)(int))(hf->hf_ptr))(v1);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG2(ptr, ptr)) {
		/* Call: ptr func(ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		*((void * *)v_rc) = ((void *(*)(void *))(hf->hf_ptr))(v1);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG3(int, int, int)) {
		/* Call: int func(int,int) */
		int   v1 = va_arg(ap, va_type(int));
		int   v2 = va_arg(ap, va_type(int));
		*((int *)v_rc) = ((int(*)(int, int))(hf->hf_ptr))(v1, v2);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG3(int, int, ptr)) {
		/* Call: int func(int,ptr) */
		int   v1 = va_arg(ap, va_type(int));
		void *v2 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(int, void *))(hf->hf_ptr))(v1, v2);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG3(int, ptr, int)) {
		/* Call: int func(ptr,int) */
		void *v1 = va_arg(ap, va_type(ptr));
		int   v2 = va_arg(ap, va_type(int));
		*((int *)v_rc) = ((int(*)(void *, int))(hf->hf_ptr))(v1, v2);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG3(int, ptr, ptr)) {
		/* Call: int func(ptr,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(void *, void *))(hf->hf_ptr))(v1, v2);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG3(ptr, int, int)) {
		/* Call: ptr func(int,int) */
		int   v1 = va_arg(ap, va_type(int));
		int   v2 = va_arg(ap, va_type(int));
		*((void * *)v_rc) = ((void *(*)(int, int))(hf->hf_ptr))(v1, v2);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG3(ptr, int, ptr)) {
		/* Call: ptr func(int,ptr) */
		int   v1 = va_arg(ap, va_type(int));
		void *v2 = va_arg(ap, va_type(ptr));
		*((void * *)v_rc) = ((void *(*)(int, void *))(hf->hf_ptr))(v1, v2);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG3(ptr, ptr, int)) {
		/* Call: ptr func(ptr,int) */
		void *v1 = va_arg(ap, va_type(ptr));
		int   v2 = va_arg(ap, va_type(int));
		*((void * *)v_rc) = ((void *(*)(void *, int))(hf->hf_ptr))(v1, v2);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG3(ptr, ptr, ptr)) {
		/* Call: ptr func(ptr,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		*((void * *)v_rc) = ((void *(*)(void *, void *))(hf->hf_ptr))(v1, v2);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG4(int, int, int, int)) {
		/* Call: int func(int,int,int) */
		int   v1 = va_arg(ap, va_type(int));
		int   v2 = va_arg(ap, va_type(int));
		int   v3 = va_arg(ap, va_type(int));
		*((int *)v_rc) = ((int(*)(int, int, int))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG4(int, int, int, ptr)) {
		/* Call: int func(int,int,ptr) */
		int   v1 = va_arg(ap, va_type(int));
		int   v2 = va_arg(ap, va_type(int));
		void *v3 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(int, int, void *))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG4(int, int, ptr, int)) {
		/* Call: int func(int,ptr,int) */
		int   v1 = va_arg(ap, va_type(int));
		void *v2 = va_arg(ap, va_type(ptr));
		int   v3 = va_arg(ap, va_type(int));
		*((int *)v_rc) = ((int(*)(int, void *, int))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG4(int, int, ptr, ptr)) {
		/* Call: int func(int,ptr,ptr) */
		int   v1 = va_arg(ap, va_type(int));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(int, void *, void *))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG4(int, ptr, int, int)) {
		/* Call: int func(ptr,int,int) */
		void *v1 = va_arg(ap, va_type(ptr));
		int   v2 = va_arg(ap, va_type(int));
		int   v3 = va_arg(ap, va_type(int));
		*((int *)v_rc) = ((int(*)(void *, int, int))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG4(int, ptr, int, ptr)) {
		/* Call: int func(ptr,int,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		int   v2 = va_arg(ap, va_type(int));
		void *v3 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(void *, int, void *))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG4(int, ptr, ptr, int)) {
		/* Call: int func(ptr,ptr,int) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		int   v3 = va_arg(ap, va_type(int));
		*((int *)v_rc) = ((int(*)(void *, void *, int))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG4(int, ptr, ptr, ptr)) {
		/* Call: int func(ptr,ptr,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(void *, void *, void *))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG4(ptr, int, int, int)) {
		/* Call: ptr func(int,int,int) */
		int   v1 = va_arg(ap, va_type(int));
		int   v2 = va_arg(ap, va_type(int));
		int   v3 = va_arg(ap, va_type(int));
		*((void * *)v_rc) = ((void *(*)(int, int, int))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG4(ptr, int, int, ptr)) {
		/* Call: ptr func(int,int,ptr) */
		int   v1 = va_arg(ap, va_type(int));
		int   v2 = va_arg(ap, va_type(int));
		void *v3 = va_arg(ap, va_type(ptr));
		*((void * *)v_rc) = ((void *(*)(int, int, void *))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG4(ptr, int, ptr, int)) {
		/* Call: ptr func(int,ptr,int) */
		int   v1 = va_arg(ap, va_type(int));
		void *v2 = va_arg(ap, va_type(ptr));
		int   v3 = va_arg(ap, va_type(int));
		*((void * *)v_rc) = ((void *(*)(int, void *, int))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG4(ptr, int, ptr, ptr)) {
		/* Call: ptr func(int,ptr,ptr) */
		int   v1 = va_arg(ap, va_type(int));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		*((void * *)v_rc) = ((void *(*)(int, void *, void *))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG4(ptr, ptr, int, int)) {
		/* Call: ptr func(ptr,int,int) */
		void *v1 = va_arg(ap, va_type(ptr));
		int   v2 = va_arg(ap, va_type(int));
		int   v3 = va_arg(ap, va_type(int));
		*((void * *)v_rc) = ((void *(*)(void *, int, int))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG4(ptr, ptr, int, ptr)) {
		/* Call: ptr func(ptr,int,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		int   v2 = va_arg(ap, va_type(int));
		void *v3 = va_arg(ap, va_type(ptr));
		*((void * *)v_rc) = ((void *(*)(void *, int, void *))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG4(ptr, ptr, ptr, int)) {
		/* Call: ptr func(ptr,ptr,int) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		int   v3 = va_arg(ap, va_type(int));
		*((void * *)v_rc) = ((void *(*)(void *, void *, int))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG4(ptr, ptr, ptr, ptr)) {
		/* Call: ptr func(ptr,ptr,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		*((void * *)v_rc) = ((void *(*)(void *, void *, void *))(hf->hf_ptr))(v1, v2, v3);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG5(int, ptr, ptr, ptr, int)) {
		/* Call: int func(ptr,ptr,ptr,int) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		int   v4 = va_arg(ap, va_type(int));
		*((int *)v_rc) = ((int(*)(void *, void *, void *, int))(hf->hf_ptr))(v1, v2, v3, v4);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG6(int, ptr, ptr, ptr, int, ptr)) {
		/* Call: int func(ptr,ptr,ptr,int,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		int   v4 = va_arg(ap, va_type(int));
		void *v5 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(void *, void *, void *, int, void *))(hf->hf_ptr))(v1, v2, v3, v4, v5);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG6(int, ptr, ptr, ptr, ptr, int)) {
		/* Call: int func(ptr,ptr,ptr,ptr,int) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		void *v4 = va_arg(ap, va_type(ptr));
		int   v5 = va_arg(ap, va_type(int));
		*((int *)v_rc) = ((int(*)(void *, void *, void *, void *, int))(hf->hf_ptr))(v1, v2, v3, v4, v5);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG6(int, ptr, ptr, ptr, ptr, ptr)) {
		/* Call: int func(ptr,ptr,ptr,ptr,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		void *v4 = va_arg(ap, va_type(ptr));
		void *v5 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(void *, void *, void *, void *, void *))(hf->hf_ptr))(v1, v2, v3, v4, v5);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG7(int, ptr, ptr, ptr, ptr, int, ptr)) {
		/* Call: int func(ptr,ptr,ptr,ptr,int,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		void *v4 = va_arg(ap, va_type(ptr));
		int   v5 = va_arg(ap, va_type(int));
		void *v6 = va_arg(ap, va_type(ptr));
		*((int *)v_rc) = ((int(*)(void *, void *, void *, void *, int, void *))(hf->hf_ptr))(v1, v2, v3, v4, v5, v6);
		rc = (*((int *)v_rc) != he->he_modeval.v_int);
	}
	else if (he->he_sig == AP_HOOK_SIG6(ptr, ptr, ptr, ptr, ptr, ptr)) {
		/* Call: ptr func(ptr,ptr,ptr,ptr,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		void *v4 = va_arg(ap, va_type(ptr));
		void *v5 = va_arg(ap, va_type(ptr));
		*((void * *)v_rc) = ((void *(*)(void *, void *, void *, void *, void *))(hf->hf_ptr))(v1, v2, v3, v4, v5);
		rc = (*((void * *)v_rc) != he->he_modeval.v_ptr);
	}
	else if (he->he_sig == AP_HOOK_SIG2(void, ptr)) {
		/* Call: void func(ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		((void(*)(void *))(hf->hf_ptr))(v1);
	}
	else if (he->he_sig == AP_HOOK_SIG4(void, ptr, int, int)) {
		/* Call: void func(ptr,int,int) */
		void *v1 = va_arg(ap, va_type(ptr));
		int   v2 = va_arg(ap, va_type(int));
		int   v3 = va_arg(ap, va_type(int));
		((void(*)(void *, int, int))(hf->hf_ptr))(v1, v2, v3);
	}
	else if (he->he_sig == AP_HOOK_SIG3(void, ptr, ptr)) {
		/* Call: void func(ptr,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		((void(*)(void *, void *))(hf->hf_ptr))(v1, v2);
	}
	else if (he->he_sig == AP_HOOK_SIG4(void, ptr, ptr, ptr)) {
		/* Call: void func(ptr,ptr,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		((void(*)(void *, void *, void *))(hf->hf_ptr))(v1, v2, v3);
	}
	else if (he->he_sig == AP_HOOK_SIG5(void, ptr, ptr, ptr, ptr)) {
		/* Call: void func(ptr,ptr,ptr,ptr) */
		void *v1 = va_arg(ap, va_type(ptr));
		void *v2 = va_arg(ap, va_type(ptr));
		void *v3 = va_arg(ap, va_type(ptr));
		void *v4 = va_arg(ap, va_type(ptr));
		((void(*)(void *, void *, void *, void *))(hf->hf_ptr))(v1, v2, v3, v4);
	}
	/* ----END GENERATED SECTION---------- */
	else
		ap_log_assert("hook signature not implemented", __FILE__, 0);

	if (he->he_modeid == AP_HOOK_MODE_ALL)
		rc = FALSE;
	else if (he->he_modeid == AP_HOOK_MODE_TOPMOST)
		rc = TRUE;

	return rc;
}
