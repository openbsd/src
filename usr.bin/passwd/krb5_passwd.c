/*	$OpenBSD: krb5_passwd.c,v 1.2 1996/06/26 05:37:45 deraadt Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
/*static char sccsid[] = "from: @(#)krb_passwd.c	5.4 (Berkeley) 3/1/91";*/
static char rcsid[] = "$OpenBSD: krb5_passwd.c,v 1.2 1996/06/26 05:37:45 deraadt Exp $";
#endif /* not lint */

#ifdef KERBEROS5

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <krb5/adm_defs.h>
#include <krb5/krb5.h>
#include <krb5/kdb.h>
#include <krb5/kdb_dbm.h>
#include <krb5/ext-proto.h>
#include <krb5/los-proto.h>
#include <krb5/asn1.h>
#include <krb5/config.h>
#include <krb5/base-defs.h>
#include <krb5/asn.1/encode.h>

#include <krb5/widen.h>

#include <krb5/adm_err.h>
#include <krb5/errors.h>
#include <krb5/kdb5_err.h>
#include <krb5/krb5_err.h>

static krb5_error_code get_first_ticket __P((krb5_ccache, krb5_principal));
static krb5_error_code print_and_choose_password __P((char *, krb5_data *));
static krb5_error_code adm5_init_link __P((krb5_data *, int *));

struct sockaddr_in local_sin, remote_sin;

krb5_creds my_creds;

extern char *krb5_default_pwd_prompt1;

/*
 * Try no preauthentication first; then try the encrypted timestamp
 */
int preauth_search_list[] = {
    0,			
    KRB5_PADATA_ENC_TIMESTAMP,
    -1
};

krb_passwd()
{
    static void finish();
    krb5_ccache cache = NULL;
    char cache_name[255];
    krb5_flags cc_flags;
    krb5_address local_addr, foreign_addr;
    struct passwd *pw;
    krb5_principal client, server;
    char default_name[256];
    char *client_name;		/* Single string representation of client id */
    krb5_data requested_realm;
    char *local_realm;
    char input_string[768];
    krb5_error_code retval;	/* return code */
    int local_socket;
    int c, count;
    krb5_error *err_ret;
    krb5_ap_rep_enc_part *rep_ret;
    kadmin_requests rd_priv_resp;
    krb5_checksum send_cksum;
    int cksum_alloc = 0;
    krb5_data msg_data, inbuf;
    krb5_int32 seqno;
    char *new_password;
    int new_pwsize;
    krb5_data *decodable_pwd_string;
    int i, j;
    static struct rlimit rl = { 0, 0 };

#ifdef KRB_NONETWORK
    extern int networked();
    int krb_secure;
    struct stat statbuf;
#endif

#ifdef KRB_NONETWORK	/* Allow or Disallow Remote Clients to Modify Passwords */
    /*
     *	If a Client Modifies a Password using kpasswd on this host
     *	from a remote host or network terminal, the Password selected 
     *	is transmitted across the network in Cleartext.
     *
     *	The systems administrator can disallow "remote" kpasswd usage by
     *	creating the file "/etc/krb.secure"
     */
    krb_secure = 0;
    /* 
     *	First check to see if the file /etc/krb.secure exists.
     *	If it does then krb_secure to 1.
     */

    if (stat("/etc/krb.secure", &statbuf) == 0) krb_secure = 1;

    /*
     *	Check to see if this process is tied to a physical terminal.
     *	Network() verifies the terminal device is not a pseudo tty
     */
    if (networked() && krb_secure) {
	fprintf(stderr,"passwd: Sorry but you cannot %s from a\n", argv[0]);
	fprintf(stderr,"        pseudo tty terminal.\n");
	retval = 1;
	goto finish;
    }
#endif    

    /* (3 * 255) + 1 (/) + 1 (@) + 1 (NULL) */
    if ((client_name = (char *) calloc (1, (3 * 256))) == NULL) {
	fprintf(stderr, "passwd: No Memory for Client_name\n");
	retval = 1;
	goto finish;
    }

    if ((requested_realm.data = (char *) calloc (1, 256)) == NULL) {
	fprintf(stderr, "passwd: No Memory for realm_name\n");
	retval = 1;
	free(client_name);
	goto finish;
    }

    (void)signal(SIGHUP, SIG_IGN);
    (void)signal(SIGINT, SIG_IGN);
    (void)signal(SIGTSTP, SIG_IGN);

    if (setrlimit(RLIMIT_CORE, &rl) < 0) {
	(void)fprintf(stderr,
	    "passwd: setrlimit: %s\n", strerror(errno));
	return(1);
    }

    krb5_init_ets();
    memset((char *) default_name, 0, sizeof(default_name));
    
    /* Identify Default Credentials Cache */
    if ((retval = krb5_cc_default(&cache))) {
	fprintf(stderr, "passwd: Error while getting default ccache.\n");
	goto finish;
    }

    /*
     * 	Attempt to Modify Credentials Cache 
     *		retval == 0 ==> ccache Exists - Use It 
     * 		retval == ENOENT ==> No Entries, but ccache Exists 
     *		retval != 0 ==> Assume ccache does NOT Exist 
     */
    cc_flags = 0;
    if ((retval = krb5_cc_set_flags(cache, cc_flags))) {
	/* Search passwd file for client */
	pw = getpwuid((int) getuid());
	if (pw) {
	    (void) strcpy(default_name, pw->pw_name);
	}
	else {
	    fprintf(stderr, 
		    "passwd: Unable to Identify Customer from Password File\n");
	    retval = 1;
	    goto finish;
	}

	/* Use this to get default_realm and format client_name */
	if ((retval = krb5_parse_name(default_name, &client))) {
	    fprintf(stderr, "passwd: Unable to Parse Client Name\n");
	    goto finish;
	}

	if ((retval = krb5_unparse_name(client, &client_name))) {
	    fprintf(stderr, "passwd: Unable to Parse Client Name\n");
	    goto finish;
	}

	requested_realm.length = client->realm.length;
	memcpy((char *) requested_realm.data, 
	       (char *) client->realm.data,
	       requested_realm.length);
    }
    else {
	/* Read Client from Cache */
	if ((retval = krb5_cc_get_principal(cache, (krb5_principal *) &client))) {
	    fprintf(stderr, "passwd: Unable to Read Customer Credentials File\n");
	    goto finish;
	}

	if ((retval = krb5_unparse_name(client, &client_name))) {
	    fprintf(stderr, "passwd: Unable to Parse Client Name\n");
	    goto finish;
	}

	requested_realm.length = client->realm.length;
	memcpy((char *) requested_realm.data, 
	       (char *) client->realm.data,
	       requested_realm.length);

	(void) krb5_cc_close(cache);
    }

    /* Create credential cache for changepw */
    (void) sprintf(cache_name, "FILE:/tmp/tkt_cpw_%d", getpid());

    if ((retval = krb5_cc_resolve(cache_name, &cache))) {
	fprintf(stderr, "passwd: Unable to Resolve Cache: %s\n", cache_name);
    }
    
    if ((retval = krb5_cc_initialize(cache, client))) {
        fprintf(stderr, "passwd: Error initializing cache: %s\n", cache_name);
        goto finish;
    }
 
    /*
     *	Verify User by Obtaining Initial Credentials prior to Initial Link
     */
    if ((retval = get_first_ticket(cache, client))) {
	goto finish;
    }
    
    /* Initiate Link to Server */
    if ((retval = adm5_init_link(&requested_realm, &local_socket))) {
	goto finish;
    } 

#define SIZEOF_INADDR sizeof(struct in_addr)

    /* V4 kpasswd Protocol Hack */
{
    int msg_length = 0;

    retval = krb5_net_write(local_socket, (char *) &msg_length + 2, 2);
    if (retval < 0) {
        fprintf(stderr, "passwd: krb5_net_write failure\n");
        goto finish;
    }
}

    local_addr.addrtype = ADDRTYPE_INET;
    local_addr.length = SIZEOF_INADDR ;
    local_addr.contents = (krb5_octet *)&local_sin.sin_addr;

    foreign_addr.addrtype = ADDRTYPE_INET;
    foreign_addr.length = SIZEOF_INADDR ;
    foreign_addr.contents = (krb5_octet *)&remote_sin.sin_addr;

    /* compute checksum, using CRC-32 */
    if (!(send_cksum.contents = (krb5_octet *)
	malloc(krb5_checksum_size(CKSUMTYPE_CRC32)))) {
        fprintf(stderr, "passwd: Insufficient Memory while Allocating Checksum\n");
        goto finish;
    }
    cksum_alloc++;
    /* choose some random stuff to compute checksum from */
    if (retval = krb5_calculate_checksum(CKSUMTYPE_CRC32,
	                                 ADM_CPW_VERSION,
					 strlen(ADM_CPW_VERSION),
					 0,
					 0, /* if length is 0, crc-32 doesn't
                                               use the seed */
					 &send_cksum)) {
        fprintf(stderr, "Error while Computing Checksum: %s\n",
		error_message(retval));
        goto finish;
    }

    /* call Kerberos library routine to obtain an authenticator,
       pass it over the socket to the server, and obtain mutual
       authentication. */

   if ((retval = krb5_sendauth((krb5_pointer) &local_socket,
			ADM_CPW_VERSION, 
			my_creds.client, 
			my_creds.server,
			AP_OPTS_MUTUAL_REQUIRED,
			&send_cksum,
			0,           
			cache,
			&seqno, 
			0,           /* don't need a subsession key */
			&err_ret,
			&rep_ret))) {
	fprintf(stderr, "passwd: Error while performing sendauth: %s\n",
		error_message(retval));
        goto finish;
    }

    /* Get credentials : to use for safe and private messages */
    if (retval = krb5_get_credentials(0, cache, &my_creds)){
	fprintf(stderr, "passwd: Error Obtaining Credentials: %s\n", 
		error_message(retval));
	goto finish;
    }

    /* Read back what the server has to say... */
    if (retval = krb5_read_message(&local_socket, &inbuf)){
	fprintf(stderr, "passwd: Read Message Error: %s\n",
	        error_message(retval));
        goto finish;
    }
    if ((inbuf.length != 2) || (inbuf.data[0] != KADMIND) ||
	(inbuf.data[1] != KADMSAG)){
	fprintf(stderr, "passwd: Invalid ack from admin server.\n");
	goto finish;
    }

    inbuf.data[0] = KPASSWD;
    inbuf.data[1] = CHGOPER;
    inbuf.length = 2;

    if ((retval = krb5_mk_priv(&inbuf,
			ETYPE_DES_CBC_CRC,
			&my_creds.keyblock, 
			&local_addr, 
			&foreign_addr,
			seqno,
			KRB5_PRIV_DOSEQUENCE|KRB5_PRIV_NOTIME,
			0,
			0,
			&msg_data))) {
        fprintf(stderr, "passwd: Error during First Message Encoding: %s\n",
			error_message(retval));
        goto finish;
    }
    free(inbuf.data);

    /* write private message to server */
    if (krb5_write_message(&local_socket, &msg_data)){
        fprintf(stderr, "passwd: Write Error During First Message Transmission\n");
	retval = 1;
        goto finish;
    } 
    free(msg_data.data);

	(void)signal(SIGHUP, finish);
	(void)signal(SIGINT, finish);

#ifdef MACH_PASS /* Machine-generated Passwords */
    /* Ok Now let's get the private message */
    if (retval = krb5_read_message(&local_socket, &inbuf)){
        fprintf(stderr, "passwd: Read Error During First Reply: %s\n",
		error_message(retval));
	retval = 1;
        goto finish;
    }

    if ((retval = krb5_rd_priv(&inbuf,
			&my_creds.keyblock,
    			&foreign_addr, 
			&local_addr,
			rep_ret->seq_number, 
			KRB5_PRIV_DOSEQUENCE|KRB5_PRIV_NOTIME,
			0,
			0,
			&msg_data))) {
        fprintf(stderr, "passwd: Error during First Read Decoding: %s\n", 
		error_message(retval));
        goto finish;
    }
    free(inbuf.data);
#endif

    if ((new_password = (char *) calloc (1, ADM_MAX_PW_LENGTH+1)) == NULL) {
	fprintf(stderr, "passwd: Unable to Allocate Space for New Password\n");
	goto finish;
    }

#ifdef MACH_PASS /* Machine-generated passwords */
	/* Offer Client Password Choices */
    if ((retval = print_and_choose_password(new_password,
			 &msg_data))) {
	(void) memset((char *) new_password, 0, ADM_MAX_PW_LENGTH+1);
	free(new_password);
        goto finish;
    }
#else
    new_pwsize = ADM_MAX_PW_LENGTH+1;
    if ((retval = krb5_read_password("New Kerberos password: ",
				     "Retype new Kerberos password: ",
				     new_password,
				     &new_pwsize))) {
	fprintf(stderr, "\nError while reading new password for '%s'\n",
                                client_name);
	(void) memset((char *) new_password, 0, ADM_MAX_PW_LENGTH+1);
	free(new_password);
        goto finish;
    }
#endif

    inbuf.data = new_password;
    inbuf.length = strlen(new_password);

    if ((retval = krb5_mk_priv(&inbuf,
			ETYPE_DES_CBC_CRC,
			&my_creds.keyblock, 
			&local_addr, 
			&foreign_addr,
			seqno,
			KRB5_PRIV_DOSEQUENCE|KRB5_PRIV_NOTIME,
			0,
			0,
			&msg_data))) {
        fprintf(stderr, "passwd: Error during Second Message Encoding: %s\n",
		error_message(retval));
        goto finish;
    }
    memset(inbuf.data,0,inbuf.length);
    free(inbuf.data);

    /* write private message to server */
    if (krb5_write_message(&local_socket, &msg_data)){
        fprintf(stderr, "passwd: Write Error During Second Message Transmission\n");
	retval = 1;
        goto finish;
    } 
    free(msg_data.data);

    /* Ok Now let's get the private message */
    if (retval = krb5_read_message(&local_socket, &inbuf)){
        fprintf(stderr, "passwd: Read Error During Second Reply: %s\n",
			error_message(retval));
	retval = 1;
        goto finish;
    }

    if ((retval = krb5_rd_priv(&inbuf,
			&my_creds.keyblock,
    			&foreign_addr, 
			&local_addr,
			rep_ret->seq_number, 
			KRB5_PRIV_DOSEQUENCE|KRB5_PRIV_NOTIME,
			0,
			0,
			&msg_data))) {
        fprintf(stderr, "passwd: Error during Second Read Decoding :%s\n", 
		error_message(retval));
        goto finish;
    }

    rd_priv_resp.appl_code = msg_data.data[0];
    rd_priv_resp.oper_code = msg_data.data[1];
    rd_priv_resp.retn_code = msg_data.data[2];
    if (msg_data.length > 3 && msg_data.data[3]) {
	rd_priv_resp.message = malloc(msg_data.length - 2);
	if (rd_priv_resp.message) {
	    memcpy(rd_priv_resp.message, msg_data.data + 3,
		   msg_data.length - 3);
	    rd_priv_resp.message[msg_data.length - 3] = 0;
	}
    } else
	rd_priv_resp.message = NULL;


    free(inbuf.data);
    free(msg_data.data);
    if (rd_priv_resp.appl_code == KPASSWD) {
	if (rd_priv_resp.retn_code == KPASSBAD) {
	    if (rd_priv_resp.message)
		fprintf(stderr, "passwd: %s\n", rd_priv_resp.message);
	    else
		fprintf(stderr, "passwd: Server returned KPASSBAD.\n");
	} else if (rd_priv_resp.retn_code != KPASSGOOD)
	    fprintf(stderr, "passwd: Server returned unknown kerberos code.\n");
    } else
	fprintf(stderr, "passwd: Server returned bad application code %d\n",
		rd_priv_resp.appl_code);
    
    if (rd_priv_resp.message)
	free(rd_priv_resp.message);

 finish:
    (void) krb5_cc_destroy(cache);

    free(client_name);
    free(requested_realm.data);
    if (cksum_alloc) free(send_cksum.contents);
    if (retval) {
	fprintf(stderr, "passwd: Protocol Failure - Password NOT changed\n");
	exit(1);
    }

    exit(0);
}



krb5_data cpwname = {
	sizeof(CPWNAME)-1,
	CPWNAME
};

static krb5_error_code
get_first_ticket(cache, client)
    krb5_ccache cache;
    krb5_principal client;
{
    char prompt[255];			/* for the password prompt */
    char verify_prompt[255];		/* Verification Prompt if Desired */
    char pword[ADM_MAX_PW_LENGTH+1];	/* storage for the password */
    int  pword_length = sizeof(pword);
    char *old_password;
    int  old_pwsize;
    int	 i;
    
    krb5_address **my_addresses;

    char *client_name;
    char local_realm[255];
    krb5_error_code retval;
    
    if ((retval = krb5_unparse_name(client, &client_name))) {
	fprintf(stderr, "Unable to Unparse Client Name\n");
	return(1);
    }

    (void) printf("Changing Kerberos password for %s\n", client_name);

    if ((retval = krb5_os_localaddr(&my_addresses))) {
	fprintf(stderr, "passwd: Unable to Get Customers Address\n");
	return(1);
    }

    memset((char *) &my_creds, 0, sizeof(my_creds));

    my_creds.client = client;                           
 
    if ((retval = krb5_build_principal_ext(&my_creds.server,
                                        client->realm.length, 
					client->realm.data,
                                        cpwname.length,		/* 6 */ 
					cpwname.data,		/* "kadmin" */
                                        client->realm.length,  
					   /* instance is local realm */
					client->realm.data,
                                        0))) {
        fprintf(stderr, "Error %s while building server name\n");
        return(1);
    }


    if ((old_password = (char *) calloc (1, 255)) == NULL) {
	fprintf(stderr, "passwd: No Memory for Retrieving old password\n");
	return(1);
    }

    old_pwsize = 255;
    if ((retval = krb5_read_password("Old kerberos password: ",
	                             0,
                                     old_password,
                                     &old_pwsize))) {
	fprintf(stderr, "\nError while reading password for '%s'\n",
                client_name);
	return(1);
    }

    /*	Build Request for Initial Credentials */
    for (i=0; preauth_search_list[i] >= 0; i++) {
	retval = krb5_get_in_tkt_with_password(
					0,	/* options */
					my_addresses,
					/* do random preauth */
                                        preauth_search_list[i],
					ETYPE_DES_CBC_CRC,   /* etype */
					KEYTYPE_DES,
					old_password,
					cache,
					&my_creds,
				        0);
	if (retval != KRB5KDC_PREAUTH_FAILED &&
	    retval != KRB5KRB_ERR_GENERIC)
	    break;
    }
	
    if (retval) {
	fprintf(stderr, "passwd: Unable to Get Initial Credentials : %s\n",
		error_message(retval));
    }

    /* Do NOT Forget to zap password  */
    memset((char *) old_password, 0, old_pwsize);
    free(old_password);
    memset((char *) pword, 0, sizeof(pword));
    return(retval);
}

#ifdef MACH_PASS /* Machine-generated Passwords */
static krb5_error_code
print_and_choose_password(new_password, decodable_pwd_string)
    char * new_password;
    krb5_data *decodable_pwd_string;
{
    krb5_error_code retval;
    krb5_pwd_data *pwd_data;
    passwd_phrase_element **next_passwd_phrase_element;
    char prompt[255];
    char *verify_prompt = 0;
    int i, j, k;
    int legit_pswd = 0;	/* Assume No Legitimate Password */
    char *password_list[ADM_MAX_PW_CHOICES];
    char verification_passwd[ADM_MAX_PW_LENGTH+1];
    char phrase_in[ADM_MAX_PHRASE_LENGTH];
    int new_passwd_length;
    char *ptr;
    int verify = 0;	/* Do Not Request Password Selection Verification */ 
    int ok = 0;

#define free_local_password_list() \
{  for ( k = 0; k < i && k < ADM_MAX_PW_CHOICES; k++) { \
      (void) memset(password_list[k], 0, ADM_MAX_PW_LENGTH); \
      free(password_list[k]); } \
}

     /* Decode Password and Phrase Information Obtained from krb5_rd_priv */
     if ((retval = decode_krb5_pwd_data(decodable_pwd_string , &pwd_data))) { 
	fprintf(stderr, "passwd: Unable to Decode Passwords and Phrases\n");
        fprintf(stderr, "	 Notify your System Administrator or the Kerberos Administrator\n");
	return(1);
   }

   next_passwd_phrase_element = pwd_data->element;
   /* Display List in 5 Password/Phrase Increments up to MAX Iterations */
   memset((char *) phrase_in, 0, ADM_MAX_PHRASE_LENGTH);
   for ( j = 0; j <= ADM_MAX_PW_ITERATIONS; j++) {
	if (j == ADM_MAX_PW_ITERATIONS) {
	    fprintf(stderr, "passwd: Sorry - You Have Exceeded the List of Choices (%d) Allowed for Password\n",
		    ADM_MAX_PW_ITERATIONS * ADM_MAX_PW_CHOICES);
	    fprintf(stderr, "	     Modification.  You Must Repeat this Operation in order to Successfully\n");
	    fprintf(stderr, "	     Change your Password.\n");
	    break;
	}

	display_print:
	printf("Choose a password from the following list:\n");

	printf("\nPassword                        Remembrance Aid\n");

	/* Print Passwords and Assistance Phrases List */
	for ( i = 0; i < ADM_MAX_PW_CHOICES; i++){
	    if ((password_list[i] = (char *) calloc (1, 
			ADM_MAX_PW_LENGTH + 1)) == NULL) {
		fprintf(stderr, "passwd: Unable to Allocate Password List.\n");
		return(1);
	    }

	    memcpy(password_list[i],
		(*next_passwd_phrase_element)->passwd->data,
		(*next_passwd_phrase_element)->passwd->length);
	    printf("%s	", password_list[i]);

	    memcpy((char *) phrase_in,
		(*next_passwd_phrase_element)->phrase->data,
		(*next_passwd_phrase_element)->phrase->length);
	    for ( k = 0; 
		  k < 50 && k < (*next_passwd_phrase_element)->phrase->length; 
		  k++) {
		printf("%c", phrase_in[k]);
	    }
	    for ( k = k;
		  k < 70 && k < (*next_passwd_phrase_element)->phrase->length;
		  k++) {
		if (phrase_in[k] == ' ') {
		    printf("\n		");
		    k++;
		    break;
		} else {
		    printf("%c", phrase_in[k]);
		}
	    }
	    for ( k = k;
		  k < (*next_passwd_phrase_element)->phrase->length;
		  k++) {
		printf("%c", phrase_in[k]);
	    }
	    printf("\n");
	    memset((char *) phrase_in, 0, ADM_MAX_PHRASE_LENGTH);
	    next_passwd_phrase_element++;
	}

	sprintf(prompt, 
		"\nEnter Password Selection or a <CR> to get new list: ");

	new_passwd_length = ADM_MAX_PW_LENGTH+1;
	/* Read New Password from Terminal (Do Not Print on Screen) */
	if ((retval = krb5_read_password(&prompt[0], 0, 
	                                 new_password, &new_passwd_length))) {
	    fprintf(stderr, 
		"passwd: Error Reading Password Input or Input Aborted\n");
	    free_local_password_list();
	    break;;
	}

	/* Check for <CR> ==> Provide a New List */
	if (new_passwd_length == 0) continue;

	/* Check that Selection is from List - Server also does this */
	legit_pswd = 0;
	for (i = 0; i < ADM_MAX_PW_CHOICES && !legit_pswd; i++)
	    if ((retval = memcmp(new_password, 
				 password_list[i], 8)) == 0) {
		legit_pswd++;
	    }
	    free_local_password_list();

	    if (!(legit_pswd)) {
	 	printf("\07\07Password must be from the specified list ");
        	printf("- Try Again\n");
	    }

	    if (legit_pswd) break;	/* Exit Loop */
	}		/* ADM_MAX_PW_CHOICES Loop */

   if (!(legit_pswd)) return (1);

   return(0);		/* SUCCESS */
}
#endif

static krb5_error_code
adm5_init_link(realm_of_server, local_socket)
krb5_data *realm_of_server;
int * local_socket;
{
    struct servent *service_process;	       /* service we will talk to */
    struct hostent *local_host;		       /* us */
    struct hostent *remote_host;	       /* host we will talk to */
    struct sockaddr *sockaddr_list;

    char **hostlist;

    int host_count;
    int namelen;
    int i, count;

    krb5_error_code retval;

    /* clear out the structure first */
    (void) memset((char *)&remote_sin, 0, sizeof(remote_sin));

    if ((service_process = getservbyname(CPW_SNAME, "tcp")) == NULL) {
	fprintf(stderr, "passwd: Unable to find Service (%s) Check services file\n",
		CPW_SNAME);
	return(1);
    }

    		/* Copy the Port Number */
    remote_sin.sin_port = service_process->s_port;

    hostlist = 0;

		/* Identify all Hosts Associated with this Realm */
    if ((retval = krb5_get_krbhst (realm_of_server, &hostlist))) {
        fprintf(stderr, "passwd: Unable to Determine Server Name\n");
        return(1);
    }

    for (i=0; hostlist[i]; i++);
 
    count = i;

    if (count == 0) {
        host_count = 0;
        fprintf(stderr, "passwd: No hosts found\n");
        return(1);
    }

    for (i=0; hostlist[i]; i++) {
        remote_host = gethostbyname(hostlist[i]);
        if (remote_host != 0) {

		/* set up the address of the foreign socket for connect() */
	    remote_sin.sin_family = remote_host->h_addrtype;
	    (void) memcpy((char *) &remote_sin.sin_addr, 
			(char *) remote_host->h_addr,
			sizeof(remote_host->h_addr));
	    break;	/* Only Need one */
	}
    }

    free ((char *)hostlist);

    /* open a TCP socket */
    *local_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (*local_socket < 0) {
	fprintf(stderr, "passwd: Cannot Open Socket\n");
	return(1);
    }
    /* connect to the server */
    if (connect(*local_socket, (struct sockaddr *)&remote_sin, sizeof(remote_sin)) < 0) {
	fprintf(stderr, "passwd: Cannot Connect to Socket\n");
	close(*local_socket);
	return(1);
    }

    /* find out who I am, now that we are connected and therefore bound */
    namelen = sizeof(local_sin);
    if (getsockname(*local_socket, 
		(struct sockaddr *) &local_sin, &namelen) < 0) {
	fprintf(stderr, "passwd: Cannot Perform getsockname\n");
	close(*local_socket);
	return(1);
    }
	return(0);
}

static void
finish()
{
	exit(1);
}

#ifdef KRB_NONETWORK
#include <utmp.h>

#ifndef MAXHOSTNAME
#define MAXHOSTNAME 64
#endif

int utfile;		/* Global utfile file descriptor for BSD version
			   of setutent, getutline, and endutent */

#if !defined(SYSV) && !defined(UMIPS)	/* Setutent, Endutent, and getutline
					   routines for non System V Unix
						 systems */
#include <fcntl.h>

void setutent()
{
  utfile = open("/etc/utmp",O_RDONLY);
}

struct utmp * getutline(utmpent)
struct utmp *utmpent;
{
 static struct utmp tmputmpent;
 int found = 0;
 while ( read(utfile,&tmputmpent,sizeof(struct utmp)) > 0 ){
	if ( strcmp(tmputmpent.ut_line,utmpent->ut_line) == 0){
#ifdef NO_UT_HOST
		if ( ( 1) &&
#else
		if ( (strcmp(tmputmpent.ut_host,"") == 0) && 
#endif
	     	   (strcmp(tmputmpent.ut_name,"") == 0)) continue;
		found = 1;
		break;
	}
 }
 if (found) 
	return(&tmputmpent);
 return((struct utmp *) 0);
}

void endutent()
{
  close(utfile);
}
#endif /* not SYSV */


int network_connected()
{
struct utmp utmpent;
struct utmp retutent, *tmpptr;
char *display_indx;
char currenthost[MAXHOSTNAME];
char *username,*tmpname;


/* Macro for pseudo_tty */
#define pseudo_tty(ut) \
        ((strncmp((ut).ut_line, "tty", 3) == 0 && ((ut).ut_line[3] == 'p' \
                                                || (ut).ut_line[3] == 'q' \
                                                || (ut).ut_line[3] == 'r' \
                                                || (ut).ut_line[3] == 's'))\
				|| (strncmp((ut).ut_line, "pty", 3) == 0))

    /* Check to see if getlogin returns proper name */
    if ( (tmpname = (char *) getlogin()) == (char *) 0) return(1);
    username = (char *) malloc(strlen(tmpname) + 1);
    if ( username == (char *) 0) return(1);
    strcpy(username,tmpname);
    
    /* Obtain tty device for controlling tty of current process.*/
    strncpy(utmpent.ut_line,ttyname(0) + strlen("/dev/"),
	    sizeof(utmpent.ut_line));

    /* See if this device is currently listed in /etc/utmp under
       calling user */
#ifdef SYSV
    utmpent.ut_type = USER_PROCESS;
#define ut_name ut_user
#endif
    setutent();
    while ( (tmpptr = (struct utmp *) getutline(&utmpent)) 
            != ( struct utmp *) 0) {

	/* If logged out name and host will be empty */
	if ((strcmp(tmpptr->ut_name,"") == 0) &&
#ifdef NO_UT_HOST
	    ( 1)) continue;
#else
	    (strcmp(tmpptr->ut_host,"") == 0)) continue;
#endif
	else break;
    }
    if (  tmpptr   == (struct utmp *) 0) {
	endutent();
	return(1);
    }
    bcopy((char *)&retutent, (char *)tmpptr, sizeof(struct utmp));
    endutent();
#ifdef DEBUG
#ifdef NO_UT_HOST
    printf("User %s on line %s :\n",
		retutent.ut_name,retutent.ut_line);
#else
    printf("User %s on line %s connected from host :%s:\n",
		retutent.ut_name,retutent.ut_line,retutent.ut_host);
#endif
#endif
    if  (strcmp(retutent.ut_name,username) != 0) {
	 return(1);
    }


    /* If this is not a pseudo tty then everything is OK */
    if (! pseudo_tty(retutent)) return(0);

    /* OK now the work begins there is an entry in utmp and
       the device is a pseudo tty. */

    /* Check if : is in hostname if so this is xwindow display */

    if (gethostname(currenthost,sizeof(currenthost))) return(1);
#ifdef NO_UT_HOST
    display_indx = (char *) 0;
#else
    display_indx = (char *) strchr(retutent.ut_host,':');
#endif
    if ( display_indx != (char *) 0) {
        /* 
           We have X window application here. The host field should have
	   the form => local_system_name:0.0 or :0.0  
           if the window is being displayed on the local system.
         */
#ifdef NO_UT_HOST
	return(1);
#else
        if (strncmp(currenthost,retutent.ut_host,
                (display_indx - retutent.ut_host)) != 0) return(1);
        else return(0);
#endif
    }
    
    /* Host field is empty or is not X window entry. At this point
       we can't trust that the pseudo tty is not connected to a 
       networked process so let's return 1.
     */
    return(1);
}

int networked()
{
  return(network_connected());
}
#endif

#endif /* KERBEROS5 */
