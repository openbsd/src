/*
 * options.c -- options functions.
 *
 * Copyright (c) 2001-2006, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */
#include "config.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "options.h"
#include "query.h"
#include "tsig.h"
#include "difffile.h"
#include "rrl.h"

#include "configyyrename.h"
#include "configparser.h"
config_parser_state_t* cfg_parser = 0;
extern FILE* c_in, *c_out;
int c_parse(void);
int c_lex(void);
int c_wrap(void);
void c_error(const char *message);
extern char* c_text;

static int
rbtree_strcmp(const void* p1, const void* p2)
{
	if(p1 == NULL && p2 == NULL) return 0;
	if(p1 == NULL) return -1;
	if(p2 == NULL) return 1;
	return strcmp((const char*)p1, (const char*)p2);
}

nsd_options_t*
nsd_options_create(region_type* region)
{
	nsd_options_t* opt;
	opt = (nsd_options_t*)region_alloc(region, sizeof(nsd_options_t));
	opt->region = region;
	opt->zone_options = rbtree_create(region,
		(int (*)(const void *, const void *)) dname_compare);
	opt->configfile = NULL;
	opt->zonestatnames = rbtree_create(opt->region, rbtree_strcmp);
	opt->patterns = rbtree_create(region, rbtree_strcmp);
	opt->keys = rbtree_create(region, rbtree_strcmp);
	opt->ip_addresses = NULL;
	opt->ip_transparent = 0;
	opt->ip_freebind = 0;
	opt->debug_mode = 0;
	opt->verbosity = 0;
	opt->hide_version = 0;
	opt->do_ip4 = 1;
	opt->do_ip6 = 1;
	opt->database = DBFILE;
	opt->identity = 0;
	opt->version = 0;
	opt->nsid = 0;
	opt->logfile = 0;
	opt->log_time_ascii = 1;
	opt->round_robin = 0; /* also packet.h::round_robin */
	opt->server_count = 1;
	opt->tcp_count = 100;
	opt->tcp_query_count = 0;
	opt->tcp_timeout = TCP_TIMEOUT;
	opt->tcp_mss = 0;
	opt->outgoing_tcp_mss = 0;
	opt->ipv4_edns_size = EDNS_MAX_MESSAGE_LEN;
	opt->ipv6_edns_size = EDNS_MAX_MESSAGE_LEN;
	opt->pidfile = PIDFILE;
	opt->port = UDP_PORT;
/* deprecated?	opt->port = TCP_PORT; */
	opt->reuseport = 0;
	opt->statistics = 0;
	opt->chroot = 0;
	opt->username = USER;
	opt->zonesdir = ZONESDIR;
	opt->xfrdfile = XFRDFILE;
	opt->xfrdir = XFRDIR;
	opt->zonelistfile = ZONELISTFILE;
#ifdef RATELIMIT
	opt->rrl_size = RRL_BUCKETS;
	opt->rrl_slip = RRL_SLIP;
	opt->rrl_ipv4_prefix_length = RRL_IPV4_PREFIX_LENGTH;
	opt->rrl_ipv6_prefix_length = RRL_IPV6_PREFIX_LENGTH;
#  ifdef RATELIMIT_DEFAULT_OFF
	opt->rrl_ratelimit = 0;
	opt->rrl_whitelist_ratelimit = 0;
#  else
	opt->rrl_ratelimit = RRL_LIMIT/2;
	opt->rrl_whitelist_ratelimit = RRL_WLIST_LIMIT/2;
#  endif
#endif
	opt->zonefiles_check = 1;
	if(opt->database == NULL || opt->database[0] == 0)
		opt->zonefiles_write = ZONEFILES_WRITE_INTERVAL;
	else	opt->zonefiles_write = 0;
	opt->xfrd_reload_timeout = 1;
	opt->control_enable = 0;
	opt->control_interface = NULL;
	opt->control_port = NSD_CONTROL_PORT;
	opt->server_key_file = CONFIGDIR"/nsd_server.key";
	opt->server_cert_file = CONFIGDIR"/nsd_server.pem";
	opt->control_key_file = CONFIGDIR"/nsd_control.key";
	opt->control_cert_file = CONFIGDIR"/nsd_control.pem";
	return opt;
}

int
nsd_options_insert_zone(nsd_options_t* opt, zone_options_t* zone)
{
	/* create dname for lookup */
	const dname_type* dname = dname_parse(opt->region, zone->name);
	if(!dname)
		return 0;
	zone->node.key = dname;
	if(!rbtree_insert(opt->zone_options, (rbnode_t*)zone))
		return 0;
	return 1;
}

int
nsd_options_insert_pattern(nsd_options_t* opt, pattern_options_t* pat)
{
	if(!pat->pname)
		return 0;
	pat->node.key = pat->pname;
	if(!rbtree_insert(opt->patterns, (rbnode_t*)pat))
		return 0;
	return 1;
}

int
parse_options_file(nsd_options_t* opt, const char* file,
	void (*err)(void*,const char*), void* err_arg)
{
	FILE *in = 0;
	pattern_options_t* pat;
	acl_options_t* acl;

	if(!cfg_parser) {
		cfg_parser = (config_parser_state_t*)region_alloc(
			opt->region, sizeof(config_parser_state_t));
		cfg_parser->chroot = 0;
	}
	cfg_parser->err = err;
	cfg_parser->err_arg = err_arg;
	cfg_parser->filename = (char*)file;
	cfg_parser->line = 1;
	cfg_parser->errors = 0;
	cfg_parser->server_settings_seen = 0;
	cfg_parser->opt = opt;
	cfg_parser->current_pattern = 0;
	cfg_parser->current_zone = 0;
	cfg_parser->current_key = 0;
	cfg_parser->current_ip_address_option = opt->ip_addresses;
	while(cfg_parser->current_ip_address_option && cfg_parser->current_ip_address_option->next)
		cfg_parser->current_ip_address_option = cfg_parser->current_ip_address_option->next;
	cfg_parser->current_allow_notify = 0;
	cfg_parser->current_request_xfr = 0;
	cfg_parser->current_notify = 0;
	cfg_parser->current_provide_xfr = 0;
	
	in = fopen(cfg_parser->filename, "r");
	if(!in) {
		if(err) {
			char m[MAXSYSLOGMSGLEN];
			snprintf(m, sizeof(m), "Could not open %s: %s\n",
				file, strerror(errno));
			err(err_arg, m);
		} else {
			fprintf(stderr, "Could not open %s: %s\n",
				file, strerror(errno));
		}
		return 0;
	}
	c_in = in;
	c_parse();
	fclose(in);

	opt->configfile = region_strdup(opt->region, file);
	if(cfg_parser->current_pattern) {
		if(!cfg_parser->current_pattern->pname)
			c_error("last pattern has no name");
		else {
			if(!nsd_options_insert_pattern(cfg_parser->opt,
				cfg_parser->current_pattern))
				c_error("duplicate pattern");
		}
	}
	if(cfg_parser->current_zone) {
		if(!cfg_parser->current_zone->name)
			c_error("last zone has no name");
		else {
			if(!nsd_options_insert_zone(opt,
				cfg_parser->current_zone))
				c_error("duplicate zone");
		}
		if(!cfg_parser->current_zone->pattern)
			c_error("last zone has no pattern");
	}
	if(cfg_parser->current_key)
	{
		if(!cfg_parser->current_key->name)
			c_error("last key has no name");
		if(!cfg_parser->current_key->algorithm)
			c_error("last key has no algorithm");
		if(!cfg_parser->current_key->secret)
			c_error("last key has no secret blob");
		key_options_insert(opt, cfg_parser->current_key);
	}
	RBTREE_FOR(pat, pattern_options_t*, opt->patterns)
	{
		/* lookup keys for acls */
		for(acl=pat->allow_notify; acl; acl=acl->next)
		{
			if(acl->nokey || acl->blocked)
				continue;
			acl->key_options = key_options_find(opt, acl->key_name);
			if(!acl->key_options)
				c_error_msg("key %s in pattern %s could not be found",
					acl->key_name, pat->pname);
		}
		for(acl=pat->notify; acl; acl=acl->next)
		{
			if(acl->nokey || acl->blocked)
				continue;
			acl->key_options = key_options_find(opt, acl->key_name);
			if(!acl->key_options)
				c_error_msg("key %s in pattern %s could not be found",
					acl->key_name, pat->pname);
		}
		for(acl=pat->request_xfr; acl; acl=acl->next)
		{
			if(acl->nokey || acl->blocked)
				continue;
			acl->key_options = key_options_find(opt, acl->key_name);
			if(!acl->key_options)
				c_error_msg("key %s in pattern %s could not be found",
					acl->key_name, pat->pname);
		}
		for(acl=pat->provide_xfr; acl; acl=acl->next)
		{
			if(acl->nokey || acl->blocked)
				continue;
			acl->key_options = key_options_find(opt, acl->key_name);
			if(!acl->key_options)
				c_error_msg("key %s in pattern %s could not be found",
					acl->key_name, pat->pname);
		}
	}

	if(cfg_parser->errors > 0)
	{
		if(err) {
			char m[MAXSYSLOGMSGLEN];
			snprintf(m, sizeof(m), "read %s failed: %d errors in "
				"configuration file\n", file,
				cfg_parser->errors);
			err(err_arg, m);
		} else {
			fprintf(stderr, "read %s failed: %d errors in "
				"configuration file\n", file,
				cfg_parser->errors);
		}
		return 0;
	}
	return 1;
}

void options_zonestatnames_create(nsd_options_t* opt)
{
	zone_options_t* zopt;
	/* allocate "" as zonestat 0, for zones without a zonestat */
	if(!rbtree_search(opt->zonestatnames, "")) {
		struct zonestatname* n;
		n = (struct zonestatname*)xalloc(sizeof(*n));
		memset(n, 0, sizeof(*n));
		n->node.key = strdup("");
		if(!n->node.key) {
			log_msg(LOG_ERR, "malloc failed: %s", strerror(errno));
			exit(1);
		}
		n->id = (unsigned)(opt->zonestatnames->count);
		rbtree_insert(opt->zonestatnames, (rbnode_t*)n);
	}
	RBTREE_FOR(zopt, zone_options_t*, opt->zone_options) {
		/* insert into tree, so that when read in later id exists */
		(void)getzonestatid(opt, zopt);
	}
}

#define ZONELIST_HEADER "# NSD zone list\n# name pattern\n"
static int
comp_zonebucket(const void* a, const void* b)
{
	/* the line size is much smaller than max-int, and positive,
	 * so the subtraction works */
	return *(const int*)b - *(const int*)a;
}

/* insert free entry into zonelist free buckets */
static void
zone_list_free_insert(nsd_options_t* opt, int linesize, off_t off)
{
	struct zonelist_free* e;
	struct zonelist_bucket* b = (struct zonelist_bucket*)rbtree_search(
		opt->zonefree, &linesize);
	if(!b) {
		b = region_alloc_zero(opt->region, sizeof(*b));
		b->linesize = linesize;
		b->node = *RBTREE_NULL;
		b->node.key = &b->linesize;
		rbtree_insert(opt->zonefree, &b->node);
	}
	e = (struct zonelist_free*)region_alloc_zero(opt->region, sizeof(*e));
	e->next = b->list;
	b->list = e;
	e->off = off;
	opt->zonefree_number++;
}

zone_options_t*
zone_list_zone_insert(nsd_options_t* opt, const char* nm, const char* patnm,
	int linesize, off_t off)
{
	pattern_options_t* pat = pattern_options_find(opt, patnm);
	zone_options_t* zone;
	if(!pat) {
		log_msg(LOG_ERR, "pattern does not exist for zone %s "
			"pattern %s", nm, patnm);
		return NULL;
	}
	zone = zone_options_create(opt->region);
	zone->part_of_config = 0;
	zone->name = region_strdup(opt->region, nm);
	zone->linesize = linesize;
	zone->off = off;
	zone->pattern = pat;
	if(!nsd_options_insert_zone(opt, zone)) {
		log_msg(LOG_ERR, "bad domain name or duplicate zone '%s' "
			"pattern %s", nm, patnm);
		region_recycle(opt->region, (void*)zone->name, strlen(nm)+1);
		region_recycle(opt->region, zone, sizeof(*zone));
		return NULL;
	}
	return zone;
}

int
parse_zone_list_file(nsd_options_t* opt)
{
	/* zonelist looks like this:
	# name pattern
	add example.com master
	del example.net slave
	add foo.bar.nl slave
	add rutabaga.uk config
	*/
	char buf[1024];
	
	/* create empty data structures */
	opt->zonefree = rbtree_create(opt->region, comp_zonebucket);
	opt->zonelist = NULL;
	opt->zonefree_number = 0;
	opt->zonelist_off = 0;

	/* try to open the zonelist file, an empty or nonexist file is OK */
	opt->zonelist = fopen(opt->zonelistfile, "r+");
	if(!opt->zonelist) {
		if(errno == ENOENT)
			return 1; /* file does not exist, it is created later */
		log_msg(LOG_ERR, "could not open zone list %s: %s", opt->zonelistfile,
			strerror(errno));
		return 0;
	}
	/* read header */
	buf[strlen(ZONELIST_HEADER)] = 0;
	if(fread(buf, 1, strlen(ZONELIST_HEADER), opt->zonelist) !=
		strlen(ZONELIST_HEADER) || strncmp(buf, ZONELIST_HEADER,
		strlen(ZONELIST_HEADER)) != 0) {
		log_msg(LOG_ERR, "zone list %s contains bad header\n", opt->zonelistfile);
		fclose(opt->zonelist);
		opt->zonelist = NULL;
		return 0;
	}

	/* read entries in file */
	while(fgets(buf, sizeof(buf), opt->zonelist)) {
		/* skip comments and empty lines */
		if(buf[0] == 0 || buf[0] == '\n' || buf[0] == '#')
			continue;
		if(strncmp(buf, "add ", 4) == 0) {
			int linesize = strlen(buf);
			/* parse the 'add' line */
			/* pick last space on the line, so that the domain
			 * name can have a space in it (but not the pattern)*/
			char* space = strrchr(buf+4, ' ');
			char* nm, *patnm;
			if(!space) {
				/* parse error */
				log_msg(LOG_ERR, "parse error in %s: '%s'",
					opt->zonelistfile, buf);
				continue;
			}
			nm = buf+4;
			*space = 0;
			patnm = space+1;
			if(linesize && buf[linesize-1] == '\n')
				buf[linesize-1] = 0;

			/* store offset and line size for zone entry */
			/* and create zone entry in zonetree */
			(void)zone_list_zone_insert(opt, nm, patnm, linesize,
				ftello(opt->zonelist)-linesize);
		} else if(strncmp(buf, "del ", 4) == 0) {
			/* store offset and line size for deleted entry */
			int linesize = strlen(buf);
			zone_list_free_insert(opt, linesize,
				ftello(opt->zonelist)-linesize);
		} else {
			log_msg(LOG_WARNING, "bad data in %s, '%s'", opt->zonelistfile,
				buf);
		}
	}
	/* store EOF offset */
	opt->zonelist_off = ftello(opt->zonelist);
	return 1;
}

void
zone_options_delete(nsd_options_t* opt, zone_options_t* zone)
{
	rbtree_delete(opt->zone_options, zone->node.key);
	region_recycle(opt->region, (void*)zone->node.key, dname_total_size(
		(dname_type*)zone->node.key));
	region_recycle(opt->region, zone, sizeof(*zone));
}

/* add a new zone to the zonelist */
zone_options_t*
zone_list_add(nsd_options_t* opt, const char* zname, const char* pname)
{
	int r;
	struct zonelist_free* e;
	struct zonelist_bucket* b;
	int linesize = 6 + strlen(zname) + strlen(pname);
	/* create zone entry */
	zone_options_t* zone = zone_list_zone_insert(opt, zname, pname,
		linesize, 0);
	if(!zone)
		return NULL;

	/* use free entry or append to file or create new file */
	if(!opt->zonelist || opt->zonelist_off == 0) {
		/* create new file */
		if(opt->zonelist) fclose(opt->zonelist);
		opt->zonelist = fopen(opt->zonelistfile, "w+");
		if(!opt->zonelist) {
			log_msg(LOG_ERR, "could not create zone list %s: %s",
				opt->zonelistfile, strerror(errno));
			log_msg(LOG_ERR, "zone %s could not be added", zname);
			zone_options_delete(opt, zone);
			return NULL;
		}
		r = fprintf(opt->zonelist, ZONELIST_HEADER);
		if(r != strlen(ZONELIST_HEADER)) {
			if(r == -1)
				log_msg(LOG_ERR, "could not write to %s: %s",
					opt->zonelistfile, strerror(errno));
			else log_msg(LOG_ERR, "partial write to %s: disk full",
				opt->zonelistfile);
			log_msg(LOG_ERR, "zone %s could not be added", zname);
			zone_options_delete(opt, zone);
			return NULL;
		}
		zone->off = ftello(opt->zonelist);
		if(zone->off == -1)
			log_msg(LOG_ERR, "ftello(%s): %s", opt->zonelistfile, strerror(errno));
		r = fprintf(opt->zonelist, "add %s %s\n", zname, pname);
		if(r != zone->linesize) {
			if(r == -1)
				log_msg(LOG_ERR, "could not write to %s: %s",
					opt->zonelistfile, strerror(errno));
			else log_msg(LOG_ERR, "partial write to %s: disk full",
				opt->zonelistfile);
			log_msg(LOG_ERR, "zone %s could not be added", zname);
			zone_options_delete(opt, zone);
			return NULL;
		}
		opt->zonelist_off = ftello(opt->zonelist);
		if(opt->zonelist_off == -1)
			log_msg(LOG_ERR, "ftello(%s): %s", opt->zonelistfile, strerror(errno));
		if(fflush(opt->zonelist) != 0) {
			log_msg(LOG_ERR, "fflush %s: %s", opt->zonelistfile, strerror(errno));
		}
		return zone;
	}
	b = (struct zonelist_bucket*)rbtree_search(opt->zonefree,
		&zone->linesize);
	if(!b || b->list == NULL) {
		/* no empty place, append to file */
		zone->off = opt->zonelist_off;
		if(fseeko(opt->zonelist, zone->off, SEEK_SET) == -1) {
			log_msg(LOG_ERR, "fseeko(%s): %s", opt->zonelistfile, strerror(errno));
			log_msg(LOG_ERR, "zone %s could not be added", zname);
			zone_options_delete(opt, zone);
			return NULL;
		}
		r = fprintf(opt->zonelist, "add %s %s\n", zname, pname);
		if(r != zone->linesize) {
			if(r == -1)
				log_msg(LOG_ERR, "could not write to %s: %s",
					opt->zonelistfile, strerror(errno));
			else log_msg(LOG_ERR, "partial write to %s: disk full",
				opt->zonelistfile);
			log_msg(LOG_ERR, "zone %s could not be added", zname);
			zone_options_delete(opt, zone);
			return NULL;
		}
		opt->zonelist_off += linesize;
		if(fflush(opt->zonelist) != 0) {
			log_msg(LOG_ERR, "fflush %s: %s", opt->zonelistfile, strerror(errno));
		}
		return zone;
	}
	/* reuse empty spot */
	e = b->list;
	zone->off = e->off;
	if(fseeko(opt->zonelist, zone->off, SEEK_SET) == -1) {
		log_msg(LOG_ERR, "fseeko(%s): %s", opt->zonelistfile, strerror(errno));
		log_msg(LOG_ERR, "zone %s could not be added", zname);
		zone_options_delete(opt, zone);
		return NULL;
	}
	r = fprintf(opt->zonelist, "add %s %s\n", zname, pname);
	if(r != zone->linesize) {
		if(r == -1)
			log_msg(LOG_ERR, "could not write to %s: %s",
				opt->zonelistfile, strerror(errno));
		else log_msg(LOG_ERR, "partial write to %s: disk full",
			opt->zonelistfile);
		log_msg(LOG_ERR, "zone %s could not be added", zname);
		zone_options_delete(opt, zone);
		return NULL;
	}
	if(fflush(opt->zonelist) != 0) {
		log_msg(LOG_ERR, "fflush %s: %s", opt->zonelistfile, strerror(errno));
	}

	/* snip off and recycle element */
	b->list = e->next;
	region_recycle(opt->region, e, sizeof(*e));
	if(b->list == NULL) {
		rbtree_delete(opt->zonefree, &b->linesize);
		region_recycle(opt->region, b, sizeof(*b));
	}
	opt->zonefree_number--;
	return zone;
}

/* remove a zone on the zonelist */
void
zone_list_del(nsd_options_t* opt, zone_options_t* zone)
{
	/* put its space onto the free entry */
	if(fseeko(opt->zonelist, zone->off, SEEK_SET) == -1) {
		log_msg(LOG_ERR, "fseeko(%s): %s", opt->zonelistfile, strerror(errno));
		return;
	}
	fprintf(opt->zonelist, "del");
	zone_list_free_insert(opt, zone->linesize, zone->off);

	/* remove zone_options_t */
	zone_options_delete(opt, zone);

	/* see if we need to compact: it is going to halve the zonelist */
	if(opt->zonefree_number > opt->zone_options->count) {
		zone_list_compact(opt);
	} else {
		if(fflush(opt->zonelist) != 0) {
			log_msg(LOG_ERR, "fflush %s: %s", opt->zonelistfile, strerror(errno));
		}
	}
}
/* postorder delete of zonelist free space tree */
static void
delbucket(region_type* region, struct zonelist_bucket* b)
{
	struct zonelist_free* e, *f;
	if(!b || (rbnode_t*)b==RBTREE_NULL)
		return;
	delbucket(region, (struct zonelist_bucket*)b->node.left);
	delbucket(region, (struct zonelist_bucket*)b->node.right);
	e = b->list;
	while(e) {
		f = e->next;
		region_recycle(region, e, sizeof(*e));
		e = f;
	}
	region_recycle(region, b, sizeof(*b));
}

/* compact zonelist file */
void
zone_list_compact(nsd_options_t* opt)
{
	char outname[1024];
	FILE* out;
	zone_options_t* zone;
	off_t off;
	int r;
	snprintf(outname, sizeof(outname), "%s~", opt->zonelistfile);
	/* useful, when : count-of-free > count-of-used */
	/* write zonelist to zonelist~ */
	out = fopen(outname, "w+");
	if(!out) {
		log_msg(LOG_ERR, "could not open %s: %s", outname, strerror(errno));
		return;
	}
	r = fprintf(out, ZONELIST_HEADER);
	if(r == -1) {
		log_msg(LOG_ERR, "write %s failed: %s", outname,
			strerror(errno));
		fclose(out);
		return;
	} else if(r != strlen(ZONELIST_HEADER)) {
		log_msg(LOG_ERR, "write %s was partial: disk full",
			outname);
		fclose(out);
		return;
	}
	off = ftello(out);
	if(off == -1) {
		log_msg(LOG_ERR, "ftello(%s): %s", outname, strerror(errno));
		fclose(out);
		return;
	}
	RBTREE_FOR(zone, zone_options_t*, opt->zone_options) {
		if(zone->part_of_config)
			continue;
		r = fprintf(out, "add %s %s\n", zone->name,
			zone->pattern->pname);
		if(r < 0) {
			log_msg(LOG_ERR, "write %s failed: %s", outname,
				strerror(errno));
			fclose(out);
			return;
		} else if(r != zone->linesize) {
			log_msg(LOG_ERR, "write %s was partial: disk full",
				outname);
			fclose(out);
			return;
		}
	}
	if(fflush(out) != 0) {
		log_msg(LOG_ERR, "fflush %s: %s", outname, strerror(errno));
	}

	/* rename zonelist~ onto zonelist */
	if(rename(outname, opt->zonelistfile) == -1) {
		log_msg(LOG_ERR, "rename(%s to %s) failed: %s",
			outname, opt->zonelistfile, strerror(errno));
		fclose(out);
		return;
	}
	fclose(opt->zonelist);
	/* set offsets */
	RBTREE_FOR(zone, zone_options_t*, opt->zone_options) {
		if(zone->part_of_config)
			continue;
		zone->off = off;
		off += zone->linesize;
	}
	/* empty the free tree */
	delbucket(opt->region, (struct zonelist_bucket*)opt->zonefree->root);
	opt->zonefree->root = RBTREE_NULL;
	opt->zonefree->count = 0;
	opt->zonefree_number = 0;
	/* finish */
	opt->zonelist = out;
	opt->zonelist_off = off;
}

/* close zonelist file */
void
zone_list_close(nsd_options_t* opt)
{
	fclose(opt->zonelist);
	opt->zonelist = NULL;
}

void
c_error_va_list_pos(int showpos, const char* fmt, va_list args)
{
	char* at = NULL;
	cfg_parser->errors++;
	if(showpos && c_text && c_text[0]!=0) {
		at = c_text;
	}
	if(cfg_parser->err) {
		char m[MAXSYSLOGMSGLEN];
		snprintf(m, sizeof(m), "%s:%d: ", cfg_parser->filename,
			cfg_parser->line);
		(*cfg_parser->err)(cfg_parser->err_arg, m);
		if(at) {
			snprintf(m, sizeof(m), "at '%s': ", at);
			(*cfg_parser->err)(cfg_parser->err_arg, m);
		}
		(*cfg_parser->err)(cfg_parser->err_arg, "error: ");
		vsnprintf(m, sizeof(m), fmt, args);
		(*cfg_parser->err)(cfg_parser->err_arg, m);
		(*cfg_parser->err)(cfg_parser->err_arg, "\n");
		return;
	}
        fprintf(stderr, "%s:%d: ", cfg_parser->filename, cfg_parser->line);
	if(at) fprintf(stderr, "at '%s': ", at);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
}

void
c_error_msg_pos(int showpos, const char* fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        c_error_va_list_pos(showpos, fmt, args);
        va_end(args);
}

void
c_error_msg(const char* fmt, ...)
{
        va_list args;
        va_start(args, fmt);
        c_error_va_list_pos(0, fmt, args);
        va_end(args);
}

void
c_error(const char* str)
{
	if((strcmp(str, "syntax error")==0 || strcmp(str, "parse error")==0))
		c_error_msg_pos(1, "%s", str);
	else	c_error_msg("%s", str);
}

int
c_wrap()
{
        return 1;
}

zone_options_t*
zone_options_create(region_type* region)
{
	zone_options_t* zone;
	zone = (zone_options_t*)region_alloc(region, sizeof(zone_options_t));
	zone->node = *RBTREE_NULL;
	zone->name = 0;
	zone->pattern = 0;
	zone->part_of_config = 0;
	return zone;
}

/* true is booleans are the same truth value */
#define booleq(x,y) ( ((x) && (y)) || (!(x) && !(y)) )

int
acl_equal(acl_options_t* p, acl_options_t* q)
{
	if(!booleq(p->use_axfr_only, q->use_axfr_only)) return 0;
	if(!booleq(p->allow_udp, q->allow_udp)) return 0;
	if(strcmp(p->ip_address_spec, q->ip_address_spec)!=0) return 0;
	/* the ip6, port, addr, mask, type: are derived from the ip_address_spec */
	if(!booleq(p->nokey, q->nokey)) return 0;
	if(!booleq(p->blocked, q->blocked)) return 0;
	if(p->key_name && q->key_name) {
		if(strcmp(p->key_name, q->key_name)!=0) return 0;
	} else if(p->key_name && !q->key_name) return 0;
	else if(!p->key_name && q->key_name) return 0;
	/* key_options is derived from key_name */
	return 1;
}

int
acl_list_equal(acl_options_t* p, acl_options_t* q)
{
	/* must be same and in same order */
	while(p && q) {
		if(!acl_equal(p, q))
			return 0;
		p = p->next;
		q = q->next;
	}
	if(!p && !q) return 1;
	/* different lengths */
	return 0;
}

pattern_options_t*
pattern_options_create(region_type* region)
{
	pattern_options_t* p;
	p = (pattern_options_t*)region_alloc(region, sizeof(pattern_options_t));
	p->node = *RBTREE_NULL;
	p->pname = 0;
	p->zonefile = 0;
	p->zonestats = 0;
	p->allow_notify = 0;
	p->request_xfr = 0;
	p->size_limit_xfr = 0;
	p->notify = 0;
	p->provide_xfr = 0;
	p->outgoing_interface = 0;
	p->notify_retry = 5;
	p->notify_retry_is_default = 1;
	p->allow_axfr_fallback = 1;
	p->allow_axfr_fallback_is_default = 1;
	p->implicit = 0;
	p->xfrd_flags = 0;
	p->max_refresh_time = 2419200;	/* 4 weeks */
	p->max_refresh_time_is_default = 1;
	p->min_refresh_time = 0;
	p->min_refresh_time_is_default = 1;
	p->max_retry_time = 1209600;	/* 2 weeks */
	p->max_retry_time_is_default = 1;
	p->min_retry_time = 0;
	p->min_retry_time_is_default = 1;
#ifdef RATELIMIT
	p->rrl_whitelist = 0;
#endif
	p->multi_master_check = 0;
	return p;
}

static void
acl_delete(region_type* region, acl_options_t* acl)
{
	if(acl->ip_address_spec)
		region_recycle(region, (void*)acl->ip_address_spec,
			strlen(acl->ip_address_spec)+1);
	if(acl->key_name)
		region_recycle(region, (void*)acl->key_name,
			strlen(acl->key_name)+1);
	/* key_options is a convenience pointer, not owned by the acl */
	region_recycle(region, acl, sizeof(*acl));
}

static void
acl_list_delete(region_type* region, acl_options_t* list)
{
	acl_options_t* n;
	while(list) {
		n = list->next;
		acl_delete(region, list);
		list = n;
	}
}

void
pattern_options_remove(nsd_options_t* opt, const char* name)
{
	pattern_options_t* p = (pattern_options_t*)rbtree_delete(
		opt->patterns, name);
	/* delete p and its contents */
	if (!p)
		return;
	if(p->pname)
		region_recycle(opt->region, (void*)p->pname,
			strlen(p->pname)+1);
	if(p->zonefile)
		region_recycle(opt->region, (void*)p->zonefile,
			strlen(p->zonefile)+1);
	if(p->zonestats)
		region_recycle(opt->region, (void*)p->zonestats,
			strlen(p->zonestats)+1);
	acl_list_delete(opt->region, p->allow_notify);
	acl_list_delete(opt->region, p->request_xfr);
	acl_list_delete(opt->region, p->notify);
	acl_list_delete(opt->region, p->provide_xfr);
	acl_list_delete(opt->region, p->outgoing_interface);

	region_recycle(opt->region, p, sizeof(pattern_options_t));
}

static acl_options_t*
copy_acl(region_type* region, acl_options_t* a)
{
	acl_options_t* b;
	if(!a) return NULL;
	b = (acl_options_t*)region_alloc(region, sizeof(*b));
	/* copy the whole lot */
	*b = *a;
	/* fix the pointers */
	if(a->ip_address_spec)
		b->ip_address_spec = region_strdup(region, a->ip_address_spec);
	if(a->key_name)
		b->key_name = region_strdup(region, a->key_name);
	b->next = NULL;
	b->key_options = NULL;
	return b;
}

static acl_options_t*
copy_acl_list(nsd_options_t* opt, acl_options_t* a)
{
	acl_options_t* b, *blast = NULL, *blist = NULL;
	while(a) {
		b = copy_acl(opt->region, a);
		/* fixup key_options */
		if(b->key_name)
			b->key_options = key_options_find(opt, b->key_name);
		else	b->key_options = NULL;

		/* link as last into list */
		b->next = NULL;
		if(!blist) blist = b;
		else blast->next = b;
		blast = b;
		
		a = a->next;
	}
	return blist;
}

static void
copy_changed_acl(nsd_options_t* opt, acl_options_t** orig,
	acl_options_t* anew)
{
	if(!acl_list_equal(*orig, anew)) {
		acl_list_delete(opt->region, *orig);
		*orig = copy_acl_list(opt, anew);
	}
}

static void
copy_pat_fixed(region_type* region, pattern_options_t* orig,
	pattern_options_t* p)
{
	orig->allow_axfr_fallback = p->allow_axfr_fallback;
	orig->allow_axfr_fallback_is_default =
		p->allow_axfr_fallback_is_default;
	orig->notify_retry = p->notify_retry;
	orig->notify_retry_is_default = p->notify_retry_is_default;
	orig->implicit = p->implicit;
	if(p->zonefile)
		orig->zonefile = region_strdup(region, p->zonefile);
	else orig->zonefile = NULL;
	if(p->zonestats)
		orig->zonestats = region_strdup(region, p->zonestats);
	else orig->zonestats = NULL;
	orig->max_refresh_time = p->max_refresh_time;
	orig->max_refresh_time_is_default = p->max_refresh_time_is_default;
	orig->min_refresh_time = p->min_refresh_time;
	orig->min_refresh_time_is_default = p->min_refresh_time_is_default;
	orig->max_retry_time = p->max_retry_time;
	orig->max_retry_time_is_default = p->max_retry_time_is_default;
	orig->min_retry_time = p->min_retry_time;
	orig->min_retry_time_is_default = p->min_retry_time_is_default;
#ifdef RATELIMIT
	orig->rrl_whitelist = p->rrl_whitelist;
#endif
	orig->multi_master_check = p->multi_master_check;
}

void
pattern_options_add_modify(nsd_options_t* opt, pattern_options_t* p)
{
	pattern_options_t* orig = pattern_options_find(opt, p->pname);
	if(!orig) {
		/* needs to be copied to opt region */
		orig = pattern_options_create(opt->region);
		orig->pname = region_strdup(opt->region, p->pname);
		copy_pat_fixed(opt->region, orig, p);
		orig->allow_notify = copy_acl_list(opt, p->allow_notify);
		orig->request_xfr = copy_acl_list(opt, p->request_xfr);
		orig->notify = copy_acl_list(opt, p->notify);
		orig->provide_xfr = copy_acl_list(opt, p->provide_xfr);
		orig->outgoing_interface = copy_acl_list(opt,
			p->outgoing_interface);
		nsd_options_insert_pattern(opt, orig);
	} else {
		/* modify in place so pointers stay valid (and copy
		   into region). Do not touch unchanged acls. */
		if(orig->zonefile)
			region_recycle(opt->region, (char*)orig->zonefile,
				strlen(orig->zonefile)+1);
		if(orig->zonestats)
			region_recycle(opt->region, (char*)orig->zonestats,
				strlen(orig->zonestats)+1);
		copy_pat_fixed(opt->region, orig, p);
		copy_changed_acl(opt, &orig->allow_notify, p->allow_notify);
		copy_changed_acl(opt, &orig->request_xfr, p->request_xfr);
		copy_changed_acl(opt, &orig->notify, p->notify);
		copy_changed_acl(opt, &orig->provide_xfr, p->provide_xfr);
		copy_changed_acl(opt, &orig->outgoing_interface,
			p->outgoing_interface);
	}
}

pattern_options_t*
pattern_options_find(nsd_options_t* opt, const char* name)
{
	return (pattern_options_t*)rbtree_search(opt->patterns, name);
}

int
pattern_options_equal(pattern_options_t* p, pattern_options_t* q)
{
	if(strcmp(p->pname, q->pname) != 0) return 0;
	if(!p->zonefile && q->zonefile) return 0;
	else if(p->zonefile && !q->zonefile) return 0;
	else if(p->zonefile && q->zonefile) {
		if(strcmp(p->zonefile, q->zonefile) != 0) return 0;
	}
	if(!p->zonestats && q->zonestats) return 0;
	else if(p->zonestats && !q->zonestats) return 0;
	else if(p->zonestats && q->zonestats) {
		if(strcmp(p->zonestats, q->zonestats) != 0) return 0;
	}
	if(!booleq(p->allow_axfr_fallback, q->allow_axfr_fallback)) return 0;
	if(!booleq(p->allow_axfr_fallback_is_default,
		q->allow_axfr_fallback_is_default)) return 0;
	if(p->notify_retry != q->notify_retry) return 0;
	if(!booleq(p->notify_retry_is_default,
		q->notify_retry_is_default)) return 0;
	if(!booleq(p->implicit, q->implicit)) return 0;
	if(!acl_list_equal(p->allow_notify, q->allow_notify)) return 0;
	if(!acl_list_equal(p->request_xfr, q->request_xfr)) return 0;
	if(!acl_list_equal(p->notify, q->notify)) return 0;
	if(!acl_list_equal(p->provide_xfr, q->provide_xfr)) return 0;
	if(!acl_list_equal(p->outgoing_interface, q->outgoing_interface))
		return 0;
	if(p->max_refresh_time != q->max_refresh_time) return 0;
	if(!booleq(p->max_refresh_time_is_default,
		q->max_refresh_time_is_default)) return 0;
	if(p->min_refresh_time != q->min_refresh_time) return 0;
	if(!booleq(p->min_refresh_time_is_default,
		q->min_refresh_time_is_default)) return 0;
	if(p->max_retry_time != q->max_retry_time) return 0;
	if(!booleq(p->max_retry_time_is_default,
		q->max_retry_time_is_default)) return 0;
	if(p->min_retry_time != q->min_retry_time) return 0;
	if(!booleq(p->min_retry_time_is_default,
		q->min_retry_time_is_default)) return 0;
#ifdef RATELIMIT
	if(p->rrl_whitelist != q->rrl_whitelist) return 0;
#endif
	if(!booleq(p->multi_master_check,q->multi_master_check)) return 0;
	if(p->size_limit_xfr != q->size_limit_xfr) return 0;
	return 1;
}

static void
marshal_u8(struct buffer* b, uint8_t v)
{
	buffer_reserve(b, 1);
	buffer_write_u8(b, v);
}

static uint8_t
unmarshal_u8(struct buffer* b)
{
	return buffer_read_u8(b);
}

static void
marshal_u64(struct buffer* b, uint64_t v)
{
	buffer_reserve(b, 8);
	buffer_write_u64(b, v);
}

static uint64_t
unmarshal_u64(struct buffer* b)
{
	return buffer_read_u64(b);
}

#ifdef RATELIMIT
static void
marshal_u16(struct buffer* b, uint16_t v)
{
	buffer_reserve(b, 2);
	buffer_write_u16(b, v);
}
#endif

#ifdef RATELIMIT
static uint16_t
unmarshal_u16(struct buffer* b)
{
	return buffer_read_u16(b);
}
#endif

static void
marshal_u32(struct buffer* b, uint32_t v)
{
	buffer_reserve(b, 4);
	buffer_write_u32(b, v);
}

static uint32_t
unmarshal_u32(struct buffer* b)
{
	return buffer_read_u32(b);
}

static void
marshal_str(struct buffer* b, const char* s)
{
	if(!s) marshal_u8(b, 0);
	else {
		size_t len = strlen(s);
		marshal_u8(b, 1);
		buffer_reserve(b, len+1);
		buffer_write(b, s, len+1);
	}
}

static char*
unmarshal_str(region_type* r, struct buffer* b)
{
	uint8_t nonnull = unmarshal_u8(b);
	if(nonnull) {
		char* result = region_strdup(r, (char*)buffer_current(b));
		size_t len = strlen((char*)buffer_current(b));
		buffer_skip(b, len+1);
		return result;
	} else return NULL;
}

static void
marshal_acl(struct buffer* b, acl_options_t* acl)
{
	buffer_reserve(b, sizeof(*acl));
	buffer_write(b, acl, sizeof(*acl));
	marshal_str(b, acl->ip_address_spec);
	marshal_str(b, acl->key_name);
}

static acl_options_t*
unmarshal_acl(region_type* r, struct buffer* b)
{
	acl_options_t* acl = (acl_options_t*)region_alloc(r, sizeof(*acl));
	buffer_read(b, acl, sizeof(*acl));
	acl->next = NULL;
	acl->key_options = NULL;
	acl->ip_address_spec = unmarshal_str(r, b);
	acl->key_name = unmarshal_str(r, b);
	return acl;
}

static void
marshal_acl_list(struct buffer* b, acl_options_t* list)
{
	while(list) {
		marshal_u8(b, 1); /* is there a next one marker */
		marshal_acl(b, list);
		list = list->next;
	}
	marshal_u8(b, 0); /* end of list marker */
}

static acl_options_t*
unmarshal_acl_list(region_type* r, struct buffer* b)
{
	acl_options_t* a, *last=NULL, *list=NULL;
	while(unmarshal_u8(b)) {
		a = unmarshal_acl(r, b);
		/* link in */
		a->next = NULL;
		if(!list) list = a;
		else last->next = a;
		last = a;
	}
	return list;
}

void
pattern_options_marshal(struct buffer* b, pattern_options_t* p)
{
	marshal_str(b, p->pname);
	marshal_str(b, p->zonefile);
	marshal_str(b, p->zonestats);
#ifdef RATELIMIT
	marshal_u16(b, p->rrl_whitelist);
#endif
	marshal_u8(b, p->allow_axfr_fallback);
	marshal_u8(b, p->allow_axfr_fallback_is_default);
	marshal_u8(b, p->notify_retry);
	marshal_u8(b, p->notify_retry_is_default);
	marshal_u8(b, p->implicit);
	marshal_u64(b, p->size_limit_xfr);
	marshal_acl_list(b, p->allow_notify);
	marshal_acl_list(b, p->request_xfr);
	marshal_acl_list(b, p->notify);
	marshal_acl_list(b, p->provide_xfr);
	marshal_acl_list(b, p->outgoing_interface);
	marshal_u32(b, p->max_refresh_time);
	marshal_u8(b, p->max_refresh_time_is_default);
	marshal_u32(b, p->min_refresh_time);
	marshal_u8(b, p->min_refresh_time_is_default);
	marshal_u32(b, p->max_retry_time);
	marshal_u8(b, p->max_retry_time_is_default);
	marshal_u32(b, p->min_retry_time);
	marshal_u8(b, p->min_retry_time_is_default);
	marshal_u8(b, p->multi_master_check);
}

pattern_options_t*
pattern_options_unmarshal(region_type* r, struct buffer* b)
{
	pattern_options_t* p = pattern_options_create(r);
	p->pname = unmarshal_str(r, b);
	p->zonefile = unmarshal_str(r, b);
	p->zonestats = unmarshal_str(r, b);
#ifdef RATELIMIT
	p->rrl_whitelist = unmarshal_u16(b);
#endif
	p->allow_axfr_fallback = unmarshal_u8(b);
	p->allow_axfr_fallback_is_default = unmarshal_u8(b);
	p->notify_retry = unmarshal_u8(b);
	p->notify_retry_is_default = unmarshal_u8(b);
	p->implicit = unmarshal_u8(b);
	p->size_limit_xfr = unmarshal_u64(b);
	p->allow_notify = unmarshal_acl_list(r, b);
	p->request_xfr = unmarshal_acl_list(r, b);
	p->notify = unmarshal_acl_list(r, b);
	p->provide_xfr = unmarshal_acl_list(r, b);
	p->outgoing_interface = unmarshal_acl_list(r, b);
	p->max_refresh_time = unmarshal_u32(b);
	p->max_refresh_time_is_default = unmarshal_u8(b);
	p->min_refresh_time = unmarshal_u32(b);
	p->min_refresh_time_is_default = unmarshal_u8(b);
	p->max_retry_time = unmarshal_u32(b);
	p->max_retry_time_is_default = unmarshal_u8(b);
	p->min_retry_time = unmarshal_u32(b);
	p->min_retry_time_is_default = unmarshal_u8(b);
	p->multi_master_check = unmarshal_u8(b);
	return p;
}

key_options_t*
key_options_create(region_type* region)
{
	key_options_t* key;
	key = (key_options_t*)region_alloc_zero(region, sizeof(key_options_t));
	return key;
}

void
key_options_insert(nsd_options_t* opt, key_options_t* key)
{
	if(!key->name) return;
	key->node.key = key->name;
	(void)rbtree_insert(opt->keys, &key->node);
}

key_options_t*
key_options_find(nsd_options_t* opt, const char* name)
{
	return (key_options_t*)rbtree_search(opt->keys, name);
}

/** remove tsig_key contents */
void
key_options_desetup(region_type* region, key_options_t* key)
{
	/* keep tsig_key pointer so that existing references keep valid */
	if(!key->tsig_key)
		return;
	/* name stays the same */
	if(key->tsig_key->data) {
		/* wipe secret! */
		memset(key->tsig_key->data, 0xdd, key->tsig_key->size);
		region_recycle(region, key->tsig_key->data,
			key->tsig_key->size);
		key->tsig_key->data = NULL;
		key->tsig_key->size = 0;
	}
}

/** add tsig_key contents */
void
key_options_setup(region_type* region, key_options_t* key)
{
	uint8_t data[16384]; /* 16KB */
	int size;
	if(!key->tsig_key) {
		/* create it */
		key->tsig_key = (tsig_key_type *) region_alloc(region,
			sizeof(tsig_key_type));
		/* create name */
		key->tsig_key->name = dname_parse(region, key->name);
		if(!key->tsig_key->name) {
			log_msg(LOG_ERR, "Failed to parse tsig key name %s",
				key->name);
			/* key and base64 were checked during syntax parse */
			exit(1);
		}
		key->tsig_key->size = 0;
		key->tsig_key->data = NULL;
	}
	size = __b64_pton(key->secret, data, sizeof(data));
	if(size == -1) {
		log_msg(LOG_ERR, "Failed to parse tsig key data %s",
			key->name);
		/* key and base64 were checked during syntax parse */
		exit(1);
	}
	key->tsig_key->size = size;
	key->tsig_key->data = (uint8_t *)region_alloc_init(region, data, size);
}

void
key_options_remove(nsd_options_t* opt, const char* name)
{
	key_options_t* k = key_options_find(opt, name);
	if(!k) return;
	(void)rbtree_delete(opt->keys, name);
	if(k->name)
		region_recycle(opt->region, k->name, strlen(k->name)+1);
	if(k->algorithm)
		region_recycle(opt->region, k->algorithm, strlen(k->algorithm)+1);
	if(k->secret) {
		memset(k->secret, 0xdd, strlen(k->secret)); /* wipe secret! */
		region_recycle(opt->region, k->secret, strlen(k->secret)+1);
	}
	if(k->tsig_key) {
		tsig_del_key(k->tsig_key);
		if(k->tsig_key->name)
			region_recycle(opt->region, (void*)k->tsig_key->name,
				dname_total_size(k->tsig_key->name));
		key_options_desetup(opt->region, k);
		region_recycle(opt->region, k->tsig_key, sizeof(tsig_key_type));
	}
	region_recycle(opt->region, k, sizeof(key_options_t));
}

int
key_options_equal(key_options_t* p, key_options_t* q)
{
	return strcmp(p->name, q->name)==0 && strcmp(p->algorithm,
		q->algorithm)==0 && strcmp(p->secret, q->secret)==0;
}

void
key_options_add_modify(nsd_options_t* opt, key_options_t* key)
{
	key_options_t* orig = key_options_find(opt, key->name);
	if(!orig) {
		/* needs to be copied to opt region */
		orig = key_options_create(opt->region);
		orig->name = region_strdup(opt->region, key->name);
		orig->algorithm = region_strdup(opt->region, key->algorithm);
		orig->secret = region_strdup(opt->region, key->secret);
		key_options_setup(opt->region, orig);
		tsig_add_key(orig->tsig_key);
		key_options_insert(opt, orig);
	} else {
		/* modify entries in existing key, and copy to opt region */
		key_options_desetup(opt->region, orig);
		region_recycle(opt->region, orig->algorithm,
			strlen(orig->algorithm)+1);
		orig->algorithm = region_strdup(opt->region, key->algorithm);
		region_recycle(opt->region, orig->secret,
			strlen(orig->secret)+1);
		orig->secret = region_strdup(opt->region, key->secret);
		key_options_setup(opt->region, orig);
	}
}

int
acl_check_incoming(acl_options_t* acl, struct query* q,
	acl_options_t** reason)
{
	/* check each acl element.
	   if 1 blocked element matches - return -1.
	   if any element matches - return number.
	   else return -1. */
	int found_match = -1;
	int number = 0;
	acl_options_t* match = 0;

	if(reason)
		*reason = NULL;

	while(acl)
	{
		DEBUG(DEBUG_XFRD,2, (LOG_INFO, "testing acl %s %s",
			acl->ip_address_spec, acl->nokey?"NOKEY":
			(acl->blocked?"BLOCKED":acl->key_name)));
		if(acl_addr_matches(acl, q) && acl_key_matches(acl, q)) {
			if(!match)
			{
				match = acl; /* remember first match */
				found_match=number;
			}
			if(acl->blocked) {
				if(reason)
					*reason = acl;
				return -1;
			}
		}
		number++;
		acl = acl->next;
	}

	if(reason)
		*reason = match;
	return found_match;
}

#ifdef INET6
int
acl_addr_matches_ipv6host(acl_options_t* acl, struct sockaddr_storage* addr_storage, unsigned int port)
{
	struct sockaddr_in6* addr = (struct sockaddr_in6*)addr_storage;
	if(acl->port != 0 && acl->port != port)
		return 0;
	switch(acl->rangetype) {
	case acl_range_mask:
	case acl_range_subnet:
		if(!acl_addr_match_mask((uint32_t*)&acl->addr.addr6, (uint32_t*)&addr->sin6_addr,
			(uint32_t*)&acl->range_mask.addr6, sizeof(struct in6_addr)))
			return 0;
		break;
	case acl_range_minmax:
		if(!acl_addr_match_range((uint32_t*)&acl->addr.addr6, (uint32_t*)&addr->sin6_addr,
			(uint32_t*)&acl->range_mask.addr6, sizeof(struct in6_addr)))
			return 0;
		break;
	case acl_range_single:
	default:
		if(memcmp(&addr->sin6_addr, &acl->addr.addr6,
			sizeof(struct in6_addr)) != 0)
			return 0;
		break;
	}
	return 1;
}
#endif

int
acl_addr_matches_ipv4host(acl_options_t* acl, struct sockaddr_in* addr, unsigned int port)
{
	if(acl->port != 0 && acl->port != port)
		return 0;
	switch(acl->rangetype) {
	case acl_range_mask:
	case acl_range_subnet:
		if(!acl_addr_match_mask((uint32_t*)&acl->addr.addr, (uint32_t*)&addr->sin_addr,
			(uint32_t*)&acl->range_mask.addr, sizeof(struct in_addr)))
			return 0;
		break;
	case acl_range_minmax:
		if(!acl_addr_match_range((uint32_t*)&acl->addr.addr, (uint32_t*)&addr->sin_addr,
			(uint32_t*)&acl->range_mask.addr, sizeof(struct in_addr)))
			return 0;
		break;
	case acl_range_single:
	default:
		if(memcmp(&addr->sin_addr, &acl->addr.addr,
			sizeof(struct in_addr)) != 0)
			return 0;
		break;
	}
	return 1;
}

int
acl_addr_matches_host(acl_options_t* acl, acl_options_t* host)
{
	if(acl->is_ipv6)
	{
#ifdef INET6
		struct sockaddr_storage* addr = (struct sockaddr_storage*)&host->addr;
		if(!host->is_ipv6) return 0;
		return acl_addr_matches_ipv6host(acl, addr, host->port);
#else
		return 0; /* no inet6, no match */
#endif
	}
	else
	{
		struct sockaddr_in* addr = (struct sockaddr_in*)&host->addr;
		if(host->is_ipv6) return 0;
		return acl_addr_matches_ipv4host(acl, addr, host->port);
	}
	/* ENOTREACH */
	return 0;
}

int
acl_addr_matches(acl_options_t* acl, struct query* q)
{
	if(acl->is_ipv6)
	{
#ifdef INET6
		struct sockaddr_storage* addr = (struct sockaddr_storage*)&q->addr;
		if(addr->ss_family != AF_INET6)
			return 0;
		return acl_addr_matches_ipv6host(acl, addr, ntohs(((struct sockaddr_in6*)addr)->sin6_port));
#else
		return 0; /* no inet6, no match */
#endif
	}
	else
	{
		struct sockaddr_in* addr = (struct sockaddr_in*)&q->addr;
		if(addr->sin_family != AF_INET)
			return 0;
		return acl_addr_matches_ipv4host(acl, addr, ntohs(addr->sin_port));
	}
	/* ENOTREACH */
	return 0;
}

int
acl_addr_match_mask(uint32_t* a, uint32_t* b, uint32_t* mask, size_t sz)
{
	size_t i;
#ifndef NDEBUG
	assert(sz % 4 == 0);
#endif
	sz /= 4;
	for(i=0; i<sz; ++i)
	{
		if(((*a++)&*mask) != ((*b++)&*mask))
			return 0;
		++mask;
	}
	return 1;
}

int
acl_addr_match_range(uint32_t* minval, uint32_t* x, uint32_t* maxval, size_t sz)
{
	size_t i;
	uint8_t checkmin = 1, checkmax = 1;
#ifndef NDEBUG
	assert(sz % 4 == 0);
#endif
	/* check treats x as one huge number */
	sz /= 4;
	for(i=0; i<sz; ++i)
	{
		/* if outside bounds, we are done */
		if(checkmin)
			if(minval[i] > x[i])
				return 0;
		if(checkmax)
			if(maxval[i] < x[i])
				return 0;
		/* if x is equal to a bound, that bound needs further checks */
		if(checkmin && minval[i]!=x[i])
			checkmin = 0;
		if(checkmax && maxval[i]!=x[i])
			checkmax = 0;
		if(!checkmin && !checkmax)
			return 1; /* will always match */
	}
	return 1;
}

int
acl_key_matches(acl_options_t* acl, struct query* q)
{
	if(acl->blocked)
		return 1;
	if(acl->nokey) {
		if(q->tsig.status == TSIG_NOT_PRESENT)
			return 1;
		return 0;
	}
	/* check name of tsig key */
	if(q->tsig.status != TSIG_OK) {
		DEBUG(DEBUG_XFRD,2, (LOG_INFO, "keymatch fail query has no TSIG"));
		return 0; /* query has no TSIG */
	}
	if(q->tsig.error_code != TSIG_ERROR_NOERROR) {
		DEBUG(DEBUG_XFRD,2, (LOG_INFO, "keymatch fail, tsig has error"));
		return 0; /* some tsig error */
	}
	if(!acl->key_options->tsig_key) {
		DEBUG(DEBUG_XFRD,2, (LOG_INFO, "keymatch fail no config"));
		return 0; /* key not properly configured */
	}
	if(dname_compare(q->tsig.key_name,
		acl->key_options->tsig_key->name) != 0) {
		DEBUG(DEBUG_XFRD,2, (LOG_INFO, "keymatch fail wrong key name"));
		return 0; /* wrong key name */
	}
	if(tsig_strlowercmp(q->tsig.algorithm->short_name,
		acl->key_options->algorithm) != 0 && (
		strncmp("hmac-", q->tsig.algorithm->short_name, 5) != 0 ||
		tsig_strlowercmp(q->tsig.algorithm->short_name+5,
		acl->key_options->algorithm) != 0) ) {
		DEBUG(DEBUG_XFRD,2, (LOG_ERR, "query tsig wrong algorithm"));
		return 0; /* no such algo */
	}
	return 1;
}

int
acl_same_host(acl_options_t* a, acl_options_t* b)
{
	if(a->is_ipv6 && !b->is_ipv6)
		return 0;
	if(!a->is_ipv6 && b->is_ipv6)
		return 0;
	if(a->port != b->port)
		return 0;
	if(a->rangetype != b->rangetype)
		return 0;
	if(!a->is_ipv6) {
		if(memcmp(&a->addr.addr, &b->addr.addr,
		   sizeof(struct in_addr)) != 0)
			return 0;
		if(a->rangetype != acl_range_single &&
		   memcmp(&a->range_mask.addr, &b->range_mask.addr,
		   sizeof(struct in_addr)) != 0)
			return 0;
	} else {
#ifdef INET6
		if(memcmp(&a->addr.addr6, &b->addr.addr6,
		   sizeof(struct in6_addr)) != 0)
			return 0;
		if(a->rangetype != acl_range_single &&
		   memcmp(&a->range_mask.addr6, &b->range_mask.addr6,
		   sizeof(struct in6_addr)) != 0)
			return 0;
#else
		return 0;
#endif
	}
	return 1;
}

#if defined(HAVE_SSL)
void
key_options_tsig_add(nsd_options_t* opt)
{
	key_options_t* optkey;
	RBTREE_FOR(optkey, key_options_t*, opt->keys) {
		key_options_setup(opt->region, optkey);
		tsig_add_key(optkey->tsig_key);
	}
}
#endif

int
zone_is_slave(zone_options_t* opt)
{
	return opt && opt->pattern && opt->pattern->request_xfr != 0;
}

/* get a character in string (or replacement char if not long enough) */
static const char*
get_char(const char* str, size_t i)
{
	static char res[2];
	if(i >= strlen(str))
		return ".";
	res[0] = str[i];
	res[1] = 0;
	return res;
}
/* get end label of the zone name (or .) */
static const char*
get_end_label(zone_options_t* zone, int i)
{
	const dname_type* d = (const dname_type*)zone->node.key;
	if(i >= d->label_count) {
		return ".";
	}
	return wirelabel2str(dname_label(d, i));
}
/* replace occurrences of one with two */
void
replace_str(char* str, size_t len, const char* one, const char* two)
{
	char* pos;
	char* at = str;
	while( (pos=strstr(at, one)) ) {
		if(strlen(str)+strlen(two)-strlen(one) >= len)
			return; /* no more space to replace */
		/* stuff before pos is fine */
		/* move the stuff after pos to make space for two, add
		 * one to length of remainder to also copy the 0 byte end */
		memmove(pos+strlen(two), pos+strlen(one),
			strlen(pos+strlen(one))+1);
		/* copy in two */
		memmove(pos, two, strlen(two));
		/* at is end of the newly inserted two (avoids recursion if
		 * two contains one) */
		at = pos+strlen(two);
	}
}

const char*
config_cook_string(zone_options_t* zone, const char* input)
{
	static char f[1024];
	/* if not a template, return as-is */
	if(!strchr(input, '%')) {
		return input;
	}
	strlcpy(f, input, sizeof(f));
	if(strstr(f, "%1"))
		replace_str(f, sizeof(f), "%1", get_char(zone->name, 0));
	if(strstr(f, "%2"))
		replace_str(f, sizeof(f), "%2", get_char(zone->name, 1));
	if(strstr(f, "%3"))
		replace_str(f, sizeof(f), "%3", get_char(zone->name, 2));
	if(strstr(f, "%z"))
		replace_str(f, sizeof(f), "%z", get_end_label(zone, 1));
	if(strstr(f, "%y"))
		replace_str(f, sizeof(f), "%y", get_end_label(zone, 2));
	if(strstr(f, "%x"))
		replace_str(f, sizeof(f), "%x", get_end_label(zone, 3));
	if(strstr(f, "%s"))
		replace_str(f, sizeof(f), "%s", zone->name);
	return f;
}

const char*
config_make_zonefile(zone_options_t* zone, struct nsd* nsd)
{
	static char f[1024];
	/* if not a template, return as-is */
	if(!strchr(zone->pattern->zonefile, '%')) {
		if (nsd->chrootdir && nsd->chrootdir[0] && 
			zone->pattern->zonefile &&
			zone->pattern->zonefile[0] == '/' &&
			strncmp(zone->pattern->zonefile, nsd->chrootdir,
			strlen(nsd->chrootdir)) == 0)
			/* -1 because chrootdir ends in trailing slash */
			return zone->pattern->zonefile + strlen(nsd->chrootdir) - 1;
		return zone->pattern->zonefile;
	}
	strlcpy(f, zone->pattern->zonefile, sizeof(f));
	if(strstr(f, "%1"))
		replace_str(f, sizeof(f), "%1", get_char(zone->name, 0));
	if(strstr(f, "%2"))
		replace_str(f, sizeof(f), "%2", get_char(zone->name, 1));
	if(strstr(f, "%3"))
		replace_str(f, sizeof(f), "%3", get_char(zone->name, 2));
	if(strstr(f, "%z"))
		replace_str(f, sizeof(f), "%z", get_end_label(zone, 1));
	if(strstr(f, "%y"))
		replace_str(f, sizeof(f), "%y", get_end_label(zone, 2));
	if(strstr(f, "%x"))
		replace_str(f, sizeof(f), "%x", get_end_label(zone, 3));
	if(strstr(f, "%s"))
		replace_str(f, sizeof(f), "%s", zone->name);
	if (nsd->chrootdir && nsd->chrootdir[0] && f[0] == '/' &&
		strncmp(f, nsd->chrootdir, strlen(nsd->chrootdir)) == 0)
		/* -1 because chrootdir ends in trailing slash */
		return f + strlen(nsd->chrootdir) - 1;
	return f;
}

zone_options_t*
zone_options_find(nsd_options_t* opt, const struct dname* apex)
{
	return (zone_options_t*) rbtree_search(opt->zone_options, apex);
}

acl_options_t*
acl_find_num(acl_options_t* acl, int num)
{
	int count = num;
	if(num < 0)
		return 0;
	while(acl && count > 0) {
		acl = acl->next;
		count--;
	}
	if(count == 0)
		return acl;
	return 0;
}

/* true if ipv6 address, false if ipv4 */
int
parse_acl_is_ipv6(const char* p)
{
	/* see if addr is ipv6 or ipv4 -- by : and . */
	while(*p) {
		if(*p == '.') return 0;
		if(*p == ':') return 1;
		++p;
	}
	return 0;
}

/* returns range type. mask is the 2nd part of the range */
int
parse_acl_range_type(char* ip, char** mask)
{
	char *p;
	if((p=strchr(ip, '&'))!=0) {
		*p = 0;
		*mask = p+1;
		return acl_range_mask;
	}
	if((p=strchr(ip, '/'))!=0) {
		*p = 0;
		*mask = p+1;
		return acl_range_subnet;
	}
	if((p=strchr(ip, '-'))!=0) {
		*p = 0;
		*mask = p+1;
		return acl_range_minmax;
	}
	*mask = 0;
	return acl_range_single;
}

/* parses subnet mask, fills 0 mask as well */
void
parse_acl_range_subnet(char* p, void* addr, int maxbits)
{
	int subnet_bits = atoi(p);
	uint8_t* addr_bytes = (uint8_t*)addr;
	if(subnet_bits == 0 && strcmp(p, "0")!=0) {
		c_error_msg("bad subnet range '%s'", p);
		return;
	}
	if(subnet_bits < 0 || subnet_bits > maxbits) {
		c_error_msg("subnet of %d bits out of range [0..%d]", subnet_bits, maxbits);
		return;
	}
	/* fill addr with n bits of 1s (struct has been zeroed) */
	while(subnet_bits >= 8) {
		*addr_bytes++ = 0xff;
		subnet_bits -= 8;
	}
	if(subnet_bits > 0) {
		uint8_t shifts[] = {0x0, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
		*addr_bytes = shifts[subnet_bits];
	}
}

acl_options_t*
parse_acl_info(region_type* region, char* ip, const char* key)
{
	char* p;
	acl_options_t* acl = (acl_options_t*)region_alloc(region, sizeof(acl_options_t));
	acl->next = 0;
	/* ip */
	acl->ip_address_spec = region_strdup(region, ip);
	acl->use_axfr_only = 0;
	acl->allow_udp = 0;
	acl->ixfr_disabled = 0;
	acl->bad_xfr_count = 0;
	acl->key_options = 0;
	acl->is_ipv6 = 0;
	acl->port = 0;
	memset(&acl->addr, 0, sizeof(union acl_addr_storage));
	memset(&acl->range_mask, 0, sizeof(union acl_addr_storage));
	if((p=strrchr(ip, '@'))!=0) {
		if(atoi(p+1) == 0) c_error("expected port number after '@'");
		else acl->port = atoi(p+1);
		*p=0;
	}
	acl->rangetype = parse_acl_range_type(ip, &p);
	if(parse_acl_is_ipv6(ip)) {
		acl->is_ipv6 = 1;
#ifdef INET6
		if(inet_pton(AF_INET6, ip, &acl->addr.addr6) != 1)
			c_error_msg("Bad ip6 address '%s'", ip);
		if(acl->rangetype==acl_range_mask || acl->rangetype==acl_range_minmax)
			if(inet_pton(AF_INET6, p, &acl->range_mask.addr6) != 1)
				c_error_msg("Bad ip6 address mask '%s'", p);
		if(acl->rangetype==acl_range_subnet)
			parse_acl_range_subnet(p, &acl->range_mask.addr6, 128);
#else
		c_error_msg("encountered IPv6 address '%s'.", ip);
#endif /* INET6 */
	} else {
		acl->is_ipv6 = 0;
		if(inet_pton(AF_INET, ip, &acl->addr.addr) != 1)
			c_error_msg("Bad ip4 address '%s'", ip);
		if(acl->rangetype==acl_range_mask || acl->rangetype==acl_range_minmax)
			if(inet_pton(AF_INET, p, &acl->range_mask.addr) != 1)
				c_error_msg("Bad ip4 address mask '%s'", p);
		if(acl->rangetype==acl_range_subnet)
			parse_acl_range_subnet(p, &acl->range_mask.addr, 32);
	}

	/* key */
	if(strcmp(key, "NOKEY")==0) {
		acl->nokey = 1;
		acl->blocked = 0;
		acl->key_name = 0;
	} else if(strcmp(key, "BLOCKED")==0) {
		acl->nokey = 0;
		acl->blocked = 1;
		acl->key_name = 0;
	} else {
		acl->nokey = 0;
		acl->blocked = 0;
		acl->key_name = region_strdup(region, key);
	}
	return acl;
}

/* copy acl list at end of parser start, update current */
static
void append_acl(acl_options_t** start, acl_options_t** cur,
	acl_options_t* list)
{
	while(list) {
		acl_options_t* acl = copy_acl(cfg_parser->opt->region, list);
		acl->next = NULL;
		if(*cur)
			(*cur)->next = acl;
		else	*start = acl;
		*cur = acl;
		list = list->next;
	}
}

void
config_apply_pattern(const char* name)
{
	/* find the pattern */
	pattern_options_t* pat = pattern_options_find(cfg_parser->opt, name);
	pattern_options_t* a = cfg_parser->current_pattern;
	if(!pat) {
		c_error_msg("could not find pattern %s", name);
		return;
	}

	/* apply settings */
	if(pat->zonefile)
		a->zonefile = region_strdup(cfg_parser->opt->region,
			pat->zonefile);
	if(pat->zonestats)
		a->zonestats = region_strdup(cfg_parser->opt->region,
			pat->zonestats);
	if(!pat->allow_axfr_fallback_is_default) {
		a->allow_axfr_fallback = pat->allow_axfr_fallback;
		a->allow_axfr_fallback_is_default = 0;
	}
	if(!pat->notify_retry_is_default) {
		a->notify_retry = pat->notify_retry;
		a->notify_retry_is_default = 0;
	}
	if(!pat->max_refresh_time_is_default) {
		a->max_refresh_time = pat->max_refresh_time;
		a->max_refresh_time_is_default = 0;
	}
	if(!pat->min_refresh_time_is_default) {
		a->min_refresh_time = pat->min_refresh_time;
		a->min_refresh_time_is_default = 0;
	}
	if(!pat->max_retry_time_is_default) {
		a->max_retry_time = pat->max_retry_time;
		a->max_retry_time_is_default = 0;
	}
	if(!pat->min_retry_time_is_default) {
		a->min_retry_time = pat->min_retry_time;
		a->min_retry_time_is_default = 0;
	}
	a->size_limit_xfr = pat->size_limit_xfr;
#ifdef RATELIMIT
	a->rrl_whitelist |= pat->rrl_whitelist;
#endif
	/* append acl items */
	append_acl(&a->allow_notify, &cfg_parser->current_allow_notify,
		pat->allow_notify);
	append_acl(&a->request_xfr, &cfg_parser->current_request_xfr,
		pat->request_xfr);
	append_acl(&a->notify, &cfg_parser->current_notify, pat->notify);
	append_acl(&a->provide_xfr, &cfg_parser->current_provide_xfr,
		pat->provide_xfr);
	append_acl(&a->outgoing_interface, &cfg_parser->
		current_outgoing_interface, pat->outgoing_interface);
	if(pat->multi_master_check)
		a->multi_master_check = pat->multi_master_check;
}

void
nsd_options_destroy(nsd_options_t* opt)
{
	region_destroy(opt->region);
}

unsigned getzonestatid(nsd_options_t* opt, zone_options_t* zopt)
{
#ifdef USE_ZONE_STATS
	const char* statname;
	struct zonestatname* n;
	rbnode_t* res;
	/* try to find the instantiated zonestat name */
	if(!zopt->pattern->zonestats || zopt->pattern->zonestats[0]==0)
		return 0; /* no zone stats */
	statname = config_cook_string(zopt, zopt->pattern->zonestats);
	res = rbtree_search(opt->zonestatnames, statname);
	if(res)
		return ((struct zonestatname*)res)->id;
	/* create it */
	n = (struct zonestatname*)xalloc(sizeof(*n));
	memset(n, 0, sizeof(*n));
	n->node.key = strdup(statname);
	if(!n->node.key) {
		log_msg(LOG_ERR, "malloc failed: %s", strerror(errno));
		exit(1);
	}
	n->id = (unsigned)(opt->zonestatnames->count);
	rbtree_insert(opt->zonestatnames, (rbnode_t*)n);
	return n->id;
#else /* USE_ZONE_STATS */
	(void)opt; (void)zopt;
	return 0;
#endif /* USE_ZONE_STATS */
}
