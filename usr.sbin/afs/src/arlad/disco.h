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

/* $arla: disco.h,v 1.2 2002/09/07 10:43:04 lha Exp $ */

#ifndef ARLA_DISCO_H
#define ARLA_DISCO_H 1

#define ARLA_LOG	"arla-disco-log"

enum {
    DISCO_OP_NOP = 0,
    DISCO_OP_STOREDATA,
    DISCO_OP_STORESTATUS,
    DISCO_OP_CREATE_FILE,
    DISCO_OP_CREATE_SYMLINK,
    DISCO_OP_CREATE_LINK,
    DISCO_OP_UNLINK,
    DISCO_OP_RENAME,
    DISCO_OP_CREATE_DIR,
    DISCO_OP_REMOVE_DIR,
    DISCO_OP_MAX_OPCODE
};

#define DISCO_OP_NAMES {			\
    "nop",					\
    "storedata",				\
    "storestatus",				\
    "create-file",				\
    "create-symlink",				\
    "create-link",				\
    "unlink",					\
    "rename",					\
    "crete-dir",				\
    "remove-dir"				\
}

struct disco_header {
    uint32_t checksum;
    uint32_t flags;
#define DISCO_HEADER_NOP	1
    uint32_t prev_id;
    uint16_t opcode;
    uint16_t size;
};

struct disco_nop {
    struct disco_header header;
};

struct disco_store_data {
    struct disco_header header;
    VenusFid	fid;
    AFSStoreStatus storestatus;
};

struct disco_store_status {
    struct disco_header header;
    VenusFid	fid;
    AFSStoreStatus storestatus;
};

struct disco_create_file {
    struct disco_header header;
    VenusFid parentfid;
    AFSStoreStatus storestatus;
    VenusFid fid;
    char name[AFSNAMEMAX];
};

struct disco_create_symlink {
    struct disco_header header;
    VenusFid parentfid;
    AFSStoreStatus storestatus;
    VenusFid fid;
    char name[AFSNAMEMAX];
    char targetname[AFSNAMEMAX];
};

struct disco_create_link {
    struct disco_header header;
    VenusFid parentfid;
    AFSStoreStatus storestatus;
    VenusFid fid;
    char name[AFSNAMEMAX];
    VenusFid targetfid;
};

struct disco_unlink {
    struct disco_header header;
    VenusFid parentfid;
    VenusFid fid;
    char name[AFSNAMEMAX];
};

struct disco_rename {
    struct disco_header header;
    VenusFid sourcepfid;
    VenusFid destpfid;
    char sourcename[AFSNAMEMAX];
    char destname[AFSNAMEMAX];
};


struct disco_create_dir {
    struct disco_header header;
    VenusFid parentfid;
    AFSStoreStatus storestatus;
    VenusFid fid;
    char name[AFSNAMEMAX];
};

struct disco_remove_dir {
    struct disco_header header;
    VenusFid parentfid;
    VenusFid fid;
    char name[AFSNAMEMAX];
};

/*
 *
 */

int
disco_openlog(void);

int
disco_closelog(void);

int
disco_reintegrate(nnpfs_pag_t pag);

int
disco_need_integrate(void);

/* operations */

void
disco_nop_entry(void);

uint32_t
disco_store_status(VenusFid *fid, AFSStoreStatus *ss, uint32_t prev_disco_id);

uint32_t
disco_store_data(VenusFid *fid, AFSStoreStatus *ss, uint32_t prev_disco_id);

uint32_t
disco_create_file(VenusFid *parent, VenusFid *fid, char *name,
		  AFSStoreStatus *ss);

uint32_t
disco_create_symlink(VenusFid *parent, VenusFid *fid, char *name,
		     AFSStoreStatus *ss);

uint32_t
disco_create_link(VenusFid *parent, VenusFid *fid, char *name,
		  AFSStoreStatus *ss);

uint32_t
disco_unlink(VenusFid *parent, VenusFid *fid, const char *name, uint32_t prev_id);

uint32_t
disco_create_dir(VenusFid *parent, VenusFid *fid, char *name,
		 AFSStoreStatus *ss);

uint32_t
disco_remove_dir(VenusFid *parent, VenusFid *fid, char *name, uint32_t prev_id);

struct disco_play_context;

/* printing/iteration support */

#define DISCO_MAX_BUF_SZ	400 /* XXX */

int
disco_init_context(struct disco_play_context **);

int
disco_next_entry(struct disco_play_context *, void *ptr, size_t sz);

int
disco_close_context(struct disco_play_context *);

void
disco_print_entry(FILE *f, void *ptr, size_t sz);

#endif /* ARLA_DISCO_H */
