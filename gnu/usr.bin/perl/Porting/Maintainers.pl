# A simple listing of core files that have specific maintainers,
# or at least someone that can be called an "interested party".
# Also, a "module" does not necessarily mean a CPAN module, it
# might mean a file or files or a subdirectory.
# Most (but not all) of the modules have dual lives in the core
# and in CPAN.

package Maintainers;

use utf8;
use File::Glob qw(:case);

%Maintainers = (
    'abergman'  => 'Arthur Bergman <abergman@cpan.org>',
    'abigail'   => 'Abigail <abigail@abigail.be>',
    'ambs'      => 'Alberto Simões <ambs@cpan.org>',
    'ams'       => 'Abhijit Menon-Sen <ams@cpan.org>',
    'andk'      => 'Andreas J. Koenig <andk@cpan.org>',
    'andya'     => 'Andy Armstrong <andy@hexten.net>',
    'arandal'   => 'Allison Randal <allison@perl.org>',
    'audreyt'   => 'Audrey Tang <cpan@audreyt.org>',
    'avar'      => 'Ævar Arnfjörð Bjarmason <avar@cpan.org>',
    'bingos'    => 'Chris Williams <chris@bingosnet.co.uk>',
    'chorny'    => 'Alexandr Ciornii <alexchorny@gmail.com>',
    'corion'    => 'Max Maischein <corion@corion.net>',
    'craig'     => 'Craig Berry <craigberry@mac.com>',
    'dankogai'  => 'Dan Kogai <dankogai@cpan.org>',
    'dconway'   => 'Damian Conway <dconway@cpan.org>',
    'dland'     => 'David Landgren <dland@cpan.org>',
    'dmanura'   => 'David Manura <dmanura@cpan.org>',
    'drolsky'   => 'Dave Rolsky <drolsky@cpan.org>',
    'elizabeth' => 'Elizabeth Mattijsen <liz@dijkmat.nl>',
    'ferreira'  => 'Adriano Ferreira <ferreira@cpan.org>',
    'gbarr'     => 'Graham Barr <gbarr@cpan.org>',
    'gaas'      => 'Gisle Aas <gaas@cpan.org>',
    'gsar'      => 'Gurusamy Sarathy <gsar@activestate.com>',
    'ilyam'     => 'Ilya Martynov <ilyam@cpan.org>',
    'ilyaz'     => 'Ilya Zakharevich <ilyaz@cpan.org>',
    'jand'      => 'Jan Dubois <jand@activestate.com>',
    'jdhedden'  => 'Jerry D. Hedden <jdhedden@cpan.org>',
    'jesse'     => 'Jesse Vincent <jesse@bestpractical.com>',
    'jhi'       => 'Jarkko Hietaniemi <jhi@cpan.org>',
    'jjore'     => 'Joshua ben Jore <jjore@cpan.org>',
    'jpeacock'  => 'John Peacock <jpeacock@cpan.org>',
    'jstowe'    => 'Jonathan Stowe <jstowe@cpan.org>',
    'jv'        => 'Johan Vromans <jv@cpan.org>',
    'kane'      => 'Jos Boumans <kane@cpan.org>',
    'kwilliams' => 'Ken Williams <kwilliams@cpan.org>',
    'laun'      => 'Wolfgang Laun <Wolfgang.Laun@alcatel.at>',
    'lstein'    => 'Lincoln D. Stein <lds@cpan.org>',
    'lwall'     => 'Larry Wall <lwall@cpan.org>',
    'makamaka'  => 'Makamaka Hannyaharamitu <makamaka@cpan.org>',
    'mallen'    => 'Mark Allen <mrallen1@yahoo.com>',
    'marekr'    => 'Marek Rouchal <marekr@cpan.org>',
    'markm'     => 'Mark Mielke <markm@cpan.org>',
    'mhx'       => 'Marcus Holland-Moritz <mhx@cpan.org>',
    'mjd'       => 'Mark-Jason Dominus <mjd@plover.com>',
    'msergeant' => 'Matt Sergeant <msergeant@cpan.org>',
    'mshelor'   => 'Mark Shelor <mshelor@cpan.org>',
    'muir'      => 'David Muir Sharnoff <muir@cpan.org>',
    'neilb'     => 'Neil Bowers <neilb@cpan.org>',
    'nuffin'    => 'Yuval Kogman <nothingmuch@woobling.org>',
    'nwclark'   => 'Nicholas Clark <nwclark@cpan.org>',
    'osfameron' => 'Hakim Cassimally <osfameron@perl.org>',
    'p5p'       => 'perl5-porters <perl5-porters@perl.org>',
    'perlfaq'   => 'perlfaq-workers <perlfaq-workers@perl.org>',
    'petdance'  => 'Andy Lester <andy@petdance.com>',
    'pevans'    => 'Paul Evans <leonerd@leonerd.org.uk>',
    'pjf'       => 'Paul Fenwick <pjf@cpan.org>',
    'pmqs'      => 'Paul Marquess <pmqs@cpan.org>',
    'pvhp'      => 'Peter Prymmer <pvhp@best.com>',
    'rafl'      => 'Florian Ragwitz <flora@cpan.org>',
    'rclamp'    => 'Richard Clamp <rclamp@cpan.org>',
    'rgarcia'   => 'Rafael Garcia-Suarez <rgarcia@cpan.org>',
    'rkobes'    => 'Randy Kobes <rkobes@cpan.org>',
    'rmbarker'  => 'Robin Barker <rmbarker@cpan.org>',
    'rra'       => 'Russ Allbery <rra@cpan.org>',
    'rurban'    => 'Reini Urban <rurban@cpan.org>',
    'sadahiro'  => 'SADAHIRO Tomoyuki <SADAHIRO@cpan.org>',
    'salva'     => 'Salvador Fandiño García <salva@cpan.org>',
    'saper'     => 'Sébastien Aperghis-Tramoni <saper@cpan.org>',
    'sartak'    => 'Shawn M Moore <sartak@gmail.com>',
    'sbeck'     => 'Sullivan Beck <sbeck@cpan.org>',
    'sburke'    => 'Sean Burke <sburke@cpan.org>',
    'mschwern'  => 'Michael Schwern <mschwern@cpan.org>',
    'simonw'    => 'Simon Wistow <simonw@cpan.org>',
    'smccam'    => 'Stephen McCamant <smccam@cpan.org>',
    'smpeters'  => 'Steve Peters <steve@fisharerojo.org>',
    'smueller'  => 'Steffen Mueller <smueller@cpan.org>',
    'tomhughes' => 'Tom Hughes <tomhughes@cpan.org>',
    'tjenness'  => 'Tim Jenness <tjenness@cpan.org>',
    'tyemq'     => 'Tye McQueen <tyemq@cpan.org>',
    'yves'      => 'Yves Orton <yves@cpan.org>',
    'zefram'    => 'Andrew Main <zefram@cpan.org>',
);

# IGNORABLE: files which, if they appear in the root of a CPAN
# distribution, need not appear in core (i.e. core-cpan-diff won't
# complain if it can't find them)

@IGNORABLE = qw(
    .cvsignore .dualLivedDiffConfig .gitignore
    ANNOUNCE Announce Artistic AUTHORS BENCHMARK BUGS Build.PL
    CHANGELOG ChangeLog CHANGES Changes COPYING Copying CREDITS dist.ini
    GOALS HISTORY INSTALL INSTALL.SKIP LICENSE Makefile.PL
    MANIFEST MANIFEST.SKIP META.yml MYMETA.yml META.json MYMETA.json
    NEW NOTES perlcritic.rc ppport.h README README.PATCHING SIGNATURE
    THANKS TODO Todo VERSION WHATSNEW
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
#
# FILES is a list of filenames, glob patterns, and directory
# names to be recursed down, which collectively generate a complete list
# of the files associated with the distribution.

# UPSTREAM indicates where patches should go. undef implies
# that this hasn't been discussed for the module at hand.
# "blead" indicates that the copy of the module in the blead
# sources is to be considered canonical, "cpan" means that the
# module on CPAN is to be patched first. "first-come" means
# that blead can be patched freely if it is in sync with the
# latest release on CPAN.

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
# path should be relative to the distribution directory.

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

    'AnyDBM_File' => {
        'MAINTAINER'  => 'p5p',
        'FILES'       => q[lib/AnyDBM_File.{pm,t}],
        'UPSTREAM'    => 'blead',
    },

    'Archive::Extract' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Archive-Extract-0.58.tar.gz',
        'FILES'        => q[cpan/Archive-Extract],
        'UPSTREAM'     => 'cpan',
        'BUGS'         => 'bug-archive-extract@rt.cpan.org',
    },

    'Archive::Tar' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Archive-Tar-1.82.tar.gz',
        'FILES'        => q[cpan/Archive-Tar],
        'EXCLUDED'     => ['Makefile.PL'],
        'UPSTREAM'     => 'cpan',
        'BUGS'         => 'bug-archive-tar@rt.cpan.org',
    },

    'Attribute::Handlers' => {
        'MAINTAINER'   => 'rgarcia',
        'DISTRIBUTION' => 'SMUELLER/Attribute-Handlers-0.93.tar.gz',
        'FILES'        => q[dist/Attribute-Handlers],
        'UPSTREAM'     => 'blead',
    },

    'attributes' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/attributes],
        'UPSTREAM'   => 'blead',
    },

    'autodie' => {
        'MAINTAINER'   => 'pjf',
        'DISTRIBUTION' => 'PJF/autodie-2.10.tar.gz',
        'FILES'        => q[cpan/autodie],
        'EXCLUDED'     => [
            qr{^inc/Module/},

            # All these tests depend upon external
            # modules that don't exist when we're
            # building the core.  Hence, they can
            # never run, and should not be merged.
            qw( t/boilerplate.t
                t/critic.t
                t/fork.t
                t/kwalitee.t
                t/lex58.t
                t/pod-coverage.t
                t/pod.t
                t/socket.t
                t/system.t
                )
        ],
        'UPSTREAM'   => 'cpan',
        'CUSTOMIZED' => ['t/open.t'],
    },

    'AutoLoader' => {
        'MAINTAINER'   => 'smueller',
        'DISTRIBUTION' => 'SMUELLER/AutoLoader-5.72.tar.gz',
        'FILES'        => q[cpan/AutoLoader],
        'EXCLUDED'     => ['t/00pod.t'],
        'UPSTREAM'     => 'cpan',
    },

    'autouse' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/autouse-1.06.tar.gz',
        'FILES'        => q[dist/autouse],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'B' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/B],
        'EXCLUDED'   => [
            qw( B/Concise.pm
                t/concise.t
                ),
        ],
        'UPSTREAM' => 'blead',
    },

    'B::Concise' => {
        'MAINTAINER' => 'smccam',
        'FILES'      => q[ext/B/B/Concise.pm ext/B/t/concise.t],
        'UPSTREAM'   => 'blead',
    },

    'B::Debug' => {
        'MAINTAINER'   => 'rurban',
        'DISTRIBUTION' => 'RURBAN/B-Debug-1.17.tar.gz',
        'FILES'        => q[cpan/B-Debug],
        'EXCLUDED'     => ['t/pod.t'],
        'UPSTREAM'     => 'cpan',
    },

    'B::Deparse' => {
        'MAINTAINER' => 'smccam',
        'FILES'      => q[dist/B-Deparse],
        'UPSTREAM'   => 'blead',
    },

    'B::Lint' => {
        'MAINTAINER'   => 'jjore',
        'DISTRIBUTION' => 'FLORA/B-Lint-1.13.tar.gz',
        'FILES'        => q[dist/B-Lint],
        'EXCLUDED'     => ['t/test.pl'],
        'UPSTREAM'     => 'blead',
    },

    'base' => {
        'MAINTAINER'   => 'rgarcia',
        'DISTRIBUTION' => 'RGARCIA/base-2.15.tar.gz',
        'FILES'        => q[dist/base],
        'UPSTREAM'     => 'blead',
    },

    'Benchmark' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/Benchmark.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'bignum' => {
        'MAINTAINER'   => 'rafl',
        'DISTRIBUTION' => 'FLORA/bignum-0.29.tar.gz',
        'FILES'        => q[dist/bignum],
        'EXCLUDED'     => [
            qr{^inc/Module/},
            qw( t/pod.t
                t/pod_cov.t
                ),
        ],
        'UPSTREAM' => 'blead',
    },

    'Carp' => {
        'MAINTAINER'   => 'zefram',
        'DISTRIBUTION' => 'ZEFRAM/Carp-1.25.tar.gz',
        'FILES'        => q[dist/Carp],
        'UPSTREAM'     => 'blead',
    },

    'CGI' => {
        'MAINTAINER'   => 'lstein',
        'DISTRIBUTION' => 'MARKSTOS/CGI.pm-3.59.tar.gz',
        'FILES'        => q[cpan/CGI],
        'EXCLUDED'     => [
            qr{^t/lib/Test},
            qw( cgi-lib_porting.html
                cgi_docs.html
                examples/WORLD_WRITABLE/18.157.1.253.sav
                t/gen-tests/gen-start-end-tags.pl
                t/fast.t
                ),
        ],
        'UPSTREAM'   => 'cpan',
    },

    'Class::Struct' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/Class/Struct.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'Compress::Raw::Bzip2' => {
        'MAINTAINER'   => 'pmqs',
        'DISTRIBUTION' => 'PMQS/Compress-Raw-Bzip2-2.048.tar.gz',
        'FILES'        => q[cpan/Compress-Raw-Bzip2],
        'EXCLUDED'     => [
            qr{^t/Test/},
            'bzip2-src/bzip2-cpp.patch',
        ],
        'UPSTREAM' => 'cpan',
    },

    'Compress::Raw::Zlib' => {
        'MAINTAINER'   => 'pmqs',
        'DISTRIBUTION' => 'PMQS/Compress-Raw-Zlib-2.048.tar.gz',

        'FILES'    => q[cpan/Compress-Raw-Zlib],
        'EXCLUDED' => [
            qr{^t/Test/},
            qw( t/000prereq.t
                t/99pod.t
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'constant' => {
        'MAINTAINER'   => 'saper',
        'DISTRIBUTION' => 'SAPER/constant-1.21.tar.gz',
        'FILES'        => q[dist/constant],
        'EXCLUDED'     => [
            qw( t/00-load.t
                t/more-tests.t
                t/pod-coverage.t
                t/pod.t
                eg/synopsis.pl
                ),
        ],
        'UPSTREAM' => 'blead',
    },

    'CPAN' => {
        'MAINTAINER'   => 'andk',
        'DISTRIBUTION' => 'ANDK/CPAN-1.9800.tar.gz',
        'FILES'        => q[cpan/CPAN],
        'EXCLUDED'     => [
            qr{^distroprefs/},
            qr{^inc/Test/},
            qr{^t/CPAN/authors/},
            qw( lib/CPAN/Admin.pm
                Makefile.PL
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
                t/50pod.t
                t/51pod.t
                t/52podcover.t
                t/60credentials.t
                t/70_critic.t
                t/CPAN/CpanTestDummies-1.55.pm
                t/CPAN/TestConfig.pm
                t/CPAN/TestMirroredBy
                t/CPAN/TestPatch.txt
                t/CPAN/modules/02packages.details.txt
                t/CPAN/modules/03modlist.data
                t/data/META-dynamic.yml
                t/data/META-static.yml
                t/local_utils.pm
                t/perlcriticrc
                t/yaml_code.yml
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'CPANPLUS' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/CPANPLUS-0.9121.tar.gz',
        'FILES'        => q[cpan/CPANPLUS],
        'EXCLUDED'     => [
            qr{^inc/},
            qr{^t/dummy-.*\.hidden$},
            'bin/cpanp-boxed',

            # SQLite tests would be skipped in core, and
            # the filenames are too long for VMS!
            qw( t/031_CPANPLUS-Internals-Source-SQLite.t
                t/032_CPANPLUS-Internals-Source-via-sqlite.t
                ),
            'Makefile.PL',
        ],
        'CUSTOMIZED' => ['Makefile.PL'],
        'UPSTREAM'   => 'cpan',
        'BUGS'       => 'bug-cpanplus@rt.cpan.org',
    },

    'CPANPLUS::Dist::Build' => {
        'MAINTAINER'   => 'bingos',
        'DISTRIBUTION' => 'BINGOS/CPANPLUS-Dist-Build-0.62.tar.gz',
        'FILES'        => q[cpan/CPANPLUS-Dist-Build],
        'EXCLUDED'     => [
            qr{^inc/},
            qw( t/99_pod.t
                t/99_pod_coverage.t
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'CPAN::Meta' => {
        'MAINTAINER'   => 'dagolden',
        'DISTRIBUTION' => 'DAGOLDEN/CPAN-Meta-2.120630.tar.gz',
        'FILES'        => q[cpan/CPAN-Meta],
        'EXCLUDED'     => [
            qw(t/00-compile.t),
            qr{^xt},
            qr{^history},
        ],
        'UPSTREAM' => 'cpan',
    },

    'CPAN::Meta::YAML' => {
        'MAINTAINER'   => 'dagolden',
        'DISTRIBUTION' => 'DAGOLDEN/CPAN-Meta-YAML-0.007.tar.gz',
        'FILES'        => q[cpan/CPAN-Meta-YAML],
        'EXCLUDED'     => [
            't/00-compile.t',
            't/04_scalar.t',    # requires YAML.pm
            qr{^xt},
        ],
        'UPSTREAM' => 'cpan',
    },

    'Data::Dumper' => {
        'MAINTAINER' => 'p5p',    # Not gsar. Not ilyam. Not really smueller
        'DISTRIBUTION' => 'SMUELLER/Data-Dumper-2.135_01.tar.gz',
        'FILES'        => q[dist/Data-Dumper],
        'UPSTREAM'     => 'blead',
    },

    'DB_File' => {
        'MAINTAINER'   => 'pmqs',
        'DISTRIBUTION' => 'PMQS/DB_File-1.826.tar.gz',
        'FILES'        => q[cpan/DB_File],
        'EXCLUDED'     => [
            qr{^patches/},
            qw( t/pod.t
                fallback.h
                fallback.xs
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'DBM_Filter' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/DBM_Filter.pm lib/DBM_Filter],
        'UPSTREAM'   => 'blead',
    },

    'Devel::SelfStubber' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/Devel-SelfStubber-1.05.tar.gz',
        'FILES'        => q[dist/Devel-SelfStubber],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'Devel::Peek' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Devel-Peek],
        'UPSTREAM'   => 'blead',
    },

    'Devel::PPPort' => {
        'MAINTAINER'   => 'mhx',
        'DISTRIBUTION' => 'MHX/Devel-PPPort-3.20.tar.gz',
        'FILES'        => q[cpan/Devel-PPPort],
        'EXCLUDED' => ['PPPort.pm'],    # we use PPPort_pm.PL instead
        'UPSTREAM' => 'cpan',
    },

    'diagnostics' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/diagnostics.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'Digest' => {
        'MAINTAINER'   => 'gaas',
        'DISTRIBUTION' => 'GAAS/Digest-1.17.tar.gz',
        'FILES'        => q[cpan/Digest],
        'EXCLUDED'     => ['digest-bench'],
        'UPSTREAM'     => "cpan",
    },

    'Digest::MD5' => {
        'MAINTAINER'   => 'gaas',
        'DISTRIBUTION' => 'GAAS/Digest-MD5-2.51.tar.gz',
        'FILES'        => q[cpan/Digest-MD5],
        'EXCLUDED'     => ['rfc1321.txt'],
        'UPSTREAM'     => "cpan",
    },

    'Digest::SHA' => {
        'MAINTAINER'   => 'mshelor',
        'DISTRIBUTION' => 'MSHELOR/Digest-SHA-5.71.tar.gz',
        'FILES'        => q[cpan/Digest-SHA],
        'EXCLUDED'     => [
            qw( t/pod.t
                t/podcover.t
                examples/dups
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'DirHandle' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/DirHandle.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'Dumpvalue' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/Dumpvalue-1.16.tar.gz',
        'FILES'        => q[dist/Dumpvalue],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'DynaLoader' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/DynaLoader],
        'UPSTREAM'   => 'blead',
    },

    'Encode' => {
        'MAINTAINER'   => 'dankogai',
        'DISTRIBUTION' => 'DANKOGAI/Encode-2.44.tar.gz',
        'FILES'        => q[cpan/Encode],
        'UPSTREAM'     => 'cpan',
    },

    'encoding::warnings' => {
        'MAINTAINER'   => 'audreyt',
        'DISTRIBUTION' => 'AUDREYT/encoding-warnings-0.11.tar.gz',
        'FILES'        => q[cpan/encoding-warnings],
        'EXCLUDED'     => [
            qr{^inc/Module/},
            qw( t/0-signature.t
                Makefile.PL
                MANIFEST
                META.yml
                README
                SIGNATURE
                ),
        ],
        'UPSTREAM' => undef,
    },

    'English' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/English.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'Env' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/Env-1.03.tar.gz',
        'FILES'        => q[dist/Env],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'Errno' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Errno],
        'UPSTREAM'   => 'blead',
    },

    'Exporter' => {
        'MAINTAINER'   => 'ferreira',
        'DISTRIBUTION' => 'TODDR/Exporter-5.66.tar.gz',
        'FILES'        => q[lib/Exporter.pm
                            lib/Exporter.t
                            lib/Exporter/Heavy.pm
                           ],
        'EXCLUDED' => [
            qw( t/pod.t
                t/use.t
                ),
        ],
        'MAP' => {
            't/'   => 'lib/',
            'lib/' => 'lib/',
        },
        'UPSTREAM' => 'blead',
    },

    'ExtUtils::CBuilder' => {
        'MAINTAINER'   => 'ambs',
        'DISTRIBUTION' => 'DAGOLDEN/ExtUtils-CBuilder-0.280205.tar.gz',
        'FILES'        => q[dist/ExtUtils-CBuilder],
        'UPSTREAM'     => 'blead',
    },

    'ExtUtils::Command' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/ExtUtils-Command-1.17.tar.gz',
        'FILES'        => q[dist/ExtUtils-Command],
        'EXCLUDED'     => [qr{^t/release-}],
        'UPSTREAM'     => 'blead',
    },

    'ExtUtils::Constant' => {
        'MAINTAINER' => 'nwclark',

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
        'UPSTREAM' => undef,
    },

    'ExtUtils::Install' => {
        'MAINTAINER'   => 'yves',
        'DISTRIBUTION' => 'YVES/ExtUtils-Install-1.54.tar.gz',
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
        'UPSTREAM' => 'blead',
    },

    'ExtUtils::MakeMaker' => {
        'MAINTAINER'   => 'mschwern',
        'DISTRIBUTION' => 'MSCHWERN/ExtUtils-MakeMaker-6.63_02.tar.gz',
        'FILES'        => q[cpan/ExtUtils-MakeMaker],
        'EXCLUDED'     => [
            qr{^t/lib/Test/},
            qr{^(bundled|my)/},
            qr{^t/Liblist_Kid.t},
            qr{^t/liblist/},
        ],
        'UPSTREAM' => 'first-come',
    },

    'ExtUtils::Manifest' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/ExtUtils-Manifest-1.60.tar.gz',
        'FILES'        => q[dist/ExtUtils-Manifest],
        'EXCLUDED'     => [qr(t/release-.*\.t)],
        'UPSTREAM'     => 'blead',
    },

    'ExtUtils::ParseXS' => {
        'MAINTAINER'   => 'smueller',
        'DISTRIBUTION' => 'SMUELLER/ExtUtils-ParseXS-3.15.tar.gz',
        'FILES'        => q[dist/ExtUtils-ParseXS],
        'UPSTREAM'     => 'blead',
    },

    'perlfaq' => {
        'MAINTAINER'   => 'perlfaq',
        'DISTRIBUTION' => 'LLAP/perlfaq-5.0150039.tar.gz',
        'FILES'        => q[cpan/perlfaq],
        'EXCLUDED'     => [
            qw( t/release-pod-syntax.t
                t/release-eol.t
                t/release-no-tabs.t
                )
        ],
        'UPSTREAM' => 'cpan',
    },

    'File::Basename' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/File/Basename.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'File::Compare' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/File/Compare.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'File::Copy' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/File/Copy.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'File::CheckTree' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/File-CheckTree-4.41.tar.gz',
        'FILES'        => q[dist/File-CheckTree],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'File::DosGlob' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/File/DosGlob.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'File::Fetch' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/File-Fetch-0.32.tar.gz',
        'FILES'        => q[cpan/File-Fetch],
        'UPSTREAM'     => 'cpan',
    },

    'File::Find' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/File/Find.pm lib/File/Find],
        'UPSTREAM'   => 'blead',
    },

    'File::Glob' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/File-Glob],
        'UPSTREAM'   => 'blead',
    },

    'File::Path' => {
        'MAINTAINER'   => 'dland',
        'DISTRIBUTION' => 'DLAND/File-Path-2.08.tar.gz',
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
        'UPSTREAM' => undef,
    },

    'File::stat' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/File/stat.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'File::Temp' => {
        'MAINTAINER'   => 'tjenness',
        'DISTRIBUTION' => 'TJENNESS/File-Temp-0.22.tar.gz',
        'FILES'        => q[cpan/File-Temp],
        'EXCLUDED'     => [
            qw( misc/benchmark.pl
                misc/results.txt
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'FileCache' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/FileCache],
        'UPSTREAM'   => 'blead',
    },

    'FileHandle' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/FileHandle.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'Filter::Simple' => {
        'MAINTAINER'   => 'smueller',
        'DISTRIBUTION' => 'SMUELLER/Filter-Simple-0.88.tar.gz',
        'FILES'        => q[dist/Filter-Simple],
        'EXCLUDED'     => [
            'Makefile.PL',
            qr{^demo/}
        ],
        'UPSTREAM' => 'blead',
    },

    'Filter::Util::Call' => {
        'MAINTAINER'   => 'pmqs',
        'DISTRIBUTION' => 'RURBAN/Filter-1.40.tar.gz',
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
                t/pod.t
                t/sh.t
                t/tee.t
                ),
        ],
        'MAP' => {
            'Call/'          => 'cpan/Filter-Util-Call/',
            'filter-util.pl' => 'cpan/Filter-Util-Call/filter-util.pl',
            'perlfilter.pod' => 'pod/perlfilter.pod',
            ''               => 'cpan/Filter-Util-Call/',
        },
        'UPSTREAM' => 'cpan',
    },

    'FindBin' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/FindBin.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'GDBM_File' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/GDBM_File],
        'UPSTREAM'   => 'blead',
    },

    'Fcntl' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Fcntl],
        'UPSTREAM'   => 'blead',
    },

    'Getopt::Long' => {
        'MAINTAINER'   => 'jv',
        'DISTRIBUTION' => 'JV/Getopt-Long-2.38.tar.gz',
        'FILES'        => q[cpan/Getopt-Long],
        'EXCLUDED'     => [
            qr{^examples/},
            qw( perl-Getopt-Long.spec
                lib/newgetopt.pl
                t/gol-compat.t
                ),
        ],
        'MAP'      => { '' => 'cpan/Getopt-Long/' },
        'UPSTREAM' => 'cpan',
    },

    'Getopt::Std' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/Getopt/Std.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'Hash::Util::FieldHash' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Hash-Util-FieldHash],
        'UPSTREAM'   => 'blead',
    },

    'Hash::Util' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Hash-Util],
        'UPSTREAM'   => 'blead',
    },

    'HTTP::Tiny' => {
        'MAINTAINER'   => 'dagolden',
        'DISTRIBUTION' => 'DAGOLDEN/HTTP-Tiny-0.017.tar.gz',
        'FILES'        => q[cpan/HTTP-Tiny],
        'EXCLUDED'     => [
            't/200_live.t',
            qr/^eg/,
            qr/^xt/
        ],
        'UPSTREAM' => 'cpan',
    },

    'I18N::Collate' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/I18N-Collate-1.02.tar.gz',
        'FILES'        => q[dist/I18N-Collate],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'I18N::Langinfo' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/I18N-Langinfo],
        'UPSTREAM'   => 'blead',
    },

    # Sean has donated it to us.
    # Nothing has changed since his last CPAN release.
    # (not strictly true: there have been some trivial typo fixes; DAPM 6/2009)
    'I18N::LangTags' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'SBURKE/I18N-LangTags-0.35.tar.gz',
        'FILES'        => q[dist/I18N-LangTags],
        'UPSTREAM'     => 'blead',
    },

    'if' => {
        'MAINTAINER'   => 'ilyaz',
        'DISTRIBUTION' => 'ILYAZ/modules/if-0.0601.tar.gz',
        'FILES'        => q[dist/if],
        'UPSTREAM'     => 'blead',
    },

    'IO' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'GBARR/IO-1.25.tar.gz',
        'FILES'        => q[dist/IO/],
        'EXCLUDED'     => ['t/test.pl'],
        'UPSTREAM'     => 'blead',
    },

    'IO-Compress' => {
        'MAINTAINER'   => 'pmqs',
        'DISTRIBUTION' => 'PMQS/IO-Compress-2.048.tar.gz',
        'FILES'        => q[cpan/IO-Compress],
        'EXCLUDED'     => [qr{t/Test/}],
        'UPSTREAM'     => 'cpan',
    },

    'IO::Zlib' => {
        'MAINTAINER'   => 'tomhughes',
        'DISTRIBUTION' => 'TOMHUGHES/IO-Zlib-1.10.tar.gz',
        'FILES'        => q[cpan/IO-Zlib],
        'UPSTREAM'     => undef,
    },

    'IPC::Cmd' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/IPC-Cmd-0.76.tar.gz',
        'FILES'        => q[cpan/IPC-Cmd],
        'UPSTREAM'     => 'cpan',
    },

    'IPC::Open3' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/IPC-Open3],
        'UPSTREAM'   => 'blead',
    },

    'IPC::SysV' => {
        'MAINTAINER'   => 'mhx',
        'DISTRIBUTION' => 'MHX/IPC-SysV-2.03.tar.gz',
        'FILES'        => q[cpan/IPC-SysV],
        'EXCLUDED'     => [
            qw( const-c.inc
                const-xs.inc
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'JSON::PP' => {
        'MAINTAINER'   => 'makamaka',
        'DISTRIBUTION' => 'MAKAMAKA/JSON-PP-2.27200.tar.gz',
        'FILES'        => q[cpan/JSON-PP],
        'EXCLUDED'     => [
            't/900_pod.t',    # Pod testing
        ],
        'UPSTREAM' => 'cpan',
    },

    'lib' => {
        'MAINTAINER'   => 'smueller',
        'DISTRIBUTION' => 'SMUELLER/lib-0.63.tar.gz',
        'FILES'        => q[dist/lib/],
        'EXCLUDED'     => [
            qw( forPAUSE/lib.pm
                t/00pod.t
                ),
        ],
        'UPSTREAM' => 'blead',
    },

    'libnet' => {
        'MAINTAINER'   => 'gbarr',
        'DISTRIBUTION' => 'GBARR/libnet-1.22.tar.gz',
        'FILES'        => q[cpan/libnet],
        'EXCLUDED'     => [
            qw( Configure
                install-nomake
                Makefile.PL
                ),
        ],
        'CUSTOMIZED' => ['Makefile.PL'],
        'UPSTREAM'   => undef,
    },

    'Locale-Codes' => {
        'MAINTAINER'   => 'sbeck',
        'DISTRIBUTION' => 'SBECK/Locale-Codes-3.21.tar.gz',
        'FILES'        => q[cpan/Locale-Codes],
        'EXCLUDED'     => [
            qw( t/pod_coverage.t
                t/pod.t),
            qr{^t/runtests},
            qr{^t/runtests\.bat},
            qr{^internal/},
            qr{^examples/},
        ],
        'UPSTREAM' => 'cpan',
    },

    'Locale::Maketext' => {
        'MAINTAINER'   => 'ferreira',
        'DISTRIBUTION' => 'TODDR/Locale-Maketext-1.22.tar.gz',
        'FILES'        => q[dist/Locale-Maketext],
        'EXCLUDED'     => [
            qw(
                perlcriticrc
                t/00_load.t
                t/pod.t
                ),
        ],
        'UPSTREAM' => 'blead',
    },

    'Locale::Maketext::Simple' => {
        'MAINTAINER'   => 'audreyt',
        'DISTRIBUTION' => 'JESSE/Locale-Maketext-Simple-0.21.tar.gz',
        'FILES'        => q[cpan/Locale-Maketext-Simple],
        'EXCLUDED'     => [qr{^inc/}],
        'UPSTREAM'     => 'cpan',
    },

    'Log::Message' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Log-Message-0.04.tar.gz',
        'FILES'        => q[cpan/Log-Message],
        'UPSTREAM'     => 'cpan',
    },

    'Log::Message::Simple' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Log-Message-Simple-0.08.tar.gz',
        'FILES'        => q[cpan/Log-Message-Simple],
        'UPSTREAM'     => 'cpan',
    },

    'mad' => {
        'MAINTAINER' => 'lwall',
        'FILES'      => q[mad],
        'UPSTREAM'   => undef,
    },

    'Math::BigInt' => {
        'MAINTAINER'   => 'rafl',
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
        'UPSTREAM' => 'blead',
    },

    'Math::BigInt::FastCalc' => {
        'MAINTAINER'   => 'rafl',
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
        'UPSTREAM' => 'blead',
    },

    'Math::BigRat' => {
        'MAINTAINER'   => 'rafl',
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
        'UPSTREAM' => 'blead',
    },

    'Math::Complex' => {
        'MAINTAINER'   => 'zefram',
        'DISTRIBUTION' => 'ZEFRAM/Math-Complex-1.59.tar.gz',
        'FILES'        => q[cpan/Math-Complex],
        'EXCLUDED'     => [
            qw( t/pod.t
                t/pod-coverage.t
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'Memoize' => {
        'MAINTAINER'   => 'mjd',
        'DISTRIBUTION' => 'FLORA/Memoize-1.02.tar.gz',
        'FILES'        => q[cpan/Memoize],
        'EXCLUDED'     => ['article.html'],
        'UPSTREAM'     => 'cpan',
    },

    'MIME::Base64' => {
        'MAINTAINER'   => 'gaas',
        'DISTRIBUTION' => 'GAAS/MIME-Base64-3.13.tar.gz',
        'FILES'        => q[cpan/MIME-Base64],
        'EXCLUDED'     => ['t/bad-sv.t'],
        'UPSTREAM'     => 'cpan',
    },

    #
    # To update Module-Build in blead see
    # https://github.com/Perl-Toolchain-Gang/Module-Build/blob/master/devtools/patching_blead.pod
    #

    'Module::Build' => {
        'MAINTAINER'   => 'kwilliams',
        'DISTRIBUTION' => 'DAGOLDEN/Module-Build-0.39_01.tar.gz',
        'FILES'        => q[cpan/Module-Build],
        'EXCLUDED'     => [
            qw( t/par.t
                t/signature.t
                ),
            qr{^contrib/},
            qr{^devtools},
            qr{^inc},
        ],
        'CUSTOMIZED' => ['lib/Module/Build/ConfigData.pm'],
        'UPSTREAM'   => 'cpan',
    },

    'Module::CoreList' => {
        'MAINTAINER'   => 'bingos',
        'DISTRIBUTION' => 'BINGOS/Module-CoreList-2.61.tar.gz',
        'FILES'        => q[dist/Module-CoreList],
        'UPSTREAM'     => 'blead',
    },

    'Module::Load' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Module-Load-0.22.tar.gz',
        'FILES'        => q[cpan/Module-Load],
        'UPSTREAM'     => 'cpan',
    },

    'Module::Load::Conditional' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Module-Load-Conditional-0.46.tar.gz',
        'FILES'        => q[cpan/Module-Load-Conditional],
        'UPSTREAM'     => 'cpan',
    },

    'Module::Loaded' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Module-Loaded-0.08.tar.gz',
        'FILES'        => q[cpan/Module-Loaded],
        'UPSTREAM'     => 'cpan',
    },

    'Module::Metadata' => {
        'MAINTAINER'   => 'dagolden',
        'DISTRIBUTION' => 'DAGOLDEN/Module-Metadata-1.000009.tar.gz',
        'FILES'        => q[cpan/Module-Metadata],
        'EXCLUDED'     => [
            qr{^maint},
            qr{^xt},
        ],
        'UPSTREAM' => 'cpan',
    },

    'Module::Pluggable' => {
        'MAINTAINER'   => 'simonw',
        'DISTRIBUTION' => 'SIMONW/Module-Pluggable-4.0.tar.gz',
        'FILES'        => q[cpan/Module-Pluggable],
        'UPSTREAM'     => 'cpan',
        'CUSTOMIZED'   => ['Makefile.PL'],
    },

    'mro' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/mro],
        'UPSTREAM'   => 'blead',
    },

    'NDBM_File' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/NDBM_File],
        'UPSTREAM'   => 'blead',
    },

    'Net::Ping' => {
        'MAINTAINER'   => 'smpeters',
        'DISTRIBUTION' => 'SMPETERS/Net-Ping-2.36.tar.gz',
        'FILES'        => q[dist/Net-Ping],
        'UPSTREAM'     => 'blead',
    },

    'NEXT' => {
        'MAINTAINER'   => 'rafl',
        'DISTRIBUTION' => 'FLORA/NEXT-0.65.tar.gz',
        'FILES'        => q[cpan/NEXT],
        'EXCLUDED'     => [qr{^demo/}],
        'UPSTREAM'     => 'cpan',
    },

    'Object::Accessor' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Object-Accessor-0.42.tar.gz',
        'FILES'        => q[cpan/Object-Accessor],
        'UPSTREAM'     => 'cpan',
    },

    'ODBM_File' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/ODBM_File],
        'UPSTREAM'   => 'blead',
    },

    'Opcode' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Opcode],
        'UPSTREAM'   => 'blead',
    },

    'overload' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/overload{.pm,.t,64.t}],
        'UPSTREAM'   => 'blead',
    },

    'Package::Constants' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'KANE/Package-Constants-0.02.tar.gz',
        'FILES'        => q[cpan/Package-Constants],
        'UPSTREAM'     => 'cpan',
    },

    'Params::Check' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Params-Check-0.32.tar.gz',
        'EXCLUDED'     => ['Params-Check-0.26.tar.gz'],
        'FILES'        => q[cpan/Params-Check],
        'UPSTREAM'     => 'cpan',
    },

    'parent' => {
        'MAINTAINER'   => 'corion',
        'DISTRIBUTION' => 'CORION/parent-0.225.tar.gz',
        'FILES'        => q[cpan/parent],
        'UPSTREAM'     => undef,
    },

    'Parse::CPAN::Meta' => {
        'MAINTAINER'   => 'dagolden',
        'DISTRIBUTION' => 'DAGOLDEN/Parse-CPAN-Meta-1.4402.tar.gz',
        'FILES'        => q[cpan/Parse-CPAN-Meta],
        'EXCLUDED'     => [],
        'UPSTREAM'     => 'cpan',
    },

    'PathTools' => {
        'MAINTAINER'   => 'kwilliams',
        'DISTRIBUTION' => 'SMUELLER/PathTools-3.33.tar.gz',
        'FILES'        => q[dist/Cwd],
        'EXCLUDED'     => [qr{^t/lib/Test/}],
        'UPSTREAM'     => "blead",

        # NOTE: PathTools is in dist/Cwd/ instead of dist/PathTools because it
        # contains Cwd.xs and something, possibly Makefile.SH, makes an assumption
        # that the leafname of some file corresponds with the pathname of the
        # directory.
    },

    'perldtrace' => {
        'MAINTAINER' => 'sartak',
        'FILES'      => q[pod/perldtrace.pod],
        'UPSTREAM'   => 'blead',
    },

    'perlebcdic' => {
        'MAINTAINER' => 'pvhp',
        'FILES'      => q[pod/perlebcdic.pod],
        'UPSTREAM'   => undef,
    },

    'PerlIO' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/PerlIO.pm],
        'UPSTREAM'   => undef,
    },

    'PerlIO::encoding' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/PerlIO-encoding],
        'UPSTREAM'   => 'blead',
    },

    'PerlIO::mmap' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/PerlIO-mmap],
        'UPSTREAM'   => 'blead',
    },

    'PerlIO::scalar' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/PerlIO-scalar],
        'UPSTREAM'   => 'blead',
    },

    'PerlIO::via' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/PerlIO-via],
        'UPSTREAM'   => 'blead',
    },

    'PerlIO::via::QuotedPrint' => {
        'MAINTAINER'   => 'elizabeth',
        'DISTRIBUTION' => 'ELIZABETH/PerlIO-via-QuotedPrint-0.06.tar.gz',
        'FILES'        => q[cpan/PerlIO-via-QuotedPrint],
        'UPSTREAM'     => undef,
    },

    'Perl::OSType' => {
        'MAINTAINER'   => 'dagolden',
        'DISTRIBUTION' => 'DAGOLDEN/Perl-OSType-1.002.tar.gz',
        'FILES'        => q[cpan/Perl-OSType],
        'EXCLUDED'     => [qr/^xt/],
        'UPSTREAM'     => 'cpan',
    },

    'perlpacktut' => {
        'MAINTAINER' => 'laun',
        'FILES'      => q[pod/perlpacktut.pod],
        'UPSTREAM'   => undef,
    },

    'perlpodspec' => {
        'MAINTAINER' => 'sburke',
        'FILES'      => q[pod/perlpodspec.pod],
        'UPSTREAM'   => undef,
    },

    'perlre' => {
        'MAINTAINER' => 'abigail',
        'FILES'      => q[pod/perlrecharclass.pod
                 pod/perlrebackslash.pod],
        'UPSTREAM' => undef,
    },

    'perlreapi' => {
        MAINTAINER => 'avar',
        FILES      => q[pod/perlreapi.pod],
        'UPSTREAM' => undef,
    },

    'perlreftut' => {
        'MAINTAINER' => 'mjd',
        'FILES'      => q[pod/perlreftut.pod],
        'UPSTREAM'   => 'blead',
    },

    'perlthrtut' => {
        'MAINTAINER' => 'elizabeth',
        'FILES'      => q[pod/perlthrtut.pod],
        'UPSTREAM'   => undef,
    },

    'Pod::Escapes' => {
        'MAINTAINER'   => 'arandal',
        'DISTRIBUTION' => 'SBURKE/Pod-Escapes-1.04.tar.gz',
        'FILES'        => q[cpan/Pod-Escapes],
        'UPSTREAM'     => undef,
    },

    'Pod::Functions' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Pod-Functions],
        'UPSTREAM'   => 'blead',
    },

    'Pod::Html' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Pod-Html],
        'UPSTREAM'   => 'blead',
    },

    'Pod::LaTeX' => {
        'MAINTAINER'   => 'tjenness',
        'DISTRIBUTION' => 'TJENNESS/Pod-LaTeX-0.60.tar.gz',
        'FILES'        => q[cpan/Pod-LaTeX],
        'EXCLUDED'     => ['t/require.t'],
        'UPSTREAM'     => undef,
    },

    'Pod::Parser' => {
        'MAINTAINER' => 'marekr',
        'DISTRIBUTION' => 'MAREKR/Pod-Parser-1.51.tar.gz',
        'FILES'        => q[cpan/Pod-Parser],
        'UPSTREAM'     => 'cpan',
    },

    'Pod::Perldoc' => {
        'MAINTAINER'   => 'mallen',
        'DISTRIBUTION' => 'MALLEN/Pod-Perldoc-3.15_15.tar.gz',
        'FILES'        => q[cpan/Pod-Perldoc],

        # in blead, the perldoc executable is generated by perldoc.PL
        # instead
        # XXX We can and should fix this, but clean up the DRY-failure in utils
        # first
        'EXCLUDED' => ['perldoc'],
        'UPSTREAM' => 'cpan',
    },

    'Pod::Simple' => {
        'MAINTAINER'   => 'arandal',
        'DISTRIBUTION' => 'DWHEELER/Pod-Simple-3.20.tar.gz',
        'FILES'        => q[cpan/Pod-Simple],
        'UPSTREAM'     => 'cpan',
    },

    'podlators' => {
        'MAINTAINER'   => 'rra',
        'DISTRIBUTION' => 'RRA/podlators-2.4.0.tar.gz',
        'FILES'        => q[cpan/podlators pod/perlpodstyle.pod],

        # The perl distribution has pod2man.PL and pod2text.PL,  which are
        # run to create pod2man and pod2text, while the CPAN distribution
        # just has the post-generated pod2man and pod2text files.
        # The following entries attempt to codify that odd fact.
        'CUSTOMIZED' => [
            qw( scripts/pod2man.PL
                scripts/pod2text.PL
                pod/perlpodstyle.pod
                ),
        ],
        'MAP' => {
            ''                 => 'cpan/podlators/',
            'scripts/pod2man'  => 'cpan/podlators/scripts/pod2man.PL',
            'scripts/pod2text' => 'cpan/podlators/scripts/pod2text.PL',

            # this file lives outside the cpan/ directory
            'pod/perlpodstyle.pod' => 'pod/perlpodstyle.pod',
        },
        'UPSTREAM' => 'cpan',
    },

    'POSIX' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/POSIX],
        'UPSTREAM'   => 'blead',
    },

    're' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/re],
        'UPSTREAM'   => 'blead',
    },

    'Safe' => {
        'MAINTAINER'   => 'rgarcia',
        'DISTRIBUTION' => 'RGARCIA/Safe-2.30.tar.gz',
        'FILES'        => q[dist/Safe],
        'UPSTREAM'     => 'blead',
    },

    'Scalar-List-Utils' => {
        'MAINTAINER'   => 'gbarr',
        'DISTRIBUTION' => 'PEVANS/Scalar-List-Utils-1.25.tar.gz',

        # Note that perl uses its own version of Makefile.PL
        'FILES'    => q[cpan/List-Util],
        'EXCLUDED' => [
            qr{^inc/Module/},
            qr{^inc/Test/},
            'mytypemap',
        ],
        'UPSTREAM' => 'cpan',
    },

    'SDBM_File' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/SDBM_File],
        'UPSTREAM'   => 'blead',
    },

    'Search::Dict' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/Search-Dict-1.03.tar.gz',
        'FILES'        => q[dist/Search-Dict],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'SelfLoader' => {
        'MAINTAINER'   => 'smueller',
        'DISTRIBUTION' => 'SMUELLER/SelfLoader-1.20.tar.gz',
        'FILES'        => q[dist/SelfLoader],
        'EXCLUDED'     => ['t/00pod.t'],
        'UPSTREAM'     => 'blead',
    },

    'sigtrap' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/sigtrap.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'Socket' => {
        'MAINTAINER'   => 'pevans',
        'DISTRIBUTION' => 'PEVANS/Socket-2.001.tar.gz',
        'FILES'        => q[cpan/Socket],
        'UPSTREAM'     => 'cpan',
    },

    'Storable' => {
        'MAINTAINER'   => 'ams',
        'DISTRIBUTION' => 'AMS/Storable-2.30.tar.gz',
        'FILES'        => q[dist/Storable],
        'EXCLUDED'     => [qr{^t/Test/}],
        'UPSTREAM'     => 'blead',
    },

    'Sys::Hostname' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Sys-Hostname],
        'UPSTREAM'   => 'blead',
    },

    'Sys::Syslog' => {
        'MAINTAINER'   => 'saper',
        'DISTRIBUTION' => 'SAPER/Sys-Syslog-0.29.tar.gz',
        'FILES'        => q[cpan/Sys-Syslog],
        'EXCLUDED'     => [
            qr{^eg/},
            qw( t/data-validation.t
                t/distchk.t
                t/pod.t
                t/podcover.t
                t/podspell.t
                t/portfs.t
                win32/PerlLog.RES
                win32/PerlLog_RES.uu
                ),
        ],
        'UPSTREAM'   => 'cpan',
        'CUSTOMIZED' => ['t/syslog.t'],
    },

    'Term::ANSIColor' => {
        'MAINTAINER'   => 'rra',
        'DISTRIBUTION' => 'RRA/Term-ANSIColor-3.01.tar.gz',
        'FILES'        => q[cpan/Term-ANSIColor],
        'EXCLUDED'     => [
            qr{^tests/},
            qw( t/pod-spelling.t
                t/pod.t
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'Term::Cap' => {
        'MAINTAINER'   => 'jstowe',
        'DISTRIBUTION' => 'JSTOWE/Term-Cap-1.12.tar.gz',
        'FILES'        => q[cpan/Term-Cap],
        'UPSTREAM'     => undef,
    },

    'Term::Complete' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/Term-Complete-1.402.tar.gz',
        'FILES'        => q[dist/Term-Complete],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'Term::ReadLine' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/Term-ReadLine-1.07.tar.gz',
        'FILES'        => q[dist/Term-ReadLine],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'Term::UI' => {
        'MAINTAINER'   => 'kane',
        'DISTRIBUTION' => 'BINGOS/Term-UI-0.30.tar.gz',
        'FILES'        => q[cpan/Term-UI],
        'UPSTREAM'     => 'cpan',
    },

    'Test' => {
        'MAINTAINER'   => 'jesse',
        'DISTRIBUTION' => 'JESSE/Test-1.25_02.tar.gz',
        'FILES'        => q[cpan/Test],
        'UPSTREAM'     => 'cpan',
    },

    'Test::Harness' => {
        'MAINTAINER'   => 'andya',
        'DISTRIBUTION' => 'ANDYA/Test-Harness-3.23.tar.gz',
        'FILES'        => q[cpan/Test-Harness],
        'EXCLUDED'     => [
            qr{^examples/},
            qr{^inc/},
            qr{^t/lib/Test/},
            qr{^xt/},
            qw( Changes-2.64
                NotBuild.PL
                HACKING.pod
                perlcriticrc
                t/lib/if.pm
                ),
        ],
        'UPSTREAM'   => 'cpan',
        'CUSTOMIZED' => [
            qw( t/source.t
                t/testargs.t
                ),
        ],
    },

    'Test::Simple' => {
        'MAINTAINER'   => 'mschwern',
        'DISTRIBUTION' => 'MSCHWERN/Test-Simple-0.98.tar.gz',
        'FILES'        => q[cpan/Test-Simple],
        'EXCLUDED'     => [
            qw( .perlcriticrc
                .perltidyrc
                t/00compile.t
                t/pod.t
                t/pod-coverage.t
                t/Builder/reset_outputs.t
                lib/Test/Builder/IO/Scalar.pm
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'Text::Abbrev' => {
        'MAINTAINER'   => 'p5p',
        'DISTRIBUTION' => 'FLORA/Text-Abbrev-1.01.tar.gz',
        'FILES'        => q[dist/Text-Abbrev],
        'EXCLUDED'     => [qr{^t/release-.*\.t}],
        'UPSTREAM'     => 'blead',
    },

    'Text::Balanced' => {
        'MAINTAINER'   => 'dmanura',
        'DISTRIBUTION' => 'ADAMK/Text-Balanced-2.02.tar.gz',
        'FILES'        => q[cpan/Text-Balanced],
        'EXCLUDED'     => [
            qw( t/97_meta.t
                t/98_pod.t
                t/99_pmv.t
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'Text::ParseWords' => {
        'MAINTAINER'   => 'chorny',
        'DISTRIBUTION' => 'CHORNY/Text-ParseWords-3.27.zip',
        'FILES'        => q[cpan/Text-ParseWords],
        'EXCLUDED'     => ['t/pod.t'],

        # For the benefit of make_ext.pl, we have to have this accessible:
        'MAP' => {
            'ParseWords.pm' => 'cpan/Text-ParseWords/lib/Text/ParseWords.pm',
            ''              => 'cpan/Text-ParseWords/',
        },
        'UPSTREAM' => undef,
    },

    'Text::Soundex' => {
        'MAINTAINER'   => 'markm',
        'DISTRIBUTION' => 'MARKM/Text-Soundex-3.03.tar.gz',
        'FILES'        => q[cpan/Text-Soundex],
        'MAP'          => {
            '' => 'cpan/Text-Soundex/',

            # XXX these two files are clearly related,
            # but they appear to have diverged
            # considerably over the years
            'test.pl' => 'cpan/Text-Soundex/t/Soundex.t',
        },
        'UPSTREAM' => undef,
    },

    'Text-Tabs+Wrap' => {
        'MAINTAINER'   => 'muir',
        'DISTRIBUTION' => 'MUIR/modules/Text-Tabs+Wrap-2009.0305.tar.gz',
        'FILES'        => q[cpan/Text-Tabs],
        'EXCLUDED'   => ['t/dnsparks.t'],    # see af6492bf9e
        'UPSTREAM'   => 'cpan',
        'CUSTOMIZED' => [
            qw( t/fill.t
                t/tabs.t
                ),
        ],
    },

    'Thread::Queue' => {
        'MAINTAINER'   => 'jdhedden',
        'DISTRIBUTION' => 'JDHEDDEN/Thread-Queue-2.12.tar.gz',
        'FILES'        => q[dist/Thread-Queue],
        'EXCLUDED'     => [
            qw( examples/queue.pl
                t/00_load.t
                t/99_pod.t
                t/test.pl
                ),
        ],
        'UPSTREAM' => 'blead',
    },

    'Thread::Semaphore' => {
        'MAINTAINER'   => 'jdhedden',
        'DISTRIBUTION' => 'JDHEDDEN/Thread-Semaphore-2.12.tar.gz',
        'FILES'        => q[dist/Thread-Semaphore],
        'EXCLUDED'     => [
            qw( examples/semaphore.pl
                t/00_load.t
                t/99_pod.t
                t/test.pl
                ),
        ],
        'UPSTREAM' => 'blead',
    },

    'threads' => {
        'MAINTAINER'   => 'jdhedden',
        'DISTRIBUTION' => 'JDHEDDEN/threads-1.86.tar.gz',
        'FILES'        => q[dist/threads],
        'EXCLUDED'     => [
            qr{^examples/},
            qw( t/pod.t
                t/test.pl
                threads.h
                ),
        ],
        'UPSTREAM' => 'blead',
    },

    'threads::shared' => {
        'MAINTAINER'   => 'jdhedden',
        'DISTRIBUTION' => 'JDHEDDEN/threads-shared-1.40.tar.gz',
        'FILES'        => q[dist/threads-shared],
        'EXCLUDED'     => [
            qw( examples/class.pl
                shared.h
                t/pod.t
                t/test.pl
                ),
        ],
        'UPSTREAM' => 'blead',
    },

    'Tie::File' => {
        'MAINTAINER'   => 'mjd',
        'DISTRIBUTION' => 'TODDR/Tie-File-0.98.tar.gz',
        'FILES'        => q[dist/Tie-File],
        'UPSTREAM'     => 'blead',
    },

    'Tie::Hash' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[lib/Tie/Hash.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'Tie::Hash::NamedCapture' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Tie-Hash-NamedCapture],
        'UPSTREAM'   => 'blead',
    },

    'Tie::Memoize' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/Tie-Memoize],
        'UPSTREAM'   => 'blead',
    },

    'Tie::RefHash' => {
        'MAINTAINER'   => 'nuffin',
        'DISTRIBUTION' => 'FLORA/Tie-RefHash-1.39.tar.gz',
        'FILES'        => q[cpan/Tie-RefHash],
        'UPSTREAM'     => 'cpan',
    },

    'Time::HiRes' => {
        'MAINTAINER'   => 'zefram',
        'DISTRIBUTION' => 'ZEFRAM/Time-HiRes-1.9725.tar.gz',
        'FILES'        => q[cpan/Time-HiRes],
        'UPSTREAM'     => 'cpan',
    },

    'Time::Local' => {
        'MAINTAINER'   => 'drolsky',
        'DISTRIBUTION' => 'FLORA/Time-Local-1.2000.tar.gz',
        'FILES'        => q[cpan/Time-Local],
        'EXCLUDED'     => [
            qw( t/pod-coverage.t
                t/pod.t
                ),
        ],
        'UPSTREAM' => 'cpan',
    },

    'Time::Piece' => {
        'MAINTAINER'   => 'msergeant',
        'DISTRIBUTION' => 'MSERGEANT/Time-Piece-1.20.tar.gz',
        'FILES'        => q[cpan/Time-Piece],
        'UPSTREAM'     => undef,
    },

    'Unicode::Collate' => {
        'MAINTAINER'   => 'sadahiro',
        'DISTRIBUTION' => 'SADAHIRO/Unicode-Collate-0.89.tar.gz',
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
        'UPSTREAM' => 'first-come',
    },

    'Unicode::Normalize' => {
        'MAINTAINER'   => 'sadahiro',
        'DISTRIBUTION' => 'SADAHIRO/Unicode-Normalize-1.14.tar.gz',
        'FILES'        => q[cpan/Unicode-Normalize],
        'EXCLUDED'     => [
            qw( MANIFEST.N
                Normalize.pmN
                disableXS
                enableXS
                ),
        ],
        'UPSTREAM' => 'first-come',
    },

    'Unicode::UCD' => {
        'MAINTAINER' => 'blead',
        'FILES'      => q[lib/Unicode/UCD.{pm,t}],
        'UPSTREAM'   => 'blead',
    },

    'Version::Requirements' => {
        'MAINTAINER'   => 'rjbs',
        'DISTRIBUTION' => 'RJBS/Version-Requirements-0.101022.tar.gz',
        'FILES'        => q[cpan/Version-Requirements],
        'EXCLUDED'     => ['t/release-pod-syntax.t'],
        'UPSTREAM'     => 'cpan',
    },

    'version' => {
        'MAINTAINER'   => 'jpeacock',
        'DISTRIBUTION' => 'JPEACOCK/version-0.97.tar.gz',
        'FILES'        => q[lib/version.pm lib/version.pod lib/version],
        'EXCLUDED' => [
            qr{^t/.*\.t$},
            qr{^vutil/},
            'lib/version/typemap',
            't/survey_locales',
            'vperl/vpp.pm',
        ],
        'MAP' => {
            'lib/'           => 'lib/',
            't/'             => 'lib/version/t/'
        },
        'UPSTREAM' => undef,
    },

    'vms' => {
        'MAINTAINER' => 'craig',
        'FILES'      => q[vms configure.com README.vms],
        'UPSTREAM'   => undef,
    },

    'VMS::DCLsym' => {
        'MAINTAINER' => 'craig',
        'FILES'      => q[ext/VMS-DCLsym],
        'UPSTREAM'   => undef,
    },

    'VMS::Stdio' => {
        'MAINTAINER' => 'craig',
        'FILES'      => q[ext/VMS-Stdio],
        'UPSTREAM'   => undef,
    },

    'warnings' => {
        'MAINTAINER' => 'pmqs',
        'FILES'      => q[regen/warnings.pl
                 lib/warnings.{pm,t}
                 lib/warnings
                 t/lib/warnings
                ],
        'UPSTREAM' => 'blead',
    },

    'win32' => {
        'MAINTAINER' => 'jand',
        'FILES'      => q[win32 t/win32 README.win32 ext/Win32CORE],
        'UPSTREAM'   => undef,
    },

    'Win32' => {
        'MAINTAINER'   => 'jand',
        'DISTRIBUTION' => "JDB/Win32-0.44.tar.gz",
        'FILES'        => q[cpan/Win32],
        'UPSTREAM'     => 'cpan',
    },

    'Win32API::File' => {
        'MAINTAINER'   => 'chorny',
        'DISTRIBUTION' => 'CHORNY/Win32API-File-0.1200.tar.gz',
        'FILES'        => q[cpan/Win32API-File],
        'EXCLUDED'     => [
            qr{^ex/},
            't/pod.t',
        ],
        'UPSTREAM' => 'cpan',
    },

    'XS::Typemap' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[ext/XS-Typemap],
        'UPSTREAM'   => 'blead',
    },

    'XSLoader' => {
        'MAINTAINER'   => 'saper',
        'DISTRIBUTION' => 'SAPER/XSLoader-0.15.tar.gz',
        'FILES'        => q[dist/XSLoader],
        'EXCLUDED'     => [
            qr{^eg/},
            qw( t/pod.t
                t/podcover.t
                t/portfs.t
                ),
            'XSLoader.pm',    # we use XSLoader_pm.PL
        ],
        'UPSTREAM' => 'blead',
    },

    's2p' => {
        'MAINTAINER' => 'laun',
        'FILES'      => q[x2p/s2p.PL],
        'UPSTREAM'   => undef,
    },

    # this pseudo-module represents all the files under ext/ and lib/
    # that aren't otherwise claimed. This means that the following two
    # commands will check that every file under ext/ and lib/ is
    # accounted for, and that there are no duplicates:
    #
    #    perl Porting/Maintainers --checkmani lib ext
    #    perl Porting/Maintainers --checkmani

    '_PERLLIB' => {
        'MAINTAINER' => 'p5p',
        'FILES'      => q[
                ext/arybase/
                ext/XS-APItest/
                lib/CORE.pod
                lib/Config.t
                lib/Config/Extensions.{pm,t}
                lib/DB.{pm,t}
                lib/ExtUtils/Embed.pm
                lib/ExtUtils/XSSymSet.pm
                lib/ExtUtils/t/Embed.t
                lib/ExtUtils/typemap
                lib/Internals.t
                lib/Net/hostent.{pm,t}
                lib/Net/netent.{pm,t}
                lib/Net/protoent.{pm,t}
                lib/Net/servent.{pm,t}
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
                lib/Tie/Scalar.{pm,t}
                lib/Tie/StdHandle.pm
                lib/Tie/SubstrHash.{pm,t}
                lib/Time/gmtime.{pm,t}
                lib/Time/localtime.{pm,t}
                lib/Time/tm.pm
                lib/UNIVERSAL.pm
                lib/Unicode/README
                lib/User/grent.{pm,t}
                lib/User/pwent.{pm,t}
                lib/blib.{pm,t}
                lib/bytes.{pm,t}
                lib/bytes_heavy.pl
                lib/_charnames.pm
                lib/charnames.{pm,t}
                lib/dbm_filter_util.pl
                lib/deprecate.pm
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
                lib/perl5db.{pl,t}
                lib/perl5db/
                lib/sort.{pm,t}
                lib/strict.{pm,t}
                lib/subs.{pm,t}
                lib/unicore/
                lib/utf8.{pm,t}
                lib/utf8_heavy.pl
                lib/vars{.pm,.t,_carp.t}
                lib/vmsish.{pm,t}
                ],
        'UPSTREAM' => 'blead',
    },
);

# legacy CPAN flag
for ( values %Modules ) {
    $_->{CPAN} = !!$_->{DISTRIBUTION};
}

1;
