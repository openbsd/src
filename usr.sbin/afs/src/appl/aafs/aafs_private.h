/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef AAFS_PRIVATE_H
#define AAFS_PRIVATE_H 1

#include <aafs/aafs_cell.h>
#include <fs.h>

#include <log.h>

struct aafs_object {
    const char *type;
    int refcount;
    void (*destruct)(void *, const char *);
};

void	*aafs_object_create(size_t, const char *, void (*)(void *, const char *));
void	*aafs_object_ref(void *, const char *type);
void 	aafs_object_unref(void *, const char *type);

void	*aafs_cell_ref(struct aafs_cell *);
void	aafs_cell_unref(struct aafs_cell *);

struct aafs_cell;

struct aafs_server {
    struct aafs_object obj;
    struct aafs_cell *cell;
    unsigned flags;
#define SERVER_HAVE_ADDR	1
#define SERVER_HAVE_UUID	2
    int				num_addr;
    struct site_addr {
	struct sockaddr_storage   addr;
	int			  addrlen;
    } 				*addr; 
    afsUUID			uuid;
};


#endif /* AAFS_CELL_H */
