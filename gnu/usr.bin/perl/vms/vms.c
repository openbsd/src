/* vms.c
 *
 * VMS-specific routines for perl5
 *
 * Last revised: 20-Aug-1999 by Charles Bailey  bailey@newman.upenn.edu
 * Version: 5.5.60
 */

#include <acedef.h>
#include <acldef.h>
#include <armdef.h>
#include <atrdef.h>
#include <chpdef.h>
#include <clidef.h>
#include <climsgdef.h>
#include <descrip.h>
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

/* Temp for subprocess commands */
static struct dsc$descriptor_s VMScmd = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,Nullch};

/*{{{int vmstrnenv(const char *lnm, char *eqv, unsigned long int idx, struct dsc$descriptor_s **tabvec, unsigned long int flags) */
int
vmstrnenv(const char *lnm, char *eqv, unsigned long int idx,
  struct dsc$descriptor_s **tabvec, unsigned long int flags)
{
    char uplnm[LNM$C_NAMLENGTH+1], *cp1, *cp2;
    unsigned short int eqvlen, curtab, ivlnm = 0, ivsym = 0, ivenv = 0, secure;
    unsigned long int retsts, attr = LNM$M_CASE_BLIND;
    unsigned char acmode;
    struct dsc$descriptor_s lnmdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0},
                            tmpdsc = {6,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
    struct itmlst_3 lnmlst[3] = {{sizeof idx, LNM$_INDEX, &idx, 0},
                                 {LNM$C_NAMLENGTH, LNM$_STRING, eqv, &eqvlen},
                                 {0, 0, 0, 0}};
    $DESCRIPTOR(crtlenv,"CRTL_ENV");  $DESCRIPTOR(clisym,"CLISYM");
#if defined(USE_THREADS)
    /* We jump through these hoops because we can be called at */
    /* platform-specific initialization time, which is before anything is */
    /* set up--we can't even do a plain dTHX since that relies on the */
    /* interpreter structure to be initialized */
    struct perl_thread *thr;
    if (PL_curinterp) {
      thr = PL_threadnum? THR : (struct perl_thread*)SvPVX(PL_thrsv);
    } else {
      thr = NULL;
    }
#endif

    if (!lnm || !eqv || idx > LNM$_MAX_INDEX) {
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
#if defined(USE_THREADS)
	      if (thr && PL_curcop) {
#endif
		if (ckWARN(WARN_MISC)) {
		  Perl_warner(aTHX_ WARN_MISC,"Value of CLI symbol \"%s\" too long",lnm);
		}
#if defined(USE_THREADS)
	      } else {
		  Perl_warner(aTHX_ WARN_MISC,"Value of CLI symbol \"%s\" too long",lnm);
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
        retsts = sys$trnlnm(&attr,tabvec[curtab],&lnmdsc,&acmode,lnmlst);
        if (retsts == SS$_IVLOGNAM) { ivlnm = 1; continue; }
        if (retsts == SS$_NOLOGNAM) continue;
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
int my_trnlnm(const char *lnm, char *eqv, unsigned long int idx)
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
    static char __my_getenv_eqv[LNM$C_NAMLENGTH+1];
    char uplnm[LNM$C_NAMLENGTH+1], *cp1, *cp2, *eqv;
    unsigned long int idx = 0;
    int trnsuccess;
    SV *tmpsv;

    if (PL_curinterp) {  /* Perl interpreter running -- may be threaded */
      /* Set up a temporary buffer for the return value; Perl will
       * clean it up at the next statement transition */
      tmpsv = sv_2mortal(newSVpv("",LNM$C_NAMLENGTH+1));
      if (!tmpsv) return NULL;
      eqv = SvPVX(tmpsv);
    }
    else eqv = __my_getenv_eqv;  /* Assume no interpreter ==> single thread */
    for (cp1 = (char *) lnm, cp2 = eqv; *cp1; cp1++,cp2++) *cp2 = _toupper(*cp1);
    if (cp1 - lnm == 7 && !strncmp(eqv,"DEFAULT",7)) {
      getcwd(eqv,LNM$C_NAMLENGTH);
      return eqv;
    }
    else {
      if ((cp2 = strchr(lnm,';')) != NULL) {
        strcpy(uplnm,lnm);
        uplnm[cp2-lnm] = '\0';
        idx = strtoul(cp2+1,NULL,0);
        lnm = uplnm;
      }
      /* Impose security constraints only if tainting */
      if (sys) sys = PL_curinterp ? PL_tainting : will_taint;
      if (vmstrnenv(lnm,eqv,idx,
                    sys ? fildev : NULL,
#ifdef SECURE_INTERNAL_GETENV
                    sys ? PERL__TRNENV_SECURE : 0
#else
                                                0
#endif
                                                 )) return eqv;
      else return Nullch;
    }

}  /* end of my_getenv() */
/*}}}*/


/*{{{ SV *my_getenv_len(const char *lnm, bool sys)*/
char *
my_getenv_len(const char *lnm, unsigned long *len, bool sys)
{
    dTHX;
    char *buf, *cp1, *cp2;
    unsigned long idx = 0;
    static char __my_getenv_len_eqv[LNM$C_NAMLENGTH+1];
    SV *tmpsv;
    
    if (PL_curinterp) {  /* Perl interpreter running -- may be threaded */
      /* Set up a temporary buffer for the return value; Perl will
       * clean it up at the next statement transition */
      tmpsv = sv_2mortal(newSVpv("",LNM$C_NAMLENGTH+1));
      if (!tmpsv) return NULL;
      buf = SvPVX(tmpsv);
    }
    else buf = __my_getenv_len_eqv;  /* Assume no interpreter ==> single thread */
    for (cp1 = (char *)lnm, cp2 = buf; *cp1; cp1++,cp2++) *cp2 = _toupper(*cp1);
    if (cp1 - lnm == 7 && !strncmp(buf,"DEFAULT",7)) {
      getcwd(buf,LNM$C_NAMLENGTH);
      *len = strlen(buf);
      return buf;
    }
    else {
      if ((cp2 = strchr(lnm,';')) != NULL) {
        strcpy(buf,lnm);
        buf[cp2-lnm] = '\0';
        idx = strtoul(cp2+1,NULL,0);
        lnm = buf;
      }
      /* Impose security constraints only if tainting */
      if (sys) sys = PL_curinterp ? PL_tainting : will_taint;
      if ((*len = vmstrnenv(lnm,buf,idx,
                           sys ? fildev : NULL,
#ifdef SECURE_INTERNAL_GETENV
                           sys ? PERL__TRNENV_SECURE : 0
#else
                                                       0
#endif
                                                         )))
	  return buf;
      else
	  return Nullch;
    }

}  /* end of my_getenv_len() */
/*}}}*/

static void create_mbx(unsigned short int *, struct dsc$descriptor_s *);

static void riseandshine(unsigned long int dummy) { sys$wake(0,0); }

/*{{{ void prime_env_iter() */
void
prime_env_iter(void)
/* Fill the %ENV associative array with all logical names we can
 * find, in preparation for iterating over it.
 */
{
  dTHX;
  static int primed = 0;
  HV *seenhv = NULL, *envhv;
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
#ifdef USE_THREADS
  static perl_mutex primenv_mutex;
  MUTEX_INIT(&primenv_mutex);
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
            Perl_warner(aTHX_ WARN_INTERNAL,"Ill-formed CRTL environ value \"%s\"\n",environ[j]);
        }
        else {
          start++;
          (void) hv_store(envhv,environ[j],start - environ[j] - 1,
                          newSVpv(start,0),0);
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
        Perl_warner(aTHX_ WARN_INTERNAL,"Buffer overflow in prime_env_iter: %s",buf);

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
        Perl_warner(aTHX_ WARN_INTERNAL,"Ill-formed message in prime_env_iter: |%s|",buf);
        continue;
      }
      PERL_HASH(hash,key,keylen);
      hv_store(envhv,key,keylen,newSVpvn(cp2,cp1 - cp2 + 1),hash);
      hv_store(seenhv,key,keylen,&PL_sv_yes,hash);
    }
    if (cmddsc.dsc$w_length == 14) { /* We just read LNM$FILE_DEV */
      /* get the PPFs for this process, not the subprocess */
      char *ppfs[] = {"SYS$COMMAND", "SYS$INPUT", "SYS$OUTPUT", "SYS$ERROR", NULL};
      char eqv[LNM$C_NAMLENGTH+1];
      int trnlen, i;
      for (i = 0; ppfs[i]; i++) {
        trnlen = vmstrnenv(ppfs[i],eqv,0,fildev,0);
        hv_store(envhv,ppfs[i],strlen(ppfs[i]),newSVpv(eqv,trnlen),0);
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
vmssetenv(char *lnm, char *eqv, struct dsc$descriptor_s **tabvec)
{
    char uplnm[LNM$C_NAMLENGTH], *cp1, *cp2;
    unsigned short int curtab, ivlnm = 0, ivsym = 0, ivenv = 0;
    unsigned long int retsts, usermode = PSL$C_USER;
    struct dsc$descriptor_s lnmdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,uplnm},
                            eqvdsc = {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0},
                            tmpdsc = {6,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
    $DESCRIPTOR(crtlenv,"CRTL_ENV");  $DESCRIPTOR(clisym,"CLISYM");
    $DESCRIPTOR(local,"_LOCAL");
    dTHX;

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
          for (i = 0; environ[i]; i++) { /* Iff it's an environ elt, reset */
            if ((cp1 = strchr(environ[i],'=')) && 
                !strncmp(environ[i],lnm,cp1 - environ[i])) {
#ifdef HAS_SETENV
              return setenv(lnm,eqv,1) ? vaxc$errno : 0;
            }
          }
          ivenv = 1; retsts = SS$_NOLOGNAM;
#else
              if (ckWARN(WARN_INTERNAL))
                Perl_warner(aTHX_ WARN_INTERNAL,"This Perl can't reset CRTL environ elements (%s)",lnm);
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
          Perl_warner(aTHX_ WARN_INTERNAL,"This Perl can't set CRTL environ elements (%s=%s)",lnm,eqv);
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
	    eqvdsc.dsc$w_length = LNM$C_NAMLENGTH;
	    if (ckWARN(WARN_MISC)) {
	      Perl_warner(aTHX_ WARN_MISC,"Value of logical \"%s\" too long. Truncating to %i bytes",lnm, LNM$C_NAMLENGTH);
	    }
	  }
          retsts = lib$set_logical(&lnmdsc,&eqvdsc,tabvec[0],0,0);
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
  if (lnm && *lnm && strlen(lnm) == 7) {
    char uplnm[8];
    int i;
    for (i = 0; lnm[i]; i++) uplnm[i] = _toupper(lnm[i]);
    if (!strcmp(uplnm,"DEFAULT")) {
      if (eqv && *eqv) chdir(eqv);
      return;
    }
  }
  (void) vmssetenv(lnm,eqv,NULL);
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
my_crypt(const char *textpasswd, const char *usrname)
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
        case SS$_NOGRPPRV:
        case SS$_NOSYSPRV:
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


static char *do_rmsexpand(char *, char *, int, char *, unsigned);
static char *do_fileify_dirspec(char *, char *, int);
static char *do_tovmsspec(char *, char *, int);

/*{{{int do_rmdir(char *name)*/
int
do_rmdir(char *name)
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
kill_file(char *name)
{
    char vmsname[NAM$C_MAXRSS+1], rspec[NAM$C_MAXRSS+1];
    unsigned long int jpicode = JPI$_UIC, type = ACL$C_FILE;
    unsigned long int cxt = 0, aclsts, fndsts, rmsts = -1;
    dTHX;
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
        case RMS$_FNF:
        case RMS$_DNF:
        case RMS$_DIR:
        case SS$_NOSUCHOBJECT:
          set_errno(ENOENT); break;
        case RMS$_DEV:
          set_errno(ENODEV); break;
        case RMS$_SYN:
        case SS$_INVFILFOROP:
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
my_mkdir(char *dir, Mode_t mode)
{
  STRLEN dirlen = strlen(dir);
  dTHX;

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


static void
create_mbx(unsigned short int *chan, struct dsc$descriptor_s *namdsc)
{
  static unsigned long int mbxbufsiz;
  long int syiitm = SYI$_MAXBUF, dviitm = DVI$_DEVNAM;
  dTHX;
  
  if (!mbxbufsiz) {
    /*
     * Get the SYSGEN parameter MAXBUF, and the smaller of it and the
     * preprocessor consant BUFSIZ from stdio.h as the size of the
     * 'pipe' mailbox.
     */
    _ckvmssts(lib$getsyi(&syiitm, &mbxbufsiz, 0, 0, 0, 0));
    if (mbxbufsiz > BUFSIZ) mbxbufsiz = BUFSIZ; 
  }
  _ckvmssts(sys$crembx(0,chan,mbxbufsiz,mbxbufsiz,0,0,0));

  _ckvmssts(lib$getdvi(&dviitm, chan, NULL, NULL, namdsc, &namdsc->dsc$w_length));
  namdsc->dsc$a_pointer[namdsc->dsc$w_length] = '\0';

}  /* end of create_mbx() */

/*{{{  my_popen and my_pclose*/
struct pipe_details
{
    struct pipe_details *next;
    PerlIO *fp;  /* stdio file pointer to pipe mailbox */
    int pid;   /* PID of subprocess */
    int mode;  /* == 'r' if pipe open for reading */
    int done;  /* subprocess has completed */
    unsigned long int completion;  /* termination status of subprocess */
};

struct exit_control_block
{
    struct exit_control_block *flink;
    unsigned long int	(*exit_routine)();
    unsigned long int arg_count;
    unsigned long int *status_address;
    unsigned long int exit_status;
}; 

static struct pipe_details *open_pipes = NULL;
static $DESCRIPTOR(nl_desc, "NL:");
static int waitpid_asleep = 0;

/* Send an EOF to a mbx.  N.B.  We don't check that fp actually points
 * to a mbx; that's the caller's responsibility.
 */
static unsigned long int
pipe_eof(FILE *fp, int immediate)
{
  char devnam[NAM$C_MAXRSS+1], *cp;
  unsigned long int chan, iosb[2], retsts, retsts2;
  struct dsc$descriptor devdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, devnam};
  dTHX;

  if (fgetname(fp,devnam,1)) {
    /* It oughta be a mailbox, so fgetname should give just the device
     * name, but just in case . . . */
    if ((cp = strrchr(devnam,':')) != NULL) *(cp+1) = '\0';
    devdsc.dsc$w_length = strlen(devnam);
    _ckvmssts(sys$assign(&devdsc,&chan,0,0));
    retsts = sys$qiow(0,chan,IO$_WRITEOF|(immediate?IO$M_NOW|IO$M_NORSWAIT:0),
             iosb,0,0,0,0,0,0,0,0);
    if (retsts & 1) retsts = iosb[0];
    retsts2 = sys$dassgn(chan);  /* Be sure to deassign the channel */
    if (retsts & 1) retsts = retsts2;
    _ckvmssts(retsts);
    return retsts;
  }
  else _ckvmssts(vaxc$errno);  /* Should never happen */
  return (unsigned long int) vaxc$errno;
}

static unsigned long int
pipe_exit_routine()
{
    struct pipe_details *info;
    unsigned long int retsts = SS$_NORMAL, abort = SS$_TIMEOUT;
    int sts, did_stuff;
    dTHX;

    /* 
     first we try sending an EOF...ignore if doesn't work, make sure we
     don't hang
    */
    did_stuff = 0;
    info = open_pipes;

    while (info) {
      int need_eof;
      _ckvmssts(sys$setast(0));
      need_eof = info->mode != 'r' && !info->done;
      _ckvmssts(sys$setast(1));
      if (need_eof) {
        if (pipe_eof(info->fp, 1) & 1) did_stuff = 1;
      }
      info = info->next;
    }
    if (did_stuff) sleep(1);   /* wait for EOF to have an effect */

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
    if (did_stuff) sleep(1);    /* wait for them to respond */

    info = open_pipes;
    while (info) {
      _ckvmssts(sys$setast(0));
      if (!info->done) {  /* We tried to be nice . . . */
        sts = sys$delprc(&info->pid,0);
        if (!(sts&1) && sts != SS$_NONEXPR) _ckvmssts(sts); 
        info->done = 1; /* so my_pclose doesn't try to write EOF */
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


static void
popen_completion_ast(struct pipe_details *thispipe)
{
  thispipe->done = TRUE;
  if (waitpid_asleep) {
    waitpid_asleep = 0;
    sys$wake(0,0);
  }
}

static unsigned long int setup_cmddsc(char *cmd, int check_img);
static void vms_execfree();

static PerlIO *
safe_popen(char *cmd, char *mode)
{
    static int handler_set_up = FALSE;
    char mbxname[64];
    unsigned short int chan;
    unsigned long int sts, flags=1;  /* nowait - gnu c doesn't allow &1 */
    dTHX;
    struct pipe_details *info;
    struct dsc$descriptor_s namdsc = {sizeof mbxname, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, mbxname},
                            cmddsc = {0, DSC$K_DTYPE_T,
                                      DSC$K_CLASS_S, 0};
                            

    if (!(setup_cmddsc(cmd,0) & 1)) { set_errno(EINVAL); return Nullfp; }
    New(1301,info,1,struct pipe_details);

    /* create mailbox */
    create_mbx(&chan,&namdsc);

    /* open a FILE* onto it */
    info->fp = PerlIO_open(mbxname, mode);

    /* give up other channel onto it */
    _ckvmssts(sys$dassgn(chan));

    if (!info->fp)
        return Nullfp;
        
    info->mode = *mode;
    info->done = FALSE;
    info->completion=0;
        
    if (*mode == 'r') {
      _ckvmssts(lib$spawn(&VMScmd, &nl_desc, &namdsc, &flags,
                     0  /* name */, &info->pid, &info->completion,
                     0, popen_completion_ast,info,0,0,0));
    }
    else {
      _ckvmssts(lib$spawn(&VMScmd, &namdsc, 0 /* sys$output */, &flags,
                     0  /* name */, &info->pid, &info->completion,
                     0, popen_completion_ast,info,0,0,0));
    }

    vms_execfree();
    if (!handler_set_up) {
      _ckvmssts(sys$dclexh(&pipe_exitblock));
      handler_set_up = TRUE;
    }
    info->next=open_pipes;  /* prepend to list */
    open_pipes=info;
        
    PL_forkprocess = info->pid;
    return info->fp;
}  /* end of safe_popen */


/*{{{  FILE *my_popen(char *cmd, char *mode)*/
FILE *
Perl_my_popen(pTHX_ char *cmd, char *mode)
{
    TAINT_ENV();
    TAINT_PROPER("popen");
    PERL_FLUSHALL_FOR_CHILD;
    return safe_popen(cmd,mode);
}

/*}}}*/

/*{{{  I32 my_pclose(FILE *fp)*/
I32 Perl_my_pclose(pTHX_ FILE *fp)
{
    struct pipe_details *info, *last = NULL;
    unsigned long int retsts;
    int need_eof;
    
    for (info = open_pipes; info != NULL; last = info, info = info->next)
        if (info->fp == fp) break;

    if (info == NULL) {  /* no such pipe open */
      set_errno(ECHILD); /* quoth POSIX */
      set_vaxc_errno(SS$_NONEXPR);
      return -1;
    }

    /* If we were writing to a subprocess, insure that someone reading from
     * the mailbox gets an EOF.  It looks like a simple fclose() doesn't
     * produce an EOF record in the mailbox.  */
    _ckvmssts(sys$setast(0));
    need_eof = info->mode != 'r' && !info->done;
    _ckvmssts(sys$setast(1));
    if (need_eof) pipe_eof(info->fp,0);
    PerlIO_close(info->fp);

    if (info->done) retsts = info->completion;
    else waitpid(info->pid,(int *) &retsts,0);

    /* remove from list of open pipes */
    _ckvmssts(sys$setast(0));
    if (last) last->next = info->next;
    else open_pipes = info->next;
    _ckvmssts(sys$setast(1));
    Safefree(info);

    return retsts;

}  /* end of my_pclose() */

/* sort-of waitpid; use only with popen() */
/*{{{Pid_t my_waitpid(Pid_t pid, int *statusp, int flags)*/
Pid_t
my_waitpid(Pid_t pid, int *statusp, int flags)
{
    struct pipe_details *info;
    dTHX;
    
    for (info = open_pipes; info != NULL; info = info->next)
        if (info->pid == pid) break;

    if (info != NULL) {  /* we know about this child */
      while (!info->done) {
        waitpid_asleep = 1;
        sys$hiber();
      }

      *statusp = info->completion;
      return pid;
    }
    else {  /* we haven't heard of this child */
      $DESCRIPTOR(intdsc,"0 00:00:01");
      unsigned long int ownercode = JPI$_OWNER, ownerpid, mypid;
      unsigned long int interval[2],sts;

      if (ckWARN(WARN_EXEC)) {
        _ckvmssts(lib$getjpi(&ownercode,&pid,0,&ownerpid,0,0));
        _ckvmssts(lib$getjpi(&ownercode,0,0,&mypid,0,0));
        if (ownerpid != mypid)
          Perl_warner(aTHX_ WARN_EXEC,"pid %x not a child",pid);
      }

      _ckvmssts(sys$bintim(&intdsc,interval));
      while ((sts=lib$getjpi(&ownercode,&pid,0,&ownerpid,0,0)) & 1) {
        _ckvmssts(sys$schdwk(0,0,interval,0));
        _ckvmssts(sys$hiber());
      }
      _ckvmssts(sts);

      /* There's no easy way to find the termination status a child we're
       * not aware of beforehand.  If we're really interested in the future,
       * we can go looking for a termination mailbox, or chase after the
       * accounting record for the process.
       */
      *statusp = 0;
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
static char *do_tounixspec(char *, char *, int);

static char *
do_rmsexpand(char *filespec, char *outbuf, int ts, char *defspec, unsigned opts)
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
    if (retsts == RMS$_DNF || retsts == RMS$_DIR ||
        retsts == RMS$_DEV || retsts == RMS$_DEV) {
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
char *rmsexpand(char *spec, char *buf, char *def, unsigned opt)
{ return do_rmsexpand(spec,buf,0,def,opt); }
char *rmsexpand_ts(char *spec, char *buf, char *def, unsigned opt)
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
static char *do_fileify_dirspec(char *dir,char *buf,int ts)
{
    static char __fileify_retbuf[NAM$C_MAXRSS+1];
    unsigned long int dirlen, retlen, addmfd = 0, hasfilename = 0;
    char *retspec, *cp1, *cp2, *lastdir;
    char trndir[NAM$C_MAXRSS+2], vmsdir[NAM$C_MAXRSS+1];

    if (!dir || !*dir) {
      set_errno(EINVAL); set_vaxc_errno(SS$_BADPARAM); return NULL;
    }
    dirlen = strlen(dir);
    while (dir[dirlen-1] == '/') --dirlen;
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
      while (!strpbrk(trndir,"/]>:>") && my_trnlnm(trndir,trndir,0)) ;
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
    if (!strcmp(dir+dirlen-2,".]")) {
      dir[--dirlen] = '\0';
      dir[dirlen-1] = ']';
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
      if (dir[dirlen-1] == '/') {    /* path ends with '/'; just add .dir;1 */
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
      else if (!strcmp(&dir[dirlen-7],"/000000")) {
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
char *fileify_dirspec(char *dir, char *buf)
{ return do_fileify_dirspec(dir,buf,0); }
char *fileify_dirspec_ts(char *dir, char *buf)
{ return do_fileify_dirspec(dir,buf,1); }

/*{{{ char *pathify_dirspec[_ts](char *path, char *buf)*/
static char *do_pathify_dirspec(char *dir,char *buf, int ts)
{
    static char __pathify_retbuf[NAM$C_MAXRSS+1];
    unsigned long int retlen;
    char *retpath, *cp1, *cp2, trndir[NAM$C_MAXRSS+1];

    if (!dir || !*dir) {
      set_errno(EINVAL); set_vaxc_errno(SS$_BADPARAM); return NULL;
    }

    if (*dir) strcpy(trndir,dir);
    else getcwd(trndir,sizeof trndir - 1);

    while (!strpbrk(trndir,"/]:>") && !no_translate_barewords
	   && my_trnlnm(trndir,trndir,0)) {
      STRLEN trnlen = strlen(trndir);

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
char *pathify_dirspec(char *dir, char *buf)
{ return do_pathify_dirspec(dir,buf,0); }
char *pathify_dirspec_ts(char *dir, char *buf)
{ return do_pathify_dirspec(dir,buf,1); }

/*{{{ char *tounixspec[_ts](char *path, char *buf)*/
static char *do_tounixspec(char *spec, char *buf, int ts)
{
  static char __tounixspec_retbuf[NAM$C_MAXRSS+1];
  char *dirend, *rslt, *cp1, *cp2, *cp3, tmp[NAM$C_MAXRSS+1];
  int devlen, dirlen, retlen = NAM$C_MAXRSS+1, expand = 0;

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
      do {
        cp3 = tmp;
        while (*cp3 != ':' && *cp3) cp3++;
        *(cp3++) = '\0';
        if (strchr(cp3,']') != NULL) break;
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
char *tounixspec(char *spec, char *buf) { return do_tounixspec(spec,buf,0); }
char *tounixspec_ts(char *spec, char *buf) { return do_tounixspec(spec,buf,1); }

/*{{{ char *tovmsspec[_ts](char *path, char *buf)*/
static char *do_tovmsspec(char *path, char *buf, int ts) {
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
char *tovmsspec(char *path, char *buf) { return do_tovmsspec(path,buf,0); }
char *tovmsspec_ts(char *path, char *buf) { return do_tovmsspec(path,buf,1); }

/*{{{ char *tovmspath[_ts](char *path, char *buf)*/
static char *do_tovmspath(char *path, char *buf, int ts) {
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
char *tovmspath(char *path, char *buf) { return do_tovmspath(path,buf,0); }
char *tovmspath_ts(char *path, char *buf) { return do_tovmspath(path,buf,1); }


/*{{{ char *tounixpath[_ts](char *path, char *buf)*/
static char *do_tounixpath(char *path, char *buf, int ts) {
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
char *tounixpath(char *path, char *buf) { return do_tounixpath(path,buf,0); }
char *tounixpath_ts(char *path, char *buf) { return do_tounixpath(path,buf,1); }

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

static void expand_wild_cards(char *item,
			      struct list_item **head,
			      struct list_item **tail,
			      int *count);

static int background_process(int argc, char **argv);

static void pipe_and_fork(char **cmargv);

/*{{{ void getredirection(int *ac, char ***av)*/
static void
getredirection(int *ac, char ***av)
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
	exit(background_process(--argc, argv));
    if (*ap && '&' == ap[strlen(ap)-1])
	{
	ap[strlen(ap)-1] = '\0';
	exit(background_process(argc, argv));
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
		PerlIO_printf(Perl_debug_log,"No input file after < on command line");
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
		PerlIO_printf(Perl_debug_log,"No output file after > on command line");
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
		PerlIO_printf(Perl_debug_log,"No output file after > or >> on command line");
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
		PerlIO_printf(Perl_debug_log,"No output file after 2> or 2>> on command line");
		exit(LIB$_WRONUMARG);
		}
	    continue;
	    }
	if (0 == strcmp("|", argv[j]))
	    {
	    if (j+1 >= argc)
		{
		PerlIO_printf(Perl_debug_log,"No command into which to pipe on command line");
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
	    PerlIO_printf(Perl_debug_log,"'|' and '>' may not both be specified on command line");
	    exit(LIB$_INVARGORD);
	    }
	pipe_and_fork(cmargv);
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

	PerlIO_getname(stdin, mbxname);
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
	    PerlIO_printf(Perl_debug_log,"Can't reopen input pipe (name: %s) in binary mode",mbxname);
	    exit(vaxc$errno);
	    }
	}
    if ((in != NULL) && (NULL == freopen(in, "r", stdin, "mbc=32", "mbf=2")))
	{
	PerlIO_printf(Perl_debug_log,"Can't open input file %s as stdin",in);
	exit(vaxc$errno);
	}
    if ((out != NULL) && (NULL == freopen(out, outmode, stdout, "mbc=32", "mbf=2")))
	{	
	PerlIO_printf(Perl_debug_log,"Can't open output file %s as stdout",out);
	exit(vaxc$errno);
	}
    if (err != NULL) {
        if (strcmp(err,"&1") == 0) {
            dup2(fileno(stdout), fileno(Perl_debug_log));
        } else {
	FILE *tmperr;
	if (NULL == (tmperr = fopen(err, errmode, "mbc=32", "mbf=2")))
	    {
	    PerlIO_printf(Perl_debug_log,"Can't open error file %s as stderr",err);
	    exit(vaxc$errno);
	    }
	    fclose(tmperr);
	    if (NULL == freopen(err, "a", Perl_debug_log, "mbc=32", "mbf=2"))
		{
		exit(vaxc$errno);
		}
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

static void expand_wild_cards(char *item,
			      struct list_item **head,
			      struct list_item **tail,
			      int *count)
{
int expcount = 0;
unsigned long int context = 0;
int isunix = 0;
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
	    case RMS$_FNF:
	    case RMS$_DNF:
	    case RMS$_DIR:
		set_errno(ENOENT); break;
	    case RMS$_DEV:
		set_errno(ENODEV); break;
	    case RMS$_FNM:
	    case RMS$_SYN:
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

static void pipe_and_fork(char **cmargv)
{
    char subcmd[2048];
    $DESCRIPTOR(cmddsc, "");
    static char mbxname[64];
    $DESCRIPTOR(mbxdsc, mbxname);
    int pid, j;
    unsigned long int zero = 0, one = 1;

    strcpy(subcmd, cmargv[0]);
    for (j = 1; NULL != cmargv[j]; ++j)
	{
	strcat(subcmd, " \"");
	strcat(subcmd, cmargv[j]);
	strcat(subcmd, "\"");
	}
    cmddsc.dsc$a_pointer = subcmd;
    cmddsc.dsc$w_length = strlen(cmddsc.dsc$a_pointer);

	create_mbx(&child_chan,&mbxdsc);
#ifdef ARGPROC_DEBUG
    PerlIO_printf(Perl_debug_log, "Pipe Mailbox Name = '%s'\n", mbxdsc.dsc$a_pointer);
    PerlIO_printf(Perl_debug_log, "Sub Process Command = '%s'\n", cmddsc.dsc$a_pointer);
#endif
    _ckvmssts_noperl(lib$spawn(&cmddsc, &mbxdsc, 0, &one,
                               0, &pid, child_st, &zero, sig_child,
                               &child_chan));
#ifdef ARGPROC_DEBUG
    PerlIO_printf(Perl_debug_log, "Subprocess's Pid = %08X\n", pid);
#endif
    sys$dclexh(&exit_block);
    if (NULL == freopen(mbxname, "wb", stdout))
	{
	PerlIO_printf(Perl_debug_log,"Can't open output pipe (name %s)",mbxname);
	}
}

static int background_process(int argc, char **argv)
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
  dTHX;
  struct itmlst_3 jpilist[4] = { {sizeof iprv,    JPI$_IMAGPRIV, iprv, &dummy},
                                 {sizeof rlst,  JPI$_RIGHTSLIST, rlst,  &rlen},
                                 { sizeof rsz, JPI$_RIGHTS_SIZE, &rsz, &dummy},
                                 {          0,                0,    0,      0} };

  _ckvmssts(sys$getjpiw(0,NULL,NULL,jpilist,iosb,NULL,NULL));
  _ckvmssts(iosb[0]);
  for (i = 0; i < sizeof iprv / sizeof(unsigned long int); i++) {
    if (iprv[i]) {           /* Running image installed with privs? */
      _ckvmssts(sys$setprv(0,iprv,0,NULL));       /* Turn 'em off. */
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
       */
      if (jpilist[1].bufadr != rlst) Safefree(jpilist[1].bufadr);
      jpilist[1].bufadr = New(1320,mask,rsz,unsigned long int);
      jpilist[1].buflen = rsz * sizeof(unsigned long int);
      _ckvmssts(sys$getjpiw(0,NULL,NULL,&jpilist[1],iosb,NULL,NULL));
      _ckvmssts(iosb[0]);
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
    char ***newap;
    New(1320,newap,*argcp+2,char **);
    newap[0] = argvp[0];
    *newap[1] = "-T";
    Copy(argvp[1],newap[2],*argcp-1,char **);
    /* We orphan the old argv, since we don't know where it's come from,
     * so we don't know how to free it.
     */
    *argcp++; argvp = newap;
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
    _ckvmssts(lib$scopy_r_dx(&len,eqv,tabvec[tabidx]));
  }
  if (tabidx) { tabvec[tabidx] = NULL; env_tables = tabvec; }

  getredirection(argcp,argvp);
#if defined(USE_THREADS) && defined(__DECC)
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
trim_unixpath(char *fspec, char *wildspec, int opts)
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

    /* Number of elements in vms_versions array */
#define VERSIZE(e)	(sizeof e->vms_versions / sizeof e->vms_versions[0])

/*
 *  Open a directory, return a handle for later use.
 */
/*{{{ DIR *opendir(char*name) */
DIR *
opendir(char *name)
{
    DIR *dd;
    char dir[NAM$C_MAXRSS+1];
    Stat_t sb;

    if (do_tovmspath(name,dir,0) == NULL) {
      return NULL;
    }
    if (flex_stat(dir,&sb) == -1) return NULL;
    if (!S_ISDIR(sb.st_mode)) {
      set_errno(ENOTDIR);  set_vaxc_errno(RMS$_DIR);
      return NULL;
    }
    if (!cando_by_name(S_IRUSR,0,dir)) {
      set_errno(EACCES); set_vaxc_errno(RMS$_PRV);
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
    Safefree((char *)dd);
}
/*}}}*/

/*
 *  Collect all the version numbers for the current file.
 */
static void
collectversions(dd)
    DIR *dd;
{
    struct dsc$descriptor_s	pat;
    struct dsc$descriptor_s	res;
    struct dirent *e;
    char *p, *text, buff[sizeof dd->entry.d_name];
    int i;
    unsigned long context, tmpsts;
    dTHX;

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
readdir(DIR *dd)
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
        case RMS$_FNF:
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
    if (dd->vms_wantversions) collectversions(dd);
    return &dd->entry;

}  /* end of readdir() */
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
seekdir(DIR *dd, long count)
{
    int vms_wantversions;
    dTHX;

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
 * call out the the regular perl routines in doio.c which do an
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
 * the the command string is handed off to DCL directly.  Otherwise,
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
vms_execfree() {
  if (PL_Cmd) {
    if (PL_Cmd != VMScmd.dsc$a_pointer) Safefree(PL_Cmd);
    PL_Cmd = Nullch;
  }
  if (VMScmd.dsc$a_pointer) {
    Safefree(VMScmd.dsc$a_pointer);
    VMScmd.dsc$w_length = 0;
    VMScmd.dsc$a_pointer = Nullch;
  }
}

static char *
setup_argstr(SV *really, SV **mark, SV **sp)
{
  dTHX;
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
setup_cmddsc(char *cmd, int check_img)
{
  char vmsspec[NAM$C_MAXRSS+1], resspec[NAM$C_MAXRSS+1];
  $DESCRIPTOR(defdsc,".EXE");
  $DESCRIPTOR(defdsc2,".");
  $DESCRIPTOR(resdsc,resspec);
  struct dsc$descriptor_s imgdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
  unsigned long int cxt = 0, flags = 1, retsts = SS$_NORMAL;
  register char *s, *rest, *cp, *wordbreak;
  register int isdcl;
  dTHX;

  if (strlen(cmd) >
      (sizeof(vmsspec) > sizeof(resspec) ? sizeof(resspec) : sizeof(vmsspec)))
    return LIB$_INVARG;
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
  if (*s == '@') isdcl = 1;
  else {
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
        New(402,VMScmd.dsc$a_pointer,7 + s - resspec + (rest ? strlen(rest) : 0),char);
        if (!isdcl) {
        strcpy(VMScmd.dsc$a_pointer,"$ MCR ");
        } else {
            strcpy(VMScmd.dsc$a_pointer,"@");
        }
        strcat(VMScmd.dsc$a_pointer,resspec);
        if (rest) strcat(VMScmd.dsc$a_pointer,rest);
        VMScmd.dsc$w_length = strlen(VMScmd.dsc$a_pointer);
        return retsts;
      }
      else retsts = RMS$_PRV;
    }
  }
  /* It's either a DCL command or we couldn't find a suitable image */
  VMScmd.dsc$w_length = strlen(cmd);
  if (cmd == PL_Cmd) VMScmd.dsc$a_pointer = PL_Cmd;
  else VMScmd.dsc$a_pointer = savepvn(cmd,VMScmd.dsc$w_length);
  if (!(retsts & 1)) {
    /* just hand off status values likely to be due to user error */
    if (retsts == RMS$_FNF || retsts == RMS$_DNF || retsts == RMS$_PRV ||
        retsts == RMS$_DEV || retsts == RMS$_DIR || retsts == RMS$_SYN ||
       (retsts & STS$M_CODE) == (SHR$_NOWILD & STS$M_CODE)) return retsts;
    else { _ckvmssts(retsts); }
  }

  return (VMScmd.dsc$w_length > 255 ? CLI$_BUFOVF : retsts);

}  /* end of setup_cmddsc() */


/* {{{ bool vms_do_aexec(SV *really,SV **mark,SV **sp) */
bool
vms_do_aexec(SV *really,SV **mark,SV **sp)
{
  dTHX;
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
    return vms_do_exec(setup_argstr(really,mark,sp));

  }

  return FALSE;
}  /* end of vms_do_aexec() */
/*}}}*/

/* {{{bool vms_do_exec(char *cmd) */
bool
vms_do_exec(char *cmd)
{

  dTHX;
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
    if ((retsts = setup_cmddsc(cmd,1)) & 1)
      retsts = lib$do_command(&VMScmd);

    switch (retsts) {
      case RMS$_FNF:
        set_errno(ENOENT); break;
      case RMS$_DNF: case RMS$_DIR: case RMS$_DEV:
        set_errno(ENOTDIR); break;
      case RMS$_PRV:
        set_errno(EACCES); break;
      case RMS$_SYN:
        set_errno(EINVAL); break;
      case CLI$_BUFOVF:
        set_errno(E2BIG); break;
      case LIB$_INVARG: case LIB$_INVSTRDES: case SS$_ACCVIO: /* shouldn't happen */
        _ckvmssts(retsts); /* fall through */
      default:  /* SS$_DUPLNAM, SS$_CLI, resource exhaustion, etc. */
        set_errno(EVMSERR); 
    }
    set_vaxc_errno(retsts);
    if (ckWARN(WARN_EXEC)) {
      Perl_warner(aTHX_ WARN_EXEC,"Can't exec \"%*s\": %s",
             VMScmd.dsc$w_length, VMScmd.dsc$a_pointer, Strerror(errno));
    }
    vms_execfree();
  }

  return FALSE;

}  /* end of vms_do_exec() */
/*}}}*/

unsigned long int do_spawn(char *);

/* {{{ unsigned long int do_aspawn(void *really,void **mark,void **sp) */
unsigned long int
do_aspawn(void *really,void **mark,void **sp)
{
  dTHX;
  if (sp > mark) return do_spawn(setup_argstr((SV *)really,(SV **)mark,(SV **)sp));

  return SS$_ABORT;
}  /* end of do_aspawn() */
/*}}}*/

/* {{{unsigned long int do_spawn(char *cmd) */
unsigned long int
do_spawn(char *cmd)
{
  unsigned long int sts, substs, hadcmd = 1;
  dTHX;

  TAINT_ENV();
  TAINT_PROPER("spawn");
  if (!cmd || !*cmd) {
    hadcmd = 0;
    sts = lib$spawn(0,0,0,0,0,0,&substs,0,0,0,0,0,0);
  }
  else if ((sts = setup_cmddsc(cmd,0)) & 1) {
    sts = lib$spawn(&VMScmd,0,0,0,0,0,&substs,0,0,0,0,0,0);
  }
  
  if (!(sts & 1)) {
    switch (sts) {
      case RMS$_FNF:
        set_errno(ENOENT); break;
      case RMS$_DNF: case RMS$_DIR: case RMS$_DEV:
        set_errno(ENOTDIR); break;
      case RMS$_PRV:
        set_errno(EACCES); break;
      case RMS$_SYN:
        set_errno(EINVAL); break;
      case CLI$_BUFOVF:
        set_errno(E2BIG); break;
      case LIB$_INVARG: case LIB$_INVSTRDES: case SS$_ACCVIO: /* shouldn't happen */
        _ckvmssts(sts); /* fall through */
      default:  /* SS$_DUPLNAM, SS$_CLI, resource exhaustion, etc. */
        set_errno(EVMSERR); 
    }
    set_vaxc_errno(sts);
    if (ckWARN(WARN_EXEC)) {
      Perl_warner(aTHX_ WARN_EXEC,"Can't spawn \"%*s\": %s",
             hadcmd ? VMScmd.dsc$w_length :  0,
             hadcmd ? VMScmd.dsc$a_pointer : "",
             Strerror(errno));
    }
  }
  vms_execfree();
  return substs;

}  /* end of do_spawn() */
/*}}}*/

/* 
 * A simple fwrite replacement which outputs itmsz*nitm chars without
 * introducing record boundaries every itmsz chars.
 */
/*{{{ int my_fwrite(void *src, size_t itmsz, size_t nitm, FILE *dest)*/
int
my_fwrite(void *src, size_t itmsz, size_t nitm, FILE *dest)
{
  register char *cp, *end;

  end = (char *)src + itmsz * nitm;

  while ((char *)src <= end) {
    for (cp = src; cp <= end; cp++) if (!*cp) break;
    if (fputs(src,dest) == EOF) return EOF;
    if (cp < end)
      if (fputc('\0',dest) == EOF) return EOF;
    src = cp + 1;
  }

  return 1;

}  /* end of my_fwrite() */
/*}}}*/

/*{{{ int my_flush(FILE *fp)*/
int
my_flush(FILE *fp)
{
    int res;
    if ((res = fflush(fp)) == 0 && fp) {
#ifdef VMS_DO_SOCKETS
	Stat_t s;
	if (Fstat(fileno(fp), &s) == 0 && !S_ISSOCK(s.st_mode))
#endif
	    res = fsync(fileno(fp));
    }
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
static int fillpasswd (const char *name, struct passwd *pwd)
{
    dTHX;
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
struct passwd *my_getpwnam(char *name)
{
    struct dsc$descriptor_s name_desc;
    union uicdef uic;
    unsigned long int status, sts;
    dTHX;
                                  
    __pwdcache = __passwd_empty;
    if (!fillpasswd(name, &__pwdcache)) {
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
struct passwd *my_getpwuid(Uid_t uid)
{
    const $DESCRIPTOR(name_desc,__pw_namecache);
    unsigned short lname;
    union uicdef uic;
    unsigned long int status;
    dTHX;

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

    fillpasswd(__pw_namecache, &__pwdcache);
    return &__pwdcache;

}  /* end of my_getpwuid() */
/*}}}*/

/*
 * Get information for next user.
*/
/*{{{struct passwd *my_getpwent()*/
struct passwd *my_getpwent()
{
    return (my_getpwuid((unsigned int) -1));
}
/*}}}*/

/*
 * Finish searching rights database for users.
*/
/*{{{void my_endpwent()*/
void my_endpwent()
{
    dTHX;
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
    *set & (1 << (sig - 1));
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

#if defined(__VMS_VER) && __VMS_VER >= 70000000 && __DECC_VER >= 50200000
#  define RTL_USES_UTC 1
#endif

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
time_t my_time(time_t *timep)
{
  dTHX;
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
my_gmtime(const time_t *timep)
{
  dTHX;
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
my_localtime(const time_t *timep)
{
  dTHX;
  time_t when;
  struct tm *rsltmp;

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
# else
# ifdef VMSISH_TIME
  if (!VMSISH_TIME) when = _toloc(when);   /*  Input was UTC */
# endif
# endif
  /* CRTL localtime() wants local time as input, so does no tz correction */
  rsltmp = localtime(&when);
  if (rsltmp && gmtime_emulation_type != 1) rsltmp->tm_isdst = -1;
  return rsltmp;

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
int my_utime(char *file, struct utimbuf *utimes)
{
  dTHX;
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
#ifdef __DECC
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
static mydev_t encode_dev (const char *dev)
{
  int i;
  unsigned long int f;
  mydev_t enc;
  char c;
  const char *q;
  dTHX;

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
    dTHX;
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
      return cando_by_name(bit,effective,fname);
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
cando_by_name(I32 bit, Uid_t effective, char *fname)
{
  static char usrname[L_cuserid];
  static struct dsc$descriptor_s usrdsc =
         {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, usrname};
  char vmsname[NAM$C_MAXRSS+1], fileified[NAM$C_MAXRSS+1];
  unsigned long int objtyp = ACL$C_FILE, access, retsts, privused, iosb[2];
  unsigned short int retlen;
  dTHX;
  struct dsc$descriptor_s namdsc = {0, DSC$K_DTYPE_T, DSC$K_CLASS_S, 0};
  union prvdef curprv;
  struct itmlst_3 armlst[3] = {{sizeof access, CHP$_ACCESS, &access, &retlen},
         {sizeof privused, CHP$_PRIVUSED, &privused, &retlen},{0,0,0,0}};
  struct itmlst_3 jpilst[2] = {{sizeof curprv, JPI$_CURPRIV, &curprv, &retlen},
         {0,0,0,0}};

  if (!fname || !*fname) return FALSE;
  /* Make sure we expand logical names, since sys$check_access doesn't */
  if (!strpbrk(fname,"/]>:")) {
    strcpy(fileified,fname);
    while (!strpbrk(fileified,"/]>:>") && my_trnlnm(fileified,fileified,0)) ;
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

  if (!usrdsc.dsc$w_length) {
    cuserid(usrname);
    usrdsc.dsc$w_length = strlen(usrname);
  }

  switch (bit) {
    case S_IXUSR:
    case S_IXGRP:
    case S_IXOTH:
      access = ARM$M_EXECUTE;
      break;
    case S_IRUSR:
    case S_IRGRP:
    case S_IROTH:
      access = ARM$M_READ;
      break;
    case S_IWUSR:
    case S_IWGRP:
    case S_IWOTH:
      access = ARM$M_WRITE;
      break;
    case S_IDUSR:
    case S_IDGRP:
    case S_IDOTH:
      access = ARM$M_DELETE;
      break;
    default:
      return FALSE;
  }

  retsts = sys$check_access(&objtyp,&namdsc,&usrdsc,armlst);
  if (retsts == SS$_NOPRIV      || retsts == SS$_NOSUCHOBJECT ||
      retsts == SS$_INVFILFOROP || retsts == RMS$_FNF || retsts == RMS$_SYN ||
      retsts == RMS$_DIR        || retsts == RMS$_DEV) {
    set_vaxc_errno(retsts);
    if (retsts == SS$_NOPRIV) set_errno(EACCES);
    else if (retsts == SS$_INVFILFOROP) set_errno(EINVAL);
    else set_errno(ENOENT);
    return FALSE;
  }
  if (retsts == SS$_NORMAL) {
    if (!privused) return TRUE;
    /* We can get access, but only by using privs.  Do we have the
       necessary privs currently enabled? */
    _ckvmssts(sys$getjpiw(0,0,0,jpilst,iosb,0,0));
    if ((privused & CHP$M_BYPASS) &&  !curprv.prv$v_bypass)  return FALSE;
    if ((privused & CHP$M_SYSPRV) &&  !curprv.prv$v_sysprv &&
                                      !curprv.prv$v_bypass)  return FALSE;
    if ((privused & CHP$M_GRPPRV) &&  !curprv.prv$v_grpprv &&
         !curprv.prv$v_sysprv &&      !curprv.prv$v_bypass)  return FALSE;
    if ((privused & CHP$M_READALL) && !curprv.prv$v_readall) return FALSE;
    return TRUE;
  }
  if (retsts == SS$_ACCONFLICT) {
    return TRUE;
  }
  _ckvmssts(retsts);

  return FALSE;  /* Should never get here */

}  /* end of cando_by_name() */
/*}}}*/


/*{{{ int flex_fstat(int fd, Stat_t *statbuf)*/
int
flex_fstat(int fd, Stat_t *statbufp)
{
  dTHX;
  if (!fstat(fd,(stat_t *) statbufp)) {
    if (statbufp == (Stat_t *) &PL_statcache) *namecache == '\0';
    statbufp->st_dev = encode_dev(statbufp->st_devnam);
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
flex_stat(const char *fspec, Stat_t *statbufp)
{
    dTHX;
    char fileified[NAM$C_MAXRSS+1];
    char temp_fspec[NAM$C_MAXRSS+300];
    int retval = -1;

    strcpy(temp_fspec, fspec);
    if (statbufp == (Stat_t *) &PL_statcache)
      do_tovmsspec(temp_fspec,namecache,0);
    if (is_null_device(temp_fspec)) { /* Fake a stat() for the null device */
      memset(statbufp,0,sizeof *statbufp);
      statbufp->st_dev = encode_dev("_NLA0:");
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
      statbufp->st_dev = encode_dev(statbufp->st_devnam);
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
rmscopy(char *spec_in, char *spec_out, int preserve_dates)
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
        case RMS$_FNF:
        case RMS$_DIR:
          set_errno(ENOENT); break;
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
        case RMS$_DIR:
          set_errno(ENOENT); break;
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
    if (!(io = GvIOp(mysv)) || !fgetname(IoIFP(io),fspec,1)) {
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
    if (!(io = GvIOp(mysv)) || !fgetname(IoIFP(io),inspec,1)) {
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
    if (!(io = GvIOp(mysv)) || !fgetname(IoIFP(io),outspec,1)) {
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
init_os_extras()
{
  char* file = __FILE__;
  dTHX;
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
  newXS("File::Copy::rmscopy",rmscopy_fromperl,file);

  return;
}
  
/*  End of vms.c */
