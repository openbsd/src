/*
 * copyright (c) 1994 Mats O Jansson <moj@stacken.kth.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$Id: ypserv_db.c,v 1.1 1995/11/01 16:56:37 deraadt Exp $";
#endif

#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "yplog.h"
#include "ypdb.h"
#include "ypdef.h"

struct opt_map {
	mapname	map;
	DBM	*db;
	int	*dptr;
	struct	opt_map *next;
	struct	opt_map *prev;
	struct	opt_map *rnext;
	struct	opt_map *rprev;
	int	host_lookup;
};
typedef struct opt_map opt_map;

struct opt_domain {
	domainname	domain;
	opt_map		*map_root;
	struct		opt_domain *next;
};
typedef struct opt_domain opt_domain;

	opt_domain	*domain_root = NULL;
	opt_map		*map_first = NULL;
	opt_map		*map_last  = NULL;

extern int usedns;

/*
 * Check if key is a YP private key. Return TRUE if it is and
 * ypprivate is FALSE.
 */

int
yp_private(key,ypprivate)
        datum	key;
        int	ypprivate;
{
        int	result;

  	if (ypprivate) {
		return (FALSE);
	}

        result = FALSE;

	if ((!result) && (key.dsize == YP_LAST_LEN)) {
		result = (strcmp(key.dptr,YP_LAST_KEY) == 0);
	}
	
	if ((!result) && (key.dsize == YP_INPUT_LEN)) {
		result = (strcmp(key.dptr,YP_INPUT_KEY) == 0);
	}
	
	if ((!result) && (key.dsize == YP_OUTPUT_LEN)) {
		result = (strcmp(key.dptr,YP_OUTPUT_KEY) == 0);
	}
	
	if ((!result) && (key.dsize == YP_MASTER_LEN)) {
		result = (strcmp(key.dptr,YP_MASTER_KEY) == 0);
	}
	
	if ((!result) && (key.dsize == YP_DOMAIN_LEN)) {
		result = (strcmp(key.dptr,YP_DOMAIN_KEY) == 0);
	}
	
	if ((!result) && (key.dsize == YP_INTERDOMAIN_LEN)) {
		result = (strcmp(key.dptr,YP_INTERDOMAIN_KEY) == 0);
	}
	
	if ((!result) && (key.dsize == YP_SECURE_LEN)) {
		result = (strcmp(key.dptr,YP_SECURE_KEY) == 0);
	}
	
	return(result);
}     

/*
 * Close least recent used map. This routine is called when we have
 * no more file descripotors free, or we want to close all maps.
 */

void
ypdb_close_last()
{
	opt_map		*m = NULL;
	opt_domain	*d = NULL;

	m = map_last;
	d = (opt_domain *) m->dptr;
	
	/* Close database */
	ypdb_close(m->db);
	free(m->db);

#ifdef DEBUG
	yplog_cat("info: ypdb_close_last: close map ");
	yplog_cat(m->map);
	yplog_cat(" in domain ");
	yplog_cat(d->domain);
	yplog_cat("\n");
#endif

	/* Deallocate space for map name */
	free(m->map);
	
	/* Unlink record from recent used list */
	if (m->rprev != NULL) {
		m->rprev->rnext = m->rnext;
	}
	map_last = m->rprev;

	/* Unlink record from domain list */
	if (m->prev == NULL) {
		d->map_root = m->next;
	} else {
		m->prev->next = m->next;
	}
	if (m->next != NULL) {
		m->next->prev = m->prev;
	}
	  
	free(m);
	
}

/*
 * Close all open maps.
 */

void
ypdb_close_all()
{
	
	while (map_last != NULL) {
		ypdb_close_last();
	}
}

/*
 * Close Database if Open/Close Optimization isn't turned on.
 */

void
ypdb_close_db(db)
	DBM	*db;
{
#ifndef OPTDB
	ypdb_close_all();
#endif
}

DBM *
ypdb_open_db_std(domain, map, status, map_info)
	domainname	domain;
        mapname		map;
	ypstat		*status;
        opt_map		*map_info;
{
	static	char map_path[255];
	struct	stat finfo;
	DBM	*db;
	
	*status = YP_TRUE;			/* Preset return value */

	db = NULL;				/* Database isn't opened */
	
	/* Check domain */
	sprintf(map_path,"%s/%s",YP_DB_PATH,domain);
	if (!((stat(map_path, &finfo) == 0) &&
	      ((finfo.st_mode & S_IFMT) == S_IFDIR))) {
		*status = YP_NODOM;
#ifdef DEBUG
		yplog_cat("err: ypdb_open_db_std: domain not found\n");
#endif
	}
		
	if (*status >= 0) {
		/* Check map */
		sprintf(map_path,"%s/%s/%s%s",YP_DB_PATH,domain,map,YPDB_SUFFIX);
		if (!(stat(map_path, &finfo) == 0)) {
			*status = YP_NOMAP;
#ifdef DEBUG
			yplog_cat("err: ypdb_open_db: map not found\n");
#endif
		}
	}
		
	/* Ok, if no error yet, open database */
	if (*status >= 0) {
		sprintf(map_path,"%s/%s/%s",YP_DB_PATH,domain,map);
		db = ypdb_open(map_path, O_RDONLY, 0444);
#ifdef DEBUG
		yplog_cat("info: ypdb_open_db: open ");
		yplog_cat(map_path);
		yplog_cat("\n");
#endif
	}

	return(db);
}

DBM *
ypdb_open_db(domain, map, status, map_info)
	domainname	domain;
        mapname		map;
	ypstat		*status;
        opt_map		*map_info;
{
	static	char map_path[255];
	struct	stat finfo;
	DBM	*db;
	static char   domain_key[YP_INTERDOMAIN_LEN] = YP_INTERDOMAIN_KEY;
	FILE	*fd;
	opt_domain *d = NULL;
	opt_map	*m = NULL;
	datum	k,v;
	
	*status = YP_TRUE;			/* Preset return value */

	db = NULL;				/* Database isn't opened */

	d = domain_root;			/* Find domain in list */
	while ((d != NULL) && (strcmp(d->domain,domain) != 0)) {
		d = d->next;
	}

	if (d != NULL) {			/* Domain found ! */
		m = d->map_root;		/* Find map in list */
		while ((m != NULL) && (strcmp(m->map,map) != 0)) {
			m = m->next;
		}
	}

	if (m != NULL) {			/* Map found ! */
		db = m->db;
		if (m != map_first) {
			/* Move map to top of recent used list */
			m->rprev->rnext = m->rnext;
			if (m->rnext == NULL) {
				map_last = m->rprev;
			} else {
				m->rnext->rprev = m->rprev;
			}
			m->rprev = NULL;
			map_first->rprev = m;
			m->rnext = map_first;
			map_first = m;
		}
	}

	if (db == NULL) {			/* Database not opened */

		/* Make a dummy open, if it succeeds then it's OK */
		/* to open a new database, else we must close one */
	  	/* first. Close least recent used database        */
	  
		fd = fopen("/dev/null","r");
		if (fd != NULL) {
			fclose(fd);		/* All is OK */
		} else {
			ypdb_close_last();	/* Not OK, close one */
		}

		/* Check domain */
		sprintf(map_path,"%s/%s",YP_DB_PATH,domain);
		if (!((stat(map_path, &finfo) == 0) &&
		      ((finfo.st_mode & S_IFMT) == S_IFDIR))) {
			*status = YP_NODOM;
#ifdef DEBUG
			yplog_cat("err: ypdb_open_db: domain not found\n");
#endif
		}
		
		if (*status >= 0) {
			/* Check map */
			sprintf(map_path,"%s/%s/%s%s",YP_DB_PATH,domain,map,YPDB_SUFFIX);
			if (!(stat(map_path, &finfo) == 0)) {
				*status = YP_NOMAP;
#ifdef DEBUG
				yplog_cat("err: ypdb_open_db: map not found\n");
#endif
			}
		}
		
		/* Ok, if no error yet, open database */
		if (*status >= 0) {
			sprintf(map_path,"%s/%s/%s",YP_DB_PATH,domain,map);
			db = ypdb_open(map_path, O_RDONLY, 0444);
#ifdef DEBUG
			yplog_cat("info: ypdb_open_db: open ");
			yplog_cat(map_path);
			yplog_cat("\n");
#endif
		}

		if (*status >= 0) {
		        
			if (d == NULL) {		/* Domain is new */
			        /* Allocate a domain record */
				d = (opt_domain *)
				    malloc((unsigned) sizeof(opt_domain));

				/* Setup domain record */
				d->domain   = (domainname)
				              malloc(strlen(domain)+1);
				(void)strcpy(d->domain,domain);
				d->map_root = NULL;
				d->next     = domain_root;
				domain_root = d;
			}
			
			if (m == NULL) {		/* Map is new */
			        /* Allocatr a map record */
				m = (opt_map *)
				    malloc((unsigned) sizeof(opt_map));
				
				/* Setup map record */
				m->map      = (mapname)
				              malloc(strlen(map)+1);
				(void)strcpy(m->map,map);
				m->db	  = malloc((unsigned) sizeof(DBM));
				memcpy(m->db,db,sizeof(DBM));
				m->next     = d->map_root;
				if (m->next != NULL) {
					m->next->prev = m;
				}
				m->prev     = NULL;
				m->dptr	  = (int *) d;
				d->map_root = m;
				m->rnext    = map_first;
				m->rprev    = NULL;
				if (map_first != NULL) {
					map_first->rprev = m;
				}
				if (map_last == NULL) {
					map_last = m;
				}
				m->host_lookup = FALSE;
				/* Check if hosts. */
				if ((strcmp(map, YP_HOSTNAME) == 0) ||
				    (strcmp(map, YP_HOSTADDR) == 0)) {
					if (!usedns) {
						k.dptr = (char *) &domain_key;
						k.dsize = YP_INTERDOMAIN_LEN;
						v = ypdb_fetch(db,k);
						if (v.dptr != NULL) {
							m->host_lookup = TRUE;
#ifdef DEBUG
			yplog_cat("info: ypdb_open_db: YP_INTERDOMAIN\n");
#endif
						}
					} else {
						m->host_lookup = usedns;
#ifdef DEBUG
			yplog_cat("info: ypdb_open_db: ypserv -d\n");
#endif
					}
				}
				map_first = m;
			}

			if (map_info != NULL) {
				map_info->map	      = NULL;
				map_info->db	      = NULL;
				map_info->dptr	      = m->dptr;
				map_info->next	      = m->next;
				map_info->prev	      = m->prev;
				map_info->rnext	      = m->rnext;
				map_info->rprev	      = m->rprev;
				map_info->host_lookup = m->host_lookup;
			}
		}
		
	}

	return(db);
}

ypstat
lookup_host(nametable, host_lookup, db, keystr, result)
	int	nametable;
        int	host_lookup;
	DBM	*db;
	char	*keystr;
	ypresp_val *result;
{
	struct	hostent	*host;
	struct  in_addr *addr_name;
	struct	in_addr addr_addr;
	static  char val[255];
	ypstat	status;
	char	*ptr;
	
	status = YP_NOKEY;

	if (host_lookup) {
		if (nametable) {
			host = gethostbyname(keystr);
			if ((host != NULL) &&
			    (host->h_addrtype == AF_INET)) {
				addr_name = (struct in_addr *)
				  *host->h_addr_list;
				sprintf(val,"%s %s",
					inet_ntoa(*addr_name), keystr);
				while ((ptr = *(host->h_aliases)) != NULL) {
					strcat(val," ");
					strcat(val,ptr);
					host->h_aliases++;
				}
				result->val.valdat_val = val;
				result->val.valdat_len = strlen(val);
				status = YP_TRUE;
			}
		} else {
			inet_aton(keystr, &addr_addr);
			host = gethostbyaddr((char *) &addr_addr,
					     sizeof(addr_addr), AF_INET);
			if (host != NULL) {
				sprintf(val,"%s %s",keystr,host->h_name);
				while ((ptr = *(host->h_aliases)) != NULL) {
					strcat(val," ");
					strcat(val,ptr);
					host->h_aliases++;
				}
				result->val.valdat_val = val;
				result->val.valdat_len = strlen(val);
				status = YP_TRUE;
			}
		}
	}
	
	return(status);
}

ypresp_val
ypdb_get_record(domain, map, key, ypprivate)
	domainname	domain;
        mapname		map;
        keydat		key;
        int		ypprivate;
{
	static	ypresp_val res;
	static	char keystr[255];
	DBM	*db;
	datum	k,v;
	ypstat	status;
	int	host_lookup;
	opt_map	map_info;

	bzero((char *)&res, sizeof(res));
	
	db = ypdb_open_db(domain, map, &status, &map_info);
	host_lookup = map_info.host_lookup;
	
	if (status >= 0) {
		
		(void) strncpy(keystr, key.keydat_val, key.keydat_len);
		keystr[key.keydat_len] = '\0';
		
		k.dptr = (char *) &keystr;
		k.dsize = key.keydat_len + 1;
		
		if (yp_private(k,ypprivate)) {
			status = YP_NOKEY;
		} else {
			
			v = ypdb_fetch(db,k);
			if (v.dptr == NULL) {
			  
				status = YP_NOKEY;
				
				if (strcmp(map, YP_HOSTNAME) == 0) {
					status = lookup_host(TRUE, host_lookup,
							     db, &keystr,&res);
				}

				if (strcmp(map, YP_HOSTADDR) == 0) {
					status = lookup_host(FALSE,host_lookup,
							     db, &keystr,&res);
				}

			} else {
				res.val.valdat_val = v.dptr;
				res.val.valdat_len = v.dsize - 1;
			}
			
		}
	}
	
	ypdb_close_db(db);
	
	res.stat = status;
	
	return (res);
}

ypresp_key_val
ypdb_get_first(domain, map, ypprivate)
	domainname domain;
        mapname map;
        int ypprivate;
{
	static ypresp_key_val res;
	DBM	*db;
	datum	k,v;
	ypstat	status;

	bzero((char *)&res, sizeof(res));
	
	db = ypdb_open_db(domain, map, &status, NULL);

	if (status >= 0) {

	  k = ypdb_firstkey(db);
	  
	  while (yp_private(k,ypprivate)) {
	    k = ypdb_nextkey(db);
	  };
	  
	  if (k.dptr == NULL) {
	    status = YP_NOKEY;
	  } else {
	    res.key.keydat_val = k.dptr;
	    res.key.keydat_len = k.dsize;
	    v = ypdb_fetch(db,k);
	    if (v.dptr == NULL) {
	      status = YP_NOKEY;
	    } else {
	      res.val.valdat_val = v.dptr;
	      res.val.valdat_len = v.dsize - 1;
	    }
	  }
	}

	ypdb_close_db(db);
	
	res.stat = status;

	return (res);
}

ypresp_key_val
ypdb_get_next(domain, map, key, ypprivate)
	domainname domain;
        mapname map;
        keydat key;
        int ypprivate;
{
	static ypresp_key_val res;
	DBM	*db;
	datum	k,v,n;
	ypstat	status;

	bzero((char *)&res, sizeof(res));
	
	db = ypdb_open_db(domain, map, &status, NULL);
	
	if (status >= 0) {

	  n.dptr = key.keydat_val;
	  n.dsize = key.keydat_len;
	  v.dptr = NULL;
	  v.dsize = 0;
	  k.dptr = NULL;
	  k.dsize = 0;

	  n = ypdb_setkey(db,n);

	  if (n.dptr != NULL) {
	    k = ypdb_nextkey(db);
	  } else {
	    k.dptr = NULL;
	  };

	  if (k.dptr != NULL) {
	    while (yp_private(k,ypprivate)) {
	      k = ypdb_nextkey(db);
	    };
	  };

	  if (k.dptr == NULL) {
	    status = YP_NOMORE;
	  } else {
	    res.key.keydat_val = k.dptr;
	    res.key.keydat_len = k.dsize;
	    v = ypdb_fetch(db,k);
	    if (v.dptr == NULL) {
	      status = YP_NOMORE;
	    } else {
	      res.val.valdat_val = v.dptr;
	      res.val.valdat_len = v.dsize - 1;
	    }
	  }
	}

	ypdb_close_db(db);
	
	res.stat = status;

	return (res);
}

ypresp_order
ypdb_get_order(domain, map)
	domainname domain;
        mapname map;
{
	static ypresp_order res;
	static char   order_key[YP_LAST_LEN] = YP_LAST_KEY;
	static char   order[MAX_LAST_LEN+1];
	DBM	*db;
	datum	k,v;
	ypstat	status;

	bzero((char *)&res, sizeof(res));
	
	db = ypdb_open_db(domain, map, &status, NULL);
	
	if (status >= 0) {

	  k.dptr = (char *) &order_key;
	  k.dsize = YP_LAST_LEN;

	  v = ypdb_fetch(db,k);
	  if (v.dptr == NULL) {
	    status = YP_NOKEY;
	  } else {
	    strncpy(order, v.dptr, v.dsize);
	    order[v.dsize] = '\0';
	    res.ordernum = (u_int) atol(order);
	  }
	}

	ypdb_close_db(db);
	
	res.stat = status;

	return (res);
}

ypresp_master
ypdb_get_master(domain, map)
	domainname domain;
        mapname map;
{
	static ypresp_master res;
	static char   master_key[YP_MASTER_LEN] = YP_MASTER_KEY;
	static char   master[MAX_MASTER_LEN+1];
	DBM	*db;
	datum	k,v;
	ypstat	status;

	bzero((char *)&res, sizeof(res));
	
	db = ypdb_open_db(domain, map, &status, NULL);
	
	if (status >= 0) {

	  k.dptr = (char *) &master_key;
	  master_key[YP_MASTER_LEN] = '\0';
	  k.dsize = YP_MASTER_LEN;

	  v = ypdb_fetch(db,k);
	  if (v.dptr == NULL) {
	    status = YP_NOKEY;
	  } else {
	    strncpy(master, v.dptr, v.dsize);
	    master[v.dsize] = '\0';
	    res.peer = (peername) &master;
	  }
	}

	ypdb_close_db(db);
	
	res.stat = status;

	return (res);
}

bool_t
ypdb_xdr_get_all(xdrs, req)
	XDR *xdrs;
        ypreq_nokey *req;
{
	static ypresp_all resp;
	static bool_t more = TRUE;
	DBM	*db;
	datum	k,v;
	ypstat	status;
	extern	int	acl_access_ok;

	bzero((char *)&resp, sizeof(resp));
	
	if(acl_access_ok) {
		db = ypdb_open_db_std(req->domain, req->map, &status);
	} else {
		db = NULL;
		resp.ypresp_all_u.val.stat = YP_NODOM;
	}

	if (resp.ypresp_all_u.val.stat < 0) {
		resp.more = FALSE;
		
		if (!xdr_ypresp_all(xdrs, &resp)) {
#ifdef DEBUG
			yplog_cat("xdr_ypresp_all: 1 failed\n");
#endif				        
			return(FALSE);
		}

		return(TRUE);
	        
	}

	k = ypdb_firstkey(db);
	
	while (yp_private(k,FALSE)) {
		k = ypdb_nextkey(db);
        };
	
	while(resp.ypresp_all_u.val.stat >= 0) {
		
		if (k.dptr == NULL) {
			v.dptr = NULL;
		} else {
			v = ypdb_fetch(db,k);
	        }

		if (v.dptr == NULL) {

			resp.ypresp_all_u.val.stat = YP_NOKEY;
			
		} else {
		  
#ifdef DEBUG
			yplog_cat("key: ");
			yplog_cat(k.dptr);
			yplog_cat(" val: ");
			yplog_cat(v.dptr);
			yplog_cat("\n");
#endif
			resp.more = more;
			resp.ypresp_all_u.val.stat = YP_TRUE;
			resp.ypresp_all_u.val.key.keydat_val = k.dptr;
			resp.ypresp_all_u.val.key.keydat_len = k.dsize - 1;
			resp.ypresp_all_u.val.val.valdat_val = v.dptr;
			resp.ypresp_all_u.val.val.valdat_len = v.dsize - 1;
			
			if (!xdr_ypresp_all(xdrs, &resp)) {
#ifdef DEBUG
				yplog_cat("xdr_ypresp_all: 2 failed\n");
#endif				        
				return(FALSE);
			}
			
			k = ypdb_nextkey(db);
			while (yp_private(k,FALSE)) {
				k = ypdb_nextkey(db);
			};

		};
	  
	};
		
	bzero((char *)&resp, sizeof(resp));
	
	resp.more = FALSE;
	resp.ypresp_all_u.val.stat = status;
	
	if (!xdr_ypresp_all(xdrs, &resp)) {
#ifdef DEBUG
		yplog_cat("xdr_ypresp_all: 3 failed\n");
#endif				        
		return(FALSE);
	}
		
	ypdb_close(db);
	
	return (TRUE);
}

