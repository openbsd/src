#!/pro/bin/perl

use strict;
use warnings;

BEGIN {
    use Test::More;
    my $tests = 137;
    unless ($ENV{PERL_CORE}) {
	require Test::NoWarnings;
	Test::NoWarnings->import ();
	$tests++;
	}

    plan tests => $tests;
    }

use Config::Perl::V qw( summary );

ok (my $conf = Config::Perl::V::plv2hash (<DATA>), "Read perl -v block");
ok (exists $conf->{$_}, "Has $_ entry") for qw( build environment config inc );

is ($conf->{build}{osname}, $conf->{config}{osname}, "osname");
is ($conf->{build}{stamp}, "Jun 10 2024 10:14:20", "Build time");
is ($conf->{config}{version}, "5.40.0", "reconstructed \$Config{version}");

my $opt = Config::Perl::V::plv2hash ("")->{build}{options};
foreach my $o (sort qw(
	DEBUGGING HAS_LONG_DOUBLE HAS_STRTOLD HAS_TIMES MULTIPLICITY
	PERL_COPY_ON_WRITE PERL_DONT_CREATE_GVSV PERL_HASH_FUNC_SIPHASH13
	PERL_HASH_USE_SBOX32 PERL_MALLOC_WRAP PERL_OP_PARENT PERL_PRESERVE_IVUV
	PERL_TRACK_MEMPOOL PERL_USE_SAFE_PUTENV PERLIO_LAYERS USE_64_BIT_ALL
	USE_64_BIT_INT USE_ITHREADS USE_LARGE_FILES USE_LOCALE
	USE_LOCALE_COLLATE USE_LOCALE_CTYPE USE_LOCALE_NUMERIC USE_LOCALE_TIME
	USE_PERL_ATOF USE_PERLIO USE_QUADMATH USE_REENTRANT_API
	USE_THREAD_SAFE_LOCALE
	)) {
    is ($conf->{build}{options}{$o}, 1, "Runtime option $o set");
    delete $opt->{$o};
    }
foreach my $o (sort keys %$opt) {
    is ($conf->{build}{options}{$o}, 0, "Runtime option $o unset");
    }

eval { require Digest::MD5; };
my $md5 = $@ ? "0" x 32 : "83aed22122624285f6c4faaab38547ef";
ok (my $sig = Config::Perl::V::signature ($conf), "Get signature");

SKIP: {
    ord "A" == 65 or skip "ASCII-centric test", 1;
    is ($sig, $md5, "MD5");
    }

is_deeply ($conf->{build}{patches}, [ ], "No patches");

my %check = (
    alignbytes      => 16,
    api_version     => 40,
    bincompat5005   => undef,	# GONE, chainsawed
    byteorder       => 12345678,
    cc              => "cc",
    cccdlflags      => "-fPIC",
    ccdlflags       => "-Wl,-E -Wl,-rpath,/pro/lib/perl5/5.40.0/x86_64-linux-thread-multi-quadmath/CORE",
    config_args     => "-Uversiononly -Dinc_version_list=none -Duse64bitall -Dusethreads -Duseithreads -Dusequadmath -Duseshrplib -des",
    gccversion      => "13.3.0",
    gnulibc_version => "2.39",
    ivsize          => 8,
    ivtype          => "long",
    ld              => "cc",
    lddlflags       => "-shared -O2 -L/pro/local/lib -fstack-protector-strong",
    ldflags         => "-L/pro/local/lib -fstack-protector-strong",
    libc            => "/lib/../lib64/libc.so.6",
    lseektype       => "off_t",
    osvers          => "6.9.3-1-default",
    use64bitall     => "define",
    use64bitint     => "define",
    usemymalloc     => "n",
    default_inc_excludes_dot
		    => "define",
    );
is ($conf->{config}{$_}, $check{$_}, "reconstructed \$Config{$_}") for sort keys %check;

ok (my $info = summary ($conf), "A summary");
ok (exists $info->{$_}, "Summary has $_") for qw( cc config_args usemymalloc default_inc_excludes_dot );
is ($info->{default_inc_excludes_dot}, "define", "This build has . NOT in INC");

__END__
Summary of my perl5 (revision 5 version 40 subversion 0) configuration:
   
  Platform:
    osname=linux
    osvers=6.9.3-1-default
    archname=x86_64-linux-thread-multi-quadmath
    uname='linux lx09 6.9.3-1-default #1 smp preempt_dynamic thu may 30 08:29:01 utc 2024 (279162a) x86_64 x86_64 x86_64 gnulinux '
    config_args='-Uversiononly -Dinc_version_list=none -Duse64bitall -Dusethreads -Duseithreads -Dusequadmath -Duseshrplib -des'
    hint=recommended
    useposix=true
    d_sigaction=define
    useithreads=define
    usemultiplicity=define
    use64bitint=define
    use64bitall=define
    uselongdouble=undef
    usemymalloc=n
    default_inc_excludes_dot=define
  Compiler:
    cc='cc'
    ccflags ='-D_REENTRANT -D_GNU_SOURCE -pie -fPIE -fPIC -DDEBUGGING -fwrapv -fno-strict-aliasing -pipe -fstack-protector-strong -I/pro/local/include -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=2'
    optimize='-O2'
    cppflags='-D_REENTRANT -D_GNU_SOURCE -pie -fPIE -fPIC -DDEBUGGING -fwrapv -fno-strict-aliasing -pipe -fstack-protector-strong -I/pro/local/include'
    ccversion=''
    gccversion='13.3.0'
    gccosandvers=''
    intsize=4
    longsize=8
    ptrsize=8
    doublesize=8
    byteorder=12345678
    doublekind=3
    d_longlong=define
    longlongsize=8
    d_longdbl=define
    longdblsize=16
    longdblkind=3
    ivtype='long'
    ivsize=8
    nvtype='__float128'
    nvsize=16
    Off_t='off_t'
    lseeksize=8
    alignbytes=16
    prototype=define
  Linker and Libraries:
    ld='cc'
    ldflags ='-L/pro/local/lib -fstack-protector-strong'
    libpth=/usr/local/lib /usr/x86_64-suse-linux/lib /usr/lib /pro/local/lib /usr/lib64 /usr/local/lib64
    libs=-lpthread -lgdbm -ldb -ldl -lm -lcrypt -lutil -lc -lgdbm_compat -lquadmath
    perllibs=-lpthread -ldl -lm -lcrypt -lutil -lc -lquadmath
    libc=/lib/../lib64/libc.so.6
    so=so
    useshrplib=true
    libperl=libperl.so
    gnulibc_version='2.39'
  Dynamic Linking:
    dlsrc=dl_dlopen.xs
    dlext=so
    d_dlsymun=undef
    ccdlflags='-Wl,-E -Wl,-rpath,/pro/lib/perl5/5.40.0/x86_64-linux-thread-multi-quadmath/CORE'
    cccdlflags='-fPIC'
    lddlflags='-shared -O2 -L/pro/local/lib -fstack-protector-strong'


Characteristics of this binary (from libperl): 
  Compile-time options:
    DEBUGGING
    HAS_LONG_DOUBLE
    HAS_STRTOLD
    HAS_TIMES
    MULTIPLICITY
    PERLIO_LAYERS
    PERL_COPY_ON_WRITE
    PERL_DONT_CREATE_GVSV
    PERL_HASH_FUNC_SIPHASH13
    PERL_HASH_USE_SBOX32
    PERL_MALLOC_WRAP
    PERL_OP_PARENT
    PERL_PRESERVE_IVUV
    PERL_TRACK_MEMPOOL
    PERL_USE_SAFE_PUTENV
    USE_64_BIT_ALL
    USE_64_BIT_INT
    USE_ITHREADS
    USE_LARGE_FILES
    USE_LOCALE
    USE_LOCALE_COLLATE
    USE_LOCALE_CTYPE
    USE_LOCALE_NUMERIC
    USE_LOCALE_TIME
    USE_PERLIO
    USE_PERL_ATOF
    USE_QUADMATH
    USE_REENTRANT_API
    USE_THREAD_SAFE_LOCALE
  Built under linux
  Compiled at Jun 10 2024 10:14:20
  %ENV:
    PERL6LIB="inst#/pro/3gl/CPAN/rakudo/install"
  @INC:
    /pro/lib/perl5/site_perl/5.40.0/x86_64-linux-thread-multi-quadmath
    /pro/lib/perl5/site_perl/5.40.0
    /pro/lib/perl5/5.40.0/x86_64-linux-thread-multi-quadmath
    /pro/lib/perl5/5.40.0
