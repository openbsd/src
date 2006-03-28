#define PERL_constant_NOTFOUND	1
#define PERL_constant_NOTDEF	2
#define PERL_constant_ISIV	3
#define PERL_constant_ISNO	4
#define PERL_constant_ISNV	5
#define PERL_constant_ISPV	6
#define PERL_constant_ISPVN	7
#define PERL_constant_ISSV	8
#define PERL_constant_ISUNDEF	9
#define PERL_constant_ISUV	10
#define PERL_constant_ISYES	11

#ifndef NVTYPE
typedef double NV; /* 5.6 and later define NVTYPE, and typedef NV to it.  */
#endif
#ifndef aTHX_
#define aTHX_ /* 5.6 or later define this for threading support.  */
#endif
#ifndef pTHX_
#define pTHX_ /* 5.6 or later define this for threading support.  */
#endif

static int
constant_11 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     ITIMER_PROF ITIMER_REAL d_getitimer d_nanosleep d_setitimer */
  /* Offset 7 gives the best switch position.  */
  switch (name[7]) {
  case 'P':
    if (memEQ(name, "ITIMER_PROF", 11)) {
    /*                      ^          */
#ifdef ITIMER_PROF
      *iv_return = ITIMER_PROF;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'R':
    if (memEQ(name, "ITIMER_REAL", 11)) {
    /*                      ^          */
#ifdef ITIMER_REAL
      *iv_return = ITIMER_REAL;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'i':
    if (memEQ(name, "d_getitimer", 11)) {
    /*                      ^          */
#ifdef HAS_GETITIMER
      *iv_return = 1;
      return PERL_constant_ISIV;
#else
      *iv_return = 0;
      return PERL_constant_ISIV;
#endif
    }
    if (memEQ(name, "d_setitimer", 11)) {
    /*                      ^          */
#ifdef HAS_SETITIMER
      *iv_return = 1;
      return PERL_constant_ISIV;
#else
      *iv_return = 0;
      return PERL_constant_ISIV;
#endif
    }
    break;
  case 'l':
    if (memEQ(name, "d_nanosleep", 11)) {
    /*                      ^          */
#ifdef TIME_HIRES_NANOSLEEP
      *iv_return = 1;
      return PERL_constant_ISIV;
#else
      *iv_return = 0;
      return PERL_constant_ISIV;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_14 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     CLOCKS_PER_SEC CLOCK_REALTIME CLOCK_SOFTTIME ITIMER_VIRTUAL d_clock_getres
     d_gettimeofday */
  /* Offset 8 gives the best switch position.  */
  switch (name[8]) {
  case 'A':
    if (memEQ(name, "CLOCK_REALTIME", 14)) {
    /*                       ^            */
#ifdef CLOCK_REALTIME
      *iv_return = CLOCK_REALTIME;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'E':
    if (memEQ(name, "CLOCKS_PER_SEC", 14)) {
    /*                       ^            */
#ifdef CLOCKS_PER_SEC
      *iv_return = CLOCKS_PER_SEC;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'F':
    if (memEQ(name, "CLOCK_SOFTTIME", 14)) {
    /*                       ^            */
#ifdef CLOCK_SOFTTIME
      *iv_return = CLOCK_SOFTTIME;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'I':
    if (memEQ(name, "ITIMER_VIRTUAL", 14)) {
    /*                       ^            */
#ifdef ITIMER_VIRTUAL
      *iv_return = ITIMER_VIRTUAL;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'e':
    if (memEQ(name, "d_gettimeofday", 14)) {
    /*                       ^            */
#ifdef HAS_GETTIMEOFDAY
      *iv_return = 1;
      return PERL_constant_ISIV;
#else
      *iv_return = 0;
      return PERL_constant_ISIV;
#endif
    }
    break;
  case 'g':
    if (memEQ(name, "d_clock_getres", 14)) {
    /*                       ^            */
#ifdef TIME_HIRES_CLOCK_GETRES
      *iv_return = 1;
      return PERL_constant_ISIV;
#else
      *iv_return = 0;
      return PERL_constant_ISIV;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant_15 (pTHX_ const char *name, IV *iv_return) {
  /* When generated this function returned values for the list of names given
     here.  However, subsequent manual editing may have added or removed some.
     CLOCK_MONOTONIC CLOCK_TIMEOFDAY ITIMER_REALPROF d_clock_gettime */
  /* Offset 7 gives the best switch position.  */
  switch (name[7]) {
  case 'I':
    if (memEQ(name, "CLOCK_TIMEOFDAY", 15)) {
    /*                      ^              */
#ifdef CLOCK_TIMEOFDAY
      *iv_return = CLOCK_TIMEOFDAY;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'O':
    if (memEQ(name, "CLOCK_MONOTONIC", 15)) {
    /*                      ^              */
#ifdef CLOCK_MONOTONIC
      *iv_return = CLOCK_MONOTONIC;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 'R':
    if (memEQ(name, "ITIMER_REALPROF", 15)) {
    /*                      ^              */
#ifdef ITIMER_REALPROF
      *iv_return = ITIMER_REALPROF;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case '_':
    if (memEQ(name, "d_clock_gettime", 15)) {
    /*                      ^              */
#ifdef TIME_HIRES_CLOCK_GETTIME
      *iv_return = 1;
      return PERL_constant_ISIV;
#else
      *iv_return = 0;
      return PERL_constant_ISIV;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

static int
constant (pTHX_ const char *name, STRLEN len, IV *iv_return) {
  /* Initially switch on the length of the name.  */
  /* When generated this function returned values for the list of names given
     in this section of perl code.  Rather than manually editing these functions
     to add or remove constants, which would result in this comment and section
     of code becoming inaccurate, we recommend that you edit this section of
     code, and use it to regenerate a new set of constant functions which you
     then use to replace the originals.

     Regenerate these constant functions by feeding this entire source file to
     perl -x

#!perl -w
use ExtUtils::Constant qw (constant_types C_constant XS_constant);

my $types = {map {($_, 1)} qw(IV)};
my @names = (qw(CLOCKS_PER_SEC CLOCK_HIGHRES CLOCK_MONOTONIC
	       CLOCK_PROCESS_CPUTIME_ID CLOCK_REALTIME CLOCK_SOFTTIME
	       CLOCK_THREAD_CPUTIME_ID CLOCK_TIMEOFDAY ITIMER_PROF ITIMER_REAL
	       ITIMER_REALPROF ITIMER_VIRTUAL TIMER_ABSTIME),
            {name=>"d_clock", type=>"IV", macro=>"TIME_HIRES_CLOCK", value=>"1", default=>["IV", "0"]},
            {name=>"d_clock_getres", type=>"IV", macro=>"TIME_HIRES_CLOCK_GETRES", value=>"1", default=>["IV", "0"]},
            {name=>"d_clock_gettime", type=>"IV", macro=>"TIME_HIRES_CLOCK_GETTIME", value=>"1", default=>["IV", "0"]},
            {name=>"d_clock_nanosleep", type=>"IV", macro=>"TIME_HIRES_CLOCK_NANOSLEEP", value=>"1", default=>["IV", "0"]},
            {name=>"d_getitimer", type=>"IV", macro=>"HAS_GETITIMER", value=>"1", default=>["IV", "0"]},
            {name=>"d_gettimeofday", type=>"IV", macro=>"HAS_GETTIMEOFDAY", value=>"1", default=>["IV", "0"]},
            {name=>"d_nanosleep", type=>"IV", macro=>"TIME_HIRES_NANOSLEEP", value=>"1", default=>["IV", "0"]},
            {name=>"d_setitimer", type=>"IV", macro=>"HAS_SETITIMER", value=>"1", default=>["IV", "0"]},
            {name=>"d_ualarm", type=>"IV", macro=>"HAS_UALARM", value=>"1", default=>["IV", "0"]},
            {name=>"d_usleep", type=>"IV", macro=>"HAS_USLEEP", value=>"1", default=>["IV", "0"]});

print constant_types(); # macro defs
foreach (C_constant ("Time::HiRes", 'constant', 'IV', $types, undef, 3, @names) ) {
    print $_, "\n"; # C constant subs
}
print "#### XS Section:\n";
print XS_constant ("Time::HiRes", $types);
__END__
   */

  switch (len) {
  case 7:
    if (memEQ(name, "d_clock", 7)) {
#ifdef TIME_HIRES_CLOCK
      *iv_return = 1;
      return PERL_constant_ISIV;
#else
      *iv_return = 0;
      return PERL_constant_ISIV;
#endif
    }
    break;
  case 8:
    /* Names all of length 8.  */
    /* d_ualarm d_usleep */
    /* Offset 7 gives the best switch position.  */
    switch (name[7]) {
    case 'm':
      if (memEQ(name, "d_ualar", 7)) {
      /*                      m     */
#ifdef HAS_UALARM
        *iv_return = 1;
        return PERL_constant_ISIV;
#else
        *iv_return = 0;
        return PERL_constant_ISIV;
#endif
      }
      break;
    case 'p':
      if (memEQ(name, "d_uslee", 7)) {
      /*                      p     */
#ifdef HAS_USLEEP
        *iv_return = 1;
        return PERL_constant_ISIV;
#else
        *iv_return = 0;
        return PERL_constant_ISIV;
#endif
      }
      break;
    }
    break;
  case 11:
    return constant_11 (aTHX_ name, iv_return);
    break;
  case 13:
    /* Names all of length 13.  */
    /* CLOCK_HIGHRES TIMER_ABSTIME */
    /* Offset 2 gives the best switch position.  */
    switch (name[2]) {
    case 'M':
      if (memEQ(name, "TIMER_ABSTIME", 13)) {
      /*                 ^                 */
#ifdef TIMER_ABSTIME
        *iv_return = TIMER_ABSTIME;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    case 'O':
      if (memEQ(name, "CLOCK_HIGHRES", 13)) {
      /*                 ^                 */
#ifdef CLOCK_HIGHRES
        *iv_return = CLOCK_HIGHRES;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    }
    break;
  case 14:
    return constant_14 (aTHX_ name, iv_return);
    break;
  case 15:
    return constant_15 (aTHX_ name, iv_return);
    break;
  case 17:
    if (memEQ(name, "d_clock_nanosleep", 17)) {
#ifdef TIME_HIRES_CLOCK_NANOSLEEP
      *iv_return = 1;
      return PERL_constant_ISIV;
#else
      *iv_return = 0;
      return PERL_constant_ISIV;
#endif
    }
    break;
  case 23:
    if (memEQ(name, "CLOCK_THREAD_CPUTIME_ID", 23)) {
#ifdef CLOCK_THREAD_CPUTIME_ID
      *iv_return = CLOCK_THREAD_CPUTIME_ID;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  case 24:
    if (memEQ(name, "CLOCK_PROCESS_CPUTIME_ID", 24)) {
#ifdef CLOCK_PROCESS_CPUTIME_ID
      *iv_return = CLOCK_PROCESS_CPUTIME_ID;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

