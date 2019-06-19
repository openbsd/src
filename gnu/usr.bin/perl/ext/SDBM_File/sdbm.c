/*
 * sdbm - ndbm work-alike hashed database library
 * based on Per-Aake Larson's Dynamic Hashing algorithms. BIT 18 (1978).
 * author: oz@nexus.yorku.ca
 * status: public domain.
 *
 * core routines
 */

#include "INTERN.h"
#include "config.h"
#ifdef WIN32
#include "io.h"
#endif
#include "sdbm.h"
#include "tune.h"
#include "pair.h"

#ifdef I_FCNTL
# include <fcntl.h>
#endif
#ifdef I_SYS_FILE
# include <sys/file.h>
#endif

#include <string.h>

/*
 * externals
 */

#include <errno.h> /* See notes in perl.h about avoiding
			extern int errno; */
#ifdef __cplusplus
extern "C" {
#endif

extern Malloc_t malloc(MEM_SIZE);
extern Free_t free(Malloc_t);

#ifdef __cplusplus
}
#endif

const datum nullitem = {0, 0};

/*
 * forward
 */
static int getdbit(DBM *, long);
static int setdbit(DBM *, long);
static int getpage(DBM *, long);
static datum getnext(DBM *);
static int makroom(DBM *, long, int);

/*
 * useful macros
 */
#define bad(x)		((x).dptr == NULL || (x).dsize < 0)
#define exhash(item)	sdbm_hash((item).dptr, (item).dsize)
#define ioerr(db)	((db)->flags |= DBM_IOERR)

#define OFF_PAG(off)	(long) (off) * PBLKSIZ
#define OFF_DIR(off)	(long) (off) * DBLKSIZ

static const long masks[] = {
	000000000000, 000000000001, 000000000003, 000000000007,
	000000000017, 000000000037, 000000000077, 000000000177,
	000000000377, 000000000777, 000000001777, 000000003777,
	000000007777, 000000017777, 000000037777, 000000077777,
	000000177777, 000000377777, 000000777777, 000001777777,
	000003777777, 000007777777, 000017777777, 000037777777,
	000077777777, 000177777777, 000377777777, 000777777777,
	001777777777, 003777777777, 007777777777, 017777777777
};

DBM *
sdbm_open(char *file, int flags, int mode)
{
	DBM *db;
	char *dirname;
	char *pagname;
	size_t filelen;
	const size_t dirfext_size = sizeof(DIRFEXT "");
	const size_t pagfext_size = sizeof(PAGFEXT "");

	if (file == NULL || !*file)
		return errno = EINVAL, (DBM *) NULL;
/*
 * need space for two separate filenames
 */
	filelen = strlen(file);

	if ((dirname = (char *) malloc(filelen + dirfext_size
				       + filelen + pagfext_size)) == NULL)
		return errno = ENOMEM, (DBM *) NULL;
/*
 * build the file names
 */
	memcpy(dirname, file, filelen);
	memcpy(dirname + filelen, DIRFEXT, dirfext_size);
	pagname = dirname + filelen + dirfext_size;
	memcpy(pagname, file, filelen);
	memcpy(pagname + filelen, PAGFEXT, pagfext_size);

	db = sdbm_prep(dirname, pagname, flags, mode);
	free((char *) dirname);
	return db;
}

DBM *
sdbm_prep(char *dirname, char *pagname, int flags, int mode)
{
	DBM *db;
	struct stat dstat;

	if ((db = (DBM *) malloc(sizeof(DBM))) == NULL)
		return errno = ENOMEM, (DBM *) NULL;

        db->flags = 0;
        db->hmask = 0;
        db->blkptr = 0;
        db->keyptr = 0;
/*
 * adjust user flags so that WRONLY becomes RDWR, 
 * as required by this package. Also set our internal
 * flag for RDONLY if needed.
 */
	if (flags & O_WRONLY)
		flags = (flags & ~O_WRONLY) | O_RDWR;

	else if ((flags & 03) == O_RDONLY)
		db->flags = DBM_RDONLY;
/*
 * open the files in sequence, and stat the dirfile.
 * If we fail anywhere, undo everything, return NULL.
 */
#if defined(OS2) || defined(MSDOS) || defined(WIN32) || defined(__CYGWIN__)
	flags |= O_BINARY;
#	endif
	if ((db->pagf = open(pagname, flags, mode)) > -1) {
		if ((db->dirf = open(dirname, flags, mode)) > -1) {
/*
 * need the dirfile size to establish max bit number.
 */
			if (fstat(db->dirf, &dstat) == 0) {
/*
 * zero size: either a fresh database, or one with a single,
 * unsplit data page: dirpage is all zeros.
 */
				db->dirbno = (!dstat.st_size) ? 0 : -1;
				db->pagbno = -1;
				db->maxbno = dstat.st_size * BYTESIZ;

				(void) memset(db->pagbuf, 0, PBLKSIZ);
				(void) memset(db->dirbuf, 0, DBLKSIZ);
			/*
			 * success
			 */
				return db;
			}
			(void) close(db->dirf);
		}
		(void) close(db->pagf);
	}
	free((char *) db);
	return (DBM *) NULL;
}

void
sdbm_close(DBM *db)
{
	if (db == NULL)
		errno = EINVAL;
	else {
		(void) close(db->dirf);
		(void) close(db->pagf);
		free((char *) db);
	}
}

datum
sdbm_fetch(DBM *db, datum key)
{
	if (db == NULL || bad(key))
		return errno = EINVAL, nullitem;

	if (getpage(db, exhash(key)))
		return getpair(db->pagbuf, key);

	return ioerr(db), nullitem;
}

int
sdbm_exists(DBM *db, datum key)
{
	if (db == NULL || bad(key))
		return errno = EINVAL, -1;

	if (getpage(db, exhash(key)))
		return exipair(db->pagbuf, key);

	return ioerr(db), -1;
}

int
sdbm_delete(DBM *db, datum key)
{
	if (db == NULL || bad(key))
		return errno = EINVAL, -1;
	if (sdbm_rdonly(db))
		return errno = EPERM, -1;

	if (getpage(db, exhash(key))) {
		if (!delpair(db->pagbuf, key))
			return -1;
/*
 * update the page file
 */
		if (lseek(db->pagf, OFF_PAG(db->pagbno), SEEK_SET) < 0
		    || write(db->pagf, db->pagbuf, PBLKSIZ) < 0)
			return ioerr(db), -1;

		return 0;
	}

	return ioerr(db), -1;
}

int
sdbm_store(DBM *db, datum key, datum val, int flags)
{
	int need;
	long hash;

	if (db == NULL || bad(key))
		return errno = EINVAL, -1;
	if (sdbm_rdonly(db))
		return errno = EPERM, -1;

	need = key.dsize + val.dsize;
/*
 * is the pair too big (or too small) for this database ??
 */
	if (need < 0 || need > PAIRMAX)
		return errno = EINVAL, -1;

	if (getpage(db, (hash = exhash(key)))) {
/*
 * if we need to replace, delete the key/data pair
 * first. If it is not there, ignore.
 */
		if (flags == DBM_REPLACE)
			(void) delpair(db->pagbuf, key);
#ifdef SEEDUPS
		else if (duppair(db->pagbuf, key))
			return 1;
#endif
/*
 * if we do not have enough room, we have to split.
 */
		if (!fitpair(db->pagbuf, need))
			if (!makroom(db, hash, need))
				return ioerr(db), -1;
/*
 * we have enough room or split is successful. insert the key,
 * and update the page file.
 */
		(void) putpair(db->pagbuf, key, val);

		if (lseek(db->pagf, OFF_PAG(db->pagbno), SEEK_SET) < 0
		    || write(db->pagf, db->pagbuf, PBLKSIZ) < 0)
			return ioerr(db), -1;
	/*
	 * success
	 */
		return 0;
	}

	return ioerr(db), -1;
}

/*
 * makroom - make room by splitting the overfull page
 * this routine will attempt to make room for SPLTMAX times before
 * giving up.
 */
static int
makroom(DBM *db, long int hash, int need)
{
	long newp;
	char twin[PBLKSIZ];
#if defined(DOSISH) || defined(WIN32)
	char zer[PBLKSIZ];
	long oldtail;
#endif
	char *pag = db->pagbuf;
	char *New = twin;
	int smax = SPLTMAX;
#ifdef BADMESS
	int rc;
#endif

	do {
/*
 * split the current page
 */
		(void) splpage(pag, New, db->hmask + 1);
/*
 * address of the new page
 */
		newp = (hash & db->hmask) | (db->hmask + 1);

/*
 * write delay, read avoidance/cache shuffle:
 * select the page for incoming pair: if key is to go to the new page,
 * write out the previous one, and copy the new one over, thus making
 * it the current page. If not, simply write the new page, and we are
 * still looking at the page of interest. current page is not updated
 * here, as sdbm_store will do so, after it inserts the incoming pair.
 */

#if defined(DOSISH) || defined(WIN32)
		/*
		 * Fill hole with 0 if made it.
		 * (hole is NOT read as 0)
		 */
		oldtail = lseek(db->pagf, 0L, SEEK_END);
		memset(zer, 0, PBLKSIZ);
		while (OFF_PAG(newp) > oldtail) {
			if (lseek(db->pagf, 0L, SEEK_END) < 0 ||
			    write(db->pagf, zer, PBLKSIZ) < 0) {

				return 0;
			}
			oldtail += PBLKSIZ;
		}
#endif
		if (hash & (db->hmask + 1)) {
			if (lseek(db->pagf, OFF_PAG(db->pagbno), SEEK_SET) < 0
			    || write(db->pagf, db->pagbuf, PBLKSIZ) < 0)
				return 0;
			db->pagbno = newp;
			(void) memcpy(pag, New, PBLKSIZ);
		}
		else if (lseek(db->pagf, OFF_PAG(newp), SEEK_SET) < 0
			 || write(db->pagf, New, PBLKSIZ) < 0)
			return 0;

		if (!setdbit(db, db->curbit))
			return 0;
/*
 * see if we have enough room now
 */
		if (fitpair(pag, need))
			return 1;
/*
 * try again... update curbit and hmask as getpage would have
 * done. because of our update of the current page, we do not
 * need to read in anything. BUT we have to write the current
 * [deferred] page out, as the window of failure is too great.
 */
		db->curbit = 2 * db->curbit +
			((hash & (db->hmask + 1)) ? 2 : 1);
		db->hmask |= db->hmask + 1;

		if (lseek(db->pagf, OFF_PAG(db->pagbno), SEEK_SET) < 0
		    || write(db->pagf, db->pagbuf, PBLKSIZ) < 0)
			return 0;

	} while (--smax);
/*
 * if we are here, this is real bad news. After SPLTMAX splits,
 * we still cannot fit the key. say goodnight.
 */
#ifdef BADMESS
	rc = write(2, "sdbm: cannot insert after SPLTMAX attempts.\n", 44);
	/* PERL_UNUSED_VAR() or PERL_UNUSED_RESULT() would be
	 * useful here but that would mean pulling in perl.h */
	(void)rc;
#endif
	return 0;

}

/*
 * the following two routines will break if
 * deletions aren't taken into account. (ndbm bug)
 */
datum
sdbm_firstkey(DBM *db)
{
	if (db == NULL)
		return errno = EINVAL, nullitem;
/*
 * start at page 0
 */
	if (lseek(db->pagf, OFF_PAG(0), SEEK_SET) < 0
	    || read(db->pagf, db->pagbuf, PBLKSIZ) < 0)
		return ioerr(db), nullitem;
	db->pagbno = 0;
	db->blkptr = 0;
	db->keyptr = 0;

	return getnext(db);
}

datum
sdbm_nextkey(DBM *db)
{
	if (db == NULL)
		return errno = EINVAL, nullitem;
	return getnext(db);
}

/*
 * all important binary trie traversal
 */
static int
getpage(DBM *db, long int hash)
{
	int hbit;
	long dbit;
	long pagb;

	dbit = 0;
	hbit = 0;
	while (dbit < db->maxbno && getdbit(db, dbit))
		dbit = 2 * dbit + ((hash & (1 << hbit++)) ? 2 : 1);

	debug(("dbit: %d...", dbit));

	db->curbit = dbit;
	db->hmask = masks[hbit];

	pagb = hash & db->hmask;
/*
 * see if the block we need is already in memory.
 * note: this lookaside cache has about 10% hit rate.
 */
	if (pagb != db->pagbno) { 
/*
 * note: here, we assume a "hole" is read as 0s.
 * if not, must zero pagbuf first.
 */
		if (lseek(db->pagf, OFF_PAG(pagb), SEEK_SET) < 0
		    || read(db->pagf, db->pagbuf, PBLKSIZ) < 0)
			return 0;
		if (!chkpage(db->pagbuf))
			return 0;
		db->pagbno = pagb;

		debug(("pag read: %d\n", pagb));
	}
	return 1;
}

static int
getdbit(DBM *db, long int dbit)
{
	long c;
	long dirb;

	c = dbit / BYTESIZ;
	dirb = c / DBLKSIZ;

	if (dirb != db->dirbno) {
		int got;
		if (lseek(db->dirf, OFF_DIR(dirb), SEEK_SET) < 0
		    || (got=read(db->dirf, db->dirbuf, DBLKSIZ)) < 0)
			return 0;
		if (got==0) 
			memset(db->dirbuf,0,DBLKSIZ);
		db->dirbno = dirb;

		debug(("dir read: %d\n", dirb));
	}

	return db->dirbuf[c % DBLKSIZ] & (1 << dbit % BYTESIZ);
}

static int
setdbit(DBM *db, long int dbit)
{
	long c;
	long dirb;

	c = dbit / BYTESIZ;
	dirb = c / DBLKSIZ;

	if (dirb != db->dirbno) {
		int got;
		if (lseek(db->dirf, OFF_DIR(dirb), SEEK_SET) < 0
		    || (got=read(db->dirf, db->dirbuf, DBLKSIZ)) < 0)
			return 0;
		if (got==0) 
			memset(db->dirbuf,0,DBLKSIZ);
		db->dirbno = dirb;

		debug(("dir read: %d\n", dirb));
	}

	db->dirbuf[c % DBLKSIZ] |= (1 << dbit % BYTESIZ);

#if 0
	if (dbit >= db->maxbno)
		db->maxbno += DBLKSIZ * BYTESIZ;
#else
	if (OFF_DIR((dirb+1))*BYTESIZ > db->maxbno) 
		db->maxbno=OFF_DIR((dirb+1))*BYTESIZ;
#endif

	if (lseek(db->dirf, OFF_DIR(dirb), SEEK_SET) < 0
	    || write(db->dirf, db->dirbuf, DBLKSIZ) < 0)
		return 0;

	return 1;
}

/*
 * getnext - get the next key in the page, and if done with
 * the page, try the next page in sequence
 */
static datum
getnext(DBM *db)
{
	datum key;

	for (;;) {
		db->keyptr++;
		key = getnkey(db->pagbuf, db->keyptr);
		if (key.dptr != NULL)
			return key;
/*
 * we either run out, or there is nothing on this page..
 * try the next one... If we lost our position on the
 * file, we will have to seek.
 */
		db->keyptr = 0;
		if (db->pagbno != db->blkptr++)
			if (lseek(db->pagf, OFF_PAG(db->blkptr), SEEK_SET) < 0)
				break;
		db->pagbno = db->blkptr;
		if (read(db->pagf, db->pagbuf, PBLKSIZ) <= 0)
			break;
		if (!chkpage(db->pagbuf))
			break;
	}

	return ioerr(db), nullitem;
}

