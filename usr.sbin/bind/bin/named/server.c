/*
 * Copyright (C) 1999-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: server.c,v 1.339.2.18 2003/09/19 13:40:42 marka Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/app.h>
#include <isc/base64.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/lex.h>
#include <isc/print.h>
#include <isc/resource.h>
#include <isc/stdio.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <isccfg/cfg.h>
#include <isccfg/check.h>

#include <dns/cache.h>
#include <dns/db.h>
#include <dns/dispatch.h>
#include <dns/forward.h>
#include <dns/journal.h>
#include <dns/keytable.h>
#include <dns/master.h>
#include <dns/peer.h>
#include <dns/rdataclass.h>
#include <dns/rdatastruct.h>
#include <dns/resolver.h>
#include <dns/rootns.h>
#include <dns/stats.h>
#include <dns/tkey.h>
#include <dns/view.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include <dst/dst.h>
#include <dst/result.h>

#include <named/client.h>
#include <named/config.h>
#include <named/control.h>
#include <named/interfacemgr.h>
#include <named/log.h>
#include <named/logconf.h>
#include <named/lwresd.h>
#include <named/os.h>
#include <named/server.h>
#include <named/tkeyconf.h>
#include <named/tsigconf.h>
#include <named/zoneconf.h>

/*
 * Check an operation for failure.  Assumes that the function
 * using it has a 'result' variable and a 'cleanup' label.
 */
#define CHECK(op) \
	do { result = (op); 				  	 \
	       if (result != ISC_R_SUCCESS) goto cleanup; 	 \
	} while (0)

#define CHECKM(op, msg) \
	do { result = (op); 				  	  \
	       if (result != ISC_R_SUCCESS) {			  \
			isc_log_write(ns_g_lctx,		  \
				      NS_LOGCATEGORY_GENERAL,	  \
				      NS_LOGMODULE_SERVER,	  \
				      ISC_LOG_ERROR,		  \
				      "%s: %s", msg,		  \
				      isc_result_totext(result)); \
			goto cleanup;				  \
		}						  \
	} while (0)						  \

#define CHECKFATAL(op, msg) \
	do { result = (op); 				  	  \
	       if (result != ISC_R_SUCCESS)			  \
			fatal(msg, result);			  \
	} while (0)						  \

static void
fatal(const char *msg, isc_result_t result);

static void
ns_server_reload(isc_task_t *task, isc_event_t *event);

static isc_result_t
ns_listenelt_fromconfig(cfg_obj_t *listener, cfg_obj_t *config,
			ns_aclconfctx_t *actx,
			isc_mem_t *mctx, ns_listenelt_t **target);
static isc_result_t
ns_listenlist_fromconfig(cfg_obj_t *listenlist, cfg_obj_t *config,
			 ns_aclconfctx_t *actx,
			 isc_mem_t *mctx, ns_listenlist_t **target);

static isc_result_t
configure_forward(cfg_obj_t *config, dns_view_t *view, dns_name_t *origin,
		  cfg_obj_t *forwarders, cfg_obj_t *forwardtype);

static isc_result_t
configure_zone(cfg_obj_t *config, cfg_obj_t *zconfig, cfg_obj_t *vconfig,
	       isc_mem_t *mctx, dns_view_t *view,
	       ns_aclconfctx_t *aclconf);

/*
 * Configure a single view ACL at '*aclp'.  Get its configuration by
 * calling 'getvcacl' (for per-view configuration) and maybe 'getscacl'
 * (for a global default).
 */
static isc_result_t
configure_view_acl(cfg_obj_t *vconfig, cfg_obj_t *config,
		   const char *aclname, ns_aclconfctx_t *actx,
		   isc_mem_t *mctx, dns_acl_t **aclp)
{
	isc_result_t result;
	cfg_obj_t *maps[3];
	cfg_obj_t *aclobj = NULL;
	int i = 0;

	if (*aclp != NULL)
		dns_acl_detach(aclp);
	if (vconfig != NULL)
		maps[i++] = cfg_tuple_get(vconfig, "options");
	if (config != NULL) {
		cfg_obj_t *options = NULL;
		cfg_map_get(config, "options", &options);
		if (options != NULL)
			maps[i++] = options;
	}
	maps[i] = NULL;

	result = ns_config_get(maps, aclname, &aclobj);
	if (aclobj == NULL)
		/*
		 * No value available.  *aclp == NULL.
		 */
		return (ISC_R_SUCCESS);

	result = ns_acl_fromconfig(aclobj, config, actx, mctx, aclp);

	return (result);
}

#ifdef ISC_RFC2535
static isc_result_t
configure_view_dnsseckey(cfg_obj_t *vconfig, cfg_obj_t *key,
			 dns_keytable_t *keytable, isc_mem_t *mctx)
{
	dns_rdataclass_t viewclass;
	dns_rdata_key_t keystruct;
	isc_uint32_t flags, proto, alg;
	char *keystr, *keynamestr;
	unsigned char keydata[4096];
	isc_buffer_t keydatabuf;
	unsigned char rrdata[4096];
	isc_buffer_t rrdatabuf;
	isc_region_t r;
	dns_fixedname_t fkeyname;
	dns_name_t *keyname;
	isc_buffer_t namebuf;
	isc_result_t result;
	dst_key_t *dstkey = NULL;

	flags = cfg_obj_asuint32(cfg_tuple_get(key, "flags"));
	proto = cfg_obj_asuint32(cfg_tuple_get(key, "protocol"));
	alg = cfg_obj_asuint32(cfg_tuple_get(key, "algorithm"));
	keyname = dns_fixedname_name(&fkeyname);
	keynamestr = cfg_obj_asstring(cfg_tuple_get(key, "name"));

	if (vconfig == NULL)
		viewclass = dns_rdataclass_in;
	else {
		cfg_obj_t *classobj = cfg_tuple_get(vconfig, "class");
		CHECK(ns_config_getclass(classobj, dns_rdataclass_in,
					 &viewclass));
	}
	keystruct.common.rdclass = viewclass;
	keystruct.common.rdtype = dns_rdatatype_key;
	/*
	 * The key data in keystruct is not dynamically allocated.
	 */
	keystruct.mctx = NULL;

	ISC_LINK_INIT(&keystruct.common, link);

	if (flags > 0xffff)
		CHECKM(ISC_R_RANGE, "key flags");
	if (proto > 0xff)
		CHECKM(ISC_R_RANGE, "key protocol");
	if (alg > 0xff)
		CHECKM(ISC_R_RANGE, "key algorithm");
	keystruct.flags = (isc_uint16_t)flags;
	keystruct.protocol = (isc_uint8_t)proto;
	keystruct.algorithm = (isc_uint8_t)alg;

	isc_buffer_init(&keydatabuf, keydata, sizeof(keydata));
	isc_buffer_init(&rrdatabuf, rrdata, sizeof(rrdata));

	keystr = cfg_obj_asstring(cfg_tuple_get(key, "key"));
	CHECK(isc_base64_decodestring(keystr, &keydatabuf));
	isc_buffer_usedregion(&keydatabuf, &r);
	keystruct.datalen = r.length;
	keystruct.data = r.base;

	CHECK(dns_rdata_fromstruct(NULL,
				   keystruct.common.rdclass,
				   keystruct.common.rdtype,
				   &keystruct, &rrdatabuf));
	dns_fixedname_init(&fkeyname);
	isc_buffer_init(&namebuf, keynamestr, strlen(keynamestr));
	isc_buffer_add(&namebuf, strlen(keynamestr));
	CHECK(dns_name_fromtext(keyname, &namebuf,
				dns_rootname, ISC_FALSE,
				NULL));
	CHECK(dst_key_fromdns(keyname, viewclass, &rrdatabuf,
			      mctx, &dstkey));

	CHECK(dns_keytable_add(keytable, &dstkey));
	INSIST(dstkey == NULL);
	return (ISC_R_SUCCESS);

 cleanup:
	if (result == DST_R_NOCRYPTO) {
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_ERROR,
			    "ignoring trusted key for '%s': no crypto support",
			    keynamestr);
		result = ISC_R_SUCCESS;
	} else {
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_ERROR,
			    "configuring trusted key for '%s': %s",
			    keynamestr, isc_result_totext(result));
		result = ISC_R_FAILURE;
	}

	if (dstkey != NULL)
		dst_key_free(&dstkey);

	return (result);
}
#endif

/*
 * Configure DNSSEC keys for a view.  Currently used only for
 * the security roots.
 *
 * The per-view configuration values and the server-global defaults are read
 * from 'vconfig' and 'config'.  The variable to be configured is '*target'.
 */
static isc_result_t
configure_view_dnsseckeys(cfg_obj_t *vconfig, cfg_obj_t *config,
			  isc_mem_t *mctx, dns_keytable_t **target)
{
	isc_result_t result;
#ifdef ISC_RFC2535
	cfg_obj_t *keys = NULL;
	cfg_obj_t *voptions = NULL;
	cfg_listelt_t *element, *element2;
	cfg_obj_t *keylist;
	cfg_obj_t *key;
#endif
	dns_keytable_t *keytable = NULL;

	CHECK(dns_keytable_create(mctx, &keytable));

#ifndef ISC_RFC2535
	UNUSED(vconfig);
	UNUSED(config);
#else
	if (vconfig != NULL)
		voptions = cfg_tuple_get(vconfig, "options");

	keys = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "trusted-keys", &keys);
	if (keys == NULL)
		(void)cfg_map_get(config, "trusted-keys", &keys);

	for (element = cfg_list_first(keys);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		keylist = cfg_listelt_value(element);
		for (element2 = cfg_list_first(keylist);
		     element2 != NULL;
		     element2 = cfg_list_next(element2))
		{
			key = cfg_listelt_value(element2);
			CHECK(configure_view_dnsseckey(vconfig, key,
						       keytable, mctx));
		}
	}
#endif
	dns_keytable_detach(target);
	*target = keytable; /* Transfer ownership. */
	keytable = NULL;
	result = ISC_R_SUCCESS;
	
 cleanup:
	return (result);
}


/*
 * Get a dispatch appropriate for the resolver of a given view.
 */
static isc_result_t
get_view_querysource_dispatch(cfg_obj_t **maps,
			      int af, dns_dispatch_t **dispatchp)
{
	isc_result_t result;
	dns_dispatch_t *disp;
	isc_sockaddr_t sa;
	unsigned int attrs, attrmask;
	cfg_obj_t *obj = NULL;

	/*
	 * Make compiler happy.
	 */
	result = ISC_R_FAILURE;

	switch (af) {
	case AF_INET:
		result = ns_config_get(maps, "query-source", &obj);
		INSIST(result == ISC_R_SUCCESS);

		break;
	case AF_INET6:
		result = ns_config_get(maps, "query-source-v6", &obj);
		INSIST(result == ISC_R_SUCCESS);
		break;
	default:
		INSIST(0);
	}

	sa = *(cfg_obj_assockaddr(obj));
	INSIST(isc_sockaddr_pf(&sa) == af);

	/*
	 * If we don't support this address family, we're done!
	 */
	switch (af) {
	case AF_INET:
		result = isc_net_probeipv4();
		break;
	case AF_INET6:
		result = isc_net_probeipv6();
		break;
	default:
		INSIST(0);
	}
	if (result != ISC_R_SUCCESS)
		return (ISC_R_SUCCESS);

	/*
	 * Try to find a dispatcher that we can share.
	 */
	attrs = 0;
	attrs |= DNS_DISPATCHATTR_UDP;
	switch (af) {
	case AF_INET:
		attrs |= DNS_DISPATCHATTR_IPV4;
		break;
	case AF_INET6:
		attrs |= DNS_DISPATCHATTR_IPV6;
		break;
	}
	attrmask = 0;
	attrmask |= DNS_DISPATCHATTR_UDP;
	attrmask |= DNS_DISPATCHATTR_TCP;
	attrmask |= DNS_DISPATCHATTR_IPV4;
	attrmask |= DNS_DISPATCHATTR_IPV6;

	disp = NULL;
	result = dns_dispatch_getudp(ns_g_dispatchmgr, ns_g_socketmgr,
				     ns_g_taskmgr, &sa, 4096,
				     1000, 32768, 16411, 16433,
				     attrs, attrmask, &disp);
	if (result != ISC_R_SUCCESS) {
		isc_sockaddr_t any;
		char buf[ISC_SOCKADDR_FORMATSIZE];

		switch (af) {
		case AF_INET:
			isc_sockaddr_any(&any);
			break;
		case AF_INET6:
			isc_sockaddr_any6(&any);
			break;
		}
		if (isc_sockaddr_equal(&sa, &any))
			return (ISC_R_SUCCESS);
		isc_sockaddr_format(&sa, buf, sizeof(buf));
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "could not get query source dispatcher (%s)",
			      buf);
		return (result);
	}

	*dispatchp = disp;

	return (ISC_R_SUCCESS);
}

static isc_result_t
configure_peer(cfg_obj_t *cpeer, isc_mem_t *mctx, dns_peer_t **peerp) {
	isc_sockaddr_t *sa;
	isc_netaddr_t na;
	dns_peer_t *peer;
	cfg_obj_t *obj;
	char *str;
	isc_result_t result;

	sa = cfg_obj_assockaddr(cfg_map_getname(cpeer));
	isc_netaddr_fromsockaddr(&na, sa);

	peer = NULL;
	result = dns_peer_new(mctx, &na, &peer);
	if (result != ISC_R_SUCCESS)
		return (result);

	obj = NULL;
	(void)cfg_map_get(cpeer, "bogus", &obj);
	if (obj != NULL)
		dns_peer_setbogus(peer, cfg_obj_asboolean(obj));

	obj = NULL;
	(void)cfg_map_get(cpeer, "provide-ixfr", &obj);
	if (obj != NULL)
		dns_peer_setprovideixfr(peer, cfg_obj_asboolean(obj));

	obj = NULL;
	(void)cfg_map_get(cpeer, "request-ixfr", &obj);
	if (obj != NULL)
		dns_peer_setrequestixfr(peer, cfg_obj_asboolean(obj));

	obj = NULL;
	(void)cfg_map_get(cpeer, "edns", &obj);
	if (obj != NULL)
		dns_peer_setsupportedns(peer, cfg_obj_asboolean(obj));

	obj = NULL;
	(void)cfg_map_get(cpeer, "transfers", &obj);
	if (obj != NULL)
		dns_peer_settransfers(peer, cfg_obj_asuint32(obj));

	obj = NULL;
	(void)cfg_map_get(cpeer, "transfer-format", &obj);
	if (obj != NULL) {
		str = cfg_obj_asstring(obj);
		if (strcasecmp(str, "many-answers") == 0)
			dns_peer_settransferformat(peer, dns_many_answers);
		else if (strcasecmp(str, "one-answer") == 0)
			dns_peer_settransferformat(peer, dns_one_answer);
		else
			INSIST(0);
	}

	obj = NULL;
	(void)cfg_map_get(cpeer, "keys", &obj);
	if (obj != NULL) {
		result = dns_peer_setkeybycharp(peer, cfg_obj_asstring(obj));
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}
	*peerp = peer;
	return (ISC_R_SUCCESS);

 cleanup:
	dns_peer_detach(&peer);
	return (result);
}

/*
 * Configure 'view' according to 'vconfig', taking defaults from 'config'
 * where values are missing in 'vconfig'.
 *
 * When configuring the default view, 'vconfig' will be NULL and the
 * global defaults in 'config' used exclusively.
 */
static isc_result_t
configure_view(dns_view_t *view, cfg_obj_t *config, cfg_obj_t *vconfig,
	       isc_mem_t *mctx, ns_aclconfctx_t *actx)
{
	cfg_obj_t *maps[4];
	cfg_obj_t *cfgmaps[3];
	cfg_obj_t *options = NULL;
	cfg_obj_t *voptions = NULL;
	cfg_obj_t *forwardtype;
	cfg_obj_t *forwarders;
	cfg_obj_t *zonelist;
	cfg_obj_t *obj;
	cfg_listelt_t *element;
	in_port_t port;
	dns_cache_t *cache = NULL;
	isc_result_t result;
	isc_uint32_t max_cache_size;
	isc_uint32_t lame_ttl;
	dns_tsig_keyring_t *ring;
	dns_view_t *pview = NULL;	/* Production view */
	isc_mem_t *cmctx;
	dns_dispatch_t *dispatch4 = NULL;
	dns_dispatch_t *dispatch6 = NULL;
	isc_boolean_t reused_cache = ISC_FALSE;
	int i;
	char *str;

	REQUIRE(DNS_VIEW_VALID(view));

	cmctx = NULL;

	if (config != NULL)
		cfg_map_get(config, "options", &options);

	i = 0;
	if (vconfig != NULL) {
		voptions = cfg_tuple_get(vconfig, "options");
		maps[i++] = voptions;
	}
	if (options != NULL)
		maps[i++] = options;
	maps[i++] = ns_g_defaults;
	maps[i] = NULL;

	i = 0;
	if (voptions != NULL)
		cfgmaps[i++] = voptions;
	if (config != NULL)
		cfgmaps[i++] = config;
	cfgmaps[i] = NULL;


	/*
	 * Set the view's port number for outgoing queries.
	 */
	CHECKM(ns_config_getport(config, &port), "port");
	dns_view_setdstport(view, port);

	/*
	 * Configure the zones.
	 */
	zonelist = NULL;
	if (voptions != NULL)
		(void)cfg_map_get(voptions, "zone", &zonelist);
	else
		(void)cfg_map_get(config, "zone", &zonelist);
	for (element = cfg_list_first(zonelist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		cfg_obj_t *zconfig = cfg_listelt_value(element);
		CHECK(configure_zone(config, zconfig, vconfig, mctx, view,
				     actx));
	}

	/*
	 * Configure the view's cache.  Try to reuse an existing
	 * cache if possible, otherwise create a new cache.
	 * Note that the ADB is not preserved in either case.
	 *
	 * XXX Determining when it is safe to reuse a cache is
	 * tricky.  When the view's configuration changes, the cached
	 * data may become invalid because it reflects our old
	 * view of the world.  As more view attributes become
	 * configurable, we will have to add code here to check
	 * whether they have changed in ways that could
	 * invalidate the cache.
	 */
	result = dns_viewlist_find(&ns_g_server->viewlist,
				   view->name, view->rdclass,
				   &pview);
	if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
		goto cleanup;
	if (pview != NULL) {
		INSIST(pview->cache != NULL);
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_DEBUG(3),
			      "reusing existing cache");
		reused_cache = ISC_TRUE;
		dns_cache_attach(pview->cache, &cache);
		dns_view_detach(&pview);
	} else {
		CHECK(isc_mem_create(0, 0, &cmctx));
		CHECK(dns_cache_create(cmctx, ns_g_taskmgr, ns_g_timermgr,
				       view->rdclass, "rbt", 0, NULL, &cache));
	}
	dns_view_setcache(view, cache);

	/*
	 * cache-file cannot be inherited if views are present, but this
	 * should be caught by the configuration checking stage.
	 */
	obj = NULL;
	result = ns_config_get(maps, "cache-file", &obj);
	if (result == ISC_R_SUCCESS) {
		dns_cache_setfilename(cache, cfg_obj_asstring(obj));
		if (!reused_cache)
			CHECK(dns_cache_load(cache));
	}

	obj = NULL;
	result = ns_config_get(maps, "cleaning-interval", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_cache_setcleaninginterval(cache, cfg_obj_asuint32(obj) * 60);

	obj = NULL;
	result = ns_config_get(maps, "max-cache-size", &obj);
	INSIST(result == ISC_R_SUCCESS);
	if (cfg_obj_isstring(obj)) {
		str = cfg_obj_asstring(obj);
		INSIST(strcasecmp(str, "unlimited") == 0);
		max_cache_size = ISC_UINT32_MAX;
	} else {
		isc_resourcevalue_t value;
		value = cfg_obj_asuint64(obj);
		if (value > ISC_UINT32_MAX) {
			cfg_obj_log(obj, ns_g_lctx, ISC_LOG_ERROR,
				    "'max-cache-size "
				    "%" ISC_PRINT_QUADFORMAT "d' is too large",
				    value);
			result = ISC_R_RANGE;
			goto cleanup;
		}
		max_cache_size = (isc_uint32_t)value;
	}
	dns_cache_setcachesize(cache, max_cache_size);

	dns_cache_detach(&cache);

	/*
	 * Resolver.
	 *
	 * XXXRTH  Hardwired number of tasks.
	 */
	CHECK(get_view_querysource_dispatch(maps, AF_INET, &dispatch4));
	CHECK(get_view_querysource_dispatch(maps, AF_INET6, &dispatch6));
	CHECK(dns_view_createresolver(view, ns_g_taskmgr, 31,
				      ns_g_socketmgr, ns_g_timermgr,
				      0, ns_g_dispatchmgr,
				      dispatch4, dispatch6));
	if (dispatch4 != NULL)
		dns_dispatch_detach(&dispatch4);
	if (dispatch6 != NULL)
		dns_dispatch_detach(&dispatch6);

	/*
	 * Set resolver's lame-ttl.
	 */
	obj = NULL;
	result = ns_config_get(maps, "lame-ttl", &obj);
	INSIST(result == ISC_R_SUCCESS);
	lame_ttl = cfg_obj_asuint32(obj);
	if (lame_ttl > 1800)
		lame_ttl = 1800;
	dns_resolver_setlamettl(view->resolver, lame_ttl);
	
	/*
	 * A global or view "forwarders" option, if present,
	 * creates an entry for "." in the forwarding table.
	 */
	forwardtype = NULL;
	forwarders = NULL;
	(void)ns_config_get(maps, "forward", &forwardtype);
	(void)ns_config_get(maps, "forwarders", &forwarders);
	if (forwarders != NULL)
		CHECK(configure_forward(config, view, dns_rootname, 
					forwarders, forwardtype));

	/*
	 * We have default hints for class IN if we need them.
	 */
	if (view->rdclass == dns_rdataclass_in && view->hints == NULL)
		dns_view_sethints(view, ns_g_server->in_roothints);

	/*
	 * If we still have no hints, this is a non-IN view with no
	 * "hints zone" configured.  Issue a warning, except if this
	 * is a root server.  Root servers never need to consult 
	 * their hints, so it's no point requireing users to configure
	 * them.
	 */
	if (view->hints == NULL) {
		dns_zone_t *rootzone = NULL;
		dns_view_findzone(view, dns_rootname, &rootzone);
		if (rootzone != NULL) {
			dns_zone_detach(&rootzone);
		} else {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
				      "no root hints for view '%s'",
				      view->name);
		}
	}

	/*
	 * Configure the view's TSIG keys.
	 */
	ring = NULL;
	CHECK(ns_tsigkeyring_fromconfig(config, vconfig, view->mctx, &ring));
	dns_view_setkeyring(view, ring);

	/*
	 * Configure the view's peer list.
	 */
	{
		cfg_obj_t *peers = NULL;
		cfg_listelt_t *element;
		dns_peerlist_t *newpeers = NULL;

		(void)ns_config_get(cfgmaps, "server", &peers);
		CHECK(dns_peerlist_new(mctx, &newpeers));
		for (element = cfg_list_first(peers);
		     element != NULL;
		     element = cfg_list_next(element))
		{
			cfg_obj_t *cpeer = cfg_listelt_value(element);
			dns_peer_t *peer;

			CHECK(configure_peer(cpeer, mctx, &peer));
			dns_peerlist_addpeer(newpeers, peer);
			dns_peer_detach(&peer);
		}
		dns_peerlist_detach(&view->peers);
		view->peers = newpeers; /* Transfer ownership. */
	}

	/*
	 * Copy the aclenv object.
	 */
	dns_aclenv_copy(&view->aclenv, &ns_g_server->aclenv);

	/*
	 * Configure the "match-clients" and "match-destinations" ACL.
	 */
	CHECK(configure_view_acl(vconfig, config, "match-clients", actx,
				 ns_g_mctx, &view->matchclients));
	CHECK(configure_view_acl(vconfig, config, "match-destinations", actx,
				 ns_g_mctx, &view->matchdestinations));

	/*
	 * Configure the "match-recursive-only" option.
	 */
	obj = NULL;
	(void) ns_config_get(maps, "match-recursive-only", &obj);
	if (obj != NULL && cfg_obj_asboolean(obj))
		view->matchrecursiveonly = ISC_TRUE;
	else
		view->matchrecursiveonly = ISC_FALSE;

	/*
	 * Configure other configurable data.
	 */
	obj = NULL;
	result = ns_config_get(maps, "recursion", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->recursion = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "auth-nxdomain", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->auth_nxdomain = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "minimal-responses", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->minimalresponses = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "transfer-format", &obj);
	INSIST(result == ISC_R_SUCCESS);
	str = cfg_obj_asstring(obj);
	if (strcasecmp(str, "many-answers") == 0)
		view->transfer_format = dns_many_answers;
	else if (strcasecmp(str, "one-answer") == 0)
		view->transfer_format = dns_one_answer;
	else
		INSIST(0);
	
	/*
	 * Set sources where additional data and CNAME/DNAME
	 * targets for authoritative answers may be found.
	 */
	obj = NULL;
	result = ns_config_get(maps, "additional-from-auth", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->additionalfromauth = cfg_obj_asboolean(obj);
	if (view->recursion && ! view->additionalfromauth) {
		cfg_obj_log(obj, ns_g_lctx, ISC_LOG_WARNING,
			    "'additional-from-auth no' is only supported "
			    "with 'recursion no'");
		view->additionalfromauth = ISC_TRUE;
	}

	obj = NULL;
	result = ns_config_get(maps, "additional-from-cache", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->additionalfromcache = cfg_obj_asboolean(obj);
	if (view->recursion && ! view->additionalfromcache) {
		cfg_obj_log(obj, ns_g_lctx, ISC_LOG_WARNING,
			    "'additional-from-cache no' is only supported "
			    "with 'recursion no'");
		view->additionalfromcache = ISC_TRUE;
	}

	CHECK(configure_view_acl(vconfig, config, "allow-query",
				 actx, ns_g_mctx, &view->queryacl));

	CHECK(configure_view_acl(vconfig, config, "allow-recursion",
				 actx, ns_g_mctx, &view->recursionacl));

	CHECK(configure_view_acl(vconfig, config, "allow-v6-synthesis",
				 actx, ns_g_mctx, &view->v6synthesisacl));

	CHECK(configure_view_acl(vconfig, config, "sortlist",
				 actx, ns_g_mctx, &view->sortlist));

	obj = NULL;
	result = ns_config_get(maps, "request-ixfr", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->requestixfr = cfg_obj_asboolean(obj);

	obj = NULL;
	result = ns_config_get(maps, "provide-ixfr", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->provideixfr = cfg_obj_asboolean(obj);

	/*
	 * For now, there is only one kind of trusted keys, the
	 * "security roots".
	 */
	CHECK(configure_view_dnsseckeys(vconfig, config, mctx,
				  &view->secroots));

	obj = NULL;
	result = ns_config_get(maps, "max-cache-ttl", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->maxcachettl = cfg_obj_asuint32(obj);

	obj = NULL;
	result = ns_config_get(maps, "max-ncache-ttl", &obj);
	INSIST(result == ISC_R_SUCCESS);
	view->maxncachettl = cfg_obj_asuint32(obj);
	if (view->maxncachettl > 7 * 24 * 3600)
		view->maxncachettl = 7 * 24 * 3600;

	obj = NULL;
	result = ns_config_get(maps, "root-delegation-only", &obj);
	if (result == ISC_R_SUCCESS) {
		dns_view_setrootdelonly(view, ISC_TRUE);
		if (!cfg_obj_isvoid(obj)) {
			dns_fixedname_t fixed;
			dns_name_t *name;
			isc_buffer_t b;
			char *str;
			cfg_obj_t *exclude;

			dns_fixedname_init(&fixed);
			name = dns_fixedname_name(&fixed);
			for (element = cfg_list_first(obj);
			     element != NULL;
			     element = cfg_list_next(element)) {
				exclude = cfg_listelt_value(element);
				str = cfg_obj_asstring(exclude);
				isc_buffer_init(&b, str, strlen(str));
				isc_buffer_add(&b, strlen(str));
				CHECK(dns_name_fromtext(name, &b, dns_rootname,
							ISC_FALSE, NULL));
				CHECK(dns_view_excludedelegationonly(view,
								     name));
			}
		}
	} else
		dns_view_setrootdelonly(view, ISC_FALSE);

	result = ISC_R_SUCCESS;

 cleanup:
	if (cmctx != NULL)
		isc_mem_detach(&cmctx);

	if (cache != NULL)
		dns_cache_detach(&cache);

	return (result);
}

/*
 * Create the special view that handles queries under "bind. CH".
 */
static isc_result_t
create_bind_view(dns_view_t **viewp) {
	isc_result_t result;
	dns_view_t *view = NULL;

	REQUIRE(viewp != NULL && *viewp == NULL);

	CHECK(dns_view_create(ns_g_mctx, dns_rdataclass_ch, "_bind", &view));

	/* Transfer ownership. */
	*viewp = view;
	view = NULL;

	result = ISC_R_SUCCESS;

 cleanup:
	if (view != NULL)
		dns_view_detach(&view);

	return (result);
}

/*
 * Create the zone that handles queries for "version.bind. CH".   The
 * version string is returned either from the "version" configuration
 * option or the global defaults.
 */
static isc_result_t
create_version_zone(cfg_obj_t **maps, dns_zonemgr_t *zmgr, dns_view_t *view) {
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_zone_t *zone = NULL;
	dns_dbversion_t *dbver = NULL;
	dns_difftuple_t *tuple = NULL;
	dns_diff_t diff;
	char *versiontext;
	unsigned char buf[256];
	isc_region_t r;
	size_t len;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	static unsigned char origindata[] = "\007version\004bind";
	dns_name_t origin;
	cfg_obj_t *obj = NULL;
	dns_acl_t *acl = NULL;

	dns_diff_init(ns_g_mctx, &diff);

	dns_name_init(&origin, NULL);
	r.base = origindata;
	r.length = sizeof(origindata);
	dns_name_fromregion(&origin, &r);

	result = ns_config_get(maps, "version", &obj);
	INSIST(result == ISC_R_SUCCESS);
	versiontext = cfg_obj_asstring(obj);
	len = strlen(versiontext);
	if (len > 255U)
		len = 255; /* Silently truncate. */
	buf[0] = len;
	memcpy(buf + 1, versiontext, len);

	r.base = buf;
	r.length = 1 + len;
	dns_rdata_fromregion(&rdata, dns_rdataclass_ch, dns_rdatatype_txt, &r);

	CHECK(dns_zone_create(&zone, ns_g_mctx));
	CHECK(dns_zone_setorigin(zone, &origin));
	dns_zone_settype(zone, dns_zone_master);
	dns_zone_setclass(zone, dns_rdataclass_ch);
	/* Transfers don't work so deny them. */
	CHECK(dns_acl_none(ns_g_mctx, &acl));
	dns_zone_setxfracl(zone, acl);
	dns_acl_detach(&acl);
	dns_zone_setview(zone, view);

	CHECK(dns_zonemgr_managezone(zmgr, zone));

	CHECK(dns_db_create(ns_g_mctx, "rbt", &origin, dns_dbtype_zone,
			    dns_rdataclass_ch, 0, NULL, &db));

	CHECK(dns_db_newversion(db, &dbver));

	CHECK(dns_difftuple_create(ns_g_mctx, DNS_DIFFOP_ADD, &origin,
				   0, &rdata, &tuple));
	dns_diff_append(&diff, &tuple);
	CHECK(dns_diff_apply(&diff, db, dbver));

	dns_db_closeversion(db, &dbver, ISC_TRUE);

	CHECK(dns_zone_replacedb(zone, db, ISC_FALSE));

	CHECK(dns_view_addzone(view, zone));
			
	result = ISC_R_SUCCESS;

 cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (dbver != NULL)
		dns_db_closeversion(db, &dbver, ISC_FALSE);
	if (db != NULL)
		dns_db_detach(&db);
	dns_diff_clear(&diff);

	return (result);
}

/*
 * Create the special zone that handles queries for "authors.bind. CH".
 * The strings returned list the BIND 9 authors.
 */
static isc_result_t
create_authors_zone(cfg_obj_t *options, dns_zonemgr_t *zmgr, dns_view_t *view)
{
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_zone_t *zone = NULL;
	dns_dbversion_t *dbver = NULL;
	dns_difftuple_t *tuple;
	dns_diff_t diff;
	isc_constregion_t r;
	isc_constregion_t cr;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	static const char origindata[] = "\007authors\004bind";
	dns_name_t origin;
	int i;
	static const char *authors[] = {
		"\014Mark Andrews",
		"\015James Brister",
		"\014Ben Cottrell",
		"\015Michael Graff",
		"\022Andreas Gustafsson",
		"\012Bob Halley",
		"\016David Lawrence",
		"\013Danny Mayer",
		"\013Damien Neil",
		"\013Matt Nelson",
		"\016Michael Sawyer",
		"\020Brian Wellington",
		NULL,
	};
	cfg_obj_t *obj = NULL;
	dns_acl_t *acl = NULL;

	/*
	 * If a version string is specified, disable the authors.bind zone.
	 */
	if (options != NULL &&
	    cfg_map_get(options, "version", &obj) == ISC_R_SUCCESS)
		return (ISC_R_SUCCESS);

	dns_diff_init(ns_g_mctx, &diff);

	dns_name_init(&origin, NULL);
	r.base = origindata;
	r.length = sizeof(origindata);
	dns_name_fromregion(&origin, (isc_region_t *)&r);

	CHECK(dns_zone_create(&zone, ns_g_mctx));
	CHECK(dns_zone_setorigin(zone, &origin));
	dns_zone_settype(zone, dns_zone_master);
	dns_zone_setclass(zone, dns_rdataclass_ch);
	/* Transfers don't work so deny them. */
	CHECK(dns_acl_none(ns_g_mctx, &acl));
	dns_zone_setxfracl(zone, acl);
	dns_acl_detach(&acl);
	dns_zone_setview(zone, view);

	CHECK(dns_zonemgr_managezone(zmgr, zone));

	CHECK(dns_db_create(ns_g_mctx, "rbt", &origin, dns_dbtype_zone,
			    dns_rdataclass_ch, 0, NULL, &db));

	CHECK(dns_db_newversion(db, &dbver));

	for (i = 0; authors[i] != NULL; i++) {
		cr.base = authors[i];
		cr.length = strlen(authors[i]);
		INSIST(cr.length == ((const unsigned char *)cr.base)[0] + 1U);
		dns_rdata_fromregion(&rdata, dns_rdataclass_ch,
				     dns_rdatatype_txt, (isc_region_t *)&cr);
		tuple = NULL;
		CHECK(dns_difftuple_create(ns_g_mctx, DNS_DIFFOP_ADD, &origin,
					   0, &rdata, &tuple));
		dns_diff_append(&diff, &tuple);
		dns_rdata_reset(&rdata);
	}

	CHECK(dns_diff_apply(&diff, db, dbver));

	dns_db_closeversion(db, &dbver, ISC_TRUE);

	CHECK(dns_zone_replacedb(zone, db, ISC_FALSE));

	CHECK(dns_view_addzone(view, zone));

	result = ISC_R_SUCCESS;

 cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (dbver != NULL)
		dns_db_closeversion(db, &dbver, ISC_FALSE);
	if (db != NULL)
		dns_db_detach(&db);
	dns_diff_clear(&diff);

	return (result);
}

static isc_result_t
configure_hints(dns_view_t *view, const char *filename) {
	isc_result_t result;
	dns_db_t *db;

	db = NULL;
	result = dns_rootns_create(view->mctx, view->rdclass, filename, &db);
	if (result == ISC_R_SUCCESS) {
		dns_view_sethints(view, db);
		dns_db_detach(&db);
	}

	return (result);
}

static isc_result_t
configure_forward(cfg_obj_t *config, dns_view_t *view, dns_name_t *origin,
		  cfg_obj_t *forwarders, cfg_obj_t *forwardtype)
{
	cfg_obj_t *portobj;
	cfg_obj_t *faddresses;
	cfg_listelt_t *element;
	dns_fwdpolicy_t fwdpolicy = dns_fwdpolicy_none;
	isc_sockaddrlist_t addresses;
	isc_sockaddr_t *sa;
	isc_result_t result;
	in_port_t port;

	/*
	 * Determine which port to send forwarded requests to.
	 */
	if (ns_g_lwresdonly && ns_g_port != 0)
		port = ns_g_port;
	else
		CHECKM(ns_config_getport(config, &port), "port");

	if (forwarders != NULL) {
		portobj = cfg_tuple_get(forwarders, "port");
		if (cfg_obj_isuint32(portobj)) {
			isc_uint32_t val = cfg_obj_asuint32(portobj);
			if (val > ISC_UINT16_MAX) {
				cfg_obj_log(portobj, ns_g_lctx, ISC_LOG_ERROR,
					    "port '%u' out of range", val);
				return (ISC_R_RANGE);
			}
			port = (in_port_t) val;
		}
	}

	faddresses = NULL;
	if (forwarders != NULL)
		faddresses = cfg_tuple_get(forwarders, "addresses");

	ISC_LIST_INIT(addresses);

	for (element = cfg_list_first(faddresses);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		cfg_obj_t *forwarder = cfg_listelt_value(element);
		sa = isc_mem_get(view->mctx, sizeof(isc_sockaddr_t));
		if (sa == NULL) {
			result = ISC_R_NOMEMORY;
			goto cleanup;
		}
		*sa = *cfg_obj_assockaddr(forwarder);
		if (isc_sockaddr_getport(sa) == 0)
			isc_sockaddr_setport(sa, port);
		ISC_LINK_INIT(sa, link);
		ISC_LIST_APPEND(addresses, sa, link);
	}

	if (ISC_LIST_EMPTY(addresses)) {
		if (forwardtype != NULL)
			cfg_obj_log(forwarders, ns_g_lctx, ISC_LOG_WARNING,
				    "no forwarders seen; disabling "
				    "forwarding");
		fwdpolicy = dns_fwdpolicy_none;
	} else {
		if (forwardtype == NULL)
			fwdpolicy = dns_fwdpolicy_first;
		else {
			char *forwardstr = cfg_obj_asstring(forwardtype);
			if (strcasecmp(forwardstr, "first") == 0)
				fwdpolicy = dns_fwdpolicy_first;
			else if (strcasecmp(forwardstr, "only") == 0)
				fwdpolicy = dns_fwdpolicy_only;
			else
				INSIST(0);
		}
	}

	result = dns_fwdtable_add(view->fwdtable, origin, &addresses,
				  fwdpolicy);
	if (result != ISC_R_SUCCESS) {
		char namebuf[DNS_NAME_FORMATSIZE];
		dns_name_format(origin, namebuf, sizeof(namebuf));
		cfg_obj_log(forwarders, ns_g_lctx, ISC_LOG_WARNING,
			    "could not set up forwarding for domain '%s': %s",
			    namebuf, isc_result_totext(result));
		goto cleanup;
	}

	result = ISC_R_SUCCESS;

 cleanup:

	while (!ISC_LIST_EMPTY(addresses)) {
		sa = ISC_LIST_HEAD(addresses);
		ISC_LIST_UNLINK(addresses, sa, link);
		isc_mem_put(view->mctx, sa, sizeof(isc_sockaddr_t));
	}

	return (result);
}

/*
 * Create a new view and add it to the list.
 *
 * If 'vconfig' is NULL, create the default view.
 *
 * The view created is attached to '*viewp'.
 */
static isc_result_t
create_view(cfg_obj_t *vconfig, dns_viewlist_t *viewlist, dns_view_t **viewp) {
	isc_result_t result;
	const char *viewname;
	dns_rdataclass_t viewclass;
	dns_view_t *view = NULL;

	if (vconfig != NULL) {
		cfg_obj_t *classobj = NULL;

		viewname = cfg_obj_asstring(cfg_tuple_get(vconfig, "name"));
		classobj = cfg_tuple_get(vconfig, "class");
		result = ns_config_getclass(classobj, dns_rdataclass_in,
					    &viewclass);
	} else {
		viewname = "_default";
		viewclass = dns_rdataclass_in;
	}
	result = dns_viewlist_find(viewlist, viewname, viewclass, &view);
	if (result == ISC_R_SUCCESS)
		return (ISC_R_EXISTS);
	if (result != ISC_R_NOTFOUND)
		return (result);
	INSIST(view == NULL);

	result = dns_view_create(ns_g_mctx, viewclass, viewname, &view);
	if (result != ISC_R_SUCCESS)
		return (result);

	ISC_LIST_APPEND(*viewlist, view, link);
	dns_view_attach(view, viewp);
	return (ISC_R_SUCCESS);
}

/*
 * Configure or reconfigure a zone.
 */
static isc_result_t
configure_zone(cfg_obj_t *config, cfg_obj_t *zconfig, cfg_obj_t *vconfig,
	       isc_mem_t *mctx, dns_view_t *view,
	       ns_aclconfctx_t *aclconf)
{
	dns_view_t *pview = NULL;	/* Production view */
	dns_zone_t *zone = NULL;	/* New or reused zone */
	dns_zone_t *dupzone = NULL;
	cfg_obj_t *options = NULL;
	cfg_obj_t *zoptions = NULL;
	cfg_obj_t *typeobj = NULL;
	cfg_obj_t *forwarders = NULL;
	cfg_obj_t *forwardtype = NULL;
	cfg_obj_t *only = NULL;
	isc_result_t result;
	isc_result_t tresult;
	isc_buffer_t buffer;
	dns_fixedname_t fixorigin;
	dns_name_t *origin;
	const char *zname;
	dns_rdataclass_t zclass;
	const char *ztypestr;

	options = NULL;
	(void)cfg_map_get(config, "options", &options);

	zoptions = cfg_tuple_get(zconfig, "options");

	/*
	 * Get the zone origin as a dns_name_t.
	 */
	zname = cfg_obj_asstring(cfg_tuple_get(zconfig, "name"));
	isc_buffer_init(&buffer, zname, strlen(zname));
	isc_buffer_add(&buffer, strlen(zname));
	dns_fixedname_init(&fixorigin);
	CHECK(dns_name_fromtext(dns_fixedname_name(&fixorigin),
				&buffer, dns_rootname, ISC_FALSE, NULL));
	origin = dns_fixedname_name(&fixorigin);

	CHECK(ns_config_getclass(cfg_tuple_get(zconfig, "class"),
				 view->rdclass, &zclass));
	if (zclass != view->rdclass) {
		const char *vname = NULL;
		if (vconfig != NULL)
			vname = cfg_obj_asstring(cfg_tuple_get(vconfig,
							       "name"));
		else
			vname = "<default view>";
	
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "zone '%s': wrong class for view '%s'",
			      zname, vname);
		result = ISC_R_FAILURE;
		goto cleanup;
	}

	(void)cfg_map_get(zoptions, "type", &typeobj);
	if (typeobj == NULL) {
		cfg_obj_log(zconfig, ns_g_lctx, ISC_LOG_ERROR,
			    "zone '%s' 'type' not specified", zname);
		return (ISC_R_FAILURE);
	}
	ztypestr = cfg_obj_asstring(typeobj);

	/*
	 * "hints zones" aren't zones.  If we've got one,
	 * configure it and return.
	 */
	if (strcasecmp(ztypestr, "hint") == 0) {
		cfg_obj_t *fileobj = NULL;
		if (cfg_map_get(zoptions, "file", &fileobj) != ISC_R_SUCCESS) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
				      "zone '%s': 'file' not specified",
				      zname);
			result = ISC_R_FAILURE;
			goto cleanup;
		}
		if (dns_name_equal(origin, dns_rootname)) {
			char *hintsfile = cfg_obj_asstring(fileobj);

			result = configure_hints(view, hintsfile);
			if (result != ISC_R_SUCCESS) {
				isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_SERVER,
					      ISC_LOG_ERROR,
					      "could not configure root hints "
					      "from '%s': %s", hintsfile,
					      isc_result_totext(result));
				goto cleanup;
			}
			/*
			 * Hint zones may also refer to delegation only points.
			 */
			only = NULL;
			tresult = cfg_map_get(zoptions, "delegation-only",
					      &only);
			if (tresult == ISC_R_SUCCESS && cfg_obj_asboolean(only))
				CHECK(dns_view_adddelegationonly(view, origin));
		} else {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_WARNING,
				      "ignoring non-root hint zone '%s'",
				      zname);
			result = ISC_R_SUCCESS;
		}
		/* Skip ordinary zone processing. */
		goto cleanup;
	}

	/*
	 * "forward zones" aren't zones either.  Translate this syntax into
	 * the appropriate selective forwarding configuration and return.
	 */
	if (strcasecmp(ztypestr, "forward") == 0) {
		forwardtype = NULL;
		forwarders = NULL;

		(void)cfg_map_get(zoptions, "forward", &forwardtype);
		(void)cfg_map_get(zoptions, "forwarders", &forwarders);
		result = configure_forward(config, view, origin, forwarders,
					   forwardtype);
		goto cleanup;
	}

	/*
	 * "delegation-only zones" aren't zones either.
	 */
	if (strcasecmp(ztypestr, "delegation-only") == 0) {
		result = dns_view_adddelegationonly(view, origin);
		goto cleanup;
	}

	/*
	 * Check for duplicates in the new zone table.
	 */
	result = dns_view_findzone(view, origin, &dupzone);
	if (result == ISC_R_SUCCESS) {
		/*
		 * We already have this zone!
		 */
		dns_zone_detach(&dupzone);
		result = ISC_R_EXISTS;
		goto cleanup;
	}
	INSIST(dupzone == NULL);

	/*
	 * See if we can reuse an existing zone.  This is
	 * only possible if all of these are true:
	 *   - The zone's view exists
	 *   - A zone with the right name exists in the view
	 *   - The zone is compatible with the config
	 *     options (e.g., an existing master zone cannot
	 *     be reused if the options specify a slave zone)
	 */
	result = dns_viewlist_find(&ns_g_server->viewlist,
				   view->name, view->rdclass,
				   &pview);
	if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
		goto cleanup;
	if (pview != NULL)
		result = dns_view_findzone(pview, origin, &zone);
	if (result != ISC_R_NOTFOUND && result != ISC_R_SUCCESS)
		goto cleanup;
	if (zone != NULL) {
		if (! ns_zone_reusable(zone, zconfig))
			dns_zone_detach(&zone);
	}

	if (zone != NULL) {
		/*
		 * We found a reusable zone.  Make it use the
		 * new view.
		 */
		dns_zone_setview(zone, view);
	} else {
		/*
		 * We cannot reuse an existing zone, we have
		 * to create a new one.
		 */
		CHECK(dns_zone_create(&zone, mctx));
		CHECK(dns_zone_setorigin(zone, origin));
		dns_zone_setview(zone, view);
		CHECK(dns_zonemgr_managezone(ns_g_server->zonemgr, zone));
	}

	/*
	 * If the zone contains a 'forwarders' statement, configure
	 * selective forwarding.
	 */
	forwarders = NULL;
	if (cfg_map_get(zoptions, "forwarders", &forwarders) == ISC_R_SUCCESS)
	{
		forwardtype = NULL;
		cfg_map_get(zoptions, "forward", &forwardtype);
		CHECK(configure_forward(config, view, origin, forwarders,
					forwardtype));
	}

	/*
	 * Stub and forward zones may also refer to delegation only points.
	 */
	only = NULL;
	if (cfg_map_get(zoptions, "delegation-only", &only) == ISC_R_SUCCESS)
	{
		if (cfg_obj_asboolean(only))
			CHECK(dns_view_adddelegationonly(view, origin));
	}

	/*
	 * Configure the zone.
	 */
	CHECK(ns_zone_configure(config, vconfig, zconfig, aclconf, zone));

	/*
	 * Add the zone to its view in the new view list.
	 */
	CHECK(dns_view_addzone(view, zone));

 cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	if (pview != NULL)
		dns_view_detach(&pview);

	return (result);
}

/*
 * Configure a single server quota.
 */
static void
configure_server_quota(cfg_obj_t **maps, const char *name, isc_quota_t *quota)
{
	cfg_obj_t *obj = NULL;
	isc_result_t result;

	result = ns_config_get(maps, name, &obj);
	INSIST(result == ISC_R_SUCCESS);
	quota->max = cfg_obj_asuint32(obj);
}

/*
 * This function is called as soon as the 'directory' statement has been
 * parsed.  This can be extended to support other options if necessary.
 */
static isc_result_t
directory_callback(const char *clausename, cfg_obj_t *obj, void *arg) {
	isc_result_t result;
	char *directory;

	REQUIRE(strcasecmp("directory", clausename) == 0);

	UNUSED(arg);
	UNUSED(clausename);

	/*
	 * Change directory.
	 */
	directory = cfg_obj_asstring(obj);

	if (! isc_file_ischdiridempotent(directory))
		cfg_obj_log(obj, ns_g_lctx, ISC_LOG_WARNING,
			    "option 'directory' contains relative path '%s'",
			    directory);

	result = isc_dir_chdir(directory);
	if (result != ISC_R_SUCCESS) {
		cfg_obj_log(obj, ns_g_lctx, ISC_LOG_ERROR,
			    "change directory to '%s' failed: %s",
			    directory, isc_result_totext(result));
		return (result);
	}

	return (ISC_R_SUCCESS);
}

static void
scan_interfaces(ns_server_t *server, isc_boolean_t verbose) {
	isc_boolean_t match_mapped = server->aclenv.match_mapped;

	ns_interfacemgr_scan(server->interfacemgr, verbose);
	/*
	 * Update the "localhost" and "localnets" ACLs to match the
	 * current set of network interfaces.
	 */
	dns_aclenv_copy(&server->aclenv,
			ns_interfacemgr_getaclenv(server->interfacemgr));

	server->aclenv.match_mapped = match_mapped;
}

/*
 * This event callback is invoked to do periodic network
 * interface scanning.
 */
static void
interface_timer_tick(isc_task_t *task, isc_event_t *event) {
        isc_result_t result;
	ns_server_t *server = (ns_server_t *) event->ev_arg;
	INSIST(task == server->task);
	UNUSED(task);
	isc_event_free(&event);
	/*
	 * XXX should scan interfaces unlocked and get exclusive access
	 * only to replace ACLs.
	 */
	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	scan_interfaces(server, ISC_FALSE);
	isc_task_endexclusive(server->task);	
}

static void
heartbeat_timer_tick(isc_task_t *task, isc_event_t *event) {
	ns_server_t *server = (ns_server_t *) event->ev_arg;
	dns_view_t *view;

	UNUSED(task);
	isc_event_free(&event);
	view = ISC_LIST_HEAD(server->viewlist);
	while (view != NULL) {
		dns_view_dialup(view);
		view = ISC_LIST_NEXT(view, link);
	}
}

static isc_result_t
setstatsfile(ns_server_t *server, const char *name) {
	char *p;

	REQUIRE(name != NULL);

	p = isc_mem_strdup(server->mctx, name);
	if (p == NULL)
		return (ISC_R_NOMEMORY);
	if (server->statsfile != NULL)
		isc_mem_free(server->mctx, server->statsfile);
	server->statsfile = p;
	return (ISC_R_SUCCESS);
}

static isc_result_t
setdumpfile(ns_server_t *server, const char *name) {
	char *p;

	REQUIRE(name != NULL);

	p = isc_mem_strdup(server->mctx, name);
	if (p == NULL)
		return (ISC_R_NOMEMORY);
	if (server->dumpfile != NULL)
		isc_mem_free(server->mctx, server->dumpfile);
	server->dumpfile = p;
	return (ISC_R_SUCCESS);
}

static void
set_limit(cfg_obj_t **maps, const char *configname, const char *description,
	  isc_resource_t resourceid, isc_resourcevalue_t defaultvalue)
{
	cfg_obj_t *obj = NULL;
	char *resource;
	isc_resourcevalue_t value;
	isc_result_t result;

	if (ns_config_get(maps, configname, &obj) != ISC_R_SUCCESS)
		return;

	if (cfg_obj_isstring(obj)) {
		resource = cfg_obj_asstring(obj);
		if (strcasecmp(resource, "unlimited") == 0)
			value = ISC_RESOURCE_UNLIMITED;
		else {
			INSIST(strcasecmp(resource, "default") == 0);
			value = defaultvalue;
		}
	} else
		value = cfg_obj_asuint64(obj);

	result = isc_resource_setlimit(resourceid, value);
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      result == ISC_R_SUCCESS ?
		      	ISC_LOG_DEBUG(3) : ISC_LOG_WARNING,
		      "set maximum %s to %" ISC_PRINT_QUADFORMAT "d: %s",
		      description, value, isc_result_totext(result));
}

#define SETLIMIT(cfgvar, resource, description) \
	set_limit(maps, cfgvar, description, isc_resource_ ## resource, \
		  ns_g_init ## resource)

static void
set_limits(cfg_obj_t **maps) {
	SETLIMIT("stacksize", stacksize, "stack size");
	SETLIMIT("datasize", datasize, "data size");
	SETLIMIT("coresize", coresize, "core size");
	SETLIMIT("files", openfiles, "open files");
}

static isc_result_t
load_configuration(const char *filename, ns_server_t *server,
		   isc_boolean_t first_time)
{
	isc_result_t result;
	cfg_parser_t *parser = NULL;
	cfg_obj_t *config;
	cfg_obj_t *options;
	cfg_obj_t *views;
	cfg_obj_t *obj;
	cfg_obj_t *maps[3];
	cfg_listelt_t *element;
	dns_view_t *view = NULL;
	dns_view_t *view_next;
	dns_viewlist_t viewlist;
	dns_viewlist_t tmpviewlist;
	ns_aclconfctx_t aclconfctx;
	dns_dispatch_t *dispatchv4 = NULL;
	dns_dispatch_t *dispatchv6 = NULL;
	isc_uint32_t interface_interval;
	isc_uint32_t heartbeat_interval;
	in_port_t listen_port;
	int i;

	ns_aclconfctx_init(&aclconfctx);
	ISC_LIST_INIT(viewlist);

	/* Ensure exclusive access to configuration data. */
	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);	

	/*
	 * Parse the global default pseudo-config file.
	 */
	if (first_time) {
		CHECK(ns_config_parsedefaults(ns_g_parser, &ns_g_config));
		RUNTIME_CHECK(cfg_map_get(ns_g_config, "options",
					  &ns_g_defaults) ==
			      ISC_R_SUCCESS);
	}

	/*
	 * Parse the configuration file using the new config code.
	 */
	result = ISC_R_FAILURE;
	config = NULL;

	/*
	 * Unless this is lwresd with the -C option, parse the config file.
	 */
	if (!(ns_g_lwresdonly && lwresd_g_useresolvconf)) {
		isc_log_write(ns_g_lctx,
			      NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
			      ISC_LOG_INFO, "loading configuration from '%s'",
			      filename);
		CHECK(cfg_parser_create(ns_g_mctx, ns_g_lctx, &parser));
		cfg_parser_setcallback(parser, directory_callback, NULL);
		result = cfg_parse_file(parser, filename, &cfg_type_namedconf,
					&config);
	}

	/*
	 * If this is lwresd with the -C option, or lwresd with no -C or -c
	 * option where the above parsing failed, parse resolv.conf.
	 */
	if (ns_g_lwresdonly &&
            (lwresd_g_useresolvconf ||
	     (!ns_g_conffileset && result == ISC_R_FILENOTFOUND)))
	{
		isc_log_write(ns_g_lctx,
			      NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
			      ISC_LOG_INFO, "loading configuration from '%s'",
			      lwresd_g_resolvconffile);
		if (parser != NULL)
			cfg_parser_destroy(&parser);
		CHECK(cfg_parser_create(ns_g_mctx, ns_g_lctx, &parser));
		result = ns_lwresd_parseeresolvconf(ns_g_mctx, parser,
						    &config);
	}
	CHECK(result);

	/*
	 * Check the validity of the configuration.
	 */
	CHECK(cfg_check_namedconf(config, ns_g_lctx, ns_g_mctx));

	/*
	 * Fill in the maps array, used for resolving defaults.
	 */
	i = 0;
	options = NULL;
	result = cfg_map_get(config, "options", &options);
	if (result == ISC_R_SUCCESS)
		maps[i++] = options;
	maps[i++] = ns_g_defaults;
	maps[i++] = NULL;

	/*
	 * Set process limits, which (usually) needs to be done as root.
	 */
	set_limits(maps);

	/*
	 * Configure various server options.
	 */
	configure_server_quota(maps, "transfers-out", &server->xfroutquota);
	configure_server_quota(maps, "tcp-clients", &server->tcpquota);
	configure_server_quota(maps, "recursive-clients",
			       &server->recursionquota);

	CHECK(configure_view_acl(NULL, config, "blackhole", &aclconfctx,
				 ns_g_mctx, &server->blackholeacl));
	if (server->blackholeacl != NULL)
		dns_dispatchmgr_setblackhole(ns_g_dispatchmgr,
					     server->blackholeacl);

	obj = NULL;
	result = ns_config_get(maps, "match-mapped-addresses", &obj);
	INSIST(result == ISC_R_SUCCESS);
	server->aclenv.match_mapped = cfg_obj_asboolean(obj);

	/*
	 * Configure the zone manager.
	 */
	obj = NULL;
	result = ns_config_get(maps, "transfers-in", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_zonemgr_settransfersin(server->zonemgr, cfg_obj_asuint32(obj));

	obj = NULL;
	result = ns_config_get(maps, "transfers-per-ns", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_zonemgr_settransfersperns(server->zonemgr, cfg_obj_asuint32(obj));

	obj = NULL;
	result = ns_config_get(maps, "serial-query-rate", &obj);
	INSIST(result == ISC_R_SUCCESS);
	dns_zonemgr_setserialqueryrate(server->zonemgr, cfg_obj_asuint32(obj));

	/*
	 * Determine which port to use for listening for incoming connections.
	 */
	if (ns_g_port != 0)
		listen_port = ns_g_port;
	else
		CHECKM(ns_config_getport(config, &listen_port), "port");

	/*
	 * Configure the interface manager according to the "listen-on"
	 * statement.
	 */
	{
		cfg_obj_t *clistenon = NULL;
		ns_listenlist_t *listenon = NULL;

		clistenon = NULL;
		/*
		 * Even though listen-on is present in the default
		 * configuration, we can't use it here, since it isn't
		 * used if we're in lwresd mode.  This way is easier.
		 */
		if (options != NULL)
			(void)cfg_map_get(options, "listen-on", &clistenon);
		if (clistenon != NULL) {
			result = ns_listenlist_fromconfig(clistenon,
							  config,
							  &aclconfctx,
							  ns_g_mctx,
							  &listenon);
		} else if (!ns_g_lwresdonly) {
			/*
			 * Not specified, use default.
			 */
			CHECK(ns_listenlist_default(ns_g_mctx, listen_port,
						    ISC_TRUE, &listenon));
		}
		if (listenon != NULL) {
			ns_interfacemgr_setlistenon4(server->interfacemgr,
						     listenon);
			ns_listenlist_detach(&listenon);
		}
	}
	/*
	 * Ditto for IPv6.
	 */
	{
		cfg_obj_t *clistenon = NULL;
		ns_listenlist_t *listenon = NULL;

		if (options != NULL)
			(void)cfg_map_get(options, "listen-on-v6", &clistenon);
		if (clistenon != NULL) {
			result = ns_listenlist_fromconfig(clistenon,
							  config,
							  &aclconfctx,
							  ns_g_mctx,
							  &listenon);
		} else if (!ns_g_lwresdonly) {
			/*
			 * Not specified, use default.
			 */
			CHECK(ns_listenlist_default(ns_g_mctx, listen_port,
						    ISC_TRUE, &listenon));
		}
		if (listenon != NULL) {
			ns_interfacemgr_setlistenon6(server->interfacemgr,
						     listenon);
			ns_listenlist_detach(&listenon);
		}
	}

	/*
	 * Rescan the interface list to pick up changes in the
	 * listen-on option.  It's important that we do this before we try
	 * to configure the query source, since the dispatcher we use might
	 * be shared with an interface.
	 */
	scan_interfaces(server, ISC_TRUE);

	/*
	 * Arrange for further interface scanning to occur periodically
	 * as specified by the "interface-interval" option.
	 */
	obj = NULL;
	result = ns_config_get(maps, "interface-interval", &obj);
	INSIST(result == ISC_R_SUCCESS);
	interface_interval = cfg_obj_asuint32(obj) * 60;
	if (interface_interval == 0) {
		isc_timer_reset(server->interface_timer,
				isc_timertype_inactive,
				NULL, NULL, ISC_TRUE);
	} else if (server->interface_interval != interface_interval) {
		isc_interval_t interval;
		isc_interval_set(&interval, interface_interval, 0);
		isc_timer_reset(server->interface_timer, isc_timertype_ticker,
				NULL, &interval, ISC_FALSE);
	}
	server->interface_interval = interface_interval;

	/*
	 * Configure the dialup heartbeat timer.
	 */
	obj = NULL;
	result = ns_config_get(maps, "heartbeat-interval", &obj);
	INSIST(result == ISC_R_SUCCESS);
	heartbeat_interval = cfg_obj_asuint32(obj) * 60;
	if (heartbeat_interval == 0) {
		isc_timer_reset(server->heartbeat_timer,
				isc_timertype_inactive,
				NULL, NULL, ISC_TRUE);
	} else if (server->heartbeat_interval != heartbeat_interval) {
		isc_interval_t interval;
		isc_interval_set(&interval, heartbeat_interval, 0);
		isc_timer_reset(server->heartbeat_timer, isc_timertype_ticker,
				NULL, &interval, ISC_FALSE);
	}
	server->heartbeat_interval = heartbeat_interval;

	/*
	 * Configure and freeze all explicit views.  Explicit
	 * views that have zones were already created at parsing
	 * time, but views with no zones must be created here.
	 */
	views = NULL;
	(void)cfg_map_get(config, "view", &views);
	for (element = cfg_list_first(views);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		cfg_obj_t *vconfig;

		view = NULL;
		vconfig = cfg_listelt_value(element);
		CHECK(create_view(vconfig, &viewlist, &view));
		INSIST(view != NULL);
		CHECK(configure_view(view, config, vconfig,
				     ns_g_mctx, &aclconfctx));
		dns_view_freeze(view);
		dns_view_detach(&view);
	}

	/*
	 * Make sure we have a default view if and only if there
	 * were no explicit views.
	 */
	if (views == NULL) {
		/*
		 * No explicit views; there ought to be a default view.
		 * There may already be one created as a side effect
		 * of zone statements, or we may have to create one.
		 * In either case, we need to configure and freeze it.
		 */
		CHECK(create_view(NULL, &viewlist, &view));
		CHECK(configure_view(view, config, NULL, ns_g_mctx,
				     &aclconfctx));
		dns_view_freeze(view);
		dns_view_detach(&view);
	}

	/*
	 * Create (or recreate) the internal _bind view.
	 */
	CHECK(create_bind_view(&view));
	CHECK(configure_view_acl(NULL, config, "allow-query",
				 &aclconfctx, ns_g_mctx, &view->queryacl));
	ISC_LIST_APPEND(viewlist, view, link);
	CHECK(create_version_zone(maps, server->zonemgr, view));
	CHECK(create_authors_zone(options, server->zonemgr, view));
	dns_view_freeze(view);
	view = NULL;

	/*
	 * Swap our new view list with the production one.
	 */
	tmpviewlist = server->viewlist;
	server->viewlist = viewlist;
	viewlist = tmpviewlist;

	/*
	 * Load the TKEY information from the configuration.
	 */
	if (options != NULL) {
		dns_tkeyctx_t *t = NULL;
		CHECKM(ns_tkeyctx_fromconfig(options, ns_g_mctx, ns_g_entropy,
					     &t),
		       "configuring TKEY");
		if (server->tkeyctx != NULL)
			dns_tkeyctx_destroy(&server->tkeyctx);
		server->tkeyctx = t;
	}

	/*
	 * Bind the control port(s).
	 */
	CHECKM(ns_controls_configure(ns_g_server->controls, config,
				     &aclconfctx),
	       "binding control channel(s)");

	/*
	 * Bind the lwresd port(s).
	 */
	CHECKM(ns_lwresd_configure(ns_g_mctx, config),
	       "binding lightweight resolver ports");

	/*
	 * Open the source of entropy.
	 */
	if (first_time) {
		obj = NULL;
		result = ns_config_get(maps, "random-device", &obj);
		if (result != ISC_R_SUCCESS) {
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
				      "no source of entropy found");
		} else {
			const char *randomdev = cfg_obj_asstring(obj);
			result = isc_entropy_createfilesource(ns_g_entropy,
							      randomdev);
			if (result != ISC_R_SUCCESS && ns_g_chrootdir == NULL) {
				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_SERVER,
					      ISC_LOG_INFO,
					      "could not open entropy source "
					      "%s: %s",
					      randomdev,
					      isc_result_totext(result));
			}
#ifdef PATH_RANDOMDEV
			if (result != ISC_R_SUCCESS && ns_g_chrootdir != NULL) {
				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_SERVER,
					      ISC_LOG_INFO,
					      "using pre-chroot entropy source "
					      "%s",
					      PATH_RANDOMDEV);
		  		isc_entropy_detach(&ns_g_entropy);
				isc_entropy_attach(ns_g_fallbackentropy,
						   &ns_g_entropy);

			}
#endif
		}
	}

	/*
	 * Relinquish root privileges. Not used due to privsep
	 */
#if 0
	if (first_time)
		ns_os_changeuser();
#endif

	/*
	 * Configure the logging system.
	 *
	 * Do this after changing UID to make sure that any log
	 * files specified in named.conf get created by the
	 * unprivileged user, not root.
	 */
	if (ns_g_logstderr) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
			      "ignoring config file logging "
			      "statement due to -g option");
	} else {
		cfg_obj_t *logobj = NULL;
		isc_logconfig_t *logc = NULL;

		CHECKM(isc_logconfig_create(ns_g_lctx, &logc),
		       "creating new logging configuration");

		logobj = NULL;
		(void)cfg_map_get(config, "logging", &logobj);
		if (logobj != NULL) {
			CHECKM(ns_log_configure(logc, logobj),
			       "configuring logging");
		} else {
			CHECKM(ns_log_setdefaultchannels(logc),
			       "setting up default logging channels");
			CHECKM(ns_log_setunmatchedcategory(logc),
			       "setting up default 'category unmatched'");
			CHECKM(ns_log_setdefaultcategory(logc),
			       "setting up default 'category default'");
		}

		result = isc_logconfig_use(ns_g_lctx, logc);
		if (result != ISC_R_SUCCESS) {
			isc_logconfig_destroy(&logc);
			CHECKM(result, "installing logging configuration");
		}

		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_DEBUG(1),
			      "now using logging configuration from "
			      "config file");
	}

	/*
	 * Set the default value of the query logging flag depending
	 * whether a "queries" category has been defined.  This is
	 * a disgusting hack, but we need to do this for BIND 8
	 * compatibility.
	 */
	if (first_time) {
		cfg_obj_t *logobj = NULL;
		cfg_obj_t *categories = NULL;
		(void)cfg_map_get(config, "logging", &logobj);
		if (logobj != NULL)
			(void)cfg_map_get(logobj, "category", &categories);
		if (categories != NULL) {
			cfg_listelt_t *element;
			for (element = cfg_list_first(categories);
			     element != NULL;
			     element = cfg_list_next(element))
			{
				cfg_obj_t *catobj;
				char *str;

				obj = cfg_listelt_value(element);
				catobj = cfg_tuple_get(obj, "name");
				str = cfg_obj_asstring(catobj);
				if (strcasecmp(str, "queries") == 0)
					server->log_queries = ISC_TRUE;
			}
		}
	}

	if (ns_g_pidfile != NULL) {
		ns_os_writepidfile(ns_g_pidfile, first_time);
	} else {
		obj = NULL;
		if (ns_config_get(maps, "pid-file", &obj) == ISC_R_SUCCESS)
			ns_os_writepidfile(cfg_obj_asstring(obj), first_time);
		else if (ns_g_lwresdonly)
			ns_os_writepidfile(lwresd_g_defaultpidfile, first_time);
		else
			ns_os_writepidfile(ns_g_defaultpidfile, first_time);
	}

	obj = NULL;
	result = ns_config_get(maps, "statistics-file", &obj);
	INSIST(result == ISC_R_SUCCESS);
	CHECKM(setstatsfile(server, cfg_obj_asstring(obj)), "strdup");

	obj = NULL;
	result = ns_config_get(maps, "dump-file", &obj);
	INSIST(result == ISC_R_SUCCESS);
	CHECKM(setdumpfile(server, cfg_obj_asstring(obj)), "strdup");

 cleanup:
	ns_aclconfctx_destroy(&aclconfctx);

	if (parser != NULL) {
		if (config != NULL)
			cfg_obj_destroy(parser, &config);
		cfg_parser_destroy(&parser);
	}

	if (view != NULL)
		dns_view_detach(&view);

	/*
	 * This cleans up either the old production view list
	 * or our temporary list depending on whether they
	 * were swapped above or not.
	 */
	for (view = ISC_LIST_HEAD(viewlist);
	     view != NULL;
	     view = view_next) {
		view_next = ISC_LIST_NEXT(view, link);
		ISC_LIST_UNLINK(viewlist, view, link);
		dns_view_detach(&view);

	}

	if (dispatchv4 != NULL)
		dns_dispatch_detach(&dispatchv4);
	if (dispatchv6 != NULL)
		dns_dispatch_detach(&dispatchv6);

	/* Relinquish exclusive access to configuration data. */
	isc_task_endexclusive(server->task);

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_DEBUG(1), "load_configuration: %s",
		      isc_result_totext(result));

	return (result);
}

static isc_result_t
load_zones(ns_server_t *server, isc_boolean_t stop) {
	isc_result_t result;
	dns_view_t *view;

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	/*
	 * Load zone data from disk.
	 */
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		CHECK(dns_view_load(view, stop));
	}

	/*
	 * Force zone maintenance.  Do this after loading
	 * so that we know when we need to force AXFR of
	 * slave zones whose master files are missing.
	 */
	CHECK(dns_zonemgr_forcemaint(server->zonemgr));
 cleanup:
	isc_task_endexclusive(server->task);	
	return (result);
}

static isc_result_t
load_new_zones(ns_server_t *server, isc_boolean_t stop) {
	isc_result_t result;
	dns_view_t *view;

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	/*
	 * Load zone data from disk.
	 */
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		CHECK(dns_view_loadnew(view, stop));
	}
	/*
	 * Force zone maintenance.  Do this after loading
	 * so that we know when we need to force AXFR of
	 * slave zones whose master files are missing.
	 */
	CHECK(dns_zonemgr_forcemaint(server->zonemgr));
 cleanup:
	isc_task_endexclusive(server->task);	
	return (result);
}

static void
run_server(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	ns_server_t *server = (ns_server_t *)event->ev_arg;

	UNUSED(task);

	isc_event_free(&event);

	CHECKFATAL(dns_dispatchmgr_create(ns_g_mctx, ns_g_entropy,
					  &ns_g_dispatchmgr),
		   "creating dispatch manager");

	CHECKFATAL(ns_interfacemgr_create(ns_g_mctx, ns_g_taskmgr,
					  ns_g_socketmgr, ns_g_dispatchmgr,
					  &server->interfacemgr),
		   "creating interface manager");

	CHECKFATAL(isc_timer_create(ns_g_timermgr, isc_timertype_inactive,
				    NULL, NULL, server->task,
				    interface_timer_tick,
				    server, &server->interface_timer),
		   "creating interface timer");

	CHECKFATAL(isc_timer_create(ns_g_timermgr, isc_timertype_inactive,
				    NULL, NULL, server->task,
				    heartbeat_timer_tick,
				    server, &server->heartbeat_timer),
		   "creating heartbeat timer");

	CHECKFATAL(cfg_parser_create(ns_g_mctx, NULL, &ns_g_parser),
		   "creating default configuration parser");

	if (ns_g_lwresdonly)
		CHECKFATAL(load_configuration(lwresd_g_conffile, server,
					      ISC_TRUE),
			   "loading configuration");
	else
		CHECKFATAL(load_configuration(ns_g_conffile, server, ISC_TRUE),
			   "loading configuration");

	isc_hash_init();

	CHECKFATAL(load_zones(server, ISC_FALSE),
		   "loading zones");

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_INFO, "running");
}

void 
ns_server_flushonshutdown(ns_server_t *server, isc_boolean_t flush) {

	REQUIRE(NS_SERVER_VALID(server));

	server->flushonshutdown = flush;
}

static void
shutdown_server(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	dns_view_t *view, *view_next;
	ns_server_t *server = (ns_server_t *)event->ev_arg;
	isc_boolean_t flush = server->flushonshutdown;

	UNUSED(task);
	INSIST(task == server->task);

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_INFO, "shutting down%s",
		      flush ? ": flushing changes" : "");

	ns_controls_shutdown(server->controls);

	cfg_obj_destroy(ns_g_parser, &ns_g_config);
	cfg_parser_destroy(&ns_g_parser);

	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = view_next) {
		view_next = ISC_LIST_NEXT(view, link);
		ISC_LIST_UNLINK(server->viewlist, view, link);
		if (flush)
			dns_view_flushanddetach(&view);
		else
			dns_view_detach(&view);
	}

	isc_timer_detach(&server->interface_timer);
	isc_timer_detach(&server->heartbeat_timer);

	ns_interfacemgr_shutdown(server->interfacemgr);
	ns_interfacemgr_detach(&server->interfacemgr);

	dns_dispatchmgr_destroy(&ns_g_dispatchmgr);

	dns_zonemgr_shutdown(server->zonemgr);

	if (server->blackholeacl != NULL)
		dns_acl_detach(&server->blackholeacl);

	isc_task_endexclusive(server->task);

	isc_task_detach(&server->task);

	isc_event_free(&event);
}

void
ns_server_create(isc_mem_t *mctx, ns_server_t **serverp) {
	isc_result_t result;

	ns_server_t *server = isc_mem_get(mctx, sizeof(*server));
	if (server == NULL)
		fatal("allocating server object", ISC_R_NOMEMORY);

	server->mctx = mctx;
	server->task = NULL;

	/* Initialize configuration data with default values. */

	result = isc_quota_init(&server->xfroutquota, 10);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	result = isc_quota_init(&server->tcpquota, 10);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	result = isc_quota_init(&server->recursionquota, 100);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	result = dns_aclenv_init(mctx, &server->aclenv);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	/* Initialize server data structures. */
	server->zonemgr = NULL;
	server->interfacemgr = NULL;
	ISC_LIST_INIT(server->viewlist);
	server->in_roothints = NULL;
	server->blackholeacl = NULL;

	CHECKFATAL(dns_rootns_create(mctx, dns_rdataclass_in, NULL,
				     &server->in_roothints),
		   "setting up root hints");

	CHECKFATAL(isc_mutex_init(&server->reload_event_lock),
		   "initializing reload event lock");
	server->reload_event =
		isc_event_allocate(ns_g_mctx, server,
				   NS_EVENT_RELOAD,
				   ns_server_reload,
				   server,
				   sizeof(isc_event_t));
	CHECKFATAL(server->reload_event == NULL ?
		   ISC_R_NOMEMORY : ISC_R_SUCCESS,
		   "allocating reload event");

	CHECKFATAL(dst_lib_init(ns_g_mctx, ns_g_entropy, ISC_ENTROPY_GOODONLY),
		   "initializing DST");

	server->tkeyctx = NULL;
	CHECKFATAL(dns_tkeyctx_create(ns_g_mctx, ns_g_entropy,
				      &server->tkeyctx),
		   "creating TKEY context");

	/*
	 * Setup the server task, which is responsible for coordinating
	 * startup and shutdown of the server.
	 */
	CHECKFATAL(isc_task_create(ns_g_taskmgr, 0, &server->task),
		   "creating server task");
	isc_task_setname(server->task, "server", server);
	CHECKFATAL(isc_task_onshutdown(server->task, shutdown_server, server),
		   "isc_task_onshutdown");
	CHECKFATAL(isc_app_onrun(ns_g_mctx, server->task, run_server, server),
		   "isc_app_onrun");

	server->interface_timer = NULL;
	server->heartbeat_timer = NULL;
	
	server->interface_interval = 0;
	server->heartbeat_interval = 0;

	CHECKFATAL(dns_zonemgr_create(ns_g_mctx, ns_g_taskmgr, ns_g_timermgr,
				      ns_g_socketmgr, &server->zonemgr),
		   "dns_zonemgr_create");

	server->statsfile = isc_mem_strdup(server->mctx, "named.stats");
	CHECKFATAL(server->statsfile == NULL ? ISC_R_NOMEMORY : ISC_R_SUCCESS,
		   "isc_mem_strdup");
	server->querystats = NULL;
	CHECKFATAL(dns_stats_alloccounters(ns_g_mctx, &server->querystats),
		   "dns_stats_alloccounters");

	server->dumpfile = isc_mem_strdup(server->mctx, "named_dump.db");
	CHECKFATAL(server->dumpfile == NULL ? ISC_R_NOMEMORY : ISC_R_SUCCESS,
		   "isc_mem_strdup");

	server->flushonshutdown = ISC_FALSE;
	server->log_queries = ISC_FALSE;

	server->controls = NULL;
	CHECKFATAL(ns_controls_create(server, &server->controls),
		   "ns_controls_create");

	server->magic = NS_SERVER_MAGIC;
	*serverp = server;
}

void
ns_server_destroy(ns_server_t **serverp) {
	ns_server_t *server = *serverp;
	REQUIRE(NS_SERVER_VALID(server));

	ns_controls_destroy(&server->controls);

	dns_stats_freecounters(server->mctx, &server->querystats);
	isc_mem_free(server->mctx, server->statsfile);

	isc_mem_free(server->mctx, server->dumpfile);

	dns_zonemgr_detach(&server->zonemgr);

	if (server->tkeyctx != NULL)
		dns_tkeyctx_destroy(&server->tkeyctx);

	dst_lib_destroy();

	isc_event_free(&server->reload_event);

	INSIST(ISC_LIST_EMPTY(server->viewlist));

	dns_db_detach(&server->in_roothints);

	dns_aclenv_destroy(&server->aclenv);

	isc_quota_destroy(&server->recursionquota);
	isc_quota_destroy(&server->tcpquota);
	isc_quota_destroy(&server->xfroutquota);

	server->magic = 0;
	isc_mem_put(server->mctx, server, sizeof(*server));
	*serverp = NULL;
}

static void
fatal(const char *msg, isc_result_t result) {
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_CRITICAL, "%s: %s", msg,
		      isc_result_totext(result));
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL, NS_LOGMODULE_SERVER,
		      ISC_LOG_CRITICAL, "exiting (due to fatal error)");
	exit(1);
}

static isc_result_t
loadconfig(ns_server_t *server) {
	isc_result_t result;
	result = load_configuration(ns_g_lwresdonly ?
				    lwresd_g_conffile : ns_g_conffile,
				    server,
				    ISC_FALSE);
	if (result != ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "reloading configuration failed: %s",
			      isc_result_totext(result));
	return (result);
}

static void
reload(ns_server_t *server) {
	isc_result_t result;
	CHECK(loadconfig(server));

	result = load_zones(server, ISC_FALSE);
	if (result != ISC_R_SUCCESS) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "reloading zones failed: %s",
			      isc_result_totext(result));
	}
 cleanup: ;
}

static void
reconfig(ns_server_t *server) {
	isc_result_t result;
	CHECK(loadconfig(server));

	result = load_new_zones(server, ISC_FALSE);
	if (result != ISC_R_SUCCESS) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_SERVER, ISC_LOG_ERROR,
			      "loading new zones failed: %s",
			      isc_result_totext(result));
	}
 cleanup: ;
}

/*
 * Handle a reload event (from SIGHUP).
 */
static void
ns_server_reload(isc_task_t *task, isc_event_t *event) {
	ns_server_t *server = (ns_server_t *)event->ev_arg;

	INSIST(task = server->task);
	UNUSED(task);

	reload(server);

	LOCK(&server->reload_event_lock);
	INSIST(server->reload_event == NULL);
	server->reload_event = event;
	UNLOCK(&server->reload_event_lock);
}

void
ns_server_reloadwanted(ns_server_t *server) {
	LOCK(&server->reload_event_lock);
	if (server->reload_event != NULL)
		isc_task_send(server->task, &server->reload_event);
	UNLOCK(&server->reload_event_lock);
}

static char *
next_token(char **stringp, const char *delim) {
	char *res;

	do {
		res = strsep(stringp, delim);
		if (res == NULL)
			break;
	} while (*res == '\0');
	return (res);
}                       

/*
 * Find the zone specified in the control channel command 'args',
 * if any.  If a zone is specified, point '*zonep' at it, otherwise
 * set '*zonep' to NULL.
 */
static isc_result_t
zone_from_args(ns_server_t *server, char *args, dns_zone_t **zonep) {
	char *input, *ptr;
	const char *zonetxt;
	char *classtxt;
	const char *viewtxt = NULL;
	dns_fixedname_t name;
	isc_result_t result;
	isc_buffer_t buf;
	dns_view_t *view = NULL;
	dns_rdataclass_t rdclass;

	REQUIRE(zonep != NULL && *zonep == NULL);

	input = args;

	/* Skip the command name. */
	ptr = next_token(&input, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	/* Look for the zone name. */
	zonetxt = next_token(&input, " \t");
	if (zonetxt == NULL)
		return (ISC_R_SUCCESS);

	/* Look for the optional class name. */
	classtxt = next_token(&input, " \t");
	if (classtxt != NULL) {
		/* Look for the optional view name. */
		viewtxt = next_token(&input, " \t");
	}

	isc_buffer_init(&buf, zonetxt, strlen(zonetxt));
	isc_buffer_add(&buf, strlen(zonetxt));
	dns_fixedname_init(&name);
	result = dns_name_fromtext(dns_fixedname_name(&name),
				   &buf, dns_rootname, ISC_FALSE, NULL);
	if (result != ISC_R_SUCCESS)
		goto fail1;

	if (classtxt != NULL) {
		isc_textregion_t r;
		r.base = classtxt;
		r.length = strlen(classtxt);
		result = dns_rdataclass_fromtext(&rdclass, &r);
		if (result != ISC_R_SUCCESS)
			goto fail1;
	} else {
		rdclass = dns_rdataclass_in;
	}
	
	if (viewtxt == NULL)
		viewtxt = "_default";
	result = dns_viewlist_find(&server->viewlist, viewtxt,
				   rdclass, &view);
	if (result != ISC_R_SUCCESS)
		goto fail1;
	
	result = dns_zt_find(view->zonetable, dns_fixedname_name(&name),
			     0, NULL, zonep);
	/* Partial match? */
	if (result != ISC_R_SUCCESS && *zonep != NULL)
		dns_zone_detach(zonep);
	dns_view_detach(&view);
 fail1:
	return (result);
}

/*
 * Act on a "reload" command from the command channel.
 */
isc_result_t
ns_server_reloadcommand(ns_server_t *server, char *args) {
	isc_result_t result;
	dns_zone_t *zone = NULL;
	dns_zonetype_t type;
	
	result = zone_from_args(server, args, &zone);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL) {
		reload(server);
	} else {
		type = dns_zone_gettype(zone);
		if (type == dns_zone_slave || type == dns_zone_stub)
			dns_zone_refresh(zone);
		else
			dns_zone_load(zone);
		dns_zone_detach(&zone);
	}
	return (ISC_R_SUCCESS);
}	

/*
 * Act on a "reconfig" command from the command channel.
 */
isc_result_t
ns_server_reconfigcommand(ns_server_t *server, char *args) {
	UNUSED(args);

	reconfig(server);
	return (ISC_R_SUCCESS);
}

/*
 * Act on a "refresh" command from the command channel.
 */
isc_result_t
ns_server_refreshcommand(ns_server_t *server, char *args) {
	isc_result_t result;
	dns_zone_t *zone = NULL;

	result = zone_from_args(server, args, &zone);
	if (result != ISC_R_SUCCESS)
		return (result);
	if (zone == NULL)
		return (ISC_R_UNEXPECTEDEND);
	
	dns_zone_refresh(zone);
	dns_zone_detach(&zone);

	return (ISC_R_SUCCESS);
}	

isc_result_t
ns_server_togglequerylog(ns_server_t *server) {
	server->log_queries = server->log_queries ? ISC_FALSE : ISC_TRUE;
	
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_SERVER, ISC_LOG_INFO,
		      "query logging is now %s",
		      server->log_queries ? "on" : "off");
	return (ISC_R_SUCCESS);
}

static isc_result_t
ns_listenlist_fromconfig(cfg_obj_t *listenlist, cfg_obj_t *config,
			 ns_aclconfctx_t *actx,
			 isc_mem_t *mctx, ns_listenlist_t **target)
{
	isc_result_t result;
	cfg_listelt_t *element;
	ns_listenlist_t *dlist = NULL;

	REQUIRE(target != NULL && *target == NULL);

	result = ns_listenlist_create(mctx, &dlist);
	if (result != ISC_R_SUCCESS)
		return (result);

	for (element = cfg_list_first(listenlist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		ns_listenelt_t *delt = NULL;
		cfg_obj_t *listener = cfg_listelt_value(element);
		result = ns_listenelt_fromconfig(listener, config, actx,
						 mctx, &delt);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		ISC_LIST_APPEND(dlist->elts, delt, link);
	}
	*target = dlist;
	return (ISC_R_SUCCESS);

 cleanup:
	ns_listenlist_detach(&dlist);
	return (result);
}

/*
 * Create a listen list from the corresponding configuration
 * data structure.
 */
static isc_result_t
ns_listenelt_fromconfig(cfg_obj_t *listener, cfg_obj_t *config,
			ns_aclconfctx_t *actx,
			isc_mem_t *mctx, ns_listenelt_t **target)
{
	isc_result_t result;
	cfg_obj_t *portobj;
	in_port_t port;
	ns_listenelt_t *delt = NULL;
	REQUIRE(target != NULL && *target == NULL);

	portobj = cfg_tuple_get(listener, "port");
	if (!cfg_obj_isuint32(portobj)) {
		if (ns_g_port != 0) {
			port = ns_g_port;
		} else {
			result = ns_config_getport(config, &port);
			if (result != ISC_R_SUCCESS)
				return (result);
		}
	} else {
		if (cfg_obj_asuint32(portobj) >= ISC_UINT16_MAX) {
			cfg_obj_log(portobj, ns_g_lctx, ISC_LOG_ERROR,
				    "port value '%u' is out of range",
				    cfg_obj_asuint32(portobj));
			return (ISC_R_RANGE);
		}
		port = (in_port_t)cfg_obj_asuint32(portobj);
	}

	result = ns_listenelt_create(mctx, port, NULL, &delt);
	if (result != ISC_R_SUCCESS)
		return (result);

	result = ns_acl_fromconfig(cfg_tuple_get(listener, "acl"),
				   config, actx, mctx, &delt->acl);
	if (result != ISC_R_SUCCESS) {
		ns_listenelt_destroy(delt);
		return (result);
	}
	*target = delt;
	return (ISC_R_SUCCESS);
}

isc_result_t
ns_server_dumpstats(ns_server_t *server) {
	isc_result_t result;
	dns_zone_t *zone, *next;
	isc_stdtime_t now;
	FILE *fp = NULL;
	int i;
	int ncounters;

	isc_stdtime_get(&now);

	CHECKM(isc_stdio_open(server->statsfile, "a", &fp),
	       "could not open statistics dump file");
	
	ncounters = DNS_STATS_NCOUNTERS;
	fprintf(fp, "+++ Statistics Dump +++ (%lu)\n", (unsigned long)now);
	
	for (i = 0; i < ncounters; i++)
		fprintf(fp, "%s %" ISC_PRINT_QUADFORMAT "u\n",
			dns_statscounter_names[i],
			server->querystats[i]);
	
	zone = NULL;
	for (result = dns_zone_first(server->zonemgr, &zone);
	     result == ISC_R_SUCCESS;
	     next = NULL, result = dns_zone_next(zone, &next), zone = next)
	{
		isc_uint64_t *zonestats = dns_zone_getstatscounters(zone);
		if (zonestats != NULL) {
			char zonename[DNS_NAME_FORMATSIZE];
			dns_view_t *view;
			char *viewname;
			
			dns_name_format(dns_zone_getorigin(zone),
					zonename, sizeof(zonename));
			view = dns_zone_getview(zone);
			viewname = view->name;
			for (i = 0; i < ncounters; i++) {
				fprintf(fp, "%s %" ISC_PRINT_QUADFORMAT
					"u %s",
					dns_statscounter_names[i],
					zonestats[i],
					zonename);
				if (strcmp(viewname, "_default") != 0)
					fprintf(fp, " %s", viewname);
				fprintf(fp, "\n");
			}
		}
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
	CHECK(result);
	
	fprintf(fp, "--- Statistics Dump --- (%lu)\n", (unsigned long)now);

 cleanup:
	if (fp != NULL)
		(void)isc_stdio_close(fp);
	return (result);
}

isc_result_t
ns_server_dumpdb(ns_server_t *server) {
	FILE *fp = NULL;
	dns_view_t *view;
	isc_result_t result;

	CHECKM(isc_stdio_open(server->dumpfile, "w", &fp),
	       "could not open dump file");

	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		if (view->cachedb != NULL)
			CHECKM(dns_view_dumpdbtostream(view, fp),
			       "could not dump view databases");
	}
 cleanup:
	if (fp != NULL)
		(void)isc_stdio_close(fp);
	return (result);
}

isc_result_t
ns_server_setdebuglevel(ns_server_t *server, char *args) {
	char *ptr;
	char *levelstr;
	char *endp;
	long newlevel;

	UNUSED(server);

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	/* Look for the new level name. */
	levelstr = next_token(&args, " \t");
	if (levelstr == NULL) {
		if (ns_g_debuglevel < 99)
			ns_g_debuglevel++;
	} else {
		newlevel = strtol(levelstr, &endp, 10);
		if (*endp != '\0' || newlevel < 0 || newlevel > 99)
			return (ISC_R_RANGE);
		ns_g_debuglevel = (unsigned int)newlevel;
	}
	isc_log_setdebuglevel(ns_g_lctx, ns_g_debuglevel);
	return (ISC_R_SUCCESS);
}

isc_result_t
ns_server_flushcache(ns_server_t *server, char *args) {
	char *ptr, *viewname;
	dns_view_t *view;
	isc_boolean_t flushed = ISC_FALSE;
	isc_result_t result;

	/* Skip the command name. */
	ptr = next_token(&args, " \t");
	if (ptr == NULL)
		return (ISC_R_UNEXPECTEDEND);

	/* Look for the view name. */
	viewname = next_token(&args, " \t");

	result = isc_task_beginexclusive(server->task);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	for (view = ISC_LIST_HEAD(server->viewlist);
	     view != NULL;
	     view = ISC_LIST_NEXT(view, link))
	{
		if (viewname != NULL && strcasecmp(viewname, view->name) != 0)
			continue;
		result = dns_view_flushcache(view);
		if (result != ISC_R_SUCCESS)
			goto out;
		flushed = ISC_TRUE;
	}
	if (flushed)
		result = ISC_R_SUCCESS;
	else
		result = ISC_R_FAILURE;
 out:
	isc_task_endexclusive(server->task);	
	return (result);
}

isc_result_t
ns_server_status(ns_server_t *server, isc_buffer_t *text) {
	int zonecount, xferrunning, xferdeferred, soaqueries;
	unsigned int n;

	zonecount = dns_zonemgr_getcount(server->zonemgr, DNS_ZONESTATE_ANY);
	xferrunning = dns_zonemgr_getcount(server->zonemgr,
					   DNS_ZONESTATE_XFERRUNNING);
	xferdeferred = dns_zonemgr_getcount(server->zonemgr,
					    DNS_ZONESTATE_XFERDEFERRED);
	soaqueries = dns_zonemgr_getcount(server->zonemgr,
					  DNS_ZONESTATE_SOAQUERY);
	n = snprintf((char *)isc_buffer_used(text),
		     isc_buffer_availablelength(text),
		     "number of zones: %u\n"
		     "debug level: %d\n"
		     "xfers running: %u\n"
		     "xfers deferred: %u\n"
		     "soa queries in progress: %u\n"
		     "query logging is %s\n"
		     "server is up and running",
		     zonecount, ns_g_debuglevel, xferrunning, xferdeferred,
		     soaqueries, server->log_queries ? "ON" : "OFF");
	if (n >= isc_buffer_availablelength(text))
		return (ISC_R_NOSPACE);
	isc_buffer_add(text, n);
	return (ISC_R_SUCCESS);
}
