/*	$OpenBSD: setup.c,v 1.9 2003/04/25 21:37:47 deraadt Exp $	*/
/*	$NetBSD: setup.c,v 1.4 1995/04/24 12:24:41 cgd Exp $	*/

/*
 * setup.c - set up all files for Phantasia
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "include.h"
#include <fcntl.h>

void Error(char *, char *);

/**/
/************************************************************************
/
/ FUNCTION NAME: main()
/
/ FUNCTION: setup files for Phantasia 3.3.2
/
/ AUTHOR: E. A. Estes, 12/4/85
/
/ ARGUMENTS: none
/
/ RETURN VALUE: none
/
/ MODULES CALLED: time(), exit(), stat(), Error(), open(), close(), fopen(), 
/	fgets(), floor(), srandomdev(), umask(), strlcpy(),
/	unlink(), fwrite(), fclose(), sscanf(), printf(), strlen(), fprintf()
/
/ GLOBAL INPUTS: Curmonster, _iob[], Databuf[], *Monstfp, Enrgyvoid
/
/ GLOBAL OUTPUTS: Curmonster, Databuf[], *Monstfp, Enrgyvoid
/
/ DESCRIPTION: 
/
/	This program tries to verify the parameters specified in
/	the Makefile.
/
/	Create all necessary files.  Note that nothing needs to be
/	put in these files.
/	Also, the monster binary data base is created here.
/
*************************************************************************/

static char *files[] = {		/* all files to create */
	_PATH_MONST,
	_PATH_PEOPLE,
	_PATH_MESS,
	_PATH_LASTDEAD,
	_PATH_MOTD,
	_PATH_GOLD,
	_PATH_VOID,
	_PATH_SCORE,
	NULL,
};

char *monsterfile="monsters.asc";

int
main(argc, argv)
	int argc;
	char *argv[];
{
	char	**filename;	/* for pointing to file names */
	int	fd;		/* file descriptor */
	FILE	*fp;			/* for opening files */
	struct stat	fbuf;		/* for getting files statistics */
	int ch;
	char path[MAXPATHLEN], *prefix;

	while ((ch = getopt(argc, argv, "m:")) != -1)
		switch(ch) {
		case 'm':
			monsterfile = optarg;
			break;
		case '?':
		default:
			break;
		}
	argc -= optind;
	argv += optind;

    srandomdev();	/* prime random numbers */

    umask(0117);		/* only owner can read/write created files */

    prefix = getenv("DESTDIR");

    /* try to create data files */
    filename = &files[0];
    while (*filename != NULL)
	/* create each file */
	{
	snprintf(path, sizeof(path), "%s%s", prefix?prefix:"", *filename);
	if (stat(path, &fbuf) == 0)
	    /* file exists; remove it */
	    {
	    if (!strcmp(*filename, _PATH_PEOPLE))
		/* do not reset character file if it already exists */
		{
		++filename;
		continue;
		}

	    if (!strcmp(*filename, _PATH_SCORE))
		/* do not reset score file if it already exists */
		{
		++filename;
		continue;
		}

	    if (unlink(path) < 0)
		Error("Cannot unlink %s.\n", path);
		/*NOTREACHED*/
	    }

	if ((fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0660)) < 0)
	    Error("Cannot create %s.\n", path);
	    /*NOTREACHED*/

	close(fd);			/* close newly created file */

	++filename;			/* process next file */
	}

    /* put holy grail info into energy void file */
    Enrgyvoid.ev_active = TRUE;
    Enrgyvoid.ev_x = ROLL(-1.0e6, 2.0e6);
    Enrgyvoid.ev_y = ROLL(-1.0e6, 2.0e6);
    snprintf(path, sizeof(path), "%s%s", prefix?prefix:"", _PATH_VOID);
    if ((fp = fopen(path, "w")) == NULL)
	Error("Cannot update %s.\n", path);
    else
	{
	fwrite(&Enrgyvoid, SZ_VOIDSTRUCT, 1, fp);
	fclose(fp);
	}

    /* create binary monster data base */
    snprintf(path, sizeof(path), "%s%s", prefix?prefix:"", _PATH_MONST);
    if ((Monstfp = fopen(path, "w")) == NULL)
	Error("Cannot update %s.\n", path);
    else
	{
	if ((fp = fopen(monsterfile, "r")) == NULL)
	    {
	    fclose(Monstfp);
	    Error("cannot open %s to create monster database.\n", "monsters.asc");
	    }
	else
	    {
	    Curmonster.m_o_strength =
	    Curmonster.m_o_speed =
	    Curmonster.m_maxspeed =
	    Curmonster.m_o_energy =
	    Curmonster.m_melee =
	    Curmonster.m_skirmish = 0.0;

	    while (fgets(Databuf, SZ_DATABUF, fp) != NULL)
		/* read in text file, convert to binary */
		{
		sscanf(&Databuf[24], "%lf%lf%lf%lf%lf%d%d%lf",
		    &Curmonster.m_strength, &Curmonster.m_brains,
		    &Curmonster.m_speed, &Curmonster.m_energy,
		    &Curmonster.m_experience, &Curmonster.m_treasuretype,
		    &Curmonster.m_type, &Curmonster.m_flock);
		Databuf[24] = '\0';
		strlcpy(Curmonster.m_name, Databuf, sizeof Curmonster.m_name);
		fwrite((char *) &Curmonster, SZ_MONSTERSTRUCT, 1, Monstfp);
		}
	    fclose(fp);
	    fclose(Monstfp);
	    }
	}

#ifdef MAKE_INSTALLS_THIS_AND_DOESNT_WANT_TO_HEAR_ABOUT_IT
    /* write to motd file */
    printf("One line 'motd' ? ");
    if (fgets(Databuf, SZ_DATABUF, stdin) == NULL)
	Databuf[0] = '\0';
    snprintf(path, sizeof(path), "%s%s", prefix?prefix:"", _PATH_MOTD);
    if ((fp = fopen(path, "w")) == NULL)
	Error("Cannot update %s.\n", path);
    else
	{
	fwrite(Databuf, sizeof(char), strlen(Databuf), fp);
	fclose(fp);
	}

    /* report compile-time options */
    printf("Compiled options:\n\n");
    printf("Phantasia destination directory:  %s\n", _PATH_PHANTDIR);
    printf("Wizard: root UID: 0\n");

#ifdef BSD41
    printf("Compiled for BSD 4.1\n");
#endif

#ifdef BSD42
    printf("Compiled for BSD 4.2\n");
#endif

#ifdef SYS3
    printf("Compiled for System III\n");
#endif

#ifdef SYS5
    printf("Compiled for System V\n");
#endif
#endif

    exit(0);
    /*NOTREACHED*/
}
/**/
/************************************************************************
/
/ FUNCTION NAME: Error()
/
/ FUNCTION: print an error message, and exit
/
/ AUTHOR: E. A. Estes, 12/4/85
/
/ ARGUMENTS:
/	char *str - format string for printf()
/	char *file - file which caused error
/
/ RETURN VALUE: none
/
/ MODULES CALLED: exit(), perror(), fprintf()
/
/ GLOBAL INPUTS: _iob[]
/
/ GLOBAL OUTPUTS: none
/
/ DESCRIPTION:
/	Print an error message, then exit.
/
*************************************************************************/

void
Error(str, file)
	char	*str, *file;
{
	fprintf(stderr, "Error: ");
	fprintf(stderr, str, file);
	perror(file);
	exit(1);
	/* NOTREACHED */
}
/**/
/************************************************************************
/
/ FUNCTION NAME: drandom()
/
/ FUNCTION: return a random number
/
/ AUTHOR: E. A. Estes, 2/7/86
/
/ ARGUMENTS: none
/
/ RETURN VALUE: none
/
/ MODULES CALLED: random()
/
/ GLOBAL INPUTS: none
/
/ GLOBAL OUTPUTS: none
/
/ DESCRIPTION: 
/
*************************************************************************/

double
drandom()
{
    if (sizeof(int) != 2)
	return((double) (random() & 0x7fff) / 32768.0);
    else
	return((double) random() / 32768.0);
}
