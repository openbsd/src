/*
 * Copyright (c) 1997, 1998, 1999, 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#ifndef KCONF_H
#define KCONF_H 1

struct kconf_config_binding {
    enum { kconf_config_string, kconf_config_list } type;
    char *name;
    struct kconf_config_binding *next;
    union {
	char *string;
	struct kconf_config_binding *list;
	void *generic;
    } u;
};

typedef struct kconf_config_binding kconf_config_binding;

typedef kconf_config_binding kconf_config_section;

struct kconf_context {
    kconf_config_section *cf;
};

typedef struct kconf_context * kconf_context;

typedef enum { KCONF_FALSE = 0, KCONF_TRUE = 1 } kconf_boolean;


int
kconf_init (kconf_context *context);

void
kconf_free (kconf_context context);

int
kconf_config_parse_file_debug (const char *fname,
			       kconf_config_section **res,
			       unsigned *lineno,
			       char **error_message);
int
kconf_config_parse_file (const char *fname, kconf_config_section **res);

int
kconf_config_file_free (kconf_context context, kconf_config_section *s);

const void *
kconf_config_get_next (kconf_context context,
		       kconf_config_section *c,
		       kconf_config_binding **pointer,
		       int type,
		       ...);
const void *
kconf_config_vget_next (kconf_context context,
			kconf_config_section *c,
			kconf_config_binding **pointer,
			int type,
			va_list args);
const void *
kconf_config_get (kconf_context context,
		  kconf_config_section *c,
		  int type,
		  ...);

const void *
kconf_config_vget (kconf_context context,
		   kconf_config_section *c,
		   int type,
		   va_list args);

const kconf_config_binding *
kconf_config_get_list (kconf_context context,
		       kconf_config_section *c,
		       ...);

const kconf_config_binding *
kconf_config_vget_list (kconf_context context,
			kconf_config_section *c,
			va_list args);

const char *
kconf_config_get_string (kconf_context context,
			 kconf_config_section *c,
			 ...);

const char *
kconf_config_get_string_default (kconf_context context,
				 kconf_config_section *c,
				 const char *def,
				 ...);

const char *
kconf_config_vget_string (kconf_context context,
			  kconf_config_section *c,
			  va_list args);


char **
kconf_config_vget_strings(kconf_context context,
			  kconf_config_section *c,
			  va_list args);


char**
kconf_config_get_strings(kconf_context context,
			 kconf_config_section *c,
			 ...);

void
kconf_config_free_strings(char **strings);

kconf_boolean
kconf_config_vget_bool_default (kconf_context context,
				kconf_config_section *c,
				kconf_boolean def_value,
				va_list args);


kconf_boolean
kconf_config_vget_bool  (kconf_context context,
			 kconf_config_section *c,
			 va_list args);

kconf_boolean
kconf_config_get_bool_default (kconf_context context,
			       kconf_config_section *c,
			       kconf_boolean def_value,
			       ...);

kconf_boolean
kconf_config_get_bool (kconf_context context,
		       kconf_config_section *c,
		       ...);

int
kconf_config_vget_time_default (kconf_context context,
				kconf_config_section *c,
				int def_value,
				va_list args);

int
kconf_config_vget_time  (kconf_context context,
			 kconf_config_section *c,
			 va_list args);

int
kconf_config_get_time_default (kconf_context context,
			       kconf_config_section *c,
			       int def_value,
			       ...);
    
int
kconf_config_get_time (kconf_context context,
		       kconf_config_section *c,
		       ...);

int
kconf_config_vget_int_default (kconf_context context,
			       kconf_config_section *c,
			       int def_value,
			       va_list args);

int
kconf_config_vget_int  (kconf_context context,
			kconf_config_section *c,
			va_list args);

int
kconf_config_get_int_default (kconf_context context,
			      kconf_config_section *c,
			      int def_value,
			      ...);

int
kconf_config_get_int (kconf_context context,
		      kconf_config_section *c,
		      ...);

#endif /* KCONF_H */
