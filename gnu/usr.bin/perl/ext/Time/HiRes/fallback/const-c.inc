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
#ifdef HAS_NANOSLEEP
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

#!/usr/local/bin/perl5.8.0 -w
use ExtUtils::Constant qw (constant_types C_constant XS_constant);

my $types = {map {($_, 1)} qw(IV)};
my @names = (qw(ITIMER_PROF ITIMER_REAL ITIMER_REALPROF ITIMER_VIRTUAL),
            {name=>"d_getitimer", type=>"IV", macro=>"HAS_GETITIMER", value=>"1", default=>["IV", "0"]},
            {name=>"d_gettimeofday", type=>"IV", macro=>"HAS_GETTIMEOFDAY", value=>"1", default=>["IV", "0"]},
            {name=>"d_nanosleep", type=>"IV", macro=>"HAS_NANOSLEEP", value=>"1", default=>["IV", "0"]},
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
  case 8:
    /* Names all of length 8.  */
    /* d_ualarm d_usleep */
    /* Offset 7 gives the best switch position.  */
    switch (name[7]) {
    case 'm':
      if (memEQ(name, "d_ualarm", 8)) {
      /*                      ^      */
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
      if (memEQ(name, "d_usleep", 8)) {
      /*                      ^      */
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
  case 14:
    /* Names all of length 14.  */
    /* ITIMER_VIRTUAL d_gettimeofday */
    /* Offset 6 gives the best switch position.  */
    switch (name[6]) {
    case '_':
      if (memEQ(name, "ITIMER_VIRTUAL", 14)) {
      /*                     ^              */
#ifdef ITIMER_VIRTUAL
        *iv_return = ITIMER_VIRTUAL;
        return PERL_constant_ISIV;
#else
        return PERL_constant_NOTDEF;
#endif
      }
      break;
    case 'i':
      if (memEQ(name, "d_gettimeofday", 14)) {
      /*                     ^              */
#ifdef HAS_GETTIMEOFDAY
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
  case 15:
    if (memEQ(name, "ITIMER_REALPROF", 15)) {
#ifdef ITIMER_REALPROF
      *iv_return = ITIMER_REALPROF;
      return PERL_constant_ISIV;
#else
      return PERL_constant_NOTDEF;
#endif
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

