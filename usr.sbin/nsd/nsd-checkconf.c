/*
 * checkconf - Read and repeat configuration file to output.
 *
 * Copyright (c) 2001-2011, NLnet Labs. All rights reserved.
 *
 * See LICENSE for the license.
 *
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include "tsig.h"
#include "options.h"
#include "util.h"
#include "dname.h"

extern char *optarg;
extern int optind;

#define ZONE_GET_ACL(NAME, VAR) 		\
	if (strcasecmp(#NAME, (VAR)) == 0) { 	\
		quote_acl((zone->NAME)); 	\
		return; 			\
	}

#define ZONE_GET_OUTGOING(NAME, VAR)			\
	if (strcasecmp(#NAME, (VAR)) == 0) {		\
		acl_options_t* acl; 			\
		for(acl=zone->NAME; acl; acl=acl->next)	\
			quote(acl->ip_address_spec);	\
		return; 				\
	}

#define ZONE_GET_STR(NAME, VAR) 		\
	if (strcasecmp(#NAME, (VAR)) == 0) { 	\
		quote(zone->NAME); 		\
		return; 			\
	}

#define ZONE_GET_BIN(NAME, VAR) 			\
	if (strcasecmp(#NAME, (VAR)) == 0) { 		\
		printf("%s\n", zone->NAME?"yes":"no"); 	\
	}

#define SERV_GET_BIN(NAME, VAR) 			\
	if (strcasecmp(#NAME, (VAR)) == 0) { 		\
		printf("%s\n", opt->NAME?"yes":"no"); 	\
	}

#define SERV_GET_STR(NAME, VAR) 		\
	if (strcasecmp(#NAME, (VAR)) == 0) { 	\
		quote(opt->NAME); 		\
		return; 			\
	}

#define SERV_GET_INT(NAME, VAR) 		\
	if (strcasecmp(#NAME, (VAR)) == 0) { 	\
		printf("%d\n", (int) opt->NAME); 	\
		return; 			\
	}

#define SERV_GET_IP(NAME, VAR) 				\
	if (strcasecmp(#NAME, (VAR)) == 0) { 		\
		for(ip = opt->ip_addresses; ip; ip=ip->next)	\
		{						\
			quote(ip->address);			\
		}						\
		return;						\
	}

static char buf[BUFSIZ];

static char *
underscore(const char *s) {
	const char *j = s;
	size_t i = 0;

	while(j && *j) {
		if (*j == '-') {
			buf[i++] = '_';
		} else {
			buf[i++] = *j;
		}
		j++;
		if (i >= BUFSIZ) {
			return NULL;
		}
	}
	buf[i] = '\0';
	return buf;
}

static void
usage(void)
{
	fprintf(stderr, "usage: nsd-checkconf [-v|-h] [-o option] [-z zonename]\n");
	fprintf(stderr, "                     [-s keyname] <configfilename>\n");
	fprintf(stderr, "       Checks NSD configuration file for errors.\n");
	fprintf(stderr, "       Version %s. Report bugs to <%s>.\n\n",
		PACKAGE_VERSION, PACKAGE_BUGREPORT);
	fprintf(stderr, "Use with a configfile as argument to check syntax.\n");
	fprintf(stderr, "Use with -o, -z or -s options to query the configuration.\n\n");
	fprintf(stderr, "-v		Verbose, echo settings that take effect to std output.\n");
	fprintf(stderr, "-h		Print this help information.\n");
	fprintf(stderr, "-o option	Print value of the option specified to stdout.\n");
	fprintf(stderr, "-z zonename	Print option value for the zone given.\n");
	fprintf(stderr, "-a keyname	Print algorithm name for the TSIG key.\n");
	fprintf(stderr, "-s keyname	Print base64 secret blob for the TSIG key.\n");
	exit(1);
}

static void
print_string_var(const char* varname, const char* value)
{
	if (!value) {
		printf("\t#%s\n", varname);
	} else {
		printf("\t%s \"%s\"\n", varname, value);
	}
}

static void
quote(const char *v)
{
	if(v==NULL)
		printf("\n");
	else
		printf("%s\n", v);
}

static void
quote_acl(acl_options_t* acl)
{
	while(acl)
	{
		printf("%s %s\n", acl->ip_address_spec,
			acl->nokey?"NOKEY":(acl->blocked?"BLOCKED":
			(acl->key_name?acl->key_name:"(null)")));
		acl=acl->next;
	}
}

static void
print_acl(const char* varname, acl_options_t* acl)
{
	while(acl)
	{
		printf("\t%s ", varname);
		if(acl->use_axfr_only)
			printf("AXFR ");
		if(acl->allow_udp)
			printf("UDP ");
		printf("%s %s\n", acl->ip_address_spec,
			acl->nokey?"NOKEY":(acl->blocked?"BLOCKED":
			(acl->key_name?acl->key_name:"(null)")));
		if(1) {
			printf("\t# %s", acl->is_ipv6?"ip6":"ip4");
			if(acl->port == 0) printf(" noport");
			else printf(" port=%d", acl->port);
			if(acl->rangetype == acl_range_single) printf(" single");
			if(acl->rangetype == acl_range_mask)   printf(" masked");
			if(acl->rangetype == acl_range_subnet) printf(" subnet");
			if(acl->rangetype == acl_range_minmax) printf(" minmax");
			if(acl->is_ipv6) {
#ifdef INET6
				char dest[128];
				inet_ntop(AF_INET6, &acl->addr.addr6, dest, sizeof(dest));
				printf(" addr=%s", dest);
				if(acl->rangetype != acl_range_single) {
					inet_ntop(AF_INET6, &acl->range_mask.addr6, dest, sizeof(dest));
					printf(" rangemask=%s", dest);
				}
#else
				printf(" ip6addr-noip6defined");
#endif
			} else {
				char dest[128];
				inet_ntop(AF_INET, &acl->addr.addr, dest, sizeof(dest));
				printf(" addr=%s", dest);
				if(acl->rangetype != acl_range_single) {
					inet_ntop(AF_INET, &acl->range_mask.addr, dest, sizeof(dest));
					printf(" rangemask=%s", dest);
				}
			}
			printf("\n");
		}
		acl=acl->next;
	}
}

static void
print_acl_ips(const char* varname, acl_options_t* acl)
{
	while(acl)
	{
		printf("\t%s %s\n", varname, acl->ip_address_spec);
		acl=acl->next;
	}
}

void
config_print_zone(nsd_options_t* opt, const char* k, int s, const char *o, const char *z)
{
	zone_options_t* zone;
	ip_address_option_t* ip;

	if (k) {
		/* find key */
		key_options_t* key = opt->keys;
		for( ; key ; key=key->next) {
			if(strcmp(key->name, k) == 0) {
				if (s) {
					quote(key->secret);
				} else {
					quote(key->algorithm);
				}
				return;
			}
		}
		printf("Could not find key %s\n", k);
		return;
	}

	if (!o) {
		return;
	}

	if (z) {
		const dname_type *dname = dname_parse(opt->region, z);
		if(!dname) {
			printf("Could not parse zone name %s\n", z);
			exit(1);
		}
		/* look per zone */
		RBTREE_FOR(zone, zone_options_t*, opt->zone_options)
		{
			if (dname_compare(dname, zone->node.key) == 0) {
				/* -z matches, return are in the defines */
				ZONE_GET_STR(name, o);
				ZONE_GET_STR(zonefile, o);
				ZONE_GET_ACL(request_xfr, o);
				ZONE_GET_ACL(provide_xfr, o);
				ZONE_GET_ACL(allow_notify, o);
				ZONE_GET_ACL(notify, o);
				ZONE_GET_BIN(notify_retry, o);
				ZONE_GET_OUTGOING(outgoing_interface, o);
				ZONE_GET_BIN(allow_axfr_fallback, o);
				printf("Zone option not handled: %s %s\n", z, o);
				exit(1);
			}
		}
		printf("Zone does not exist: %s\n", z);
		exit(1);
	} else {
		/* look in the server section */
		SERV_GET_IP(ip_address, o);
		/* bin */
		SERV_GET_BIN(debug_mode, o);
		SERV_GET_BIN(ip4_only, o);
		SERV_GET_BIN(ip6_only, o);
		SERV_GET_BIN(hide_version, o);
		/* str */
		SERV_GET_STR(database, o);
		SERV_GET_STR(identity, o);
		SERV_GET_STR(nsid, o);
		SERV_GET_STR(logfile, o);
		SERV_GET_STR(pidfile, o);
		SERV_GET_STR(chroot, o);
		SERV_GET_STR(username, o);
		SERV_GET_STR(zonesdir, o);
		SERV_GET_STR(difffile, o);
		SERV_GET_STR(xfrdfile, o);
		SERV_GET_STR(port, o);
		/* int */
		SERV_GET_INT(server_count, o);
		SERV_GET_INT(tcp_count, o);
		SERV_GET_INT(tcp_query_count, o);
		SERV_GET_INT(tcp_timeout, o);
		SERV_GET_INT(ipv4_edns_size, o);
		SERV_GET_INT(ipv6_edns_size, o);
		SERV_GET_INT(statistics, o);
		SERV_GET_INT(xfrd_reload_timeout, o);
		SERV_GET_INT(verbosity, o);

		if(strcasecmp(o, "zones") == 0) {
			RBTREE_FOR(zone, zone_options_t*, opt->zone_options)
				quote(zone->name);
			return;
		}
		printf("Server option not handled: %s\n", o);
		exit(1);
	}
}

void
config_test_print_server(nsd_options_t* opt)
{
	ip_address_option_t* ip;
	key_options_t* key;
	zone_options_t* zone;

	printf("# Config settings.\n");
	printf("server:\n");
	printf("\tdebug-mode: %s\n", opt->debug_mode?"yes":"no");
	printf("\tip4-only: %s\n", opt->ip4_only?"yes":"no");
	printf("\tip6-only: %s\n", opt->ip6_only?"yes":"no");
	printf("\thide-version: %s\n", opt->hide_version?"yes":"no");
	print_string_var("database:", opt->database);
	print_string_var("identity:", opt->identity);
	print_string_var("nsid:", opt->nsid);
	print_string_var("logfile:", opt->logfile);
	printf("\tserver_count: %d\n", opt->server_count);
	printf("\ttcp_count: %d\n", opt->tcp_count);
	printf("\ttcp_query_count: %d\n", opt->tcp_query_count);
	printf("\ttcp_timeout: %d\n", opt->tcp_timeout);
	printf("\tipv4-edns-size: %d\n", (int) opt->ipv4_edns_size);
	printf("\tipv6-edns-size: %d\n", (int) opt->ipv6_edns_size);
	print_string_var("pidfile:", opt->pidfile);
	print_string_var("port:", opt->port);
	printf("\tstatistics: %d\n", opt->statistics);
	print_string_var("chroot:", opt->chroot);
	print_string_var("username:", opt->username);
	print_string_var("zonesdir:", opt->zonesdir);
	print_string_var("difffile:", opt->difffile);
	print_string_var("xfrdfile:", opt->xfrdfile);
	printf("\txfrd_reload_timeout: %d\n", opt->xfrd_reload_timeout);
	printf("\tverbosity: %d\n", opt->verbosity);

	for(ip = opt->ip_addresses; ip; ip=ip->next)
	{
		print_string_var("ip-address:", ip->address);
	}
	for(key = opt->keys; key; key=key->next)
	{
		printf("\nkey:\n");
		print_string_var("name:", key->name);
		print_string_var("algorithm:", key->algorithm);
		print_string_var("secret:", key->secret);
	}
	RBTREE_FOR(zone, zone_options_t*, opt->zone_options)
	{
		printf("\nzone:\n");
		print_string_var("name:", zone->name);
		print_string_var("zonefile:", zone->zonefile);
		print_acl("allow-notify:", zone->allow_notify);
		print_acl("request-xfr:", zone->request_xfr);
		printf("\tnotify-retry: %d\n", zone->notify_retry);
		print_acl("notify:", zone->notify);
		print_acl("provide-xfr:", zone->provide_xfr);
		print_acl_ips("outgoing-interface:", zone->outgoing_interface);
		printf("\tallow-axfr-fallback: %s\n", zone->allow_axfr_fallback?"yes":"no");
	}

}

static int
additional_checks(nsd_options_t* opt, const char* filename)
{
	ip_address_option_t* ip = opt->ip_addresses;
	zone_options_t* zone;
	key_options_t* key;
	int num = 0;
	int errors = 0;
	while(ip) {
		num++;
		ip = ip->next;
	}
	if(num > MAX_INTERFACES) {
		fprintf(stderr, "%s: too many interfaces (ip-address:) specified.\n", filename);
		errors ++;
	}

	RBTREE_FOR(zone, zone_options_t*, opt->zone_options)
	{
		const dname_type* dname = dname_parse(opt->region, zone->name); /* memory leak. */
		if(!dname) {
			fprintf(stderr, "%s: cannot parse zone name syntax for zone %s.\n", filename, zone->name);
			errors ++;
		}
		if(zone->allow_notify && !zone->request_xfr) {
			fprintf(stderr, "%s: zone %s has allow-notify but no request-xfr"
				" items. Where can it get a zone transfer when a notify "
				"is received?\n", filename, zone->name);
			errors ++;
		}
	}

	for(key = opt->keys; key; key=key->next)
	{
		const dname_type* dname = dname_parse(opt->region, key->name); /* memory leak. */
		uint8_t data[4000];
		int size;

		if(!dname) {
			fprintf(stderr, "%s: cannot parse tsig name syntax for key %s.\n", filename, key->name);
			errors ++;
		}
		size = b64_pton(key->secret, data, sizeof(data));
		if(size == -1) {
			fprintf(stderr, "%s: cannot base64 decode tsig secret: for key %s.\n", filename, key->name);
			errors ++;
		}
		if(tsig_get_algorithm_by_name(key->algorithm) != NULL)
		{
			fprintf(stderr, "%s: bad tsig algorithm %s: for key \
%s.\n", filename, key->algorithm, key->name);
			errors ++;
		}
	}

#ifndef BIND8_STATS
	if(opt->statistics > 0)
	{
		fprintf(stderr, "%s: 'statistics: %d' but BIND 8 statistics feature not enabled.\n",
			filename, opt->statistics);
		errors ++;
	}
#endif
#ifndef HAVE_CHROOT
	if(opt->chroot != 0)
	{
		fprintf(stderr, "%s: chroot %s given. chroot not supported on this platform.\n",
			filename, opt->chroot);
		errors ++;
	}
#endif
	if (opt->identity && strlen(opt->identity) > UCHAR_MAX) {
                fprintf(stderr, "%s: server identity too long (%u characters)\n",
                      filename, (unsigned) strlen(opt->identity));
		errors ++;
        }

	/* not done here: parsing of ip-address. parsing of username. */

        if (opt->chroot) {
                int l = strlen(opt->chroot);

                if (strncmp(opt->chroot, opt->pidfile, l) != 0) {
			fprintf(stderr, "%s: pidfile %s is not relative to chroot %s.\n",
				filename, opt->pidfile, opt->chroot);
			errors ++;
                }
		if (strncmp(opt->chroot, opt->database, l) != 0) {
			fprintf(stderr, "%s: databasefile %s is not relative to chroot %s.\n",
				filename, opt->database, opt->chroot);
			errors ++;
                }
		if (strncmp(opt->chroot, opt->difffile, l) != 0) {
			fprintf(stderr, "%s: difffile %s is not relative to chroot %s.\n",
				filename, opt->difffile, opt->chroot);
			errors ++;
                }
		if (strncmp(opt->chroot, opt->xfrdfile, l) != 0) {
			fprintf(stderr, "%s: xfrdfile %s is not relative to chroot %s.\n",
				filename, opt->xfrdfile, opt->chroot);
			errors ++;
                }
        }
	if (atoi(opt->port) <= 0) {
		fprintf(stderr, "%s: port number '%s' is not a positive number.\n",
			filename, opt->port);
		errors ++;
	}
	if(errors != 0) {
		fprintf(stderr, "%s: %d semantic errors in %d zones, %d keys.\n",
			filename, errors, (int)nsd_options_num_zones(opt),
			(int)opt->numkeys);
	}

	return (errors == 0);
}

int
main(int argc, char* argv[])
{
	int c;
	int verbose = 0;
	int key_sec = 0;
	const char * conf_opt = NULL; /* what option do you want? Can be NULL -> print all */
	const char * conf_zone = NULL; /* what zone are we talking about */
	const char * conf_key = NULL; /* what key is needed */
	const char* configfile;
	nsd_options_t *options;

	log_init("nsd-checkconf");


        /* Parse the command line... */
        while ((c = getopt(argc, argv, "vo:a:s:z:")) != -1) {
		switch (c) {
		case 'v':
			verbose = 1;
			break;
		case 'o':
			conf_opt = optarg;
			break;
		case 'a':
			if (conf_key) {
				fprintf(stderr, "Error: cannot combine -a with -s or other -a.\n");
				exit(1);
			}
			conf_key = optarg;
			break;
		case 's':
			if (conf_key) {
				fprintf(stderr, "Error: cannot combine -s with -a or other -s.\n");
				exit(1);
			}
			conf_key = optarg;
			key_sec = 1;
			break;
		case 'z':
			conf_zone = optarg;
			break;
		default:
			usage();
		};
	}
        argc -= optind;
        argv += optind;
        if (argc == 0 || argc>=2) {
		usage();
	}
	configfile = argv[0];

	/* read config file */
	options = nsd_options_create(region_create(xalloc, free));
	if (!parse_options_file(options, configfile) ||
	   !additional_checks(options, configfile)) {
		exit(2);
	}
	if (conf_opt || conf_key) {
		config_print_zone(options, conf_key, key_sec, underscore(conf_opt), conf_zone);
	} else {
		if (verbose) {
			printf("# Read file %s: %d zones, %d keys.\n",
				configfile,
				(int)nsd_options_num_zones(options),
				(int)options->numkeys);
			config_test_print_server(options);
		}
	}
	return 0;
}
