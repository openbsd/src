/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
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

/* $arla: ptserver.h,v 1.10 2001/10/12 15:25:50 tol Exp $ */

void
write_header(void);

void
read_header(void);

int
read_prentry(int id, prentry *pr_entry);

int
write_prentry(prentry *pr_entry);

int
store_disk_entry(prentry_disk *entry);

int
get_disk_entry(int id, prentry_disk *disk_entry);

int
get_ydr_disk_entry(int id, char **buf);

int
conv_name_to_id(const char *name, int *id);

int
conv_id_to_name(int id, char *name);

int
next_free_group_id(int *id);

int
next_free_user_id(int *id);

int
create_user(const char *name, int32_t id, int32_t owner, int32_t creator);
int
create_group(const char *name, int32_t id, int32_t owner, int32_t creator);

int
addtogroup(int32_t uid, int32_t gid);

int
removefromgroup(int32_t uid, int32_t gid);

int
listelements(int32_t id, prlist *elist, Bool default_id_p);

char *
localize_name(const char *name, Bool *localp);

extern prheader_disk pr_header;


