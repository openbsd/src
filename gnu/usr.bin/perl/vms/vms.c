/* vms.c
 *
 * VMS-specific routines for perl5
 * Version: 5.7.0
 *
 * August 2000 tweaks to vms_image_init, my_flush, my_fwrite, cando_by_name, 
 *             and Perl_cando by Craig Berry
 * 29-Aug-2000 Charles Lane's piping improvements rolled in
 * 20-Aug-1999 revisions by Charles Bailey  bailey@newman.upenn.edu
 */

#include <acedef.h>
#include <acldef.h>
#include <armdef.h>
#include <atrdef.h>
#include <chpdef.h>
#include <clidef.h>
#include <climsgdef.h>
#include <descrip.h>
#include <devdef.h>
#include <dvidef.h>
#include <fibdef.h>
#include <float.h>
#include <fscndef.h>
#include <iodef.h>
#include <jpidef.h>
#include <kgbdef.h>
#include <libclidef.h>
#include <libdef.h>
#include <lib$routines.h>
#include <lnmdef.h>
#include <msgdef.h>
#include <prvdef.h>
#include <psldef.h>
#include <rms.h>
#include <shrdef.h>
#include <ssdef.h>
#include <starlet.h>
#include <strdef.h>
#include <str$routines.h>
#include <syidef.h>
#include <uaidef.h>
#include <uicdef.h>

/* Older versions of ssdef.h don't have these */
#ifndef SS$_INVFILFOROP
#  define SS$_INVFILFOROP 3930
#endif
#ifndef SS$_NOSUCHOBJECT
#  define SS$_NOSUCHOBJECT 2696
#endif

/* We implement I/O here, so we will be mixing PerlIO and stdio calls. */
#define PERLIO_NOT_STDIO 0 

/* Don't replace system definitions of vfork, getenv, and stat, 
 * code below needs to get to the underlying CRTL routines. */
#define DONT_MASK_RTL_CALLS
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
/* Anticipating future expansion in lexical warnings . . . */
#ifndef WARN_INTERNAL
#  define WARN_INTERNAL WARN_MISC
#endif

#if defined(__VMS_VER) && __VMS_VER >= 70000000 && __DECC_VER >= 50200000
#  define RTL_USES_UTC 1
#endif


/* gcc's header files don't #define direct access macros
 * corresponding to VAXC's variant structs */
#ifdef __GNUC__
#  define uic$v_format uic$r_uic_form.uic$v_format
#  define uic$v_group uic$r_uic_form.uic$v_group
#  define uic$v_member uic$r_uic_form.uic$v_member
#  define prv$v_bypass  prv$r_prvdef_bits0.prv$v_bypass
#  define prv$v_grpprv  prv$r_prvdef_bits0.prv$v_grpprv
#  define prv$v_readall prv$r_prvdef_bits0.prv$v_readall
#  define prv$v_sysprv  prv$r_prvdef_bits0.prv$v_sysprv
#endif

#if defined(NEED_AN_H_ERRNO)
dEXT int h_errno;
#endif

struct itmlst_3 {
  unsigned short int buflen;
  unsigned short int itmcode;
  void *bufadr;
  unsigned short int *retlen;
};

#define do_fileify_dirspec(a,b,c)	mp_do_fileify_dirspec(aTHX_ a,b,c)
#define do_pathify_dirspec(a,b,c)	mp_do_pathify_dirspec(aTHX_ a,b,c)
#define do_tovmsspec(a,b,c)		mp_do_tovmsspec(aTHX_ a,b,c)
#define do_tovmspath(a,b,c)		mp_do_tovmspath(aTHX_ a,b,c)
#define do_rmsexpand(a,b,c,d,e)		mp_do_rmsexpand(aTHX_ a,b,c,d,e)
#define do_tounixspec(a,b,c)		mp_do_tounixspec(aTHX_ a,b,c)
#define do_tounixpath(a,b,c)		mp_do_tounixpath(aTHX_ a,b,c)
#define expand_wild_cards(a,b,c,d)	mp_expand_wild_cards(aTHX_ a,b,c,d)
#define getredirection(a,b)		mp_getredirection(aTHX_ a,b)

/* see system service docs for $TRNLNM -- NOT the same as LNM$_MAX_INDEX */
#define PERL_LNM_MAX_ALLOWED_INDEX 127

/* OpenVMS User's Guide says at least 9 iterative translations will be performed,
 * depending on the facility.  SHOW LOGICAL does 10, so we'll imitate that for
 * the Perl facility.
 */
#define PERL_LNM_MAX_ITER 10

#define MAX_DCL_SYMBOL              255     /* well, what *we* can set, at least*/
#define MAX_DCL_LINE_LENGTH        (4*MAX_DCL_SYMBOL-4)

static char *__mystrtolower(char *str)
{
  if (str) for (; *str; ++str) *str= tolower(*str);
  return str;
}

static struct dsc$descriptor_s fildevdsc = 
  { 12, DSC$K_DTYPE_T, DSC$K_CLASS_S, "LNM$FILE_DEV" };
static struct dsc$descriptor_s crtlenvdsc = 
  { 8, DSC$K_DTYPE_T, DSC$K_CLASS_S, "CRTL_ENV" };
static struct dsc$descriptor_s *fildev[] = { &fildevdsc, NULL };
static struct dsc$descriptor_s *defenv[] = { &fildevdsc, &crtlenvdsc, NULL };
static struct dsc$descriptor_s **env_tables = defenv;
static bool will_taint = FALSE;  /* tainting active, but no PL_curinterp yet */

/* True if we shouldn't treat barewords as logicals during directory */
/* munching */ 
static int no_translate_barewords;

#ifndef RTL_USES_UTC
static int tz_updated = 1;
#endif

/* my_maxidx
 * Routine to retrieve the maximum equivalence index for an input
 * logical name.  Some calls to this routine have no knowledge if
 * the variable is a logical or not.  So on error we return a max
 * index of zero.
 */
/*{{{int my_maxidx(char *lnm) */
static int
my_maxidx(char *lnm)
{
    int status;
    int midx;
    int attr = LNM$M_CASE_BLIND;
    struct dsc$descriptor lnmdsc;
    struct itmlst_3 itlst[2] = {{sizeof(midx), LNM$_MAX_INDEX, &midx, 0},
                                {0, 0, 0, 0}};

    lnmdsc.dsc$w_length = strlen(lnm);
    lnmdsc.dsc$b_dtype = DSC$K_DTYPE_T;
    lnmdsc.dsc$b_class = DSC$K_CLASS_S;
    lnmdsc.dsc$a_pointer = lnm;

    status = sys$trnlnm(&attr, &fildevdsc, &lnmdsc, 0, itlst);
    if ((status & 1) == 0)
       midx = 0;

    return (midx);
}
/*}}}*/

/*{{{int vmstrnenv(const char *lnm, char *eqv, unsigned long int idx, struct dsc$descriptor_s **tabvec, unsigned long int flags) */
int
Perl_vmstrnenv(const char *lnm, char *eqv, unsigned long int idx,
  struct dsc$descriptor_s **tabvec, unsigned long int flags)
{
    char uplnm[LNM$C_NAMLENGTH+1], *cp1, *cp2;
    unsigned short int eqvlen, curtab, ivlnm = 0, ivsym = 0, ivenv = 0, secure;
    unsigned long int retsts, attr = LNM$M_CASE_BLIND;
    int midx;
    unsigned char acmode;
    struct dsc$descriptor_s lnmdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0},
                            tmpdsc = {6,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
    struct itmlst_3 lnmlst[3] = {{sizeof idx, LNM$_INDEX, &idx, 0},
                                 {LNM$C_NAMLENGTH, LNM$_STRING, eqv, &eqvlen},
                                 {0, 0, 0, 0}};
    $DESCRIPTOR(crtlenv,"CRTL_ENV");  $DESCRIPTOR(clisym,"CLISYM");
#if defined(PERL_IMPLICIT_CONTEXT)
    pTHX = NULL;
#  if defined(USE_5005THREADS)
    /* We jump through these hoops because we can be called at */
    /* platform-specific initialization time, which is before anything is */
    /* set up--we can't even do a plain dTHX since that relies on the */
    /* interpreter structure to be initialized */
    if (PL_curinterp) {
      aTHX = PL_threadnum? THR : (struct perl_thread*)SvPVX(PL_thrsv);
    } else {
      aTHX = NULL;
    }
# else
    if (PL_curinterp) {
      aTHX = PERL_GET_INTERP;
    } else {
      aTHX = NULL;
    }

#  endif
#endif

    if (!lnm || !eqv || ((idx != 0) && ((idx-1) > PERL_LNM_MAX_ALLOWED_INDEX))) {
      set_errno(EINVAL); set_vaxc_errno(SS$_BADPARAM); return 0;
    }
    for (cp1 = (char *)lnm, cp2 = uplnm; *cp1; cp1++, cp2++) {
      *cp2 = _toupper(*cp1);
      if (cp1 - lnm > LNM$C_NAMLENGTH) {
        set_errno(EINVAL); set_vaxc_errno(SS$_IVLOGNAM);
        return 0;
      }
    }
    lnmdsc.dsc$w_length = cp1 - lnm;
    lnmdsc.dsc$a_pointer = uplnm;
    uplnm[lnmdsc.dsc$w_length] = '\0';
    secure = flags & PERL__TRNENV_SECURE;
    acmode = secure ? PSL$C_EXEC : PSL$C_USER;
    if (!tabvec || !*tabvec) tabvec = env_tables;

    for (curtab = 0; tabvec[curtab]; curtab++) {
      if (!str$case_blind_compare(tabvec[curtab],&crtlenv)) {
        if (!ivenv && !secure) {
          char *eq, *end;
          int i;
          if (!environ) {
            ivenv = 1; 
            Perl_warn(aTHX_ "Can't read CRTL environ\n");
            continue;
          }
          retsts = SS$_NOLOGNAM;
          for (i = 0; environ[i]; i++) { 
            if ((eq = strchr(environ[i],'=')) && 
                lnmdsc.dsc$w_length == (eq - environ[i]) &&
                !strncmp(environ[i],uplnm,eq - environ[i])) {
              eq++;
              for (eqvlen = 0; eq[eqvlen]; eqvlen++) eqv[eqvlen] = eq[eqvlen];
              if (!eqvlen) continue;
              retsts = SS$_NORMAL;
              break;
            }
          }
          if (retsts != SS$_NOLOGNAM) break;
        }
      }
      else if ((tmpdsc.dsc$a_pointer = tabvec[curtab]->dsc$a_pointer) &&
               !str$case_blind_compare(&tmpdsc,&clisym)) {
        if (!ivsym && !secure) {
          unsigned short int deflen = LNM$C_NAMLENGTH;
          struct dsc$descriptor_d eqvdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_D,0};
          /* dynamic dsc to accomodate possible long value */
          _ckvmssts(lib$sget1_dd(&deflen,&eqvdsc));
          retsts = lib$get_symbol(&lnmdsc,&eqvdsc,&eqvlen,0);
          if (retsts & 1) { 
            if (eqvlen > 1024) {
              set_errno(EVMSERR); set_vaxc_errno(LIB$_STRTRU);
              eqvlen = 1024;
	      /* Special hack--we might be called before the interpreter's */
	      /* fully initialized, in which case either thr or PL_curcop */
	      /* might be bogus. We have to check, since ckWARN needs them */
	      /* both to be valid if running threaded */
#if defined(USE_5005THREADS)
	      if (thr && PL_curcop) {
#endif
		if (ckWARN(WARN_MISC)) {
		  Perl_warner(aTHX_ packWARN(WARN_MISC),"Value of CLI symbol \"%s\" too long",lnm);
		}
#if defined(USE_5005THREADS)
	      } else {
		  Perl_warner(aTHX_ packWARN(WARN_MISC),"Value of CLI symbol \"%s\" too long",lnm);
	      }
#endif
	      
            }
            strncpy(eqv,eqvdsc.dsc$a_pointer,eqvlen);
          }
          _ckvmssts(lib$sfree1_dd(&eqvdsc));
          if (retsts == LIB$_INVSYMNAM) { ivsym = 1; continue; }
          if (retsts == LIB$_NOSUCHSYM) continue;
          break;
        }
      }
      else if (!ivlnm) {
        if ( (idx == 0) && (flags & PERL__TRNENV_JOIN_SEARCHLIST) ) {
          midx = my_maxidx((char *) lnm);
          for (idx = 0, cp1 = eqv; idx <= midx; idx++) {
            lnmlst[1].bufadr = cp1;
            eqvlen = 0;
            retsts = sys$trnlnm(&attr,tabvec[curtab],&lnmdsc,&acmode,lnmlst);
            if (retsts == SS$_IVLOGNAM) { ivlnm = 1; break; }
            if (retsts == SS$_NOLOGNAM) break;
            /* PPFs have a prefix */
            if (
#if INTSIZE == 4
                 *((int *)uplnm) == *((int *)"SYS$")                    &&
#endif
                 eqvlen >= 4 && eqv[0] == 0x1b && eqv[1] == 0x00        &&
                 ( (uplnm[4] == 'O' && !strcmp(uplnm,"SYS$OUTPUT"))  ||
                   (uplnm[4] == 'I' && !strcmp(uplnm,"SYS$INPUT"))   ||
                   (uplnm[4] == 'E' && !strcmp(uplnm,"SYS$ERROR"))   ||
                   (uplnm[4] == 'C' && !strcmp(uplnm,"SYS$COMMAND")) )  ) {
              memcpy(eqv,eqv+4,eqvlen-4);
              eqvlen -= 4;
            }
            cp1 += eqvlen;
            *cp1 = '\0';
          }
          if ((retsts == SS$_IVLOGNAM) ||
              (retsts == SS$_NOLOGNAM)) { continue; }
        }
        else {
          retsts = sys$trnlnm(&attr,tabvec[curtab],&lnmdsc,&acmode,lnmlst);
          if (retsts == SS$_IVLOGNAM) { ivlnm = 1; continue; }
          if (retsts == SS$_NOLOGNAM) continue;
          eqv[eqvlen] = '\0';
        }
        eqvlen = strlen(eqv);
        break;
      }
    }
    if (retsts & 1) { eqv[eqvlen] = '\0'; return eqvlen; }
    else if (retsts == LIB$_NOSUCHSYM || retsts == LIB$_INVSYMNAM ||
             retsts == SS$_IVLOGNAM   || retsts == SS$_IVLOGTAB   ||
             retsts == SS$_NOLOGNAM) {
      set_errno(EINVAL);  set_vaxc_errno(retsts);
    }
    else _ckvmssts(retsts);
    return 0;
}  /* end of vmstrnenv */
/*}}}*/

/*{{{ int my_trnlnm(const char *lnm, char *eqv, unsigned long int idx)*/
/* Define as a function so we can access statics. */
int Perl_my_trnlnm(pTHX_ const char *lnm, char *eqv, unsigned long int idx)
{
  return vmstrnenv(lnm,eqv,idx,fildev,                                   
#ifdef SECURE_INTERNAL_GETENV
                   (PL_curinterp ? PL_tainting : will_taint) ? PERL__TRNENV_SECURE : 0
#else
                   0
#endif
                                                                              );
}
/*}}}*/

/* my_getenv
 * Note: Uses Perl temp to store result so char * can be returned to
 * caller; this pointer will be invalidated at next Perl statement
 * transition.
 * We define this as a function rather than a macro in terms of my_getenv_len()
 * so that it'll work when PL_curinterp is undefined (and we therefore can't
 * allocate SVs).
 */
/*{{{ char *my_getenv(const char *lnm, bool sys)*/
char *
Perl_my_getenv(pTHX_ const char *lnm, bool sys)
{
    static char *__my_getenv_eqv = NULL;
    char uplnm[LNM$C_NAMLENGTH+1], *cp1, *cp2, *eqv;
    unsigned long int idx = 0;
    int trnsuccess, success, secure, saverr, savvmserr;
    int midx, flags;
    SV *tmpsv;

    midx = my_maxidx((char *) lnm) + 1;

    if (PL_curinterp) {  /* Perl interpreter running -- may be threaded */
      /* Set up a temporary buffer for the return value; Perl will
       * clean it up at the next statement transition */
      tmpsv = sv_2mortal(newSVpv("",(LNM$C_NAMLENGTH*midx)+1));
      if (!tmpsv) return NULL;
      eqv = SvPVX(tmpsv);
    }
    else {
      /* Assume no interpreter ==> single thread */
      if (__my_getenv_eqv != NULL) {
        Renew(__my_getenv_eqv,LNM$C_NAMLENGTH*midx+1,char);
      }
      else {
        New(1380,__my_getenv_eqv,LNM$C_NAMLENGTH*midx+1,char);
      }
      eqv = __my_getenv_eqv;  
    }

    for (cp1 = (char *) lnm, cp2 = eqv; *cp1; cp1++,cp2++) *cp2 = _toupper(*cp1);
    if (cp1 - lnm == 7 && !strncmp(eqv,"DEFAULT",7)) {
      getcwd(eqv,LNM$C_NAMLENGTH);
      return eqv;
    }
    else {
      /* Impose security constraints only if tainting */
      if (sys) {
        /* Impose security constraints only if tainting */
        secure = PL_curinterp ? PL_tainting : will_taint;
        saverr = errno;  savvmserr = vaxc$errno;
      }
      else {
        secure = 0;
      }

      flags = 
#ifdef SECURE_INTERNAL_GETENV
              secure ? PERL__TRNENV_SECURE : 0
#else
              0
#endif
      ;

      /* For the getenv interface we combine all the equivalence names
       * of a search list logical into one value to acquire a maximum
       * value length of 255*128 (assuming %ENV is using logicals).
       */
      flags |= PERL__TRNENV_JOIN_SEARCHLIST;

      /* If the name contains a semicolon-delimited index, parse it
       * off and make sure we only retrieve the equivalence name for 
       * that index.  */
      if ((cp2 = strchr(lnm,';')) != NULL) {
        strcpy(uplnm,lnm);
        uplnm[cp2-lnm] = '\0';
        idx = strtoul(cp2+1,NULL,0);
        lnm = uplnm;
        flags &= ~PERL__TRNENV_JOIN_SEARCHLIST;
      }

      success = vmstrnenv(lnm,eqv,idx,secure ? fildev : NULL,flags);

      /* Discard NOLOGNAM on internal calls since we're often looking
       * for an optional name, and this "error" often shows up as the
       * (bogus) exit status for a die() call later on.  */
      if (sys && vaxc$errno == SS$_NOLOGNAM) SETERRNO(saverr,savvmserr);
      return success ? eqv : Nullch;
    }

}  /* end of my_getenv() */
/*}}}*/


/*{{{ SV *my_getenv_len(const char *lnm, bool sys)*/
char *
Perl_my_getenv_len(pTHX_ const char *lnm, unsigned long *len, bool sys)
{
    char *buf, *cp1, *cp2;
    unsigned long idx = 0;
    int midx, flags;
    static char *__my_getenv_len_eqv = NULL;
    int secure, saverr, savvmserr;
    SV *tmpsv;
    
    midx = my_maxidx((char *) lnm) + 1;

    if (PL_curinterp) {  /* Perl interpreter running -- may be threaded */
      /* Set up a temporary buffer for the return value; Perl will
       * clean it up at the next statement transition */
      tmpsv = sv_2mortal(newSVpv("",(LNM$C_NAMLENGTH*midx)+1));
      if (!tmpsv) return NULL;
      buf = SvPVX(tmpsv);
    }
    else {
      /* Assume no interpreter ==> single thread */
      if (__my_getenv_len_eqv != NULL) {
        Renew(__my_getenv_len_eqv,LNM$C_NAMLENGTH*midx+1,char);
      }
      else {
        New(1381,__my_getenv_len_eqv,LNM$C_NAMLENGTH*midx+1,char);
      }
      buf = __my_getenv_len_eqv;  
    }

    for (cp1 = (char *)lnm, cp2 = buf; *cp1; cp1++,cp2++) *cp2 = _toupper(*cp1);
    if (cp1 - lnm == 7 && !strncmp(buf,"DEFAULT",7)) {
      getcwd(buf,LNM$C_NAMLENGTH);
      *len = strlen(buf);
      return buf;
    }
    else {
      if (sys) {
        /* Impose security constraints only if tainting */
        secure = PL_curinterp ? PL_tainting : will_taint;
        saverr = errno;  savvmserr = vaxc$errno;
      }
      else {
        secure = 0;
      }

      flags = 
#ifdef SECURE_INTERNAL_GETENV
              secure ? PERL__TRNENV_SECURE : 0
#else
              0
#endif
      ;

      flags |= PERL__TRNENV_JOIN_SEARCHLIST;

      if ((cp2 = strchr(lnm,';')) != NULL) {
        strcpy(buf,lnm);
        buf[cp2-lnm] = '\0';
        idx = strtoul(cp2+1,NULL,0);
        lnm = buf;
        flags &= ~PERL__TRNENV_JOIN_SEARCHLIST;
      }

      *len = vmstrnenv(lnm,buf,idx,secure ? fildev : NULL,flags);

      /* Discard NOLOGNAM on internal calls since we're often looking
       * for an optional name, and this "error" often shows up as the
       * (bogus) exit status for a die() call later on.  */
      if (sys && vaxc$errno == SS$_NOLOGNAM) SETERRNO(saverr,savvmserr);
      return *len ? buf : Nullch;
    }

}  /* end of my_getenv_len() */
/*}}}*/

static void create_mbx(pTHX_ unsigned short int *, struct dsc$descriptor_s *);

static void riseandshine(unsigned long int dummy) { sys$wake(0,0); }

/*{{{ void prime_env_iter() */
void
prime_env_iter(void)
/* Fill the %ENV associative array with all logical names we can
 * find, in preparation for iterating over it.
 */
{
  static int primed = 0;
  HV *seenhv = NULL, *envhv;
  SV *sv = NULL;
  char cmd[LNM$C_NAMLENGTH+24], mbxnam[LNM$C_NAMLENGTH], *buf = Nullch;
  unsigned short int chan;
#ifndef CLI$M_TRUSTED
#  define CLI$M_TRUSTED 0x40  /* Missing from VAXC headers */
#endif
  unsigned long int defflags = CLI$M_NOWAIT | CLI$M_NOKEYPAD | CLI$M_TRUSTED;
  unsigned long int mbxbufsiz, flags, retsts, subpid = 0, substs = 0, wakect = 0;
  long int i;
  bool have_sym = FALSE, have_lnm = FALSE;
  struct dsc$descriptor_s tmpdsc = {6,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
  $DESCRIPTOR(cmddsc,cmd);    $DESCRIPTOR(nldsc,"_NLA0:");
  $DESCRIPTOR(clidsc,"DCL");  $DESCRIPTOR(clitabdsc,"DCLTABLES");
  $DESCRIPTOR(crtlenv,"CRTL_ENV");  $DESCRIPTOR(clisym,"CLISYM");
  $DESCRIPTOR(local,"_LOCAL"); $DESCRIPTOR(mbxdsc,mbxnam); 
#if defined(PERL_IMPLICIT_CONTEXT)
  pTHX;
#endif
#if defined(USE_5005THREADS) || defined(USE_ITHREADS)
  static perl_mutex primenv_mutex;
  MUTEX_INIT(&primenv_mutex);
#endif

#if defined(PERL_IMPLICIT_CONTEXT)
    /* We jump through these hoops because we can be called at */
    /* platform-specific initialization time, which is before anything is */
    /* set up--we can't even do a plain dTHX since that relies on the */
    /* interpreter structure to be initialized */
#if defined(USE_5005THREADS)
    if (PL_curinterp) {
      aTHX = PL_threadnum? THR : (struct perl_thread*)SvPVX(PL_thrsv);
    } else {
      aTHX = NULL;
    }
#else
    if (PL_curinterp) {
      aTHX = PERL_GET_INTERP;
    } else {
      aTHX = NULL;
    }
#endif
#endif

  if (primed || !PL_envgv) return;
  MUTEX_LOCK(&primenv_mutex);
  if (primed) { MUTEX_UNLOCK(&primenv_mutex); return; }
  envhv = GvHVn(PL_envgv);
  /* Perform a dummy fetch as an lval to insure that the hash table is
   * set up.  Otherwise, the hv_store() will turn into a nullop. */
  (void) hv_fetch(envhv,"DEFAULT",7,TRUE);

  for (i = 0; env_tables[i]; i++) {
     if (!have_sym && (tmpdsc.dsc$a_pointer = env_tables[i]->dsc$a_pointer) &&
         !str$case_blind_compare(&tmpdsc,&clisym)) have_sym = 1;
     if (!have_lnm && !str$case_blind_compare(env_tables[i],&crtlenv)) have_lnm = 1;
  }
  if (have_sym || have_lnm) {
    long int syiitm = SYI$_MAXBUF, dviitm = DVI$_DEVNAM;
    _ckvmssts(lib$getsyi(&syiitm, &mbxbufsiz, 0, 0, 0, 0));
    _ckvmssts(sys$crembx(0,&chan,mbxbufsiz,mbxbufsiz,0xff0f,0,0));
    _ckvmssts(lib$getdvi(&dviitm, &chan, NULL, NULL, &mbxdsc, &mbxdsc.dsc$w_length));
  }

  for (i--; i >= 0; i--) {
    if (!str$case_blind_compare(env_tables[i],&crtlenv)) {
      char *start;
      int j;
      for (j = 0; environ[j]; j++) { 
        if (!(start = strchr(environ[j],'='))) {
          if (ckWARN(WARN_INTERNAL)) 
            Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"Ill-formed CRTL environ value \"%s\"\n",environ[j]);
        }
        else {
          start++;
          sv = newSVpv(start,0);
          SvTAINTED_on(sv);
          (void) hv_store(envhv,environ[j],start - environ[j] - 1,sv,0);
        }
      }
      continue;
    }
    else if ((tmpdsc.dsc$a_pointer = env_tables[i]->dsc$a_pointer) &&
             !str$case_blind_compare(&tmpdsc,&clisym)) {
      strcpy(cmd,"Show Symbol/Global *");
      cmddsc.dsc$w_length = 20;
      if (env_tables[i]->dsc$w_length == 12 &&
          (tmpdsc.dsc$a_pointer = env_tables[i]->dsc$a_pointer + 6) &&
          !str$case_blind_compare(&tmpdsc,&local)) strcpy(cmd+12,"Local  *");
      flags = defflags | CLI$M_NOLOGNAM;
    }
    else {
      strcpy(cmd,"Show Logical *");
      if (str$case_blind_compare(env_tables[i],&fildevdsc)) {
        strcat(cmd," /Table=");
        strncat(cmd,env_tables[i]->dsc$a_pointer,env_tables[i]->dsc$w_length);
        cmddsc.dsc$w_length = strlen(cmd);
      }
      else cmddsc.dsc$w_length = 14;  /* N.B. We test this below */
      flags = defflags | CLI$M_NOCLISYM;
    }
    
    /* Create a new subprocess to execute each command, to exclude the
     * remote possibility that someone could subvert a mbx or file used
     * to write multiple commands to a single subprocess.
     */
    do {
      retsts = lib$spawn(&cmddsc,&nldsc,&mbxdsc,&flags,0,&subpid,&substs,
                         0,&riseandshine,0,0,&clidsc,&clitabdsc);
      flags &= ~CLI$M_TRUSTED; /* Just in case we hit a really old version */
      defflags &= ~CLI$M_TRUSTED;
    } while (retsts == LIB$_INVARG && (flags | CLI$M_TRUSTED));
    _ckvmssts(retsts);
    if (!buf) New(1322,buf,mbxbufsiz + 1,char);
    if (seenhv) SvREFCNT_dec(seenhv);
    seenhv = newHV();
    while (1) {
      char *cp1, *cp2, *key;
      unsigned long int sts, iosb[2], retlen, keylen;
      register U32 hash;

      sts = sys$qiow(0,chan,IO$_READVBLK,iosb,0,0,buf,mbxbufsiz,0,0,0,0);
      if (sts & 1) sts = iosb[0] & 0xffff;
      if (sts == SS$_ENDOFFILE) {
        int wakect = 0;
        while (substs == 0) { sys$hiber(); wakect++;}
        if (wakect > 1) sys$wake(0,0);  /* Stole someone else's wake */
        _ckvmssts(substs);
        break;
      }
      _ckvmssts(sts);
      retlen = iosb[0] >> 16;      
      if (!retlen) continue;  /* blank line */
      buf[retlen] = '\0';
      if (iosb[1] != subpid) {
        if (iosb[1]) {
          Perl_croak(aTHX_ "Unknown process %x sent message to prime_env_iter: %s",buf);
        }
        continue;
      }
      if (sts == SS$_BUFFEROVF && ckWARN(WARN_INTERNAL))
        Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"Buffer overflow in prime_env_iter: %s",buf);

      for (cp1 = buf; *cp1 && isspace(*cp1); cp1++) ;
      if (*cp1 == '(' || /* Logical name table name */
          *cp1 == '='    /* Next eqv of searchlist  */) continue;
      if (*cp1 == '"') cp1++;
      for (cp2 = cp1; *cp2 && *cp2 != '"' && *cp2 != ' '; cp2++) ;
      key = cp1;  keylen = cp2 - cp1;
      if (keylen && hv_exists(seenhv,key,keylen)) continue;
      while (*cp2 && *cp2 != '=') cp2++;
      while (*cp2 && *cp2 == '=') cp2++;
      while (*cp2 && *cp2 == ' ') cp2++;
      if (*cp2 == '"') {  /* String translation; may embed "" */
        for (cp1 = buf + retlen; *cp1 != '"'; cp1--) ;
        cp2++;  cp1--; /* Skip "" surrounding translation */
      }
      else {  /* Numeric translation */
        for (cp1 = cp2; *cp1 && *cp1 != ' '; cp1++) ;
        cp1--;  /* stop on last non-space char */
      }
      if ((!keylen || (cp1 - cp2 < -1)) && ckWARN(WARN_INTERNAL)) {
        Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"Ill-formed message in prime_env_iter: |%s|",buf);
        continue;
      }
      PERL_HASH(hash,key,keylen);

      if (cp1 == cp2 && *cp2 == '.') {
        /* A single dot usually means an unprintable character, such as a null
         * to indicate a zero-length value.  Get the actual value to make sure.
         */
        char lnm[LNM$C_NAMLENGTH+1];
        char eqv[LNM$C_NAMLENGTH+1];
        strncpy(lnm, key, keylen);
        int trnlen = vmstrnenv(lnm, eqv, 0, fildev, 0);
        sv = newSVpvn(eqv, strlen(eqv));
      }
      else {
        sv = newSVpvn(cp2,cp1 - cp2 + 1);
      }

      SvTAINTED_on(sv);
      hv_store(envhv,key,keylen,sv,hash);
      hv_store(seenhv,key,keylen,&PL_sv_yes,hash);
    }
    if (cmddsc.dsc$w_length == 14) { /* We just read LNM$FILE_DEV */
      /* get the PPFs for this process, not the subprocess */
      char *ppfs[] = {"SYS$COMMAND", "SYS$INPUT", "SYS$OUTPUT", "SYS$ERROR", NULL};
      char eqv[LNM$C_NAMLENGTH+1];
      int trnlen, i;
      for (i = 0; ppfs[i]; i++) {
        trnlen = vmstrnenv(ppfs[i],eqv,0,fildev,0);
        sv = newSVpv(eqv,trnlen);
        SvTAINTED_on(sv);
        hv_store(envhv,ppfs[i],strlen(ppfs[i]),sv,0);
      }
    }
  }
  primed = 1;
  if (have_sym || have_lnm) _ckvmssts(sys$dassgn(chan));
  if (buf) Safefree(buf);
  if (seenhv) SvREFCNT_dec(seenhv);
  MUTEX_UNLOCK(&primenv_mutex);
  return;

}  /* end of prime_env_iter */
/*}}}*/


/*{{{ int  vmssetenv(char *lnm, char *eqv)*/
/* Define or delete an element in the same "environment" as
 * vmstrnenv().  If an element is to be deleted, it's removed from
 * the first place it's found.  If it's to be set, it's set in the
 * place designated by the first element of the table vector.
 * Like setenv() returns 0 for success, non-zero on error.
 */
int
Perl_vmssetenv(pTHX_ char *lnm, char *eqv, struct dsc$descriptor_s **tabvec)
{
    char uplnm[LNM$C_NAMLENGTH], *cp1, *cp2, *c;
    unsigned short int curtab, ivlnm = 0, ivsym = 0, ivenv = 0;
    int nseg = 0, j;
    unsigned long int retsts, usermode = PSL$C_USER;
    struct itmlst_3 *ile, *ilist;
    struct dsc$descriptor_s lnmdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,uplnm},
                            eqvdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0},
                            tmpdsc = {6,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
    $DESCRIPTOR(crtlenv,"CRTL_ENV");  $DESCRIPTOR(clisym,"CLISYM");
    $DESCRIPTOR(local,"_LOCAL");

    if (!lnm) {
        set_errno(EINVAL); set_vaxc_errno(SS$_IVLOGNAM);
        return SS$_IVLOGNAM;
    }

    for (cp1 = lnm, cp2 = uplnm; *cp1; cp1++, cp2++) {
      *cp2 = _toupper(*cp1);
      if (cp1 - lnm > LNM$C_NAMLENGTH) {
        set_errno(EINVAL); set_vaxc_errno(SS$_IVLOGNAM);
        return SS$_IVLOGNAM;
      }
    }
    lnmdsc.dsc$w_length = cp1 - lnm;
    if (!tabvec || !*tabvec) tabvec = env_tables;

    if (!eqv) {  /* we're deleting n element */
      for (curtab = 0; tabvec[curtab]; curtab++) {
        if (!ivenv && !str$case_blind_compare(tabvec[curtab],&crtlenv)) {
        int i;
          for (i = 0; environ[i]; i++) { /* If it's an environ elt, reset */
            if ((cp1 = strchr(environ[i],'=')) && 
                lnmdsc.dsc$w_length == (cp1 - environ[i]) &&
                !strncmp(environ[i],lnm,cp1 - environ[i])) {
#ifdef HAS_SETENV
              return setenv(lnm,"",1) ? vaxc$errno : 0;
            }
          }
          ivenv = 1; retsts = SS$_NOLOGNAM;
#else
              if (ckWARN(WARN_INTERNAL))
                Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"This Perl can't reset CRTL environ elements (%s)",lnm);
              ivenv = 1; retsts = SS$_NOSUCHPGM;
              break;
            }
          }
#endif
        }
        else if ((tmpdsc.dsc$a_pointer = tabvec[curtab]->dsc$a_pointer) &&
                 !str$case_blind_compare(&tmpdsc,&clisym)) {
          unsigned int symtype;
          if (tabvec[curtab]->dsc$w_length == 12 &&
              (tmpdsc.dsc$a_pointer = tabvec[curtab]->dsc$a_pointer + 6) &&
              !str$case_blind_compare(&tmpdsc,&local)) 
            symtype = LIB$K_CLI_LOCAL_SYM;
          else symtype = LIB$K_CLI_GLOBAL_SYM;
          retsts = lib$delete_symbol(&lnmdsc,&symtype);
          if (retsts == LIB$_INVSYMNAM) { ivsym = 1; continue; }
          if (retsts == LIB$_NOSUCHSYM) continue;
          break;
        }
        else if (!ivlnm) {
          retsts = sys$dellnm(tabvec[curtab],&lnmdsc,&usermode); /* try user mode first */
          if (retsts == SS$_IVLOGNAM) { ivlnm = 1; continue; }
          if (retsts != SS$_NOLOGNAM && retsts != SS$_NOLOGTAB) break;
          retsts = lib$delete_logical(&lnmdsc,tabvec[curtab]); /* then supervisor mode */
          if (retsts != SS$_NOLOGNAM && retsts != SS$_NOLOGTAB) break;
        }
      }
    }
    else {  /* we're defining a value */
      if (!ivenv && !str$case_blind_compare(tabvec[0],&crtlenv)) {
#ifdef HAS_SETENV
        return setenv(lnm,eqv,1) ? vaxc$errno : 0;
#else
        if (ckWARN(WARN_INTERNAL))
          Perl_warner(aTHX_ packWARN(WARN_INTERNAL),"This Perl can't set CRTL environ elements (%s=%s)",lnm,eqv);
        retsts = SS$_NOSUCHPGM;
#endif
      }
      else {
        eqvdsc.dsc$a_pointer = eqv;
        eqvdsc.dsc$w_length  = strlen(eqv);
        if ((tmpdsc.dsc$a_pointer = tabvec[0]->dsc$a_pointer) &&
            !str$case_blind_compare(&tmpdsc,&clisym)) {
          unsigned int symtype;
          if (tabvec[0]->dsc$w_length == 12 &&
              (tmpdsc.dsc$a_pointer = tabvec[0]->dsc$a_pointer + 6) &&
               !str$case_blind_compare(&tmpdsc,&local)) 
            symtype = LIB$K_CLI_LOCAL_SYM;
          else symtype = LIB$K_CLI_GLOBAL_SYM;
          retsts = lib$set_symbol(&lnmdsc,&eqvdsc,&symtype);
        }
        else {
          if (!*eqv) eqvdsc.dsc$w_length = 1;
	  if (eqvdsc.dsc$w_length > LNM$C_NAMLENGTH) {

            nseg = (eqvdsc.dsc$w_length + LNM$C_NAMLENGTH - 1) / LNM$C_NAMLENGTH;
            if (nseg > PERL_LNM_MAX_ALLOWED_INDEX + 1) {
	      Perl_warner(aTHX_ packWARN(WARN_MISC),"Value of logical \"%s\" too long. Truncating to %i bytes",
                          lnm, LNM$C_NAMLENGTH * (PERL_LNM_MAX_ALLOWED_INDEX+1));
              eqvdsc.dsc$w_length = LNM$C_NAMLENGTH * (PERL_LNM_MAX_ALLOWED_INDEX+1);
              nseg = PERL_LNM_MAX_ALLOWED_INDEX + 1;
	    }

            New(1382,ilist,nseg+1,struct itmlst_3);
            ile = ilist;
            if (!ile) {
	      set_errno(ENOMEM); set_vaxc_errno(SS$_INSFMEM);
              return SS$_INSFMEM;
	    }
            memset(ilist, 0, (sizeof(struct itmlst_3) * (nseg+1)));

            for (j = 0, c = eqvdsc.dsc$a_pointer; j < nseg; j++, ile++, c += LNM$C_NAMLENGTH) {
              ile->itmcode = LNM$_STRING;
              ile->bufadr = c;
              if ((j+1) == nseg) {
                ile->buflen = strlen(c);
                /* in case we are truncating one that's too long */
                if (ile->buflen > LNM$C_NAMLENGTH) ile->buflen = LNM$C_NAMLENGTH;
              }
              else {
                ile->buflen = LNM$C_NAMLENGTH;
              }
            }

            retsts = lib$set_logical(&lnmdsc,0,tabvec[0],0,ilist);
            Safefree (ilist);
	  }
          else {
            retsts = lib$set_logical(&lnmdsc,&eqvdsc,tabvec[0],0,0);
	  }
        }
      }
    }
    if (!(retsts & 1)) {
      switch (retsts) {
        case LIB$_AMBSYMDEF: case LIB$_INSCLIMEM:
        case SS$_NOLOGTAB: case SS$_TOOMANYLNAM: case SS$_IVLOGTAB:
          set_errno(EVMSERR); break;
        case LIB$_INVARG: case LIB$_INVSYMNAM: case SS$_IVLOGNAM: 
        case LIB$_NOSUCHSYM: case SS$_NOLOGNAM:
          set_errno(EINVAL); break;
        case SS$_NOPRIV:
          set_errno(EACCES);
        default:
          _ckvmssts(retsts);
          set_errno(EVMSERR);
       }
       set_vaxc_errno(retsts);
       return (int) retsts || 44; /* retsts should never be 0, but just in case */
    }
    else {
      /* We reset error values on success because Perl does an hv_fetch()
       * before each hv_store(), and if the thing we're setting didn't
       * previously exist, we've got a leftover error message.  (Of course,
       * this fails in the face of
       *    $foo = $ENV{nonexistent}; $ENV{existent} = 'foo';
       * in that the error reported in $! isn't spurious, 
       * but it's right more often than not.)
       */
      set_errno(0); set_vaxc_errno(retsts);
      return 0;
    }

}  /* end of vmssetenv() */
/*}}}*/

/*{{{ void  my_setenv(char *lnm, char *eqv)*/
/* This has to be a function since there's a prototype for it in proto.h */
void
Perl_my_setenv(pTHX_ char *lnm,char *eqv)
{
    if (lnm && *lnm) {
      int len = strlen(lnm);
      if  (len == 7) {
        char uplnm[8];
        int i;
        for (i = 0; lnm[i]; i++) uplnm[i] = _toupper(lnm[i]);
        if (!strcmp(uplnm,"DEFAULT")) {
          if (eqv && *eqv) chdir(eqv);
          return;
        }
    } 
#ifndef RTL_USES_UTC
    if (len == 6 || len == 2) {
      char uplnm[7];
      int i;
      for (i = 0; lnm[i]; i++) uplnm[i] = _toupper(lnm[i]);
      uplnm[len] = '\0';
      if (!strcmp(uplnm,"UCX$TZ")) tz_updated = 1;
      if (!strcmp(uplnm,"TZ")) tz_updated = 1;
    }
#endif
  }
  (void) vmssetenv(lnm,eqv,NULL);
}
/*}}}*/

/*{{{static void vmssetuserlnm(char *name, char *eqv); */
/*  vmssetuserlnm
 *  sets a user-mode logical in the process logical name table
 *  used for redirection of sys$error
 */
void
Perl_vmssetuserlnm(pTHX_ char *name, char *eqv)
{
    $DESCRIPTOR(d_tab, "LNM$PROCESS");
    struct dsc$descriptor_d d_name = {0,DSC$K_DTYPE_T,DSC$K_CLASS_D,0};
    unsigned long int iss, attr = LNM$M_CONFINE;
    unsigned char acmode = PSL$C_USER;
    struct itmlst_3 lnmlst[2] = {{0, LNM$_STRING, 0, 0},
                                 {0, 0, 0, 0}};
    d_name.dsc$a_pointer = name;
    d_name.dsc$w_length = strlen(name);

    lnmlst[0].buflen = strlen(eqv);
    lnmlst[0].bufadr = eqv;

    iss = sys$crelnm(&attr,&d_tab,&d_name,&acmode,lnmlst);
    if (!(iss&1)) lib$signal(iss);
}
/*}}}*/


/*{{{ char *my_crypt(const char *textpasswd, const char *usrname)*/
/* my_crypt - VMS password hashing
 * my_crypt() provides an interface compatible with the Unix crypt()
 * C library function, and uses sys$hash_password() to perform VMS
 * password hashing.  The quadword hashed password value is returned
 * as a NUL-terminated 8 character string.  my_crypt() does not change
 * the case of its string arguments; in order to match the behavior
 * of LOGINOUT et al., alphabetic characters in both arguments must
 *  be upcased by the caller.
 */
char *
Perl_my_crypt(pTHX_ const char *textpasswd, const char *usrname)
{
#   ifndef UAI$C_PREFERRED_ALGORITHM
#     define UAI$C_PREFERRED_ALGORITHM 127
#   endif
    unsigned char alg = UAI$C_PREFERRED_ALGORITHM;
    unsigned short int salt = 0;
    unsigned long int sts;
    struct const_dsc {
        unsigned short int dsc$w_length;
        unsigned char      dsc$b_type;
        unsigned char      dsc$b_class;
        const char *       dsc$a_pointer;
    }  usrdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0},
       txtdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
    struct itmlst_3 uailst[3] = {
        { sizeof alg,  UAI$_ENCRYPT, &alg, 0},
        { sizeof salt, UAI$_SALT,    &salt, 0},
        { 0,           0,            NULL,  NULL}};
    static char hash[9];

    usrdsc.dsc$w_length = strlen(usrname);
    usrdsc.dsc$a_pointer = usrname;
    if (!((sts = sys$getuai(0, 0, &usrdsc, uailst, 0, 0, 0)) & 1)) {
      switch (sts) {
        case SS$_NOGRPPRV: case SS$_NOSYSPRV:
          set_errno(EACCES);
          break;
        case RMS$_RNF:
          set_errno(ESRCH);  /* There isn't a Unix no-such-user error */
          break;
        default:
          set_errno(EVMSERR);
      }
      set_vaxc_errno(sts);
      if (sts != RMS$_RNF) return NULL;
    }

    txtdsc.dsc$w_length = strlen(textpasswd);
    txtdsc.dsc$a_pointer = textpasswd;
    if (!((sts = sys$hash_password(&txtdsc, alg, salt, &usrdsc, &hash)) & 1)) {
      set_errno(EVMSERR);  set_vaxc_errno(sts);  return NULL;
    }

    return (char *) hash;

}  /* end of my_crypt() */
/*}}}*/


static char *mp_do_rmsexpand(pTHX_ char *, char *, int, char *, unsigned);
static char *mp_do_fileify_dirspec(pTHX_ char *, char *, int);
static char *mp_do_tovmsspec(pTHX_ char *, char *, int);

/*{{{int do_rmdir(char *name)*/
int
Perl_do_rmdir(pTHX_ char *name)
{
    char dirfile[NAM$C_MAXRSS+1];
    int retval;
    Stat_t st;

    if (do_fileify_dirspec(name,dirfile,0) == NULL) return -1;
    if (flex_stat(dirfile,&st) || !S_ISDIR(st.st_mode)) retval = -1;
    else retval = kill_file(dirfile);
    return retval;

}  /* end of do_rmdir */
/*}}}*/

/* kill_file
 * Delete any file to which user has control access, regardless of whether
 * delete access is explicitly allowed.
 * Limitations: User must have write access to parent directory.
 *              Does not block signals or ASTs; if interrupted in midstream
 *              may leave file with an altered ACL.
 * HANDLE WITH CARE!
 */
/*{{{int kill_file(char *name)*/
int
Perl_kill_file(pTHX_ char *name)
{
    char vmsname[NAM$C_MAXRSS+1], rspec[NAM$C_MAXRSS+1];
    unsigned long int jpicode = JPI$_UIC, type = ACL$C_FILE;
    unsigned long int cxt = 0, aclsts, fndsts, rmsts = -1;
    struct dsc$descriptor_s fildsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
    struct myacedef {
      unsigned char myace$b_length;
      unsigned char myace$b_type;
      unsigned short int myace$w_flags;
      unsigned long int myace$l_access;
      unsigned long int myace$l_ident;
    } newace = { sizeof(struct myacedef), ACE$C_KEYID, 0,
                 ACE$M_READ | ACE$M_WRITE | ACE$M_DELETE | ACE$M_CONTROL, 0},
      oldace = { sizeof(struct myacedef), ACE$C_KEYID, 0, 0, 0};
     struct itmlst_3
       findlst[3] = {{sizeof oldace, ACL$C_FNDACLENT, &oldace, 0},
                     {sizeof oldace, ACL$C_READACE,   &oldace, 0},{0,0,0,0}},
       addlst[2] = {{sizeof newace, ACL$C_ADDACLENT, &newace, 0},{0,0,0,0}},
       dellst[2] = {{sizeof newace, ACL$C_DELACLENT, &newace, 0},{0,0,0,0}},
       lcklst[2] = {{sizeof newace, ACL$C_WLOCK_ACL, &newace, 0},{0,0,0,0}},
       ulklst[2] = {{sizeof newace, ACL$C_UNLOCK_ACL, &newace, 0},{0,0,0,0}};
      
    /* Expand the input spec using RMS, since the CRTL remove() and
     * system services won't do this by themselves, so we may miss
     * a file "hiding" behind a logical name or search list. */
    if (do_tovmsspec(name,vmsname,0) == NULL) return -1;
    if (do_rmsexpand(vmsname,rspec,1,NULL,0) == NULL) return -1;
    if (!remove(rspec)) return 0;   /* Can we just get rid of it? */
    /* If not, can changing protections help? */
    if (vaxc$errno != RMS$_PRV) return -1;

    /* No, so we get our own UIC to use as a rights identifier,
     * and the insert an ACE at the head of the ACL which allows us
     * to delete the file.
     */
    _ckvmssts(lib$getjpi(&jpicode,0,0,&(oldace.myace$l_ident),0,0));
    fildsc.dsc$w_length = strlen(rspec);
    fildsc.dsc$a_pointer = rspec;
    cxt = 0;
    newace.myace$l_ident = oldace.myace$l_ident;
    if (!((aclsts = sys$change_acl(0,&type,&fildsc,lcklst,0,0,0)) & 1)) {
      switch (aclsts) {
        case RMS$_FNF: case RMS$_DNF: case SS$_NOSUCHOBJECT:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_SYN: case SS$_INVFILFOROP:
          set_errno(EINVAL); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        default:
          _ckvmssts(aclsts);
      }
      set_vaxc_errno(aclsts);
      return -1;
    }
    /* Grab any existing ACEs with this identifier in case we fail */
    aclsts = fndsts = sys$change_acl(0,&type,&fildsc,findlst,0,0,&cxt);
    if ( fndsts & 1 || fndsts == SS$_ACLEMPTY || fndsts == SS$_NOENTRY
                    || fndsts == SS$_NOMOREACE ) {
      /* Add the new ACE . . . */
      if (!((aclsts = sys$change_acl(0,&type,&fildsc,addlst,0,0,0)) & 1))
        goto yourroom;
      if ((rmsts = remove(name))) {
        /* We blew it - dir with files in it, no write priv for
         * parent directory, etc.  Put things back the way they were. */
        if (!((aclsts = sys$change_acl(0,&type,&fildsc,dellst,0,0,0)) & 1))
          goto yourroom;
        if (fndsts & 1) {
          addlst[0].bufadr = &oldace;
          if (!((aclsts = sys$change_acl(0,&type,&fildsc,addlst,0,0,&cxt)) & 1))
            goto yourroom;
        }
      }
    }

    yourroom:
    fndsts = sys$change_acl(0,&type,&fildsc,ulklst,0,0,0);
    /* We just deleted it, so of course it's not there.  Some versions of
     * VMS seem to return success on the unlock operation anyhow (after all
     * the unlock is successful), but others don't.
     */
    if (fndsts == RMS$_FNF || fndsts == SS$_NOSUCHOBJECT) fndsts = SS$_NORMAL;
    if (aclsts & 1) aclsts = fndsts;
    if (!(aclsts & 1)) {
      set_errno(EVMSERR);
      set_vaxc_errno(aclsts);
      return -1;
    }

    return rmsts;

}  /* end of kill_file() */
/*}}}*/


/*{{{int my_mkdir(char *,Mode_t)*/
int
Perl_my_mkdir(pTHX_ char *dir, Mode_t mode)
{
  STRLEN dirlen = strlen(dir);

  /* zero length string sometimes gives ACCVIO */
  if (dirlen == 0) return -1;

  /* CRTL mkdir() doesn't tolerate trailing /, since that implies
   * null file name/type.  However, it's commonplace under Unix,
   * so we'll allow it for a gain in portability.
   */
  if (dir[dirlen-1] == '/') {
    char *newdir = savepvn(dir,dirlen-1);
    int ret = mkdir(newdir,mode);
    Safefree(newdir);
    return ret;
  }
  else return mkdir(dir,mode);
}  /* end of my_mkdir */
/*}}}*/

/*{{{int my_chdir(char *)*/
int
Perl_my_chdir(pTHX_ char *dir)
{
  STRLEN dirlen = strlen(dir);

  /* zero length string sometimes gives ACCVIO */
  if (dirlen == 0) return -1;

  /* some versions of CRTL chdir() doesn't tolerate trailing /, since
   * that implies
   * null file name/type.  However, it's commonplace under Unix,
   * so we'll allow it for a gain in portability.
   */
  if (dir[dirlen-1] == '/') {
    char *newdir = savepvn(dir,dirlen-1);
    int ret = chdir(newdir);
    Safefree(newdir);
    return ret;
  }
  else return chdir(dir);
}  /* end of my_chdir */
/*}}}*/


/*{{{FILE *my_tmpfile()*/
FILE *
my_tmpfile(void)
{
  FILE *fp;
  char *cp;

  if ((fp = tmpfile())) return fp;

  New(1323,cp,L_tmpnam+24,char);
  strcpy(cp,"Sys$Scratch:");
  tmpnam(cp+strlen(cp));
  strcat(cp,".Perltmp");
  fp = fopen(cp,"w+","fop=dlt");
  Safefree(cp);
  return fp;
}
/*}}}*/


#ifndef HOMEGROWN_POSIX_SIGNALS
/*
 * The C RTL's sigaction fails to check for invalid signal numbers so we 
 * help it out a bit.  The docs are correct, but the actual routine doesn't
 * do what the docs say it will.
 */
/*{{{int Perl_my_sigaction (pTHX_ int, const struct sigaction*, struct sigaction*);*/
int
Perl_my_sigaction (pTHX_ int sig, const struct sigaction* act, 
                   struct sigaction* oact)
{
  if (sig == SIGKILL || sig == SIGSTOP || sig == SIGCONT) {
	SETERRNO(EINVAL, SS$_INVARG);
	return -1;
  }
  return sigaction(sig, act, oact);
}
/*}}}*/
#endif

#ifdef KILL_BY_SIGPRC
#include <errnodef.h>

/* We implement our own kill() using the undocumented system service
   sys$sigprc for one of two reasons:

   1.) If the kill() in an older CRTL uses sys$forcex, causing the
   target process to do a sys$exit, which usually can't be handled 
   gracefully...certainly not by Perl and the %SIG{} mechanism.

   2.) If the kill() in the CRTL can't be called from a signal
   handler without disappearing into the ether, i.e., the signal
   it purportedly sends is never trapped. Still true as of VMS 7.3.

   sys$sigprc has the same parameters as sys$forcex, but throws an exception
   in the target process rather than calling sys$exit.

   Note that distinguishing SIGSEGV from SIGBUS requires an extra arg
   on the ACCVIO condition, which sys$sigprc (and sys$forcex) don't
   provide.  On VMS 7.0+ this is taken care of by doing sys$sigprc
   with condition codes C$_SIG0+nsig*8, catching the exception on the 
   target process and resignaling with appropriate arguments.

   But we don't have that VMS 7.0+ exception handler, so if you
   Perl_my_kill(.., SIGSEGV) it will show up as a SIGBUS.  Oh well.

   Also note that SIGTERM is listed in the docs as being "unimplemented",
   yet always seems to be signaled with a VMS condition code of 4 (and
   correctly handled for that code).  So we hardwire it in.

   Unlike the VMS 7.0+ CRTL kill() function, we actually check the signal
   number to see if it's valid.  So Perl_my_kill(pid,0) returns -1 rather
   than signalling with an unrecognized (and unhandled by CRTL) code.
*/

#define _MY_SIG_MAX 17

unsigned int
Perl_sig_to_vmscondition(int sig)
{
    static unsigned int sig_code[_MY_SIG_MAX+1] = 
    {
        0,                  /*  0 ZERO     */
        SS$_HANGUP,         /*  1 SIGHUP   */
        SS$_CONTROLC,       /*  2 SIGINT   */
        SS$_CONTROLY,       /*  3 SIGQUIT  */
        SS$_RADRMOD,        /*  4 SIGILL   */
        SS$_BREAK,          /*  5 SIGTRAP  */
        SS$_OPCCUS,         /*  6 SIGABRT  */
        SS$_COMPAT,         /*  7 SIGEMT   */
#ifdef __VAX                      
        SS$_FLTOVF,         /*  8 SIGFPE VAX */
#else                             
        SS$_HPARITH,        /*  8 SIGFPE AXP */
#endif                            
        SS$_ABORT,          /*  9 SIGKILL  */
        SS$_ACCVIO,         /* 10 SIGBUS   */
        SS$_ACCVIO,         /* 11 SIGSEGV  */
        SS$_BADPARAM,       /* 12 SIGSYS   */
        SS$_NOMBX,          /* 13 SIGPIPE  */
        SS$_ASTFLT,         /* 14 SIGALRM  */
        4,                  /* 15 SIGTERM  */
        0,                  /* 16 SIGUSR1  */
        0                   /* 17 SIGUSR2  */
    };

#if __VMS_VER >= 60200000
    static int initted = 0;
    if (!initted) {
        initted = 1;
        sig_code[16] = C$_SIGUSR1;
        sig_code[17] = C$_SIGUSR2;
    }
#endif

    if (sig < _SIG_MIN) return 0;
    if (sig > _MY_SIG_MAX) return 0;
    return sig_code[sig];
}


int
Perl_my_kill(int pid, int sig)
{
    dTHX;
    int iss;
    unsigned int code;
    int sys$sigprc(unsigned int *pidadr,
                     struct dsc$descriptor_s *prcname,
                     unsigned int code);

    code = Perl_sig_to_vmscondition(sig);

    if (!pid || !code) {
        return -1;
    }

    iss = sys$sigprc((unsigned int *)&pid,0,code);
    if (iss&1) return 0;

    switch (iss) {
      case SS$_NOPRIV:
        set_errno(EPERM);  break;
      case SS$_NONEXPR:  
      case SS$_NOSUCHNODE:
      case SS$_UNREACHABLE:
        set_errno(ESRCH);  break;
      case SS$_INSFMEM:
        set_errno(ENOMEM); break;
      default:
        _ckvmssts(iss);
        set_errno(EVMSERR);
    } 
    set_vaxc_errno(iss);
 
    return -1;
}
#endif

/* default piping mailbox size */
#define PERL_BUFSIZ        512


static void
create_mbx(pTHX_ unsigned short int *chan, struct dsc$descriptor_s *namdsc)
{
  unsigned long int mbxbufsiz;
  static unsigned long int syssize = 0;
  unsigned long int dviitm = DVI$_DEVNAM;
  char csize[LNM$C_NAMLENGTH+1];
  
  if (!syssize) {
    unsigned long syiitm = SYI$_MAXBUF;
    /*
     * Get the SYSGEN parameter MAXBUF
     *
     * If the logical 'PERL_MBX_SIZE' is defined
     * use the value of the logical instead of PERL_BUFSIZ, but 
     * keep the size between 128 and MAXBUF.
     *
     */
    _ckvmssts(lib$getsyi(&syiitm, &syssize, 0, 0, 0, 0));
  }

  if (vmstrnenv("PERL_MBX_SIZE", csize, 0, fildev, 0)) {
      mbxbufsiz = atoi(csize);
  } else {
      mbxbufsiz = PERL_BUFSIZ;
  }
  if (mbxbufsiz < 128) mbxbufsiz = 128;
  if (mbxbufsiz > syssize) mbxbufsiz = syssize;

  _ckvmssts(sys$crembx(0,chan,mbxbufsiz,mbxbufsiz,0,0,0));

  _ckvmssts(lib$getdvi(&dviitm, chan, NULL, NULL, namdsc, &namdsc->dsc$w_length));
  namdsc->dsc$a_pointer[namdsc->dsc$w_length] = '\0';

}  /* end of create_mbx() */


/*{{{  my_popen and my_pclose*/

typedef struct _iosb           IOSB;
typedef struct _iosb*         pIOSB;
typedef struct _pipe           Pipe;
typedef struct _pipe*         pPipe;
typedef struct pipe_details    Info;
typedef struct pipe_details*  pInfo;
typedef struct _srqp            RQE;
typedef struct _srqp*          pRQE;
typedef struct _tochildbuf      CBuf;
typedef struct _tochildbuf*    pCBuf;

struct _iosb {
    unsigned short status;
    unsigned short count;
    unsigned long  dvispec;
};

#pragma member_alignment save
#pragma nomember_alignment quadword
struct _srqp {          /* VMS self-relative queue entry */
    unsigned long qptr[2];
};
#pragma member_alignment restore
static RQE  RQE_ZERO = {0,0};

struct _tochildbuf {
    RQE             q;
    int             eof;
    unsigned short  size;
    char            *buf;
};

struct _pipe {
    RQE            free;
    RQE            wait;
    int            fd_out;
    unsigned short chan_in;
    unsigned short chan_out;
    char          *buf;
    unsigned int   bufsize;
    IOSB           iosb;
    IOSB           iosb2;
    int           *pipe_done;
    int            retry;
    int            type;
    int            shut_on_empty;
    int            need_wake;
    pPipe         *home;
    pInfo          info;
    pCBuf          curr;
    pCBuf          curr2;
#if defined(PERL_IMPLICIT_CONTEXT)
    void	    *thx;	    /* Either a thread or an interpreter */
                                    /* pointer, depending on how we're built */
#endif
};


struct pipe_details
{
    pInfo           next;
    PerlIO *fp;  /* file pointer to pipe mailbox */
    int useFILE; /* using stdio, not perlio */
    int pid;   /* PID of subprocess */
    int mode;  /* == 'r' if pipe open for reading */
    int done;  /* subprocess has completed */
    int waiting; /* waiting for completion/closure */
    int             closing;        /* my_pclose is closing this pipe */
    unsigned long   completion;     /* termination status of subprocess */
    pPipe           in;             /* pipe in to sub */
    pPipe           out;            /* pipe out of sub */
    pPipe           err;            /* pipe of sub's sys$error */
    int             in_done;        /* true when in pipe finished */
    int             out_done;
    int             err_done;
};

struct exit_control_block
{
    struct exit_control_block *flink;
    unsigned long int	(*exit_routine)();
    unsigned long int arg_count;
    unsigned long int *status_address;
    unsigned long int exit_status;
}; 

typedef struct _closed_pipes    Xpipe;
typedef struct _closed_pipes*  pXpipe;

struct _closed_pipes {
    int             pid;            /* PID of subprocess */
    unsigned long   completion;     /* termination status of subprocess */
};
#define NKEEPCLOSED 50
static Xpipe closed_list[NKEEPCLOSED];
static int   closed_index = 0;
static int   closed_num = 0;

#define RETRY_DELAY     "0 ::0.20"
#define MAX_RETRY              50

static int pipe_ef = 0;          /* first call to safe_popen inits these*/
static unsigned long mypid;
static unsigned long delaytime[2];

static pInfo open_pipes = NULL;
static $DESCRIPTOR(nl_desc, "NL:");

#define PIPE_COMPLETION_WAIT    30  /* seconds, for EOF/FORCEX wait */



static unsigned long int
pipe_exit_routine(pTHX)
{
    pInfo info;
    unsigned long int retsts = SS$_NORMAL, abort = SS$_TIMEOUT;
    int sts, did_stuff, need_eof, j;

    /* 
        flush any pending i/o
    */
    info = open_pipes;
    while (info) {
        if (info->fp) {
           if (!info->useFILE) 
               PerlIO_flush(info->fp);   /* first, flush data */
           else 
               fflush((FILE *)info->fp);
        }
        info = info->next;
    }

    /* 
     next we try sending an EOF...ignore if doesn't work, make sure we
     don't hang
    */
    did_stuff = 0;
    info = open_pipes;

    while (info) {
      int need_eof;
      _ckvmssts(sys$setast(0));
      if (info->in && !info->in->shut_on_empty) {
        _ckvmssts(sys$qio(0,info->in->chan_in,IO$_WRITEOF,0,0,0,
                          0, 0, 0, 0, 0, 0));
        info->waiting = 1;
        did_stuff = 1;
      }
      _ckvmssts(sys$setast(1));
      info = info->next;
    }

    /* wait for EOF to have effect, up to ~ 30 sec [default] */

    for (j = 0; did_stuff && j < PIPE_COMPLETION_WAIT; j++) {
        int nwait = 0;

        info = open_pipes;
        while (info) {
          _ckvmssts(sys$setast(0));
          if (info->waiting && info->done) 
                info->waiting = 0;
          nwait += info->waiting;
          _ckvmssts(sys$setast(1));
          info = info->next;
        }
        if (!nwait) break;
        sleep(1);  
    }

    did_stuff = 0;
    info = open_pipes;
    while (info) {
      _ckvmssts(sys$setast(0));
      if (!info->done) { /* Tap them gently on the shoulder . . .*/
        sts = sys$forcex(&info->pid,0,&abort);
        if (!(sts&1) && sts != SS$_NONEXPR) _ckvmssts(sts); 
        did_stuff = 1;
      }
      _ckvmssts(sys$setast(1));
      info = info->next;
    }

    /* again, wait for effect */

    for (j = 0; did_stuff && j < PIPE_COMPLETION_WAIT; j++) {
        int nwait = 0;

        info = open_pipes;
        while (info) {
          _ckvmssts(sys$setast(0));
          if (info->waiting && info->done) 
                info->waiting = 0;
          nwait += info->waiting;
          _ckvmssts(sys$setast(1));
          info = info->next;
        }
        if (!nwait) break;
        sleep(1);  
    }

    info = open_pipes;
    while (info) {
      _ckvmssts(sys$setast(0));
      if (!info->done) {  /* We tried to be nice . . . */
        sts = sys$delprc(&info->pid,0);
        if (!(sts&1) && sts != SS$_NONEXPR) _ckvmssts(sts); 
      }
      _ckvmssts(sys$setast(1));
      info = info->next;
    }

    while(open_pipes) {
      if ((sts = my_pclose(open_pipes->fp)) == -1) retsts = vaxc$errno;
      else if (!(sts & 1)) retsts = sts;
    }
    return retsts;
}

static struct exit_control_block pipe_exitblock = 
       {(struct exit_control_block *) 0,
        pipe_exit_routine, 0, &pipe_exitblock.exit_status, 0};

static void pipe_mbxtofd_ast(pPipe p);
static void pipe_tochild1_ast(pPipe p);
static void pipe_tochild2_ast(pPipe p);

static void
popen_completion_ast(pInfo info)
{
  pInfo i = open_pipes;
  int iss;
  pXpipe x;

  info->completion &= 0x0FFFFFFF; /* strip off "control" field */
  closed_list[closed_index].pid = info->pid;
  closed_list[closed_index].completion = info->completion;
  closed_index++;
  if (closed_index == NKEEPCLOSED) 
    closed_index = 0;
  closed_num++;

  while (i) {
    if (i == info) break;
    i = i->next;
  }
  if (!i) return;       /* unlinked, probably freed too */

  info->done = TRUE;

/*
    Writing to subprocess ...
            if my_pclose'd: EOF already sent, should shutdown chan_in part of pipe

            chan_out may be waiting for "done" flag, or hung waiting
            for i/o completion to child...cancel the i/o.  This will
            put it into "snarf mode" (done but no EOF yet) that discards
            input.

    Output from subprocess (stdout, stderr) needs to be flushed and
    shut down.   We try sending an EOF, but if the mbx is full the pipe
    routine should still catch the "shut_on_empty" flag, telling it to
    use immediate-style reads so that "mbx empty" -> EOF.


*/
  if (info->in && !info->in_done) {               /* only for mode=w */
        if (info->in->shut_on_empty && info->in->need_wake) {
            info->in->need_wake = FALSE;
            _ckvmssts_noperl(sys$dclast(pipe_tochild2_ast,info->in,0));
        } else {
            _ckvmssts_noperl(sys$cancel(info->in->chan_out));
        }
  }

  if (info->out && !info->out_done) {             /* were we also piping output? */
      info->out->shut_on_empty = TRUE;
      iss = sys$qio(0,info->out->chan_in,IO$_WRITEOF|IO$M_NORSWAIT, 0, 0, 0, 0, 0, 0, 0, 0, 0);
      if (iss == SS$_MBFULL) iss = SS$_NORMAL;
      _ckvmssts_noperl(iss);
  }

  if (info->err && !info->err_done) {        /* we were piping stderr */
        info->err->shut_on_empty = TRUE;
        iss = sys$qio(0,info->err->chan_in,IO$_WRITEOF|IO$M_NORSWAIT, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        if (iss == SS$_MBFULL) iss = SS$_NORMAL;
        _ckvmssts_noperl(iss);
  }
  _ckvmssts_noperl(sys$setef(pipe_ef));

}

static unsigned long int setup_cmddsc(pTHX_ char *cmd, int check_img, int *suggest_quote, struct dsc$descriptor_s **pvmscmd);
static void vms_execfree(struct dsc$descriptor_s *vmscmd);

/*
    we actually differ from vmstrnenv since we use this to
    get the RMS IFI to check if SYS$OUTPUT and SYS$ERROR *really*
    are pointing to the same thing
*/

static unsigned short
popen_translate(pTHX_ char *logical, char *result)
{
    int iss;
    $DESCRIPTOR(d_table,"LNM$PROCESS_TABLE");
    $DESCRIPTOR(d_log,"");
    struct _il3 {
        unsigned short length;
        unsigned short code;
        char *         buffer_addr;
        unsigned short *retlenaddr;
    } itmlst[2];
    unsigned short l, ifi;

    d_log.dsc$a_pointer = logical;
    d_log.dsc$w_length  = strlen(logical);

    itmlst[0].code = LNM$_STRING;
    itmlst[0].length = 255;
    itmlst[0].buffer_addr = result;
    itmlst[0].retlenaddr = &l;

    itmlst[1].code = 0;
    itmlst[1].length = 0;
    itmlst[1].buffer_addr = 0;
    itmlst[1].retlenaddr = 0;

    iss = sys$trnlnm(0, &d_table, &d_log, 0, itmlst);
    if (iss == SS$_NOLOGNAM) {
        iss = SS$_NORMAL;
        l = 0;
    }
    if (!(iss&1)) lib$signal(iss);
    result[l] = '\0';
/*
    logicals for PPFs have a 4 byte prefix  ESC+NUL+(RMS IFI)
    strip it off and return the ifi, if any
*/
    ifi  = 0;
    if (result[0] == 0x1b && result[1] == 0x00) {
        memcpy(&ifi,result+2,2);
        strcpy(result,result+4);
    }
    return ifi;     /* this is the RMS internal file id */
}

static void pipe_infromchild_ast(pPipe p);

/*
    I'm using LIB$(GET|FREE)_VM here so that we can allocate and deallocate
    inside an AST routine without worrying about reentrancy and which Perl
    memory allocator is being used.

    We read data and queue up the buffers, then spit them out one at a
    time to the output mailbox when the output mailbox is ready for one.

*/
#define INITIAL_TOCHILDQUEUE  2

static pPipe
pipe_tochild_setup(pTHX_ char *rmbx, char *wmbx)
{
    pPipe p;
    pCBuf b;
    char mbx1[64], mbx2[64];
    struct dsc$descriptor_s d_mbx1 = {sizeof mbx1, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx1},
                            d_mbx2 = {sizeof mbx2, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx2};
    unsigned int dviitm = DVI$_DEVBUFSIZ;
    int j, n;

    New(1368, p, 1, Pipe);

    create_mbx(aTHX_ &p->chan_in , &d_mbx1);
    create_mbx(aTHX_ &p->chan_out, &d_mbx2);
    _ckvmssts(lib$getdvi(&dviitm, &p->chan_in, 0, &p->bufsize));

    p->buf           = 0;
    p->shut_on_empty = FALSE;
    p->need_wake     = FALSE;
    p->type          = 0;
    p->retry         = 0;
    p->iosb.status   = SS$_NORMAL;
    p->iosb2.status  = SS$_NORMAL;
    p->free          = RQE_ZERO;
    p->wait          = RQE_ZERO;
    p->curr          = 0;
    p->curr2         = 0;
    p->info          = 0;
#ifdef PERL_IMPLICIT_CONTEXT
    p->thx	     = aTHX;
#endif

    n = sizeof(CBuf) + p->bufsize;

    for (j = 0; j < INITIAL_TOCHILDQUEUE; j++) {
        _ckvmssts(lib$get_vm(&n, &b));
        b->buf = (char *) b + sizeof(CBuf);
        _ckvmssts(lib$insqhi(b, &p->free));
    }

    pipe_tochild2_ast(p);
    pipe_tochild1_ast(p);
    strcpy(wmbx, mbx1);
    strcpy(rmbx, mbx2);
    return p;
}

/*  reads the MBX Perl is writing, and queues */

static void
pipe_tochild1_ast(pPipe p)
{
    pCBuf b = p->curr;
    int iss = p->iosb.status;
    int eof = (iss == SS$_ENDOFFILE);
#ifdef PERL_IMPLICIT_CONTEXT
    pTHX = p->thx;
#endif

    if (p->retry) {
        if (eof) {
            p->shut_on_empty = TRUE;
            b->eof     = TRUE;
            _ckvmssts(sys$dassgn(p->chan_in));
        } else  {
            _ckvmssts(iss);
        }

        b->eof  = eof;
        b->size = p->iosb.count;
        _ckvmssts(lib$insqhi(b, &p->wait));
        if (p->need_wake) {
            p->need_wake = FALSE;
            _ckvmssts(sys$dclast(pipe_tochild2_ast,p,0));
        }
    } else {
        p->retry = 1;   /* initial call */
    }

    if (eof) {                  /* flush the free queue, return when done */
        int n = sizeof(CBuf) + p->bufsize;
        while (1) {
            iss = lib$remqti(&p->free, &b);
            if (iss == LIB$_QUEWASEMP) return;
            _ckvmssts(iss);
            _ckvmssts(lib$free_vm(&n, &b));
        }
    }

    iss = lib$remqti(&p->free, &b);
    if (iss == LIB$_QUEWASEMP) {
        int n = sizeof(CBuf) + p->bufsize;
        _ckvmssts(lib$get_vm(&n, &b));
        b->buf = (char *) b + sizeof(CBuf);
    } else {
       _ckvmssts(iss);
    }

    p->curr = b;
    iss = sys$qio(0,p->chan_in,
             IO$_READVBLK|(p->shut_on_empty ? IO$M_NOWAIT : 0),
             &p->iosb,
             pipe_tochild1_ast, p, b->buf, p->bufsize, 0, 0, 0, 0);
    if (iss == SS$_ENDOFFILE && p->shut_on_empty) iss = SS$_NORMAL;
    _ckvmssts(iss);
}


/* writes queued buffers to output, waits for each to complete before
   doing the next */

static void
pipe_tochild2_ast(pPipe p)
{
    pCBuf b = p->curr2;
    int iss = p->iosb2.status;
    int n = sizeof(CBuf) + p->bufsize;
    int done = (p->info && p->info->done) ||
              iss == SS$_CANCEL || iss == SS$_ABORT;
#if defined(PERL_IMPLICIT_CONTEXT)
    pTHX = p->thx;
#endif

    do {
        if (p->type) {         /* type=1 has old buffer, dispose */
            if (p->shut_on_empty) {
                _ckvmssts(lib$free_vm(&n, &b));
            } else {
                _ckvmssts(lib$insqhi(b, &p->free));
            }
            p->type = 0;
        }

        iss = lib$remqti(&p->wait, &b);
        if (iss == LIB$_QUEWASEMP) {
            if (p->shut_on_empty) {
                if (done) {
                    _ckvmssts(sys$dassgn(p->chan_out));
                    *p->pipe_done = TRUE;
                    _ckvmssts(sys$setef(pipe_ef));
                } else {
                    _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEOF,
                        &p->iosb2, pipe_tochild2_ast, p, 0, 0, 0, 0, 0, 0));
                }
                return;
            }
            p->need_wake = TRUE;
            return;
        }
        _ckvmssts(iss);
        p->type = 1;
    } while (done);


    p->curr2 = b;
    if (b->eof) {
        _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEOF,
            &p->iosb2, pipe_tochild2_ast, p, 0, 0, 0, 0, 0, 0));
    } else {
        _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEVBLK,
            &p->iosb2, pipe_tochild2_ast, p, b->buf, b->size, 0, 0, 0, 0));
    }

    return;

}


static pPipe
pipe_infromchild_setup(pTHX_ char *rmbx, char *wmbx)
{
    pPipe p;
    char mbx1[64], mbx2[64];
    struct dsc$descriptor_s d_mbx1 = {sizeof mbx1, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx1},
                            d_mbx2 = {sizeof mbx2, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx2};
    unsigned int dviitm = DVI$_DEVBUFSIZ;

    New(1367, p, 1, Pipe);
    create_mbx(aTHX_ &p->chan_in , &d_mbx1);
    create_mbx(aTHX_ &p->chan_out, &d_mbx2);

    _ckvmssts(lib$getdvi(&dviitm, &p->chan_in, 0, &p->bufsize));
    New(1367, p->buf, p->bufsize, char);
    p->shut_on_empty = FALSE;
    p->info   = 0;
    p->type   = 0;
    p->iosb.status = SS$_NORMAL;
#if defined(PERL_IMPLICIT_CONTEXT)
    p->thx = aTHX;
#endif
    pipe_infromchild_ast(p);

    strcpy(wmbx, mbx1);
    strcpy(rmbx, mbx2);
    return p;
}

static void
pipe_infromchild_ast(pPipe p)
{
    int iss = p->iosb.status;
    int eof = (iss == SS$_ENDOFFILE);
    int myeof = (eof && (p->iosb.dvispec == mypid || p->iosb.dvispec == 0));
    int kideof = (eof && (p->iosb.dvispec == p->info->pid));
#if defined(PERL_IMPLICIT_CONTEXT)
    pTHX = p->thx;
#endif

    if (p->info && p->info->closing && p->chan_out)  {           /* output shutdown */
        _ckvmssts(sys$dassgn(p->chan_out));
        p->chan_out = 0;
    }

    /* read completed:
            input shutdown if EOF from self (done or shut_on_empty)
            output shutdown if closing flag set (my_pclose)
            send data/eof from child or eof from self
            otherwise, re-read (snarf of data from child)
    */

    if (p->type == 1) {
        p->type = 0;
        if (myeof && p->chan_in) {                  /* input shutdown */
            _ckvmssts(sys$dassgn(p->chan_in));
            p->chan_in = 0;
        }

        if (p->chan_out) {
            if (myeof || kideof) {      /* pass EOF to parent */
                _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEOF, &p->iosb,
                              pipe_infromchild_ast, p,
                              0, 0, 0, 0, 0, 0));
                return;
            } else if (eof) {       /* eat EOF --- fall through to read*/

            } else {                /* transmit data */
                _ckvmssts(sys$qio(0,p->chan_out,IO$_WRITEVBLK,&p->iosb,
                              pipe_infromchild_ast,p,
                              p->buf, p->iosb.count, 0, 0, 0, 0));
                return;
            }
        }
    }

    /*  everything shut? flag as done */

    if (!p->chan_in && !p->chan_out) {
        *p->pipe_done = TRUE;
        _ckvmssts(sys$setef(pipe_ef));
        return;
    }

    /* write completed (or read, if snarfing from child)
            if still have input active,
               queue read...immediate mode if shut_on_empty so we get EOF if empty
            otherwise,
               check if Perl reading, generate EOFs as needed
    */

    if (p->type == 0) {
        p->type = 1;
        if (p->chan_in) {
            iss = sys$qio(0,p->chan_in,IO$_READVBLK|(p->shut_on_empty ? IO$M_NOW : 0),&p->iosb,
                          pipe_infromchild_ast,p,
                          p->buf, p->bufsize, 0, 0, 0, 0);
            if (p->shut_on_empty && iss == SS$_ENDOFFILE) iss = SS$_NORMAL;
            _ckvmssts(iss);
        } else {           /* send EOFs for extra reads */
            p->iosb.status = SS$_ENDOFFILE;
            p->iosb.dvispec = 0;
            _ckvmssts(sys$qio(0,p->chan_out,IO$_SETMODE|IO$M_READATTN,
                      0, 0, 0,
                      pipe_infromchild_ast, p, 0, 0, 0, 0));
        }
    }
}

static pPipe
pipe_mbxtofd_setup(pTHX_ int fd, char *out)
{
    pPipe p;
    char mbx[64];
    unsigned long dviitm = DVI$_DEVBUFSIZ;
    struct stat s;
    struct dsc$descriptor_s d_mbx = {sizeof mbx, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbx};

    /* things like terminals and mbx's don't need this filter */
    if (fd && fstat(fd,&s) == 0) {
        unsigned long dviitm = DVI$_DEVCHAR, devchar;
        struct dsc$descriptor_s d_dev = {strlen(s.st_dev), DSC$K_DTYPE_T,
                                         DSC$K_CLASS_S, s.st_dev};

        _ckvmssts(lib$getdvi(&dviitm,0,&d_dev,&devchar,0,0));
        if (!(devchar & DEV$M_DIR)) {  /* non directory structured...*/
            strcpy(out, s.st_dev);
            return 0;
        }
    }

    New(1366, p, 1, Pipe);
    p->fd_out = dup(fd);
    create_mbx(aTHX_ &p->chan_in, &d_mbx);
    _ckvmssts(lib$getdvi(&dviitm, &p->chan_in, 0, &p->bufsize));
    New(1366, p->buf, p->bufsize+1, char);
    p->shut_on_empty = FALSE;
    p->retry = 0;
    p->info  = 0;
    strcpy(out, mbx);

    _ckvmssts(sys$qio(0, p->chan_in, IO$_READVBLK, &p->iosb,
                  pipe_mbxtofd_ast, p,
                  p->buf, p->bufsize, 0, 0, 0, 0));

    return p;
}

static void
pipe_mbxtofd_ast(pPipe p)
{
    int iss = p->iosb.status;
    int done = p->info->done;
    int iss2;
    int eof = (iss == SS$_ENDOFFILE);
    int myeof = eof && ((p->iosb.dvispec == mypid)||(p->iosb.dvispec == 0));
    int err = !(iss&1) && !eof;
#if defined(PERL_IMPLICIT_CONTEXT)
    pTHX = p->thx;
#endif

    if (done && myeof) {               /* end piping */
        close(p->fd_out);
        sys$dassgn(p->chan_in);
        *p->pipe_done = TRUE;
        _ckvmssts(sys$setef(pipe_ef));
        return;
    }

    if (!err && !eof) {             /* good data to send to file */
        p->buf[p->iosb.count] = '\n';
        iss2 = write(p->fd_out, p->buf, p->iosb.count+1);
        if (iss2 < 0) {
            p->retry++;
            if (p->retry < MAX_RETRY) {
                _ckvmssts(sys$setimr(0,delaytime,pipe_mbxtofd_ast,p));
                return;
            }
        }
        p->retry = 0;
    } else if (err) {
        _ckvmssts(iss);
    }


    iss = sys$qio(0, p->chan_in, IO$_READVBLK|(p->shut_on_empty ? IO$M_NOW : 0), &p->iosb,
          pipe_mbxtofd_ast, p,
          p->buf, p->bufsize, 0, 0, 0, 0);
    if (p->shut_on_empty && (iss == SS$_ENDOFFILE)) iss = SS$_NORMAL;
    _ckvmssts(iss);
}


typedef struct _pipeloc     PLOC;
typedef struct _pipeloc*   pPLOC;

struct _pipeloc {
    pPLOC   next;
    char    dir[NAM$C_MAXRSS+1];
};
static pPLOC  head_PLOC = 0;

void
free_pipelocs(pTHX_ void *head)
{
    pPLOC p, pnext;
    pPLOC *pHead = (pPLOC *)head;

    p = *pHead;
    while (p) {
        pnext = p->next;
        Safefree(p);
        p = pnext;
    }
    *pHead = 0;
}

static void
store_pipelocs(pTHX)
{
    int    i;
    pPLOC  p;
    AV    *av = 0;
    SV    *dirsv;
    GV    *gv;
    char  *dir, *x;
    char  *unixdir;
    char  temp[NAM$C_MAXRSS+1];
    STRLEN n_a;

    if (head_PLOC)  
        free_pipelocs(aTHX_ &head_PLOC);

/*  the . directory from @INC comes last */

    New(1370,p,1,PLOC);
    p->next = head_PLOC;
    head_PLOC = p;
    strcpy(p->dir,"./");

/*  get the directory from $^X */

#ifdef PERL_IMPLICIT_CONTEXT
    if (aTHX && PL_origargv && PL_origargv[0]) {    /* maybe nul if embedded Perl */
#else
    if (PL_origargv && PL_origargv[0]) {    /* maybe nul if embedded Perl */
#endif
        strcpy(temp, PL_origargv[0]);
        x = strrchr(temp,']');
        if (x) x[1] = '\0';

        if ((unixdir = tounixpath(temp, Nullch)) != Nullch) {
            New(1370,p,1,PLOC);
            p->next = head_PLOC;
            head_PLOC = p;
            strncpy(p->dir,unixdir,sizeof(p->dir)-1);
            p->dir[NAM$C_MAXRSS] = '\0';
        }
    }

/*  reverse order of @INC entries, skip "." since entered above */

#ifdef PERL_IMPLICIT_CONTEXT
    if (aTHX)
#endif
    if (PL_incgv) av = GvAVn(PL_incgv);

    for (i = 0; av && i <= AvFILL(av); i++) {
        dirsv = *av_fetch(av,i,TRUE);

        if (SvROK(dirsv)) continue;
        dir = SvPVx(dirsv,n_a);
        if (strcmp(dir,".") == 0) continue;
        if ((unixdir = tounixpath(dir, Nullch)) == Nullch)
            continue;

        New(1370,p,1,PLOC);
        p->next = head_PLOC;
        head_PLOC = p;
        strncpy(p->dir,unixdir,sizeof(p->dir)-1);
        p->dir[NAM$C_MAXRSS] = '\0';
    }

/* most likely spot (ARCHLIB) put first in the list */

#ifdef ARCHLIB_EXP
    if ((unixdir = tounixpath(ARCHLIB_EXP, Nullch)) != Nullch) {
        New(1370,p,1,PLOC);
        p->next = head_PLOC;
        head_PLOC = p;
        strncpy(p->dir,unixdir,sizeof(p->dir)-1);
        p->dir[NAM$C_MAXRSS] = '\0';
    }
#endif
}


static char *
find_vmspipe(pTHX)
{
    static int   vmspipe_file_status = 0;
    static char  vmspipe_file[NAM$C_MAXRSS+1];

    /* already found? Check and use ... need read+execute permission */

    if (vmspipe_file_status == 1) {
        if (cando_by_name(S_IRUSR, 0, vmspipe_file)
         && cando_by_name(S_IXUSR, 0, vmspipe_file)) {
            return vmspipe_file;
        }
        vmspipe_file_status = 0;
    }

    /* scan through stored @INC, $^X */

    if (vmspipe_file_status == 0) {
        char file[NAM$C_MAXRSS+1];
        pPLOC  p = head_PLOC;

        while (p) {
            strcpy(file, p->dir);
            strncat(file, "vmspipe.com",NAM$C_MAXRSS);
            file[NAM$C_MAXRSS] = '\0';
            p = p->next;

            if (!do_tovmsspec(file,vmspipe_file,0)) continue;

            if (cando_by_name(S_IRUSR, 0, vmspipe_file)
             && cando_by_name(S_IXUSR, 0, vmspipe_file)) {
                vmspipe_file_status = 1;
                return vmspipe_file;
            }
        }
        vmspipe_file_status = -1;   /* failed, use tempfiles */
    }

    return 0;
}

static FILE *
vmspipe_tempfile(pTHX)
{
    char file[NAM$C_MAXRSS+1];
    FILE *fp;
    static int index = 0;
    stat_t s0, s1;

    /* create a tempfile */

    /* we can't go from   W, shr=get to  R, shr=get without
       an intermediate vulnerable state, so don't bother trying...

       and lib$spawn doesn't shr=put, so have to close the write

       So... match up the creation date/time and the FID to
       make sure we're dealing with the same file

    */

    index++;
    sprintf(file,"sys$scratch:perlpipe_%08.8x_%d.com",mypid,index);
    fp = fopen(file,"w");
    if (!fp) {
        sprintf(file,"sys$login:perlpipe_%08.8x_%d.com",mypid,index);
        fp = fopen(file,"w");
        if (!fp) {
            sprintf(file,"sys$disk:[]perlpipe_%08.8x_%d.com",mypid,index);
            fp = fopen(file,"w");
        }
    }
    if (!fp) return 0;  /* we're hosed */

    fprintf(fp,"$! 'f$verify(0)'\n");
    fprintf(fp,"$!  ---  protect against nonstandard definitions ---\n");
    fprintf(fp,"$ perl_cfile  = f$environment(\"procedure\")\n");
    fprintf(fp,"$ perl_define = \"define/nolog\"\n");
    fprintf(fp,"$ perl_on     = \"set noon\"\n");
    fprintf(fp,"$ perl_exit   = \"exit\"\n");
    fprintf(fp,"$ perl_del    = \"delete\"\n");
    fprintf(fp,"$ pif         = \"if\"\n");
    fprintf(fp,"$!  --- define i/o redirection (sys$output set by lib$spawn)\n");
    fprintf(fp,"$ pif perl_popen_in  .nes. \"\" then perl_define/user/name_attributes=confine sys$input  'perl_popen_in'\n");
    fprintf(fp,"$ pif perl_popen_err .nes. \"\" then perl_define/user/name_attributes=confine sys$error  'perl_popen_err'\n");
    fprintf(fp,"$ pif perl_popen_out .nes. \"\" then perl_define      sys$output 'perl_popen_out'\n");
    fprintf(fp,"$!  --- build command line to get max possible length\n");
    fprintf(fp,"$c=perl_popen_cmd0\n"); 
    fprintf(fp,"$c=c+perl_popen_cmd1\n"); 
    fprintf(fp,"$c=c+perl_popen_cmd2\n"); 
    fprintf(fp,"$x=perl_popen_cmd3\n"); 
    fprintf(fp,"$c=c+x\n"); 
    fprintf(fp,"$ perl_on\n");
    fprintf(fp,"$ 'c'\n");
    fprintf(fp,"$ perl_status = $STATUS\n");
    fprintf(fp,"$ perl_del  'perl_cfile'\n");
    fprintf(fp,"$ perl_exit 'perl_status'\n");
    fsync(fileno(fp));

    fgetname(fp, file, 1);
    fstat(fileno(fp), &s0);
    fclose(fp);

    fp = fopen(file,"r","shr=get");
    if (!fp) return 0;
    fstat(fileno(fp), &s1);

    if (s0.st_ino[0] != s1.st_ino[0] ||
        s0.st_ino[1] != s1.st_ino[1] ||
        s0.st_ino[2] != s1.st_ino[2] ||
        s0.st_ctime  != s1.st_ctime  )  {
        fclose(fp);
        return 0;
    }

    return fp;
}



static PerlIO *
safe_popen(pTHX_ char *cmd, char *in_mode, int *psts)
{
    static int handler_set_up = FALSE;
    unsigned long int sts, flags = CLI$M_NOWAIT;
    /* The use of a GLOBAL table (as was done previously) rendered
     * Perl's qx() or `` unusable from a C<$ SET SYMBOL/SCOPE=NOGLOBAL> DCL
     * environment.  Hence we've switched to LOCAL symbol table.
     */
    unsigned int table = LIB$K_CLI_LOCAL_SYM;
    int j, wait = 0;
    char *p, mode[10], symbol[MAX_DCL_SYMBOL+1], *vmspipe;
    char in[512], out[512], err[512], mbx[512];
    FILE *tpipe = 0;
    char tfilebuf[NAM$C_MAXRSS+1];
    pInfo info;
    char cmd_sym_name[20];
    struct dsc$descriptor_s d_symbol= {0, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, symbol};
    struct dsc$descriptor_s vmspipedsc = {0, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, 0};
    struct dsc$descriptor_s d_sym_cmd = {0, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, cmd_sym_name};
    struct dsc$descriptor_s *vmscmd;
    $DESCRIPTOR(d_sym_in ,"PERL_POPEN_IN");
    $DESCRIPTOR(d_sym_out,"PERL_POPEN_OUT");
    $DESCRIPTOR(d_sym_err,"PERL_POPEN_ERR");
                            
    if (!head_PLOC) store_pipelocs(aTHX);   /* at least TRY to use a static vmspipe file */

    /* once-per-program initialization...
       note that the SETAST calls and the dual test of pipe_ef
       makes sure that only the FIRST thread through here does
       the initialization...all other threads wait until it's
       done.

       Yeah, uglier than a pthread call, it's got all the stuff inline
       rather than in a separate routine.
    */

    if (!pipe_ef) {
        _ckvmssts(sys$setast(0));
        if (!pipe_ef) {
            unsigned long int pidcode = JPI$_PID;
            $DESCRIPTOR(d_delay, RETRY_DELAY);
            _ckvmssts(lib$get_ef(&pipe_ef));
            _ckvmssts(lib$getjpi(&pidcode,0,0,&mypid,0,0));
            _ckvmssts(sys$bintim(&d_delay, delaytime));
        }
        if (!handler_set_up) {
          _ckvmssts(sys$dclexh(&pipe_exitblock));
          handler_set_up = TRUE;
        }
        _ckvmssts(sys$setast(1));
    }

    /* see if we can find a VMSPIPE.COM */

    tfilebuf[0] = '@';
    vmspipe = find_vmspipe(aTHX);
    if (vmspipe) {
        strcpy(tfilebuf+1,vmspipe);
    } else {        /* uh, oh...we're in tempfile hell */
        tpipe = vmspipe_tempfile(aTHX);
        if (!tpipe) {       /* a fish popular in Boston */
            if (ckWARN(WARN_PIPE)) {
                Perl_warner(aTHX_ packWARN(WARN_PIPE),"unable to find VMSPIPE.COM for i/o piping");
            }
        return Nullfp;
        }
        fgetname(tpipe,tfilebuf+1,1);
    }
    vmspipedsc.dsc$a_pointer = tfilebuf;
    vmspipedsc.dsc$w_length  = strlen(tfilebuf);

    sts = setup_cmddsc(aTHX_ cmd,0,0,&vmscmd);
    if (!(sts & 1)) { 
      switch (sts) {
        case RMS$_FNF:  case RMS$_DNF:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        case RMS$_SYN:
          set_errno(EINVAL); break;
        case CLI$_BUFOVF: case RMS$_RTB: case CLI$_TKNOVF: case CLI$_RSLOVF:
          set_errno(E2BIG); break;
        case LIB$_INVARG: case LIB$_INVSTRDES: case SS$_ACCVIO: /* shouldn't happen */
          _ckvmssts(sts); /* fall through */
        default:  /* SS$_DUPLNAM, SS$_CLI, resource exhaustion, etc. */
          set_errno(EVMSERR); 
      }
      set_vaxc_errno(sts);
      if (*mode != 'n' && ckWARN(WARN_PIPE)) {
        Perl_warner(aTHX_ packWARN(WARN_PIPE),"Can't pipe \"%*s\": %s", strlen(cmd), cmd, Strerror(errno));
      }
      *psts = sts;
      return Nullfp; 
    }
    New(1301,info,1,Info);
        
    strcpy(mode,in_mode);
    info->mode = *mode;
    info->done = FALSE;
    info->completion = 0;
    info->closing    = FALSE;
    info->in         = 0;
    info->out        = 0;
    info->err        = 0;
    info->fp         = Nullfp;
    info->useFILE    = 0;
    info->waiting    = 0;
    info->in_done    = TRUE;
    info->out_done   = TRUE;
    info->err_done   = TRUE;
    in[0] = out[0] = err[0] = '\0';

    if ((p = strchr(mode,'F')) != NULL) {   /* F -> use FILE* */
        info->useFILE = 1;
        strcpy(p,p+1);
    }
    if ((p = strchr(mode,'W')) != NULL) {   /* W -> wait for completion */
        wait = 1;
        strcpy(p,p+1);
    }

    if (*mode == 'r') {             /* piping from subroutine */

        info->out = pipe_infromchild_setup(aTHX_ mbx,out);
        if (info->out) {
            info->out->pipe_done = &info->out_done;
            info->out_done = FALSE;
            info->out->info = info;
        }
        if (!info->useFILE) {
        info->fp  = PerlIO_open(mbx, mode);
        } else {
            info->fp = (PerlIO *) freopen(mbx, mode, stdin);
            Perl_vmssetuserlnm(aTHX_ "SYS$INPUT",mbx);
        }

        if (!info->fp && info->out) {
            sys$cancel(info->out->chan_out);
        
            while (!info->out_done) {
                int done;
                _ckvmssts(sys$setast(0));
                done = info->out_done;
                if (!done) _ckvmssts(sys$clref(pipe_ef));
                _ckvmssts(sys$setast(1));
                if (!done) _ckvmssts(sys$waitfr(pipe_ef));
            }

            if (info->out->buf) Safefree(info->out->buf);
            Safefree(info->out);
            Safefree(info);
            *psts = RMS$_FNF;
            return Nullfp;
        }

        info->err = pipe_mbxtofd_setup(aTHX_ fileno(stderr), err);
        if (info->err) {
            info->err->pipe_done = &info->err_done;
            info->err_done = FALSE;
            info->err->info = info;
        }

    } else if (*mode == 'w') {      /* piping to subroutine */

        info->out = pipe_mbxtofd_setup(aTHX_ fileno(stdout), out);
        if (info->out) {
            info->out->pipe_done = &info->out_done;
            info->out_done = FALSE;
            info->out->info = info;
        }

        info->err = pipe_mbxtofd_setup(aTHX_ fileno(stderr), err);
        if (info->err) {
            info->err->pipe_done = &info->err_done;
            info->err_done = FALSE;
            info->err->info = info;
        }

        info->in = pipe_tochild_setup(aTHX_ in,mbx);
        if (!info->useFILE) {
        info->fp  = PerlIO_open(mbx, mode);
        } else {
            info->fp = (PerlIO *) freopen(mbx, mode, stdout);
            Perl_vmssetuserlnm(aTHX_ "SYS$OUTPUT",mbx);
        }

        if (info->in) {
            info->in->pipe_done = &info->in_done;
            info->in_done = FALSE;
            info->in->info = info;
        }

        /* error cleanup */
        if (!info->fp && info->in) {
            info->done = TRUE;
            _ckvmssts(sys$qiow(0,info->in->chan_in, IO$_WRITEOF, 0,
                              0, 0, 0, 0, 0, 0, 0, 0));

            while (!info->in_done) {
                int done;
                _ckvmssts(sys$setast(0));
                done = info->in_done;
                if (!done) _ckvmssts(sys$clref(pipe_ef));
                _ckvmssts(sys$setast(1));
                if (!done) _ckvmssts(sys$waitfr(pipe_ef));
            }

            if (info->in->buf) Safefree(info->in->buf);
            Safefree(info->in);
            Safefree(info);
            *psts = RMS$_FNF;
            return Nullfp;
        }
        

    } else if (*mode == 'n') {       /* separate subprocess, no Perl i/o */
        info->out = pipe_mbxtofd_setup(aTHX_ fileno(stdout), out);
        if (info->out) {
            info->out->pipe_done = &info->out_done;
            info->out_done = FALSE;
            info->out->info = info;
        }

        info->err = pipe_mbxtofd_setup(aTHX_ fileno(stderr), err);
        if (info->err) {
            info->err->pipe_done = &info->err_done;
            info->err_done = FALSE;
            info->err->info = info;
        }
    }

    symbol[MAX_DCL_SYMBOL] = '\0';

    strncpy(symbol, in, MAX_DCL_SYMBOL);
    d_symbol.dsc$w_length = strlen(symbol);
    _ckvmssts(lib$set_symbol(&d_sym_in, &d_symbol, &table));

    strncpy(symbol, err, MAX_DCL_SYMBOL);
    d_symbol.dsc$w_length = strlen(symbol);
    _ckvmssts(lib$set_symbol(&d_sym_err, &d_symbol, &table));

    strncpy(symbol, out, MAX_DCL_SYMBOL);
    d_symbol.dsc$w_length = strlen(symbol);
    _ckvmssts(lib$set_symbol(&d_sym_out, &d_symbol, &table));

    p = vmscmd->dsc$a_pointer;
    while (*p && *p != '\n') p++;
    *p = '\0';                                  /* truncate on \n */
    p = vmscmd->dsc$a_pointer;
    while (*p == ' ' || *p == '\t') p++;        /* remove leading whitespace */
    if (*p == '$') p++;                         /* remove leading $ */
    while (*p == ' ' || *p == '\t') p++;

    for (j = 0; j < 4; j++) {
        sprintf(cmd_sym_name,"PERL_POPEN_CMD%d",j);
        d_sym_cmd.dsc$w_length = strlen(cmd_sym_name);

    strncpy(symbol, p, MAX_DCL_SYMBOL);
    d_symbol.dsc$w_length = strlen(symbol);
    _ckvmssts(lib$set_symbol(&d_sym_cmd, &d_symbol, &table));

        if (strlen(p) > MAX_DCL_SYMBOL) {
            p += MAX_DCL_SYMBOL;
        } else {
            p += strlen(p);
        }
    }
    _ckvmssts(sys$setast(0));
    info->next=open_pipes;  /* prepend to list */
    open_pipes=info;
    _ckvmssts(sys$setast(1));
    /* Omit arg 2 (input file) so the child will get the parent's SYS$INPUT
     * and SYS$COMMAND.  vmspipe.com will redefine SYS$INPUT, but we'll still
     * have SYS$COMMAND if we need it.
     */
    _ckvmssts(lib$spawn(&vmspipedsc, 0, &nl_desc, &flags,
                      0, &info->pid, &info->completion,
                      0, popen_completion_ast,info,0,0,0));

    /* if we were using a tempfile, close it now */

    if (tpipe) fclose(tpipe);

    /* once the subprocess is spawned, it has copied the symbols and
       we can get rid of ours */

    for (j = 0; j < 4; j++) {
        sprintf(cmd_sym_name,"PERL_POPEN_CMD%d",j);
        d_sym_cmd.dsc$w_length = strlen(cmd_sym_name);
    _ckvmssts(lib$delete_symbol(&d_sym_cmd, &table));
    }
    _ckvmssts(lib$delete_symbol(&d_sym_in,  &table));
    _ckvmssts(lib$delete_symbol(&d_sym_err, &table));
    _ckvmssts(lib$delete_symbol(&d_sym_out, &table));
    vms_execfree(vmscmd);
        
#ifdef PERL_IMPLICIT_CONTEXT
    if (aTHX) 
#endif
    PL_forkprocess = info->pid;

    if (wait) {
         int done = 0;
         while (!done) {
             _ckvmssts(sys$setast(0));
             done = info->done;
             if (!done) _ckvmssts(sys$clref(pipe_ef));
             _ckvmssts(sys$setast(1));
             if (!done) _ckvmssts(sys$waitfr(pipe_ef));
         }
        *psts = info->completion;
        my_pclose(info->fp);
    } else { 
        *psts = SS$_NORMAL;
    }
    return info->fp;
}  /* end of safe_popen */


/*{{{  PerlIO *my_popen(char *cmd, char *mode)*/
PerlIO *
Perl_my_popen(pTHX_ char *cmd, char *mode)
{
    int sts;
    TAINT_ENV();
    TAINT_PROPER("popen");
    PERL_FLUSHALL_FOR_CHILD;
    return safe_popen(aTHX_ cmd,mode,&sts);
}

/*}}}*/

/*{{{  I32 my_pclose(PerlIO *fp)*/
I32 Perl_my_pclose(pTHX_ PerlIO *fp)
{
    pInfo info, last = NULL;
    unsigned long int retsts;
    int done, iss;
    
    for (info = open_pipes; info != NULL; last = info, info = info->next)
        if (info->fp == fp) break;

    if (info == NULL) {  /* no such pipe open */
      set_errno(ECHILD); /* quoth POSIX */
      set_vaxc_errno(SS$_NONEXPR);
      return -1;
    }

    /* If we were writing to a subprocess, insure that someone reading from
     * the mailbox gets an EOF.  It looks like a simple fclose() doesn't
     * produce an EOF record in the mailbox.
     *
     *  well, at least sometimes it *does*, so we have to watch out for
     *  the first EOF closing the pipe (and DASSGN'ing the channel)...
     */
     if (info->fp) {
        if (!info->useFILE) 
     PerlIO_flush(info->fp);   /* first, flush data */
        else 
            fflush((FILE *)info->fp);
    }

    _ckvmssts(sys$setast(0));
     info->closing = TRUE;
     done = info->done && info->in_done && info->out_done && info->err_done;
     /* hanging on write to Perl's input? cancel it */
     if (info->mode == 'r' && info->out && !info->out_done) {
        if (info->out->chan_out) {
            _ckvmssts(sys$cancel(info->out->chan_out));
            if (!info->out->chan_in) {   /* EOF generation, need AST */
                _ckvmssts(sys$dclast(pipe_infromchild_ast,info->out,0));
            }
        }
     }
     if (info->in && !info->in_done && !info->in->shut_on_empty)  /* EOF if hasn't had one yet */
         _ckvmssts(sys$qio(0,info->in->chan_in,IO$_WRITEOF,0,0,0,
                           0, 0, 0, 0, 0, 0));
    _ckvmssts(sys$setast(1));
    if (info->fp) {
     if (!info->useFILE) 
    PerlIO_close(info->fp);
     else 
        fclose((FILE *)info->fp);
    }
     /*
        we have to wait until subprocess completes, but ALSO wait until all
        the i/o completes...otherwise we'll be freeing the "info" structure
        that the i/o ASTs could still be using...
     */

     while (!done) {
         _ckvmssts(sys$setast(0));
         done = info->done && info->in_done && info->out_done && info->err_done;
         if (!done) _ckvmssts(sys$clref(pipe_ef));
         _ckvmssts(sys$setast(1));
         if (!done) _ckvmssts(sys$waitfr(pipe_ef));
     }
     retsts = info->completion;

    /* remove from list of open pipes */
    _ckvmssts(sys$setast(0));
    if (last) last->next = info->next;
    else open_pipes = info->next;
    _ckvmssts(sys$setast(1));

    /* free buffers and structures */

    if (info->in) {
        if (info->in->buf) Safefree(info->in->buf);
        Safefree(info->in);
    }
    if (info->out) {
        if (info->out->buf) Safefree(info->out->buf);
        Safefree(info->out);
    }
    if (info->err) {
        if (info->err->buf) Safefree(info->err->buf);
        Safefree(info->err);
    }
    Safefree(info);

    return retsts;

}  /* end of my_pclose() */

#if defined(__CRTL_VER) && __CRTL_VER >= 70200000
  /* Roll our own prototype because we want this regardless of whether
   * _VMS_WAIT is defined.
   */
  __pid_t __vms_waitpid( __pid_t __pid, int *__stat_loc, int __options );
#endif
/* sort-of waitpid; special handling of pipe clean-up for subprocesses 
   created with popen(); otherwise partially emulate waitpid() unless 
   we have a suitable one from the CRTL that came with VMS 7.2 and later.
   Also check processes not considered by the CRTL waitpid().
 */
/*{{{Pid_t my_waitpid(Pid_t pid, int *statusp, int flags)*/
Pid_t
Perl_my_waitpid(pTHX_ Pid_t pid, int *statusp, int flags)
{
    pInfo info;
    int done;
    int sts;
    int j;
    
    if (statusp) *statusp = 0;
    
    for (info = open_pipes; info != NULL; info = info->next)
        if (info->pid == pid) break;

    if (info != NULL) {  /* we know about this child */
      while (!info->done) {
          _ckvmssts(sys$setast(0));
          done = info->done;
          if (!done) _ckvmssts(sys$clref(pipe_ef));
          _ckvmssts(sys$setast(1));
          if (!done) _ckvmssts(sys$waitfr(pipe_ef));
      }

      if (statusp) *statusp = info->completion;
      return pid;
    }

    /* child that already terminated? */

    for (j = 0; j < NKEEPCLOSED && j < closed_num; j++) {
        if (closed_list[j].pid == pid) {
            if (statusp) *statusp = closed_list[j].completion;
            return pid;
        }
    }

    /* fall through if this child is not one of our own pipe children */

#if defined(__CRTL_VER) && __CRTL_VER >= 70200000

      /* waitpid() became available in the CRTL as of VMS 7.0, but only
       * in 7.2 did we get a version that fills in the VMS completion
       * status as Perl has always tried to do.
       */

      sts = __vms_waitpid( pid, statusp, flags );

      if ( sts == 0 || !(sts == -1 && errno == ECHILD) ) 
         return sts;

      /* If the real waitpid tells us the child does not exist, we 
       * fall through here to implement waiting for a child that 
       * was created by some means other than exec() (say, spawned
       * from DCL) or to wait for a process that is not a subprocess 
       * of the current process.
       */

#endif /* defined(__CRTL_VER) && __CRTL_VER >= 70200000 */

    {
      $DESCRIPTOR(intdsc,"0 00:00:01");
      unsigned long int ownercode = JPI$_OWNER, ownerpid;
      unsigned long int pidcode = JPI$_PID, mypid;
      unsigned long int interval[2];
      unsigned int jpi_iosb[2];
      struct itmlst_3 jpilist[2] = { 
          {sizeof(ownerpid),        JPI$_OWNER, &ownerpid,        0},
          {                      0,         0,                 0, 0} 
      };

      if (pid <= 0) {
        /* Sorry folks, we don't presently implement rooting around for 
           the first child we can find, and we definitely don't want to
           pass a pid of -1 to $getjpi, where it is a wildcard operation.
         */
        set_errno(ENOTSUP); 
        return -1;
      }

      /* Get the owner of the child so I can warn if it's not mine. If the 
       * process doesn't exist or I don't have the privs to look at it, 
       * I can go home early.
       */
      sts = sys$getjpiw(0,&pid,NULL,&jpilist,&jpi_iosb,NULL,NULL);
      if (sts & 1) sts = jpi_iosb[0];
      if (!(sts & 1)) {
        switch (sts) {
            case SS$_NONEXPR:
                set_errno(ECHILD);
                break;
            case SS$_NOPRIV:
                set_errno(EACCES);
                break;
            default:
                _ckvmssts(sts);
        }
        set_vaxc_errno(sts);
        return -1;
      }

      if (ckWARN(WARN_EXEC)) {
        /* remind folks they are asking for non-standard waitpid behavior */
        _ckvmssts(lib$getjpi(&pidcode,0,0,&mypid,0,0));
        if (ownerpid != mypid)
          Perl_warner(aTHX_ packWARN(WARN_EXEC),
                      "waitpid: process %x is not a child of process %x",
                      pid,mypid);
      }

      /* simply check on it once a second until it's not there anymore. */

      _ckvmssts(sys$bintim(&intdsc,interval));
      while ((sts=lib$getjpi(&ownercode,&pid,0,&ownerpid,0,0)) & 1) {
            _ckvmssts(sys$schdwk(0,0,interval,0));
            _ckvmssts(sys$hiber());
      }
      if (sts == SS$_NONEXPR) sts = SS$_NORMAL;

      _ckvmssts(sts);
      return pid;
    }
}  /* end of waitpid() */
/*}}}*/
/*}}}*/
/*}}}*/

/*{{{ char *my_gconvert(double val, int ndig, int trail, char *buf) */
char *
my_gconvert(double val, int ndig, int trail, char *buf)
{
  static char __gcvtbuf[DBL_DIG+1];
  char *loc;

  loc = buf ? buf : __gcvtbuf;

#ifndef __DECC  /* VAXCRTL gcvt uses E format for numbers < 1 */
  if (val < 1) {
    sprintf(loc,"%.*g",ndig,val);
    return loc;
  }
#endif

  if (val) {
    if (!buf && ndig > DBL_DIG) ndig = DBL_DIG;
    return gcvt(val,ndig,loc);
  }
  else {
    loc[0] = '0'; loc[1] = '\0';
    return loc;
  }

}
/*}}}*/


/*{{{char *do_rmsexpand(char *fspec, char *out, int ts, char *def, unsigned opts)*/
/* Shortcut for common case of simple calls to $PARSE and $SEARCH
 * to expand file specification.  Allows for a single default file
 * specification and a simple mask of options.  If outbuf is non-NULL,
 * it must point to a buffer at least NAM$C_MAXRSS bytes long, into which
 * the resultant file specification is placed.  If outbuf is NULL, the
 * resultant file specification is placed into a static buffer.
 * The third argument, if non-NULL, is taken to be a default file
 * specification string.  The fourth argument is unused at present.
 * rmesexpand() returns the address of the resultant string if
 * successful, and NULL on error.
 */
static char *mp_do_tounixspec(pTHX_ char *, char *, int);

static char *
mp_do_rmsexpand(pTHX_ char *filespec, char *outbuf, int ts, char *defspec, unsigned opts)
{
  static char __rmsexpand_retbuf[NAM$C_MAXRSS+1];
  char vmsfspec[NAM$C_MAXRSS+1], tmpfspec[NAM$C_MAXRSS+1];
  char esa[NAM$C_MAXRSS], *cp, *out = NULL;
  struct FAB myfab = cc$rms_fab;
  struct NAM mynam = cc$rms_nam;
  STRLEN speclen;
  unsigned long int retsts, trimver, trimtype, haslower = 0, isunix = 0;

  if (!filespec || !*filespec) {
    set_vaxc_errno(LIB$_INVARG); set_errno(EINVAL);
    return NULL;
  }
  if (!outbuf) {
    if (ts) out = New(1319,outbuf,NAM$C_MAXRSS+1,char);
    else    outbuf = __rmsexpand_retbuf;
  }
  if ((isunix = (strchr(filespec,'/') != NULL))) {
    if (do_tovmsspec(filespec,vmsfspec,0) == NULL) return NULL;
    filespec = vmsfspec;
  }

  myfab.fab$l_fna = filespec;
  myfab.fab$b_fns = strlen(filespec);
  myfab.fab$l_nam = &mynam;

  if (defspec && *defspec) {
    if (strchr(defspec,'/') != NULL) {
      if (do_tovmsspec(defspec,tmpfspec,0) == NULL) return NULL;
      defspec = tmpfspec;
    }
    myfab.fab$l_dna = defspec;
    myfab.fab$b_dns = strlen(defspec);
  }

  mynam.nam$l_esa = esa;
  mynam.nam$b_ess = sizeof esa;
  mynam.nam$l_rsa = outbuf;
  mynam.nam$b_rss = NAM$C_MAXRSS;

  retsts = sys$parse(&myfab,0,0);
  if (!(retsts & 1)) {
    mynam.nam$b_nop |= NAM$M_SYNCHK;
    if (retsts == RMS$_DNF || retsts == RMS$_DIR || retsts == RMS$_DEV) {
      retsts = sys$parse(&myfab,0,0);
      if (retsts & 1) goto expanded;
    }  
    mynam.nam$l_rlf = NULL; myfab.fab$b_dns = 0;
    (void) sys$parse(&myfab,0,0);  /* Free search context */
    if (out) Safefree(out);
    set_vaxc_errno(retsts);
    if      (retsts == RMS$_PRV) set_errno(EACCES);
    else if (retsts == RMS$_DEV) set_errno(ENODEV);
    else if (retsts == RMS$_DIR) set_errno(ENOTDIR);
    else                         set_errno(EVMSERR);
    return NULL;
  }
  retsts = sys$search(&myfab,0,0);
  if (!(retsts & 1) && retsts != RMS$_FNF) {
    mynam.nam$b_nop |= NAM$M_SYNCHK; mynam.nam$l_rlf = NULL;
    myfab.fab$b_dns = 0; (void) sys$parse(&myfab,0,0);  /* Free search context */
    if (out) Safefree(out);
    set_vaxc_errno(retsts);
    if      (retsts == RMS$_PRV) set_errno(EACCES);
    else                         set_errno(EVMSERR);
    return NULL;
  }

  /* If the input filespec contained any lowercase characters,
   * downcase the result for compatibility with Unix-minded code. */
  expanded:
  for (out = myfab.fab$l_fna; *out; out++)
    if (islower(*out)) { haslower = 1; break; }
  if (mynam.nam$b_rsl) { out = outbuf; speclen = mynam.nam$b_rsl; }
  else                 { out = esa;    speclen = mynam.nam$b_esl; }
  /* Trim off null fields added by $PARSE
   * If type > 1 char, must have been specified in original or default spec
   * (not true for version; $SEARCH may have added version of existing file).
   */
  trimver  = !(mynam.nam$l_fnb & NAM$M_EXP_VER);
  trimtype = !(mynam.nam$l_fnb & NAM$M_EXP_TYPE) &&
             (mynam.nam$l_ver - mynam.nam$l_type == 1);
  if (trimver || trimtype) {
    if (defspec && *defspec) {
      char defesa[NAM$C_MAXRSS];
      struct FAB deffab = cc$rms_fab;
      struct NAM defnam = cc$rms_nam;
     
      deffab.fab$l_nam = &defnam;
      deffab.fab$l_fna = defspec;  deffab.fab$b_fns = myfab.fab$b_dns;
      defnam.nam$l_esa = defesa;   defnam.nam$b_ess = sizeof defesa;
      defnam.nam$b_nop = NAM$M_SYNCHK;
      if (sys$parse(&deffab,0,0) & 1) {
        if (trimver)  trimver  = !(defnam.nam$l_fnb & NAM$M_EXP_VER);
        if (trimtype) trimtype = !(defnam.nam$l_fnb & NAM$M_EXP_TYPE); 
      }
    }
    if (trimver) speclen = mynam.nam$l_ver - out;
    if (trimtype) {
      /* If we didn't already trim version, copy down */
      if (speclen > mynam.nam$l_ver - out)
        memcpy(mynam.nam$l_type, mynam.nam$l_ver, 
               speclen - (mynam.nam$l_ver - out));
      speclen -= mynam.nam$l_ver - mynam.nam$l_type; 
    }
  }
  /* If we just had a directory spec on input, $PARSE "helpfully"
   * adds an empty name and type for us */
  if (mynam.nam$l_name == mynam.nam$l_type &&
      mynam.nam$l_ver  == mynam.nam$l_type + 1 &&
      !(mynam.nam$l_fnb & NAM$M_EXP_NAME))
    speclen = mynam.nam$l_name - out;
  out[speclen] = '\0';
  if (haslower) __mystrtolower(out);

  /* Have we been working with an expanded, but not resultant, spec? */
  /* Also, convert back to Unix syntax if necessary. */
  if (!mynam.nam$b_rsl) {
    if (isunix) {
      if (do_tounixspec(esa,outbuf,0) == NULL) return NULL;
    }
    else strcpy(outbuf,esa);
  }
  else if (isunix) {
    if (do_tounixspec(outbuf,tmpfspec,0) == NULL) return NULL;
    strcpy(outbuf,tmpfspec);
  }
  mynam.nam$b_nop |= NAM$M_SYNCHK; mynam.nam$l_rlf = NULL;
  mynam.nam$l_rsa = NULL; mynam.nam$b_rss = 0;
  myfab.fab$b_dns = 0; (void) sys$parse(&myfab,0,0);  /* Free search context */
  return outbuf;
}
/*}}}*/
/* External entry points */
char *Perl_rmsexpand(pTHX_ char *spec, char *buf, char *def, unsigned opt)
{ return do_rmsexpand(spec,buf,0,def,opt); }
char *Perl_rmsexpand_ts(pTHX_ char *spec, char *buf, char *def, unsigned opt)
{ return do_rmsexpand(spec,buf,1,def,opt); }


/*
** The following routines are provided to make life easier when
** converting among VMS-style and Unix-style directory specifications.
** All will take input specifications in either VMS or Unix syntax. On
** failure, all return NULL.  If successful, the routines listed below
** return a pointer to a buffer containing the appropriately
** reformatted spec (and, therefore, subsequent calls to that routine
** will clobber the result), while the routines of the same names with
** a _ts suffix appended will return a pointer to a mallocd string
** containing the appropriately reformatted spec.
** In all cases, only explicit syntax is altered; no check is made that
** the resulting string is valid or that the directory in question
** actually exists.
**
**   fileify_dirspec() - convert a directory spec into the name of the
**     directory file (i.e. what you can stat() to see if it's a dir).
**     The style (VMS or Unix) of the result is the same as the style
**     of the parameter passed in.
**   pathify_dirspec() - convert a directory spec into a path (i.e.
**     what you prepend to a filename to indicate what directory it's in).
**     The style (VMS or Unix) of the result is the same as the style
**     of the parameter passed in.
**   tounixpath() - convert a directory spec into a Unix-style path.
**   tovmspath() - convert a directory spec into a VMS-style path.
**   tounixspec() - convert any file spec into a Unix-style file spec.
**   tovmsspec() - convert any file spec into a VMS-style spec.
**
** Copyright 1996 by Charles Bailey  <bailey@newman.upenn.edu>
** Permission is given to distribute this code as part of the Perl
** standard distribution under the terms of the GNU General Public
** License or the Perl Artistic License.  Copies of each may be
** found in the Perl standard distribution.
 */

/*{{{ char *fileify_dirspec[_ts](char *path, char *buf)*/
static char *mp_do_fileify_dirspec(pTHX_ char *dir,char *buf,int ts)
{
    static char __fileify_retbuf[NAM$C_MAXRSS+1];
    unsigned long int dirlen, retlen, addmfd = 0, hasfilename = 0;
    char *retspec, *cp1, *cp2, *lastdir;
    char trndir[NAM$C_MAXRSS+2], vmsdir[NAM$C_MAXRSS+1];
    unsigned short int trnlnm_iter_count;

    if (!dir || !*dir) {
      set_errno(EINVAL); set_vaxc_errno(SS$_BADPARAM); return NULL;
    }
    dirlen = strlen(dir);
    while (dirlen && dir[dirlen-1] == '/') --dirlen;
    if (!dirlen) { /* We had Unixish '/' -- substitute top of current tree */
      strcpy(trndir,"/sys$disk/000000");
      dir = trndir;
      dirlen = 16;
    }
    if (dirlen > NAM$C_MAXRSS) {
      set_errno(ENAMETOOLONG); set_vaxc_errno(RMS$_SYN); return NULL;
    }
    if (!strpbrk(dir+1,"/]>:")) {
      strcpy(trndir,*dir == '/' ? dir + 1: dir);
      trnlnm_iter_count = 0;
      while (!strpbrk(trndir,"/]>:>") && my_trnlnm(trndir,trndir,0)) {
        trnlnm_iter_count++; 
        if (trnlnm_iter_count >= PERL_LNM_MAX_ITER) break;
      }
      dir = trndir;
      dirlen = strlen(dir);
    }
    else {
      strncpy(trndir,dir,dirlen);
      trndir[dirlen] = '\0';
      dir = trndir;
    }
    /* If we were handed a rooted logical name or spec, treat it like a
     * simple directory, so that
     *    $ Define myroot dev:[dir.]
     *    ... do_fileify_dirspec("myroot",buf,1) ...
     * does something useful.
     */
    if (dirlen >= 2 && !strcmp(dir+dirlen-2,".]")) {
      dir[--dirlen] = '\0';
      dir[dirlen-1] = ']';
    }
    if (dirlen >= 2 && !strcmp(dir+dirlen-2,".>")) {
      dir[--dirlen] = '\0';
      dir[dirlen-1] = '>';
    }

    if ((cp1 = strrchr(dir,']')) != NULL || (cp1 = strrchr(dir,'>')) != NULL) {
      /* If we've got an explicit filename, we can just shuffle the string. */
      if (*(cp1+1)) hasfilename = 1;
      /* Similarly, we can just back up a level if we've got multiple levels
         of explicit directories in a VMS spec which ends with directories. */
      else {
        for (cp2 = cp1; cp2 > dir; cp2--) {
          if (*cp2 == '.') {
            *cp2 = *cp1; *cp1 = '\0';
            hasfilename = 1;
            break;
          }
          if (*cp2 == '[' || *cp2 == '<') break;
        }
      }
    }

    if (hasfilename || !strpbrk(dir,"]:>")) { /* Unix-style path or filename */
      if (dir[0] == '.') {
        if (dir[1] == '\0' || (dir[1] == '/' && dir[2] == '\0'))
          return do_fileify_dirspec("[]",buf,ts);
        else if (dir[1] == '.' &&
                 (dir[2] == '\0' || (dir[2] == '/' && dir[3] == '\0')))
          return do_fileify_dirspec("[-]",buf,ts);
      }
      if (dirlen && dir[dirlen-1] == '/') {    /* path ends with '/'; just add .dir;1 */
        dirlen -= 1;                 /* to last element */
        lastdir = strrchr(dir,'/');
      }
      else if ((cp1 = strstr(dir,"/.")) != NULL) {
        /* If we have "/." or "/..", VMSify it and let the VMS code
         * below expand it, rather than repeating the code to handle
         * relative components of a filespec here */
        do {
          if (*(cp1+2) == '.') cp1++;
          if (*(cp1+2) == '/' || *(cp1+2) == '\0') {
            if (do_tovmsspec(dir,vmsdir,0) == NULL) return NULL;
            if (strchr(vmsdir,'/') != NULL) {
              /* If do_tovmsspec() returned it, it must have VMS syntax
               * delimiters in it, so it's a mixed VMS/Unix spec.  We take
               * the time to check this here only so we avoid a recursion
               * loop; otherwise, gigo.
               */
              set_errno(EINVAL);  set_vaxc_errno(RMS$_SYN);  return NULL;
            }
            if (do_fileify_dirspec(vmsdir,trndir,0) == NULL) return NULL;
            return do_tounixspec(trndir,buf,ts);
          }
          cp1++;
        } while ((cp1 = strstr(cp1,"/.")) != NULL);
        lastdir = strrchr(dir,'/');
      }
      else if (dirlen >= 7 && !strcmp(&dir[dirlen-7],"/000000")) {
        /* Ditto for specs that end in an MFD -- let the VMS code
         * figure out whether it's a real device or a rooted logical. */
        dir[dirlen] = '/'; dir[dirlen+1] = '\0';
        if (do_tovmsspec(dir,vmsdir,0) == NULL) return NULL;
        if (do_fileify_dirspec(vmsdir,trndir,0) == NULL) return NULL;
        return do_tounixspec(trndir,buf,ts);
      }
      else {
        if ( !(lastdir = cp1 = strrchr(dir,'/')) &&
             !(lastdir = cp1 = strrchr(dir,']')) &&
             !(lastdir = cp1 = strrchr(dir,'>'))) cp1 = dir;
        if ((cp2 = strchr(cp1,'.'))) {  /* look for explicit type */
          int ver; char *cp3;
          if (!*(cp2+1) || toupper(*(cp2+1)) != 'D' ||  /* Wrong type. */
              !*(cp2+2) || toupper(*(cp2+2)) != 'I' ||  /* Bzzt. */
              !*(cp2+3) || toupper(*(cp2+3)) != 'R' ||
              (*(cp2+4) && ((*(cp2+4) != ';' && *(cp2+4) != '.')  ||
              (*(cp2+5) && ((ver = strtol(cp2+5,&cp3,10)) != 1 &&
                            (ver || *cp3)))))) {
            set_errno(ENOTDIR);
            set_vaxc_errno(RMS$_DIR);
            return NULL;
          }
          dirlen = cp2 - dir;
        }
      }
      /* If we lead off with a device or rooted logical, add the MFD
         if we're specifying a top-level directory. */
      if (lastdir && *dir == '/') {
        addmfd = 1;
        for (cp1 = lastdir - 1; cp1 > dir; cp1--) {
          if (*cp1 == '/') {
            addmfd = 0;
            break;
          }
        }
      }
      retlen = dirlen + (addmfd ? 13 : 6);
      if (buf) retspec = buf;
      else if (ts) New(1309,retspec,retlen+1,char);
      else retspec = __fileify_retbuf;
      if (addmfd) {
        dirlen = lastdir - dir;
        memcpy(retspec,dir,dirlen);
        strcpy(&retspec[dirlen],"/000000");
        strcpy(&retspec[dirlen+7],lastdir);
      }
      else {
        memcpy(retspec,dir,dirlen);
        retspec[dirlen] = '\0';
      }
      /* We've picked up everything up to the directory file name.
         Now just add the type and version, and we're set. */
      strcat(retspec,".dir;1");
      return retspec;
    }
    else {  /* VMS-style directory spec */
      char esa[NAM$C_MAXRSS+1], term, *cp;
      unsigned long int sts, cmplen, haslower = 0;
      struct FAB dirfab = cc$rms_fab;
      struct NAM savnam, dirnam = cc$rms_nam;

      dirfab.fab$b_fns = strlen(dir);
      dirfab.fab$l_fna = dir;
      dirfab.fab$l_nam = &dirnam;
      dirfab.fab$l_dna = ".DIR;1";
      dirfab.fab$b_dns = 6;
      dirnam.nam$b_ess = NAM$C_MAXRSS;
      dirnam.nam$l_esa = esa;

      for (cp = dir; *cp; cp++)
        if (islower(*cp)) { haslower = 1; break; }
      if (!((sts = sys$parse(&dirfab))&1)) {
        if (dirfab.fab$l_sts == RMS$_DIR) {
          dirnam.nam$b_nop |= NAM$M_SYNCHK;
          sts = sys$parse(&dirfab) & 1;
        }
        if (!sts) {
          set_errno(EVMSERR);
          set_vaxc_errno(dirfab.fab$l_sts);
          return NULL;
        }
      }
      else {
        savnam = dirnam;
        if (sys$search(&dirfab)&1) {  /* Does the file really exist? */
          /* Yes; fake the fnb bits so we'll check type below */
          dirnam.nam$l_fnb |= NAM$M_EXP_TYPE | NAM$M_EXP_VER;
        }
        else { /* No; just work with potential name */
          if (dirfab.fab$l_sts == RMS$_FNF) dirnam = savnam;
          else { 
            set_errno(EVMSERR);  set_vaxc_errno(dirfab.fab$l_sts);
            dirnam.nam$b_nop |= NAM$M_SYNCHK;  dirnam.nam$l_rlf = NULL;
            dirfab.fab$b_dns = 0;  (void) sys$parse(&dirfab,0,0);
            return NULL;
          }
        }
      }
      if (!(dirnam.nam$l_fnb & (NAM$M_EXP_DEV | NAM$M_EXP_DIR))) {
        cp1 = strchr(esa,']');
        if (!cp1) cp1 = strchr(esa,'>');
        if (cp1) {  /* Should always be true */
          dirnam.nam$b_esl -= cp1 - esa - 1;
          memcpy(esa,cp1 + 1,dirnam.nam$b_esl);
        }
      }
      if (dirnam.nam$l_fnb & NAM$M_EXP_TYPE) {  /* Was type specified? */
        /* Yep; check version while we're at it, if it's there. */
        cmplen = (dirnam.nam$l_fnb & NAM$M_EXP_VER) ? 6 : 4;
        if (strncmp(dirnam.nam$l_type,".DIR;1",cmplen)) { 
          /* Something other than .DIR[;1].  Bzzt. */
          dirnam.nam$b_nop |= NAM$M_SYNCHK;  dirnam.nam$l_rlf = NULL;
          dirfab.fab$b_dns = 0;  (void) sys$parse(&dirfab,0,0);
          set_errno(ENOTDIR);
          set_vaxc_errno(RMS$_DIR);
          return NULL;
        }
      }
      esa[dirnam.nam$b_esl] = '\0';
      if (dirnam.nam$l_fnb & NAM$M_EXP_NAME) {
        /* They provided at least the name; we added the type, if necessary, */
        if (buf) retspec = buf;                            /* in sys$parse() */
        else if (ts) New(1311,retspec,dirnam.nam$b_esl+1,char);
        else retspec = __fileify_retbuf;
        strcpy(retspec,esa);
        dirnam.nam$b_nop |= NAM$M_SYNCHK;  dirnam.nam$l_rlf = NULL;
        dirfab.fab$b_dns = 0;  (void) sys$parse(&dirfab,0,0);
        return retspec;
      }
      if ((cp1 = strstr(esa,".][000000]")) != NULL) {
        for (cp2 = cp1 + 9; *cp2; cp1++,cp2++) *cp1 = *cp2;
        *cp1 = '\0';
        dirnam.nam$b_esl -= 9;
      }
      if ((cp1 = strrchr(esa,']')) == NULL) cp1 = strrchr(esa,'>');
      if (cp1 == NULL) { /* should never happen */
        dirnam.nam$b_nop |= NAM$M_SYNCHK;  dirnam.nam$l_rlf = NULL;
        dirfab.fab$b_dns = 0;  (void) sys$parse(&dirfab,0,0);
        return NULL;
      }
      term = *cp1;
      *cp1 = '\0';
      retlen = strlen(esa);
      if ((cp1 = strrchr(esa,'.')) != NULL) {
        /* There's more than one directory in the path.  Just roll back. */
        *cp1 = term;
        if (buf) retspec = buf;
        else if (ts) New(1311,retspec,retlen+7,char);
        else retspec = __fileify_retbuf;
        strcpy(retspec,esa);
      }
      else {
        if (dirnam.nam$l_fnb & NAM$M_ROOT_DIR) {
          /* Go back and expand rooted logical name */
          dirnam.nam$b_nop = NAM$M_SYNCHK | NAM$M_NOCONCEAL;
          if (!(sys$parse(&dirfab) & 1)) {
            dirnam.nam$l_rlf = NULL;
            dirfab.fab$b_dns = 0;  (void) sys$parse(&dirfab,0,0);
            set_errno(EVMSERR);
            set_vaxc_errno(dirfab.fab$l_sts);
            return NULL;
          }
          retlen = dirnam.nam$b_esl - 9; /* esa - '][' - '].DIR;1' */
          if (buf) retspec = buf;
          else if (ts) New(1312,retspec,retlen+16,char);
          else retspec = __fileify_retbuf;
          cp1 = strstr(esa,"][");
          if (!cp1) cp1 = strstr(esa,"]<");
          dirlen = cp1 - esa;
          memcpy(retspec,esa,dirlen);
          if (!strncmp(cp1+2,"000000]",7)) {
            retspec[dirlen-1] = '\0';
            for (cp1 = retspec+dirlen-1; *cp1 != '.' && *cp1 != '['; cp1--) ;
            if (*cp1 == '.') *cp1 = ']';
            else {
              memmove(cp1+8,cp1+1,retspec+dirlen-cp1);
              memcpy(cp1+1,"000000]",7);
            }
          }
          else {
            memcpy(retspec+dirlen,cp1+2,retlen-dirlen);
            retspec[retlen] = '\0';
            /* Convert last '.' to ']' */
            for (cp1 = retspec+retlen-1; *cp1 != '.' && *cp1 != '['; cp1--) ;
            if (*cp1 == '.') *cp1 = ']';
            else {
              memmove(cp1+8,cp1+1,retspec+dirlen-cp1);
              memcpy(cp1+1,"000000]",7);
            }
          }
        }
        else {  /* This is a top-level dir.  Add the MFD to the path. */
          if (buf) retspec = buf;
          else if (ts) New(1312,retspec,retlen+16,char);
          else retspec = __fileify_retbuf;
          cp1 = esa;
          cp2 = retspec;
          while (*cp1 != ':') *(cp2++) = *(cp1++);
          strcpy(cp2,":[000000]");
          cp1 += 2;
          strcpy(cp2+9,cp1);
        }
      }
      dirnam.nam$b_nop |= NAM$M_SYNCHK;  dirnam.nam$l_rlf = NULL;
      dirfab.fab$b_dns = 0;  (void) sys$parse(&dirfab,0,0);
      /* We've set up the string up through the filename.  Add the
         type and version, and we're done. */
      strcat(retspec,".DIR;1");

      /* $PARSE may have upcased filespec, so convert output to lower
       * case if input contained any lowercase characters. */
      if (haslower) __mystrtolower(retspec);
      return retspec;
    }
}  /* end of do_fileify_dirspec() */
/*}}}*/
/* External entry points */
char *Perl_fileify_dirspec(pTHX_ char *dir, char *buf)
{ return do_fileify_dirspec(dir,buf,0); }
char *Perl_fileify_dirspec_ts(pTHX_ char *dir, char *buf)
{ return do_fileify_dirspec(dir,buf,1); }

/*{{{ char *pathify_dirspec[_ts](char *path, char *buf)*/
static char *mp_do_pathify_dirspec(pTHX_ char *dir,char *buf, int ts)
{
    static char __pathify_retbuf[NAM$C_MAXRSS+1];
    unsigned long int retlen;
    char *retpath, *cp1, *cp2, trndir[NAM$C_MAXRSS+1];
    unsigned short int trnlnm_iter_count;
    STRLEN trnlen;

    if (!dir || !*dir) {
      set_errno(EINVAL); set_vaxc_errno(SS$_BADPARAM); return NULL;
    }

    if (*dir) strcpy(trndir,dir);
    else getcwd(trndir,sizeof trndir - 1);

    trnlnm_iter_count = 0;
    while (!strpbrk(trndir,"/]:>") && !no_translate_barewords
	   && my_trnlnm(trndir,trndir,0)) {
      trnlnm_iter_count++; 
      if (trnlnm_iter_count >= PERL_LNM_MAX_ITER) break;
      trnlen = strlen(trndir);

      /* Trap simple rooted lnms, and return lnm:[000000] */
      if (!strcmp(trndir+trnlen-2,".]")) {
        if (buf) retpath = buf;
        else if (ts) New(1318,retpath,strlen(dir)+10,char);
        else retpath = __pathify_retbuf;
        strcpy(retpath,dir);
        strcat(retpath,":[000000]");
        return retpath;
      }
    }
    dir = trndir;

    if (!strpbrk(dir,"]:>")) { /* Unix-style path or plain name */
      if (*dir == '.' && (*(dir+1) == '\0' ||
                          (*(dir+1) == '.' && *(dir+2) == '\0')))
        retlen = 2 + (*(dir+1) != '\0');
      else {
        if ( !(cp1 = strrchr(dir,'/')) &&
             !(cp1 = strrchr(dir,']')) &&
             !(cp1 = strrchr(dir,'>')) ) cp1 = dir;
        if ((cp2 = strchr(cp1,'.')) != NULL &&
            (*(cp2-1) != '/' ||                /* Trailing '.', '..', */
             !(*(cp2+1) == '\0' ||             /* or '...' are dirs.  */
              (*(cp2+1) == '.' && *(cp2+2) == '\0') ||
              (*(cp2+1) == '.' && *(cp2+2) == '.' && *(cp2+3) == '\0')))) {
          int ver; char *cp3;
          if (!*(cp2+1) || toupper(*(cp2+1)) != 'D' ||  /* Wrong type. */
              !*(cp2+2) || toupper(*(cp2+2)) != 'I' ||  /* Bzzt. */
              !*(cp2+3) || toupper(*(cp2+3)) != 'R' ||
              (*(cp2+4) && ((*(cp2+4) != ';' && *(cp2+4) != '.')  ||
              (*(cp2+5) && ((ver = strtol(cp2+5,&cp3,10)) != 1 &&
                            (ver || *cp3)))))) {
            set_errno(ENOTDIR);
            set_vaxc_errno(RMS$_DIR);
            return NULL;
          }
          retlen = cp2 - dir + 1;
        }
        else {  /* No file type present.  Treat the filename as a directory. */
          retlen = strlen(dir) + 1;
        }
      }
      if (buf) retpath = buf;
      else if (ts) New(1313,retpath,retlen+1,char);
      else retpath = __pathify_retbuf;
      strncpy(retpath,dir,retlen-1);
      if (retpath[retlen-2] != '/') { /* If the path doesn't already end */
        retpath[retlen-1] = '/';      /* with '/', add it. */
        retpath[retlen] = '\0';
      }
      else retpath[retlen-1] = '\0';
    }
    else {  /* VMS-style directory spec */
      char esa[NAM$C_MAXRSS+1], *cp;
      unsigned long int sts, cmplen, haslower;
      struct FAB dirfab = cc$rms_fab;
      struct NAM savnam, dirnam = cc$rms_nam;

      /* If we've got an explicit filename, we can just shuffle the string. */
      if ( ( (cp1 = strrchr(dir,']')) != NULL ||
             (cp1 = strrchr(dir,'>')) != NULL     ) && *(cp1+1)) {
        if ((cp2 = strchr(cp1,'.')) != NULL) {
          int ver; char *cp3;
          if (!*(cp2+1) || toupper(*(cp2+1)) != 'D' ||  /* Wrong type. */
              !*(cp2+2) || toupper(*(cp2+2)) != 'I' ||  /* Bzzt. */
              !*(cp2+3) || toupper(*(cp2+3)) != 'R' ||
              (*(cp2+4) && ((*(cp2+4) != ';' && *(cp2+4) != '.')  ||
              (*(cp2+5) && ((ver = strtol(cp2+5,&cp3,10)) != 1 &&
                            (ver || *cp3)))))) {
            set_errno(ENOTDIR);
            set_vaxc_errno(RMS$_DIR);
            return NULL;
          }
        }
        else {  /* No file type, so just draw name into directory part */
          for (cp2 = cp1; *cp2; cp2++) ;
        }
        *cp2 = *cp1;
        *(cp2+1) = '\0';  /* OK; trndir is guaranteed to be long enough */
        *cp1 = '.';
        /* We've now got a VMS 'path'; fall through */
      }
      dirfab.fab$b_fns = strlen(dir);
      dirfab.fab$l_fna = dir;
      if (dir[dirfab.fab$b_fns-1] == ']' ||
          dir[dirfab.fab$b_fns-1] == '>' ||
          dir[dirfab.fab$b_fns-1] == ':') { /* It's already a VMS 'path' */
        if (buf) retpath = buf;
        else if (ts) New(1314,retpath,strlen(dir)+1,char);
        else retpath = __pathify_retbuf;
        strcpy(retpath,dir);
        return retpath;
      } 
      dirfab.fab$l_dna = ".DIR;1";
      dirfab.fab$b_dns = 6;
      dirfab.fab$l_nam = &dirnam;
      dirnam.nam$b_ess = (unsigned char) sizeof esa - 1;
      dirnam.nam$l_esa = esa;

      for (cp = dir; *cp; cp++)
        if (islower(*cp)) { haslower = 1; break; }

      if (!(sts = (sys$parse(&dirfab)&1))) {
        if (dirfab.fab$l_sts == RMS$_DIR) {
          dirnam.nam$b_nop |= NAM$M_SYNCHK;
          sts = sys$parse(&dirfab) & 1;
        }
        if (!sts) {
          set_errno(EVMSERR);
          set_vaxc_errno(dirfab.fab$l_sts);
          return NULL;
        }
      }
      else {
        savnam = dirnam;
        if (!(sys$search(&dirfab)&1)) {  /* Does the file really exist? */
          if (dirfab.fab$l_sts != RMS$_FNF) {
            dirnam.nam$b_nop |= NAM$M_SYNCHK;  dirnam.nam$l_rlf = NULL;
            dirfab.fab$b_dns = 0;  (void) sys$parse(&dirfab,0,0);
            set_errno(EVMSERR);
            set_vaxc_errno(dirfab.fab$l_sts);
            return NULL;
          }
          dirnam = savnam; /* No; just work with potential name */
        }
      }
      if (dirnam.nam$l_fnb & NAM$M_EXP_TYPE) {  /* Was type specified? */
        /* Yep; check version while we're at it, if it's there. */
        cmplen = (dirnam.nam$l_fnb & NAM$M_EXP_VER) ? 6 : 4;
        if (strncmp(dirnam.nam$l_type,".DIR;1",cmplen)) { 
          /* Something other than .DIR[;1].  Bzzt. */
          dirnam.nam$b_nop |= NAM$M_SYNCHK;  dirnam.nam$l_rlf = NULL;
          dirfab.fab$b_dns = 0;  (void) sys$parse(&dirfab,0,0);
          set_errno(ENOTDIR);
          set_vaxc_errno(RMS$_DIR);
          return NULL;
        }
      }
      /* OK, the type was fine.  Now pull any file name into the
         directory path. */
      if ((cp1 = strrchr(esa,']'))) *dirnam.nam$l_type = ']';
      else {
        cp1 = strrchr(esa,'>');
        *dirnam.nam$l_type = '>';
      }
      *cp1 = '.';
      *(dirnam.nam$l_type + 1) = '\0';
      retlen = dirnam.nam$l_type - esa + 2;
      if (buf) retpath = buf;
      else if (ts) New(1314,retpath,retlen,char);
      else retpath = __pathify_retbuf;
      strcpy(retpath,esa);
      dirnam.nam$b_nop |= NAM$M_SYNCHK;  dirnam.nam$l_rlf = NULL;
      dirfab.fab$b_dns = 0;  (void) sys$parse(&dirfab,0,0);
      /* $PARSE may have upcased filespec, so convert output to lower
       * case if input contained any lowercase characters. */
      if (haslower) __mystrtolower(retpath);
    }

    return retpath;
}  /* end of do_pathify_dirspec() */
/*}}}*/
/* External entry points */
char *Perl_pathify_dirspec(pTHX_ char *dir, char *buf)
{ return do_pathify_dirspec(dir,buf,0); }
char *Perl_pathify_dirspec_ts(pTHX_ char *dir, char *buf)
{ return do_pathify_dirspec(dir,buf,1); }

/*{{{ char *tounixspec[_ts](char *path, char *buf)*/
static char *mp_do_tounixspec(pTHX_ char *spec, char *buf, int ts)
{
  static char __tounixspec_retbuf[NAM$C_MAXRSS+1];
  char *dirend, *rslt, *cp1, *cp2, *cp3, tmp[NAM$C_MAXRSS+1];
  int devlen, dirlen, retlen = NAM$C_MAXRSS+1;
  int expand = 1; /* guarantee room for leading and trailing slashes */
  unsigned short int trnlnm_iter_count;

  if (spec == NULL) return NULL;
  if (strlen(spec) > NAM$C_MAXRSS) return NULL;
  if (buf) rslt = buf;
  else if (ts) {
    retlen = strlen(spec);
    cp1 = strchr(spec,'[');
    if (!cp1) cp1 = strchr(spec,'<');
    if (cp1) {
      for (cp1++; *cp1; cp1++) {
        if (*cp1 == '-') expand++; /* VMS  '-' ==> Unix '../' */
        if (*cp1 == '.' && *(cp1+1) == '.' && *(cp1+2) == '.')
          { expand++; cp1 +=2; } /* VMS '...' ==> Unix '/.../' */
      }
    }
    New(1315,rslt,retlen+2+2*expand,char);
  }
  else rslt = __tounixspec_retbuf;
  if (strchr(spec,'/') != NULL) {
    strcpy(rslt,spec);
    return rslt;
  }

  cp1 = rslt;
  cp2 = spec;
  dirend = strrchr(spec,']');
  if (dirend == NULL) dirend = strrchr(spec,'>');
  if (dirend == NULL) dirend = strchr(spec,':');
  if (dirend == NULL) {
    strcpy(rslt,spec);
    return rslt;
  }
  if (*cp2 != '[' && *cp2 != '<') {
    *(cp1++) = '/';
  }
  else {  /* the VMS spec begins with directories */
    cp2++;
    if (*cp2 == ']' || *cp2 == '>') {
      *(cp1++) = '.'; *(cp1++) = '/'; *(cp1++) = '\0';
      return rslt;
    }
    else if ( *cp2 != '.' && *cp2 != '-') { /* add the implied device */
      if (getcwd(tmp,sizeof tmp,1) == NULL) {
        if (ts) Safefree(rslt);
        return NULL;
      }
      trnlnm_iter_count = 0;
      do {
        cp3 = tmp;
        while (*cp3 != ':' && *cp3) cp3++;
        *(cp3++) = '\0';
        if (strchr(cp3,']') != NULL) break;
        trnlnm_iter_count++; 
        if (trnlnm_iter_count >= PERL_LNM_MAX_ITER+1) break;
      } while (vmstrnenv(tmp,tmp,0,fildev,0));
      if (ts && !buf &&
          ((devlen = strlen(tmp)) + (dirlen = strlen(cp2)) + 1 > retlen)) {
        retlen = devlen + dirlen;
        Renew(rslt,retlen+1+2*expand,char);
        cp1 = rslt;
      }
      cp3 = tmp;
      *(cp1++) = '/';
      while (*cp3) {
        *(cp1++) = *(cp3++);
        if (cp1 - rslt > NAM$C_MAXRSS && !ts && !buf) return NULL; /* No room */
      }
      *(cp1++) = '/';
    }
    else if ( *cp2 == '.') {
      if (*(cp2+1) == '.' && *(cp2+2) == '.') {
        *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '/';
        cp2 += 3;
      }
      else cp2++;
    }
  }
  for (; cp2 <= dirend; cp2++) {
    if (*cp2 == ':') {
      *(cp1++) = '/';
      if (*(cp2+1) == '[') cp2++;
    }
    else if (*cp2 == ']' || *cp2 == '>') {
      if (*(cp1-1) != '/') *(cp1++) = '/'; /* Don't double after ellipsis */
    }
    else if (*cp2 == '.') {
      *(cp1++) = '/';
      if (*(cp2+1) == ']' || *(cp2+1) == '>') {
        while (*(cp2+1) == ']' || *(cp2+1) == '>' ||
               *(cp2+1) == '[' || *(cp2+1) == '<') cp2++;
        if (!strncmp(cp2,"[000000",7) && (*(cp2+7) == ']' ||
            *(cp2+7) == '>' || *(cp2+7) == '.')) cp2 += 7;
      }
      else if ( *(cp2+1) == '.' && *(cp2+2) == '.') {
        *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) ='/';
        cp2 += 2;
      }
    }
    else if (*cp2 == '-') {
      if (*(cp2-1) == '[' || *(cp2-1) == '<' || *(cp2-1) == '.') {
        while (*cp2 == '-') {
          cp2++;
          *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '/';
        }
        if (*cp2 != '.' && *cp2 != ']' && *cp2 != '>') { /* we don't allow */
          if (ts) Safefree(rslt);                        /* filespecs like */
          set_errno(EINVAL); set_vaxc_errno(RMS$_SYN);   /* [fred.--foo.bar] */
          return NULL;
        }
      }
      else *(cp1++) = *cp2;
    }
    else *(cp1++) = *cp2;
  }
  while (*cp2) *(cp1++) = *(cp2++);
  *cp1 = '\0';

  return rslt;

}  /* end of do_tounixspec() */
/*}}}*/
/* External entry points */
char *Perl_tounixspec(pTHX_ char *spec, char *buf) { return do_tounixspec(spec,buf,0); }
char *Perl_tounixspec_ts(pTHX_ char *spec, char *buf) { return do_tounixspec(spec,buf,1); }

/*{{{ char *tovmsspec[_ts](char *path, char *buf)*/
static char *mp_do_tovmsspec(pTHX_ char *path, char *buf, int ts) {
  static char __tovmsspec_retbuf[NAM$C_MAXRSS+1];
  char *rslt, *dirend;
  register char *cp1, *cp2;
  unsigned long int infront = 0, hasdir = 1;

  if (path == NULL) return NULL;
  if (buf) rslt = buf;
  else if (ts) New(1316,rslt,strlen(path)+9,char);
  else rslt = __tovmsspec_retbuf;
  if (strpbrk(path,"]:>") ||
      (dirend = strrchr(path,'/')) == NULL) {
    if (path[0] == '.') {
      if (path[1] == '\0') strcpy(rslt,"[]");
      else if (path[1] == '.' && path[2] == '\0') strcpy(rslt,"[-]");
      else strcpy(rslt,path); /* probably garbage */
    }
    else strcpy(rslt,path);
    return rslt;
  }
  if (*(dirend+1) == '.') {  /* do we have trailing "/." or "/.." or "/..."? */
    if (!*(dirend+2)) dirend +=2;
    if (*(dirend+2) == '.' && !*(dirend+3)) dirend += 3;
    if (*(dirend+2) == '.' && *(dirend+3) == '.' && !*(dirend+4)) dirend += 4;
  }
  cp1 = rslt;
  cp2 = path;
  if (*cp2 == '/') {
    char trndev[NAM$C_MAXRSS+1];
    int islnm, rooted;
    STRLEN trnend;

    while (*(cp2+1) == '/') cp2++;  /* Skip multiple /s */
    if (!*(cp2+1)) {
      if (!buf & ts) Renew(rslt,18,char);
      strcpy(rslt,"sys$disk:[000000]");
      return rslt;
    }
    while (*(++cp2) != '/' && *cp2) *(cp1++) = *cp2;
    *cp1 = '\0';
    islnm =  my_trnlnm(rslt,trndev,0);
    trnend = islnm ? strlen(trndev) - 1 : 0;
    islnm =  trnend ? (trndev[trnend] == ']' || trndev[trnend] == '>') : 0;
    rooted = islnm ? (trndev[trnend-1] == '.') : 0;
    /* If the first element of the path is a logical name, determine
     * whether it has to be translated so we can add more directories. */
    if (!islnm || rooted) {
      *(cp1++) = ':';
      *(cp1++) = '[';
      if (cp2 == dirend) while (infront++ < 6) *(cp1++) = '0';
      else cp2++;
    }
    else {
      if (cp2 != dirend) {
        if (!buf && ts) Renew(rslt,strlen(path)-strlen(rslt)+trnend+4,char);
        strcpy(rslt,trndev);
        cp1 = rslt + trnend;
        *(cp1++) = '.';
        cp2++;
      }
      else {
        *(cp1++) = ':';
        hasdir = 0;
      }
    }
  }
  else {
    *(cp1++) = '[';
    if (*cp2 == '.') {
      if (*(cp2+1) == '/' || *(cp2+1) == '\0') {
        cp2 += 2;         /* skip over "./" - it's redundant */
        *(cp1++) = '.';   /* but it does indicate a relative dirspec */
      }
      else if (*(cp2+1) == '.' && (*(cp2+2) == '/' || *(cp2+2) == '\0')) {
        *(cp1++) = '-';                                 /* "../" --> "-" */
        cp2 += 3;
      }
      else if (*(cp2+1) == '.' && *(cp2+2) == '.' &&
               (*(cp2+3) == '/' || *(cp2+3) == '\0')) {
        *(cp1++) = '.'; *(cp1++) = '.'; *(cp1++) = '.'; /* ".../" --> "..." */
        if (!*(cp2+4)) *(cp1++) = '.'; /* Simulate trailing '/' for later */
        cp2 += 4;
      }
      if (cp2 > dirend) cp2 = dirend;
    }
    else *(cp1++) = '.';
  }
  for (; cp2 < dirend; cp2++) {
    if (*cp2 == '/') {
      if (*(cp2-1) == '/') continue;
      if (*(cp1-1) != '.') *(cp1++) = '.';
      infront = 0;
    }
    else if (!infront && *cp2 == '.') {
      if (cp2+1 == dirend || *(cp2+1) == '\0') { cp2++; break; }
      else if (*(cp2+1) == '/') cp2++;   /* skip over "./" - it's redundant */
      else if (*(cp2+1) == '.' && (*(cp2+2) == '/' || *(cp2+2) == '\0')) {
        if (*(cp1-1) == '-' || *(cp1-1) == '[') *(cp1++) = '-'; /* handle "../" */
        else if (*(cp1-2) == '[') *(cp1-1) = '-';
        else {  /* back up over previous directory name */
          cp1--;
          while (*(cp1-1) != '.' && *(cp1-1) != '[') cp1--;
          if (*(cp1-1) == '[') {
            memcpy(cp1,"000000.",7);
            cp1 += 7;
          }
        }
        cp2 += 2;
        if (cp2 == dirend) break;
      }
      else if ( *(cp2+1) == '.' && *(cp2+2) == '.' &&
                (*(cp2+3) == '/' || *(cp2+3) == '\0') ) {
        if (*(cp1-1) != '.') *(cp1++) = '.'; /* May already have 1 from '/' */
        *(cp1++) = '.'; *(cp1++) = '.'; /* ".../" --> "..." */
        if (!*(cp2+3)) { 
          *(cp1++) = '.';  /* Simulate trailing '/' */
          cp2 += 2;  /* for loop will incr this to == dirend */
        }
        else cp2 += 3;  /* Trailing '/' was there, so skip it, too */
      }
      else *(cp1++) = '_';  /* fix up syntax - '.' in name not allowed */
    }
    else {
      if (!infront && *(cp1-1) == '-')  *(cp1++) = '.';
      if (*cp2 == '.')      *(cp1++) = '_';
      else                  *(cp1++) =  *cp2;
      infront = 1;
    }
  }
  if (*(cp1-1) == '.') cp1--; /* Unix spec ending in '/' ==> trailing '.' */
  if (hasdir) *(cp1++) = ']';
  if (*cp2) cp2++;  /* check in case we ended with trailing '..' */
  while (*cp2) *(cp1++) = *(cp2++);
  *cp1 = '\0';

  return rslt;

}  /* end of do_tovmsspec() */
/*}}}*/
/* External entry points */
char *Perl_tovmsspec(pTHX_ char *path, char *buf) { return do_tovmsspec(path,buf,0); }
char *Perl_tovmsspec_ts(pTHX_ char *path, char *buf) { return do_tovmsspec(path,buf,1); }

/*{{{ char *tovmspath[_ts](char *path, char *buf)*/
static char *mp_do_tovmspath(pTHX_ char *path, char *buf, int ts) {
  static char __tovmspath_retbuf[NAM$C_MAXRSS+1];
  int vmslen;
  char pathified[NAM$C_MAXRSS+1], vmsified[NAM$C_MAXRSS+1], *cp;

  if (path == NULL) return NULL;
  if (do_pathify_dirspec(path,pathified,0) == NULL) return NULL;
  if (do_tovmsspec(pathified,buf ? buf : vmsified,0) == NULL) return NULL;
  if (buf) return buf;
  else if (ts) {
    vmslen = strlen(vmsified);
    New(1317,cp,vmslen+1,char);
    memcpy(cp,vmsified,vmslen);
    cp[vmslen] = '\0';
    return cp;
  }
  else {
    strcpy(__tovmspath_retbuf,vmsified);
    return __tovmspath_retbuf;
  }

}  /* end of do_tovmspath() */
/*}}}*/
/* External entry points */
char *Perl_tovmspath(pTHX_ char *path, char *buf) { return do_tovmspath(path,buf,0); }
char *Perl_tovmspath_ts(pTHX_ char *path, char *buf) { return do_tovmspath(path,buf,1); }


/*{{{ char *tounixpath[_ts](char *path, char *buf)*/
static char *mp_do_tounixpath(pTHX_ char *path, char *buf, int ts) {
  static char __tounixpath_retbuf[NAM$C_MAXRSS+1];
  int unixlen;
  char pathified[NAM$C_MAXRSS+1], unixified[NAM$C_MAXRSS+1], *cp;

  if (path == NULL) return NULL;
  if (do_pathify_dirspec(path,pathified,0) == NULL) return NULL;
  if (do_tounixspec(pathified,buf ? buf : unixified,0) == NULL) return NULL;
  if (buf) return buf;
  else if (ts) {
    unixlen = strlen(unixified);
    New(1317,cp,unixlen+1,char);
    memcpy(cp,unixified,unixlen);
    cp[unixlen] = '\0';
    return cp;
  }
  else {
    strcpy(__tounixpath_retbuf,unixified);
    return __tounixpath_retbuf;
  }

}  /* end of do_tounixpath() */
/*}}}*/
/* External entry points */
char *Perl_tounixpath(pTHX_ char *path, char *buf) { return do_tounixpath(path,buf,0); }
char *Perl_tounixpath_ts(pTHX_ char *path, char *buf) { return do_tounixpath(path,buf,1); }

/*
 * @(#)argproc.c 2.2 94/08/16	Mark Pizzolato (mark@infocomm.com)
 *
 *****************************************************************************
 *                                                                           *
 *  Copyright (C) 1989-1994 by                                               *
 *  Mark Pizzolato - INFO COMM, Danville, California  (510) 837-5600         *
 *                                                                           *
 *  Permission is hereby  granted for the reproduction of this software,     *
 *  on condition that this copyright notice is included in the reproduction, *
 *  and that such reproduction is not for purposes of profit or material     *
 *  gain.                                                                    *
 *                                                                           *
 *  27-Aug-1994 Modified for inclusion in perl5                              *
 *              by Charles Bailey  bailey@newman.upenn.edu                   *
 *****************************************************************************
 */

/*
 * getredirection() is intended to aid in porting C programs
 * to VMS (Vax-11 C).  The native VMS environment does not support 
 * '>' and '<' I/O redirection, or command line wild card expansion, 
 * or a command line pipe mechanism using the '|' AND background 
 * command execution '&'.  All of these capabilities are provided to any
 * C program which calls this procedure as the first thing in the 
 * main program.
 * The piping mechanism will probably work with almost any 'filter' type
 * of program.  With suitable modification, it may useful for other
 * portability problems as well.
 *
 * Author:  Mark Pizzolato	mark@infocomm.com
 */
struct list_item
    {
    struct list_item *next;
    char *value;
    };

static void add_item(struct list_item **head,
		     struct list_item **tail,
		     char *value,
		     int *count);

static void mp_expand_wild_cards(pTHX_ char *item,
				struct list_item **head,
				struct list_item **tail,
				int *count);

static int background_process(pTHX_ int argc, char **argv);

static void pipe_and_fork(pTHX_ char **cmargv);

/*{{{ void getredirection(int *ac, char ***av)*/
static void
mp_getredirection(pTHX_ int *ac, char ***av)
/*
 * Process vms redirection arg's.  Exit if any error is seen.
 * If getredirection() processes an argument, it is erased
 * from the vector.  getredirection() returns a new argc and argv value.
 * In the event that a background command is requested (by a trailing "&"),
 * this routine creates a background subprocess, and simply exits the program.
 *
 * Warning: do not try to simplify the code for vms.  The code
 * presupposes that getredirection() is called before any data is
 * read from stdin or written to stdout.
 *
 * Normal usage is as follows:
 *
 *	main(argc, argv)
 *	int		argc;
 *    	char		*argv[];
 *	{
 *		getredirection(&argc, &argv);
 *	}
 */
{
    int			argc = *ac;	/* Argument Count	  */
    char		**argv = *av;	/* Argument Vector	  */
    char		*ap;   		/* Argument pointer	  */
    int	       		j;		/* argv[] index		  */
    int			item_count = 0;	/* Count of Items in List */
    struct list_item 	*list_head = 0;	/* First Item in List	    */
    struct list_item	*list_tail;	/* Last Item in List	    */
    char 		*in = NULL;	/* Input File Name	    */
    char 		*out = NULL;	/* Output File Name	    */
    char 		*outmode = "w";	/* Mode to Open Output File */
    char 		*err = NULL;	/* Error File Name	    */
    char 		*errmode = "w";	/* Mode to Open Error File  */
    int			cmargc = 0;    	/* Piped Command Arg Count  */
    char		**cmargv = NULL;/* Piped Command Arg Vector */

    /*
     * First handle the case where the last thing on the line ends with
     * a '&'.  This indicates the desire for the command to be run in a
     * subprocess, so we satisfy that desire.
     */
    ap = argv[argc-1];
    if (0 == strcmp("&", ap))
       exit(background_process(aTHX_ --argc, argv));
    if (*ap && '&' == ap[strlen(ap)-1])
	{
	ap[strlen(ap)-1] = '\0';
       exit(background_process(aTHX_ argc, argv));
	}
    /*
     * Now we handle the general redirection cases that involve '>', '>>',
     * '<', and pipes '|'.
     */
    for (j = 0; j < argc; ++j)
	{
	if (0 == strcmp("<", argv[j]))
	    {
	    if (j+1 >= argc)
		{
		fprintf(stderr,"No input file after < on command line");
		exit(LIB$_WRONUMARG);
		}
	    in = argv[++j];
	    continue;
	    }
	if ('<' == *(ap = argv[j]))
	    {
	    in = 1 + ap;
	    continue;
	    }
	if (0 == strcmp(">", ap))
	    {
	    if (j+1 >= argc)
		{
		fprintf(stderr,"No output file after > on command line");
		exit(LIB$_WRONUMARG);
		}
	    out = argv[++j];
	    continue;
	    }
	if ('>' == *ap)
	    {
	    if ('>' == ap[1])
		{
		outmode = "a";
		if ('\0' == ap[2])
		    out = argv[++j];
		else
		    out = 2 + ap;
		}
	    else
		out = 1 + ap;
	    if (j >= argc)
		{
		fprintf(stderr,"No output file after > or >> on command line");
		exit(LIB$_WRONUMARG);
		}
	    continue;
	    }
	if (('2' == *ap) && ('>' == ap[1]))
	    {
	    if ('>' == ap[2])
		{
		errmode = "a";
		if ('\0' == ap[3])
		    err = argv[++j];
		else
		    err = 3 + ap;
		}
	    else
		if ('\0' == ap[2])
		    err = argv[++j];
		else
		    err = 2 + ap;
	    if (j >= argc)
		{
		fprintf(stderr,"No output file after 2> or 2>> on command line");
		exit(LIB$_WRONUMARG);
		}
	    continue;
	    }
	if (0 == strcmp("|", argv[j]))
	    {
	    if (j+1 >= argc)
		{
		fprintf(stderr,"No command into which to pipe on command line");
		exit(LIB$_WRONUMARG);
		}
	    cmargc = argc-(j+1);
	    cmargv = &argv[j+1];
	    argc = j;
	    continue;
	    }
	if ('|' == *(ap = argv[j]))
	    {
	    ++argv[j];
	    cmargc = argc-j;
	    cmargv = &argv[j];
	    argc = j;
	    continue;
	    }
	expand_wild_cards(ap, &list_head, &list_tail, &item_count);
	}
    /*
     * Allocate and fill in the new argument vector, Some Unix's terminate
     * the list with an extra null pointer.
     */
    New(1302, argv, item_count+1, char *);
    *av = argv;
    for (j = 0; j < item_count; ++j, list_head = list_head->next)
	argv[j] = list_head->value;
    *ac = item_count;
    if (cmargv != NULL)
	{
	if (out != NULL)
	    {
	    fprintf(stderr,"'|' and '>' may not both be specified on command line");
	    exit(LIB$_INVARGORD);
	    }
	pipe_and_fork(aTHX_ cmargv);
	}
	
    /* Check for input from a pipe (mailbox) */

    if (in == NULL && 1 == isapipe(0))
	{
	char mbxname[L_tmpnam];
	long int bufsize;
	long int dvi_item = DVI$_DEVBUFSIZ;
	$DESCRIPTOR(mbxnam, "");
	$DESCRIPTOR(mbxdevnam, "");

	/* Input from a pipe, reopen it in binary mode to disable	*/
	/* carriage control processing.	 				*/

	fgetname(stdin, mbxname);
	mbxnam.dsc$a_pointer = mbxname;
	mbxnam.dsc$w_length = strlen(mbxnam.dsc$a_pointer);	
	lib$getdvi(&dvi_item, 0, &mbxnam, &bufsize, 0, 0);
	mbxdevnam.dsc$a_pointer = mbxname;
	mbxdevnam.dsc$w_length = sizeof(mbxname);
	dvi_item = DVI$_DEVNAM;
	lib$getdvi(&dvi_item, 0, &mbxnam, 0, &mbxdevnam, &mbxdevnam.dsc$w_length);
	mbxdevnam.dsc$a_pointer[mbxdevnam.dsc$w_length] = '\0';
	set_errno(0);
	set_vaxc_errno(1);
	freopen(mbxname, "rb", stdin);
	if (errno != 0)
	    {
	    fprintf(stderr,"Can't reopen input pipe (name: %s) in binary mode",mbxname);
	    exit(vaxc$errno);
	    }
	}
    if ((in != NULL) && (NULL == freopen(in, "r", stdin, "mbc=32", "mbf=2")))
	{
	fprintf(stderr,"Can't open input file %s as stdin",in);
	exit(vaxc$errno);
	}
    if ((out != NULL) && (NULL == freopen(out, outmode, stdout, "mbc=32", "mbf=2")))
	{	
	fprintf(stderr,"Can't open output file %s as stdout",out);
	exit(vaxc$errno);
	}
	if (out != NULL) Perl_vmssetuserlnm(aTHX_ "SYS$OUTPUT",out);

    if (err != NULL) {
        if (strcmp(err,"&1") == 0) {
            dup2(fileno(stdout), fileno(stderr));
            Perl_vmssetuserlnm(aTHX_ "SYS$ERROR","SYS$OUTPUT");
        } else {
	FILE *tmperr;
	if (NULL == (tmperr = fopen(err, errmode, "mbc=32", "mbf=2")))
	    {
	    fprintf(stderr,"Can't open error file %s as stderr",err);
	    exit(vaxc$errno);
	    }
	    fclose(tmperr);
           if (NULL == freopen(err, "a", stderr, "mbc=32", "mbf=2"))
		{
		exit(vaxc$errno);
		}
	    Perl_vmssetuserlnm(aTHX_ "SYS$ERROR",err);
	}
        }
#ifdef ARGPROC_DEBUG
    PerlIO_printf(Perl_debug_log, "Arglist:\n");
    for (j = 0; j < *ac;  ++j)
	PerlIO_printf(Perl_debug_log, "argv[%d] = '%s'\n", j, argv[j]);
#endif
   /* Clear errors we may have hit expanding wildcards, so they don't
      show up in Perl's $! later */
   set_errno(0); set_vaxc_errno(1);
}  /* end of getredirection() */
/*}}}*/

static void add_item(struct list_item **head,
		     struct list_item **tail,
		     char *value,
		     int *count)
{
    if (*head == 0)
	{
	New(1303,*head,1,struct list_item);
	*tail = *head;
	}
    else {
	New(1304,(*tail)->next,1,struct list_item);
	*tail = (*tail)->next;
	}
    (*tail)->value = value;
    ++(*count);
}

static void mp_expand_wild_cards(pTHX_ char *item,
			      struct list_item **head,
			      struct list_item **tail,
			      int *count)
{
int expcount = 0;
unsigned long int context = 0;
int isunix = 0;
int item_len = 0;
char *had_version;
char *had_device;
int had_directory;
char *devdir,*cp;
char vmsspec[NAM$C_MAXRSS+1];
$DESCRIPTOR(filespec, "");
$DESCRIPTOR(defaultspec, "SYS$DISK:[]");
$DESCRIPTOR(resultspec, "");
unsigned long int zero = 0, sts;

    for (cp = item; *cp; cp++) {
	if (*cp == '*' || *cp == '%' || isspace(*cp)) break;
	if (*cp == '.' && *(cp-1) == '.' && *(cp-2) =='.') break;
    }
    if (!*cp || isspace(*cp))
	{
	add_item(head, tail, item, count);
	return;
	}
    else
        {
     /* "double quoted" wild card expressions pass as is */
     /* From DCL that means using e.g.:                  */
     /* perl program """perl.*"""                        */
     item_len = strlen(item);
     if ( '"' == *item && '"' == item[item_len-1] )
       {
       item++;
       item[item_len-2] = '\0';
       add_item(head, tail, item, count);
       return;
       }
     }
    resultspec.dsc$b_dtype = DSC$K_DTYPE_T;
    resultspec.dsc$b_class = DSC$K_CLASS_D;
    resultspec.dsc$a_pointer = NULL;
    if ((isunix = (int) strchr(item,'/')) != (int) NULL)
      filespec.dsc$a_pointer = do_tovmsspec(item,vmsspec,0);
    if (!isunix || !filespec.dsc$a_pointer)
      filespec.dsc$a_pointer = item;
    filespec.dsc$w_length = strlen(filespec.dsc$a_pointer);
    /*
     * Only return version specs, if the caller specified a version
     */
    had_version = strchr(item, ';');
    /*
     * Only return device and directory specs, if the caller specifed either.
     */
    had_device = strchr(item, ':');
    had_directory = (isunix || NULL != strchr(item, '[')) || (NULL != strchr(item, '<'));
    
    while (1 == (1 & (sts = lib$find_file(&filespec, &resultspec, &context,
    				  &defaultspec, 0, 0, &zero))))
	{
	char *string;
	char *c;

	New(1305,string,resultspec.dsc$w_length+1,char);
	strncpy(string, resultspec.dsc$a_pointer, resultspec.dsc$w_length);
	string[resultspec.dsc$w_length] = '\0';
	if (NULL == had_version)
	    *((char *)strrchr(string, ';')) = '\0';
	if ((!had_directory) && (had_device == NULL))
	    {
	    if (NULL == (devdir = strrchr(string, ']')))
		devdir = strrchr(string, '>');
	    strcpy(string, devdir + 1);
	    }
	/*
	 * Be consistent with what the C RTL has already done to the rest of
	 * the argv items and lowercase all of these names.
	 */
	for (c = string; *c; ++c)
	    if (isupper(*c))
		*c = tolower(*c);
	if (isunix) trim_unixpath(string,item,1);
	add_item(head, tail, string, count);
	++expcount;
	}
    if (sts != RMS$_NMF)
	{
	set_vaxc_errno(sts);
	switch (sts)
	    {
	    case RMS$_FNF: case RMS$_DNF:
		set_errno(ENOENT); break;
	    case RMS$_DIR:
		set_errno(ENOTDIR); break;
	    case RMS$_DEV:
		set_errno(ENODEV); break;
	    case RMS$_FNM: case RMS$_SYN:
		set_errno(EINVAL); break;
	    case RMS$_PRV:
		set_errno(EACCES); break;
	    default:
		_ckvmssts_noperl(sts);
	    }
	}
    if (expcount == 0)
	add_item(head, tail, item, count);
    _ckvmssts_noperl(lib$sfree1_dd(&resultspec));
    _ckvmssts_noperl(lib$find_file_end(&context));
}

static int child_st[2];/* Event Flag set when child process completes	*/

static unsigned short child_chan;/* I/O Channel for Pipe Mailbox		*/

static unsigned long int exit_handler(int *status)
{
short iosb[4];

    if (0 == child_st[0])
	{
#ifdef ARGPROC_DEBUG
	PerlIO_printf(Perl_debug_log, "Waiting for Child Process to Finish . . .\n");
#endif
	fflush(stdout);	    /* Have to flush pipe for binary data to	*/
			    /* terminate properly -- <tp@mccall.com>	*/
	sys$qiow(0, child_chan, IO$_WRITEOF, iosb, 0, 0, 0, 0, 0, 0, 0, 0);
	sys$dassgn(child_chan);
	fclose(stdout);
	sys$synch(0, child_st);
	}
    return(1);
}

static void sig_child(int chan)
{
#ifdef ARGPROC_DEBUG
    PerlIO_printf(Perl_debug_log, "Child Completion AST\n");
#endif
    if (child_st[0] == 0)
	child_st[0] = 1;
}

static struct exit_control_block exit_block =
    {
    0,
    exit_handler,
    1,
    &exit_block.exit_status,
    0
    };

static void 
pipe_and_fork(pTHX_ char **cmargv)
{
    PerlIO *fp;
    struct dsc$descriptor_s *vmscmd;
    char subcmd[2*MAX_DCL_LINE_LENGTH], *p, *q;
    int sts, j, l, ismcr, quote, tquote = 0;

    sts = setup_cmddsc(aTHX_ cmargv[0],0,&quote,&vmscmd);
    vms_execfree(vmscmd);

    j = l = 0;
    p = subcmd;
    q = cmargv[0];
    ismcr = q && toupper(*q) == 'M'     && toupper(*(q+1)) == 'C' 
              && toupper(*(q+2)) == 'R' && !*(q+3);

    while (q && l < MAX_DCL_LINE_LENGTH) {
        if (!*q) {
            if (j > 0 && quote) {
                *p++ = '"';
                l++;
            }
            q = cmargv[++j];
            if (q) {
                if (ismcr && j > 1) quote = 1;
                tquote =  (strchr(q,' ')) != NULL || *q == '\0';
                *p++ = ' ';
                l++;
                if (quote || tquote) {
                    *p++ = '"';
                    l++;
                }
	}
        } else {
            if ((quote||tquote) && *q == '"') {
                *p++ = '"';
                l++;
	}
            *p++ = *q++;
            l++;
        }
    }
    *p = '\0';

    fp = safe_popen(aTHX_ subcmd,"wbF",&sts);
    if (fp == Nullfp) {
        PerlIO_printf(Perl_debug_log,"Can't open output pipe (status %d)",sts);
	}
}

static int background_process(pTHX_ int argc, char **argv)
{
char command[2048] = "$";
$DESCRIPTOR(value, "");
static $DESCRIPTOR(cmd, "BACKGROUND$COMMAND");
static $DESCRIPTOR(null, "NLA0:");
static $DESCRIPTOR(pidsymbol, "SHELL_BACKGROUND_PID");
char pidstring[80];
$DESCRIPTOR(pidstr, "");
int pid;
unsigned long int flags = 17, one = 1, retsts;

    strcat(command, argv[0]);
    while (--argc)
	{
	strcat(command, " \"");
	strcat(command, *(++argv));
	strcat(command, "\"");
	}
    value.dsc$a_pointer = command;
    value.dsc$w_length = strlen(value.dsc$a_pointer);
    _ckvmssts_noperl(lib$set_symbol(&cmd, &value));
    retsts = lib$spawn(&cmd, &null, 0, &flags, 0, &pid);
    if (retsts == 0x38250) { /* DCL-W-NOTIFY - We must be BATCH, so retry */
	_ckvmssts_noperl(lib$spawn(&cmd, &null, 0, &one, 0, &pid));
    }
    else {
	_ckvmssts_noperl(retsts);
    }
#ifdef ARGPROC_DEBUG
    PerlIO_printf(Perl_debug_log, "%s\n", command);
#endif
    sprintf(pidstring, "%08X", pid);
    PerlIO_printf(Perl_debug_log, "%s\n", pidstring);
    pidstr.dsc$a_pointer = pidstring;
    pidstr.dsc$w_length = strlen(pidstr.dsc$a_pointer);
    lib$set_symbol(&pidsymbol, &pidstr);
    return(SS$_NORMAL);
}
/*}}}*/
/***** End of code taken from Mark Pizzolato's argproc.c package *****/


/* OS-specific initialization at image activation (not thread startup) */
/* Older VAXC header files lack these constants */
#ifndef JPI$_RIGHTS_SIZE
#  define JPI$_RIGHTS_SIZE 817
#endif
#ifndef KGB$M_SUBSYSTEM
#  define KGB$M_SUBSYSTEM 0x8
#endif

/*{{{void vms_image_init(int *, char ***)*/
void
vms_image_init(int *argcp, char ***argvp)
{
  char eqv[LNM$C_NAMLENGTH+1] = "";
  unsigned int len, tabct = 8, tabidx = 0;
  unsigned long int *mask, iosb[2], i, rlst[128], rsz;
  unsigned long int iprv[(sizeof(union prvdef) + sizeof(unsigned long int) - 1) / sizeof(unsigned long int)];
  unsigned short int dummy, rlen;
  struct dsc$descriptor_s **tabvec;
#if defined(PERL_IMPLICIT_CONTEXT)
  pTHX = NULL;
#endif
  struct itmlst_3 jpilist[4] = { {sizeof iprv,    JPI$_IMAGPRIV, iprv, &dummy},
                                 {sizeof rlst,  JPI$_RIGHTSLIST, rlst,  &rlen},
                                 { sizeof rsz, JPI$_RIGHTS_SIZE, &rsz, &dummy},
                                 {          0,                0,    0,      0} };

#ifdef KILL_BY_SIGPRC
    (void) Perl_csighandler_init();
#endif

  _ckvmssts_noperl(sys$getjpiw(0,NULL,NULL,jpilist,iosb,NULL,NULL));
  _ckvmssts_noperl(iosb[0]);
  for (i = 0; i < sizeof iprv / sizeof(unsigned long int); i++) {
    if (iprv[i]) {           /* Running image installed with privs? */
      _ckvmssts_noperl(sys$setprv(0,iprv,0,NULL));       /* Turn 'em off. */
      will_taint = TRUE;
      break;
    }
  }
  /* Rights identifiers might trigger tainting as well. */
  if (!will_taint && (rlen || rsz)) {
    while (rlen < rsz) {
      /* We didn't get all the identifiers on the first pass.  Allocate a
       * buffer much larger than $GETJPI wants (rsz is size in bytes that
       * were needed to hold all identifiers at time of last call; we'll
       * allocate that many unsigned long ints), and go back and get 'em.
       * If it gave us less than it wanted to despite ample buffer space, 
       * something's broken.  Is your system missing a system identifier?
       */
      if (rsz <= jpilist[1].buflen) { 
         /* Perl_croak accvios when used this early in startup. */
         fprintf(stderr, "vms_image_init: $getjpiw refuses to store RIGHTSLIST of %u bytes in buffer of %u bytes.\n%s", 
                         rsz, (unsigned long) jpilist[1].buflen,
                         "Check your rights database for corruption.\n");
         exit(SS$_ABORT);
      }
      if (jpilist[1].bufadr != rlst) Safefree(jpilist[1].bufadr);
      jpilist[1].bufadr = New(1320,mask,rsz,unsigned long int);
      jpilist[1].buflen = rsz * sizeof(unsigned long int);
      _ckvmssts_noperl(sys$getjpiw(0,NULL,NULL,&jpilist[1],iosb,NULL,NULL));
      _ckvmssts_noperl(iosb[0]);
    }
    mask = jpilist[1].bufadr;
    /* Check attribute flags for each identifier (2nd longword); protected
     * subsystem identifiers trigger tainting.
     */
    for (i = 1; i < (rlen + sizeof(unsigned long int) - 1) / sizeof(unsigned long int); i += 2) {
      if (mask[i] & KGB$M_SUBSYSTEM) {
        will_taint = TRUE;
        break;
      }
    }
    if (mask != rlst) Safefree(mask);
  }
  /* We need to use this hack to tell Perl it should run with tainting,
   * since its tainting flag may be part of the PL_curinterp struct, which
   * hasn't been allocated when vms_image_init() is called.
   */
  if (will_taint) {
    char **newargv, **oldargv;
    oldargv = *argvp;
    New(1320,newargv,(*argcp)+2,char *);
    newargv[0] = oldargv[0];
    New(1320,newargv[1],3,char);
    strcpy(newargv[1], "-T");
    Copy(&oldargv[1],&newargv[2],(*argcp)-1,char **);
    (*argcp)++;
    newargv[*argcp] = NULL;
    /* We orphan the old argv, since we don't know where it's come from,
     * so we don't know how to free it.
     */
    *argvp = newargv;
  }
  else {  /* Did user explicitly request tainting? */
    int i;
    char *cp, **av = *argvp;
    for (i = 1; i < *argcp; i++) {
      if (*av[i] != '-') break;
      for (cp = av[i]+1; *cp; cp++) {
        if (*cp == 'T') { will_taint = 1; break; }
        else if ( (*cp == 'd' || *cp == 'V') && *(cp+1) == ':' ||
                  strchr("DFIiMmx",*cp)) break;
      }
      if (will_taint) break;
    }
  }

  for (tabidx = 0;
       len = my_trnlnm("PERL_ENV_TABLES",eqv,tabidx);
       tabidx++) {
    if (!tabidx) New(1321,tabvec,tabct,struct dsc$descriptor_s *);
    else if (tabidx >= tabct) {
      tabct += 8;
      Renew(tabvec,tabct,struct dsc$descriptor_s *);
    }
    New(1322,tabvec[tabidx],1,struct dsc$descriptor_s);
    tabvec[tabidx]->dsc$w_length  = 0;
    tabvec[tabidx]->dsc$b_dtype   = DSC$K_DTYPE_T;
    tabvec[tabidx]->dsc$b_class   = DSC$K_CLASS_D;
    tabvec[tabidx]->dsc$a_pointer = NULL;
    _ckvmssts_noperl(lib$scopy_r_dx(&len,eqv,tabvec[tabidx]));
  }
  if (tabidx) { tabvec[tabidx] = NULL; env_tables = tabvec; }

  getredirection(argcp,argvp);
#if ( defined(USE_5005THREADS) || defined(USE_ITHREADS) ) && ( defined(__DECC) || defined(__DECCXX) )
  {
# include <reentrancy.h>
  (void) decc$set_reentrancy(C$C_MULTITHREAD);
  }
#endif
  return;
}
/*}}}*/


/* trim_unixpath()
 * Trim Unix-style prefix off filespec, so it looks like what a shell
 * glob expansion would return (i.e. from specified prefix on, not
 * full path).  Note that returned filespec is Unix-style, regardless
 * of whether input filespec was VMS-style or Unix-style.
 *
 * fspec is filespec to be trimmed, and wildspec is wildcard spec used to
 * determine prefix (both may be in VMS or Unix syntax).  opts is a bit
 * vector of options; at present, only bit 0 is used, and if set tells
 * trim unixpath to try the current default directory as a prefix when
 * presented with a possibly ambiguous ... wildcard.
 *
 * Returns !=0 on success, with trimmed filespec replacing contents of
 * fspec, and 0 on failure, with contents of fpsec unchanged.
 */
/*{{{int trim_unixpath(char *fspec, char *wildspec, int opts)*/
int
Perl_trim_unixpath(pTHX_ char *fspec, char *wildspec, int opts)
{
  char unixified[NAM$C_MAXRSS+1], unixwild[NAM$C_MAXRSS+1],
       *template, *base, *end, *cp1, *cp2;
  register int tmplen, reslen = 0, dirs = 0;

  if (!wildspec || !fspec) return 0;
  if (strpbrk(wildspec,"]>:") != NULL) {
    if (do_tounixspec(wildspec,unixwild,0) == NULL) return 0;
    else template = unixwild;
  }
  else template = wildspec;
  if (strpbrk(fspec,"]>:") != NULL) {
    if (do_tounixspec(fspec,unixified,0) == NULL) return 0;
    else base = unixified;
    /* reslen != 0 ==> we had to unixify resultant filespec, so we must
     * check to see that final result fits into (isn't longer than) fspec */
    reslen = strlen(fspec);
  }
  else base = fspec;

  /* No prefix or absolute path on wildcard, so nothing to remove */
  if (!*template || *template == '/') {
    if (base == fspec) return 1;
    tmplen = strlen(unixified);
    if (tmplen > reslen) return 0;  /* not enough space */
    /* Copy unixified resultant, including trailing NUL */
    memmove(fspec,unixified,tmplen+1);
    return 1;
  }

  for (end = base; *end; end++) ;  /* Find end of resultant filespec */
  if ((cp1 = strstr(template,".../")) == NULL) { /* No ...; just count elts */
    for (cp1 = template; *cp1; cp1++) if (*cp1 == '/') dirs++;
    for (cp1 = end ;cp1 >= base; cp1--)
      if ((*cp1 == '/') && !dirs--) /* postdec so we get front of rel path */
        { cp1++; break; }
    if (cp1 != fspec) memmove(fspec,cp1, end - cp1 + 1);
    return 1;
  }
  else {
    char tpl[NAM$C_MAXRSS+1], lcres[NAM$C_MAXRSS+1];
    char *front, *nextell, *lcend, *lcfront, *ellipsis = cp1;
    int ells = 1, totells, segdirs, match;
    struct dsc$descriptor_s wilddsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, tpl},
                            resdsc =  {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};

    while ((cp1 = strstr(ellipsis+4,".../")) != NULL) {ellipsis = cp1; ells++;}
    totells = ells;
    for (cp1 = ellipsis+4; *cp1; cp1++) if (*cp1 == '/') dirs++;
    if (ellipsis == template && opts & 1) {
      /* Template begins with an ellipsis.  Since we can't tell how many
       * directory names at the front of the resultant to keep for an
       * arbitrary starting point, we arbitrarily choose the current
       * default directory as a starting point.  If it's there as a prefix,
       * clip it off.  If not, fall through and act as if the leading
       * ellipsis weren't there (i.e. return shortest possible path that
       * could match template).
       */
      if (getcwd(tpl, sizeof tpl,0) == NULL) return 0;
      for (cp1 = tpl, cp2 = base; *cp1 && *cp2; cp1++,cp2++)
        if (_tolower(*cp1) != _tolower(*cp2)) break;
      segdirs = dirs - totells;  /* Min # of dirs we must have left */
      for (front = cp2+1; *front; front++) if (*front == '/') segdirs--;
      if (*cp1 == '\0' && *cp2 == '/' && segdirs < 1) {
        memcpy(fspec,cp2+1,end - cp2);
        return 1;
      }
    }
    /* First off, back up over constant elements at end of path */
    if (dirs) {
      for (front = end ; front >= base; front--)
         if (*front == '/' && !dirs--) { front++; break; }
    }
    for (cp1=template,cp2=lcres; *cp1 && cp2 <= lcres + sizeof lcres;
         cp1++,cp2++) *cp2 = _tolower(*cp1);  /* Make lc copy for match */
    if (cp1 != '\0') return 0;  /* Path too long. */
    lcend = cp2;
    *cp2 = '\0';  /* Pick up with memcpy later */
    lcfront = lcres + (front - base);
    /* Now skip over each ellipsis and try to match the path in front of it. */
    while (ells--) {
      for (cp1 = ellipsis - 2; cp1 >= template; cp1--)
        if (*(cp1)   == '.' && *(cp1+1) == '.' &&
            *(cp1+2) == '.' && *(cp1+3) == '/'    ) break;
      if (cp1 < template) break; /* template started with an ellipsis */
      if (cp1 + 4 == ellipsis) { /* Consecutive ellipses */
        ellipsis = cp1; continue;
      }
      wilddsc.dsc$w_length = resdsc.dsc$w_length = ellipsis - 1 - cp1;
      nextell = cp1;
      for (segdirs = 0, cp2 = tpl;
           cp1 <= ellipsis - 1 && cp2 <= tpl + sizeof tpl;
           cp1++, cp2++) {
         if (*cp1 == '?') *cp2 = '%'; /* Substitute VMS' wildcard for Unix' */
         else *cp2 = _tolower(*cp1);  /* else lowercase for match */
         if (*cp2 == '/') segdirs++;
      }
      if (cp1 != ellipsis - 1) return 0; /* Path too long */
      /* Back up at least as many dirs as in template before matching */
      for (cp1 = lcfront - 1; segdirs && cp1 >= lcres; cp1--)
        if (*cp1 == '/' && !segdirs--) { cp1++; break; }
      for (match = 0; cp1 > lcres;) {
        resdsc.dsc$a_pointer = cp1;
        if (str$match_wild(&wilddsc,&resdsc) == STR$_MATCH) { 
          match++;
          if (match == 1) lcfront = cp1;
        }
        for ( ; cp1 >= lcres; cp1--) if (*cp1 == '/') { cp1++; break; }
      }
      if (!match) return 0;  /* Can't find prefix ??? */
      if (match > 1 && opts & 1) {
        /* This ... wildcard could cover more than one set of dirs (i.e.
         * a set of similar dir names is repeated).  If the template
         * contains more than 1 ..., upstream elements could resolve the
         * ambiguity, but it's not worth a full backtracking setup here.
         * As a quick heuristic, clip off the current default directory
         * if it's present to find the trimmed spec, else use the
         * shortest string that this ... could cover.
         */
        char def[NAM$C_MAXRSS+1], *st;

        if (getcwd(def, sizeof def,0) == NULL) return 0;
        for (cp1 = def, cp2 = base; *cp1 && *cp2; cp1++,cp2++)
          if (_tolower(*cp1) != _tolower(*cp2)) break;
        segdirs = dirs - totells;  /* Min # of dirs we must have left */
        for (st = cp2+1; *st; st++) if (*st == '/') segdirs--;
        if (*cp1 == '\0' && *cp2 == '/') {
          memcpy(fspec,cp2+1,end - cp2);
          return 1;
        }
        /* Nope -- stick with lcfront from above and keep going. */
      }
    }
    memcpy(fspec,base + (lcfront - lcres), lcend - lcfront + 1);
    return 1;
    ellipsis = nextell;
  }

}  /* end of trim_unixpath() */
/*}}}*/


/*
 *  VMS readdir() routines.
 *  Written by Rich $alz, <rsalz@bbn.com> in August, 1990.
 *
 *  21-Jul-1994  Charles Bailey  bailey@newman.upenn.edu
 *  Minor modifications to original routines.
 */

/* readdir may have been redefined by reentr.h, so make sure we get
 * the local version for what we do here.
 */
#ifdef readdir
# undef readdir
#endif
#if !defined(PERL_IMPLICIT_CONTEXT)
# define readdir Perl_readdir
#else
# define readdir(a) Perl_readdir(aTHX_ a)
#endif

    /* Number of elements in vms_versions array */
#define VERSIZE(e)	(sizeof e->vms_versions / sizeof e->vms_versions[0])

/*
 *  Open a directory, return a handle for later use.
 */
/*{{{ DIR *opendir(char*name) */
DIR *
Perl_opendir(pTHX_ char *name)
{
    DIR *dd;
    char dir[NAM$C_MAXRSS+1];
    Stat_t sb;

    if (do_tovmspath(name,dir,0) == NULL) {
      return NULL;
    }
    /* Check access before stat; otherwise stat does not
     * accurately report whether it's a directory.
     */
    if (!cando_by_name(S_IRUSR,0,dir)) {
      /* cando_by_name has already set errno */
      return NULL;
    }
    if (flex_stat(dir,&sb) == -1) return NULL;
    if (!S_ISDIR(sb.st_mode)) {
      set_errno(ENOTDIR);  set_vaxc_errno(RMS$_DIR);
      return NULL;
    }
    /* Get memory for the handle, and the pattern. */
    New(1306,dd,1,DIR);
    New(1307,dd->pattern,strlen(dir)+sizeof "*.*" + 1,char);

    /* Fill in the fields; mainly playing with the descriptor. */
    (void)sprintf(dd->pattern, "%s*.*",dir);
    dd->context = 0;
    dd->count = 0;
    dd->vms_wantversions = 0;
    dd->pat.dsc$a_pointer = dd->pattern;
    dd->pat.dsc$w_length = strlen(dd->pattern);
    dd->pat.dsc$b_dtype = DSC$K_DTYPE_T;
    dd->pat.dsc$b_class = DSC$K_CLASS_S;
#if defined(USE_5005THREADS) || defined(USE_ITHREADS)
    New(1308,dd->mutex,1,perl_mutex);
    MUTEX_INIT( (perl_mutex *) dd->mutex );
#else
    dd->mutex = NULL;
#endif

    return dd;
}  /* end of opendir() */
/*}}}*/

/*
 *  Set the flag to indicate we want versions or not.
 */
/*{{{ void vmsreaddirversions(DIR *dd, int flag)*/
void
vmsreaddirversions(DIR *dd, int flag)
{
    dd->vms_wantversions = flag;
}
/*}}}*/

/*
 *  Free up an opened directory.
 */
/*{{{ void closedir(DIR *dd)*/
void
closedir(DIR *dd)
{
    (void)lib$find_file_end(&dd->context);
    Safefree(dd->pattern);
#if defined(USE_5005THREADS) || defined(USE_ITHREADS)
    MUTEX_DESTROY( (perl_mutex *) dd->mutex );
    Safefree(dd->mutex);
#endif
    Safefree((char *)dd);
}
/*}}}*/

/*
 *  Collect all the version numbers for the current file.
 */
static void
collectversions(pTHX_ DIR *dd)
{
    struct dsc$descriptor_s	pat;
    struct dsc$descriptor_s	res;
    struct dirent *e;
    char *p, *text, buff[sizeof dd->entry.d_name];
    int i;
    unsigned long context, tmpsts;

    /* Convenient shorthand. */
    e = &dd->entry;

    /* Add the version wildcard, ignoring the "*.*" put on before */
    i = strlen(dd->pattern);
    New(1308,text,i + e->d_namlen + 3,char);
    (void)strcpy(text, dd->pattern);
    (void)sprintf(&text[i - 3], "%s;*", e->d_name);

    /* Set up the pattern descriptor. */
    pat.dsc$a_pointer = text;
    pat.dsc$w_length = i + e->d_namlen - 1;
    pat.dsc$b_dtype = DSC$K_DTYPE_T;
    pat.dsc$b_class = DSC$K_CLASS_S;

    /* Set up result descriptor. */
    res.dsc$a_pointer = buff;
    res.dsc$w_length = sizeof buff - 2;
    res.dsc$b_dtype = DSC$K_DTYPE_T;
    res.dsc$b_class = DSC$K_CLASS_S;

    /* Read files, collecting versions. */
    for (context = 0, e->vms_verscount = 0;
         e->vms_verscount < VERSIZE(e);
         e->vms_verscount++) {
	tmpsts = lib$find_file(&pat, &res, &context);
	if (tmpsts == RMS$_NMF || context == 0) break;
	_ckvmssts(tmpsts);
	buff[sizeof buff - 1] = '\0';
	if ((p = strchr(buff, ';')))
	    e->vms_versions[e->vms_verscount] = atoi(p + 1);
	else
	    e->vms_versions[e->vms_verscount] = -1;
    }

    _ckvmssts(lib$find_file_end(&context));
    Safefree(text);

}  /* end of collectversions() */

/*
 *  Read the next entry from the directory.
 */
/*{{{ struct dirent *readdir(DIR *dd)*/
struct dirent *
Perl_readdir(pTHX_ DIR *dd)
{
    struct dsc$descriptor_s	res;
    char *p, buff[sizeof dd->entry.d_name];
    unsigned long int tmpsts;

    /* Set up result descriptor, and get next file. */
    res.dsc$a_pointer = buff;
    res.dsc$w_length = sizeof buff - 2;
    res.dsc$b_dtype = DSC$K_DTYPE_T;
    res.dsc$b_class = DSC$K_CLASS_S;
    tmpsts = lib$find_file(&dd->pat, &res, &dd->context);
    if ( tmpsts == RMS$_NMF || dd->context == 0) return NULL;  /* None left. */
    if (!(tmpsts & 1)) {
      set_vaxc_errno(tmpsts);
      switch (tmpsts) {
        case RMS$_PRV:
          set_errno(EACCES); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_FNF: case RMS$_DNF:
          set_errno(ENOENT); break;
        default:
          set_errno(EVMSERR);
      }
      return NULL;
    }
    dd->count++;
    /* Force the buffer to end with a NUL, and downcase name to match C convention. */
    buff[sizeof buff - 1] = '\0';
    for (p = buff; *p; p++) *p = _tolower(*p);
    while (--p >= buff) if (!isspace(*p)) break;  /* Do we really need this? */
    *p = '\0';

    /* Skip any directory component and just copy the name. */
    if ((p = strchr(buff, ']'))) (void)strcpy(dd->entry.d_name, p + 1);
    else (void)strcpy(dd->entry.d_name, buff);

    /* Clobber the version. */
    if ((p = strchr(dd->entry.d_name, ';'))) *p = '\0';

    dd->entry.d_namlen = strlen(dd->entry.d_name);
    dd->entry.vms_verscount = 0;
    if (dd->vms_wantversions) collectversions(aTHX_ dd);
    return &dd->entry;

}  /* end of readdir() */
/*}}}*/

/*
 *  Read the next entry from the directory -- thread-safe version.
 */
/*{{{ int readdir_r(DIR *dd, struct dirent *entry, struct dirent **result)*/
int
Perl_readdir_r(pTHX_ DIR *dd, struct dirent *entry, struct dirent **result)
{
    int retval;

    MUTEX_LOCK( (perl_mutex *) dd->mutex );

    entry = readdir(dd);
    *result = entry;
    retval = ( *result == NULL ? errno : 0 );

    MUTEX_UNLOCK( (perl_mutex *) dd->mutex );

    return retval;

}  /* end of readdir_r() */
/*}}}*/

/*
 *  Return something that can be used in a seekdir later.
 */
/*{{{ long telldir(DIR *dd)*/
long
telldir(DIR *dd)
{
    return dd->count;
}
/*}}}*/

/*
 *  Return to a spot where we used to be.  Brute force.
 */
/*{{{ void seekdir(DIR *dd,long count)*/
void
Perl_seekdir(pTHX_ DIR *dd, long count)
{
    int vms_wantversions;

    /* If we haven't done anything yet... */
    if (dd->count == 0)
	return;

    /* Remember some state, and clear it. */
    vms_wantversions = dd->vms_wantversions;
    dd->vms_wantversions = 0;
    _ckvmssts(lib$find_file_end(&dd->context));
    dd->context = 0;

    /* The increment is in readdir(). */
    for (dd->count = 0; dd->count < count; )
	(void)readdir(dd);

    dd->vms_wantversions = vms_wantversions;

}  /* end of seekdir() */
/*}}}*/

/* VMS subprocess management
 *
 * my_vfork() - just a vfork(), after setting a flag to record that
 * the current script is trying a Unix-style fork/exec.
 *
 * vms_do_aexec() and vms_do_exec() are called in response to the
 * perl 'exec' function.  If this follows a vfork call, then they
 * call out the regular perl routines in doio.c which do an
 * execvp (for those who really want to try this under VMS).
 * Otherwise, they do exactly what the perl docs say exec should
 * do - terminate the current script and invoke a new command
 * (See below for notes on command syntax.)
 *
 * do_aspawn() and do_spawn() implement the VMS side of the perl
 * 'system' function.
 *
 * Note on command arguments to perl 'exec' and 'system': When handled
 * in 'VMSish fashion' (i.e. not after a call to vfork) The args
 * are concatenated to form a DCL command string.  If the first arg
 * begins with '$' (i.e. the perl script had "\$ Type" or some such),
 * the command string is handed off to DCL directly.  Otherwise,
 * the first token of the command is taken as the filespec of an image
 * to run.  The filespec is expanded using a default type of '.EXE' and
 * the process defaults for device, directory, etc., and if found, the resultant
 * filespec is invoked using the DCL verb 'MCR', and passed the rest of
 * the command string as parameters.  This is perhaps a bit complicated,
 * but I hope it will form a happy medium between what VMS folks expect
 * from lib$spawn and what Unix folks expect from exec.
 */

static int vfork_called;

/*{{{int my_vfork()*/
int
my_vfork()
{
  vfork_called++;
  return vfork();
}
/*}}}*/


static void
vms_execfree(struct dsc$descriptor_s *vmscmd) 
{
  if (vmscmd) {
      if (vmscmd->dsc$a_pointer) {
          Safefree(vmscmd->dsc$a_pointer);
      }
      Safefree(vmscmd);
  }
}

static char *
setup_argstr(pTHX_ SV *really, SV **mark, SV **sp)
{
  char *junk, *tmps = Nullch;
  register size_t cmdlen = 0;
  size_t rlen;
  register SV **idx;
  STRLEN n_a;

  idx = mark;
  if (really) {
    tmps = SvPV(really,rlen);
    if (*tmps) {
      cmdlen += rlen + 1;
      idx++;
    }
  }
  
  for (idx++; idx <= sp; idx++) {
    if (*idx) {
      junk = SvPVx(*idx,rlen);
      cmdlen += rlen ? rlen + 1 : 0;
    }
  }
  New(401,PL_Cmd,cmdlen+1,char);

  if (tmps && *tmps) {
    strcpy(PL_Cmd,tmps);
    mark++;
  }
  else *PL_Cmd = '\0';
  while (++mark <= sp) {
    if (*mark) {
      char *s = SvPVx(*mark,n_a);
      if (!*s) continue;
      if (*PL_Cmd) strcat(PL_Cmd," ");
      strcat(PL_Cmd,s);
    }
  }
  return PL_Cmd;

}  /* end of setup_argstr() */


static unsigned long int
setup_cmddsc(pTHX_ char *cmd, int check_img, int *suggest_quote,
                   struct dsc$descriptor_s **pvmscmd)
{
  char vmsspec[NAM$C_MAXRSS+1], resspec[NAM$C_MAXRSS+1];
  $DESCRIPTOR(defdsc,".EXE");
  $DESCRIPTOR(defdsc2,".");
  $DESCRIPTOR(resdsc,resspec);
  struct dsc$descriptor_s *vmscmd;
  struct dsc$descriptor_s imgdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
  unsigned long int cxt = 0, flags = 1, retsts = SS$_NORMAL;
  register char *s, *rest, *cp, *wordbreak;
  register int isdcl;

  New(402,vmscmd,sizeof(struct dsc$descriptor_s),struct dsc$descriptor_s);
  vmscmd->dsc$a_pointer = NULL;
  vmscmd->dsc$b_dtype  = DSC$K_DTYPE_T;
  vmscmd->dsc$b_class  = DSC$K_CLASS_S;
  vmscmd->dsc$w_length = 0;
  if (pvmscmd) *pvmscmd = vmscmd;

  if (suggest_quote) *suggest_quote = 0;

  if (strlen(cmd) > MAX_DCL_LINE_LENGTH)
    return CLI$_BUFOVF;                /* continuation lines currently unsupported */
  s = cmd;
  while (*s && isspace(*s)) s++;

  if (*s == '@' || *s == '$') {
    vmsspec[0] = *s;  rest = s + 1;
    for (cp = &vmsspec[1]; *rest && isspace(*rest); rest++,cp++) *cp = *rest;
  }
  else { cp = vmsspec; rest = s; }
  if (*rest == '.' || *rest == '/') {
    char *cp2;
    for (cp2 = resspec;
         *rest && !isspace(*rest) && cp2 - resspec < sizeof resspec;
         rest++, cp2++) *cp2 = *rest;
    *cp2 = '\0';
    if (do_tovmsspec(resspec,cp,0)) { 
      s = vmsspec;
      if (*rest) {
        for (cp2 = vmsspec + strlen(vmsspec);
             *rest && cp2 - vmsspec < sizeof vmsspec;
             rest++, cp2++) *cp2 = *rest;
        *cp2 = '\0';
      }
    }
  }
  /* Intuit whether verb (first word of cmd) is a DCL command:
   *   - if first nonspace char is '@', it's a DCL indirection
   * otherwise
   *   - if verb contains a filespec separator, it's not a DCL command
   *   - if it doesn't, caller tells us whether to default to a DCL
   *     command, or to a local image unless told it's DCL (by leading '$')
   */
  if (*s == '@') {
      isdcl = 1;
      if (suggest_quote) *suggest_quote = 1;
  } else {
    register char *filespec = strpbrk(s,":<[.;");
    rest = wordbreak = strpbrk(s," \"\t/");
    if (!wordbreak) wordbreak = s + strlen(s);
    if (*s == '$') check_img = 0;
    if (filespec && (filespec < wordbreak)) isdcl = 0;
    else isdcl = !check_img;
  }

  if (!isdcl) {
    imgdsc.dsc$a_pointer = s;
    imgdsc.dsc$w_length = wordbreak - s;
    retsts = lib$find_file(&imgdsc,&resdsc,&cxt,&defdsc,0,0,&flags);
    if (!(retsts&1)) {
        _ckvmssts(lib$find_file_end(&cxt));
        retsts = lib$find_file(&imgdsc,&resdsc,&cxt,&defdsc2,0,0,&flags);
    if (!(retsts & 1) && *s == '$') {
          _ckvmssts(lib$find_file_end(&cxt));
      imgdsc.dsc$a_pointer++; imgdsc.dsc$w_length--;
      retsts = lib$find_file(&imgdsc,&resdsc,&cxt,&defdsc,0,0,&flags);
          if (!(retsts&1)) {
      _ckvmssts(lib$find_file_end(&cxt));
            retsts = lib$find_file(&imgdsc,&resdsc,&cxt,&defdsc2,0,0,&flags);
          }
    }
    }
    _ckvmssts(lib$find_file_end(&cxt));

    if (retsts & 1) {
      FILE *fp;
      s = resspec;
      while (*s && !isspace(*s)) s++;
      *s = '\0';

      /* check that it's really not DCL with no file extension */
      fp = fopen(resspec,"r","ctx=bin,shr=get");
      if (fp) {
        char b[4] = {0,0,0,0};
        read(fileno(fp),b,4);
        isdcl = isprint(b[0]) && isprint(b[1]) && isprint(b[2]) && isprint(b[3]);
        fclose(fp);
      }
      if (check_img && isdcl) return RMS$_FNF;

      if (cando_by_name(S_IXUSR,0,resspec)) {
        New(402,vmscmd->dsc$a_pointer,7 + s - resspec + (rest ? strlen(rest) : 0),char);
        if (!isdcl) {
            strcpy(vmscmd->dsc$a_pointer,"$ MCR ");
            if (suggest_quote) *suggest_quote = 1;
        } else {
            strcpy(vmscmd->dsc$a_pointer,"@");
            if (suggest_quote) *suggest_quote = 1;
        }
        strcat(vmscmd->dsc$a_pointer,resspec);
        if (rest) strcat(vmscmd->dsc$a_pointer,rest);
        vmscmd->dsc$w_length = strlen(vmscmd->dsc$a_pointer);
        return (vmscmd->dsc$w_length > MAX_DCL_LINE_LENGTH ? CLI$_BUFOVF : retsts);
      }
      else retsts = RMS$_PRV;
    }
  }
  /* It's either a DCL command or we couldn't find a suitable image */
  vmscmd->dsc$w_length = strlen(cmd);
/*  if (cmd == PL_Cmd) {
      vmscmd->dsc$a_pointer = PL_Cmd;
      if (suggest_quote) *suggest_quote = 1;
  }
  else  */
      vmscmd->dsc$a_pointer = savepvn(cmd,vmscmd->dsc$w_length);

  /* check if it's a symbol (for quoting purposes) */
  if (suggest_quote && !*suggest_quote) { 
    int iss;     
    char equiv[LNM$C_NAMLENGTH];
    struct dsc$descriptor_s eqvdsc = {sizeof(equiv), DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
    eqvdsc.dsc$a_pointer = equiv;

    iss = lib$get_symbol(vmscmd,&eqvdsc);
    if (iss&1 && (*equiv == '$' || *equiv == '@')) *suggest_quote = 1;
  }
  if (!(retsts & 1)) {
    /* just hand off status values likely to be due to user error */
    if (retsts == RMS$_FNF || retsts == RMS$_DNF || retsts == RMS$_PRV ||
        retsts == RMS$_DEV || retsts == RMS$_DIR || retsts == RMS$_SYN ||
       (retsts & STS$M_CODE) == (SHR$_NOWILD & STS$M_CODE)) return retsts;
    else { _ckvmssts(retsts); }
  }

  return (vmscmd->dsc$w_length > MAX_DCL_LINE_LENGTH ? CLI$_BUFOVF : retsts);

}  /* end of setup_cmddsc() */


/* {{{ bool vms_do_aexec(SV *really,SV **mark,SV **sp) */
bool
Perl_vms_do_aexec(pTHX_ SV *really,SV **mark,SV **sp)
{
  if (sp > mark) {
    if (vfork_called) {           /* this follows a vfork - act Unixish */
      vfork_called--;
      if (vfork_called < 0) {
        Perl_warn(aTHX_ "Internal inconsistency in tracking vforks");
        vfork_called = 0;
      }
      else return do_aexec(really,mark,sp);
    }
                                           /* no vfork - act VMSish */
    return vms_do_exec(setup_argstr(aTHX_ really,mark,sp));

  }

  return FALSE;
}  /* end of vms_do_aexec() */
/*}}}*/

/* {{{bool vms_do_exec(char *cmd) */
bool
Perl_vms_do_exec(pTHX_ char *cmd)
{
  struct dsc$descriptor_s *vmscmd;

  if (vfork_called) {             /* this follows a vfork - act Unixish */
    vfork_called--;
    if (vfork_called < 0) {
      Perl_warn(aTHX_ "Internal inconsistency in tracking vforks");
      vfork_called = 0;
    }
    else return do_exec(cmd);
  }

  {                               /* no vfork - act VMSish */
    unsigned long int retsts;

    TAINT_ENV();
    TAINT_PROPER("exec");
    if ((retsts = setup_cmddsc(aTHX_ cmd,1,0,&vmscmd)) & 1)
      retsts = lib$do_command(vmscmd);

    switch (retsts) {
      case RMS$_FNF: case RMS$_DNF:
        set_errno(ENOENT); break;
      case RMS$_DIR:
        set_errno(ENOTDIR); break;
      case RMS$_DEV:
        set_errno(ENODEV); break;
      case RMS$_PRV:
        set_errno(EACCES); break;
      case RMS$_SYN:
        set_errno(EINVAL); break;
      case CLI$_BUFOVF: case RMS$_RTB: case CLI$_TKNOVF: case CLI$_RSLOVF:
        set_errno(E2BIG); break;
      case LIB$_INVARG: case LIB$_INVSTRDES: case SS$_ACCVIO: /* shouldn't happen */
        _ckvmssts(retsts); /* fall through */
      default:  /* SS$_DUPLNAM, SS$_CLI, resource exhaustion, etc. */
        set_errno(EVMSERR); 
    }
    set_vaxc_errno(retsts);
    if (ckWARN(WARN_EXEC)) {
      Perl_warner(aTHX_ packWARN(WARN_EXEC),"Can't exec \"%*s\": %s",
             vmscmd->dsc$w_length, vmscmd->dsc$a_pointer, Strerror(errno));
    }
    vms_execfree(vmscmd);
  }

  return FALSE;

}  /* end of vms_do_exec() */
/*}}}*/

unsigned long int Perl_do_spawn(pTHX_ char *);

/* {{{ unsigned long int do_aspawn(void *really,void **mark,void **sp) */
unsigned long int
Perl_do_aspawn(pTHX_ void *really,void **mark,void **sp)
{
  if (sp > mark) return do_spawn(setup_argstr(aTHX_ (SV *)really,(SV **)mark,(SV **)sp));

  return SS$_ABORT;
}  /* end of do_aspawn() */
/*}}}*/

/* {{{unsigned long int do_spawn(char *cmd) */
unsigned long int
Perl_do_spawn(pTHX_ char *cmd)
{
  unsigned long int sts, substs;

  TAINT_ENV();
  TAINT_PROPER("spawn");
  if (!cmd || !*cmd) {
    sts = lib$spawn(0,0,0,0,0,0,&substs,0,0,0,0,0,0);
    if (!(sts & 1)) {
      switch (sts) {
        case RMS$_FNF:  case RMS$_DNF:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        case RMS$_SYN:
          set_errno(EINVAL); break;
        case CLI$_BUFOVF: case RMS$_RTB: case CLI$_TKNOVF: case CLI$_RSLOVF:
          set_errno(E2BIG); break;
        case LIB$_INVARG: case LIB$_INVSTRDES: case SS$_ACCVIO: /* shouldn't happen */
          _ckvmssts(sts); /* fall through */
        default:  /* SS$_DUPLNAM, SS$_CLI, resource exhaustion, etc. */
          set_errno(EVMSERR);
      }
      set_vaxc_errno(sts);
      if (ckWARN(WARN_EXEC)) {
        Perl_warner(aTHX_ packWARN(WARN_EXEC),"Can't spawn: %s",
		    Strerror(errno));
      }
    }
    sts = substs;
  }
  else {
    (void) safe_popen(aTHX_ cmd, "nW", (int *)&sts);
  }
  return sts;
}  /* end of do_spawn() */
/*}}}*/


static unsigned int *sockflags, sockflagsize;

/*
 * Shim fdopen to identify sockets for my_fwrite later, since the stdio
 * routines found in some versions of the CRTL can't deal with sockets.
 * We don't shim the other file open routines since a socket isn't
 * likely to be opened by a name.
 */
/*{{{ FILE *my_fdopen(int fd, const char *mode)*/
FILE *my_fdopen(int fd, const char *mode)
{
  FILE *fp = fdopen(fd, (char *) mode);

  if (fp) {
    unsigned int fdoff = fd / sizeof(unsigned int);
    struct stat sbuf; /* native stat; we don't need flex_stat */
    if (!sockflagsize || fdoff > sockflagsize) {
      if (sockflags) Renew(     sockflags,fdoff+2,unsigned int);
      else           New  (1324,sockflags,fdoff+2,unsigned int);
      memset(sockflags+sockflagsize,0,fdoff + 2 - sockflagsize);
      sockflagsize = fdoff + 2;
    }
    if (fstat(fd,&sbuf) == 0 && S_ISSOCK(sbuf.st_mode))
      sockflags[fdoff] |= 1 << (fd % sizeof(unsigned int));
  }
  return fp;

}
/*}}}*/


/*
 * Clear the corresponding bit when the (possibly) socket stream is closed.
 * There still a small hole: we miss an implicit close which might occur
 * via freopen().  >> Todo
 */
/*{{{ int my_fclose(FILE *fp)*/
int my_fclose(FILE *fp) {
  if (fp) {
    unsigned int fd = fileno(fp);
    unsigned int fdoff = fd / sizeof(unsigned int);

    if (sockflagsize && fdoff <= sockflagsize)
      sockflags[fdoff] &= ~(1 << fd % sizeof(unsigned int));
  }
  return fclose(fp);
}
/*}}}*/


/* 
 * A simple fwrite replacement which outputs itmsz*nitm chars without
 * introducing record boundaries every itmsz chars.
 * We are using fputs, which depends on a terminating null.  We may
 * well be writing binary data, so we need to accommodate not only
 * data with nulls sprinkled in the middle but also data with no null 
 * byte at the end.
 */
/*{{{ int my_fwrite(const void *src, size_t itmsz, size_t nitm, FILE *dest)*/
int
my_fwrite(const void *src, size_t itmsz, size_t nitm, FILE *dest)
{
  register char *cp, *end, *cpd, *data;
  register unsigned int fd = fileno(dest);
  register unsigned int fdoff = fd / sizeof(unsigned int);
  int retval;
  int bufsize = itmsz * nitm + 1;

  if (fdoff < sockflagsize &&
      (sockflags[fdoff] | 1 << (fd % sizeof(unsigned int)))) {
    if (write(fd, src, itmsz * nitm) == EOF) return EOF;
    return nitm;
  }

  _ckvmssts_noperl(lib$get_vm(&bufsize, &data));
  memcpy( data, src, itmsz*nitm );
  data[itmsz*nitm] = '\0';

  end = data + itmsz * nitm;
  retval = (int) nitm; /* on success return # items written */

  cpd = data;
  while (cpd <= end) {
    for (cp = cpd; cp <= end; cp++) if (!*cp) break;
    if (fputs(cpd,dest) == EOF) { retval = EOF; break; }
    if (cp < end)
      if (fputc('\0',dest) == EOF) { retval = EOF; break; }
    cpd = cp + 1;
  }

  if (data) _ckvmssts_noperl(lib$free_vm(&bufsize, &data));
  return retval;

}  /* end of my_fwrite() */
/*}}}*/

/*{{{ int my_flush(FILE *fp)*/
int
Perl_my_flush(pTHX_ FILE *fp)
{
    int res;
    if ((res = fflush(fp)) == 0 && fp) {
#ifdef VMS_DO_SOCKETS
	Stat_t s;
	if (Fstat(fileno(fp), &s) == 0 && !S_ISSOCK(s.st_mode))
#endif
	    res = fsync(fileno(fp));
    }
/*
 * If the flush succeeded but set end-of-file, we need to clear
 * the error because our caller may check ferror().  BTW, this 
 * probably means we just flushed an empty file.
 */
    if (res == 0 && vaxc$errno == RMS$_EOF) clearerr(fp);

    return res;
}
/*}}}*/

/*
 * Here are replacements for the following Unix routines in the VMS environment:
 *      getpwuid    Get information for a particular UIC or UID
 *      getpwnam    Get information for a named user
 *      getpwent    Get information for each user in the rights database
 *      setpwent    Reset search to the start of the rights database
 *      endpwent    Finish searching for users in the rights database
 *
 * getpwuid, getpwnam, and getpwent return a pointer to the passwd structure
 * (defined in pwd.h), which contains the following fields:-
 *      struct passwd {
 *              char        *pw_name;    Username (in lower case)
 *              char        *pw_passwd;  Hashed password
 *              unsigned int pw_uid;     UIC
 *              unsigned int pw_gid;     UIC group  number
 *              char        *pw_unixdir; Default device/directory (VMS-style)
 *              char        *pw_gecos;   Owner name
 *              char        *pw_dir;     Default device/directory (Unix-style)
 *              char        *pw_shell;   Default CLI name (eg. DCL)
 *      };
 * If the specified user does not exist, getpwuid and getpwnam return NULL.
 *
 * pw_uid is the full UIC (eg. what's returned by stat() in st_uid).
 * not the UIC member number (eg. what's returned by getuid()),
 * getpwuid() can accept either as input (if uid is specified, the caller's
 * UIC group is used), though it won't recognise gid=0.
 *
 * Note that in VMS it is necessary to have GRPPRV or SYSPRV to return
 * information about other users in your group or in other groups, respectively.
 * If the required privilege is not available, then these routines fill only
 * the pw_name, pw_uid, and pw_gid fields (the others point to an empty
 * string).
 *
 * By Tim Adye (T.J.Adye@rl.ac.uk), 10th February 1995.
 */

/* sizes of various UAF record fields */
#define UAI$S_USERNAME 12
#define UAI$S_IDENT    31
#define UAI$S_OWNER    31
#define UAI$S_DEFDEV   31
#define UAI$S_DEFDIR   63
#define UAI$S_DEFCLI   31
#define UAI$S_PWD       8

#define valid_uic(uic) ((uic).uic$v_format == UIC$K_UIC_FORMAT  && \
                        (uic).uic$v_member != UIC$K_WILD_MEMBER && \
                        (uic).uic$v_group  != UIC$K_WILD_GROUP)

static char __empty[]= "";
static struct passwd __passwd_empty=
    {(char *) __empty, (char *) __empty, 0, 0,
     (char *) __empty, (char *) __empty, (char *) __empty, (char *) __empty};
static int contxt= 0;
static struct passwd __pwdcache;
static char __pw_namecache[UAI$S_IDENT+1];

/*
 * This routine does most of the work extracting the user information.
 */
static int fillpasswd (pTHX_ const char *name, struct passwd *pwd)
{
    static struct {
        unsigned char length;
        char pw_gecos[UAI$S_OWNER+1];
    } owner;
    static union uicdef uic;
    static struct {
        unsigned char length;
        char pw_dir[UAI$S_DEFDEV+UAI$S_DEFDIR+1];
    } defdev;
    static struct {
        unsigned char length;
        char unixdir[UAI$_DEFDEV+UAI$S_DEFDIR+1];
    } defdir;
    static struct {
        unsigned char length;
        char pw_shell[UAI$S_DEFCLI+1];
    } defcli;
    static char pw_passwd[UAI$S_PWD+1];

    static unsigned short lowner, luic, ldefdev, ldefdir, ldefcli, lpwd;
    struct dsc$descriptor_s name_desc;
    unsigned long int sts;

    static struct itmlst_3 itmlst[]= {
        {UAI$S_OWNER+1,    UAI$_OWNER,  &owner,    &lowner},
        {sizeof(uic),      UAI$_UIC,    &uic,      &luic},
        {UAI$S_DEFDEV+1,   UAI$_DEFDEV, &defdev,   &ldefdev},
        {UAI$S_DEFDIR+1,   UAI$_DEFDIR, &defdir,   &ldefdir},
        {UAI$S_DEFCLI+1,   UAI$_DEFCLI, &defcli,   &ldefcli},
        {UAI$S_PWD,        UAI$_PWD,    pw_passwd, &lpwd},
        {0,                0,           NULL,    NULL}};

    name_desc.dsc$w_length=  strlen(name);
    name_desc.dsc$b_dtype=   DSC$K_DTYPE_T;
    name_desc.dsc$b_class=   DSC$K_CLASS_S;
    name_desc.dsc$a_pointer= (char *) name;

/*  Note that sys$getuai returns many fields as counted strings. */
    sts= sys$getuai(0, 0, &name_desc, &itmlst, 0, 0, 0);
    if (sts == SS$_NOSYSPRV || sts == SS$_NOGRPPRV || sts == RMS$_RNF) {
      set_vaxc_errno(sts); set_errno(sts == RMS$_RNF ? EINVAL : EACCES);
    }
    else { _ckvmssts(sts); }
    if (!(sts & 1)) return 0;  /* out here in case _ckvmssts() doesn't abort */

    if ((int) owner.length  < lowner)  lowner=  (int) owner.length;
    if ((int) defdev.length < ldefdev) ldefdev= (int) defdev.length;
    if ((int) defdir.length < ldefdir) ldefdir= (int) defdir.length;
    if ((int) defcli.length < ldefcli) ldefcli= (int) defcli.length;
    memcpy(&defdev.pw_dir[ldefdev], &defdir.unixdir[0], ldefdir);
    owner.pw_gecos[lowner]=            '\0';
    defdev.pw_dir[ldefdev+ldefdir]= '\0';
    defcli.pw_shell[ldefcli]=          '\0';
    if (valid_uic(uic)) {
        pwd->pw_uid= uic.uic$l_uic;
        pwd->pw_gid= uic.uic$v_group;
    }
    else
      Perl_warn(aTHX_ "getpwnam returned invalid UIC %#o for user \"%s\"");
    pwd->pw_passwd=  pw_passwd;
    pwd->pw_gecos=   owner.pw_gecos;
    pwd->pw_dir=     defdev.pw_dir;
    pwd->pw_unixdir= do_tounixpath(defdev.pw_dir, defdir.unixdir,1);
    pwd->pw_shell=   defcli.pw_shell;
    if (pwd->pw_unixdir && pwd->pw_unixdir[0]) {
        int ldir;
        ldir= strlen(pwd->pw_unixdir) - 1;
        if (pwd->pw_unixdir[ldir]=='/') pwd->pw_unixdir[ldir]= '\0';
    }
    else
        strcpy(pwd->pw_unixdir, pwd->pw_dir);
    __mystrtolower(pwd->pw_unixdir);
    return 1;
}

/*
 * Get information for a named user.
*/
/*{{{struct passwd *getpwnam(char *name)*/
struct passwd *Perl_my_getpwnam(pTHX_ char *name)
{
    struct dsc$descriptor_s name_desc;
    union uicdef uic;
    unsigned long int status, sts;
                                  
    __pwdcache = __passwd_empty;
    if (!fillpasswd(aTHX_ name, &__pwdcache)) {
      /* We still may be able to determine pw_uid and pw_gid */
      name_desc.dsc$w_length=  strlen(name);
      name_desc.dsc$b_dtype=   DSC$K_DTYPE_T;
      name_desc.dsc$b_class=   DSC$K_CLASS_S;
      name_desc.dsc$a_pointer= (char *) name;
      if ((sts = sys$asctoid(&name_desc, &uic, 0)) == SS$_NORMAL) {
        __pwdcache.pw_uid= uic.uic$l_uic;
        __pwdcache.pw_gid= uic.uic$v_group;
      }
      else {
        if (sts == SS$_NOSUCHID || sts == SS$_IVIDENT || sts == RMS$_PRV) {
          set_vaxc_errno(sts);
          set_errno(sts == RMS$_PRV ? EACCES : EINVAL);
          return NULL;
        }
        else { _ckvmssts(sts); }
      }
    }
    strncpy(__pw_namecache, name, sizeof(__pw_namecache));
    __pw_namecache[sizeof __pw_namecache - 1] = '\0';
    __pwdcache.pw_name= __pw_namecache;
    return &__pwdcache;
}  /* end of my_getpwnam() */
/*}}}*/

/*
 * Get information for a particular UIC or UID.
 * Called by my_getpwent with uid=-1 to list all users.
*/
/*{{{struct passwd *my_getpwuid(Uid_t uid)*/
struct passwd *Perl_my_getpwuid(pTHX_ Uid_t uid)
{
    const $DESCRIPTOR(name_desc,__pw_namecache);
    unsigned short lname;
    union uicdef uic;
    unsigned long int status;

    if (uid == (unsigned int) -1) {
      do {
        status = sys$idtoasc(-1, &lname, &name_desc, &uic, 0, &contxt);
        if (status == SS$_NOSUCHID || status == RMS$_PRV) {
          set_vaxc_errno(status);
          set_errno(status == RMS$_PRV ? EACCES : EINVAL);
          my_endpwent();
          return NULL;
        }
        else { _ckvmssts(status); }
      } while (!valid_uic (uic));
    }
    else {
      uic.uic$l_uic= uid;
      if (!uic.uic$v_group)
        uic.uic$v_group= PerlProc_getgid();
      if (valid_uic(uic))
        status = sys$idtoasc(uic.uic$l_uic, &lname, &name_desc, 0, 0, 0);
      else status = SS$_IVIDENT;
      if (status == SS$_IVIDENT || status == SS$_NOSUCHID ||
          status == RMS$_PRV) {
        set_vaxc_errno(status); set_errno(status == RMS$_PRV ? EACCES : EINVAL);
        return NULL;
      }
      else { _ckvmssts(status); }
    }
    __pw_namecache[lname]= '\0';
    __mystrtolower(__pw_namecache);

    __pwdcache = __passwd_empty;
    __pwdcache.pw_name = __pw_namecache;

/*  Fill in the uid and gid in case fillpasswd can't (eg. no privilege).
    The identifier's value is usually the UIC, but it doesn't have to be,
    so if we can, we let fillpasswd update this. */
    __pwdcache.pw_uid =  uic.uic$l_uic;
    __pwdcache.pw_gid =  uic.uic$v_group;

    fillpasswd(aTHX_ __pw_namecache, &__pwdcache);
    return &__pwdcache;

}  /* end of my_getpwuid() */
/*}}}*/

/*
 * Get information for next user.
*/
/*{{{struct passwd *my_getpwent()*/
struct passwd *Perl_my_getpwent(pTHX)
{
    return (my_getpwuid((unsigned int) -1));
}
/*}}}*/

/*
 * Finish searching rights database for users.
*/
/*{{{void my_endpwent()*/
void Perl_my_endpwent(pTHX)
{
    if (contxt) {
      _ckvmssts(sys$finish_rdb(&contxt));
      contxt= 0;
    }
}
/*}}}*/

#ifdef HOMEGROWN_POSIX_SIGNALS
  /* Signal handling routines, pulled into the core from POSIX.xs.
   *
   * We need these for threads, so they've been rolled into the core,
   * rather than left in POSIX.xs.
   *
   * (DRS, Oct 23, 1997)
   */

  /* sigset_t is atomic under VMS, so these routines are easy */
/*{{{int my_sigemptyset(sigset_t *) */
int my_sigemptyset(sigset_t *set) {
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    *set = 0; return 0;
}
/*}}}*/


/*{{{int my_sigfillset(sigset_t *)*/
int my_sigfillset(sigset_t *set) {
    int i;
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    for (i = 0; i < NSIG; i++) *set |= (1 << i);
    return 0;
}
/*}}}*/


/*{{{int my_sigaddset(sigset_t *set, int sig)*/
int my_sigaddset(sigset_t *set, int sig) {
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    if (sig > NSIG) { SETERRNO(EINVAL,LIB$_INVARG); return -1; }
    *set |= (1 << (sig - 1));
    return 0;
}
/*}}}*/


/*{{{int my_sigdelset(sigset_t *set, int sig)*/
int my_sigdelset(sigset_t *set, int sig) {
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    if (sig > NSIG) { SETERRNO(EINVAL,LIB$_INVARG); return -1; }
    *set &= ~(1 << (sig - 1));
    return 0;
}
/*}}}*/


/*{{{int my_sigismember(sigset_t *set, int sig)*/
int my_sigismember(sigset_t *set, int sig) {
    if (!set) { SETERRNO(EFAULT,SS$_ACCVIO); return -1; }
    if (sig > NSIG) { SETERRNO(EINVAL,LIB$_INVARG); return -1; }
    return *set & (1 << (sig - 1));
}
/*}}}*/


/*{{{int my_sigprocmask(int how, sigset_t *set, sigset_t *oset)*/
int my_sigprocmask(int how, sigset_t *set, sigset_t *oset) {
    sigset_t tempmask;

    /* If set and oset are both null, then things are badly wrong. Bail out. */
    if ((oset == NULL) && (set == NULL)) {
      set_errno(EFAULT); set_vaxc_errno(SS$_ACCVIO);
      return -1;
    }

    /* If set's null, then we're just handling a fetch. */
    if (set == NULL) {
        tempmask = sigblock(0);
    }
    else {
      switch (how) {
      case SIG_SETMASK:
        tempmask = sigsetmask(*set);
        break;
      case SIG_BLOCK:
        tempmask = sigblock(*set);
        break;
      case SIG_UNBLOCK:
        tempmask = sigblock(0);
        sigsetmask(*oset & ~tempmask);
        break;
      default:
        set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
        return -1;
      }
    }

    /* Did they pass us an oset? If so, stick our holding mask into it */
    if (oset)
      *oset = tempmask;
  
    return 0;
}
/*}}}*/
#endif  /* HOMEGROWN_POSIX_SIGNALS */


/* Used for UTC calculation in my_gmtime(), my_localtime(), my_time(),
 * my_utime(), and flex_stat(), all of which operate on UTC unless
 * VMSISH_TIMES is true.
 */
/* method used to handle UTC conversions:
 *   1 == CRTL gmtime();  2 == SYS$TIMEZONE_DIFFERENTIAL;  3 == no correction
 */
static int gmtime_emulation_type;
/* number of secs to add to UTC POSIX-style time to get local time */
static long int utc_offset_secs;

/* We #defined 'gmtime', 'localtime', and 'time' as 'my_gmtime' etc.
 * in vmsish.h.  #undef them here so we can call the CRTL routines
 * directly.
 */
#undef gmtime
#undef localtime
#undef time


/*
 * DEC C previous to 6.0 corrupts the behavior of the /prefix
 * qualifier with the extern prefix pragma.  This provisional
 * hack circumvents this prefix pragma problem in previous 
 * precompilers.
 */
#if defined(__VMS_VER) && __VMS_VER >= 70000000 
#  if defined(VMS_WE_ARE_CASE_SENSITIVE) && (__DECC_VER < 60000000)
#    pragma __extern_prefix save
#    pragma __extern_prefix ""  /* set to empty to prevent prefixing */
#    define gmtime decc$__utctz_gmtime
#    define localtime decc$__utctz_localtime
#    define time decc$__utc_time
#    pragma __extern_prefix restore

     struct tm *gmtime(), *localtime();   

#  endif
#endif


static time_t toutc_dst(time_t loc) {
  struct tm *rsltmp;

  if ((rsltmp = localtime(&loc)) == NULL) return -1;
  loc -= utc_offset_secs;
  if (rsltmp->tm_isdst) loc -= 3600;
  return loc;
}
#define _toutc(secs)  ((secs) == (time_t) -1 ? (time_t) -1 : \
       ((gmtime_emulation_type || my_time(NULL)), \
       (gmtime_emulation_type == 1 ? toutc_dst(secs) : \
       ((secs) - utc_offset_secs))))

static time_t toloc_dst(time_t utc) {
  struct tm *rsltmp;

  utc += utc_offset_secs;
  if ((rsltmp = localtime(&utc)) == NULL) return -1;
  if (rsltmp->tm_isdst) utc += 3600;
  return utc;
}
#define _toloc(secs)  ((secs) == (time_t) -1 ? (time_t) -1 : \
       ((gmtime_emulation_type || my_time(NULL)), \
       (gmtime_emulation_type == 1 ? toloc_dst(secs) : \
       ((secs) + utc_offset_secs))))

#ifndef RTL_USES_UTC
/*
  
    ucx$tz = "EST5EDT4,M4.1.0,M10.5.0"  typical 
        DST starts on 1st sun of april      at 02:00  std time
            ends on last sun of october     at 02:00  dst time
    see the UCX management command reference, SET CONFIG TIMEZONE
    for formatting info.

    No, it's not as general as it should be, but then again, NOTHING
    will handle UK times in a sensible way. 
*/


/* 
    parse the DST start/end info:
    (Jddd|ddd|Mmon.nth.dow)[/hh:mm:ss]
*/

static char *
tz_parse_startend(char *s, struct tm *w, int *past)
{
    int dinm[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int ly, dozjd, d, m, n, hour, min, sec, j, k;
    time_t g;

    if (!s)    return 0;
    if (!w) return 0;
    if (!past) return 0;

    ly = 0;
    if (w->tm_year % 4        == 0) ly = 1;
    if (w->tm_year % 100      == 0) ly = 0;
    if (w->tm_year+1900 % 400 == 0) ly = 1;
    if (ly) dinm[1]++;

    dozjd = isdigit(*s);
    if (*s == 'J' || *s == 'j' || dozjd) {
        if (!dozjd && !isdigit(*++s)) return 0;
        d = *s++ - '0';
        if (isdigit(*s)) {
            d = d*10 + *s++ - '0';
            if (isdigit(*s)) {
                d = d*10 + *s++ - '0';
            }
        }
        if (d == 0) return 0;
        if (d > 366) return 0;
        d--;
        if (!dozjd && d > 58 && ly) d++;  /* after 28 feb */
        g = d * 86400;
        dozjd = 1;
    } else if (*s == 'M' || *s == 'm') {
        if (!isdigit(*++s)) return 0;
        m = *s++ - '0';
        if (isdigit(*s)) m = 10*m + *s++ - '0';
        if (*s != '.') return 0;
        if (!isdigit(*++s)) return 0;
        n = *s++ - '0';
        if (n < 1 || n > 5) return 0;
        if (*s != '.') return 0;
        if (!isdigit(*++s)) return 0;
        d = *s++ - '0';
        if (d > 6) return 0;
    }

    if (*s == '/') {
        if (!isdigit(*++s)) return 0;
        hour = *s++ - '0';
        if (isdigit(*s)) hour = 10*hour + *s++ - '0';
        if (*s == ':') {
            if (!isdigit(*++s)) return 0;
            min = *s++ - '0';
            if (isdigit(*s)) min = 10*min + *s++ - '0';
            if (*s == ':') {
                if (!isdigit(*++s)) return 0;
                sec = *s++ - '0';
                if (isdigit(*s)) sec = 10*sec + *s++ - '0';
            }
        }
    } else {
        hour = 2;
        min = 0;
        sec = 0;
    }

    if (dozjd) {
        if (w->tm_yday < d) goto before;
        if (w->tm_yday > d) goto after;
    } else {
        if (w->tm_mon+1 < m) goto before;
        if (w->tm_mon+1 > m) goto after;

        j = (42 + w->tm_wday - w->tm_mday)%7;   /*dow of mday 0 */
        k = d - j; /* mday of first d */
        if (k <= 0) k += 7;
        k += 7 * ((n>4?4:n)-1);  /* mday of n'th d */
        if (n == 5 && k+7 <= dinm[w->tm_mon]) k += 7;
        if (w->tm_mday < k) goto before;
        if (w->tm_mday > k) goto after;
    }

    if (w->tm_hour < hour) goto before;
    if (w->tm_hour > hour) goto after;
    if (w->tm_min  < min)  goto before;
    if (w->tm_min  > min)  goto after;
    if (w->tm_sec  < sec)  goto before;
    goto after;

before:
    *past = 0;
    return s;
after:
    *past = 1;
    return s;
}




/*  parse the offset:   (+|-)hh[:mm[:ss]]  */

static char *
tz_parse_offset(char *s, int *offset)
{
    int hour = 0, min = 0, sec = 0;
    int neg = 0;
    if (!s) return 0;
    if (!offset) return 0;

    if (*s == '-') {neg++; s++;}
    if (*s == '+') s++;
    if (!isdigit(*s)) return 0;
    hour = *s++ - '0';
    if (isdigit(*s)) hour = hour*10+(*s++ - '0');
    if (hour > 24) return 0;
    if (*s == ':') {
        if (!isdigit(*++s)) return 0;
        min = *s++ - '0';
        if (isdigit(*s)) min = min*10 + (*s++ - '0');
        if (min > 59) return 0;
        if (*s == ':') {
            if (!isdigit(*++s)) return 0;
            sec = *s++ - '0';
            if (isdigit(*s)) sec = sec*10 + (*s++ - '0');
            if (sec > 59) return 0;
        }
    }

    *offset = (hour*60+min)*60 + sec;
    if (neg) *offset = -*offset;
    return s;
}

/*
    input time is w, whatever type of time the CRTL localtime() uses.
    sets dst, the zone, and the gmtoff (seconds)

    caches the value of TZ and UCX$TZ env variables; note that 
    my_setenv looks for these and sets a flag if they're changed
    for efficiency. 

    We have to watch out for the "australian" case (dst starts in
    october, ends in april)...flagged by "reverse" and checked by
    scanning through the months of the previous year.

*/

static int
tz_parse(pTHX_ time_t *w, int *dst, char *zone, int *gmtoff)
{
    time_t when;
    struct tm *w2;
    char *s,*s2;
    char *dstzone, *tz, *s_start, *s_end;
    int std_off, dst_off, isdst;
    int y, dststart, dstend;
    static char envtz[1025];  /* longer than any logical, symbol, ... */
    static char ucxtz[1025];
    static char reversed = 0;

    if (!w) return 0;

    if (tz_updated) {
        tz_updated = 0;
        reversed = -1;  /* flag need to check  */
        envtz[0] = ucxtz[0] = '\0';
        tz = my_getenv("TZ",0);
        if (tz) strcpy(envtz, tz);
        tz = my_getenv("UCX$TZ",0);
        if (tz) strcpy(ucxtz, tz);
        if (!envtz[0] && !ucxtz[0]) return 0;  /* we give up */
    }
    tz = envtz;
    if (!*tz) tz = ucxtz;

    s = tz;
    while (isalpha(*s)) s++;
    s = tz_parse_offset(s, &std_off);
    if (!s) return 0;
    if (!*s) {                  /* no DST, hurray we're done! */
        isdst = 0;
        goto done;
    }

    dstzone = s;
    while (isalpha(*s)) s++;
    s2 = tz_parse_offset(s, &dst_off);
    if (s2) {
        s = s2;
    } else {
        dst_off = std_off - 3600;
    }

    if (!*s) {      /* default dst start/end?? */
        if (tz != ucxtz) {          /* if TZ tells zone only, UCX$TZ tells rule */
            s = strchr(ucxtz,',');
        }
        if (!s || !*s) s = ",M4.1.0,M10.5.0";   /* we know we do dst, default rule */
    }
    if (*s != ',') return 0;

    when = *w;
    when = _toutc(when);      /* convert to utc */
    when = when - std_off;    /* convert to pseudolocal time*/

    w2 = localtime(&when);
    y = w2->tm_year;
    s_start = s+1;
    s = tz_parse_startend(s_start,w2,&dststart);
    if (!s) return 0;
    if (*s != ',') return 0;

    when = *w;
    when = _toutc(when);      /* convert to utc */
    when = when - dst_off;    /* convert to pseudolocal time*/
    w2 = localtime(&when);
    if (w2->tm_year != y) {   /* spans a year, just check one time */
        when += dst_off - std_off;
        w2 = localtime(&when);
    }
    s_end = s+1;
    s = tz_parse_startend(s_end,w2,&dstend);
    if (!s) return 0;

    if (reversed == -1) {  /* need to check if start later than end */
        int j, ds, de;

        when = *w;
        if (when < 2*365*86400) {
            when += 2*365*86400;
        } else {
            when -= 365*86400;
        }
        w2 =localtime(&when);
        when = when + (15 - w2->tm_yday) * 86400;   /* jan 15 */

        for (j = 0; j < 12; j++) {
            w2 =localtime(&when);
            (void) tz_parse_startend(s_start,w2,&ds);
            (void) tz_parse_startend(s_end,w2,&de);
            if (ds != de) break;
            when += 30*86400;
        }
        reversed = 0;
        if (de && !ds) reversed = 1;
    }

    isdst = dststart && !dstend;
    if (reversed) isdst = dststart  || !dstend;

done:
    if (dst)    *dst = isdst;
    if (gmtoff) *gmtoff = isdst ? dst_off : std_off;
    if (isdst)  tz = dstzone;
    if (zone) {
        while(isalpha(*tz))  *zone++ = *tz++;
        *zone = '\0';
    }
    return 1;
}

#endif /* !RTL_USES_UTC */

/* my_time(), my_localtime(), my_gmtime()
 * By default traffic in UTC time values, using CRTL gmtime() or
 * SYS$TIMEZONE_DIFFERENTIAL to determine offset from local time zone.
 * Note: We need to use these functions even when the CRTL has working
 * UTC support, since they also handle C<use vmsish qw(times);>
 *
 * Contributed by Chuck Lane  <lane@duphy4.physics.drexel.edu>
 * Modified by Charles Bailey <bailey@newman.upenn.edu>
 */

/*{{{time_t my_time(time_t *timep)*/
time_t Perl_my_time(pTHX_ time_t *timep)
{
  time_t when;
  struct tm *tm_p;

  if (gmtime_emulation_type == 0) {
    int dstnow;
    time_t base = 15 * 86400; /* 15jan71; to avoid month/year ends between    */
                              /* results of calls to gmtime() and localtime() */
                              /* for same &base */

    gmtime_emulation_type++;
    if ((tm_p = gmtime(&base)) == NULL) { /* CRTL gmtime() is a fake */
      char off[LNM$C_NAMLENGTH+1];;

      gmtime_emulation_type++;
      if (!vmstrnenv("SYS$TIMEZONE_DIFFERENTIAL",off,0,fildev,0)) {
        gmtime_emulation_type++;
        utc_offset_secs = 0;
        Perl_warn(aTHX_ "no UTC offset information; assuming local time is UTC");
      }
      else { utc_offset_secs = atol(off); }
    }
    else { /* We've got a working gmtime() */
      struct tm gmt, local;

      gmt = *tm_p;
      tm_p = localtime(&base);
      local = *tm_p;
      utc_offset_secs  = (local.tm_mday - gmt.tm_mday) * 86400;
      utc_offset_secs += (local.tm_hour - gmt.tm_hour) * 3600;
      utc_offset_secs += (local.tm_min  - gmt.tm_min)  * 60;
      utc_offset_secs += (local.tm_sec  - gmt.tm_sec);
    }
  }

  when = time(NULL);
# ifdef VMSISH_TIME
# ifdef RTL_USES_UTC
  if (VMSISH_TIME) when = _toloc(when);
# else
  if (!VMSISH_TIME) when = _toutc(when);
# endif
# endif
  if (timep != NULL) *timep = when;
  return when;

}  /* end of my_time() */
/*}}}*/


/*{{{struct tm *my_gmtime(const time_t *timep)*/
struct tm *
Perl_my_gmtime(pTHX_ const time_t *timep)
{
  char *p;
  time_t when;
  struct tm *rsltmp;

  if (timep == NULL) {
    set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
    return NULL;
  }
  if (*timep == 0) gmtime_emulation_type = 0;  /* possibly reset TZ */

  when = *timep;
# ifdef VMSISH_TIME
  if (VMSISH_TIME) when = _toutc(when); /* Input was local time */
#  endif
# ifdef RTL_USES_UTC  /* this implies that the CRTL has a working gmtime() */
  return gmtime(&when);
# else
  /* CRTL localtime() wants local time as input, so does no tz correction */
  rsltmp = localtime(&when);
  if (rsltmp) rsltmp->tm_isdst = 0;  /* We already took DST into account */
  return rsltmp;
#endif
}  /* end of my_gmtime() */
/*}}}*/


/*{{{struct tm *my_localtime(const time_t *timep)*/
struct tm *
Perl_my_localtime(pTHX_ const time_t *timep)
{
  time_t when, whenutc;
  struct tm *rsltmp;
  int dst, offset;

  if (timep == NULL) {
    set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
    return NULL;
  }
  if (*timep == 0) gmtime_emulation_type = 0;  /* possibly reset TZ */
  if (gmtime_emulation_type == 0) (void) my_time(NULL); /* Init UTC */

  when = *timep;
# ifdef RTL_USES_UTC
# ifdef VMSISH_TIME
  if (VMSISH_TIME) when = _toutc(when);
# endif
  /* CRTL localtime() wants UTC as input, does tz correction itself */
  return localtime(&when);
  
# else /* !RTL_USES_UTC */
  whenutc = when;
# ifdef VMSISH_TIME
  if (!VMSISH_TIME) when = _toloc(whenutc);  /*  input was UTC */
  if (VMSISH_TIME) whenutc = _toutc(when);   /*  input was truelocal */
# endif
  dst = -1;
#ifndef RTL_USES_UTC
  if (tz_parse(aTHX_ &when, &dst, 0, &offset)) {   /* truelocal determines DST*/
      when = whenutc - offset;                   /* pseudolocal time*/
  }
# endif
  /* CRTL localtime() wants local time as input, so does no tz correction */
  rsltmp = localtime(&when);
  if (rsltmp && gmtime_emulation_type != 1) rsltmp->tm_isdst = dst;
  return rsltmp;
# endif

} /*  end of my_localtime() */
/*}}}*/

/* Reset definitions for later calls */
#define gmtime(t)    my_gmtime(t)
#define localtime(t) my_localtime(t)
#define time(t)      my_time(t)


/* my_utime - update modification time of a file
 * calling sequence is identical to POSIX utime(), but under
 * VMS only the modification time is changed; ODS-2 does not
 * maintain access times.  Restrictions differ from the POSIX
 * definition in that the time can be changed as long as the
 * caller has permission to execute the necessary IO$_MODIFY $QIO;
 * no separate checks are made to insure that the caller is the
 * owner of the file or has special privs enabled.
 * Code here is based on Joe Meadows' FILE utility.
 */

/* Adjustment from Unix epoch (01-JAN-1970 00:00:00.00)
 *              to VMS epoch  (01-JAN-1858 00:00:00.00)
 * in 100 ns intervals.
 */
static const long int utime_baseadjust[2] = { 0x4beb4000, 0x7c9567 };

/*{{{int my_utime(char *path, struct utimbuf *utimes)*/
int Perl_my_utime(pTHX_ char *file, struct utimbuf *utimes)
{
  register int i;
  long int bintime[2], len = 2, lowbit, unixtime,
           secscale = 10000000; /* seconds --> 100 ns intervals */
  unsigned long int chan, iosb[2], retsts;
  char vmsspec[NAM$C_MAXRSS+1], rsa[NAM$C_MAXRSS], esa[NAM$C_MAXRSS];
  struct FAB myfab = cc$rms_fab;
  struct NAM mynam = cc$rms_nam;
#if defined (__DECC) && defined (__VAX)
  /* VAX DEC C atrdef.h has unsigned type for pointer member atr$l_addr,
   * at least through VMS V6.1, which causes a type-conversion warning.
   */
#  pragma message save
#  pragma message disable cvtdiftypes
#endif
  struct atrdef myatr[2] = {{sizeof bintime, ATR$C_REVDATE, bintime}, {0,0,0}};
  struct fibdef myfib;
#if defined (__DECC) && defined (__VAX)
  /* This should be right after the declaration of myatr, but due
   * to a bug in VAX DEC C, this takes effect a statement early.
   */
#  pragma message restore
#endif
  struct dsc$descriptor fibdsc = {sizeof(myfib), DSC$K_DTYPE_Z, DSC$K_CLASS_S,(char *) &myfib},
                        devdsc = {0,DSC$K_DTYPE_T, DSC$K_CLASS_S,0},
                        fnmdsc = {0,DSC$K_DTYPE_T, DSC$K_CLASS_S,0};

  if (file == NULL || *file == '\0') {
    set_errno(ENOENT);
    set_vaxc_errno(LIB$_INVARG);
    return -1;
  }
  if (do_tovmsspec(file,vmsspec,0) == NULL) return -1;

  if (utimes != NULL) {
    /* Convert Unix time    (seconds since 01-JAN-1970 00:00:00.00)
     * to VMS quadword time (100 nsec intervals since 01-JAN-1858 00:00:00.00).
     * Since time_t is unsigned long int, and lib$emul takes a signed long int
     * as input, we force the sign bit to be clear by shifting unixtime right
     * one bit, then multiplying by an extra factor of 2 in lib$emul().
     */
    lowbit = (utimes->modtime & 1) ? secscale : 0;
    unixtime = (long int) utimes->modtime;
#   ifdef VMSISH_TIME
    /* If input was UTC; convert to local for sys svc */
    if (!VMSISH_TIME) unixtime = _toloc(unixtime);
#   endif
    unixtime >>= 1;  secscale <<= 1;
    retsts = lib$emul(&secscale, &unixtime, &lowbit, bintime);
    if (!(retsts & 1)) {
      set_errno(EVMSERR);
      set_vaxc_errno(retsts);
      return -1;
    }
    retsts = lib$addx(bintime,utime_baseadjust,bintime,&len);
    if (!(retsts & 1)) {
      set_errno(EVMSERR);
      set_vaxc_errno(retsts);
      return -1;
    }
  }
  else {
    /* Just get the current time in VMS format directly */
    retsts = sys$gettim(bintime);
    if (!(retsts & 1)) {
      set_errno(EVMSERR);
      set_vaxc_errno(retsts);
      return -1;
    }
  }

  myfab.fab$l_fna = vmsspec;
  myfab.fab$b_fns = (unsigned char) strlen(vmsspec);
  myfab.fab$l_nam = &mynam;
  mynam.nam$l_esa = esa;
  mynam.nam$b_ess = (unsigned char) sizeof esa;
  mynam.nam$l_rsa = rsa;
  mynam.nam$b_rss = (unsigned char) sizeof rsa;

  /* Look for the file to be affected, letting RMS parse the file
   * specification for us as well.  I have set errno using only
   * values documented in the utime() man page for VMS POSIX.
   */
  retsts = sys$parse(&myfab,0,0);
  if (!(retsts & 1)) {
    set_vaxc_errno(retsts);
    if      (retsts == RMS$_PRV) set_errno(EACCES);
    else if (retsts == RMS$_DIR) set_errno(ENOTDIR);
    else                         set_errno(EVMSERR);
    return -1;
  }
  retsts = sys$search(&myfab,0,0);
  if (!(retsts & 1)) {
    mynam.nam$b_nop |= NAM$M_SYNCHK;  mynam.nam$l_rlf = NULL;
    myfab.fab$b_dns = 0;  (void) sys$parse(&myfab,0,0);
    set_vaxc_errno(retsts);
    if      (retsts == RMS$_PRV) set_errno(EACCES);
    else if (retsts == RMS$_FNF) set_errno(ENOENT);
    else                         set_errno(EVMSERR);
    return -1;
  }

  devdsc.dsc$w_length = mynam.nam$b_dev;
  devdsc.dsc$a_pointer = (char *) mynam.nam$l_dev;

  retsts = sys$assign(&devdsc,&chan,0,0);
  if (!(retsts & 1)) {
    mynam.nam$b_nop |= NAM$M_SYNCHK;  mynam.nam$l_rlf = NULL;
    myfab.fab$b_dns = 0;  (void) sys$parse(&myfab,0,0);
    set_vaxc_errno(retsts);
    if      (retsts == SS$_IVDEVNAM)   set_errno(ENOTDIR);
    else if (retsts == SS$_NOPRIV)     set_errno(EACCES);
    else if (retsts == SS$_NOSUCHDEV)  set_errno(ENOTDIR);
    else                               set_errno(EVMSERR);
    return -1;
  }

  fnmdsc.dsc$a_pointer = mynam.nam$l_name;
  fnmdsc.dsc$w_length = mynam.nam$b_name + mynam.nam$b_type + mynam.nam$b_ver;

  memset((void *) &myfib, 0, sizeof myfib);
#if defined(__DECC) || defined(__DECCXX)
  for (i=0;i<3;i++) myfib.fib$w_fid[i] = mynam.nam$w_fid[i];
  for (i=0;i<3;i++) myfib.fib$w_did[i] = mynam.nam$w_did[i];
  /* This prevents the revision time of the file being reset to the current
   * time as a result of our IO$_MODIFY $QIO. */
  myfib.fib$l_acctl = FIB$M_NORECORD;
#else
  for (i=0;i<3;i++) myfib.fib$r_fid_overlay.fib$w_fid[i] = mynam.nam$w_fid[i];
  for (i=0;i<3;i++) myfib.fib$r_did_overlay.fib$w_did[i] = mynam.nam$w_did[i];
  myfib.fib$r_acctl_overlay.fib$l_acctl = FIB$M_NORECORD;
#endif
  retsts = sys$qiow(0,chan,IO$_MODIFY,iosb,0,0,&fibdsc,&fnmdsc,0,0,myatr,0);
  mynam.nam$b_nop |= NAM$M_SYNCHK;  mynam.nam$l_rlf = NULL;
  myfab.fab$b_dns = 0;  (void) sys$parse(&myfab,0,0);
  _ckvmssts(sys$dassgn(chan));
  if (retsts & 1) retsts = iosb[0];
  if (!(retsts & 1)) {
    set_vaxc_errno(retsts);
    if (retsts == SS$_NOPRIV) set_errno(EACCES);
    else                      set_errno(EVMSERR);
    return -1;
  }

  return 0;
}  /* end of my_utime() */
/*}}}*/

/*
 * flex_stat, flex_fstat
 * basic stat, but gets it right when asked to stat
 * a Unix-style path ending in a directory name (e.g. dir1/dir2/dir3)
 */

/* encode_dev packs a VMS device name string into an integer to allow
 * simple comparisons. This can be used, for example, to check whether two
 * files are located on the same device, by comparing their encoded device
 * names. Even a string comparison would not do, because stat() reuses the
 * device name buffer for each call; so without encode_dev, it would be
 * necessary to save the buffer and use strcmp (this would mean a number of
 * changes to the standard Perl code, to say nothing of what a Perl script
 * would have to do.
 *
 * The device lock id, if it exists, should be unique (unless perhaps compared
 * with lock ids transferred from other nodes). We have a lock id if the disk is
 * mounted cluster-wide, which is when we tend to get long (host-qualified)
 * device names. Thus we use the lock id in preference, and only if that isn't
 * available, do we try to pack the device name into an integer (flagged by
 * the sign bit (LOCKID_MASK) being set).
 *
 * Note that encode_dev cannot guarantee an 1-to-1 correspondence twixt device
 * name and its encoded form, but it seems very unlikely that we will find
 * two files on different disks that share the same encoded device names,
 * and even more remote that they will share the same file id (if the test
 * is to check for the same file).
 *
 * A better method might be to use sys$device_scan on the first call, and to
 * search for the device, returning an index into the cached array.
 * The number returned would be more intelligable.
 * This is probably not worth it, and anyway would take quite a bit longer
 * on the first call.
 */
#define LOCKID_MASK 0x80000000     /* Use 0 to force device name use only */
static mydev_t encode_dev (pTHX_ const char *dev)
{
  int i;
  unsigned long int f;
  mydev_t enc;
  char c;
  const char *q;

  if (!dev || !dev[0]) return 0;

#if LOCKID_MASK
  {
    struct dsc$descriptor_s dev_desc;
    unsigned long int status, lockid, item = DVI$_LOCKID;

    /* For cluster-mounted disks, the disk lock identifier is unique, so we
       can try that first. */
    dev_desc.dsc$w_length =  strlen (dev);
    dev_desc.dsc$b_dtype =   DSC$K_DTYPE_T;
    dev_desc.dsc$b_class =   DSC$K_CLASS_S;
    dev_desc.dsc$a_pointer = (char *) dev;
    _ckvmssts(lib$getdvi(&item, 0, &dev_desc, &lockid, 0, 0));
    if (lockid) return (lockid & ~LOCKID_MASK);
  }
#endif

  /* Otherwise we try to encode the device name */
  enc = 0;
  f = 1;
  i = 0;
  for (q = dev + strlen(dev); q--; q >= dev) {
    if (isdigit (*q))
      c= (*q) - '0';
    else if (isalpha (toupper (*q)))
      c= toupper (*q) - 'A' + (char)10;
    else
      continue; /* Skip '$'s */
    i++;
    if (i>6) break;     /* 36^7 is too large to fit in an unsigned long int */
    if (i>1) f *= 36;
    enc += f * (unsigned long int) c;
  }
  return (enc | LOCKID_MASK);  /* May have already overflowed into bit 31 */

}  /* end of encode_dev() */

static char namecache[NAM$C_MAXRSS+1];

static int
is_null_device(name)
    const char *name;
{
    /* The VMS null device is named "_NLA0:", usually abbreviated as "NL:".
       The underscore prefix, controller letter, and unit number are
       independently optional; for our purposes, the colon punctuation
       is not.  The colon can be trailed by optional directory and/or
       filename, but two consecutive colons indicates a nodename rather
       than a device.  [pr]  */
  if (*name == '_') ++name;
  if (tolower(*name++) != 'n') return 0;
  if (tolower(*name++) != 'l') return 0;
  if (tolower(*name) == 'a') ++name;
  if (*name == '0') ++name;
  return (*name++ == ':') && (*name != ':');
}

/* Do the permissions allow some operation?  Assumes PL_statcache already set. */
/* Do this via $Check_Access on VMS, since the CRTL stat() returns only a
 * subset of the applicable information.
 */
bool
Perl_cando(pTHX_ Mode_t bit, Uid_t effective, Stat_t *statbufp)
{
  char fname_phdev[NAM$C_MAXRSS+1];
  if (statbufp == &PL_statcache) return cando_by_name(bit,effective,namecache);
  else {
    char fname[NAM$C_MAXRSS+1];
    unsigned long int retsts;
    struct dsc$descriptor_s devdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0},
                            namdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};

    /* If the struct mystat is stale, we're OOL; stat() overwrites the
       device name on successive calls */
    devdsc.dsc$a_pointer = ((Stat_t *)statbufp)->st_devnam;
    devdsc.dsc$w_length = strlen(((Stat_t *)statbufp)->st_devnam);
    namdsc.dsc$a_pointer = fname;
    namdsc.dsc$w_length = sizeof fname - 1;

    retsts = lib$fid_to_name(&devdsc,&(((Stat_t *)statbufp)->st_ino),
                             &namdsc,&namdsc.dsc$w_length,0,0);
    if (retsts & 1) {
      fname[namdsc.dsc$w_length] = '\0';
/* 
 * lib$fid_to_name returns DVI$_LOGVOLNAM as the device part of the name,
 * but if someone has redefined that logical, Perl gets very lost.  Since
 * we have the physical device name from the stat buffer, just paste it on.
 */
      strcpy( fname_phdev, statbufp->st_devnam );
      strcat( fname_phdev, strrchr(fname, ':') );

      return cando_by_name(bit,effective,fname_phdev);
    }
    else if (retsts == SS$_NOSUCHDEV || retsts == SS$_NOSUCHFILE) {
      Perl_warn(aTHX_ "Can't get filespec - stale stat buffer?\n");
      return FALSE;
    }
    _ckvmssts(retsts);
    return FALSE;  /* Should never get to here */
  }
}  /* end of cando() */
/*}}}*/


/*{{{I32 cando_by_name(I32 bit, Uid_t effective, char *fname)*/
I32
Perl_cando_by_name(pTHX_ I32 bit, Uid_t effective, char *fname)
{
  static char usrname[L_cuserid];
  static struct dsc$descriptor_s usrdsc =
         {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, usrname};
  char vmsname[NAM$C_MAXRSS+1], fileified[NAM$C_MAXRSS+1];
  unsigned long int objtyp = ACL$C_FILE, access, retsts, privused, iosb[2];
  unsigned short int retlen, trnlnm_iter_count;
  struct dsc$descriptor_s namdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
  union prvdef curprv;
  struct itmlst_3 armlst[3] = {{sizeof access, CHP$_ACCESS, &access, &retlen},
         {sizeof privused, CHP$_PRIVUSED, &privused, &retlen},{0,0,0,0}};
  struct itmlst_3 jpilst[3] = {{sizeof curprv, JPI$_CURPRIV, &curprv, &retlen},
         {sizeof usrname, JPI$_USERNAME, &usrname, &usrdsc.dsc$w_length},
         {0,0,0,0}};
  struct itmlst_3 usrprolst[2] = {{sizeof curprv, CHP$_PRIV, &curprv, &retlen},
         {0,0,0,0}};
  struct dsc$descriptor_s usrprodsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};

  if (!fname || !*fname) return FALSE;
  /* Make sure we expand logical names, since sys$check_access doesn't */
  if (!strpbrk(fname,"/]>:")) {
    strcpy(fileified,fname);
    trnlnm_iter_count = 0;
    while (!strpbrk(fileified,"/]>:>") && my_trnlnm(fileified,fileified,0)) {
        trnlnm_iter_count++; 
        if (trnlnm_iter_count >= PERL_LNM_MAX_ITER) break;
    }
    fname = fileified;
  }
  if (!do_tovmsspec(fname,vmsname,1)) return FALSE;
  retlen = namdsc.dsc$w_length = strlen(vmsname);
  namdsc.dsc$a_pointer = vmsname;
  if (vmsname[retlen-1] == ']' || vmsname[retlen-1] == '>' ||
      vmsname[retlen-1] == ':') {
    if (!do_fileify_dirspec(vmsname,fileified,1)) return FALSE;
    namdsc.dsc$w_length = strlen(fileified);
    namdsc.dsc$a_pointer = fileified;
  }

  switch (bit) {
    case S_IXUSR: case S_IXGRP: case S_IXOTH:
      access = ARM$M_EXECUTE; break;
    case S_IRUSR: case S_IRGRP: case S_IROTH:
      access = ARM$M_READ; break;
    case S_IWUSR: case S_IWGRP: case S_IWOTH:
      access = ARM$M_WRITE; break;
    case S_IDUSR: case S_IDGRP: case S_IDOTH:
      access = ARM$M_DELETE; break;
    default:
      return FALSE;
  }

  /* Before we call $check_access, create a user profile with the current
   * process privs since otherwise it just uses the default privs from the
   * UAF and might give false positives or negatives.  This only works on
   * VMS versions v6.0 and later since that's when sys$create_user_profile
   * became available.
   */

  /* get current process privs and username */
  _ckvmssts(sys$getjpiw(0,0,0,jpilst,iosb,0,0));
  _ckvmssts(iosb[0]);

#if defined(__VMS_VER) && __VMS_VER >= 60000000

  /* find out the space required for the profile */
  _ckvmssts(sys$create_user_profile(&usrdsc,&usrprolst,0,0,
                                    &usrprodsc.dsc$w_length,0));

  /* allocate space for the profile and get it filled in */
  New(1330,usrprodsc.dsc$a_pointer,usrprodsc.dsc$w_length,char);
  _ckvmssts(sys$create_user_profile(&usrdsc,&usrprolst,0,usrprodsc.dsc$a_pointer,
                                    &usrprodsc.dsc$w_length,0));

  /* use the profile to check access to the file; free profile & analyze results */
  retsts = sys$check_access(&objtyp,&namdsc,0,armlst,0,0,0,&usrprodsc);
  Safefree(usrprodsc.dsc$a_pointer);
  if (retsts == SS$_NOCALLPRIV) retsts = SS$_NOPRIV; /* not really 3rd party */

#else

  retsts = sys$check_access(&objtyp,&namdsc,&usrdsc,armlst);

#endif

  if (retsts == SS$_NOPRIV      || retsts == SS$_NOSUCHOBJECT ||
      retsts == SS$_INVFILFOROP || retsts == RMS$_FNF || retsts == RMS$_SYN ||
      retsts == RMS$_DIR        || retsts == RMS$_DEV || retsts == RMS$_DNF) {
    set_vaxc_errno(retsts);
    if (retsts == SS$_NOPRIV) set_errno(EACCES);
    else if (retsts == SS$_INVFILFOROP) set_errno(EINVAL);
    else set_errno(ENOENT);
    return FALSE;
  }
  if (retsts == SS$_NORMAL || retsts == SS$_ACCONFLICT) {
    return TRUE;
  }
  _ckvmssts(retsts);

  return FALSE;  /* Should never get here */

}  /* end of cando_by_name() */
/*}}}*/


/*{{{ int flex_fstat(int fd, Stat_t *statbuf)*/
int
Perl_flex_fstat(pTHX_ int fd, Stat_t *statbufp)
{
  if (!fstat(fd,(stat_t *) statbufp)) {
    if (statbufp == (Stat_t *) &PL_statcache) *namecache == '\0';
    statbufp->st_dev = encode_dev(aTHX_ statbufp->st_devnam);
#   ifdef RTL_USES_UTC
#   ifdef VMSISH_TIME
    if (VMSISH_TIME) {
      statbufp->st_mtime = _toloc(statbufp->st_mtime);
      statbufp->st_atime = _toloc(statbufp->st_atime);
      statbufp->st_ctime = _toloc(statbufp->st_ctime);
    }
#   endif
#   else
#   ifdef VMSISH_TIME
    if (!VMSISH_TIME) { /* Return UTC instead of local time */
#   else
    if (1) {
#   endif
      statbufp->st_mtime = _toutc(statbufp->st_mtime);
      statbufp->st_atime = _toutc(statbufp->st_atime);
      statbufp->st_ctime = _toutc(statbufp->st_ctime);
    }
#endif
    return 0;
  }
  return -1;

}  /* end of flex_fstat() */
/*}}}*/

/*{{{ int flex_stat(const char *fspec, Stat_t *statbufp)*/
int
Perl_flex_stat(pTHX_ const char *fspec, Stat_t *statbufp)
{
    char fileified[NAM$C_MAXRSS+1];
    char temp_fspec[NAM$C_MAXRSS+300];
    int retval = -1;
    int saved_errno, saved_vaxc_errno;

    if (!fspec) return retval;
    saved_errno = errno; saved_vaxc_errno = vaxc$errno;
    strcpy(temp_fspec, fspec);
    if (statbufp == (Stat_t *) &PL_statcache)
      do_tovmsspec(temp_fspec,namecache,0);
    if (is_null_device(temp_fspec)) { /* Fake a stat() for the null device */
      memset(statbufp,0,sizeof *statbufp);
      statbufp->st_dev = encode_dev(aTHX_ "_NLA0:");
      statbufp->st_mode = S_IFBLK | S_IREAD | S_IWRITE | S_IEXEC;
      statbufp->st_uid = 0x00010001;
      statbufp->st_gid = 0x0001;
      time((time_t *)&statbufp->st_mtime);
      statbufp->st_atime = statbufp->st_ctime = statbufp->st_mtime;
      return 0;
    }

    /* Try for a directory name first.  If fspec contains a filename without
     * a type (e.g. sea:[wine.dark]water), and both sea:[wine.dark]water.dir
     * and sea:[wine.dark]water. exist, we prefer the directory here.
     * Similarly, sea:[wine.dark] returns the result for sea:[wine]dark.dir,
     * not sea:[wine.dark]., if the latter exists.  If the intended target is
     * the file with null type, specify this by calling flex_stat() with
     * a '.' at the end of fspec.
     */
    if (do_fileify_dirspec(temp_fspec,fileified,0) != NULL) {
      retval = stat(fileified,(stat_t *) statbufp);
      if (!retval && statbufp == (Stat_t *) &PL_statcache)
        strcpy(namecache,fileified);
    }
    if (retval) retval = stat(temp_fspec,(stat_t *) statbufp);
    if (!retval) {
      statbufp->st_dev = encode_dev(aTHX_ statbufp->st_devnam);
#     ifdef RTL_USES_UTC
#     ifdef VMSISH_TIME
      if (VMSISH_TIME) {
        statbufp->st_mtime = _toloc(statbufp->st_mtime);
        statbufp->st_atime = _toloc(statbufp->st_atime);
        statbufp->st_ctime = _toloc(statbufp->st_ctime);
      }
#     endif
#     else
#     ifdef VMSISH_TIME
      if (!VMSISH_TIME) { /* Return UTC instead of local time */
#     else
      if (1) {
#     endif
        statbufp->st_mtime = _toutc(statbufp->st_mtime);
        statbufp->st_atime = _toutc(statbufp->st_atime);
        statbufp->st_ctime = _toutc(statbufp->st_ctime);
      }
#     endif
    }
    /* If we were successful, leave errno where we found it */
    if (retval == 0) { errno = saved_errno; vaxc$errno = saved_vaxc_errno; }
    return retval;

}  /* end of flex_stat() */
/*}}}*/


/*{{{char *my_getlogin()*/
/* VMS cuserid == Unix getlogin, except calling sequence */
char *
my_getlogin()
{
    static char user[L_cuserid];
    return cuserid(user);
}
/*}}}*/


/*  rmscopy - copy a file using VMS RMS routines
 *
 *  Copies contents and attributes of spec_in to spec_out, except owner
 *  and protection information.  Name and type of spec_in are used as
 *  defaults for spec_out.  The third parameter specifies whether rmscopy()
 *  should try to propagate timestamps from the input file to the output file.
 *  If it is less than 0, no timestamps are preserved.  If it is 0, then
 *  rmscopy() will behave similarly to the DCL COPY command: timestamps are
 *  propagated to the output file at creation iff the output file specification
 *  did not contain an explicit name or type, and the revision date is always
 *  updated at the end of the copy operation.  If it is greater than 0, then
 *  it is interpreted as a bitmask, in which bit 0 indicates that timestamps
 *  other than the revision date should be propagated, and bit 1 indicates
 *  that the revision date should be propagated.
 *
 *  Returns 1 on success; returns 0 and sets errno and vaxc$errno on failure.
 *
 *  Copyright 1996 by Charles Bailey <bailey@newman.upenn.edu>.
 *  Incorporates, with permission, some code from EZCOPY by Tim Adye
 *  <T.J.Adye@rl.ac.uk>.  Permission is given to distribute this code
 * as part of the Perl standard distribution under the terms of the
 * GNU General Public License or the Perl Artistic License.  Copies
 * of each may be found in the Perl standard distribution.
 */
/*{{{int rmscopy(char *src, char *dst, int preserve_dates)*/
int
Perl_rmscopy(pTHX_ char *spec_in, char *spec_out, int preserve_dates)
{
    char vmsin[NAM$C_MAXRSS+1], vmsout[NAM$C_MAXRSS+1], esa[NAM$C_MAXRSS],
         rsa[NAM$C_MAXRSS], ubf[32256];
    unsigned long int i, sts, sts2;
    struct FAB fab_in, fab_out;
    struct RAB rab_in, rab_out;
    struct NAM nam;
    struct XABDAT xabdat;
    struct XABFHC xabfhc;
    struct XABRDT xabrdt;
    struct XABSUM xabsum;

    if (!spec_in  || !*spec_in  || !do_tovmsspec(spec_in,vmsin,1) ||
        !spec_out || !*spec_out || !do_tovmsspec(spec_out,vmsout,1)) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      return 0;
    }

    fab_in = cc$rms_fab;
    fab_in.fab$l_fna = vmsin;
    fab_in.fab$b_fns = strlen(vmsin);
    fab_in.fab$b_shr = FAB$M_SHRPUT | FAB$M_UPI;
    fab_in.fab$b_fac = FAB$M_BIO | FAB$M_GET;
    fab_in.fab$l_fop = FAB$M_SQO;
    fab_in.fab$l_nam =  &nam;
    fab_in.fab$l_xab = (void *) &xabdat;

    nam = cc$rms_nam;
    nam.nam$l_rsa = rsa;
    nam.nam$b_rss = sizeof(rsa);
    nam.nam$l_esa = esa;
    nam.nam$b_ess = sizeof (esa);
    nam.nam$b_esl = nam.nam$b_rsl = 0;

    xabdat = cc$rms_xabdat;        /* To get creation date */
    xabdat.xab$l_nxt = (void *) &xabfhc;

    xabfhc = cc$rms_xabfhc;        /* To get record length */
    xabfhc.xab$l_nxt = (void *) &xabsum;

    xabsum = cc$rms_xabsum;        /* To get key and area information */

    if (!((sts = sys$open(&fab_in)) & 1)) {
      set_vaxc_errno(sts);
      switch (sts) {
        case RMS$_FNF: case RMS$_DNF:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_SYN:
          set_errno(EINVAL); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        default:
          set_errno(EVMSERR);
      }
      return 0;
    }

    fab_out = fab_in;
    fab_out.fab$w_ifi = 0;
    fab_out.fab$b_fac = FAB$M_BIO | FAB$M_PUT;
    fab_out.fab$b_shr = FAB$M_SHRGET | FAB$M_UPI;
    fab_out.fab$l_fop = FAB$M_SQO;
    fab_out.fab$l_fna = vmsout;
    fab_out.fab$b_fns = strlen(vmsout);
    fab_out.fab$l_dna = nam.nam$l_name;
    fab_out.fab$b_dns = nam.nam$l_name ? nam.nam$b_name + nam.nam$b_type : 0;

    if (preserve_dates == 0) {  /* Act like DCL COPY */
      nam.nam$b_nop = NAM$M_SYNCHK;
      fab_out.fab$l_xab = NULL;  /* Don't disturb data from input file */
      if (!((sts = sys$parse(&fab_out)) & 1)) {
        set_errno(sts == RMS$_SYN ? EINVAL : EVMSERR);
        set_vaxc_errno(sts);
        return 0;
      }
      fab_out.fab$l_xab = (void *) &xabdat;
      if (nam.nam$l_fnb & (NAM$M_EXP_NAME | NAM$M_EXP_TYPE)) preserve_dates = 1;
    }
    fab_out.fab$l_nam = (void *) 0;  /* Done with NAM block */
    if (preserve_dates < 0)   /* Clear all bits; we'll use it as a */
      preserve_dates =0;      /* bitmask from this point forward   */

    if (!(preserve_dates & 1)) fab_out.fab$l_xab = (void *) &xabfhc;
    if (!((sts = sys$create(&fab_out)) & 1)) {
      set_vaxc_errno(sts);
      switch (sts) {
        case RMS$_DNF:
          set_errno(ENOENT); break;
        case RMS$_DIR:
          set_errno(ENOTDIR); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_SYN:
          set_errno(EINVAL); break;
        case RMS$_PRV:
          set_errno(EACCES); break;
        default:
          set_errno(EVMSERR);
      }
      return 0;
    }
    fab_out.fab$l_fop |= FAB$M_DLT;  /* in case we have to bail out */
    if (preserve_dates & 2) {
      /* sys$close() will process xabrdt, not xabdat */
      xabrdt = cc$rms_xabrdt;
#ifndef __GNUC__
      xabrdt.xab$q_rdt = xabdat.xab$q_rdt;
#else
      /* gcc doesn't like the assignment, since its prototype for xab$q_rdt
       * is unsigned long[2], while DECC & VAXC use a struct */
      memcpy(xabrdt.xab$q_rdt,xabdat.xab$q_rdt,sizeof xabrdt.xab$q_rdt);
#endif
      fab_out.fab$l_xab = (void *) &xabrdt;
    }

    rab_in = cc$rms_rab;
    rab_in.rab$l_fab = &fab_in;
    rab_in.rab$l_rop = RAB$M_BIO;
    rab_in.rab$l_ubf = ubf;
    rab_in.rab$w_usz = sizeof ubf;
    if (!((sts = sys$connect(&rab_in)) & 1)) {
      sys$close(&fab_in); sys$close(&fab_out);
      set_errno(EVMSERR); set_vaxc_errno(sts);
      return 0;
    }

    rab_out = cc$rms_rab;
    rab_out.rab$l_fab = &fab_out;
    rab_out.rab$l_rbf = ubf;
    if (!((sts = sys$connect(&rab_out)) & 1)) {
      sys$close(&fab_in); sys$close(&fab_out);
      set_errno(EVMSERR); set_vaxc_errno(sts);
      return 0;
    }

    while ((sts = sys$read(&rab_in))) {  /* always true  */
      if (sts == RMS$_EOF) break;
      rab_out.rab$w_rsz = rab_in.rab$w_rsz;
      if (!(sts & 1) || !((sts = sys$write(&rab_out)) & 1)) {
        sys$close(&fab_in); sys$close(&fab_out);
        set_errno(EVMSERR); set_vaxc_errno(sts);
        return 0;
      }
    }

    fab_out.fab$l_fop &= ~FAB$M_DLT;  /* We got this far; keep the output */
    sys$close(&fab_in);  sys$close(&fab_out);
    sts = (fab_in.fab$l_sts & 1) ? fab_out.fab$l_sts : fab_in.fab$l_sts;
    if (!(sts & 1)) {
      set_errno(EVMSERR); set_vaxc_errno(sts);
      return 0;
    }

    return 1;

}  /* end of rmscopy() */
/*}}}*/


/***  The following glue provides 'hooks' to make some of the routines
 * from this file available from Perl.  These routines are sufficiently
 * basic, and are required sufficiently early in the build process,
 * that's it's nice to have them available to miniperl as well as the
 * full Perl, so they're set up here instead of in an extension.  The
 * Perl code which handles importation of these names into a given
 * package lives in [.VMS]Filespec.pm in @INC.
 */

void
rmsexpand_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *fspec, *defspec = NULL, *rslt;
  STRLEN n_a;

  if (!items || items > 2)
    Perl_croak(aTHX_ "Usage: VMS::Filespec::rmsexpand(spec[,defspec])");
  fspec = SvPV(ST(0),n_a);
  if (!fspec || !*fspec) XSRETURN_UNDEF;
  if (items == 2) defspec = SvPV(ST(1),n_a);

  rslt = do_rmsexpand(fspec,NULL,1,defspec,0);
  ST(0) = sv_newmortal();
  if (rslt != NULL) sv_usepvn(ST(0),rslt,strlen(rslt));
  XSRETURN(1);
}

void
vmsify_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *vmsified;
  STRLEN n_a;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::vmsify(spec)");
  vmsified = do_tovmsspec(SvPV(ST(0),n_a),NULL,1);
  ST(0) = sv_newmortal();
  if (vmsified != NULL) sv_usepvn(ST(0),vmsified,strlen(vmsified));
  XSRETURN(1);
}

void
unixify_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *unixified;
  STRLEN n_a;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::unixify(spec)");
  unixified = do_tounixspec(SvPV(ST(0),n_a),NULL,1);
  ST(0) = sv_newmortal();
  if (unixified != NULL) sv_usepvn(ST(0),unixified,strlen(unixified));
  XSRETURN(1);
}

void
fileify_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *fileified;
  STRLEN n_a;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::fileify(spec)");
  fileified = do_fileify_dirspec(SvPV(ST(0),n_a),NULL,1);
  ST(0) = sv_newmortal();
  if (fileified != NULL) sv_usepvn(ST(0),fileified,strlen(fileified));
  XSRETURN(1);
}

void
pathify_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *pathified;
  STRLEN n_a;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::pathify(spec)");
  pathified = do_pathify_dirspec(SvPV(ST(0),n_a),NULL,1);
  ST(0) = sv_newmortal();
  if (pathified != NULL) sv_usepvn(ST(0),pathified,strlen(pathified));
  XSRETURN(1);
}

void
vmspath_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *vmspath;
  STRLEN n_a;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::vmspath(spec)");
  vmspath = do_tovmspath(SvPV(ST(0),n_a),NULL,1);
  ST(0) = sv_newmortal();
  if (vmspath != NULL) sv_usepvn(ST(0),vmspath,strlen(vmspath));
  XSRETURN(1);
}

void
unixpath_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char *unixpath;
  STRLEN n_a;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::unixpath(spec)");
  unixpath = do_tounixpath(SvPV(ST(0),n_a),NULL,1);
  ST(0) = sv_newmortal();
  if (unixpath != NULL) sv_usepvn(ST(0),unixpath,strlen(unixpath));
  XSRETURN(1);
}

void
candelete_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char fspec[NAM$C_MAXRSS+1], *fsp;
  SV *mysv;
  IO *io;
  STRLEN n_a;

  if (items != 1) Perl_croak(aTHX_ "Usage: VMS::Filespec::candelete(spec)");

  mysv = SvROK(ST(0)) ? SvRV(ST(0)) : ST(0);
  if (SvTYPE(mysv) == SVt_PVGV) {
    if (!(io = GvIOp(mysv)) || !PerlIO_getname(IoIFP(io),fspec)) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      XSRETURN(1);
    }
    fsp = fspec;
  }
  else {
    if (mysv != ST(0) || !(fsp = SvPV(mysv,n_a)) || !*fsp) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      XSRETURN(1);
    }
  }

  ST(0) = boolSV(cando_by_name(S_IDUSR,0,fsp));
  XSRETURN(1);
}

void
rmscopy_fromperl(pTHX_ CV *cv)
{
  dXSARGS;
  char inspec[NAM$C_MAXRSS+1], outspec[NAM$C_MAXRSS+1], *inp, *outp;
  int date_flag;
  struct dsc$descriptor indsc  = { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0},
                        outdsc = { 0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
  unsigned long int sts;
  SV *mysv;
  IO *io;
  STRLEN n_a;

  if (items < 2 || items > 3)
    Perl_croak(aTHX_ "Usage: File::Copy::rmscopy(from,to[,date_flag])");

  mysv = SvROK(ST(0)) ? SvRV(ST(0)) : ST(0);
  if (SvTYPE(mysv) == SVt_PVGV) {
    if (!(io = GvIOp(mysv)) || !PerlIO_getname(IoIFP(io),inspec)) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      XSRETURN(1);
    }
    inp = inspec;
  }
  else {
    if (mysv != ST(0) || !(inp = SvPV(mysv,n_a)) || !*inp) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      XSRETURN(1);
    }
  }
  mysv = SvROK(ST(1)) ? SvRV(ST(1)) : ST(1);
  if (SvTYPE(mysv) == SVt_PVGV) {
    if (!(io = GvIOp(mysv)) || !PerlIO_getname(IoIFP(io),outspec)) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      XSRETURN(1);
    }
    outp = outspec;
  }
  else {
    if (mysv != ST(1) || !(outp = SvPV(mysv,n_a)) || !*outp) {
      set_errno(EINVAL); set_vaxc_errno(LIB$_INVARG);
      ST(0) = &PL_sv_no;
      XSRETURN(1);
    }
  }
  date_flag = (items == 3) ? SvIV(ST(2)) : 0;

  ST(0) = boolSV(rmscopy(inp,outp,date_flag));
  XSRETURN(1);
}


void
mod2fname(pTHX_ CV *cv)
{
  dXSARGS;
  char ultimate_name[NAM$C_MAXRSS+1], work_name[NAM$C_MAXRSS*8 + 1],
       workbuff[NAM$C_MAXRSS*1 + 1];
  int total_namelen = 3, counter, num_entries;
  /* ODS-5 ups this, but we want to be consistent, so... */
  int max_name_len = 39;
  AV *in_array = (AV *)SvRV(ST(0));

  num_entries = av_len(in_array);

  /* All the names start with PL_. */
  strcpy(ultimate_name, "PL_");

  /* Clean up our working buffer */
  Zero(work_name, sizeof(work_name), char);

  /* Run through the entries and build up a working name */
  for(counter = 0; counter <= num_entries; counter++) {
    /* If it's not the first name then tack on a __ */
    if (counter) {
      strcat(work_name, "__");
    }
    strcat(work_name, SvPV(*av_fetch(in_array, counter, FALSE),
			   PL_na));
  }

  /* Check to see if we actually have to bother...*/
  if (strlen(work_name) + 3 <= max_name_len) {
    strcat(ultimate_name, work_name);
  } else {
    /* It's too darned big, so we need to go strip. We use the same */
    /* algorithm as xsubpp does. First, strip out doubled __ */
    char *source, *dest, last;
    dest = workbuff;
    last = 0;
    for (source = work_name; *source; source++) {
      if (last == *source && last == '_') {
	continue;
      }
      *dest++ = *source;
      last = *source;
    }
    /* Go put it back */
    strcpy(work_name, workbuff);
    /* Is it still too big? */
    if (strlen(work_name) + 3 > max_name_len) {
      /* Strip duplicate letters */
      last = 0;
      dest = workbuff;
      for (source = work_name; *source; source++) {
	if (last == toupper(*source)) {
	continue;
	}
	*dest++ = *source;
	last = toupper(*source);
      }
      strcpy(work_name, workbuff);
    }

    /* Is it *still* too big? */
    if (strlen(work_name) + 3 > max_name_len) {
      /* Too bad, we truncate */
      work_name[max_name_len - 2] = 0;
    }
    strcat(ultimate_name, work_name);
  }

  /* Okay, return it */
  ST(0) = sv_2mortal(newSVpv(ultimate_name, 0));
  XSRETURN(1);
}

void
hushexit_fromperl(pTHX_ CV *cv)
{
    dXSARGS;

    if (items > 0) {
        VMSISH_HUSHED = SvTRUE(ST(0));
    }
    ST(0) = boolSV(VMSISH_HUSHED);
    XSRETURN(1);
}

void  
Perl_sys_intern_dup(pTHX_ struct interp_intern *src, 
                          struct interp_intern *dst)
{
    memcpy(dst,src,sizeof(struct interp_intern));
}

void  
Perl_sys_intern_clear(pTHX)
{
}

void  
Perl_sys_intern_init(pTHX)
{
    unsigned int ix = RAND_MAX;
    double x;

    VMSISH_HUSHED = 0;

    x = (float)ix;
    MY_INV_RAND_MAX = 1./x;
}

void
init_os_extras()
{
  dTHX;
  char* file = __FILE__;
  char temp_buff[512];
  if (my_trnlnm("DECC$DISABLE_TO_VMS_LOGNAME_TRANSLATION", temp_buff, 0)) {
    no_translate_barewords = TRUE;
  } else {
    no_translate_barewords = FALSE;
  }

  newXSproto("VMS::Filespec::rmsexpand",rmsexpand_fromperl,file,"$;$");
  newXSproto("VMS::Filespec::vmsify",vmsify_fromperl,file,"$");
  newXSproto("VMS::Filespec::unixify",unixify_fromperl,file,"$");
  newXSproto("VMS::Filespec::pathify",pathify_fromperl,file,"$");
  newXSproto("VMS::Filespec::fileify",fileify_fromperl,file,"$");
  newXSproto("VMS::Filespec::vmspath",vmspath_fromperl,file,"$");
  newXSproto("VMS::Filespec::unixpath",unixpath_fromperl,file,"$");
  newXSproto("VMS::Filespec::candelete",candelete_fromperl,file,"$");
  newXSproto("DynaLoader::mod2fname", mod2fname, file, "$");
  newXS("File::Copy::rmscopy",rmscopy_fromperl,file);
  newXSproto("vmsish::hushed",hushexit_fromperl,file,";$");

  store_pipelocs(aTHX);         /* will redo any earlier attempts */

  return;
}
  
/*  End of vms.c */
