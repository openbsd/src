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

#include "arla_local.h"
#ifdef RCSID
RCSID("$arla: disco.c,v 1.3 2002/07/17 09:28:59 mattiasa Exp $") ;
#endif

const char *op_names[] = DISCO_OP_NAMES;

static int log_fd = -1;
static unsigned long modified_log = 0;

/*
 * Transaction log entries
 */

/*
 * Open the log on disk
 */

int
disco_openlog(void)
{
    int save_errno;

    if (log_fd >= 0)
	return 0;

    log_fd = open(ARLA_LOG, O_RDWR | O_BINARY | O_CREAT, 0644);
    if (log_fd < 0) {
	save_errno = errno;
#if 0
	arla_warn (ADEBWARN, save_errno, "can't open arla logfile %s",
		   ARLA_LOG);
#endif
	return save_errno;
    }

    /* if this is a new file, make sure there is a nop */
    if (lseek(log_fd, 0, SEEK_END) == 0)
	disco_nop_entry();

    return 0;
}

/*
 * Close the log
 */

int
disco_closelog(void)
{
    int ret;
    if (log_fd < 0)
	return 0;

    ret = close(log_fd);
    log_fd = -1;
    if (ret < 0)
	return errno;
    return 0;
}

/*
 *
 */

static uint32_t
checksum_of_data(void *data, size_t sz)
{
    /* XXX implement me */
    return 0;
}

/*
 *
 */

static int
validate_item(void *data, size_t size)
{
    struct disco_header *h = data;

    if (checksum_of_data(((unsigned char *)data) + 4, size - 4) == h->checksum)
	return 1;
    return 0;
}

/*
 *
 */

static uint32_t
write_log_entry(uint16_t opcode, void *data, uint16_t size, 
		int32_t flags, uint32_t prev_id, uint32_t offset)
{
    struct disco_header *h = data;
    uint32_t disco_id;
    int sz;

    assert(log_fd >= 0);
    assert(size >= sizeof(struct disco_header));
    assert(size <= DISCO_MAX_BUF_SZ);

    h->flags = flags;
    h->opcode = opcode;
    h->size = size;
    h->prev_id = prev_id;

    h->checksum = checksum_of_data(((unsigned char *)data) + 4, size - 4);

    if (offset == 0)
	disco_id = lseek(log_fd, 0, SEEK_END);
    else
	disco_id = lseek(log_fd, offset, SEEK_SET);

    sz = write(log_fd, data, size);
    if (sz != size)
	abort();

    modified_log++;

    return disco_id;
}

/* 
 *
 */

static int
read_entry(uint32_t log_item, void *data, size_t *sz)
{
    struct disco_header *h;
    ssize_t ret;
    assert(log_fd >= 0);

    assert ((((unsigned long)data) & 3) == 0); /* XXX */

    lseek(log_fd, log_item, SEEK_SET);

    ret = read(log_fd, data, sizeof(struct disco_header));
    if (ret == 0)
	return -1;
    if (ret != sizeof(struct disco_header))
	abort();

    h = data;
    if (h->size > *sz)
	abort();
    *sz = h->size;

    ret = read(log_fd, h + 1, *sz - sizeof(struct disco_header));
    if (ret < *sz - sizeof(struct disco_header))
	return -1;

    if (!validate_item(data, *sz))
	abort();

    return 0;
}

/*
 *
 */

static void
nop_entry(uint32_t disco_id, uint16_t opcode, int32_t flags, void *buf, size_t sz)
{
    write_log_entry(opcode, buf, sz, flags, 0, disco_id);
}

/*
 * Nop the operation chain for disco_id an up, return non-zero if the
 * entry was a disconncted node.
 */

static int
nop_chain(uint32_t disco_id, VenusFid *newparent,
	  char *newname, size_t namesz)
{
    char buf[DISCO_MAX_BUF_SZ];
    struct disco_header *h = (struct disco_header *)buf;
    uint32_t nop_id;
    int disco = 0, last_dir_found = 0;
    size_t sz;

    while (disco_id) {
	nop_id = disco_id;
	sz = sizeof(buf);
	if (read_entry(disco_id, buf, &sz))
	    abort();
	disco_id = h->prev_id;

	if (disco_id == 0) {
	    /* last of link in chain */
	    switch (h->opcode) {
	    case DISCO_OP_CREATE_DIR:
	    case DISCO_OP_CREATE_FILE:
	    case DISCO_OP_CREATE_SYMLINK:
	    case DISCO_OP_CREATE_LINK:
		disco = 1;
	    }
	}

	if (h->opcode == DISCO_OP_RENAME && !last_dir_found) {
	    /* keep track of name changes, so we can remove it in old
	     * directory when connecting */
	    struct disco_rename *rn = (struct disco_rename *)buf;
	    
	    assert(sz == sizeof(struct disco_rename));
	    
	    *newparent = rn->sourcepfid;
	    strlcpy(newname, rn->sourcename, namesz);
	    last_dir_found = 1;
	}
	
	assert (nop_id > h->prev_id);
	nop_entry(nop_id, h->opcode, h->flags | DISCO_HEADER_NOP, buf, sz);
    }
    return disco;
}

uint32_t
disco_store_status(VenusFid *fid, AFSStoreStatus *ss, uint32_t prev_disco_id)
{
    struct disco_store_status e;

    memset(&e, 0, sizeof(e));

    e.fid = *fid;
    e.storestatus = *ss;

    return write_log_entry(DISCO_OP_STORESTATUS, 
			   &e, sizeof(e), 
			   prev_disco_id, 0, 0);
}


uint32_t
disco_store_data(VenusFid *fid, AFSStoreStatus *ss, uint32_t prev_disco_id)
{
    struct disco_store_data e;

    memset(&e, 0, sizeof(e));

    e.fid = *fid;
    e.storestatus = *ss;

    return write_log_entry(DISCO_OP_STOREDATA, 
			   &e, sizeof(e), 
			   prev_disco_id, 0, 0);
}


uint32_t
disco_create_file(VenusFid *parent, VenusFid *fid, char *name,
		  AFSStoreStatus *ss)
{
    struct disco_create_file e;
    memset(&e, 0, sizeof(e));

    e.parentfid = *parent;
    e.fid = *fid;
    strlcpy(e.name, name, sizeof(e.name));
    e.storestatus = *ss;

    return write_log_entry(DISCO_OP_CREATE_FILE, &e, sizeof(e), 0, 0, 0);
}

uint32_t
disco_create_dir(VenusFid *parent, VenusFid *fid, char *name,
		  AFSStoreStatus *ss)
{
    struct disco_create_dir e;
    memset(&e, 0, sizeof(e));

    e.parentfid = *parent;
    e.fid = *fid;
    strlcpy(e.name, name, sizeof(e.name));
    e.storestatus = *ss;

    return write_log_entry(DISCO_OP_CREATE_DIR, &e, sizeof(e), 0, 0, 0);
}

uint32_t
disco_unlink(VenusFid *parent, VenusFid *fid, const char *name, uint32_t prev_id)
{
    VenusFid newparent = *parent;
    char newname[AFSNAMEMAX];
    int discon;
    
    strlcpy(newname, name, sizeof(newname));

    discon = nop_chain(prev_id, &newparent, newname, sizeof(newname));
    
    if (!discon) {
	struct disco_unlink e;
	memset(&e, 0, sizeof(e));

	e.parentfid = newparent;
	e.fid = *fid;
	strlcpy(e.name, newname, sizeof(e.name));

	return write_log_entry(DISCO_OP_UNLINK, &e, sizeof(e), 0, 0, 0);
    }
    return 0;
}

/*
 *
 */

void
disco_nop_entry(void)
{
    char buf[sizeof(struct disco_nop)];
    memset(buf, 0, sizeof(buf));
    nop_entry(0, DISCO_OP_NOP, 0, buf, sizeof(struct disco_nop));
}

/*
 * Replay support
 */

struct disco_play_context {
    uint32_t next_disco_id;
};

/*
 *
 */

int
disco_init_context(struct disco_play_context **c_ret)
{
    struct disco_play_context *c;

    *c_ret = NULL;
    c = malloc(sizeof(*c));
    if (c == NULL)
	return ENOMEM;
    c->next_disco_id = 0;
    *c_ret = c;
    return 0;
}

/*
 *
 */

int
disco_next_entry(struct disco_play_context *c, void *ptr, size_t sz)
{
    assert(c != NULL);

    if (read_entry(c->next_disco_id, ptr, &sz))
	return -1;

    c->next_disco_id += sz;
    return 0;
}

/*
 *
 */

int
disco_close_context(struct disco_play_context *c)
{
    free(c);
    return 0;
}

/*
 *
 */

void
disco_print_entry(FILE *f, void *ptr, size_t sz)
{
    struct disco_header *h = ptr;
    const char *nop_p = "", *ph;
    char buf[10];

    if (sz < sizeof(struct disco_header))
	return;

    if (h->opcode < sizeof(op_names)/sizeof(op_names[0]))
	ph = op_names[h->opcode];
    else {
	snprintf(buf, sizeof(buf), "%d", h->opcode);
	ph = buf;
    }

    if (h->flags & DISCO_HEADER_NOP)
	nop_p = "entry optimized away";

    fprintf(f, "entry opcode %s, sz %d %s\n", 
	    ph, h->size, nop_p);

    switch(h->opcode) {
    case DISCO_OP_CREATE_FILE: {
	struct disco_create_file *e = ptr;
	
	fprintf(f, "  parent: %d.%d.%d.%d name: %s fid: %d.%d.%d.%d\n",
		e->parentfid.Cell,
		e->parentfid.fid.Volume,
		e->parentfid.fid.Vnode,
		e->parentfid.fid.Unique,
		e->name, 
		e->fid.Cell,
		e->fid.fid.Volume,
		e->fid.fid.Vnode,
		e->fid.fid.Unique);
	break;
    }
    case DISCO_OP_UNLINK: {
	struct disco_unlink *e = ptr;
	
	fprintf(f, "  parent: %d.%d.%d.%d name: %s fid: %d.%d.%d.%d\n",
		e->parentfid.Cell,
		e->parentfid.fid.Volume,
		e->parentfid.fid.Vnode,
		e->parentfid.fid.Unique,
		e->name, 
		e->fid.Cell,
		e->fid.fid.Volume,
		e->fid.fid.Vnode,
		e->fid.fid.Unique);
	break;
    }

    default:
	break;
    }

}


int
disco_need_integrate(void)
{
#if 1
    return modified_log;
#else
    return 0;
#endif
}


