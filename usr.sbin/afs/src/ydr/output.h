/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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

/* $arla: output.h,v 1.17 2002/11/27 23:51:35 lha Exp $ */

#ifndef _OUTPUT_
#define _OUTPUT_

#include <stdio.h>
#include <bool.h>
#include <roken.h>

typedef struct {
  FILE *stream;
  char *curname;
  char *newname;
} ydr_file;

void generate_header (Symbol *s, FILE *f);
void generate_sizeof (Symbol *s, FILE *f);
void generate_function (Symbol *s, FILE *f, Bool encodep);
void generate_function_prototype (Symbol *s, FILE *f, Bool encodep);
void generate_printfunction (Symbol *s, FILE *f);
void generate_printfunction_prototype (Symbol *s, FILE *f);
void generate_client_stub (Symbol *s, FILE *f, FILE *headerf);
void generate_server_stub (Symbol *s, FILE *f, FILE *headerf, FILE *h_file);
void generate_tcpdump_stub (Symbol *s, FILE *f);
void generate_server_switch (FILE *c_f, FILE *h_file);
void generate_freefunction (Symbol *s, FILE *f);
void generate_freefunction_prototype (Symbol *s, FILE *f);
void init_generate (const char *filename);
void close_generator (const char *filename);

extern char *package;
extern List *packagelist;

extern char *prefix;

extern ydr_file headerfile, clientfile, serverfile, clienthdrfile,
    serverhdrfile, ydrfile;

extern char *error_function;

extern int parse_errors;

void
ydr_fopen (const char *name, const char *mode, ydr_file *f);

void
ydr_fclose (ydr_file *f);


#endif /* _OUTPUT_ */
