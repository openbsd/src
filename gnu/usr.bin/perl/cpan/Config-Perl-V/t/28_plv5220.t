#!/pro/bin/perl

use strict;
use warnings;

BEGIN {
    use Test::More;
    my $tests = 119;
    unless ($ENV{PERL_CORE}) {
	require Test::NoWarnings;
	Test::NoWarnings->import ();
	$tests++;
	}

    plan tests => $tests;
    }

use Config::Perl::V;

ok (my $conf = Config::Perl::V::plv2hash (<DATA>), "Read perl -v block");
ok (exists $conf->{$_}, "Has $_ entry") for qw( build environment config inc );

is ($conf->{build}{osname}, $conf->{config}{osname}, "osname");
is ($conf->{build}{stamp}, "Jun  4 2015 17:51:24", "Build time");
is ($conf->{config}{version}, "5.22.0", "reconstructed \$Config{version}");

my $opt = Config::Perl::V::plv2hash ("")->{build}{options};
foreach my $o (sort qw(
	HAS_TIMES MULTIPLICITY PERLIO_LAYERS
	PERL_DONT_CREATE_GVSV
	PERL_HASH_FUNC_ONE_AT_A_TIME_HARD
	PERL_IMPLICIT_CONTEXT PERL_MALLOC_WRAP
	PERL_NEW_COPY_ON_WRITE PERL_PRESERVE_IVUV
	USE_64_BIT_ALL USE_64_BIT_INT USE_ITHREADS
	USE_LARGE_FILES USE_LOCALE USE_LOCALE_COLLATE
	USE_LOCALE_CTYPE USE_LOCALE_NUMERIC USE_LOCALE_TIME
	USE_LONG_DOUBLE USE_PERLIO USE_PERL_ATOF USE_REENTRANT_API
	)) {
    is ($conf->{build}{options}{$o}, 1, "Runtime option $o set");
    delete $opt->{$o};
    }
foreach my $o (sort keys %$opt) {
    is ($conf->{build}{options}{$o}, 0, "Runtime option $o unset");
    }

eval { require Digest::MD5; };
my $md5 = $@ ? "0" x 32 : "ddcc2d51e43bf18f5234ba66529068ef";
ok (my $sig = Config::Perl::V::signature ($conf), "Get signature");
is ($sig, $md5, "MD5");

is_deeply ($conf->{build}{patches}, [], "No local patches");

my %check = (
    alignbytes      => 16,
    api_version     => 22,
    bincompat5005   => "undef",
    byteorder       => 12345678,
    cc              => "cc",
    cccdlflags      => "-fPIC",
    ccdlflags       => "-Wl,-E",
    config_args     => "-Duse64bitall -Duselongdouble -Dusethreads -Duseithreads -des",
    gccversion      => "4.8.3 20140627 [gcc-4_8-branch revision 212064]",
    gnulibc_version => "2.19",
    ivsize          => 8,
    ivtype          => "long",
    ld              => "cc",
    lddlflags       => "-shared -O2 -L/pro/local/lib -fstack-protector",
    ldflags         => "-L/pro/local/lib -fstack-protector",
    libc            => "libc-2.19.so",
    lseektype       => "off_t",
    osvers          => "3.16.7-21-desktop",
    use64bitall     => "define",
    use64bitint     => "define",
    );
is ($conf->{config}{$_}, $check{$_}, "reconstructed \$Config{$_}") for sort keys %check;

__END__
Summary of my perl5 (revision 5 version 22 subversion 0) configuration:
   
  Platform:
    osname=linux, osvers=3.16.7-21-desktop, archname=x86_64-linux-thread-multi-ld
    uname='linux lx09 3.16.7-21-desktop #1 smp preempt tue apr 14 07:11:37 utc 2015 (93c1539) x86_64 x86_64 x86_64 gnulinux '
    config_args='-Duse64bitall -Duselongdouble -Dusethreads -Duseithreads -des'
    hint=recommended, useposix=true, d_sigaction=define
    useithreads=define, usemultiplicity=define
    use64bitint=define, use64bitall=define, uselongdouble=define
    usemymalloc=n, bincompat5005=undef
  Compiler:
    cc='cc', ccflags ='-D_REENTRANT -D_GNU_SOURCE -fPIC -fwrapv -fno-strict-aliasing -pipe -fstack-protector -I/pro/local/include -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=2',
    optimize='-O2',
    cppflags='-D_REENTRANT -D_GNU_SOURCE -fPIC -fwrapv -fno-strict-aliasing -pipe -fstack-protector -I/pro/local/include'
    ccversion='', gccversion='4.8.3 20140627 [gcc-4_8-branch revision 212064]', gccosandvers=''
    intsize=4, longsize=8, ptrsize=8, doublesize=8, byteorder=12345678, doublekind=3
    d_longlong=define, longlongsize=8, d_longdbl=define, longdblsize=16, longdblkind=3
    ivtype='long', ivsize=8, nvtype='long double', nvsize=16, Off_t='off_t', lseeksize=8
    alignbytes=16, prototype=define
  Linker and Libraries:
    ld='cc', ldflags ='-L/pro/local/lib -fstack-protector'
    libpth=/usr/local/lib /usr/lib64/gcc/x86_64-suse-linux/4.8/include-fixed /usr/lib64/gcc/x86_64-suse-linux/4.8/../../../../x86_64-suse-linux/lib /usr/lib /pro/local/lib /lib/../lib64 /usr/lib/../lib64 /lib /lib64 /usr/lib64 /usr/local/lib64
    libs=-lpthread -lnsl -lgdbm -ldb -ldl -lm -lcrypt -lutil -lc -lgdbm_compat
    perllibs=-lpthread -lnsl -ldl -lm -lcrypt -lutil -lc
    libc=libc-2.19.so, so=so, useshrplib=false, libperl=libperl.a
    gnulibc_version='2.19'
  Dynamic Linking:
    dlsrc=dl_dlopen.xs, dlext=so, d_dlsymun=undef, ccdlflags='-Wl,-E'
    cccdlflags='-fPIC', lddlflags='-shared -O2 -L/pro/local/lib -fstack-protector'


Characteristics of this binary (from libperl): 
  Compile-time options: HAS_TIMES MULTIPLICITY PERLIO_LAYERS
                        PERL_DONT_CREATE_GVSV
                        PERL_HASH_FUNC_ONE_AT_A_TIME_HARD
                        PERL_IMPLICIT_CONTEXT PERL_MALLOC_WRAP
                        PERL_NEW_COPY_ON_WRITE PERL_PRESERVE_IVUV
                        USE_64_BIT_ALL USE_64_BIT_INT USE_ITHREADS
                        USE_LARGE_FILES USE_LOCALE USE_LOCALE_COLLATE
                        USE_LOCALE_CTYPE USE_LOCALE_NUMERIC USE_LOCALE_TIME
                        USE_LONG_DOUBLE USE_PERLIO USE_PERL_ATOF
                        USE_REENTRANT_API
  Built under linux
  Compiled at Jun  4 2015 17:51:24
  @INC:
    /pro/lib/perl5/site_perl/5.22.0/x86_64-linux-thread-multi-ld
    /pro/lib/perl5/site_perl/5.22.0
    /pro/lib/perl5/5.22.0/x86_64-linux-thread-multi-ld
    /pro/lib/perl5/5.22.0
    .
