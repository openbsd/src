/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: output.c,v 1.73.2.1 2001/03/04 04:48:48 lha Exp $");
#endif

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <list.h>
#include <err.h>
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
 * This is the list of packages so we know how to generate
 * all Execute_package().
 */

List *packagelist = NULL;

/*
 * Add this in front of the real functions implementing the server
 * functions called.
 */

char *prefix = "";

/*
 * File handles for the generated files themselves.
 */

ydr_file headerfile,
    clientfile,
    serverfile,
    clienthdrfile,
    serverhdrfile,
    td_file,
    ydrfile;

long tmpcnt = 0;

/*
 * Function to convert error codes with.
 * (the default, the empty string, conveniently means no conversion.)
 */

char *error_function = "";

typedef enum { ENCODE_RX, DECODE_RX, ENCODE_MEM, DECODE_MEM } EncodeType;

typedef enum { CLIENT, SERVER } Side;

typedef enum { FDECL, VDECL } DeclType;

static void print_type (char *name, Type *type, enum argtype argtype, 
			DeclType decl, FILE *f);
static Bool print_entry (List *list, Listitem *item, void *i);
static void generate_hdr_struct (Symbol *s, FILE *f);
static void generate_hdr_enum (Symbol *s, FILE *f);
static void generate_hdr_const (Symbol *s, FILE *f);
static void generate_hdr_typedef (Symbol *s, FILE *f);
static int sizeof_type (Type *type);
static int sizeof_symbol (Symbol *);
static void encode_type (char *name, Type *type, FILE *f, 
			 EncodeType encodetype, Side side);
static void display_type (char *where, char *name, Type *type, FILE *f);
static Bool encode_entry (List *list, Listitem *item, void *arg);
static void encode_struct (Symbol *s, char *name, FILE *f, 
			   EncodeType encodetype, Side side);
static void encode_enum (Symbol *s, char *name, FILE *f,
			 EncodeType encodetype, Side side);
static void encode_typedef (Symbol *s, char *name, FILE *f, 
			    EncodeType encodetype, Side side);
static void encode_symbol (Symbol *s, char *name, FILE *f, 
			   EncodeType encodetype, Side side);
static void print_symbol (char *where, Symbol *s, char *name, FILE *f);

static void
print_type (char *name, Type *type, enum argtype argtype, 
	    DeclType decl, FILE *f)
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
	       if (type->size && decl == VDECL)
		   fprintf (f, "char %s[%d]", name, type->size);
	       else if (argtype != TIN && type->size == 0)
		   fprintf (f, "char **%s", name);
	       else
		   fprintf (f, "char *%s", name);
	       break;
	  case TPOINTER :
	  {
	       char *tmp;
	       size_t len = strlen(name) + 2;

	       tmp = (char *)emalloc (len);
	       *tmp = '*';
	       strlcpy (tmp+1, name, len - 1);
	       print_type (tmp, type->subtype, argtype, decl, f);
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
	       size_t len = strlen (name) + 6;

	       s = (char *)emalloc (len);
	       *s = '*';
	       strlcpy (s + 1, name, len - 1);
	       strlcat (s, "_len", len);

	       fprintf (f, "struct {\n");
	       if (type->indextype)
		    print_type ("len", type->indextype, argtype, decl, f);
	       else
		    fprintf (f, "unsigned %s", "len");
	       fprintf (f, ";\n");
	       strcpy(s + strlen(s) - 3, "val");
	       print_type ("*val", type->subtype, argtype, decl, f);
	       fprintf (f, ";\n} %s", name);
	       free(s);
	       break;
	  }
	  case TARRAY :
	       print_type (name, type->subtype, argtype, decl, f);
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
     print_type (s->name, s->type, TIN, VDECL, f);
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
	  return 1;
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
     if (s->u.list)
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
     print_type (s->name, s->u.type, TIN, VDECL, f);
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

	  name = estrdup (s->name);
	  fprintf (f, "#define %s_SIZE %d\n", strupr (name), sz);
	  free (name);
     }
}

void
generate_header (Symbol *s, FILE *f)
{
     switch (s->type) {
	  case TUNDEFINED :
	       fprintf (f, "What is %s doing in generate_header?", s->name);
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

/*
 * encode/decode long
 */

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
			"if (*total_len < sizeof(int32_t)) goto fail;\n"
			"bcopy ((char*)&tmp, ptr, sizeof(int32_t)); "
			"ptr += sizeof(int32_t); "
			"*total_len -= sizeof(int32_t);}\n",
			encode_function (type, encodetype),
			name);
	       break;
	  case DECODE_MEM :
	       fprintf (f, "{ int32_t tmp; "
			"if (*total_len < sizeof(int32_t)) goto fail;"
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

/*
 * print long
 */ 

static void
print_long (char *where, char *name, Type *type, FILE *f)
{
    fprintf (f, "printf(\" %s = %%d\", %s%s);", name, where, name);
}

/*
 *
 */

static void __attribute__ ((unused))
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
	       fprintf (f, "{ if (*total_len < sizeof(char)) goto fail;\n"
			"*((char *)ptr) = %s; "
			"ptr += sizeof(char); *total_len -= sizeof(char);}\n",
			name);
	       break;
	  case DECODE_MEM :
	       fprintf (f, "{ if (*total_len < sizeof(char)) goto fail;\n"
			"%s = *((char *)ptr); "
			"ptr += sizeof(char); *total_len -= sizeof(char);}\n",
			name);
	       break;
	  default :
	       abort ();
     }
}

static void __attribute__ ((unused))
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
	  	      "if (*total_len < sizeof(int16_t)) goto fail;\n"
	              "bcopy ((char*)&tmp, ptr, sizeof(int16_t)); "
	              "ptr += sizeof(int16_t); "
	              "*total_len -= sizeof(int16_t);}\n", 
	                encode_function (type, encodetype),
			name);
	       break;
	  case DECODE_MEM :
	       fprintf (f, "{ int16_t tmp; "
			"if (*total_len < sizeof(int16_t)) goto fail;\n"
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

/*
 * encode/decode TSTRING
 */

static void
encode_string (char *name, Type *type, FILE *f, EncodeType encodetype,
	       Side side)
{
     Type lentype = {TULONG};
     char *nname;

     asprintf (&nname, "(%s%s)",
	       ((type->size == 0) && side == CLIENT 
		&& (encodetype == ENCODE_RX || encodetype == ENCODE_MEM))
	       ? "*" : "", name);

     switch (encodetype) {
	  case ENCODE_RX :
	       fprintf (f, "{ unsigned len;\n"
			"char zero[4] = {0, 0, 0, 0};\n"
			"unsigned padlen;\n"
			"len = strlen(%s);\n"
			"padlen = (4 - (len %% 4)) %% 4;\n",
			name);
	       encode_type ("len", &lentype, f, encodetype, side);
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
	       encode_type ("len", &lentype, f, encodetype, side);
	       if (type->size != 0) {
		   fprintf (f,
			    "if (len >= %u) {\n"
			    "rx_Error(call) = ENOMEM;\n"
			    "goto fail;\n"
			    "}\n",
			    type->size);
	       } else {
		   fprintf(f, "%s = malloc(len + 1);\n", nname);
	       }

	       fprintf (f, 
			"if(rx_Read(call, %s, len) != len)\n"
			"goto fail;\n"
			"%s[len] = '\\0';\n"
			"padlen = (4 - (len %% 4)) %% 4;\n"
			"if(rx_Read(call, zero, padlen) != padlen)\n"
			"goto fail;\n"
			"}\n", nname, nname);
	       break;
	  case ENCODE_MEM :
	       fprintf (f,
			"{\nunsigned len = strlen(%s);\n"
			"if (*total_len < len) goto fail;\n"
			"*total_len -= len;\n",
			name);
	       encode_type ("len", &lentype, f, encodetype, side);
	       fprintf (f, "strncpy (ptr, %s, len);\n", name);
	       fprintf (f, "ptr += len + (4 - (len %% 4) %% 4);\n"
			"*total_len -= len + (4 - (len %% 4) %% 4);\n}\n");
	       break;
	  case DECODE_MEM :
	       fprintf (f,
		   "{\nunsigned len;\n");
	       encode_type ("len", &lentype, f, encodetype, side);
	       fprintf (f,
			"if (*total_len < len) goto fail;\n"
			"*total_len -= len;\n");
	       if (type->size != 0) {
		   fprintf (f,
			    "if(len >= %u)\n"
			    "goto fail;\n",
			    type->size);
	       } else {
		   fprintf (f, "%s = malloc(len + 1);\n", nname);
	       }
	       fprintf (f,
			"memcpy (%s, ptr, len);\n"
			"%s[len] = '\\0';\n"
			"ptr += len + (4 - (len %% 4)) %% 4;\n"
			"*total_len -= len + (4 - (len %% 4) %% 4);\n}\n",
			nname, nname);
	       break;
	  default :
	       abort ();
     }
     free (nname);
}	       

/*
 * print TSTRING
 */

static void
print_string (char *where, char *name, Type *type, FILE *f)
{
    fprintf (f, "/* printing TSTRING %s%s */\n", where, name);
    fprintf (f, "printf(\" %s = %%s\", %s%s);", name, where, name);
}

/*
 * encode/decode TARRAY 
 */

static void
encode_array (char *name, Type *type, FILE *f, EncodeType encodetype,
	      Side side)
{
     if (type->subtype->type == TOPAQUE) {
	  if (type->size % 4 != 0)
	       error_message (1, "Opaque array should be"
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
		    fprintf (f, "if (*total_len < %u) goto fail;\n"
			     "memcpy (ptr, %s, %u);\n", type->size, name,
			     type->size);
		    fprintf (f, "ptr += %u; *total_len -= %u;\n", 
			     type->size, type->size);
		    break;
	       case DECODE_MEM :
		    fprintf (f, "if (*total_len < %u) goto fail;"
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
	  if (type->flags)
	      type->subtype->flags |= type->flags;
	  encode_type (tmp , type->subtype, f, encodetype, side);
	  tmpcnt--;
	  fprintf (f, "}\n}\n");
     }
}

/*
 * encode/decode TARRAY 
 */

static void
print_array (char *where, char *name, Type *type, FILE *f)
{
    fprintf (f, "{\nunsigned int i%lu;\n", tmpcnt);

    fprintf (f, "/* printing ARRAY %s%s */\n", where, name);

    if (type->subtype->type == TOPAQUE) {
	if (type->size % 4 != 0)
	    error_message (1, "print_array: Opaque array should be"
			   "multiple of 4");
	
	fprintf (f, "char *ptr = %s%s;\n", where, name);
	fprintf (f, "printf(\"0x\");");
	fprintf (f, "for (i%lu = 0; i%lu < %d; ++i%lu)\n"
		 "printf(\"%%x\", ptr[i%lu]);",
		 tmpcnt, tmpcnt,
		 type->size, tmpcnt, tmpcnt);

     } else {
	char *ptr;
	fprintf (f, "for (i%lu = 0; i%lu < %d; ++i%lu) {\n", 
		 tmpcnt, tmpcnt, type->size, tmpcnt);
	asprintf(&ptr, "%s%s[i%ld]", where, name, tmpcnt);
	tmpcnt++;
	display_type (ptr, "", type->subtype, f);
	tmpcnt--;
	free(ptr);
	fprintf (f, "\nif (i%lu != %d - 1) printf(\",\");\n", 
		 tmpcnt, type->size);
	
	fprintf (f, "}\n");
     }
    fprintf (f, "}\n");
}

/*
 * encode/decode TVARRAY
 */

static void
encode_varray (char *name, Type *type, FILE *f, EncodeType encodetype,
	       Side side)
{
     char tmp[256];
     Type lentype = {TULONG};
	       
     strlcpy (tmp, name, sizeof tmp);
     strlcat (tmp, ".len", sizeof tmp);

     encode_type (tmp, type->indextype ? type->indextype : &lentype,
		  f, encodetype, side);
     if (encodetype == DECODE_MEM || encodetype == DECODE_RX) {
	 fprintf (f, "%s.val = (", name);
	 print_type ("*", type->subtype, TIN, VDECL, f);
	 fprintf (f, ")malloc(sizeof(");
	 print_type ("", type->subtype, TIN, VDECL, f);
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
	  if (type->flags)
	      type->subtype->flags |= type->flags;
	  encode_type (tmp , type->subtype, f, encodetype, side);
	  tmpcnt--;
	  fprintf (f, "}\n}\n");
     }
}

/*
 * print TVARRAY
 */

static void
print_varray (char *where, char *name, Type *type, FILE *f)
{
    fprintf (f, "{\nunsigned int i%lu;\n", tmpcnt);

    fprintf (f, "/* printing TVARRAY %s%s */\n", where, name);

    if (type->subtype->type == TOPAQUE) {
	fprintf (f, "char *ptr = %s%s.val;\n", where, name);
	fprintf (f, "printf(\"0x\");");
	fprintf (f, "for (i%lu = 0; i%lu < %s%s.len; ++i%lu)\n"
		 "printf(\"%%x\", ptr[i%lu]);",
		 tmpcnt, tmpcnt,
		 where, name, tmpcnt, tmpcnt);
    } else {
	char *ptr;
	fprintf (f, "for (i%lu = 0; i%lu < %s%s.len; ++i%lu) {\n", 
		 tmpcnt, tmpcnt, where, name, tmpcnt);
	asprintf(&ptr, "%s%s.val[i%ld]", where, name, tmpcnt);
	tmpcnt++;
	display_type (ptr, "", type->subtype, f);
	tmpcnt--;
	free(ptr);
	fprintf (f, "\nif (i%lu != %s%s.len - 1) printf(\",\");\n", 
		 tmpcnt, where, name);
	
	fprintf (f, "}\n");
    }
    fprintf (f, "}\n");
}

/*
 * encode/decode pointer
 */

static void
encode_pointer (char *name, Type *type, FILE *f, EncodeType encodetype,
		Side side)
{
     Type booltype = {TULONG};
     char tmp[256];

     snprintf (tmp, sizeof(tmp), "*(%s)", name);

     switch(encodetype) {
     case ENCODE_RX:
	  abort ();
     case ENCODE_MEM:
	  fprintf(f, "{ unsigned bool;\n"
		  "bool = %s != NULL;\n", name);
	  encode_type ("bool", &booltype, f, encodetype, side);
	  fprintf (f, "if(%s) {\n", name);
	  encode_type (tmp, type->subtype, f, encodetype, side);
	  fprintf (f, "}\n"
		   "}\n");
	  break;
     case DECODE_RX:
	  abort();
     case DECODE_MEM:
	  fprintf(f, "{ unsigned bool;\n");
	  encode_type ("bool", &booltype, f, encodetype, side);
	  fprintf (f, "if(bool) {\n");
	  fprintf (f, "%s = malloc(sizeof(%s));\n"
		   "if (%s == NULL) return ENOMEM;\n", 
		   name, tmp, name);
	  encode_type (tmp, type->subtype, f, encodetype, side);
	  fprintf (f, "} else {\n"
		   "%s = NULL;\n"
		   "}\n"
		   "}\n", name);
	  break;
     default:
	  abort ();
     }
}

/*
 * encode type
 */

static void
encode_type (char *name, Type *type, FILE *f, EncodeType encodetype,
	     Side side)
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
	       encode_string (name, type, f, encodetype, side);
	       break;
	  case TOPAQUE :
	       error_message (1,
			      "Type opaque only allowed as part of an array");
	       break;
	  case TUSERDEF :
	       encode_symbol (type->symbol, name, f, encodetype, side);
	       break;
	  case TARRAY :
	       encode_array (name, type, f, encodetype, side);
	       break;
	  case TVARRAY :
	       encode_varray (name, type, f, encodetype, side);
	       break;
	  case TPOINTER :
	       encode_pointer (name, type, f, encodetype, side);
	       break;
	  default :
	       abort();
	  }
}

/*
 * print type
 */

static void
display_type (char *where, char *name, Type *type, FILE *f)
{
    assert (where);

    switch (type->type) {
    case TCHAR :
    case TUCHAR :
#if 0
	print_char (name, type, f, encodetype);
	break;
#endif
    case TSHORT :
    case TUSHORT :
#if 0
	print_short (name, type, f, encodetype);
	break;
#endif
    case TLONG :
    case TULONG :
	print_long (where, name, type, f);
	break;
    case TSTRING :
	print_string (where, name, type, f);
	break;
    case TOPAQUE :
	fprintf (f, "printf(\"printing TOPAQUE\\n\");");
	break;
    case TUSERDEF :
	print_symbol (where, type->symbol, name, f);
	break;
    case TARRAY :
	print_array (where, name, type, f);
	break;
    case TVARRAY :
	print_varray (where, name, type, f);
	break;
    case TPOINTER :
	fprintf (f, "printf(\"printing TPOINTER\\n\");");
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
     Side side;
};

/*
 * helpfunction for encode_struct
 */

static Bool
encode_entry (List *list, Listitem *item, void *arg)
{
     StructEntry *s = (StructEntry *)listdata (item);
     char tmp[256];
     struct context *context = (struct context *)arg;

     strlcpy (tmp, context->name, sizeof tmp);
     strlcat (tmp, ".", sizeof tmp);
     strlcat (tmp, s->name, sizeof tmp);

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
	 encode_type (tmp, s->type, context->f, context->encodetype,
		      context->side);
     }

     return FALSE;
}

/*
 * encode/decode TSTRUCT
 */

static void
encode_struct (Symbol *s, char *name, FILE *f, EncodeType encodetype,
	       Side side)
{
     struct context context;

     context.name       = name;
     context.symbol     = s;
     context.f          = f;
     context.encodetype = encodetype;
     context.side       = side;

     listiter (s->u.list, encode_entry, (void *)&context);
}

/*
 * help function for print_struct
 */

struct printcontext {
    char *where;
    char *name;
    FILE *f;
    Symbol *symbol;
};

static Bool
print_structentry (List *list, Listitem *item, void *arg)
{
     StructEntry *s = (StructEntry *)listdata (item);
     struct printcontext *context = (struct printcontext *)arg;

     char *tmp;
     char *tmp2;

     asprintf(&tmp, ".%s", s->name);
     asprintf(&tmp2, "%s%s", context->where, context->name);

     if (s->type->type == TPOINTER
	 && s->type->subtype->type == TUSERDEF
	 && s->type->subtype->symbol->type == TSTRUCT
	 && strcmp(s->type->subtype->symbol->name,
		   context->symbol->name) == 0) {
	 fprintf (context->f,
		  "ydr_print_%s%s(%s%s, ptr);\n",
		  package,
		  context->symbol->name,
		  tmp2,
		  tmp);
     } else {
	 display_type (tmp2, tmp, s->type, context->f);
     }

     free(tmp);
     free(tmp2);

     fprintf (context->f, "\n");

     return FALSE;
}

/*
 * print TSTRUCT
 */

static void
print_struct (char *where, Symbol *s, char *name, FILE *f)
{
    struct printcontext context;
    
    context.name       = name;
    context.symbol     = s;
    context.f          = f;
    context.where      = where ;

    fprintf (f, "/* printing TSTRUCT %s%s */\n", where, name);
    
    listiter (s->u.list, print_structentry, (void *)&context);
}

/*
 * encode/decode TENUM
 */

static void
encode_enum (Symbol *s, char *name, FILE *f, EncodeType encodetype,
	     Side side)
{
     Type type = {TLONG};

     encode_type (name, &type, f, encodetype, side);
}

/*
 * print TENUM
 */

static Bool
gen_printenum (List *list, Listitem *item, void *arg)
{
    Symbol *s = (Symbol *)listdata (item);
    FILE *f = (FILE *)arg;
    
    fprintf (f, "case %d:\n"
	     "printf(\"%s\");\n"
	     "break;\n", 
	     (int) s->u.val, s->name);

     return FALSE;
}

static void
print_enum (char *where, Symbol *s, char *name, FILE *f)
{
    fprintf (f, "/* print ENUM %s */\n", where);

    fprintf (f, "printf(\"%s = \");", name);
    fprintf (f, "switch(%s) {\n", where);
    listiter (s->u.list, gen_printenum, f);
    fprintf (f, 
	     "default:\n"
	     "printf(\" unknown enum %%d\", %s);\n"
	     "}\n",
	     where);
}

/*
 * encode/decode TTYPEDEF
 */

static void
encode_typedef (Symbol *s, char *name, FILE *f, EncodeType encodetype,
		Side side)
{
     encode_type (name, s->u.type, f, encodetype, side);
}

/*
 * print TTYPEDEF
 */

static void
print_typedef (char *where, Symbol *s, char *name, FILE *f)
{
    display_type (where, name, s->u.type, f);
}

/*
 * Encode symbol/TUSERDEF
 */

static void
encode_symbol (Symbol *s, char *name, FILE *f, EncodeType encodetype,
	       Side side)
{
     switch (s->type) {
	  case TSTRUCT :
	       encode_struct (s, name, f, encodetype, side);
	       break;
	  case TENUM :
	       encode_enum (s, name, f, encodetype, side);
	       break;
	  case TTYPEDEF :
	       encode_typedef (s, name, f, encodetype, side);
	       break;
	  default :
	       abort();
	  }
}

/*
 * print symbol/TUSERDEF
 */

static void
print_symbol (char *where, Symbol *s, char *name, FILE *f)
{
     switch (s->type) {
	  case TSTRUCT :
	       print_struct (where, s, name, f);
	       break;
	  case TENUM :
	       print_enum (where, s, name, f);
	       break;
	  case TTYPEDEF :
	       print_typedef (where, s, name, f);
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
	  error_message (1, "What is %s (type %d) doing here?\n",
			 s->name, s->type);
}

/*
 * Generate the definition of a print function.
 */

static void
generate_printfunction_definition (Symbol *s, FILE *f)
{
     if (s->type == TSTRUCT || s->type == TENUM || s->type == TTYPEDEF) {
	  fprintf (f, 
		   "void ydr_print_%s(%s *o)",
		   s->name, s->name);
     } else if (s->type == TCONST || s->type == TENUMVAL 
	      || s->type == TTYPEDEF)
	  ;
     else
	  error_message (1, "What is %s (type %d) doing here?\n",
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
	  encode_symbol (s, "(*o)", f,
			 encodep ? ENCODE_MEM : DECODE_MEM, CLIENT);
	  fprintf (f, "return ptr;\n"
		   "fail:\n"
		   "errno = EFAULT;\n"
		   "return NULL;}\n");
     } else if (s->type == TCONST || s->type == TENUMVAL 
	      || s->type == TTYPEDEF)
	  ;
     else
	  error_message (1, "What is %s (type %d) doing here?\n",
			 s->name, s->type);
}

/*
 * Generate a print function
 */

void
generate_printfunction (Symbol *s, FILE *f)
{
     if (s->type == TSTRUCT || s->type == TENUM || s->type == TTYPEDEF) {
	  generate_printfunction_definition (s, f);
	  fprintf (f, "\n{\n");
	  print_symbol ("(*o)",  s, "", f);
	  fprintf (f, "return;\n}\n");
     } else if (s->type == TCONST || s->type == TENUMVAL 
	      || s->type == TTYPEDEF)
	  ;
     else
	  error_message (1, "What is %s (type %d) doing here?\n",
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
	  error_message (1, "What is %s (type %d) doing here?\n",
			 s->name, s->type);
}

/*
 * Generate an prototype for a print function
 */

void
generate_printfunction_prototype (Symbol *s, FILE *f)
{
     if (s->type == TSTRUCT || s->type == TENUM || s->type == TTYPEDEF) {
	  generate_printfunction_definition (s, f);
	  fprintf (f, ";\n");
     } else if (s->type == TCONST || s->type == TENUMVAL 
	      || s->type == TTYPEDEF)
	  ;
     else
	  error_message (1, "What is %s (type %d) doing here?\n",
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
	 error_message (1, "Argument %s is OUT and not pointer or string.\n",
			a->name);
     fprintf (f, ", ");
     if (a->argtype == TIN)
	 fprintf (f, "const ");
     print_type (a->name, a->type, a->argtype, FDECL, f);
     fprintf (f, "\n");
     return FALSE;
}

static Bool
genin (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->argtype == TIN || a->argtype == TINOUT) {
	  fprintf (f, ", %s ", a->argtype == TIN ? "const" : "");
	  print_type (a->name, a->type, a->argtype, FDECL, f);
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
	  print_type (a->name, a->type->subtype, TIN, VDECL, f);
     else
	  print_type (a->name, a->type, TIN, VDECL, f);
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
     else if (a->argtype != TIN
	      && a->type->type == TSTRING && a->type->size == 0)
	 fprintf (f, "free(%s);\n", a->name);
     return FALSE;
}

static Bool
genencodein (List *list, Listitem *item, void *arg)
{
    Argument *a = (Argument *)listdata (item);
    FILE *f = (FILE *)arg;
    
    if (a->argtype == TIN || a->argtype == TINOUT) {
	if (a->type->type == TPOINTER) {
	    size_t len = strlen (a->name) + 4;
	    char *tmp = (char *)emalloc (len);
	    
	    snprintf (tmp, len, "(*%s)", a->name);
	    
	    encode_type (tmp, a->type->subtype, f, ENCODE_RX, CLIENT);
	    free (tmp);
	} else
	    encode_type (a->name, a->type, f, ENCODE_RX, CLIENT);
    }
    return FALSE;
}

static Bool
gendecodeout (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->argtype == TOUT || a->argtype == TINOUT) {
	 if (a->type->type == TPOINTER) {
	     size_t len = strlen(a->name) + 4;
	     char *tmp = (char *)emalloc (len);
	       
	     snprintf (tmp, len, "(*%s)", a->name);

	     encode_type (tmp, a->type->subtype, f, DECODE_RX, CLIENT);
	     free (tmp);
	 } else if(a->type->type == TSTRING) {
	     encode_type (a->name, a->type, f, DECODE_RX, CLIENT);
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
	       size_t len = strlen(a->name) + 4;
	       char *tmp = (char *)emalloc (len);
	       
	       snprintf (tmp, len, "(*%s)", a->name);

	       encode_type (tmp, a->type->subtype, f, DECODE_RX, SERVER);
	       free (tmp);
#endif
	       encode_type (a->name, a->type->subtype, f, DECODE_RX, SERVER);
	  } else
	       encode_type (a->name, a->type, f, DECODE_RX, SERVER);
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
	       encode_type (a->name, a->type->subtype, f, ENCODE_RX, SERVER);
	  else
	       encode_type (a->name, a->type, f, ENCODE_RX, SERVER);
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

     if (a->type->type == TPOINTER
	 || (a->argtype != TIN
	     && a->type->type == TSTRING && a->type->size == 0))
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
     
     encode_type (op, &type, f, ENCODE_RX, CLIENT);
     free (op);
     listiter (s->u.proc.arguments, genencodein, f);
     listiter (s->u.proc.arguments, gendecodeout, f);
     fprintf (f,
	      "return %s(rx_EndCall (call,0));\n"
	      "fail:\n"
	      "ret = %s(rx_Error(call));\n"
	      "rx_EndCall (call, 0);\n"
	      "return ret;\n"
	      "}\n",
	      error_function,
	      error_function);
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
     encode_type (op, &type, f, ENCODE_RX, CLIENT);
     free (op);
     listiter (s->u.proc.arguments, genencodein, f);
     fprintf (f, "return 0;\n");
     /* XXX only in arg */
     if (findargtype(s->u.proc.arguments, TIN) || 
	 findargtype(s->u.proc.arguments, TINOUT))
	 fprintf (f, "fail:\n"
                 "return %s(rx_Error(call));\n",
		  error_function);

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
                 "return %s(rx_Error(call));\n",
		  error_function);

     fprintf (f, "}\n\n");
}

struct gen_args {
    FILE *f;
    int firstp;
    int arg_type;
};

static Bool
genmacro (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     struct gen_args *args = (struct gen_args *)arg;

     if (a->argtype == args->arg_type || a->argtype == TINOUT) {
	 fprintf (args->f, "%s%s",
		  args->firstp ? "" : ", ", a->name);
	 args->firstp = 0;
     }

     return FALSE;
}

static void
generate_multi (Symbol *s, FILE *f)
{
    struct gen_args gen_args;

    fprintf (f, "\n#include <rx/rx_multi.h>");
    fprintf (f, "\n#define multi_%s%s(", package, s->name);
    gen_args.f        = f;
    gen_args.firstp   = 1;
    gen_args.arg_type = TIN;
    listiter (s->u.proc.arguments, genmacro, &gen_args);
    fprintf (f, ") multi_Body(");
    fprintf (f, "Start%s%s(multi_call", package, s->name);
    gen_args.f        = f;
    gen_args.firstp   = 0;
    gen_args.arg_type = TIN;
    listiter (s->u.proc.arguments, genmacro, &gen_args);
    fprintf (f, "), End%s%s(multi_call", package, s->name);
    gen_args.f        = f;
    gen_args.firstp   = 0;
    gen_args.arg_type = TOUT;
    listiter (s->u.proc.arguments, genmacro, f);
    fprintf (f, "))\n");
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

static void
generate_standard_c_prologue (FILE *f,
			      const char *filename,
			      const char *basename)
{
     fprintf (f, "/* Generated from %s.xg */\n", basename);
     fprintf (f, "#include \"%s.h\"\n\n", basename);
     fprintf (f, "#include <stdio.h>\n");
     fprintf (f, "#include <stdlib.h>\n");
     fprintf (f, "#include <string.h>\n");
     fprintf (f, "#include <netinet/in.h>\n");
     fprintf (f, "#include <errno.h>\n");
     fprintf (f, "#ifndef HAVE_BCOPY\n"
	      "#define bcopy(a,b,c) memcpy((b),(a),(c))\n"
	      "#endif /* !HAVE_BCOPY */\n\n");
     fprintf (f, "#ifdef RCSID\n"
	      "RCSID(\"%s generated from %s.xg with $KTH: output.c,v 1.73.2.1 2001/03/04 04:48:48 lha Exp $\");\n"
	      "#endif\n\n", filename, basename);
}

/*
 * open all files
 */

void
init_generate (const char *filename)
{
     char *tmp;
     char *fileuppr;

     func_list = listnew ();

     asprintf (&tmp, "%s.h", filename);
     if (tmp == NULL)
	 err (1, "malloc");
     ydr_fopen (tmp, "w", &headerfile);
     free (tmp);

     fileuppr = estrdup (filename);
     strupr (fileuppr);
     fprintf (headerfile.stream, "/* Generated from %s.xg */\n", filename);
     fprintf (headerfile.stream, "#ifndef _%s_\n"
	      "#define _%s_\n\n", fileuppr, fileuppr);
     fprintf (headerfile.stream, "#include <atypes.h>\n\n");
     free (fileuppr);
     
     asprintf (&tmp, "%s.ydr.c", filename);
     if (tmp == NULL)
	 err (1, "malloc");
     ydr_fopen (tmp, "w", &ydrfile);
     generate_standard_c_prologue (ydrfile.stream, tmp, filename);
     free (tmp);

     asprintf (&tmp, "%s.cs.c", filename);
     if (tmp == NULL)
	 err (1, "malloc");
     ydr_fopen (tmp, "w", &clientfile);
     generate_standard_c_prologue (clientfile.stream, tmp, filename);
     fprintf (clientfile.stream, "#include \"%s.cs.h\"\n\n", filename);
     free (tmp);

     asprintf (&tmp, "%s.ss.c", filename);
     if (tmp == NULL)
	 err (1, "malloc");
     ydr_fopen (tmp, "w", &serverfile);
     generate_standard_c_prologue (serverfile.stream, tmp, filename);
     fprintf (serverfile.stream, "#include \"%s.ss.h\"\n\n", filename);
     free (tmp);

     asprintf (&tmp, "%s.cs.h", filename);
     if (tmp == NULL)
	 err (1, "malloc");
     ydr_fopen (tmp, "w", &clienthdrfile);
     free (tmp);
     fprintf (clienthdrfile.stream, "/* Generated from %s.xg */\n", filename);
     fprintf (clienthdrfile.stream, "#include <rx/rx.h>\n");
     fprintf (clienthdrfile.stream, "#include \"%s.h\"\n\n", filename);

     asprintf (&tmp, "%s.ss.h", filename);
     if (tmp == NULL)
	 err (1, "malloc");
     ydr_fopen (tmp, "w", &serverhdrfile);
     free (tmp);
     fprintf (serverhdrfile.stream, "/* Generated from %s.xg */\n", filename);
     fprintf (serverhdrfile.stream, "#include <rx/rx.h>\n");
     fprintf (serverhdrfile.stream, "#include \"%s.h\"\n\n", filename);

     asprintf (&tmp, "%s.td.c", filename);
     if (tmp == NULL)
	 err (1, "malloc");
     ydr_fopen (tmp, "w", &td_file);
     free (tmp);

     packagelist = listnew();
     if (packagelist == NULL)
	 err (1, "init_generate: listnew: packagelist");
}

void
close_generator (const char *filename)
{
     char *fileupr = estrdup (filename);
	  
     strupr (fileupr);
     fprintf (headerfile.stream, "\n#endif /* %s */\n", fileupr);
     ydr_fclose (&headerfile);
     ydr_fclose (&clientfile);
     ydr_fclose (&serverfile);
     ydr_fclose (&clienthdrfile);
     ydr_fclose (&serverhdrfile);
     ydr_fclose (&ydrfile);
     ydr_fclose (&td_file);
}

/*
 * Generate the server-side stub function for the function in s and
 * write it to the file f.
 */

void
generate_server_stub (Symbol *s, FILE *f, FILE *headerf, FILE *h_file)
{
     fprintf (headerf, "int _%s%s(\nstruct rx_call *call);\n",
	      package, s->name);
     fprintf (headerf, "int %s%s%s(\nstruct rx_call *call\n",
	      prefix, package, s->name);
     listiter (s->u.proc.arguments, gen1, headerf);
     fprintf (headerf, ");\n\n");

     fprintf (f, "int _%s%s(\nstruct rx_call *call)\n",
	      package, s->name);
     fprintf (f, "{\n"
	      "int32_t _result;\n");
     listiter (s->u.proc.arguments, gendeclare, f);
     fprintf (f, "\n");
     listiter (s->u.proc.arguments, gendecodein, f);
     fprintf (f, "_result = %s%s%s(", prefix, package, s->name);
     if (/* s->u.proc.splitp */ 1) {
	  fprintf (f, "call");
	  if (!listemptyp (s->u.proc.arguments))
	       fprintf (f, ", ");
     }
     listiter (s->u.proc.arguments, genargs, f);
     fprintf (f, ");\n");
     fprintf (f, "if (_result) goto funcfail;\n");
     listiter (s->u.proc.arguments, genencodeout, f);
     listiter (s->u.proc.arguments, genfree, f);
     fprintf (f, "return _result;\n");
     if (!listemptyp(s->u.proc.arguments)) {
	 fprintf(f, "fail:\n");
	 listiter (s->u.proc.arguments, genfree, f);
	 fprintf(f, "return rx_Error(call);\n");

     }
     fprintf(f, "funcfail:\n"
	     "return _result;\n"
	     "}\n\n");

     listaddtail (func_list, s);
     if (s->u.proc.flags & TMULTI)
	 generate_multi (s, h_file);
}

struct gencase_context {
    FILE *f;
    char *package;
};

static Bool
gencase (List *list, Listitem *item, void *arg)
{
     Symbol *s = (Symbol *)listdata (item);
     struct gencase_context *c = (struct gencase_context *)arg;
     FILE *f = c->f;

     if (c->package == s->u.proc.package) {
	 fprintf (f, "case %u: {\n"
		  "_result = _%s%s(call);\n"
		  "break;\n"
		  "}\n",
		  s->u.proc.id, s->u.proc.package, s->name);
     }
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
     Listitem *li;
     struct gencase_context c;

     c.f = c_file;
     
     li = listhead (packagelist);
     while (li) {
	 c.package = (char *)listdata (li);

	 fprintf (h_file,
		  "int32_t %sExecuteRequest(struct rx_call *call);\n",
		  c.package);
	 
	 fprintf (c_file,
		  "int32_t %sExecuteRequest(struct rx_call *call)\n"
		  "{\n"
		  "unsigned opcode;\n"
		  "int32_t _result;\n",
		  c.package);
	 
	 encode_type ("opcode", &optype, c_file, DECODE_RX, SERVER);
	 fprintf (c_file, "switch(opcode) {\n");
	 
	 listiter (func_list, gencase, &c);
	 
	 fprintf (c_file, "default:\n"
 		  "_result = RXGEN_OPCODE;\n"
		  "fprintf (stderr, \"Ignoring %%d\\n\", opcode);\n"
		  "}\n"
		  "return _result;\n"
		  "fail:\n"
		  "return rx_Error(call);\n"
		  "}\n\n");

	 li = listnext (packagelist, li);
     }
}

static Bool
gentcpdump_decode (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     if (a->type->type == TPOINTER)
	 encode_type (a->name, a->type->subtype, f, ENCODE_MEM, CLIENT);
     else
	 encode_type (a->name, a->type, f, ENCODE_MEM, CLIENT);

     fprintf (f, "++found;\n");
     return FALSE;
}

/*
 *
 */

static Bool
gentcpdump_print (List *list, Listitem *item, void *arg)
{
     Argument *a = (Argument *)listdata (item);
     FILE *f = (FILE *)arg;

     fprintf (f, "if (found > 0)\n\n"
	      "{ --found;\n");

     if (a->type->type == TPOINTER)
	 display_type ("", a->name, a->type->subtype, f);
     else
	 display_type ("", a->name, a->type, f);

     if (listnextp(item))
	 fprintf (f, "printf(\",\");\n");
     
     fprintf (f, "} else\n"
	      "goto failandquit;\n");
     
     return FALSE;
}

/*
 *
 */

void
generate_tcpdump_stub (Symbol *s, FILE *f)
{
     fprintf (f, "void %s%s_print(char *ptr, size_t *total_len)\n"
	      "{\n"
	      "int found = 0;\n",
	      package, s->name);
     listiter (s->u.proc.arguments, gendeclare, f);
     fprintf (f, "\n");

     listiter (s->u.proc.arguments, gentcpdump_decode, f);
     if (!listemptyp(s->u.proc.arguments))
	 fprintf(f, "fail:\n");
     fprintf (f, "printf(\"{\");");
     listiter (s->u.proc.arguments, gentcpdump_print, f);
     fprintf (f, "printf(\"}\\n\");");

     fprintf (f, "\nreturn ptr;\n");
     if (!listemptyp(s->u.proc.arguments))
	 fprintf(f, "failandquit:\n"
		 "printf(\"Error decoding paket\");\n"
		 "return NULL;");
     fprintf(f, "}\n\n");
}


static Bool
gentcpdump_case (List *list, Listitem *item, void *arg)
{
     Symbol *s = (Symbol *)listdata (item);
     FILE *f = (FILE *)arg;

     fprintf (f, "case %u:\n"
	      "printf(\"%s%s(\");\n"
	      "ret = %s%s_print(ptr, &total_len);\n"
	      "break;\n",
	      s->u.proc.id,
	      package,
	      s->name,
	      package,
	      s->name);

     return FALSE;
}

/*
 * generate tcpdump patches
 */


void
generate_tcpdump_patches(FILE *td_file, const char *filename)
{
    Type optype = {TULONG};

    fprintf(td_file, "/* generated by $KTH: output.c,v 1.73.2.1 2001/03/04 04:48:48 lha Exp $ */\n\n");

    fprintf (td_file, 
	     "#include <stdio.h>\n"
	     "#include <stdlib.h>\n"
	     "#include \"%s.h\"\n",
	     filename);

    fprintf (td_file, "%stcpdump_print(const unsigned char *ptr,"
	     "unsigned int len,"
	     "unsigned char *bp2)\n"
	     "{\n"
	     "u_int32_t opcode;\n"
	     "size_t *total_len = len;\n"
	     "char *ret;\n",
	     package);
    
    encode_type ("opcode", &optype, td_file, DECODE_MEM, CLIENT);

    fprintf (td_file, "switch(opcode) {\n");

    listiter (func_list, gentcpdump_case, td_file);
    
    fprintf (td_file, "default:\n"
	     "printf (\"Unknown opcode %%s->%%d\\n\", "
	     "package, opcode);\n"
	     "}\n"
	     "fail:\n"
	     "printf(\"Error decoding packet\\n\");\n"
	     "}\n\n");

}

void
ydr_fopen (const char *name, const char *mode, ydr_file *f)
{
     int streamfd;

     asprintf (&f->curname, "%sXXXXXX", name);
     if (f->curname == NULL)
	 err (1, "malloc");

     streamfd = mkstemp(f->curname);
     if (streamfd < 0)
	 err (1, "mkstemp %s failed", f->curname);
     f->stream = fdopen (streamfd, mode);
     if (f->stream == NULL)
	 err (1, "open %s mode %s", f->curname, mode);
     f->newname = estrdup(name);
}

void
ydr_fclose (ydr_file *f)
{
     if (fclose (f->stream))
	 err (1, "close %s", f->curname);
     if (rename(f->curname, f->newname))
	 err (1, "rename %s, %s", f->curname, f->newname);
     free(f->curname);
     free(f->newname);
}
