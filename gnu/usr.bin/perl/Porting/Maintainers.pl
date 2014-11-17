#!perl
# A simple listing of core files that have specific maintainers,
# or at least someone that can be called an "interested party".
# Also, a "module" does not necessarily mean a CPAN module, it
# might mean a file or files or a subdirectory.
# Most (but not all) of the modules have dual lives in the core
# and in CPAN.

package Maintainers;

use utf8;
use File::Glob qw(:case);

# IGNORABLE: files which, if they appear in the root of a CPAN
# distribution, need not appear in core (i.e. core-cpan-diff won't
# complain if it can't find them)

@IGNORABLE = qw(
    .cvsignore .dualLivedDiffConfig .gitignore
    ANNOUNCE Announce Artistic AUTHORS BENCHMARK BUGS Build.PL
    CHANGELOG ChangeLog Changelog CHANGES Changes CONTRIBUTING COPYING Copying
    cpanfile CREDITS dist.ini GOALS HISTORY INSTALL INSTALL.SKIP LICENSE
    Makefile.PL MANIFEST MANIFEST.SKIP META.json META.yml MYMETA.json
    MYMETA.yml NEW NOTES perlcritic.rc ppport.h README README.PATCHING
    SIGNATURE THANKS TODO Todo VERSION WHATSNEW .perlcriticrc.perltidyrc
);

# Each entry in the  %Modules hash roughly represents a distribution,
# except when DISTRIBUTION is set, where it *exactly* represents a single
# CPAN distribution.

# The keys of %Modules are human descriptions of the distributions, and
# may not exactly match a module or distribution name. Distributions
# which have an obvious top-level module associated with them will usually
# have a key named for that module, e.g. 'Archive::Extract' for
# Archive-Extract-N.NN.tar.gz; the remaining keys are likely to be based
# on the name of the distribution, e.g. 'Locale-Codes' for
# Locale-Codes-N.NN.tar.gz'.

# UPSTREAM indicates where patches should go.  This is generally now
# inferred from the FILES: modules with files in dist/, ext/ and lib/
# are understood to have UPSTREAM 'blead', meaning that the copy of the
# module in the blead sources is to be considered canonical, while
# modules with files in cpan/ are understood to have UPSTREAM 'cpan',
# meaning that the module on CPAN is to be patched first.

# MAINTAINER has previously been used to indicate who the current maintainer
# of the module is, but this is no longer stated explicitly. It is now
# understood to be either the Perl 5 Porters if UPSTREAM is 'blead', or else
# the CPAN author whose PAUSE user ID forms the first part of the DISTRIBUTION
# value, e.g. 'BINGOS' in the case of 'BINGOS/Archive-Tar-1.96.tar.gz'.
# (PAUSE's View Permissions page may be consulted to find other authors who
# have owner or co-maint permissions for the module in question.)

# FILES is a list of filenames, glob patterns, and directory
# names to be recursed down, which collectively generate a complete list
# of the files associated with the distribution.

# BUGS is an email or url to post bug reports.  For modules with
# UPSTREAM => 'blead', use perl5-porters@perl.org.  rt.cpan.org
# appears to automatically provide a URL for CPAN modules; any value
# given here overrides the default:
# http://rt.cpan.org/Public/Dist/Display.html?Name=$ModuleName

# DISTRIBUTION names the tarball on CPAN which (allegedly) the files
# included in core are derived from. Note that the file's version may not
# necessarily match the newest version on CPAN.

# EXCLUDED is a list of files to be excluded from a CPAN tarball before
# comparing the remaining contents with core. Each item can either be a
# full pathname (eg 't/foo.t') or a pattern (e.g. qr{^t/}).
# It defaults to the empty list.

# CUSTOMIZED is a list of files that have been customized within the
# Perl core.  Use this whenever patching a cpan upstream distribution
# or whenever we expect to have a file that differs from the tarball.
# If the file in blead matches the file in the tarball from CPAN,
# Porting/core-cpan-diff will warn about it, as it indicates an expected
# customization might have been lost when updating from upstream.  The
# path should be relative to the distribution directory.  If the upstream
# distribution should be modified to incorporate the change then be sure
# to raise a ticket for it on rt.cpan.org and add a comment alongside the
# list of CUSTOMIZED files noting the ticket number.

# DEPRECATED contains the *first* version of Perl in which the module
# was considered deprecated.  It should only be present if the module is
# actually deprecated.  Such modules should use deprecated.pm to
# issue a warning if used.  E.g.:
#
#     use if $] >= 5.011, 'deprecate';
#

# MAP is a hash that maps CPAN paths to their core equivalents.
# Each key represents a string prefix, with longest prefixes checked
# first. The first match causes that prefix to be replaced with the
# corresponding key. For example, with the following MAP:
#   {
#     'lib/'     => 'lib/',
#     ''     => 'lib/Foo/',
#   },
#
# these files are mapped as shown:
#
#    README     becomes lib/Foo/README
#    lib/Foo.pm becomes lib/Foo.pm
#
# The default is dependent on the type of module.
# For distributions which appear to be stored under ext/, it defaults to:
#
#   { '' => 'ext/Foo-Bar/' }
#
# otherwise, it's
#
#   {
#     'lib/'     => 'lib/',
#     ''     => 'lib/Foo/Bar/',
#   }

%Modules = (

    'Archive::Tar' => {
        'DISTRIBUTION' => 'BINGOS/Archive-Tar-1.96.tar.gz',
        'FILES'        => q[cpan/Archive-Tar],
        'BUGS'         => 'bug-archive-tar@rt.cpan.org',
    },

    'Attribute::Handlers' => {
        'DISTRIBUTION' => 'SMUELLER/Attribute-Handlers-0.96.tar.gz',
        'FILES'        => q[dist/Attribute-Handlers],
    },

    'autodie' => {
        'DISTRIBUTION' => 'PJF/autodie-2.23.tar.gz',
        'FILES'        => q[cpan/autodie],
        'EXCLUDED'     => [
            qr{benchmarks},
            # All these tests depend upon external
            # modules that don't exist when we're
            # building the core.  Hence, they can
            # never run, and should not be merged.
            qw( t/author-critic.t
                t/boilerplate.t
                t/critic.t
                t/fork.t
                t/kwalitee.t
                t/lex58.t
                t/pod-coverage.t
                t/pod.t
                t/release-pod-coverage.t
                t/release-pod-syntax.t
                t/socket.t
                t/system.t
                )
        ],
        'CUSTOMIZED'   => [
            # Waiting to be merged upstream: see CPAN RT#87237
            qw(	t/utf8_open.t ),
        ],
    },

    'AutoLoader' => {
        'DISTRIBUTION' => 'SMUELLER/AutoLoader-5.74.tar.gz',
        'FILES'        => q[cpan/AutoLoader],
        'EXCLUDED'     => ['t/00pod.t'],
    },

    'autouse' => {
        'DISTRIBUTION' => 'FLORA/autouse-1.07.tar.gz',
        'FILES'        => q[dist/autouse],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
    },

    'B::Debug' => {
        'DISTRIBUTION' => 'RURBAN/B-Debug-1.19.tar.gz',
        'FILES'        => q[cpan/B-Debug],
        'EXCLUDED'     => ['t/pod.t'],
    },

    'base' => {
        'DISTRIBUTION' => 'RGARCIA/base-2.18.tar.gz',
        'FILES'        => q[dist/base],
    },

    'bignum' => {
        'DISTRIBUTION' => 'FLORA/bignum-0.32.tar.gz',
        'FILES'        => q[dist/bignum],
        'EXCLUDED'     => [
            qr{^inc/Module/},
            qw( t/pod.t
                t/pod_cov.t
                ),
        ],
    },

    'Carp' => {
        'DISTRIBUTION' => 'ZEFRAM/Carp-1.3301.tar.gz',
        'FILES'        => q[dist/Carp],
    },

    'CGI' => {
        'DISTRIBUTION' => 'MARKSTOS/CGI.pm-3.65.tar.gz',
        'FILES'        => q[cpan/CGI],
        'EXCLUDED'     => [
            qr{^examples/},
            qw( cgi_docs.html
                t/gen-tests/gen-start-end-tags.pl
                t/fast.t
                ),
        ],
        'DEPRECATED' => '5.019007',
    },

    'Compress::Raw::Bzip2' => {
        'DISTRIBUTION' => 'PMQS/Compress-Raw-Bzip2-2.064.tar.gz',
        'FILES'        => q[cpan/Compress-Raw-Bzip2],
        'EXCLUDED'     => [
            qr{^t/Test/},
            'bzip2-src/bzip2-cpp.patch',
        ],
    },

    'Compress::Raw::Zlib' => {
        'DISTRIBUTION' => 'PMQS/Compress-Raw-Zlib-2.065.tar.gz',

        'FILES'    => q[cpan/Compress-Raw-Zlib],
        'EXCLUDED' => [
            qr{^examples/},
            qr{^t/Test/},
            qw( t/000prereq.t
                t/99pod.t
                ),
        ],
    },

    'Config::Perl::V' => {
        'DISTRIBUTION' => 'HMBRAND/Config-Perl-V-0.22.tgz',
        'FILES'        => q[cpan/Config-Perl-V],
        'EXCLUDED'     => [qw(
		examples/show-v.pl
		t/00_pod.t
		t/01_pod.t
		)],
    },

    'constant' => {
        'DISTRIBUTION' => 'SAPER/constant-1.27.tar.gz',
        'FILES'        => q[dist/constant],
        'EXCLUDED'     => [
            qw( t/00-load.t
                t/more-tests.t
                t/pod-coverage.t
                t/pod.t
                eg/synopsis.pl
                ),
        ],
    },

    'CPAN' => {
        'DISTRIBUTION' => 'ANDK/CPAN-2.05.tar.gz',
        'FILES'        => q[cpan/CPAN],
        'EXCLUDED'     => [
            qr{^distroprefs/},
            qr{^inc/Test/},
            qr{^t/CPAN/},
            qr{^t/data/},
            qw( lib/CPAN/Admin.pm
                scripts/cpan-mirrors
                PAUSE2015.pub
                SlayMakefile
                t/00signature.t
                t/04clean_load.t
                t/12cpan.t
                t/13tarzip.t
                t/14forkbomb.t
                t/30shell.coverage
                t/30shell.t
                t/31sessions.t
                t/41distribution.t
                t/42distroprefs.t
                t/43distroprefspref.t
                t/44cpanmeta.t
                t/50pod.t
                t/51pod.t
                t/52podcover.t
                t/60credentials.t
                t/70_critic.t
                t/71_minimumversion.t
                t/local_utils.pm
                t/perlcriticrc
                t/yaml_code.yml
                ),
        ],
    },

    # Note: When updating CPAN-Meta the META.* files will need to be regenerated
    # perl -Icpan/CPAN-Meta/lib Porting/makemeta
    'CPAN::Meta' => {
        'DISTRIBUTION' => 'DAGOLDEN/CPAN-Meta-2.140640.tar.gz',
        'FILES'        => q[cpan/CPAN-Meta],
        'EXCLUDED'     => [
            qw[t/00-report-prereqs.t],
            qr{t/README-data.txt},
            qr{^xt},
            qr{^history},
        ],
    },

    'CPAN::Meta::Requirements' => {
        'DISTRIBUTION' => 'DAGOLDEN/CPAN-Meta-Requirements-2.125.tar.gz',
        'FILES'        => q[cpan/CPAN-Meta-Requirements],
        'EXCLUDED'     => [
            qw(t/00-compile.t),
            qw(t/00-report-prereqs.t),
            qr{^xt},
        ],
    },

    'CPAN::Meta::YAML' => {
        'DISTRIBUTION' => 'DAGOLDEN/CPAN-Meta-YAML-0.012.tar.gz',
        'FILES'        => q[cpan/CPAN-Meta-YAML],
        'EXCLUDED'     => [
            't/00-compile.t',
            't/00-report-prereqs.t',
            't/04_scalar.t',    # requires YAML.pm
            qr{^xt},
        ],
    },

    'Data::Dumper' => {
        'DISTRIBUTION' => 'SMUELLER/Data-Dumper-2.151.tar.gz',
        'FILES'        => q[dist/Data-Dumper],
    },

    'DB_File' => {
        'DISTRIBUTION' => 'PMQS/DB_File-1.831.tar.gz',
        'FILES'        => q[cpan/DB_File],
        'EXCLUDED'     => [
            qr{^patches/},
            qw( t/pod.t
                fallback.h
                fallback.xs
                ),
        ],
    },

    'Devel::PPPort' => {
        'DISTRIBUTION' => 'MHX/Devel-PPPort-3.21.tar.gz',
        # RJBS has asked MHX to have UPSTREAM be 'blead'
        # (i.e. move this from cpan/ to dist/)
        'FILES'        => q[cpan/Devel-PPPort],
        'EXCLUDED'     => [
            'PPPort.pm',    # we use PPPort_pm.PL instead
            'README.md',
        ]
    },

    'Devel::SelfStubber' => {
        'DISTRIBUTION' => 'FLORA/Devel-SelfStubber-1.05.tar.gz',
        'FILES'        => q[dist/Devel-SelfStubber],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
    },

    'Digest' => {
        'DISTRIBUTION' => 'GAAS/Digest-1.17.tar.gz',
        'FILES'        => q[cpan/Digest],
        'EXCLUDED'     => ['digest-bench'],
    },

    'Digest::MD5' => {
        'DISTRIBUTION' => 'GAAS/Digest-MD5-2.53.tar.gz',
        'FILES'        => q[cpan/Digest-MD5],
        'EXCLUDED'     => ['rfc1321.txt'],
        # Waiting to be merged upstream: see CPAN RT#89612
        'CUSTOMIZED'   => ['t/files.t'],
    },

    'Digest::SHA' => {
        'DISTRIBUTION' => 'MSHELOR/Digest-SHA-5.88.tar.gz',
        'FILES'        => q[cpan/Digest-SHA],
        'EXCLUDED'     => [
            qw( t/pod.t
                t/podcover.t
                examples/dups
                ),
        ],
    },

    'Dumpvalue' => {
        'DISTRIBUTION' => 'FLORA/Dumpvalue-1.17.tar.gz',
        'FILES'        => q[dist/Dumpvalue],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
    },

    'Encode' => {
        'DISTRIBUTION' => 'DANKOGAI/Encode-2.60.tar.gz',
        'FILES'        => q[cpan/Encode],
        'CUSTOMIZED'   => [
            # Waiting to be merged upstream: see CPAN RT#95130 and
            # Message-Id: <201405062058.s46KwdCZ013775@m-l.org>
            qw(
                Byte/Makefile.PL
                CN/Makefile.PL
                EBCDIC/Makefile.PL
                Encode.pm
                Encode.xs
                Encode/Makefile_PL.e2x
                JP/Makefile.PL
                KR/Makefile.PL
                Symbol/Makefile.PL
                TW/Makefile.PL
                bin/enc2xs
            ),
        ],

    },

    'encoding::warnings' => {
        'DISTRIBUTION' => 'AUDREYT/encoding-warnings-0.11.tar.gz',
        'FILES'        => q[cpan/encoding-warnings],
        'EXCLUDED'     => [
            qr{^inc/Module/},
            qw(t/0-signature.t),
        ],
    },

    'Env' => {
        'DISTRIBUTION' => 'FLORA/Env-1.04.tar.gz',
        'FILES'        => q[dist/Env],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
    },

    'experimental' => {
        'DISTRIBUTION' => 'LEONT/experimental-0.007.tar.gz',
        'FILES'        => q[cpan/experimental],
        'EXCLUDED'     => [
          qr{^t/release-.*\.t},
          't/00-compile.t',
        ],
    },

    'Exporter' => {
        'DISTRIBUTION' => 'TODDR/Exporter-5.68.tar.gz',
        'FILES'        => q[dist/Exporter],
        'EXCLUDED' => [
            qw( t/pod.t
                t/use.t
                ),
        ],
    },

    'ExtUtils::CBuilder' => {
        'DISTRIBUTION' => 'AMBS/ExtUtils/ExtUtils-CBuilder-0.280216.tar.gz',
        'FILES'        => q[dist/ExtUtils-CBuilder],
        'EXCLUDED'     => [
            qw(README.mkdn),
            qr{^xt},
        ],
    },

    'ExtUtils::Command' => {
        'DISTRIBUTION' => 'FLORA/ExtUtils-Command-1.18.tar.gz',
        'FILES'        => q[dist/ExtUtils-Command],
        'EXCLUDED'     => [qr{^t/release-}],
    },

    'ExtUtils::Constant' => {

        # Nick has confirmed that while we have diverged from CPAN,
        # this package isn't primarily maintained in core
        # Another release will happen "Sometime"
        'DISTRIBUTION' => '',    #'NWCLARK/ExtUtils-Constant-0.16.tar.gz',
        'FILES'    => q[cpan/ExtUtils-Constant],
        'EXCLUDED' => [
            qw( lib/ExtUtils/Constant/Aaargh56Hash.pm
                examples/perl_keyword.pl
                examples/perl_regcomp_posix_keyword.pl
                ),
        ],
    },

    'ExtUtils::Install' => {
        'DISTRIBUTION' => 'BINGOS/ExtUtils-Install-1.67.tar.gz',
        'FILES'        => q[dist/ExtUtils-Install],
        'EXCLUDED'     => [
            qw( t/lib/Test/Builder.pm
                t/lib/Test/Builder/Module.pm
                t/lib/Test/More.pm
                t/lib/Test/Simple.pm
                t/pod-coverage.t
                t/pod.t
                ),
        ],
    },

    'ExtUtils::MakeMaker' => {
        'DISTRIBUTION' => 'BINGOS/ExtUtils-MakeMaker-6.98.tar.gz',
        'FILES'        => q[cpan/ExtUtils-MakeMaker],
        'EXCLUDED'     => [
            qr{^t/lib/Test/},
            qr{^(bundled|my)/},
            qr{^t/Liblist_Kid.t},
            qr{^t/liblist/},
            qr{^\.perlcriticrc},
            'PATCHING',
            'README.packaging',
        ],
    },

    'ExtUtils::Manifest' => {
        'DISTRIBUTION' => 'FLORA/ExtUtils-Manifest-1.63.tar.gz',
        'FILES'        => q[dist/ExtUtils-Manifest],
        'EXCLUDED'     => [qr(t/release-.*\.t)],
    },

    'ExtUtils::ParseXS' => {
        'DISTRIBUTION' => 'SMUELLER/ExtUtils-ParseXS-3.24.tar.gz',
        'FILES'        => q[dist/ExtUtils-ParseXS],
    },

    'File::Fetch' => {
        'DISTRIBUTION' => 'BINGOS/File-Fetch-0.48.tar.gz',
        'FILES'        => q[cpan/File-Fetch],
    },

    'File::Path' => {
        'DISTRIBUTION' => 'DLAND/File-Path-2.09.tar.gz',
        'FILES'        => q[cpan/File-Path],
        'EXCLUDED'     => [
            qw( eg/setup-extra-tests
                t/pod.t
                )
        ],
        'MAP' => {
            ''   => 'cpan/File-Path/lib/File/',
            't/' => 'cpan/File-Path/t/',
        },
    },

    'File::Temp' => {
        'DISTRIBUTION' => 'DAGOLDEN/File-Temp-0.2304.tar.gz',
        'FILES'        => q[cpan/File-Temp],
        'EXCLUDED'     => [
            qw( misc/benchmark.pl
                misc/results.txt
                ),
            qw[t/00-report-prereqs.t],
            qr{^xt},
        ],
    },

    'Filter::Simple' => {
        'DISTRIBUTION' => 'SMUELLER/Filter-Simple-0.91.tar.gz',
        'FILES'        => q[dist/Filter-Simple],
        'EXCLUDED'     => [
            qr{^demo/}
        ],
    },

    'Filter::Util::Call' => {
        'DISTRIBUTION' => 'RURBAN/Filter-1.49.tar.gz',
        'FILES'        => q[cpan/Filter-Util-Call
                 pod/perlfilter.pod
                ],
        'EXCLUDED' => [
            qr{^decrypt/},
            qr{^examples/},
            qr{^Exec/},
            qr{^lib/Filter/},
            qr{^tee/},
            qw( Call/Makefile.PL
                Call/ppport.h
                Call/typemap
                mytest
                t/cpp.t
                t/decrypt.t
                t/exec.t
                t/order.t
                t/sh.t
                t/tee.t
                t/z_kwalitee.t
                t/z_meta.t
                t/z_perl_minimum_version.t
                t/z_pod-coverage.t
                t/z_pod.t
                ),
        ],
        'MAP' => {
            'Call/'          => 'cpan/Filter-Util-Call/',
            'filter-util.pl' => 'cpan/Filter-Util-Call/filter-util.pl',
            'perlfilter.pod' => 'pod/perlfilter.pod',
            ''               => 'cpan/Filter-Util-Call/',
        },
    },

    'Getopt::Long' => {
        'DISTRIBUTION' => 'JV/Getopt-Long-2.42.tar.gz',
        'FILES'        => q[cpan/Getopt-Long],
        'EXCLUDED'     => [
            qr{^examples/},
            qw( perl-Getopt-Long.spec
                lib/newgetopt.pl
                t/gol-compat.t
                ),
        ],
    },

    'HTTP::Tiny' => {
        'DISTRIBUTION' => 'DAGOLDEN/HTTP-Tiny-0.043.tar.gz',
        'FILES'        => q[cpan/HTTP-Tiny],
        'EXCLUDED'     => [
            't/00-report-prereqs.t',
            't/200_live.t',
            't/200_live_local_ip.t',
            't/210_live_ssl.t',
            qr/^eg/,
            qr/^xt/
        ],
    },

    'I18N::Collate' => {
        'DISTRIBUTION' => 'FLORA/I18N-Collate-1.02.tar.gz',
        'FILES'        => q[dist/I18N-Collate],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
    },

    'I18N::LangTags' => {
        'FILES'        => q[dist/I18N-LangTags],
    },

    'if' => {
        'DISTRIBUTION' => 'ILYAZ/modules/if-0.0601.tar.gz',
        'FILES'        => q[dist/if],
    },

    'IO' => {
        'DISTRIBUTION' => 'GBARR/IO-1.25.tar.gz',
        'FILES'        => q[dist/IO/],
        'EXCLUDED'     => ['t/test.pl'],
    },

    'IO-Compress' => {
        'DISTRIBUTION' => 'PMQS/IO-Compress-2.064.tar.gz',
        'FILES'        => q[cpan/IO-Compress],
        'EXCLUDED'     => [
            qr{^examples/},
            qr{^t/Test/},
            't/010examples-bzip2.t',
            't/010examples-zlib.t',
            't/cz-05examples.t',
        ],
    },

    'IO::Socket::IP' => {
        'DISTRIBUTION' => 'PEVANS/IO-Socket-IP-0.29.tar.gz',
        'FILES'        => q[cpan/IO-Socket-IP],
        'EXCLUDED'     => [
            qr{^examples/},
        ],
    },

    'IO::Zlib' => {
        'DISTRIBUTION' => 'TOMHUGHES/IO-Zlib-1.10.tar.gz',
        'FILES'        => q[cpan/IO-Zlib],
    },

    'IPC::Cmd' => {
        'DISTRIBUTION' => 'BINGOS/IPC-Cmd-0.92.tar.gz',
        'FILES'        => q[cpan/IPC-Cmd],
    },

    'IPC::SysV' => {
        'DISTRIBUTION' => 'MHX/IPC-SysV-2.04.tar.gz',
        'FILES'        => q[cpan/IPC-SysV],
        'EXCLUDED'     => [
            qw( const-c.inc
                const-xs.inc
                ),
        ],
    },

    'JSON::PP' => {
        'DISTRIBUTION' => 'MAKAMAKA/JSON-PP-2.27203.tar.gz',
        'FILES'        => q[cpan/JSON-PP],
    },

    'lib' => {
        'DISTRIBUTION' => 'SMUELLER/lib-0.63.tar.gz',
        'FILES'        => q[dist/lib/],
        'EXCLUDED'     => [
            qw( forPAUSE/lib.pm
                t/00pod.t
                ),
        ],
    },

    'libnet' => {
        'DISTRIBUTION' => 'SHAY/libnet-1.25.tar.gz',
        'FILES'        => q[cpan/libnet],
        'EXCLUDED'     => [
            qw( Configure
                install-nomake
                ),
            qr(^demos/),
        ],
        # Customized for perl since we cannot use either an auto-generated
        # script or the version in the CPAN distro.
        'CUSTOMIZED' => ['Makefile.PL'],
    },

    'Locale-Codes' => {
        'DISTRIBUTION' => 'SBECK/Locale-Codes-3.30.tar.gz',
        'FILES'        => q[cpan/Locale-Codes],
        'EXCLUDED'     => [
            qw( README.first
                t/pod_coverage.ign
                t/pod_coverage.t
                t/pod.t),
            qr{^t/runtests},
            qr{^t/runtests\.bat},
            qr{^internal/},
            qr{^examples/},
        ],
    },

    'Locale::Maketext' => {
        'DISTRIBUTION' => 'TODDR/Locale-Maketext-1.23.tar.gz',
        'FILES'        => q[dist/Locale-Maketext],
        'EXCLUDED'     => [
            qw(
                perlcriticrc
                t/00_load.t
                t/pod.t
                ),
        ],
    },

    'Locale::Maketext::Simple' => {
        'DISTRIBUTION' => 'JESSE/Locale-Maketext-Simple-0.21.tar.gz',
        'FILES'        => q[cpan/Locale-Maketext-Simple],
    },

    'Math::BigInt' => {
        'DISTRIBUTION' => 'PJACKLAM/Math-BigInt-1.997.tar.gz',
        'FILES'        => q[dist/Math-BigInt],
        'EXCLUDED'     => [
            qr{^inc/},
            qr{^examples/},
            qw( t/00sig.t
                t/01load.t
                t/02pod.t
                t/03podcov.t
                ),
        ],
    },

    'Math::BigInt::FastCalc' => {
        'DISTRIBUTION' => 'PJACKLAM/Math-BigInt-FastCalc-0.30.tar.gz',
        'FILES'        => q[dist/Math-BigInt-FastCalc],
        'EXCLUDED'     => [
            qr{^inc/},
            qw( t/00sig.t
                t/01load.t
                t/02pod.t
                t/03podcov.t
                ),

            # instead we use the versions of these test
            # files that come with Math::BigInt:
            qw( t/bigfltpm.inc
                t/bigfltpm.t
                t/bigintpm.inc
                t/bigintpm.t
                t/mbimbf.inc
                t/mbimbf.t
                ),
        ],
    },

    'Math::BigRat' => {
        'DISTRIBUTION' => 'PJACKLAM/Math-BigRat-0.2602.tar.gz',
        'FILES'        => q[dist/Math-BigRat],
        'EXCLUDED'     => [
            qr{^inc/},
            qw( t/00sig.t
                t/01load.t
                t/02pod.t
                t/03podcov.t
                ),
        ],
    },

    'Math::Complex' => {
        'DISTRIBUTION' => 'ZEFRAM/Math-Complex-1.59.tar.gz',
        'FILES'        => q[cpan/Math-Complex],
        'EXCLUDED'     => [
            qw( t/pod.t
                t/pod-coverage.t
                ),
        ],
    },

    'Memoize' => {
        'DISTRIBUTION' => 'MJD/Memoize-1.03.tgz',
        'FILES'        => q[cpan/Memoize],
        'EXCLUDED'     => ['article.html'],
    },

    'MIME::Base64' => {
        'DISTRIBUTION' => 'GAAS/MIME-Base64-3.14.tar.gz',
        'FILES'        => q[cpan/MIME-Base64],
        'EXCLUDED'     => ['t/bad-sv.t'],
    },

    #
    # To update Module-Build in blead see
    # https://github.com/Perl-Toolchain-Gang/Module-Build/blob/master/devtools/patching_blead.pod
    #

    'Module::Build' => {
        'DISTRIBUTION' => 'LEONT/Module-Build-0.4205.tar.gz',
        'FILES'        => q[cpan/Module-Build],
        'EXCLUDED'     => [
            qw( t/par.t
                t/signature.t
                ),
            qr{^contrib/},
            qr{^inc},
        ],
        # Generated file, not part of the CPAN distro:
        'CUSTOMIZED' => ['lib/Module/Build/ConfigData.pm'],
        'DEPRECATED' => '5.019000',
    },

    'Module::CoreList' => {
        'DISTRIBUTION' => 'BINGOS/Module-CoreList-3.10.tar.gz',
        'FILES'        => q[dist/Module-CoreList],
    },

    'Module::Load' => {
        'DISTRIBUTION' => 'BINGOS/Module-Load-0.32.tar.gz',
        'FILES'        => q[cpan/Module-Load],
    },

    'Module::Load::Conditional' => {
        'DISTRIBUTION' => 'BINGOS/Module-Load-Conditional-0.62.tar.gz',
        'FILES'        => q[cpan/Module-Load-Conditional],
    },

    'Module::Loaded' => {
        'DISTRIBUTION' => 'BINGOS/Module-Loaded-0.08.tar.gz',
        'FILES'        => q[cpan/Module-Loaded],
    },

    'Module::Metadata' => {
        'DISTRIBUTION' => 'ETHER/Module-Metadata-1.000019.tar.gz',
        'FILES'        => q[cpan/Module-Metadata],
        'EXCLUDED'     => [
            qr{^maint},
            qr{^xt},
        ],
    },

    'Net::Ping' => {
        'DISTRIBUTION' => 'SMPETERS/Net-Ping-2.41.tar.gz',
        'FILES'        => q[dist/Net-Ping],
        'EXCLUDED'     => [
            qr{^.travis.yml},
            qr{^README.md},
        ],
    },

    'NEXT' => {
        'DISTRIBUTION' => 'FLORA/NEXT-0.65.tar.gz',
        'FILES'        => q[cpan/NEXT],
        'EXCLUDED'     => [qr{^demo/}],
    },

    'Package::Constants' => {
        'DISTRIBUTION' => 'BINGOS/Package-Constants-0.04.tar.gz',
        'FILES'        => q[cpan/Package-Constants],
        'DEPRECATED'   => '5.019006',
    },

    'Params::Check' => {
        'DISTRIBUTION' => 'BINGOS/Params-Check-0.38.tar.gz',
        'FILES'        => q[cpan/Params-Check],
    },

    'parent' => {
        'DISTRIBUTION' => 'CORION/parent-0.228.tar.gz',
        'FILES'        => q[cpan/parent],
    },

    'Parse::CPAN::Meta' => {
        'DISTRIBUTION' => 'DAGOLDEN/Parse-CPAN-Meta-1.4414.tar.gz',
        'FILES'        => q[cpan/Parse-CPAN-Meta],
        'EXCLUDED'     => [
            qw[t/00-report-prereqs.t],
            qr{^xt},
        ],
    },

    'PathTools' => {
        'DISTRIBUTION' => 'SMUELLER/PathTools-3.47.tar.gz',
        'FILES'        => q[dist/PathTools],
        'EXCLUDED'     => [qr{^t/lib/Test/}],
    },

    'Perl::OSType' => {
        'DISTRIBUTION' => 'DAGOLDEN/Perl-OSType-1.007.tar.gz',
        'FILES'        => q[cpan/Perl-OSType],
        'EXCLUDED'     => [qw(tidyall.ini), qr/^xt/, qr{^t/00-}],
    },

    'perlfaq' => {
        'DISTRIBUTION' => 'LLAP/perlfaq-5.0150044.tar.gz',
        'FILES'        => q[cpan/perlfaq],
        'EXCLUDED'     => [
            qw( t/release-pod-syntax.t
                t/release-eol.t
                t/release-no-tabs.t
                )
        ],
    },

    'PerlIO::via::QuotedPrint' => {
        'DISTRIBUTION' => 'ELIZABETH/PerlIO-via-QuotedPrint-0.07.tar.gz',
        'FILES'        => q[cpan/PerlIO-via-QuotedPrint],

        # Waiting to be merged upstream: see CPAN RT#54047
        'CUSTOMIZED'   => [
            qw( t/QuotedPrint.t
                ),
        ],

    },

    'Pod::Checker' => {
        'DISTRIBUTION' => 'MAREKR/Pod-Checker-1.60.tar.gz',
        'FILES'        => q[cpan/Pod-Checker],
    },

    'Pod::Escapes' => {
        'DISTRIBUTION' => 'NEILB/Pod-Escapes-1.06.tar.gz',
        'FILES'        => q[cpan/Pod-Escapes],
    },

    'Pod::Parser' => {
        'DISTRIBUTION' => 'MAREKR/Pod-Parser-1.62.tar.gz',
        'FILES'        => q[cpan/Pod-Parser],
    },

    'Pod::Perldoc' => {
        'DISTRIBUTION' => 'MALLEN/Pod-Perldoc-3.23.tar.gz',
        'FILES'        => q[cpan/Pod-Perldoc],

        # Note that we use the CPAN-provided Makefile.PL, since it
        # contains special handling of the installation of perldoc.pod

        # In blead, the perldoc executable is generated by perldoc.PL
        # instead
        # XXX We can and should fix this, but clean up the DRY-failure in utils
        # first
        'EXCLUDED' => ['perldoc'],
    },

    'Pod::Simple' => {
        'DISTRIBUTION' => 'DWHEELER/Pod-Simple-3.28.tar.gz',
        'FILES'        => q[cpan/Pod-Simple],
    },

    'Pod::Usage' => {
        'DISTRIBUTION' => 'MAREKR/Pod-Usage-1.63.tar.gz',
        'FILES'        => q[cpan/Pod-Usage],
    },

    'podlators' => {
        'DISTRIBUTION' => 'RRA/podlators-2.5.3.tar.gz',
        'FILES'        => q[cpan/podlators pod/perlpodstyle.pod],

        # The perl distribution has pod2man.PL and pod2text.PL,  which are
        # run to create pod2man and pod2text, while the CPAN distribution
        # just has the post-generated pod2man and pod2text files.
        # The following entries attempt to codify that odd fact.
        'CUSTOMIZED' => [
            qw( scripts/pod2man.PL
                scripts/pod2text.PL
                ),
        ],
        'MAP' => {
            ''                 => 'cpan/podlators/',
            'scripts/pod2man'  => 'cpan/podlators/scripts/pod2man.PL',
            'scripts/pod2text' => 'cpan/podlators/scripts/pod2text.PL',

            # this file lives outside the cpan/ directory
            'pod/perlpodstyle.pod' => 'pod/perlpodstyle.pod',
        },
    },

    'Safe' => {
        'DISTRIBUTION' => 'RGARCIA/Safe-2.35.tar.gz',
        'FILES'        => q[dist/Safe],
    },

    'Scalar-List-Utils' => {
        'DISTRIBUTION' => 'PEVANS/Scalar-List-Utils-1.38.tar.gz',
        'FILES'    => q[cpan/Scalar-List-Utils],
    },

    'Search::Dict' => {
        'DISTRIBUTION' => 'DAGOLDEN/Search-Dict-1.07.tar.gz',
        'FILES'        => q[dist/Search-Dict],
    },

    'SelfLoader' => {
        'DISTRIBUTION' => 'SMUELLER/SelfLoader-1.20.tar.gz',
        'FILES'        => q[dist/SelfLoader],
        'EXCLUDED'     => ['t/00pod.t'],
    },

    'Socket' => {
        'DISTRIBUTION' => 'PEVANS/Socket-2.013.tar.gz',
        'FILES'        => q[cpan/Socket],
    },

    'Storable' => {
        'DISTRIBUTION' => 'AMS/Storable-2.45.tar.gz',
        'FILES'        => q[dist/Storable],
    },

    'Sys::Syslog' => {
        'DISTRIBUTION' => 'SAPER/Sys-Syslog-0.33.tar.gz',
        'FILES'        => q[cpan/Sys-Syslog],
        'EXCLUDED'     => [
            qr{^eg/},
            qw( README.win32
                t/data-validation.t
                t/distchk.t
                t/pod.t
                t/podcover.t
                t/podspell.t
                t/portfs.t
                win32/PerlLog.RES
                ),
        ],
    },

    'Term::ANSIColor' => {
        'DISTRIBUTION' => 'RRA/Term-ANSIColor-4.02.tar.gz',
        'FILES'        => q[cpan/Term-ANSIColor],
        'EXCLUDED'     => [
            qr{^examples/},
            qr{^t/data/},
            qw( t/aliases-env.t
                t/critic.t
                t/minimum-version.t
                t/pod-spelling.t
                t/pod-coverage.t
                t/pod.t
                t/strict.t
                t/synopsis.t
                ),
        ],
    },

    'Term::Cap' => {
        'DISTRIBUTION' => 'JSTOWE/Term-Cap-1.15.tar.gz',
        'FILES'        => q[cpan/Term-Cap],
    },

    'Term::Complete' => {
        'DISTRIBUTION' => 'FLORA/Term-Complete-1.402.tar.gz',
        'FILES'        => q[dist/Term-Complete],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
    },

    'Term::ReadLine' => {
        'DISTRIBUTION' => 'FLORA/Term-ReadLine-1.14.tar.gz',
        'FILES'        => q[dist/Term-ReadLine],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
    },

    'Test' => {
        'DISTRIBUTION' => 'JESSE/Test-1.26.tar.gz',
        'FILES'        => q[cpan/Test],
    },

    'Test::Harness' => {
        'DISTRIBUTION' => 'LEONT/Test-Harness-3.30.tar.gz',
        'FILES'        => q[cpan/Test-Harness],
        'EXCLUDED'     => [
            qr{^examples/},
            qr{^inc/},
            qr{^t/lib/Test/},
            qr{^xt/},
            qw( Changes-2.64
                MANIFEST.CUMMULATIVE
                NotBuild.PL
                HACKING.pod
                perlcriticrc
                t/000-load.t
                t/lib/if.pm
                ),
        ],
    },

    'Test::Simple' => {
        'DISTRIBUTION' => 'RJBS/Test-Simple-1.001002.tar.gz',
        'FILES'        => q[cpan/Test-Simple],
        'EXCLUDED'     => [
            qr{^t/xt},
            qw( .perlcriticrc
                .perltidyrc
                examples/indent.pl
                examples/subtest.t
                t/00compile.t
                t/pod.t
                t/pod-coverage.t
                t/Builder/reset_outputs.t
                lib/Test/Builder/IO/Scalar.pm
                ),
        ],
    },

    'Text::Abbrev' => {
        'DISTRIBUTION' => 'FLORA/Text-Abbrev-1.02.tar.gz',
        'FILES'        => q[dist/Text-Abbrev],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
    },

    'Text::Balanced' => {
        'DISTRIBUTION' => 'ADAMK/Text-Balanced-2.02.tar.gz',
        'FILES'        => q[cpan/Text-Balanced],
        'EXCLUDED'     => [
            qw( t/97_meta.t
                t/98_pod.t
                t/99_pmv.t
                ),
        ],

        # Waiting to be merged upstream: see CPAN RT#87788
        'CUSTOMIZED'   => [
            qw( t/01_compile.t
                t/02_extbrk.t
                t/03_extcbk.t
                t/04_extdel.t
                t/05_extmul.t
                t/06_extqlk.t
                t/07_exttag.t
                t/08_extvar.t
                t/09_gentag.t
                ),
        ],

    },

    'Text::ParseWords' => {
        'DISTRIBUTION' => 'CHORNY/Text-ParseWords-3.29.tar.gz',
        'FILES'        => q[cpan/Text-ParseWords],

        # Waiting to be merged upstream: see CPAN RT#50929
        'CUSTOMIZED'   => [
            qw( t/ParseWords.t
                t/taint.t
                ),
        ],

        # For the benefit of make_ext.pl, we have to have this accessible:
        'MAP' => {
            'ParseWords.pm' => 'cpan/Text-ParseWords/lib/Text/ParseWords.pm',
            ''              => 'cpan/Text-ParseWords/',
        },
    },

    'Text-Tabs+Wrap' => {
        'DISTRIBUTION' => 'MUIR/modules/Text-Tabs+Wrap-2013.0523.tar.gz',
        'FILES'        => q[cpan/Text-Tabs],
        'EXCLUDED'   => [
            qr/^lib\.old/,
            't/dnsparks.t',    # see af6492bf9e
        ],
        'MAP'          => {
            ''                        => 'cpan/Text-Tabs/',
            'lib.modern/Text/Tabs.pm' => 'cpan/Text-Tabs/lib/Text/Tabs.pm',
            'lib.modern/Text/Wrap.pm' => 'cpan/Text-Tabs/lib/Text/Wrap.pm',
        },
    },

    # Jerry Hedden does take patches that are applied to blead first, even
    # though that can be hard to discern from the Git history; so it's
    # correct for this (and Thread::Semaphore, threads, and threads::shared)
    # to be under dist/ rather than cpan/
    'Thread::Queue' => {
        'DISTRIBUTION' => 'JDHEDDEN/Thread-Queue-3.05.tar.gz',
        'FILES'        => q[dist/Thread-Queue],
        'EXCLUDED'     => [
            qr{^examples/},
            qw( t/00_load.t
                t/99_pod.t
                t/test.pl
                ),
        ],
    },

    'Thread::Semaphore' => {
        'DISTRIBUTION' => 'JDHEDDEN/Thread-Semaphore-2.12.tar.gz',
        'FILES'        => q[dist/Thread-Semaphore],
        'EXCLUDED'     => [
            qw( examples/semaphore.pl
                t/00_load.t
                t/99_pod.t
                t/test.pl
                ),
        ],
    },

    'threads' => {
        'DISTRIBUTION' => 'JDHEDDEN/threads-1.92.tar.gz',
        'FILES'        => q[dist/threads],
        'EXCLUDED'     => [
            qr{^examples/},
            qw( t/pod.t
                t/test.pl
                threads.h
                ),
        ],
    },

    'threads::shared' => {
        'DISTRIBUTION' => 'JDHEDDEN/threads-shared-1.46.tar.gz',
        'FILES'        => q[dist/threads-shared],
        'EXCLUDED'     => [
            qw( examples/class.pl
                shared.h
                t/pod.t
                t/test.pl
                ),
        ],
    },

    'Tie::File' => {
        'DISTRIBUTION' => 'TODDR/Tie-File-0.98.tar.gz',
        'FILES'        => q[dist/Tie-File],
    },

    'Tie::RefHash' => {
        'DISTRIBUTION' => 'FLORA/Tie-RefHash-1.39.tar.gz',
        'FILES'        => q[cpan/Tie-RefHash],
    },

    'Time::HiRes' => {
        'DISTRIBUTION' => 'ZEFRAM/Time-HiRes-1.9726.tar.gz',
        'FILES'        => q[cpan/Time-HiRes],
    },

    'Time::Local' => {
        'DISTRIBUTION' => 'DROLSKY/Time-Local-1.2300.tar.gz',
        'FILES'        => q[cpan/Time-Local],
        'EXCLUDED'     => [
            qr{^t/release-.*\.t},
        ],
    },

    'Time::Piece' => {
        'DISTRIBUTION' => 'RJBS/Time-Piece-1.27.tar.gz',
        'FILES'        => q[cpan/Time-Piece],
    },

    'Unicode::Collate' => {
        'DISTRIBUTION' => 'SADAHIRO/Unicode-Collate-1.04.tar.gz',
        'FILES'        => q[cpan/Unicode-Collate],
        'EXCLUDED'     => [
            qr{N$},
            qr{^data/},
            qr{^gendata/},
            qw( disableXS
                enableXS
                mklocale
                ),
        ],
    },

    'Unicode::Normalize' => {
        'DISTRIBUTION' => 'SADAHIRO/Unicode-Normalize-1.17.tar.gz',
        'FILES'        => q[cpan/Unicode-Normalize],
        'EXCLUDED'     => [
            qw( MANIFEST.N
                Normalize.pmN
                disableXS
                enableXS
                ),
        ],
    },

    'version' => {
        'DISTRIBUTION' => 'JPEACOCK/version-0.9909.tar.gz',
        'FILES'        => q[cpan/version vutil.c vutil.h vxs.inc],
        'EXCLUDED' => [
            qr{^vutil/lib/},
            'vutil/Makefile.PL',
            'vutil/ppport.h',
            'vutil/vxs.xs',
            't/survey_locales',
        ],

        # When adding the CPAN-distributed files for version.pm, it is necessary
        # to delete an entire block out of lib/version.pm, since that code is
        # only necessary with the CPAN release.
        'CUSTOMIZED'   => [
            qw( lib/version.pm
                ),
        ],

        'MAP' => {
            'vperl/'         => 'cpan/version/lib/version/',
            'vutil/'         => '',
            ''               => 'cpan/version/',
        },
    },

    'warnings' => {
        'FILES'      => q[
                 lib/warnings
                 lib/warnings.{pm,t}
                 regen/warnings.pl
                 t/lib/warnings
        ],
    },

    'Win32' => {
        'DISTRIBUTION' => "JDB/Win32-0.49.tar.gz",
        'FILES'        => q[cpan/Win32],
    },

    'Win32API::File' => {
        'DISTRIBUTION' => 'CHORNY/Win32API-File-0.1201.tar.gz',
        'FILES'        => q[cpan/Win32API-File],
        'EXCLUDED'     => [
            qr{^ex/},
            't/pod.t',
        ],
    },

    'XSLoader' => {
        'DISTRIBUTION' => 'SAPER/XSLoader-0.16.tar.gz',
        'FILES'        => q[dist/XSLoader],
        'EXCLUDED'     => [
            qr{^eg/},
            qw( t/00-load.t
                t/01-api.t
                t/distchk.t
                t/pod.t
                t/podcover.t
                t/portfs.t
                ),
            'XSLoader.pm',    # we use XSLoader_pm.PL
        ],
    },

    # this pseudo-module represents all the files under ext/ and lib/
    # that aren't otherwise claimed. This means that the following two
    # commands will check that every file under ext/ and lib/ is
    # accounted for, and that there are no duplicates:
    #
    #    perl Porting/Maintainers --checkmani lib ext
    #    perl Porting/Maintainers --checkmani

    '_PERLLIB' => {
        'FILES'    => q[
                ext/B/
                ext/Devel-Peek/
                ext/DynaLoader/
                ext/Errno/
                ext/ExtUtils-Miniperl/
                ext/Fcntl/
                ext/File-DosGlob/
                ext/File-Find/
                ext/File-Glob/
                ext/FileCache/
                ext/GDBM_File/
                ext/Hash-Util-FieldHash/
                ext/Hash-Util/
                ext/I18N-Langinfo/
                ext/IPC-Open3/
                ext/NDBM_File/
                ext/ODBM_File/
                ext/Opcode/
                ext/POSIX/
                ext/PerlIO-encoding/
                ext/PerlIO-mmap/
                ext/PerlIO-scalar/
                ext/PerlIO-via/
                ext/Pod-Functions/
                ext/Pod-Html/
                ext/SDBM_File/
                ext/Sys-Hostname/
                ext/Tie-Hash-NamedCapture/
                ext/Tie-Memoize/
                ext/VMS-DCLsym/
                ext/VMS-Filespec/
                ext/VMS-Stdio/
                ext/Win32CORE/
                ext/XS-APItest/
                ext/XS-Typemap/
                ext/arybase/
                ext/attributes/
                ext/mro/
                ext/re/
                lib/AnyDBM_File.{pm,t}
                lib/Benchmark.{pm,t}
                lib/B/Deparse{.pm,.t,-core.t}
                lib/CORE.pod
                lib/Class/Struct.{pm,t}
                lib/Config.t
                lib/Config/Extensions.{pm,t}
                lib/DB.{pm,t}
                lib/DBM_Filter.pm
                lib/DBM_Filter/
                lib/DirHandle.{pm,t}
                lib/English.{pm,t}
                lib/ExtUtils/Embed.pm
                lib/ExtUtils/XSSymSet.pm
                lib/ExtUtils/t/Embed.t
                lib/ExtUtils/typemap
                lib/File/Basename.{pm,t}
                lib/File/Compare.{pm,t}
                lib/File/Copy.{pm,t}
                lib/File/stat{.pm,.t,-7896.t}
                lib/FileHandle.{pm,t}
                lib/FindBin.{pm,t}
                lib/Getopt/Std.{pm,t}
                lib/Internals.t
                lib/Net/hostent.{pm,t}
                lib/Net/netent.{pm,t}
                lib/Net/protoent.{pm,t}
                lib/Net/servent.{pm,t}
                lib/PerlIO.pm
                lib/Pod/t/InputObjects.t
                lib/Pod/t/Select.t
                lib/Pod/t/Usage.t
                lib/Pod/t/utils.t
                lib/SelectSaver.{pm,t}
                lib/Symbol.{pm,t}
                lib/Thread.{pm,t}
                lib/Tie/Array.pm
                lib/Tie/Array/
                lib/Tie/ExtraHash.t
                lib/Tie/Handle.pm
                lib/Tie/Handle/
                lib/Tie/Hash.{pm,t}
                lib/Tie/Scalar.{pm,t}
                lib/Tie/StdHandle.pm
                lib/Tie/SubstrHash.{pm,t}
                lib/Time/gmtime.{pm,t}
                lib/Time/localtime.{pm,t}
                lib/Time/tm.pm
                lib/UNIVERSAL.pm
                lib/Unicode/README
                lib/Unicode/UCD.{pm,t}
                lib/User/grent.{pm,t}
                lib/User/pwent.{pm,t}
                lib/_charnames.pm
                lib/blib.{pm,t}
                lib/bytes.{pm,t}
                lib/bytes_heavy.pl
                lib/charnames.{pm,t}
                lib/dbm_filter_util.pl
                lib/deprecate.pm
                lib/diagnostics.{pm,t}
                lib/dumpvar.{pl,t}
                lib/feature.{pm,t}
                lib/feature/
                lib/filetest.{pm,t}
                lib/h2ph.t
                lib/h2xs.t
                lib/integer.{pm,t}
                lib/less.{pm,t}
                lib/locale.{pm,t}
                lib/open.{pm,t}
                lib/overload/numbers.pm
                lib/overloading.{pm,t}
                lib/overload{.pm,.t,64.t}
                lib/perl5db.{pl,t}
                lib/perl5db/
                lib/sigtrap.{pm,t}
                lib/sort.{pm,t}
                lib/strict.{pm,t}
                lib/subs.{pm,t}
                lib/unicore/
                lib/utf8.{pm,t}
                lib/utf8_heavy.pl
                lib/vars{.pm,.t,_carp.t}
                lib/vmsish.{pm,t}
                ],
    },
    'openbsd' => {
        'FILES'      => q[lib/Config_git.pl],
    },
);

# legacy CPAN flag
for ( values %Modules ) {
    $_->{CPAN} = !!$_->{DISTRIBUTION};
}

# legacy UPSTREAM flag
for ( keys %Modules ) {
    # Keep any existing UPSTREAM flag so that "overrides" can be applied
    next if exists $Modules{$_}{UPSTREAM};

    if ($_ eq '_PERLLIB' or $Modules{$_}{FILES} =~ m{^\s*(?:dist|ext|lib)/}) {
        $Modules{$_}{UPSTREAM} = 'blead';
    }
    elsif ($Modules{$_}{FILES} =~ m{^\s*cpan/}) {
        $Modules{$_}{UPSTREAM} = 'cpan';
    }
    else {
        warn "Unexpected location of FILES for module $_: $Modules{$_}{FILES}";
    }
}

# legacy MAINTAINER field
for ( keys %Modules ) {
    # Keep any existing MAINTAINER flag so that "overrides" can be applied
    next if exists $Modules{$_}{MAINTAINER};

    if ($Modules{$_}{UPSTREAM} eq 'blead') {
        $Modules{$_}{MAINTAINER} = 'P5P';
        $Maintainers{P5P} = 'perl5-porters <perl5-porters@perl.org>';
    }
    elsif (exists $Modules{$_}{DISTRIBUTION}) {
        (my $pause_id = $Modules{$_}{DISTRIBUTION}) =~ s{/.*$}{};
        $Modules{$_}{MAINTAINER} = $pause_id;
        $Maintainers{$pause_id} = "<$pause_id\@cpan.org>";
    }
    else {
        warn "No DISTRIBUTION for non-blead module $_";
    }
}

1;
