/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/get_in_tkt.c,v $
 *
 * $Locker:  $
 */

/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

/*
 * This file contains two routines: passwd_to_key() converts
 * a password into a DES key (prompting for the password if
 * not supplied), and krb_get_pw_in_tkt() gets an initial ticket for
 * a user.
 */

/*
 * passwd_to_key(): given a password, return a DES key.
 * There are extra arguments here which (used to be?)
 * used by srvtab_to_key().
 *
 * If the "passwd" argument is not null, generate a DES
 * key from it, using string_to_key().
 *
 * If the "passwd" argument is null, call des_read_password()
 * to prompt for a password and then convert it into a DES key.
 *
 * In either case, the resulting key is put in the "key" argument,
 * and 0 is returned.
 */

/*ARGSUSED */
static int
passwd_to_key(user, instance, realm, passwd, key)
	char *user;
	char *instance;
	char *realm;
	char *passwd;
	des_cblock *key;
{
#ifdef NOENCRYPTION
    if (!passwd)
	placebo_read_password(key, "Password: ", 0);
#else
    if (passwd)
	des_string_to_key(passwd,key);
    else
	des_read_password(key,"Password: ",0);
#endif
    return (0);
}

/*ARGSUSED */
static int
afs_passwd_to_key(user, instance, realm, passwd, key)
	char *user;
	char *instance;
	char *realm;
	char *passwd;
	des_cblock *key;
{
#ifdef NOENCRYPTION
    if (!passwd)
        placebo_read_password(key, "Password: ", 0);
#else /* Do encyryption */
    if (passwd)
        afs_string_to_key(passwd, realm, key);
    else {
        des_read_password(key, "Password: ", 0);
    }
#endif /* NOENCRYPTION */
    return (0);
}

/*
 * krb_get_pw_in_tkt() takes the name of the server for which the initial
 * ticket is to be obtained, the name of the principal the ticket is
 * for, the desired lifetime of the ticket, and the user's password.
 * It passes its arguments on to krb_get_in_tkt(), which contacts
 * Kerberos to get the ticket, decrypts it using the password provided,
 * and stores it away for future use.
 *
 * krb_get_pw_in_tkt() passes two additional arguments to krb_get_in_tkt():
 * the name of a routine (passwd_to_key()) to be used to get the
 * password in case the "password" argument is null and NULL for the
 * decryption procedure indicating that krb_get_in_tkt should use the 
 * default method of decrypting the response from the KDC.
 *
 * The result of the call to krb_get_in_tkt() is returned.
 */

int
krb_get_pw_in_tkt(user, instance, realm, service, sinstance, life, password)
	char *user;
	char *instance;
	char *realm;
	char *service;
	char *sinstance;
	int life;
	char *password;
{
    char pword[100];		/* storage for the password */
    int code;

    /* Only request password once! */
    if (!password) {
        if (des_read_pw_string(pword, sizeof(pword)-1, "Password: ", 0))
            pword[0] = '\0'; /* something wrong */
        password = pword;
    }

    code = krb_get_in_tkt(user,instance,realm,service,sinstance,life,
                          passwd_to_key, NULL, password);
    if (code != INTK_BADPW)
      goto done;

    code = krb_get_in_tkt(user,instance,realm,service,sinstance,life,
                          afs_passwd_to_key, NULL, password);
    if (code != INTK_BADPW)
      goto done;

  done:
    if (password == pword)
        bzero(pword, sizeof(pword));
    return(code);
}

#ifdef NOENCRYPTION
/*
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/get_in_tkt.c,v $
 * $Author: tholo $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * This routine prints the supplied string to standard
 * output as a prompt, and reads a password string without
 * echoing.
 */

#ifndef	lint
static char rcsid_read_password_c[] =
"Bones$Header: /home/cvs/src/kerberosIV/krb/Attic/get_in_tkt.c,v 1.1.1.1 1995/12/14 06:52:39 tholo Exp $";
#endif /* lint */

#include <des.h>
#include "conf.h"

#include <stdio.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <setjmp.h>

static jmp_buf env;

static void sig_restore();
static push_signals(), pop_signals();
int placebo_read_pw_string();

/*** Routines ****************************************************** */
int
placebo_read_password(k,prompt,verify)
    des_cblock *k;
    char *prompt;
    int	verify;
{
    int ok;
    char key_string[BUFSIZ];

    if (setjmp(env)) {
	ok = -1;
	goto lose;
    }

    ok = placebo_read_pw_string(key_string, BUFSIZ, prompt, verify);
    if (ok == 0)
	bzero(k, sizeof(C_Block));

lose:
    bzero(key_string, sizeof (key_string));
    return ok;
}

/*
 * This version just returns the string, doesn't map to key.
 *
 * Returns 0 on success, non-zero on failure.
 */

int
placebo_read_pw_string(s,max,prompt,verify)
    char *s;
    int	max;
    char *prompt;
    int	verify;
{
    int ok = 0;
    char *ptr;
    
    jmp_buf old_env;
    struct sgttyb tty_state;
    char key_string[BUFSIZ];

    if (max > BUFSIZ) {
	return -1;
    }

    bcopy(old_env, env, sizeof(env));
    if (setjmp(env))
	goto lose;

    /* save terminal state*/
    if (ioctl(0,TIOCGETP,&tty_state) == -1) 
	return -1;

    push_signals();
    /* Turn off echo */
    tty_state.sg_flags &= ~ECHO;
    if (ioctl(0,TIOCSETP,&tty_state) == -1)
	return -1;
    while (!ok) {
	printf(prompt);
	fflush(stdout);
	if (!fgets(s, max, stdin)) {
	    clearerr(stdin);
	    continue;
	}
	if ((ptr = index(s, '\n')))
	    *ptr = '\0';
	if (verify) {
	    printf("\nVerifying, please re-enter %s",prompt);
	    fflush(stdout);
	    if (!fgets(key_string, sizeof(key_string), stdin)) {
		clearerr(stdin);
		continue;
	    }
            if ((ptr = index(key_string, '\n')))
	    *ptr = '\0';
	    if (strcmp(s,key_string)) {
		printf("\n\07\07Mismatch - try again\n");
		fflush(stdout);
		continue;
	    }
	}
	ok = 1;
    }

lose:
    if (!ok)
	bzero(s, max);
    printf("\n");
    /* turn echo back on */
    tty_state.sg_flags |= ECHO;
    if (ioctl(0,TIOCSETP,&tty_state))
	ok = 0;
    pop_signals();
    bcopy(env, old_env, sizeof(env));
    if (verify)
	bzero(key_string, sizeof (key_string));
    s[max-1] = 0;		/* force termination */
    return !ok;			/* return nonzero if not okay */
}

/*
 * this can be static since we should never have more than
 * one set saved....
 */
static RETSIGTYPE (*old_sigfunc[NSIG])();

static
push_signals()
{
    register i;
    for (i = 0; i < NSIG; i++)
	old_sigfunc[i] = signal(i,sig_restore);
}

static
pop_signals()
{
    register i;
    for (i = 0; i < NSIG; i++)
	signal(i,old_sigfunc[i]);
}

static void
sig_restore(sig,code,scp)
    int sig,code;
    struct sigcontext *scp;
{
    longjmp(env,1);
}
#endif /* NOENCRYPTION */
