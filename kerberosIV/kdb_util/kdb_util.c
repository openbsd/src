/*	$Id: kdb_util.c,v 1.1.1.1 1995/12/14 06:52:42 tholo Exp $	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/*
 * Kerberos database manipulation utility. This program allows you to
 * dump a kerberos database to an ascii readable file and load this
 * file into the database. Read locking of the database is done during a
 * dump operation. NO LOCKING is done during a load operation. Loads
 * should happen with other processes shutdown. 
 *
 * Written July 9, 1987 by Jeffrey I. Schiller
 */

#include <adm_locl.h>

static char *prog; 

Principal aprinc;

static des_cblock master_key, new_master_key;
static des_key_schedule master_key_schedule, new_master_key_schedule;

#define zaptime(foo) bzero((char *)(foo), sizeof(*(foo)))

/* cv_key is a procedure which takes a principle and changes its key, 
   either for a new method of encrypting the keys, or a new master key.
   if cv_key is null no transformation of key is done (other than net byte
   order). */

struct callback_args {
    void (*cv_key)();
    FILE *output_file;
};

static void
print_time(FILE *file, time_t timeval)
{
    struct tm *tm;
    tm = gmtime(&timeval);
    fprintf(file, " %04d%02d%02d%02d%02d",
            tm->tm_year < 1900 ? tm->tm_year + 1900: tm->tm_year,
            tm->tm_mon + 1,
            tm->tm_mday,
            tm->tm_hour,
            tm->tm_min);
}

static long
time_explode(char *cp)
{
    char wbuf[5];
    struct tm tp;
    long maketime(struct tm *tp, int local);
    int local;

    zaptime(&tp);			/* clear out the struct */
    
    if (strlen(cp) > 10) {		/* new format */
	(void) strncpy(wbuf, cp, 4);
	wbuf[4] = 0;
	tp.tm_year = atoi(wbuf);
	cp += 4;			/* step over the year */
	local = 0;			/* GMT */
    } else {				/* old format: local time, 
					   year is 2 digits, assuming 19xx */
	wbuf[0] = *cp++;
	wbuf[1] = *cp++;
	wbuf[2] = 0;
	tp.tm_year = 1900 + atoi(wbuf);
	local = 1;			/* local */
    }

    wbuf[0] = *cp++;
    wbuf[1] = *cp++;
    wbuf[2] = 0;
    tp.tm_mon = atoi(wbuf)-1;

    wbuf[0] = *cp++;
    wbuf[1] = *cp++;
    tp.tm_mday = atoi(wbuf);
    
    wbuf[0] = *cp++;
    wbuf[1] = *cp++;
    tp.tm_hour = atoi(wbuf);
    
    wbuf[0] = *cp++;
    wbuf[1] = *cp++;
    tp.tm_min = atoi(wbuf);


    return(maketime(&tp, local));
}

static int
dump_db_1(char *arg, Principal *principal)
{	    /* replace null strings with "*" */
    struct callback_args *a = (struct callback_args *)arg;
    
    if (principal->instance[0] == '\0') {
	principal->instance[0] = '*';
	principal->instance[1] = '\0';
    }
    if (principal->mod_name[0] == '\0') {
	principal->mod_name[0] = '*';
	principal->mod_name[1] = '\0';
    }
    if (principal->mod_instance[0] == '\0') {
	principal->mod_instance[0] = '*';
	principal->mod_instance[1] = '\0';
    }
    if (a->cv_key != NULL) {
	(*a->cv_key) (principal);
    }
    fprintf(a->output_file, "%s %s %d %d %d %d %x %x",
	    principal->name,
	    principal->instance,
	    principal->max_life,
	    principal->kdc_key_ver,
	    principal->key_version,
	    principal->attributes,
	    (int)htonl (principal->key_low),
	    (int)htonl (principal->key_high));
    print_time(a->output_file, principal->exp_date);
    print_time(a->output_file, principal->mod_date);
    fprintf(a->output_file, " %s %s\n",
	    principal->mod_name,
	    principal->mod_instance);
    return 0;
}

static int
dump_db (char *db_file, FILE *output_file, void (*cv_key) (Principal *))
{
    struct callback_args a;

    a.cv_key = cv_key;
    a.output_file = output_file;
    
    kerb_db_iterate (dump_db_1, (char *)&a);
    return fflush(output_file);
}

static void
load_db (char *db_file, FILE *input_file)
{
    char    exp_date_str[50];
    char    mod_date_str[50];
    int     temp1, temp2, temp3;
    int code;
    char *temp_db_file;
    temp1 = strlen(db_file)+2;
    temp_db_file = malloc (temp1);
    strcpy(temp_db_file, db_file);
    strcat(temp_db_file, "~");

    /* Create the database */
    if ((code = kerb_db_create(temp_db_file)) != 0) {
	fprintf(stderr, "Couldn't create temp database %s: %s\n",
		temp_db_file, strerror(code));
	exit(1);
    }
    kerb_db_set_name(temp_db_file);
    for (;;) {			/* explicit break on eof from fscanf */
        u_long key_lo, key_hi;	/* Must match format string */
	bzero((char *)&aprinc, sizeof(aprinc));
	if (fscanf(input_file,
		   "%s %s %d %d %d %hd %lx %lx %s %s %s %s\n",
		   aprinc.name,
		   aprinc.instance,
		   &temp1,
		   &temp2,
		   &temp3,
		   &aprinc.attributes,
		   &key_lo,
		   &key_hi,
		   exp_date_str,
		   mod_date_str,
		   aprinc.mod_name,
		   aprinc.mod_instance) == EOF)
	    break;
	aprinc.key_low = ntohl (key_lo);
	aprinc.key_high = ntohl (key_hi);
	aprinc.max_life = (unsigned char) temp1;
	aprinc.kdc_key_ver = (unsigned char) temp2;
	aprinc.key_version = (unsigned char) temp3;
	aprinc.exp_date = time_explode(exp_date_str);
	aprinc.mod_date = time_explode(mod_date_str);
	if (aprinc.instance[0] == '*')
	    aprinc.instance[0] = '\0';
	if (aprinc.mod_name[0] == '*')
	    aprinc.mod_name[0] = '\0';
	if (aprinc.mod_instance[0] == '*')
	    aprinc.mod_instance[0] = '\0';
	if (kerb_db_put_principal(&aprinc, 1) != 1) {
	    fprintf(stderr, "Couldn't store %s.%s: %s; load aborted\n",
		    aprinc.name, aprinc.instance,
		    strerror(errno));
	    exit(1);
	};
    }
    if ((code = kerb_db_rename(temp_db_file, db_file)) != 0)
	perror("database rename failed");
    (void) fclose(input_file);
    free(temp_db_file);
}

/*ARGSUSED*/
static void
update_ok_file (char *file_name)
{
    /* handle slave locking/failure stuff */
    char *file_ok;
    int fd;
    static char ok[]=".dump_ok";

    if ((file_ok = (char *)malloc(strlen(file_name) + strlen(ok) + 1))
	== NULL) {
	fprintf(stderr, "kdb_util: out of memory.\n");
	(void) fflush (stderr);
	perror ("malloc");
	exit (1);
    }
    strcpy(file_ok, file_name);
    strcat(file_ok, ok);
    if ((fd = open(file_ok, O_WRONLY|O_CREAT|O_TRUNC, 0400)) < 0) {
	fprintf(stderr, "Error creating 'ok' file, '%s'", file_ok);
	perror("");
	(void) fflush (stderr);
	exit (1);
    }
    free(file_ok);
    close(fd);
}

static void
convert_key_new_master (Principal *p)
{
  des_cblock key;

  /* leave null keys alone */
  if ((p->key_low == 0) && (p->key_high == 0)) return;

  /* move current key to des_cblock for encryption, special case master key
     since that's changing */
  if ((strncmp (p->name, KERB_M_NAME, ANAME_SZ) == 0) &&
      (strncmp (p->instance, KERB_M_INST, INST_SZ) == 0)) {
    bcopy((char *)new_master_key, (char *) key, sizeof (des_cblock));
    (p->key_version)++;
  } else {
    bcopy((char *)&(p->key_low), (char *)key, 4);
    bcopy((char *)&(p->key_high), (char *) (((long *) key) + 1), 4);
    kdb_encrypt_key (&key, &key, &master_key, master_key_schedule, DES_DECRYPT);
  }

  kdb_encrypt_key (&key, &key, &new_master_key, new_master_key_schedule, DES_ENCRYPT);

  bcopy((char *)key, (char *)&(p->key_low), 4);
  bcopy((char *)(((long *) key) + 1), (char *)&(p->key_high), 4);
  bzero((char *)key, sizeof (key));  /* a little paranoia ... */

  (p->kdc_key_ver)++;
}

static void
clear_secrets (void)
{
  bzero((char *)master_key, sizeof (des_cblock));
  bzero((char *)master_key_schedule, sizeof (des_key_schedule));
  bzero((char *)new_master_key, sizeof (des_cblock));
  bzero((char *)new_master_key_schedule, sizeof (des_key_schedule));
}

static void
convert_new_master_key (char *db_file, FILE *out)
{

  printf ("\n\nEnter the CURRENT master key.");
  if (kdb_get_master_key (TRUE, &master_key, master_key_schedule) != 0) {
    fprintf (stderr, "%s: Couldn't get master key.\n", prog);
    clear_secrets ();
    exit (-1);
  }

  if (kdb_verify_master_key (&master_key, master_key_schedule, stderr) < 0) {
    clear_secrets ();
    exit (-1);
  }

  printf ("\n\nNow enter the NEW master key.  Do not forget it!!");
  if (kdb_get_master_key (TRUE, &new_master_key, new_master_key_schedule) != 0) {
    fprintf (stderr, "%s: Couldn't get new master key.\n", prog);
    clear_secrets ();
    exit (-1);
  }

  dump_db (db_file, out, convert_key_new_master);
}

static void
convert_key_old_db (Principal *p)
{
  des_cblock key;

 /* leave null keys alone */
  if ((p->key_low == 0) && (p->key_high == 0)) return;

  bcopy((char *)&(p->key_low), (char *)key, 4);
  bcopy((char *)&(p->key_high), (char *)(((long *) key) + 1), 4);

#ifndef NOENCRYPTION
  des_pcbc_encrypt((des_cblock *)key,(des_cblock *)key,
	(long)sizeof(des_cblock),master_key_schedule,
	(des_cblock *)master_key_schedule, DES_DECRYPT);
#endif

  /* make new key, new style */
  kdb_encrypt_key (&key, &key, &master_key, master_key_schedule, DES_ENCRYPT);

  bcopy((char *)key, (char *)&(p->key_low), 4);
  bcopy((char *)(((long *) key) + 1), (char *)&(p->key_high), 4);
  bzero((char *)key, sizeof (key));  /* a little paranoia ... */
}

static void
convert_old_format_db (char *db_file, FILE *out)
{
  des_cblock key_from_db;
  Principal principal_data[1];
  int n, more;

  if (kdb_get_master_key (TRUE, &master_key, master_key_schedule) != 0L) {
    fprintf (stderr, "%s: Couldn't get master key.\n", prog);
    clear_secrets();
    exit (-1);
  }

  /* can't call kdb_verify_master_key because this is an old style db */
  /* lookup the master key version */
  n = kerb_get_principal(KERB_M_NAME, KERB_M_INST, principal_data,
			 1 /* only one please */, &more);
  if ((n != 1) || more) {
    fprintf(stderr, "verify_master_key: Kerberos error on master key lookup, %d found.\n", n);
    exit (-1);
  }

  /* set up the master key */
  fprintf(stderr, "Current Kerberos master key version is %d.\n",
	  principal_data[0].kdc_key_ver);

  /*
   * now use the master key to decrypt (old style) the key in the db, had better
   * be the same! 
   */
  bcopy((char *)&principal_data[0].key_low, (char *)key_from_db, 4);
  bcopy((char *)&principal_data[0].key_high,
	(char *)(((long *) key_from_db) + 1), 4);
#ifndef NOENCRYPTION
  des_pcbc_encrypt(&key_from_db,&key_from_db,(long)sizeof(key_from_db),
	master_key_schedule,(des_cblock *)master_key_schedule, DES_DECRYPT);
#endif
  /* the decrypted database key had better equal the master key */
  n = bcmp((char *) master_key, (char *) key_from_db,
	   sizeof(master_key));
  bzero((char *)key_from_db, sizeof(key_from_db));

  if (n) {
    fprintf(stderr, "\n\07\07verify_master_key: Invalid master key, ");
    fprintf(stderr, "does not match database.\n");
    exit (-1);
  }
    
  fprintf(stderr, "Master key verified.\n");
  (void) fflush(stderr);

  dump_db (db_file, out, convert_key_old_db);
}

int
main(int argc, char **argv)
{
    FILE   *file;
    enum {
	OP_LOAD,
	OP_DUMP,
	OP_SLAVE_DUMP,
	OP_NEW_MASTER,
	OP_CONVERT_OLD_DB
    }       op;
    char *file_name;
    char *db_name;
    prog = argv[0];
    
    if (argc != 3 && argc != 4) {
	fprintf(stderr, "Usage: %s operation file-name [database name].\n",
		argv[0]);
	exit(1);
    }
    if (argc == 3)
	db_name = DBM_FILE;
    else
	db_name = argv[3];

    if (kerb_db_set_name (db_name) != 0) {
	perror("Can't open database");
	exit(1);
    }
    
    if (!strcmp(argv[1], "load"))
	op = OP_LOAD;
    else if (!strcmp(argv[1], "dump"))
	op = OP_DUMP;
    else if (!strcmp(argv[1], "slave_dump"))
        op = OP_SLAVE_DUMP;
    else if (!strcmp(argv[1], "new_master_key"))
        op = OP_NEW_MASTER;
    else if (!strcmp(argv[1], "convert_old_db"))
        op = OP_CONVERT_OLD_DB;
    else {
	fprintf(stderr,
	    "%s: %s is an invalid operation.\n", prog, argv[1]);
	fprintf(stderr,
	    "%s: Valid operations are \"dump\", \"slave_dump\",", argv[0]);
	fprintf(stderr,
		"\"load\", \"new_master_key\", and \"convert_old_db\".\n");
	exit(1);
    }

    file_name = argv[2];
    file = fopen(file_name, op == OP_LOAD ? "r" : "w");
    if (file == NULL) {
	fprintf(stderr, "%s: Unable to open %s\n", prog, argv[2]);
	(void) fflush(stderr);
	perror("open");
	exit(1);
    }

    switch (op) {
    case OP_DUMP:
      if ((dump_db (db_name, file, (void (*)()) 0) == EOF) ||
	  (fclose(file) == EOF)) {
	  fprintf(stderr, "error on file %s:", file_name);
	  perror("");
	  exit(1);
      }
      break;
    case OP_SLAVE_DUMP:
      if ((dump_db (db_name, file, (void (*)()) 0) == EOF) ||
	  (fclose(file) == EOF)) {
	  fprintf(stderr, "error on file %s:", file_name);
	  perror("");
	  exit(1);
      }
      update_ok_file (file_name);
      break;
    case OP_LOAD:
      load_db (db_name, file);
      break;
    case OP_NEW_MASTER:
      convert_new_master_key (db_name, file);
      printf("Don't forget to do a `kdb_util load %s' to reload the database!\n", file_name);
      break;
    case OP_CONVERT_OLD_DB:
      convert_old_format_db (db_name, file);
      printf("Don't forget to do a `kdb_util load %s' to reload the database!\n", file_name);      
      break;
    }
    exit(0);
  }
