/*
 * difffile.h - nsd.diff file handling header file. Read/write diff files.
 *
 * Copyright (c) 2001-2011, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */
#ifndef DIFFFILE_H
#define DIFFFILE_H

#include <config.h>
#include "rbtree.h"
#include "namedb.h"
#include "options.h"

#define DIFF_PART_IXFR ('I'<<24 | 'X'<<16 | 'F'<<8 | 'R')
#define DIFF_PART_SURE ('S'<<24 | 'U'<<16 | 'R'<<8 | 'E')

/*
 * Used to pass commit logs
 */
struct diff_log {
	char* zone_name;
	char* error;
	char* comment;
	struct diff_log* next;
};

/* write an xfr packet data to the diff file, type=IXFR.
   The diff file is created if necessary. */
void diff_write_packet(const char* zone, uint32_t new_serial, uint16_t id,
	uint32_t seq_nr, uint8_t* data, size_t len, nsd_options_t* opt);

/*
 * Write a commit packet to the diff file, type=SURE.
 * The zone data (preceding ixfr packets) are committed.
 * See NSD-DIFFFILE for meaning of the arguments.
 */
void diff_write_commit(const char* zone, uint32_t old_serial,
	uint32_t new_serial, uint16_t id, uint32_t num_parts,
	uint8_t commit, const char* log_msg,
	nsd_options_t* opt);

/* check if the crc in the nsd.db is the same in memory as on disk.
   returns 1 if different. 0 if the same. returns -1 on error. */
int db_crc_different(namedb_type* db);

/* read the diff file and apply to the database in memory.
   It will attempt to skip bad data.
   If you pass a non-null value log, log comments are alloced in namedb.region
   then, *log must be 0 on start of call (entries are prepended).
   returns 0 on an unrecoverable error. */
int diff_read_file(namedb_type* db, nsd_options_t* opt, struct diff_log** log,
	size_t child_count);

/* check the diff file for garbage at the end (bad type, partial write)
 * and snip it off.
 */
void diff_snip_garbage(namedb_type* db, nsd_options_t* opt);

/*
 * These functions read parts of the diff file.
 */
int diff_read_32(FILE *in, uint32_t* result);
int diff_read_16(FILE *in, uint16_t* result);
int diff_read_8(FILE *in, uint8_t* result);
int diff_read_str(FILE* in, char* buf, size_t len);

#endif /* DIFFFILE_H */
