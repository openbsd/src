/*
 * Copyright (c) 2001 Kungliga Tekniska Högskolan
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

struct mdb_datum {
    void *data;
    int length;
};

typedef struct MDB {
    void *db;
    char *name;
    int openp;
    int flags;
    mode_t mode;

    int (*open)(struct MDB*);
    int (*close)(struct MDB*);
    int (*store)(struct MDB*, struct mdb_datum *key, struct mdb_datum *value);
    int (*fetch)(struct MDB*, struct mdb_datum *key, struct mdb_datum *value);
    int (*delete)(struct MDB*, struct mdb_datum *key);
    int (*flush)(struct MDB*);
} MDB;

int mdb_NDBM_create(MDB **db, const char * filename, int flags, mode_t mode);

int mdb_store(MDB *db, struct mdb_datum *key, struct mdb_datum *value);
int mdb_fetch(MDB *db, struct mdb_datum *key, struct mdb_datum *value);
int mdb_delete(MDB *db, struct mdb_datum *key);
MDB *mdb_open(char *filename, int flags, mode_t mode);
int mdb_close(MDB *db);
int mdb_flush(MDB *db);
