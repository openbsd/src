/*	$OpenBSD: output.c,v 1.1.1.1 1998/09/14 21:53:26 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: output.c,v 1.31 1998/09/06 23:33:39 assar Exp $");
#endif

#include <stdio.h>
#include <list.h>
#include <efile.h>
#include <string.h>
#include <strutil.h>
#include <mem.h>
#include <roken.h>
#include "sym.h"
#include "output.h"
#include "types.h"
#include "lex.h"

/*
 * The name of the current package that we're generating stubs for
 */

char *package = "";

/*
 * File handles for the generated files themselves.
 */

FILE *headerfile,
    *clientfile,
    *serverfile,
    *clienthdrfile,
    *serverhdrfile,
    *ydrfile;

long tmpcnt = 0 ;

typedef enum { ENCODE_RX, DECODE_RX, ENCODE_MEM, DECODE_MEM } EncodeType;


static void print_type (char *name, Type *type, FILE *f);
static Bool print_entry (List *list, Listitem *item, void *i);
static void generate_hdr_struct (Symbol *s, FILE *f);
static void generate_hdr_enum (Symbol *s, FILE *f);
static void generate_hdr_const (Symbol *s, FILE *f);
static void generate_hdr_typedef (Symbol *s, FILE *f);
static int sizeof_type (Type *type);
static int sizeof_symbol (Symbol *);
static void encode_type (char *name, Type *type, FILE *f, 
			 EncodeType encodetype);
static Bool encode_entry (List *list, Listitem *item, void *arg);
static void encode_struct (Symbol *s, char *name, FILE *f, 
			   EncodeType encodetype);
static void encode_enum (Symbol *s, char *name, FILE *f, EncodeType encodetype);
static void encode_typedef (Symbol *s, char *name, FILE *f, 
			    EncodeType encodetype);
static void encode_symbol (Symbol *s, char *name, FILE *f, 
			   EncodeType encodetype);

static void
print_type (char *name, Type *type, FILE *f)
{
     switch (type->type) {
	  case TCHAR :
	       fprintf (f, "char %s", name);
	       break;
	  case TUCHAR :
	       fprintf (f, "unsigned char %s", name);
	       break;
	  case TSHORT :
	       fprintf (f, "int16_t %s", name);
	       break;
	  case TUSHORT :
	       fprintf (f, "u_int16_t %s", name);
	       break;
	  case TLONG :
	       fprintf (f, "int32_t %s", name);
	       break;
	  case TULONG :
	       fprintf (f, "u_int32_t %s", name);
	       break;
	  case TSTRING :
	       fprintf (f, "char %s[%d]", name, type->size);
	       break;
	  case TPOINTER :
	  {
	       char *tmp;

	       tmp = (char *)emalloc (strlen(name) + 2);
	       *tmp = '*';
	       strcpy (tmp+1, name);
	       print_type (tmp, type->subtype, f);
	       free (tmp);
	       break;
	  }
	  case TUSERDEF : 
	       if(type->symbol->type == TSTRUCT)
		    fprintf (f, "struct %s %s", type->symbol->name, name);
	       else
		    fprintf (f, "%s %s", type->symbol->name, name);
	       break;
	  case TVARRAY :
	  {
	       char *s;

	       s = (char *)emalloc (strlen (name) + 6);
	       *s = '*';
	       strcpy (s + 1, name);
	       strcat (s, "_len");

	       fprintf (f, "struct {\n");
	       if (type->indextype)
		    print_type ("len", type->indextype, f);
	       else
		    fprintf (f, "unsigned %s", "len");
	       fprintf (f, ";\n");
	       strcpy(s + strlen(s) - 3, "val");
	       print_type ("*val", type->subtype, f);
	       fprintf (f, ";\n} %s", name);
	       free(s);
	       break;
	  }
	  case TARRAY :
	       print_type (name, type->subtype, f);
	       fprintf (f, "[ %d ]", type->size);
	       break;
	  case TOPAQUE :
	       fprintf (f, "char %s", name);
	       break;
	  default :
	       abort();
     }
}

static Bool
print_entry (List *list, Listitem *item, void *i)
{
     StructEntry *s = (StructEntry *)listdata (item);
     FILE *f = (FILE *)i;

     fprintf (f, "     ");
     print_type (s->name, s->type, f);
     fprintf (f, ";\n");
     return FALSE;
}

/*
 * Return the size of this type in bytes.
 * In the case of a variable-sized type, return -1.
 */

static Bool
sizeof_struct_iter (List *list, Listitem *item, void *arg)
{
     int *tot = (int *)arg;
     StructEntry *s = (StructEntry *)listdata (item);
     int sz;

     sz = sizeof_type (s->type);
     if (sz == -1) {
	  *tot = -1;
	  return TRUE;
     } else {
	  *tot += sz;
	  return FALSE;
     }
}

static int
sizeof_struct (Symbol *s)
{
     int tot = 0;

     listiter (s->u.list, sizeof_struct_iter, &tot);
     return tot;
}

static int
sizeof_type (Type *t)
{
     switch (t->type) {
     case TCHAR :
     case TUCHAR :
/*	  return 1;*/
     case TSHORT :
     case TUSHORT :
/*	  return 2;*/
     case TLONG :
     case TULONG :
	  return 4;
     case TSTRING :
	 if (t->size == 0)
	     return -1;
	 else
	     return t->size;
     case TOPAQUE :
	  return -1;
     case TUSERDEF :
	  return sizeof_symbol (t->symbol);
     case TARRAY :
     {
	  int sz = sizeof_type (t->subtype);

	  if (sz == -1)
	       return -1;
	  else
	       return t->size * sz;
     }
     case TVARRAY :
	  return -1;
     case TPOINTER :
	  return -1;
     default :
	  abort ();
     }
}

static int
sizeof_symbol (Symbol *s)
{
     switch (s->type) {
	  case TUNDEFINED :
	       fprintf (stderr, "What is %s doing in sizeof_type?", s->name);
	       return 0;
	  case TSTRUCT :
	       return sizeof_struct (s);
	  case TENUM :
	       return 4;
	  case TCONST :
	       return 0;
	  case TENUMVAL :
	       return 0;
	  case TTYPEDEF :
	       return sizeof_type (s->u.type);
	  default :
	       abort ();
     }
}

/*
 * Generate header contents
 */

static void
generate_hdr_struct (Symbol *s, FILE *f)
{
     fprintf (f, "struct %s {\n", s->name);
     listiter (s->u.list, print_entry, f);
     fprintf (f, "};\ntypedef struct %s %s;\n", s->name, s->name);
}

static void
generate_hdr_enum (Symbol *s, FILE *f)
{
     Listitem *item;
     Symbol *e;

     fprintf (f, "enum %s {\n", s->name);
     for (item = listhead (s->u.list); 
	  item && listnext (s->u.list, item); 
	  item = listnext (s->u.list, item)) {
	  e = (Symbol *)listdata (item);

	  fprintf (f, "     %s = %d,\n", e->name, e->u.val);
     }
     e = (Symbol *)listdata (item);
     fprintf (f, "     %s = %d\n};\ntypedef enum %s %s;\n",
	      e->name, e->u.val, s->name, s->name);
}

static void
generate_hdr_const (Symbol *s, FILE *f)
{
     fprintf (f, "#define %s %d\n", s->name, s->u.val);
}

static void
generate_hdr_typedef (Symbol *s, FILE *f)
{
     fprintf (f, "typedef ");
     print_type (s->name, s->u.type, f);
     fprintf (f, ";\n");
}

void
generate_sizeof (Symbol *s, FILE *f)
{
     int sz;

     if (s->type == TCONST)
	 return;

     sz = sizeof_symbol (s);
     if (sz != -1) {
	  char *name;

	  name = strdup (s->name);
	  fprintf (f, "#define %s_SIZE %d\n", strupr (name), sz);
	  free (name);
     }
}

void
generate_header (Symbol *s, FILE *f)
{
     switch (s->type) {
	  case TUNDEFINED :
	       fprintf (f, "What is %s doing in generate_heaer?", s->name);
	       break;
	  case TSTRUCT :
	       generate_hdr_struct (s, f);
	       break;
	  case TENUM :
	       generate_hdr_enum (s, f);
	       break;
	  case TCONST :
	       generate_hdr_const (s, f);
	       break;
	  case TENUMVAL :
	       break;
	  case TTYPEDEF :
	       generate_hdr_typedef (s, f);
	  default :
	       break;
     }
     putc ('\n', f);
}

/*
 * Generate functions for encoding and decoding.
 */

/* XXX - still assumes that a word is 32 bits */

static char *
encode_function (Type *type, EncodeType encodetype)
{
     if (type->flags & TASIS)
	  return "";
     else if (encodetype == ENCODE_RX || encodetype == ENCODE_MEM)
	 return "htonl";
#if 0
	  if (type->type == TSHORT || type->type == TUSHORT)
	       return "htons";
	  else if (type->type == TLONG || type->type == TULONG)
	       return "htonl";
	  else
	       abort();
#endif
     else if (encodetype == DECODE_RX || encodetype == DECODE_MEM)
	 return "ntohl";
#if 0
	  if (type->type == TSHORT || type->type == TUSHORT)
	       return "ntohs";
	  else if (type->type == TLONG || type->type == TULONG)
	       return "ntohl";
	  else
	       abort();
#endif
     else
	  abort();
}

static void
encode_long (char *name, Type *type, FILE *f, EncodeType encodetype)
{
     switch (encodetype) {
	  case ENCODE_RX :
	       fprintf (f, "{ u_int32_t u;\n"
			"u = %s (%s);\n"
			"if(rx_Write(call, &u, sizeof(u)) != sizeof(u))\n"
			"goto fail;\n"
			"}\n",
			encode_function (type, encodetype),
			name);
	       break;
	  case DECODE_RX :
	       fprintf (f, "{ u_int32_t u;\n"
			"if(rx_Read(call, &u, sizeof(u)) != sizeof(u))\n"
			"goto fail;\n"
			"%s = %s (u);\n"
			"}\n", name,
			encode_function (type, encodetype));
	       break;
	  case ENCODE_MEM :
	       fprintf (f, "{ int32_t tmp = %s(%s); "
			"if (*total_len < sizeof(int32_t)) { errno = EFAULT; return NULL ; } "
			"bcopy ((char*)&tmp, ptr, sizeof(int32_t)); "
			"ptr += sizeof(int32_t); "
			"*total_len -= sizeof(int32_t);}\n",
			encode_function (type, encodetype),
			name);
	       break;
	  case DECODE_MEM :
	       fprintf (f, "{ int32_t tmp; "
			"if (*total_len < sizeof(int32_t)) { errno = EFAULT; return NULL ; } "
			"bcopy (ptr, (char *)&tmp, sizeof(int32_t)); "
			"%s = %s(tmp); "
			"ptr += sizeof(int32_t); "
			"*total_len -= sizeof(int32_t);}\n", 
			name,
			encode_function (type, encodetype));
	       break;
	  default :
	       abort ();
     }
}

#ifdef not_yet
static void
encode_char (char *name, Type *type, FILE *f, EncodeType encodetype)
{
     switch (encodetype) {
	  case ENCODE_RX :
	       fprintf (f,
			"if(rx_Write(call, &%s, sizeof(%s)) != sizeof(%s))\n"
			"goto fail;\n",
			name, name, name);
	       break;
	  case DECODE_RX :
	       fprintf (f,
			"if(rx_Read(call, &%s, sizeof(%s)) != sizeof(%s))\n"
			"goto fail;\n",
			name, name, name);
	       break;
	  case ENCODE_MEM :
	       fprintf (f, "{ if (*total_len < sizeof(char)) { errno = EFAULT; return NULL ; } "
			"*((char *)ptr) = %s; "
			"ptr += sizeof(char); *total_len -= sizeof(char);}\n",
			name);
	       break;
	  case DECODE_MEM :
	       fprintf (f, "{ if (*total_len < sizeof(char)) { errno = EFAULT; return NULL ; } "
			"%s = *((char *)ptr); "
			"ptr += sizeof(char); *total_len -= sizeof(char);}\n",
			name);
	       break;
	  default :
	       abort ();
     }
}

static void
encode_short (char *name, Type *type, FILE *f, EncodeType encodetype)
{
     switch (encodetype) {
	  case ENCODE_RX :
	       fprintf (f, "{ int16_t u;\n"
			"u = %s (%s);\n"
			"if(rx_Write(call, &u, sizeof(u)) != sizeof(u))\n"
			"goto fail;\n"
			"}\n", 
			encode_function (type, encodetype),
			name);
	       break;
	  case DECODE_RX :
	       fprintf (f, "{ int16_t u;\n"
			"if(rx_Read(call, &u, sizeof(u)) != sizeof(u))\n"
			"goto fail;\n"
			"%s = %s (u);\n"
			"}\n", name,
			encode_function (type, encodetype));
	       break;
	  case ENCODE_MEM :
	       fprintf (f, "{ in16_t tmp = %s(%s); "
			"if (*total_len < sizeof(int16_t)) { errno = EFAULT; return NULL ; } "
			"bcopy ((char*)&tmp, ptr, sizeof(int16_t)); "
			"ptr += sizeof(int16_t); "
			"*total_len -= sizeof(int16_t);}\n", 
			encode_function (type, encodetype),
			name);
	       break;
	  case DECODE_MEM :
	       fprintf (f, "{ int16_t tmp; "
			"if (*total_len < sizeof(int16_t)) { errno = EFAULT; return NULL ; } " 
			"bcopy (ptr, (char *)&tmp, sizeof(int16_t)); "
			"%s = %s(tmp); "
			"ptr += sizeof(int16_t); "
			"*total_len -= sizeof(int16_t); }\n", 
			name,
			encode_function (type, encodetype));
	       break;
	  default :
	       abort ();
     }
}
#endif

static void
encode_string (char *name, Type *type, FILE *f, EncodeType encodetype)
{
     Type lentype = {TULONG};

     switch (encodetype) {
	  case ENCODE_RX :
	       fprintf (f, "{ unsigned len;\n"
			"char zero[4] = {0, 0, 0, 0};\n"
			"unsigned padlen;\n"
			"len = strlen(%s);\n"
			"padlen = (4 - (len %% 4)) %% 4;\n",
			name);
	       encode_type ("len", &lentype, f, encodetype);
	       fprintf (f,
			"if(rx_Write(call, %s, len) != len)\n"
			"goto fail;\n"
			"if(rx_Write(call, zero, padlen) != padlen)\n"
			"goto fail;\n"
			"}\n", name);
	       break;
	  case DECODE_RX :
	       fprintf (f, "{ unsigned len;\n"
			"unsigned padlen;\n"
			"char zero[4] = {0, 0, 0, 0};\n");
	       encode_type ("len", &lentype, f, encodetype);
	       if (type->size != 0)
		   fprintf (f,
			    "if (len >= %u)\n"
			    "abort();\n",
			    type->size);
	       fprintf (f, 
			"if(rx_Read(call, %s, len) != len)\n"
			"goto fail;\n"
			"%s[len] = '\\0';\n"
			"padlen = (4 - (len %% 4)) %% 4;\n"
			"if(rx_Read(call, zero, padlen) != padlen)\n"
			"goto fail;\n"
			"}\n", name, name);
	       break;
	  case ENCODE_MEM :
	       fprintf (f,
			"{\nunsigned len = strlen(%s);\n"
			"if (*total_len < len) { errno = EFAULT; return NULL ; } "
			"*total_len -= len;\n",
			name);
	       encode_type ("len", &lentype, f, encodetype);
	       fprintf (f, "strncpy (ptr, %s, len);\n", name);
	       fprintf (f, "ptr += len + (4 - (len %% 4) %% 4);\n}\n");
	       break;
	  case DECODE_MEM :
	       fprintf (f,
		   "{\nunsigned len;\n");
	       encode_type ("len", &lentype, f, encodetype);
	       fprintf (f,
			"if (*total_len < len) { errno = EFAULT; return NULL ; }\n"
			"*total_len -= len;\n"
			"memcpy (%s, ptr, len);\n"
			"%s[len] = '\\0';\n"
			"ptr += len + (4 - (len %% 4)) %% 4;\n}\n",
			name, name);
	       break;
	  default :
	       abort ();
     }
}	       

static void
encode_array (char *name, Type *type, FILE *f, EncodeType encodetype)
{
     if (type->subtype->type == TOPAQUE) {
	  if (type->size % 4 != 0)
	       error_message ("Opaque array should be"
			      "multiple of 4");
	  switch (encodetype) {
	       case ENCODE_RX :
		    fprintf (f,
			     "if(rx_Write (call, %s, %d) != %d)\n"
			     "goto fail;",
			     name, type->size, type->size);
		    break;
	       case DECODE_RX :
		    fprintf (f,
			     "if(rx_Read (call, %s, %d) != %d)\n"
			     "goto fail;",
			     name, type->size, type->size);
		    break;
	       case ENCODE_MEM :
		    fprintf (f, "if (*total_len < %u) { errno = EFAULT; return NULL ; }\n"
			     "memcpy (ptr, %s, %u);\n", type->size, name,
			     type->size);
		    fprintf (f, "ptr += %u; *total_len -= %u;\n", 
			     type->size, type->size);
		    break;
	       case DECODE_MEM :
		    fprintf (f, "if (*total_len < %u) { errno = EFAULT; return NULL ; }\n"
			     "memcpy (%s, ptr, %u);\n", type->size, name,
			     type->size);
		    fprintf (f, "ptr += %u; *total_len -= %u;\n", 
			     type->size, type->size);
		    break;
	       default :
		    abort ();
	  }
     } else {
	  char tmp[256];

	  fprintf (f, "{\nint i%lu;\nfor(i%lu = 0; i%lu < %u;"
		   "++i%lu){\n", tmpcnt, tmpcnt, tmpcnt, type->size,tmpcnt);
	  snprintf(tmp, sizeof(tmp)-1, "%s[i%lu]", name, tmpcnt);
	  tmpcnt++;
	  encode_type (tmp , type->subtype, f, encodetype);
	  tmpcnt--;
	  fprintf (f, "}\n}\n");
     }
}

static void
encode_varray (char *name, Type *type, FILE *f, EncodeType encodetype)
{
     char tmp[256];
     Type lentype = {TULONG};
	       
     strcpy (tmp, name);
     strcat (tmp, ".len");

     encode_type (tmp, type->indextype ? type->indextype : &lentype,
		  f, encodetype);
     if (encodetype == DECODE_MEM || encodetype == DECODE_RX) {
	 fprintf (f, "%s.val = (", name);
	 print_type ("*", type->subtype, f);
	 fprintf (f, ")malloc(sizeof(");
	 print_type ("", type->subtype, f);
	 fprintf (f, ") * %s);\n", tmp);
     }
     if (type->subtype->type == TOPAQUE) {
	  switch (encodetype) {
	       case ENCODE_RX :
		    fprintf (f, "{\n"
			     "char zero[4] = {0, 0, 0, 0};\n"
			     "unsigned padlen = (4 - (%s %% 4)) %% 4;\n"
			     "if(rx_Write (call, %s.val, %s) != %s)\n"
			     "goto fail;\n"
			     "if(rx_Write (call, zero, padlen) != padlen)\n"
			     "goto fail;\n"
			     "}\n",
			     tmp, name, tmp, tmp);
		    break;
	       case DECODE_RX :
		    fprintf (f, "{\n"
			     "char zero[4] = {0, 0, 0, 0};\n"
			     "unsigned padlen = (4 - (%s %% 4)) %% 4;\n"
			     "if(rx_Read (call, %s.val, %s) != %s)\n"
			     "goto fail;\n"
			     "if(rx_Read (call, zero, padlen) != padlen)\n"
			     "goto fail;\n"
			     "}\n",
			     tmp, name, tmp, tmp);
		    break;
	       case ENCODE_MEM :
		   /* XXX bounce checking */
		    fprintf (f, "{\n"
			     "char zero[4] = {0, 0, 0, 0};\n"
			     "memcpy (ptr, %s.val, %s);\n"
			     "memcpy (ptr + %s, zero, (4 - (%s %% 4)) %% 4);\n"
			     "ptr += %s + (4 - (%s %% 4)) %% 4;\n"
			     "}\n",
			     name, tmp, tmp, tmp, tmp, tmp);
		    break;
	       case DECODE_MEM :
		   /* XXX bounce checking */
		    fprintf (f, "{\n"
			     "memcpy (%s.val, ptr, %s);\n"
			     "ptr += %s + (4 - (%s %% 4)) %% 4;\n"
			     "}\n",
			     name, tmp, tmp, tmp);
		    break;
	       default :
		    abort ();
	  }
     } else {
	  fprintf (f, "{\nint i%lu;\nfor(i%lu = 0; i%lu < %s;"
		   "++i%lu){\n", tmpcnt, tmpcnt, tmpcnt, tmp, tmpcnt);
	  snprintf(tmp, sizeof(tmp)-1, "%s.val[i%lu]", name, tmpcnt);
	  tmpcnt++;
	  encode_type (tmp , type->subtype, f, encodetype);
	  tmpcnt--;
	  fprintf (f, "}\n}\n");
     }
}

static void
encode_pointer (char *name, Type *type, FILE *f, EncodeType encodetype)
{
     Type booltype = {TULONG};
     char tmp[256];

     sprintf (tmp, "*(%s)", name);

     switch(encodetype) {
     case ENCODE_RX:
	  abort ();
     case ENCODE_MEM:
	  fprintf(f, "{ unsigned bool;\n"
		  "bool = %s != NULL;\n", name);
	  encode_type ("bool", &booltype, f, encodetype);
	  fprintf (f, "if(%s) {\n", name);
	  encode_type (tmp, type->subtype, f, encodetype);
	  fprintf (f, "}\n"
		   "}\n");
	  break;
     case DECODE_RX:
	  abort();
     case DECODE_MEM:
	  fprintf(f, "{ unsigned bool;\n");
	  encode_type ("bool", &booltype, f, encodetype);
	  fprintf (f, "if(bool) {\n");
	  fprintf (f, "%s = malloc(sizeof(%s));\n"
		   "if (%s == NULL) return ENOMEM;\n", 
		   name, tmp, name);
	  encode_type (tmp, type->subtype, f, encodetype);
	  fprintf (f, "} else {\n"
		   "%s = NULL;\n"
		   "}\n"
		   "}\n", name);
	  break;
     default:
	  abort ();
     }
}

static void
encode_type (char *name, Type *type, FILE *f, EncodeType encodetype)
{
     switch (type->type) {
	  case TCHAR :
	  case TUCHAR :
#if 0
	       encode_char (name, type, f, encodetype);
	       break;
#endif
	  case TSHORT :
	  case TUSHORT :
#if 0
	       encode_short (name, type, f, encodetype);
	       break;
#endif
	  case TLONG :
	  case TULONG :
	       encode_long (name, type, f, encodetype);
	       break;
	  case TSTRING :
	       encode_string (name, type, f, encodetype);
	       break;
	  case TOPAQUE :
	       error_message ("Type opaque only allowed as part of an array");
	       break;
	  case TUSERDEF :
	       encode_symbol (type->symbol, name, f, encodetype);
	       break;
	  case TARRAY :
	       encode_array (name, type, f, encodetype);
	       break;
	  case TVARRAY :
	       encode_varray (name, type, f, encodetype);
	       break;
	  case TPOINTER :
	       encode_pointer (name, type, f, encodetype);
	       break;
	  default :
	       abort();
	  }
}

struct context {
     char *name;
     FILE *f;
     Symbol *symbol;
     EncodeType encodetype;
};

static Bool
encode_entry (List *list, Listitem *item, void *arg)
{
     StructEntry *s = (StructEntry *)listdata (item);
     char tmp[256];
     struct context *context = (struct context *)arg;

     strcpy (tmp, context->name);
     strcat (tmp, ".");
     strcat (tmp, s->name);

     if (s->type->type == TPOINTER
	 && s->type->subtype->type == TUSERDEF
	 && s->type->subtype->symbol->type == TSTRUCT
	 && strcmp(s->type->subtype->symbol->name,
		   context->symbol->name) == 0) {
	 fprintf (context->f,
		  "ptr = ydr_encode_%s(%s, ptr);\n",
		  context->symbol->name,
		  tmp);
     } else {
	 encode_type (tmp, s->type, context->f, context->encodetype);
     }

     return FALSE;
}

static void
encode_struct (Symbol *s, char *name, FILE *f, EncodeType encodetype)
{
     struct context context;

     context.name       = name;
     context.symbol     = s;
     context.f          = f;
     context.encodetype = encodetype;

     listiter (s->u.list, encode_entry, (void *)&context);
}

static void
encode_enum (Symbol *s, char *name, FILE *f, EncodeType encodetype)
{
     Type type = {TLONG};
     char tmp[256];

     strcpy (tmp, "(int)");
     strcat (tmp, name);

     encode_type (tmp, &type, f, encodetype);
}

static void
encode_typedef (Symbol *s, char *name, FILE *f, EncodeType encodetype)
{
     encode_type (name, s->u.type, f, encodetype);
}

static void
encode_symbol (Symbol *s, char *name, FILE *f, EncodeType encodetype)
{
     switch (s->type) {
	  case TSTRUCT :
	       encode_struct (s, name, f, encodetype);
	       break;
	  case TENUM :
	       encode_enum (s, name, f, encodetype);
	       break;
	  case TTYPEDEF :
	       encode_typedef (s, name, f, encodetype);
	       break;
	  default :
	       abort();
	  }
}

/*
 * Generate the definition of an encode/decode function.
 */

static void
generate_function_definition (Symbol *s, FILE *f, Bool encodep)
{
     if (s->type == TSTRUCT || s->type == TENUM || s->type == TTYPEDEF) {
	  fprintf (f, 
		   "char *ydr_%scode_%s(%s *o, char *ptr, size_t *total_len)",
		   encodep ? "en" : "de",
		   s->name, s->name);
     } else if (s->type == TCONST || s->type == TENUMVAL 
	      || s->type == TTYPEDEF)
	  ;
     else
	  error_message ("What is %s (type %d) doing here?\n",
			 s->name, s->type);
}

/*
 * Generate an encode/decode function
 */

void
generate_function (Symbol *s, FILE *f, Bool encodep)
{
     if (s->type == TSTRUCT || s->type == TENUM || s->type == TTYPEDEF) {
	  generate_function_definition (s, f, encodep);
	  fprintf (f, "\n{\n");
	  encode_symbol (s, "(*o)", f, encodep ? ENCODE_MEM : DECODE_MEM);
	  fprintf (f, "return ptr;\n}\n");
     } else if (s->type == TCONST || s->type == TENUMVAL 
	      || s->type == TTYPEDEF)
	  ;
     else
	  error_message ("What is %s (type %d) doing here?\n",
			 s->name, s->type);
}

/*
 * Generate an prototype for an encode/decode function
 */

void
generate_function_prototype (Symbol *s, FILE *f, Bool encodep)
{
     if (s->type == TSTRUCT || s->type == TENUM || s->type == TTYPEDEF) {
	  generate_function_definition (s, f, encodep);
	  fprintf (f, ";\n");
     } else if (s->type == TCONST || s->type == TENUMVAL 
	      || s->type == TTYPEDEF)
	  ;
     else
	  error_message ("What is %s (type %d) doing here?\n",
			 s->name, s->type);
}

static Bool
gen1 (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if ((a->argtype == TOUT || a->argtype == TINOUT)
	 && a->type->type != TPOINTER
	 && a->type->type != TSTRING)
	 error_message ("Argument %s is OUT and not pointer or string.\n",
			a->name);
     fprintf (f, ", ");
     if (a->argtype == TIN)
	 fprintf (f, "const ");
     print_type (a->name, a->type, f);
     fprintf (f, "\n");
     return FALSE;
}

static Bool
genin (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->argtype == TIN || a->argtype == TINOUT) {
	  fprintf (f, ", ");
	  print_type (a->name, a->type, f);
	  fprintf (f, "\n");
     }
     return FALSE;
}

static Bool
genout (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);

     if (a->argtype == TOUT || a->argtype == TINOUT)
	  return gen1 (list, item, arg);
     else
	  return FALSE;
}

static Bool
gendeclare (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->type->type == TPOINTER)
	  print_type (a->name, a->type->subtype, f);
     else
	  print_type (a->name, a->type, f);
     fprintf (f, ";\n");
     return FALSE;
}

static Bool
genfree_isarrayp(Type *type)
{
    if (type->type == TVARRAY)
	return TRUE;
    if (type->type == TPOINTER)
	return genfree_isarrayp(type->subtype);
    if (type->type == TUSERDEF &&
	type->symbol &&
	type->symbol->type == TTYPEDEF)
	return genfree_isarrayp(type->symbol->u.type);
    
    return FALSE;
}


static Bool
genfree (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (genfree_isarrayp(a->type))
	 fprintf(f, "free(%s.val);\n", a->name);

     return FALSE;
}

static Bool
genencodein (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->argtype != TIN && a->argtype != TINOUT)
	  return TRUE;
     else {
	  if (a->type->type == TPOINTER) {
	       char *tmp = (char *)emalloc (strlen (a->name) + 4);
	       
	       sprintf (tmp, "(*%s)", a->name);

	       encode_type (tmp, a->type->subtype, f, ENCODE_RX);
	       free (tmp);
	  } else
	       encode_type (a->name, a->type, f, ENCODE_RX);
	  return FALSE;
     }
}

static Bool
gendecodeout (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->argtype == TOUT || a->argtype == TINOUT) {
	 if (a->type->type == TPOINTER) {
	     char *tmp = (char *)emalloc (strlen (a->name) + 4);
	       
	     sprintf (tmp, "(*%s)", a->name);

	     encode_type (tmp, a->type->subtype, f, DECODE_RX);
	     free (tmp);
	 } else if(a->type->type == TSTRING) {
	     encode_type (a->name, a->type, f, DECODE_RX);
	 }
     }
     return FALSE;
}

static Bool
gendecodein (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->argtype != TIN && a->argtype != TINOUT)
	  return TRUE;
     else {
	  if (a->type->type == TPOINTER) {
#if 0
	       char *tmp = (char *)emalloc (strlen (a->name) + 4);
	       
	       sprintf (tmp, "(*%s)", a->name);

	       encode_type (tmp, a->type->subtype, f, DECODE_RX);
	       free (tmp);
#endif
	       encode_type (a->name, a->type->subtype, f, DECODE_RX);
	  } else
	       encode_type (a->name, a->type, f, DECODE_RX);
	  return FALSE;
     }
}

static Bool
genencodeout (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->argtype == TOUT || a->argtype == TINOUT) {
	  if (a->type->type == TPOINTER)
	       encode_type (a->name, a->type->subtype, f, ENCODE_RX);
	  else
	       encode_type (a->name, a->type, f, ENCODE_RX);
     }
     return FALSE;
}

static Bool
findargtypeiter (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     int *type = (int *)arg;

     if (a->argtype == *type) {
	 (*type)++;
	 return TRUE;
     }
     return FALSE;
}

static Bool
findargtype(List *list, int type)
{
    int savedtype = type;
    listiter(list, findargtypeiter, &type);
    if (type != savedtype)
	return TRUE;
    return FALSE;
}

static Bool
genargs (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->type->type == TPOINTER)
	  putc ('&', f);
     fputs (a->name, f);
     if (listnext (list, item))
	  fprintf (f, ", ");
     return FALSE;
}

/*
 * Generate the stub functions for this RPC call
 */

static void
generate_simple_stub (Symbol *s, FILE *f, FILE *headerf)
{
     Type type = {TLONG};
     char *op;

     fprintf (headerf, "int %s%s(\nstruct rx_connection *connection\n",
	      package, s->name);
     listiter (s->u.proc.arguments, gen1, headerf);
     fprintf (headerf, ");\n\n");

     fprintf (f, "int %s%s(\nstruct rx_connection *connection\n",
	      package, s->name);
     listiter (s->u.proc.arguments, gen1, f);
     fprintf (f, ")\n{\n"
	      "struct rx_call *call;\n"
	      "int ret = 0;\n"
	      "call = rx_NewCall (connection);\n");

     asprintf (&op, "%u", s->u.proc.id);
     
     encode_type (op, &type, f, ENCODE_RX);
     free (op);
     listiter (s->u.proc.arguments, genencodein, f);
     listiter (s->u.proc.arguments, gendecodeout, f);
     fprintf (f,
	      "return rx_EndCall (call,0);\n"
	      "fail:\n"
	      "ret = rx_Error(call);\n"
	      "rx_EndCall (call, 0);\n"
	      "return ret;\n"
	      "}\n");
}

static void
generate_split_stub (Symbol *s, FILE *f, FILE *headerf)
{
     Type type = {TLONG};
     char *op;

     fprintf (headerf, "int Start%s%s(\nstruct rx_call *call\n",
	      package, s->name);
     listiter (s->u.proc.arguments, genin, headerf);
     fprintf (headerf, ");\n\n");

     fprintf (f, "int Start%s%s(\nstruct rx_call *call\n",
	      package, s->name);
     listiter (s->u.proc.arguments, genin, f);
     fprintf (f, ")\n{\n");


     asprintf (&op, "%u", s->u.proc.id);
     encode_type (op, &type, f, ENCODE_RX);
     free (op);
     listiter (s->u.proc.arguments, genencodein, f);
     fprintf (f, "return 0;\n");
     /* XXX only in arg */
     if (findargtype(s->u.proc.arguments, TIN) || 
	 findargtype(s->u.proc.arguments, TINOUT))
	 fprintf (f, "fail:\n"
		  "return rx_Error(call);\n");
     fprintf (f, "}\n\n");

     fprintf (headerf, "int End%s%s(\nstruct rx_call *call\n", 
	      package, s->name);
     listiter (s->u.proc.arguments, genout, headerf);
     fprintf (headerf, ");\n\n");

     fprintf (f, "int End%s%s(\nstruct rx_call *call\n", 
	      package, s->name);
     listiter (s->u.proc.arguments, genout, f);
     fprintf (f, ")\n{\n");

     listiter (s->u.proc.arguments, gendecodeout, f);
     fprintf (f, "return 0;\n");
     /* XXX only out arg */
     if (findargtype(s->u.proc.arguments, TOUT) || 
	 findargtype(s->u.proc.arguments, TINOUT))
	 fprintf (f, "fail:\n"
		  "return rx_Error(call);\n");
     fprintf (f, "}\n\n");
}

void
generate_client_stub (Symbol *s, FILE *f, FILE *headerf)
{
    if (s->u.proc.flags & TSPLIT)
	  generate_split_stub (s, f, headerf);
    if (s->u.proc.flags & TSIMPLE)
	  generate_simple_stub (s, f, headerf);
}

/*
 * A list of all the functions that are to be recognized by the
 * server, later used in generate_server_switch.
 */

static List *func_list;

/*
 * open all files
 */

void
init_generate (char *filename)
{
     char *tmp;
     char *fileuppr;

     func_list = listnew ();

     tmp = (char *)emalloc (strlen (filename) + 17);
     sprintf (tmp, "%s.h", filename);
     headerfile = efopen (tmp, "w");
     fileuppr = strdup (filename);
     strupr (fileuppr);
     fprintf (headerfile, "/* Generated from %s.xg */\n", filename);
     fprintf (headerfile, "#ifndef _%s_\n"
	      "#define _%s_\n\n", fileuppr, fileuppr);
     free (fileuppr);
     
     sprintf (tmp, "%s.ydr.c", filename);
     ydrfile = efopen (tmp, "w");
     fprintf (ydrfile, "/* Generated from %s.xg */\n", filename);
     fprintf (ydrfile, "#include <%s.h>\n\n", filename);
     fprintf (ydrfile, "#include <stdio.h>\n");
     fprintf (ydrfile, "#include <stdlib.h>\n");
     fprintf (ydrfile, "#include <sys/types.h>\n");
     fprintf (ydrfile, "#include <netinet/in.h>\n");
     fprintf (ydrfile, "#include <roken.h>\n");
     fprintf (ydrfile, "#ifndef HAVE_BCOPY\n"
	      "#define bcopy(a,b,c) memcpy((b),(a),(c))\n"
	      "#endif /* !HAVE_BCOPY */\n\n");

     sprintf (tmp, "%s.cs.c", filename);
     clientfile = efopen (tmp, "w");
     fprintf (clientfile, "/* Generated from %s.xg */\n", filename);
     fprintf (clientfile, "#include \"%s.h\"\n\n", filename);
     fprintf (clientfile, "#include \"%s.cs.h\"\n\n", filename);
     fprintf (clientfile, "#include <stdio.h>\n");
     fprintf (clientfile, "#include <stdlib.h>\n");
     fprintf (clientfile, "#include <sys/types.h>\n");
     fprintf (clientfile, "#include <netinet/in.h>\n");
     fprintf (clientfile, "#include <roken.h>\n");
     fprintf (clientfile, "#ifndef HAVE_BCOPY\n"
	      "#define bcopy(a,b,c) memcpy((b),(a),(c))\n"
	      "#endif /* !HAVE_BCOPY */\n\n");

     sprintf (tmp, "%s.ss.c", filename);
     serverfile = efopen (tmp, "w");
     fprintf (serverfile, "/* Generated from %s.xg */\n", filename);
     fprintf (serverfile, "#include \"%s.h\"\n\n", filename);
     fprintf (serverfile, "#include \"%s.ss.h\"\n\n", filename);
     fprintf (serverfile, "#include <stdio.h>\n");
     fprintf (serverfile, "#include <stdlib.h>\n");
     fprintf (serverfile, "#include <sys/types.h>\n");
     fprintf (serverfile, "#include <netinet/in.h>\n");
     fprintf (serverfile, "#include <roken.h>\n");
     fprintf (serverfile, "#ifndef HAVE_BCOPY\n"
	      "#define bcopy(a,b,c) memcpy((b),(a),(c))\n"
	      "#endif /* !HAVE_BCOPY */\n\n");

     sprintf (tmp, "%s.cs.h", filename);
     clienthdrfile = efopen (tmp, "w");
     fprintf (clienthdrfile, "/* Generated from %s.xg */\n", filename);

     sprintf (tmp, "%s.ss.h", filename);
     serverhdrfile = efopen (tmp, "w");
     fprintf (serverhdrfile, "/* Generated from %s.xg */\n", filename);
     fprintf (serverhdrfile, "#include <rx/rx.h>\n");

     free (tmp);
}

void
close_generator (char *filename)
{
     char *fileupr = strdup (filename);
	  
     strupr (fileupr);
     fprintf (headerfile, "\n#endif /* %s */\n", fileupr);
     efclose (headerfile);
     efclose (clientfile);
     efclose (serverfile);
     efclose (clienthdrfile);
     efclose (serverhdrfile);
     efclose (ydrfile);
}

/*
 * Generate the server-side stub function for the function in s and
 * write it to the file f.
 */

void
generate_server_stub (Symbol *s, FILE *f, FILE *headerf)
{
     fprintf (headerf, "int _%s%s(\nstruct rx_call *call);\n",
	      package, s->name);
     fprintf (headerf, "int %s%s(\nstruct rx_call *call\n",
	      package, s->name);
     listiter (s->u.proc.arguments, gen1, headerf);
     fprintf (headerf, ");\n\n");

     fprintf (f, "int _%s%s(\nstruct rx_call *call)\n",
	      package, s->name);
     fprintf (f, "{\n"
	      "int _result;\n");
     listiter (s->u.proc.arguments, gendeclare, f);
     fprintf (f, "\n");
     listiter (s->u.proc.arguments, gendecodein, f);
     fprintf (f, "_result = %s%s(", package, s->name);
     if (/* s->u.proc.splitp */ 1) {
	  fprintf (f, "call");
	  if (!listemptyp (s->u.proc.arguments))
	       fprintf (f, ", ");
     }
     listiter (s->u.proc.arguments, genargs, f);
     fprintf (f, ");\n");
     listiter (s->u.proc.arguments, genencodeout, f);
     listiter (s->u.proc.arguments, genfree, f);
     fprintf (f, "return _result;\n");
     if (!listemptyp(s->u.proc.arguments))
	 fprintf(f, "fail:\n"
		 "return rx_Error(call);\n");
     fprintf(f, "}\n\n");

     listaddtail (func_list, s);
}

static Bool
gencase (List *list, Listitem *item, void *arg)
{
     Symbol *s = (Symbol *)listdata (item);
     FILE *f = (FILE *)arg;

     fprintf (f, "case %u: {\n"
	      "_result = _%s%s(call);\n"
	      "break;\n"
	      "}\n",
	      s->u.proc.id, package, s->name);
     return FALSE;
}

/*
 *
 */

void
generate_server_switch (FILE *c_file,
			FILE *h_file)
{
     Type optype = {TULONG};
     
     fprintf (h_file,
	      "int %sExecuteRequest(struct rx_call *call);\n",
	      package);

     fprintf (c_file, "int %sExecuteRequest(struct rx_call *call)\n"
	      "{\n"
	      "unsigned opcode;\n"
	      "int _result;\n",
	      package);

     encode_type ("opcode", &optype, c_file, DECODE_RX);
     fprintf (c_file, "switch(opcode) {\n");

     listiter (func_list, gencase, c_file);

     fprintf (c_file, "default:\n"
	      "fprintf (stderr, \"Ignoring %%d\\n\", opcode);\n"
	      "}\n"
	      "return _result;\n"
	      "fail:\n"
	      "return rx_Error(call);\n"
	      "}\n\n");
}
