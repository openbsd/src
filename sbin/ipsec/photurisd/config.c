/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * config.c:
 * config handling functions
 */

#ifndef lint
static char rcsid[] = "$Id: config.c,v 1.10 1998/06/30 16:58:34 provos Exp $";
#endif

#define _CONFIG_C_

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <pwd.h>
#include <gmp.h>
#if defined(_AIX) || defined(NEED_STRSEP)
#include "strsep.h"
#endif
#include "config.h"
#include "photuris.h"
#include "modulus.h"
#include "exchange.h"
#include "attributes.h"
#include "buffer.h"
#include "state.h"
#include "identity.h"
#include "spi.h"
#include "server.h"
#include "errlog.h"
#include "buffer.h"
#include "scheme.h"
#include "api.h"
#ifdef IPSEC
#include "kernel.h"
#endif


static FILE *config_fp;
static struct cfgx *cfgxroot;

static void 
open_config_file(char *file)
{
     char *p;

     if (file != NULL)
	  p = file;
     else
	  p = config_file;

     if (p == NULL) 
          crit_error(0, "no file in open_config_file()"); 
 
     config_fp = fopen(p, "r"); 
     if (config_fp == (FILE *) NULL) 
          crit_error(1, "can't open file %s in open_config_file()", p); 
}

static void
close_config_file(void)
{
     fclose(config_fp);
}

static char *
config_get(char *token)
{
     char *p;
     while(fgets(buffer, BUFFER_SIZE, config_fp)) {
	  p = buffer; 
	  chomp(p);
          while(isspace(*p)) 
               p++; 
	  while(isspace(p[strlen(p)-1]))
	       p[strlen(p)-1] = '\0';

          if (*p == '#') 
               continue; 
 
          if (!strncmp(p, token, strlen(token))) 
               return p; 

     }

     return NULL;
}

int
cfgx_insert(char *name, int id)
{
     struct cfgx *ob;

     if ((ob = malloc(sizeof(struct cfgx))) == NULL)
	  return -1;

     ob->name = strdup(name);
     ob->id = id;

     ob->next = cfgxroot;
     cfgxroot = ob;

     return 0;
}

struct cfgx *
cfgx_get(char *name)
{
     struct cfgx *ob;
     
     for(ob = cfgxroot; ob; ob = ob->next)
	  if (ob->name && !strcmp(name, ob->name))
	       break;

     return ob;
}

void
cfgx_clear(void)
{
     struct cfgx *ob;
     while(cfgxroot) {
	  ob = cfgxroot;
	  cfgxroot = cfgxroot->next;
	  free(ob);
     }
}

/*
 * Parses the type of an attribute: ident|enc|auth.
 */

int
parse_type(char *line) 
{

     int type = 0;
     while (*line) {
	  if (!strncmp(line, "ident", 5)) {
	       type |= AT_ID;
	       line += 5;
	  } else if (!strncmp(line, "enc", 3)) {
	       type |= AT_ENC;
	       line += 3;
	  } else if (!strncmp(line, "auth", 4)) {
	       type |= AT_AUTH;
	       line += 4;
	  } else
	       return -1;
	  while (isspace(*line))
	       line++;
	  if (*line && *line++ != '|')
	       return -1;
     }

     return type;
}

int
init_attributes(void)
{
     char *p, *p2;
     attribute_list *ob = NULL;
     struct in_addr in;
     int i, def_flag = 0;
     char attrib[257];
     struct cfgx *cfgattrib = NULL;

#ifdef DEBUG
     printf("[Setting up attributes]\n");
#endif

     /* Set up attribute delimeter */
     cfgx_insert("AT_AH_ATTRIB", AT_AH_ATTRIB);
     cfgx_insert("AT_ESP_ATTRIB", AT_ESP_ATTRIB);

     open_config_file(attrib_file);
     while((p2 = config_get("")) != NULL) {
	  p = strsep(&p2, " ");
	  if (p == NULL)
	       continue;

	  if (p2 == NULL || inet_addr(p) == -1 || 
	       inet_network(p2) == -1) {  /* Attributes follow now */

	       cfgattrib = cfgx_get(p);
	       if (cfgattrib == NULL && strchr(p, ',') != NULL) {
		    char *name, *p3, *p4;
		    attrib_t tmpatt, *ob;

		    p4 = p;

		    if (p2 != NULL)
			 p4[strlen(p4)] = ' ';

		    name = strsep(&p4, ",");
		    while (isspace(*name))
			 name++;
		    i = strlen(name) - 1;
		    while (isspace(name[i]) && i > 0)
			 name[i--] = 0;

		    if ((p2 = strsep(&p4, ",")) == NULL ||
			(p3 = strsep(&p4, ",")) == NULL) {
			 log_error(0, "Mal formated attribute definition for %s in init_attributess()", name);
			 continue;
		    }

		    if ((tmpatt.id = atoi(p2)) <= 0) {
			 log_error(0, "Bad id %s for %s in init_attributes()", p2, name);
			 continue;
		    }

		    if ((tmpatt.klen = atoi(p4)) < 0) {
			 log_error(0, "Bad key length %s for %s in init_attributes()", p4, name);
			 continue;
		    }

		    while (isspace(*p3)) 
			 p3++;
		    i = strlen(p3) - 1;
		    while (isspace(p3[i]) && i > 0)
			 p3[i--] = 0;

		    if ((tmpatt.type = parse_type(p3)) == -1) {
			 log_error(0, "Unkown attribute type %s for %s in init_attributes()", p3, name);
			 continue;
		    }

#ifdef IPSEC
		    if ((tmpatt.type & ~AT_ID) &&
			kernel_known_transform(tmpatt.id) == -1) {
			 log_error(0, "Attribute %s not supported by kernel in init_attributes()", name);
			 continue;
		    }
#endif
		    
		    if ((ob = calloc(1, sizeof(attrib_t))) == NULL)
			 crit_error(1, "calloc() in init_attributes()");

		    *ob = tmpatt;
		    putattrib(ob);
		    cfgx_insert(name, ob->id);
		    cfgattrib = cfgx_get(name);
#ifdef DEBUG
		    printf("New attribute: %s, id: %d, type: %d, klen: %d\n", name, ob->id, ob->type, ob->klen);
#endif
	       }

	       if (cfgattrib == NULL) {
		    log_error(0, "Unknown attribute %s in init_attributes()", 
			      p);
		    continue;
	       }

	       if (ob == NULL && (ob = attrib_new()) == NULL) 
                    crit_error(1, "attribute_new() in init_attributes()");
	       else 
		    def_flag = 1;

	       attrib[0] = cfgattrib->id;
	       attrib[1] = 0;
		    
	       /* Copy attributes in object */
	       ob->attributes = realloc(ob->attributes, 
					ob->attribsize + attrib[1] +2);
	       if (ob->attributes == NULL)
		    crit_error(1, "realloc() in init_attributes()");

	       bcopy(attrib, ob->attributes + ob->attribsize, attrib[1] + 2);
	       ob->attribsize += attrib[1] + 2;
			
	  } else {
#ifdef DEBUG
	       printf("Reading attributes for %s / %s\n",
		      p, p2);
#endif
	       /* Insert previous attribute */
	       if (ob != NULL) {
		    attrib_insert(ob);
		    if (ob->address == NULL)
			 def_flag = 1;
	       }

	       /* Get a new attribute object */
	       if ((ob = attrib_new()) == NULL)
		    crit_error(1, "attribute_new() in init_attributes()");

	       ob->netmask = inet_addr(p2);
	       in.s_addr = inet_addr(p) & ob->netmask;
	       if ((ob->address = calloc(strlen(inet_ntoa(in))+1, 
					 sizeof(char))) == NULL)
		    crit_error(1, "calloc() in init_attributes()");
	       strcpy(ob->address, inet_ntoa(in));
	  }
     }
     if (ob != NULL)
	  attrib_insert(ob);
     close_config_file();

     if (!def_flag)
	  crit_error(0, "No default attribute list in init_attributes()");

     cfgx_clear();
     return 1;
}

int
init_schemes(void)
{
     struct moduli_cache *tmp;
     mpz_t generator, bits;
     u_int32_t scheme_bits;

     char *p, *p2;
     u_int16_t size;
     int gen_flag = 0;

#ifdef DEBUG
     printf("[Setting up exchange schemes]\n");
#endif

     open_config_file(NULL);

     mpz_init(generator);
     mpz_init(bits);

     while((p = config_get(CONFIG_EXCHANGE)) != NULL) {
	  p2 = p + strlen(CONFIG_EXCHANGE);
	  if (!isspace(*p2))
	       continue;
          while(isspace(*p2))
               p2++;

	  /* Get exchange Scheme */
	  if (!strncmp(p2, "DH_G_2_MD5", 10)) {
	       p = p2 + 11;
	       mpz_set_ui(generator, 2);
	       *(u_int16_t *)buffer = htons(DH_G_2_MD5);
	  } else if (!strncmp(p2, "DH_G_2_DES_MD5", 14)) { 
	       p = p2 + 15;
               mpz_set_ui(generator, 2); 
               *(u_int16_t *)buffer = htons(DH_G_2_DES_MD5);
	  } else if (!strncmp(p2, "DH_G_2_3DES_SHA1", 16)) { 
	       p  = p2 + 17;
               mpz_set_ui(generator, 2); 
               *(u_int16_t *)buffer = htons(DH_G_2_3DES_SHA1);
	  } else {
	       log_error(0, "Unknown scheme %s in init_schemes()", p2);
	       continue;
	  }

	  /* Base schemes need a modulus */
	  if ((scheme_bits = strtol(p, NULL, 10)) == 0 && 
	       ntohs(*(u_int16_t *)buffer) == scheme_get_ref(buffer) ) {
	       log_error(0, "No bits in scheme %s in init_schemes()", p2);
	       continue;
	  }
	       
	  if (scheme_bits != 0) {
	       
	       if ((tmp = mod_find_generator(generator)) == NULL)
		    continue;

	       while(tmp != NULL) {
		    mpz_get_number_bits(bits, tmp->modulus);
		    if (mpz_get_ui(bits) == scheme_bits)
			 break;
		    tmp = mod_find_generator_next(tmp, generator);
	       }
	       if (tmp == NULL) {
		    log_error(0, "Could not find %d bit modulus in init_schemes()",
			      scheme_bits);
		    continue;
	       }

	       size = BUFFER_SIZE - 2;
	       if (mpz_to_varpre(buffer+2, &size, tmp->modulus, bits) == -1)
		    continue;
	  } else {
	       size = 2;
	       buffer[2] = buffer[3] = 0;
	  }
	       
	  global_schemes = realloc(global_schemes, global_schemesize
					+ size + 2);
	  if (global_schemes == NULL)
	       crit_error(1, "out of memory in init_schems()");
	  
	  /* DH_G_2_MD5 is a MUST, so we generate it if gen_flag == 0 */
	  if (*(u_int16_t *)buffer == htons(DH_G_2_MD5))
	       gen_flag = 1;

	  bcopy(buffer, global_schemes + global_schemesize, size + 2);
	  global_schemesize += size + 2;

     }
#ifdef DEBUG
     printf("Read %d bytes of exchange schemes.\n", global_schemesize);
#endif
     close_config_file();

     if (!gen_flag) {
	  log_error(0, "DH_G_2_MD5 not in config file, inserting it");
	  mpz_set_ui(generator, 2); 
	  if ((tmp = mod_find_generator(generator)) == NULL) 
	       crit_error(0, "no modulus for generator 2 in init_schemes()");

 	  mpz_get_number_bits(bits, tmp->modulus);
	  size = BUFFER_SIZE - 2; 
	  if (mpz_to_varpre(buffer+2, &size, tmp->modulus, bits) == -1) 
	       crit_error(0, "mpz_to_varpre() in init_schemes()");
                
	  *(u_int16_t *)buffer = htons(DH_G_2_MD5);
     }

     mpz_clear(generator);
     mpz_clear(bits);

     return 1;
}

int
init_moduli(int primes)
{
     struct moduli_cache *tmp;
     char *p, *p2;
     mpz_t m, g;

     open_config_file(NULL);
 
#ifdef DEBUG
     printf("[Bootstrapping moduli]\n");
#endif

     mpz_init(m);
     mpz_init(g);

     while((p = config_get(CONFIG_MODULUS)) != NULL) {
	  p2 = p + strlen(CONFIG_MODULUS);
	  while(isspace(*p2))
	       p2++;

	  /* Get generator */
	  if ((p=strsep(&p2, " ")) == NULL)
	       continue;

	  /* Convert an ascii string to mpz, autodetect base */
	  if (mpz_set_str(g, p, 0) == -1)
	       continue;
	  
	  /* Get modulus */
	  if (mpz_set_str(m, p2, 0) == -1)
	       continue;

	  if ((tmp = mod_new_modgen(m, g)) == NULL)
	       crit_error(0, "no memory in init_moduli()");

	  mod_insert(tmp);

	  if (!primes) {
	       tmp->iterations = MOD_PRIME_MAX;
	       tmp->status = MOD_PRIME;
	  }
     }
     
     close_config_file();

     mpz_clear(m);
     mpz_clear(g);

     /* Now check primality */
     if (primes)
	  mod_check_prime(MOD_PRIME_MAX, 0);

     return 0;
}

int
init_times(void)
{
     char *p, *p2;
     int i, *value;
     open_config_file(NULL);
 
#ifdef DEBUG
     printf("[Setting up times]\n");
#endif

     while((p2 = config_get(CONFIG_CONFIG)) != NULL) {
	  p2 += sizeof(CONFIG_CONFIG);

	  if ((p=strsep(&p2, " ")) == NULL)
	       continue;
	  if (p2 == NULL)
	       continue;

	  if (!strcmp(p, CONFIG_MAX_RETRIES))
	       value = &max_retries;
	  else if (!strcmp(p, CONFIG_RET_TIMEOUT))
	       value = &retrans_timeout;
	  else if (!strcmp(p, CONFIG_EX_TIMEOUT))
	       value = &exchange_timeout;
	  else if (!strcmp(p, CONFIG_EX_LIFETIME))
	       value = &exchange_lifetime;
	  else if (!strcmp(p, CONFIG_SPI_LIFETIME))
	       value = &spi_lifetime;
	  else {
	       log_error(0, "unkown options %s in init_times()", p);
	       continue;
	  }

	  if ((i = atoi(p2)) < 1) {
	       log_error(0, "value %d too small in init_times()", i);
	       continue;
	  }

	  *value = i;
     }

     close_config_file();

     /* Now some hard coded checks */
     if (exchange_timeout < max_retries*retrans_timeout)
	  crit_error(0, "Exchange Timeout < Retransmission * Retrans. Timeout");
     if (exchange_lifetime < 2*exchange_timeout)
	  crit_error(0, "Exchange Lifetime < 2 * Exchange Timeout");
     if (spi_lifetime < 3*exchange_timeout)
	  crit_error(0, "SPI Lifetime < 3 * Exchange Timeout");

     return 0;
}

void
startup_parse(struct stateob *st, char *p2)
{
     char *p, *p3;
     struct hostent *hp;

     while((p=strsep(&p2, " ")) != NULL && strlen(p)) {
	  if ((p3 = strchr(p, '=')) == NULL) {
	       log_error(0, "missing = in %s in startup_parse()", p);
	       continue;
	  }
	  if (strlen(++p3) == 0) {
	       log_error(0, "option missing after %s in startup_parse()", p);
	       continue;
	  }
	  if (!strncmp(p, OPT_DST, strlen(OPT_DST))) {
	       hp = NULL;
	       if (inet_addr(p3) == -1 && (hp = gethostbyname(p3)) == NULL) {
		    log_error(1, "invalid destination address: %s", p3);
		    continue;
	       }
	       if (hp == NULL) 
		    strncpy(st->address, p3, 15);
	       else {
		    struct sockaddr_in sin;
		    bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
		    strncpy(st->address, inet_ntoa(sin.sin_addr), 15);
	       }
	       st->address[15] = '\0';
	  } else if (!strncmp(p, OPT_PORT, strlen(OPT_PORT))) {
	       if ((st->port = atoi(p3)) == 0) {
		    log_error(0, "invalid port number: %s", p3);
		    continue;
	       }
	  } else if (!strncmp(p, CONFIG_EX_LIFETIME, strlen(CONFIG_EX_LIFETIME))) {
	       if ((st->exchange_lifetime = atol(p3)) == 0) {
		    log_error(0, "invalid exchange lifetime: %s", p3);
		    continue;
	       }
	  } else if (!strncmp(p, CONFIG_SPI_LIFETIME, strlen(CONFIG_SPI_LIFETIME))) {
	       if ((st->spi_lifetime = atol(p3)) == 0) {
		    log_error(0, "invalid spi lifetime: %s", p3);
		    continue;
	       }
	  } else if (!strncmp(p, OPT_USER, strlen(OPT_USER))) {
	       struct passwd *pwd;
	       if ((st->user = strdup(p3)) == NULL) {
		    log_error(1, "strdup() in startup_parse()");
		    continue;
	       }
	       if ((pwd = getpwnam(st->user)) == NULL) {
		    log_error(1, "getpwnam() in startup_parse()");
		    free(st->user);
	            st->user = NULL;
		    continue;
	       }
	  } else if (!strncmp(p, OPT_OPTIONS, strlen(OPT_OPTIONS))) {
	       while((p = strsep(&p3, ",")) != NULL) {
		    if(!strcmp(p, OPT_ENC))
			 st->flags |= IPSEC_OPT_ENC;
		    else if(!strcmp(p, OPT_AUTH))
			 st->flags |= IPSEC_OPT_AUTH;
		    else {
			 log_error(0, "Unkown options %s in startup_parse()", p);
			 continue;
		    }
	       }
	  } else if (!strncmp(p, OPT_TSRC, strlen(OPT_TSRC))) {
	       p = strsep(&p3, "/");
	       if (p == NULL || p3 == NULL) {
		    log_error(0, "tsrc missing addr/mask in startup_parse()");
		    continue;
	       }
	       if ((st->isrc = inet_addr(p)) == -1) {
		    log_error(0, "invalid tsrc addr %s in startup_parse()",
			      p);
		    continue;
	       }
	       if ((st->ismask = inet_addr(p3)) == -1 &&
		   strcmp(p3, "255.255.255.255")) {
		    log_error(0, "invalid tsrc mask %s in startup_parse()",
			      p3);
		    st->isrc = -1;
		    continue;
	       }
	  } else if (!strncmp(p, OPT_TDST, strlen(OPT_TDST))) {
	       p = strsep(&p3, "/");
	       if (p == NULL || p3 == NULL) {
		    log_error(0, "tdst missing addr/mask in startup_parse()");
		    continue;
	       }
	       if ((st->idst = inet_addr(p)) == -1) {
		    log_error(0, "invalid tdst addr %s in startup_parse()", p);
		    continue;
	       }
	       if ((st->idmask = inet_addr(p3)) == -1 &&
		   strcmp(p3, "255.255.255.255")) {
		    log_error(0, "invalid tdst mask %s in startup_parse()", p3);
		    st->idst = -1;
		    continue;
	       }
	  }
     }
}

void
startup_end(struct stateob *st)
{
     if (!strlen(st->address)) {
	  log_error(0, "no destination given in startup_end()");
	  state_value_reset(st);
	  free(st);
	  return;
     }
     if (st->port == 0)
	  st->port = global_port;

     if (st->flags == 0)
	  st->flags = IPSEC_OPT_ENC | IPSEC_OPT_AUTH;

     if (st->isrc != -1 && st->idst != -1 && st->isrc && st->idst)
	  st->flags |= IPSEC_OPT_TUNNEL;

#ifdef DEBUG
     printf("Starting exchange with: %s:%d and options:", 
	    st->address, st->port);
     if (st->flags & IPSEC_OPT_ENC)
	  printf("%s ", OPT_ENC);
     if (st->flags & IPSEC_OPT_AUTH)
	  printf("%s ", OPT_AUTH);
     if (st->flags & IPSEC_OPT_TUNNEL)
	  printf("(tunnel mode) ");
     else
	  printf("(transport mode) ");
     if (st->user != NULL)
	  printf("for user %s", st->user);
     printf("\n");
#endif
     if (start_exchange(global_socket, st, 
			st->address, st->port) == -1) {
	  log_error(0, "start_exchange in startup_end()");
	  state_value_reset(st);
	  free(st);
     } else 
	  state_insert(st);
}

int
init_startup(void)
{
     char *p2;
     struct stateob *st = NULL;

#ifdef DEBUG
     printf("[Starting initial exchanges]\n");
#endif

     open_config_file(PHOTURIS_STARTUP);
     while(1) {
	  p2 = config_get("");
	  /* We read a newline or end of file */
	  if((p2 == NULL || strlen(p2) == 0) && st != NULL) {
	       startup_end(st);
	       st = NULL;
	       if (p2 != NULL)
		    continue;
	       else
		    break;
	  }
	  if (p2 == NULL)
	       break;
	  if (!strlen(p2))
	       continue;

	  if (st == NULL && ((st = state_new()) == NULL))
		    crit_error(0, "state_new() in init_startup()");

	  startup_parse(st, p2);

     }
     close_config_file();

     return 0;
}

#ifndef DEBUG
void
reconfig(int sig)
{
     log_error(0, "Reconfiguring on SIGHUP");

     clearattrib();		/* Clear attribute id hash */
     attrib_cleanup();		/* Clear list of offered attributes */

     identity_cleanup(NULL);
     mod_cleanup();
     
     free(global_schemes); global_schemes = NULL;
     global_schemesize = 0;

     state_cleanup();

     init_times();
     init_moduli(0);
     init_schemes();
     init_attributes();
     init_identities(NULL, NULL);
}

int
init_signals(void)
{
     struct sigaction sa, osa;

     bzero(&sa, sizeof(sa));
     sigemptyset(&sa.sa_mask);
     sigaddset(&sa.sa_mask, SIGHUP);
     sa.sa_handler = reconfig;
     sigaction(SIGHUP, &sa, &osa);

     return 1;
}
#endif

int 
pick_scheme(u_int8_t **scheme, u_int16_t *schemesize, 
	    u_int8_t *offered, u_int16_t offeredsize)
{
     u_int32_t size = 0;
     u_int32_t osize, asize = 0;
     u_int8_t *schemep = NULL;
     u_int8_t *modp = NULL;          /* Pointer to the modulus */
     u_int32_t modsize = 0, actsize = 0, gensize = 0;
     u_int8_t scheme_ref[2];
     u_int8_t *p = NULL;

     while(size < global_schemesize) {
	  osize = 0;
	  while(osize < offeredsize) {
	       /* XXX - Policy? now take bigger moduli */
	       p = scheme_get_mod(offered + osize);
	       actsize = varpre2octets(p);

	       if (schemep == NULL && 
		   !bcmp(offered+osize, global_schemes + size, 2)) {
		    /* We found a scheme we want use, now we need to get the
		     * modulus for it.
		     */
		    schemep = offered + osize;
		    break;
	       }
	       osize += scheme_get_len(offered + osize);
	  }
	  if (schemep != NULL)
	       break;
	  size += scheme_get_len(global_schemes + size);
     }

     if (schemep == NULL) {
	  log_error(0, "Found no scheme in pick_scheme()");
	  return -1;
     }

     if (actsize <= 2) {
	  if (ntohs(*(u_int16_t *)schemep) == scheme_get_ref(schemep)) {
	       log_error(0, "Base scheme has no modulus in pick_scheme()");
	       return -1;
	  }
	  *(u_int16_t *)scheme_ref = htons(scheme_get_ref(schemep));
	  osize = 0;
	  while(osize < offeredsize) {
	       /* XXX - Policy? now take bigger moduli */
	       p = scheme_get_mod(offered + osize);
	       actsize = varpre2octets(p);
	       if (!bcmp(offered + osize, scheme_ref,2) && actsize > 2) {
		    if (actsize > modsize) {
			 modp = p;
			 modsize = actsize;
		    }
	       }
	  
	       osize += scheme_get_len(offered + osize);
	  }
     } else {
	  modsize = actsize;
	  modp = p;
     }

     if (*scheme != NULL)
	  free(*scheme);

     p = scheme_get_gen(schemep);
     if (p != NULL) {
	  gensize = varpre2octets(p);

	  /* XXX - VPN this works only for small numbers */
	  asize = 2 + 2 + modsize + gensize;

     } else {
	  asize = 2 + modsize;
     }

     if ((*scheme = calloc(asize, sizeof(u_int8_t))) == NULL) {
	  log_error(1, "No memory in pick_scheme()");
	  return -1;
     }

     bcopy(schemep, *scheme, 2);
     /* XXX - VPN this works only for small numbers */
     if (p != NULL) {
	  (*scheme)[2] = gensize >> 8;
	  (*scheme)[3] = gensize & 0xFF;
	  bcopy(p, *scheme+2+2, gensize);
     }
     bcopy(modp, *scheme+2+(p == NULL ? 0 : 2 + gensize), modsize);

     *schemesize = asize;
     return 0;
}

/* 
 * Fills attrib, with attributes we offer to other parties,
 * read the necessary values from some config file
 */

int 
pick_attrib(struct stateob *st, u_int8_t **attrib, u_int16_t *attribsize)
{
     attribute_list *ob;
     int mode = 0, i, n, count, first;
     
     if ((ob = attrib_find(st->address)) == NULL) {
	  log_error(0, "attrib_find() in pick_attrib()");
	  return -1;
     }


     /* Get the attributes in the right order */
     count = 0;
     for (n=0; n<=AT_ESP_ATTRIB; n++) {
	  first = 1; mode = 0;
	  for (i=0; i<ob->attribsize; i += ob->attributes[i+1]+2) {
	       if (ob->attributes[i] == AT_AH_ATTRIB )
		    mode = AT_AH_ATTRIB;
	       else if (ob->attributes[i] == AT_ESP_ATTRIB)
		    mode = AT_ESP_ATTRIB;
	       else if (n == mode) {
		    if (first && n > 0) {
			 buffer[count] = n;
			 buffer[count+1] = 0;
			 count += 2;
			 first = 0;
		    }
		    bcopy(ob->attributes+i, buffer+count, 
			  ob->attributes[i+1]+2);
		    count += ob->attributes[i+1]+2;
	       }
	  }
     }
     if (count == 0) {
          log_error(0, "no attributes in attribute list for %s in pick_attrib()",
		    st->address);
	  return -1;
     }

     if ((*attrib = calloc(count, sizeof(u_int8_t))) == NULL) {
	  log_error(1, "calloc() in in pick_attrib()"); 
          return -1; 
     }
     bcopy(buffer, *attrib, count);
     *attribsize = count;

     return 0;
}


/*
 * Select attributes we actually want to use for the SA.
 */

int
select_attrib(struct stateob *st, u_int8_t **attributes, u_int16_t *attribsize)
{
     u_int16_t count = 0;
     u_int8_t *wantesp, *wantah, *offeresp, *offerah, *p;
     u_int16_t wantespsize, wantahsize, offerespsize, offerahsize;
     attribute_list *ob; 
     attrib_t *attprop;
     
     if ((ob = attrib_find(NULL)) == NULL) { 
	  log_error(0, "attrib_find() for default in select_attrib() in "
		    "exchange to %s", st->address); 
	  return -1; 
     } 
     
     /* Take from Owner */
     get_attrib_section(ob->attributes, ob->attribsize, 
			&wantesp, &wantespsize, AT_ESP_ATTRIB);
     get_attrib_section(ob->attributes, ob->attribsize, 
			&wantah, &wantahsize, AT_AH_ATTRIB);

     
     /* Take from User */
     get_attrib_section(st->uSPIoattrib, st->uSPIoattribsize,
			&offeresp, &offerespsize, AT_ESP_ATTRIB);
     get_attrib_section(st->uSPIoattrib, st->uSPIoattribsize,
			&offerah, &offerahsize, AT_AH_ATTRIB);
     
     p = buffer;
     if (wantesp != NULL && offeresp != NULL && (st->flags & IPSEC_OPT_ENC)) {
	  /* Take the ESP section */
	  char *tp = wantesp, *ta = wantesp;
	  u_int16_t tpsize = 0, tasize = 0;
	  u_int8_t flag[20], flagc, hmac = 0;
	  int res;
	  attrib_t *attah = NULL;
	  
	  /* 
	   * We travers the ESP section and look for flags,
	   * perhaps mutually exclusive flags should be handled
	   * but at the moment we only support the HMAC indicator
	   */

	  flagc = 0;
	  while (tpsize < wantespsize && flagc < sizeof(flag)) {
	       if (isinattrib(offeresp, offerespsize, tp[tpsize])) {
		    attprop = getattrib(tp[tpsize]);
		    /* A simple flag has no type */
		    if (attprop != NULL && attprop->type == 0) {
			 flag[flagc++] = attprop->id;
			 switch(attprop->id) {
			 case AT_HMAC:
			      hmac = 1;
			      break;
			 default:
			      break;
			 }
		    }
	       }
	       tpsize += tp[tpsize+1]+2;
	  }

	  tpsize = 0;
	  attprop = NULL;
	  /* We travers the ESP section and look for the first ENC attribute */
	  while (tpsize < wantespsize) {
	       if (isinattrib(offeresp, offerespsize, tp[tpsize])) {
		    attprop = getattrib(tp[tpsize]);
		    if (attprop != NULL && attprop->type == AT_ENC)
			 break;
	       }
	       tpsize += tp[tpsize+1]+2;
	  }
	  if (tpsize >= wantespsize)
	       attprop = NULL;

	  /* If we find a fitting AH, we take it */
	  while (hmac && attprop != NULL && tasize < wantespsize) {
	       if (isinattrib(offeresp, offerespsize, ta[tasize])) {
		    attah = getattrib(ta[tasize]);
		    if (attah != NULL && (attah->type & AT_AUTH)) {
#ifdef IPSEC
			 res = kernel_valid(attprop, attah);
#else
			 res = 0;
#endif
			 if (res == AT_ENC) {
			      /* 
			       * Our ESP attribute does not allow AH, but 
			       * since the ESP attribute is our first choice, 
			       * dont try for other.
			       */
			      attah = NULL;
			      break;
			 } else if (res != AT_AUTH) 
			      break;
		    }
	       }
	       
	       tasize += ta[tasize+1]+2;
	  }
	  if (tasize >= wantespsize)
	       attah = NULL;
	       
	  if (attprop != NULL) {
	       /* Put proper header in there */
	       p[0] = AT_ESP_ATTRIB;
	       p[1] = 0;
	       count += 2;
	       p += 2;

	       /* We are using our own attributes, safe to proceed */
	       bcopy(wantesp+tpsize, p, wantesp[tpsize+1] + 2);
	       count += wantesp[tpsize+1] + 2;
	       p += wantesp[tpsize+1] + 2;

	       if (attah != NULL) {
		    /* We are using our own attributes, safe to proceed */
		    bcopy(wantesp+tasize, p, wantesp[tasize+1] + 2);
		    count += wantesp[tasize+1] + 2;
		    p += wantesp[tasize+1] + 2;
	       }

	       /* Insert the flags also */
	       while (flagc--) {
		    p[0] = flag[flagc];
		    p[1] = 0;
		    p += 2;
		    count += 2;
	       }
	  }
     }

     if (wantah != NULL && offerah != NULL && (st->flags & IPSEC_OPT_AUTH)) {
	  /* Take the AH section */
	  u_int8_t *tp = wantah;
	  u_int16_t tpsize = 0;
	  u_int8_t flag[20], flagc;

	  flagc = 0;
	  /* Look for flags */
	  while (tpsize < wantahsize && flagc < sizeof(flag)) {
	       if (isinattrib(offerah, offerahsize, tp[tpsize])) {
		    attprop = getattrib(tp[tpsize]);
		    if (attprop != NULL && attprop->type == 0)
			 flag[flagc++] = attprop->id;
	       }
	       tpsize += tp[tpsize+1]+2;
	  }

	  tpsize = 0;
	  attprop = NULL;
	  /* We travers the AH section and look for the first AH attribute */
	  while (tpsize < wantahsize) {
	       if (isinattrib(offerah, offerahsize, tp[tpsize])) {
		    attprop = getattrib(tp[tpsize]);
		    if (attprop != NULL && (attprop->type & AT_AUTH)
#ifdef IPSEC
			&& (kernel_valid_auth(attprop, flag, flagc) != -1)
#endif
			)
			 break;
	       }
	       tpsize += tp[tpsize+1]+2;
	  }
	  if (tpsize >= wantahsize)
	       attprop = NULL;

	  if (attprop != NULL) {
	       /* Put proper header in there */
	       p[0] = AT_AH_ATTRIB;
	       p[1] = 0;
	       count += 2;
	       p += 2;

	       /* We are using our own attributes, safe to proceed */
	       bcopy(wantah+tpsize, p, wantah[tpsize+1] + 2);
	       count += wantah[tpsize+1] + 2;
	       p += wantah[tpsize+1] + 2;

	       /* Insert flags also */
	       while (flagc--) {
		    p[0] = flag[flagc];
		    p[1] = 0;
		    p += 2;
		    count += 2;
	       }
	  }
     }

     if (count == 0) {
	  log_error(0, "Offered and wanted list of attributes did not have a common subset in select_attrib()");
	  return -1;
     }

     if ((*attributes=calloc(count,sizeof(u_int8_t))) == NULL) {
	  log_error(1, "Out of memory for SPI attributes (%d)", count);
	  return -1;
     }
     *attribsize = count;
     bcopy(buffer, *attributes, count);

     return 0;
}
/*
 * Removes whitespace from the end of a string
 */

char *
chomp(char *p)
{
     if (!*p)
	  return p;

     while (*(p+1))
	  p++;

     if (isspace(*p))
	  *p = '\0';

     return p;
}

static const char hextab[] = { 
     '0', '1', '2', '3', '4', '5', '6', '7', 
     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' 
};

int
bin2hex(char *buffer, int *size, u_int8_t *data, u_int16_t len)
{
     u_int16_t off;

     if (*size < 2*len+1)
	  return -1;
     
     off = 0;
     while(len > 0) {
	  buffer[off++] = hextab[*data >> 4];
	  buffer[off++] = hextab[*data & 0xF];
	  data++;
	  len--;
     }
     buffer[off++] = '\0';

     *size = off;
     return 0;
}
