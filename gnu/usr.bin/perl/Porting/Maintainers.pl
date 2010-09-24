# A simple listing of core files that have specific maintainers,
# or at least someone that can be called an "interested party".
# Also, a "module" does not necessarily mean a CPAN module, it
# might mean a file or files or a subdirectory.
# Most (but not all) of the modules have dual lives in the core
# and in CPAN.  Those that have a CPAN existence, have the CPAN
# attribute set to true.

package Maintainers;

use File::Glob qw(:case);

%Maintainers =
    (
    'abergman'	=> 'Arthur Bergman <abergman@cpan.org>',
    'abigail'	=> 'Abigail <abigail@abigail.be>',
    'ams'	=> 'Abhijit Menon-Sen <ams@cpan.org>',
    'andk'	=> 'Andreas J. Koenig <andk@cpan.org>',
    'andya'	=> 'Andy Armstrong <andy@hexten.net>',
    'arandal'	=> 'Allison Randal <allison@perl.org>',
    'audreyt'	=> 'Audrey Tang <cpan@audreyt.org>',
    'avar'	=> 'Ævar Arnfjörð Bjarmason <avar@cpan.org>',
    'bingos'	=> 'Chris Williams <chris@bingosnet.co.uk>',
    'chorny'	=> 'Alexandr Ciornii <alexchorny@gmail.com>',
    'corion'	=> 'Max Maischein <corion@corion.net>',
    'craig'	=> 'Craig Berry <craigberry@mac.com>',
    'dankogai'	=> 'Dan Kogai <dankogai@cpan.org>',
    'dconway'	=> 'Damian Conway <dconway@cpan.org>',
    'dland'	=> 'David Landgren <dland@cpan.org>',
    'dmanura'	=> 'David Manura <dmanura@cpan.org>',
    'drolsky'	=> 'Dave Rolsky <drolsky@cpan.org>',
    'elizabeth'	=> 'Elizabeth Mattijsen <liz@dijkmat.nl>',
    'ferreira'	=> 'Adriano Ferreira <ferreira@cpan.org>',
    'gbarr'	=> 'Graham Barr <gbarr@cpan.org>',
    'gaas'	=> 'Gisle Aas <gaas@cpan.org>',
    'gsar'	=> 'Gurusamy Sarathy <gsar@activestate.com>',
    'ilyam'	=> 'Ilya Martynov <ilyam@cpan.org>',
    'ilyaz'	=> 'Ilya Zakharevich <ilyaz@cpan.org>',
    'jand'	=> 'Jan Dubois <jand@activestate.com>',
    'jdhedden'	=> 'Jerry D. Hedden <jdhedden@cpan.org>',
    'jesse'   	=> 'Jesse Vincent <jesse@bestpractical.com>',
    'jhi'	=> 'Jarkko Hietaniemi <jhi@cpan.org>',
    'jjore'	=> 'Joshua ben Jore <jjore@cpan.org>',
    'jpeacock'	=> 'John Peacock <jpeacock@cpan.org>',
    'jstowe'	=> 'Jonathan Stowe <jstowe@cpan.org>',
    'jv'	=> 'Johan Vromans <jv@cpan.org>',
    'kane'	=> 'Jos Boumans <kane@cpan.org>',
    'kwilliams'	=> 'Ken Williams <kwilliams@cpan.org>',
    'laun'	=> 'Wolfgang Laun <Wolfgang.Laun@alcatel.at>',
    'lstein'	=> 'Lincoln D. Stein <lds@cpan.org>',
    'lwall'	=> 'Larry Wall <lwall@cpan.org>',
    'marekr'	=> 'Marek Rouchal <marekr@cpan.org>',
    'markm'	=> 'Mark Mielke <markm@cpan.org>',
    'mhx'	=> 'Marcus Holland-Moritz <mhx@cpan.org>',
    'mjd'	=> 'Mark-Jason Dominus <mjd@plover.com>',
    'msergeant'	=> 'Matt Sergeant <msergeant@cpan.org>',
    'mshelor'	=> 'Mark Shelor <mshelor@cpan.org>',
    'muir'	=> 'David Muir Sharnoff <muir@cpan.org>',
    'neilb'	=> 'Neil Bowers <neilb@cpan.org>',
    'nuffin'	=> 'Yuval Kogman <nothingmuch@woobling.org>',
    'nwclark'	=> 'Nicholas Clark <nwclark@cpan.org>',
    'osfameron'	=> 'Hakim Cassimally <osfameron@perl.org>',
    'p5p'	=> 'perl5-porters <perl5-porters@perl.org>',
    'perlfaq'	=> 'perlfaq-workers <perlfaq-workers@perl.org>',
    'petdance'	=> 'Andy Lester <andy@petdance.com>',
    'pjf'	=> 'Paul Fenwick <pjf@cpan.org>',
    'pmqs'	=> 'Paul Marquess <pmqs@cpan.org>',
    'pvhp'	=> 'Peter Prymmer <pvhp@best.com>',
    'rafl'	=> 'Florian Ragwitz <flora@cpan.org>',
    'rclamp'	=> 'Richard Clamp <rclamp@cpan.org>',
    'rgarcia'	=> 'Rafael Garcia-Suarez <rgarcia@cpan.org>',
    'rkobes'	=> 'Randy Kobes <rkobes@cpan.org>',
    'rmbarker'	=> 'Robin Barker <rmbarker@cpan.org>',
    'rra'	=> 'Russ Allbery <rra@cpan.org>',
    'rurban'	=> 'Reini Urban <rurban@cpan.org>',
    'sadahiro'	=> 'SADAHIRO Tomoyuki <SADAHIRO@cpan.org>',
    'salva'	=> 'Salvador Fandiño García <salva@cpan.org>',
    'saper'	=> 'Sébastien Aperghis-Tramoni <saper@cpan.org>',
    'sbeck'     => 'Sullivan Beck <sbeck@cpan.org>',
    'sburke'	=> 'Sean Burke <sburke@cpan.org>',
    'mschwern'	=> 'Michael Schwern <mschwern@cpan.org>',
    'simonw'	=> 'Simon Wistow <simonw@cpan.org>',
    'smccam'	=> 'Stephen McCamant <smccam@cpan.org>',
    'smpeters'	=> 'Steve Peters <steve@fisharerojo.org>',
    'smueller'	=> 'Steffen Mueller <smueller@cpan.org>',
    'tels'	=> 'Tels <nospam-abuse@bloodgate.com>',
    'tomhughes'	=> 'Tom Hughes <tomhughes@cpan.org>',
    'tjenness'	=> 'Tim Jenness <tjenness@cpan.org>',
    'tyemq'	=> 'Tye McQueen <tyemq@cpan.org>',
    'yves'	=> 'Yves Orton <yves@cpan.org>',
    'zefram'	=> 'Andrew Main <zefram@cpan.org>',
    );


# IGNORABLE: files which, if they appear in the root of a CPAN
# distribution, need not appear in core (i.e. core-cpan-diff won't
# complain if it can't find them)

@IGNORABLE = qw(
    .cvsignore .dualLivedDiffConfig .gitignore
    ANNOUNCE Announce Artistic AUTHORS BENCHMARK BUGS Build.PL
    CHANGELOG ChangeLog CHANGES Changes COPYING Copying CREDITS
    GOALS HISTORY INSTALL INSTALL.SKIP LICENSE Makefile.PL
    MANIFEST MANIFEST.SKIP META.yml MYMETA.yml NEW NOTES ppport.h README
    SIGNATURE THANKS TODO Todo VERSION WHATSNEW
);


# Each entry in the  %Modules hash roughly represents a distribution,
# except in the case of CPAN=1, where it *exactly* represents a single
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

# CPAN can be either 1 (this distribution is also available on CPAN),
# or 0 (there is no # valid CPAN release).

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

# DEPRECATED contains the *first* version of Perl in which the module
# was considered deprecated.  It should only be present if the module is
# actually deprecated.  Such modules should use deprecated.pm to
# issue a warning if used.  E.g.:
#
#     use if $] >= 5.011, 'deprecate';
#

# MAP is a hash that maps CPAN paths to their core equivalents.
# Each key reprepresents a string prefix, with longest prefixes checked
# first. The first match causes that prefix to be replaced with the
# corresponding key. For example, with the following MAP:
#   {
#     'lib/'	 => 'lib/',
#     ''	 => 'lib/Foo/',
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
#     'lib/'	 => 'lib/',
#     ''	 => 'lib/Foo/Bar/',
#   }

%Modules = (

    'Archive::Extract' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'BINGOS/Archive-Extract-0.38.tar.gz',
	'FILES'		=> q[cpan/Archive-Extract],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	'BUGS'		=> 'bug-archive-extract@rt.cpan.org',
	},

    'Archive::Tar' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'BINGOS/Archive-Tar-1.54.tar.gz',
	'FILES'		=> q[cpan/Archive-Tar],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	'BUGS'		=> 'bug-archive-tar@rt.cpan.org',
	},

    'Attribute::Handlers' =>
	{
	'MAINTAINER'	=> 'rgarcia',
	'DISTRIBUTION'	=> 'SMUELLER/Attribute-Handlers-0.87.tar.gz',
	'FILES'		=> q[dist/Attribute-Handlers],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'autodie' =>
	{
	'MAINTAINER'	=> 'pjf',
	'DISTRIBUTION'	=> 'PJF/autodie-2.06_01.tar.gz',
	'FILES' 	=> q[cpan/autodie],
	'EXCLUDED'	=> [ qr{^inc/Module/},

                             # All these tests depend upon external
                             # modules that don't exist when we're
                             # building the core.  Hence, they can
                             # never run, and should not be merged.

			     qw(
				t/boilerplate.t
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
	'CPAN'  	=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'AutoLoader' =>
	{
	'MAINTAINER'	=> 'smueller',
	'DISTRIBUTION'	=> 'SMUELLER/AutoLoader-5.70.tar.gz',
	'FILES'		=> q[cpan/AutoLoader],
	'EXCLUDED'	=> [ qw( t/00pod.t ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'B::Concise' =>
	{
	'MAINTAINER'	=> 'smccam',
	'FILES'		=> q[ext/B/B/Concise.pm ext/B/t/concise.t],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'B::Debug' =>
	{
	'MAINTAINER'	=> 'rurban',
	'DISTRIBUTION'	=> 'RURBAN/B-Debug-1.12.tar.gz',
	'FILES'		=> q[cpan/B-Debug],
	'EXCLUDED'	=> [ qw( t/coverage.html t/pod.t ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'B::Deparse' =>
	{
	'MAINTAINER'	=> 'smccam',
	'FILES'		=> q[dist/B-Deparse],
	'CPAN'		=> 0,
	'UPSTREAM'	=> 'blead',
	},

    'B::Lint' =>
	{
	'MAINTAINER'	=> 'jjore',
	'DISTRIBUTION'	=> 'JJORE/B-Lint-1.11.tar.gz',
	'FILES'		=> q[cpan/B-Lint],
	'EXCLUDED'	=> [ qw( t/test.pl ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'base' =>
	{
	'MAINTAINER'	=> 'rgarcia',
	'DISTRIBUTION'	=> 'RGARCIA/base-2.15.tar.gz',
	'FILES'		=> q[dist/base],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'bignum' =>
	{
	'MAINTAINER'	=> 'tels',
	'DISTRIBUTION'	=> 'TELS/math/bignum-0.23.tar.gz',
	'FILES'		=> q[cpan/bignum],
	'EXCLUDED'	=> [ qr{^inc/Module/}, qw(t/pod.t t/pod_cov.t) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'CGI' =>
	{
	'MAINTAINER'	=> 'lstein',
	'DISTRIBUTION'	=> 'LDS/CGI.pm-3.49.tar.gz',
	'FILES'		=> q[cpan/CGI],
	'EXCLUDED'	=> [ qr{^t/lib/Test},
				qw( cgi-lib_porting.html
				    cgi_docs.html
				    examples/WORLD_WRITABLE/18.157.1.253.sav
				    t/gen-tests/gen-start-end-tags.pl
				    t/fast.t
				)
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Class::ISA' =>
	{
	'MAINTAINER'	=> 'smueller',
	'DISTRIBUTION'	=> 'SMUELLER/Class-ISA-0.36.tar.gz',
	'FILES'		=> q[cpan/Class-ISA],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	'DEPRECATED'	=> 5.011,
	},

    'Compress::Raw::Bzip2' =>
	{
	'MAINTAINER'	=> 'pmqs',
	'DISTRIBUTION'	=> 'PMQS/Compress-Raw-Bzip2-2.021.tar.gz',
	'FILES'		=> q[cpan/Compress-Raw-Bzip2],
	'EXCLUDED'	=> [ qr{^t/Test/},
			     qw( bzip2-src/bzip2-cpp.patch
			     )
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Compress::Raw::Zlib' =>
	{
	'MAINTAINER'	=> 'pmqs',
	'DISTRIBUTION'	=> 'PMQS/Compress-Raw-Zlib-2.021.tar.gz',

	'FILES'		=> q[cpan/Compress-Raw-Zlib],
	'EXCLUDED'	=> [ qr{^t/Test/},
			     qw( t/000prereq.t
				 t/99pod.t
			       )
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'constant' =>
	{
	'MAINTAINER'	=> 'saper',
	'DISTRIBUTION'	=> 'SAPER/constant-1.19.tar.gz',
	'FILES'		=> q[dist/constant],
	'EXCLUDED'	=> [ qw( t/00-load.t
				 t/more-tests.t
				 t/pod-coverage.t
				 t/pod.t
				 eg/synopsis.pl
			       )
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'CPAN' =>
	{
	'MAINTAINER'	=> 'andk',
	'DISTRIBUTION'	=> 'ANDK/CPAN-1.94_56.tar.gz',
	'FILES'		=> q[cpan/CPAN],
	'EXCLUDED'	=> [ qr{^distroprefs/},
			     qr{^inc/Test/},
			     qr{^t/CPAN/authors/},
			     qw{
				lib/CPAN/Admin.pm
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
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'CPANPLUS' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'BINGOS/CPANPLUS-0.90.tar.gz',
	'FILES'		=> q[cpan/CPANPLUS],
	'EXCLUDED'	=> [ qr{^inc/},
			     qr{^t/dummy-.*\.hidden$},
			     qw{ bin/cpanp-boxed },
			     # SQLite tests would be skipped in core, and
			     # the filenames are too long for VMS!
			     qw{
				 t/031_CPANPLUS-Internals-Source-SQLite.t
				 t/032_CPANPLUS-Internals-Source-via-sqlite.t
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	'BUGS'		=> 'bug-cpanplus@rt.cpan.org',
	},

    'CPANPLUS::Dist::Build' =>
	{
	'MAINTAINER'	=> 'bingos',
	'DISTRIBUTION'	=> 'BINGOS/CPANPLUS-Dist-Build-0.46.tar.gz',
	'FILES'		=> q[cpan/CPANPLUS-Dist-Build],
	'EXCLUDED'	=> [ qr{^inc/},
			     qw{ t/99_pod.t
			         t/99_pod_coverage.t
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Data::Dumper' =>
	{
	'MAINTAINER'	=> 'p5p', # Not gsar. Not ilyam
	'DISTRIBUTION'	=> 'SMUELLER/Data-Dumper-2.125.tar.gz',
	'FILES'		=> q[dist/Data-Dumper],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'DB_File' =>
	{
	'MAINTAINER'	=> 'pmqs',
	'DISTRIBUTION'	=> 'PMQS/DB_File-1.820.tar.gz',
	'FILES'		=> q[cpan/DB_File],
	'EXCLUDED'	=> [ qr{^patches/},
			     qw{ t/pod.t
			         fallback.h
				 fallback.xs
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Devel::PPPort' =>
	{
	'MAINTAINER'	=> 'mhx',
	'DISTRIBUTION'	=> 'MHX/Devel-PPPort-3.19.tar.gz',
	'FILES'		=> q[cpan/Devel-PPPort],
	'EXCLUDED'	=> [ qw{PPPort.pm} ], # we use PPPort_pm.PL instead
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Digest' =>
	{
	'MAINTAINER'	=> 'gaas',
	'DISTRIBUTION'	=> 'GAAS/Digest-1.16.tar.gz',
	'FILES'		=> q[cpan/Digest],
	'EXCLUDED'	=> [ qw{digest-bench} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Digest::MD5' =>
	{
	'MAINTAINER'	=> 'gaas',
	'DISTRIBUTION'	=> 'GAAS/Digest-MD5-2.39.tar.gz',
	'FILES'		=> q[cpan/Digest-MD5],
	'EXCLUDED'	=> [ qw{rfc1321.txt} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Digest::SHA' =>
	{
	'MAINTAINER'	=> 'mshelor',
	'DISTRIBUTION'	=> 'MSHELOR/Digest-SHA-5.47.tar.gz',
	'FILES' 	=> q[cpan/Digest-SHA],
	'EXCLUDED'	=> [ qw{t/pod.t t/podcover.t examples/dups} ],
	'CPAN'  	=> 1,
	'UPSTREAM'	=> undef,
	},

    'Encode' =>
	{
	'MAINTAINER'	=> 'dankogai',
	'DISTRIBUTION'	=> 'DANKOGAI/Encode-2.39.tar.gz',
	'FILES'		=> q[cpan/Encode],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'encoding::warnings' =>
	{
	'MAINTAINER'	=> 'audreyt',
	'DISTRIBUTION'	=> 'AUDREYT/encoding-warnings-0.11.tar.gz',
	'FILES'		=> q[cpan/encoding-warnings],
	'EXCLUDED'	=> [ qr{^inc/Module/},
			     qw{t/0-signature.t Makefile.PL MANIFEST META.yml
			     README SIGNATURE},
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Exporter' =>
	{
	'MAINTAINER'	=> 'ferreira',
	'DISTRIBUTION'	=> 'FERREIRA/Exporter-5.63.tar.gz',
	'FILES'		=> q[lib/Exporter.pm
			     lib/Exporter.t
			     lib/Exporter/Heavy.pm
			    ],
	'EXCLUDED'	=> [ qw{t/pod.t t/use.t}, ],
	'MAP'		=> { 't/'	=> 'lib/',
			     'lib/'	=> 'lib/',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'ExtUtils::CBuilder' =>
	{
	'MAINTAINER'	=> 'kwilliams',
	'DISTRIBUTION'	=> 'DAGOLDEN/ExtUtils-CBuilder-0.27.tar.gz',
	'FILES'		=> q[cpan/ExtUtils-CBuilder],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'ExtUtils::Command' =>
	{
	'MAINTAINER'	=> 'rkobes',
	'DISTRIBUTION'	=> 'RKOBES/ExtUtils-Command-1.16.tar.gz',
	'FILES'		=> q[cpan/ExtUtils-Command],
	'EXCLUDED'	=> [ qw{ t/shell_command.t
				 t/shell_exit.t
				 lib/Shell/Command.pm
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'ExtUtils::Constant' =>
	{
	'MAINTAINER'	=> 'nwclark',
	 # Nick has confirmed that while we have diverged from CPAN,
	 # this package isn't primarily maintained in core
	 # Another release wll happen "Sometime"
	 'DISTRIBUTION'	=> '',#'NWCLARK/ExtUtils-Constant-0.16.tar.gz',
	'FILES'		=> q[cpan/ExtUtils-Constant],
	'EXCLUDED'	=> [ qw{ lib/ExtUtils/Constant/Aaargh56Hash.pm
				 examples/perl_keyword.pl
				 examples/perl_regcomp_posix_keyword.pl
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'ExtUtils::Install' =>
	{
	'MAINTAINER'	=> 'yves',
	'DISTRIBUTION'	=> 'YVES/ExtUtils-Install-1.54.tar.gz',
	'FILES' 	=> q[dist/ExtUtils-Install],
	'EXCLUDED'	=> [ qw{ t/lib/Test/Builder.pm
				 t/lib/Test/Builder/Module.pm
				 t/lib/Test/More.pm
				 t/lib/Test/Simple.pm
				 t/pod-coverage.t
				 t/pod.t
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'ExtUtils::MakeMaker' =>
	{
	'MAINTAINER'	=> 'mschwern',
	'DISTRIBUTION'	=> 'MSCHWERN/ExtUtils-MakeMaker-6.56.tar.gz',
	'FILES'		=> q[cpan/ExtUtils-MakeMaker],
	'EXCLUDED'	=> [ qr{^t/lib/Test/},
			     qr{^inc/ExtUtils/},
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'first-come',
	},

    'ExtUtils::Manifest' =>
	{
	'MAINTAINER'	=> 'rkobes',
	'DISTRIBUTION'	=> 'RKOBES/ExtUtils-Manifest-1.57.tar.gz',
	'FILES'		=> q[cpan/ExtUtils-Manifest],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'ExtUtils::ParseXS' =>
	{
	'MAINTAINER'	=> 'kwilliams',
    'DISTRIBUTION' => 'DAGOLDEN/ExtUtils-ParseXS-2.21.tar.gz',
    'EXCLUDED'  => [ qw{
	                t/bugs/RT48104.xs
				    t/bugs/typemap
					t/include/nsUniversalDetector.h
					t/include/nscore.h
				   }],
	'FILES'		=> q[cpan/ExtUtils-ParseXS],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'faq' =>
	{
	'MAINTAINER'	=> 'perlfaq',
	'FILES'		=> q[pod/perlfaq*],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'File::Fetch' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'BINGOS/File-Fetch-0.24.tar.gz',
	'FILES'		=> q[cpan/File-Fetch],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'File::Path' =>
	{
	'MAINTAINER'	=> 'dland',
	'DISTRIBUTION'	=> 'DLAND/File-Path-2.08.tar.gz',
	'FILES'		=> q[cpan/File-Path],
	'EXCLUDED'	=> [ qw{eg/setup-extra-tests
				t/pod.t
			       }
			   ],
	'MAP'		=> { ''		=> 'cpan/File-Path/lib/File/',
			     't/'	=> 'cpan/File-Path/t/',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'File::Temp' =>
	{
	'MAINTAINER'	=> 'tjenness',
	'DISTRIBUTION'	=> 'TJENNESS/File-Temp-0.22.tar.gz',
	'FILES'		=> q[cpan/File-Temp],
	'EXCLUDED'	=> [ qw{misc/benchmark.pl
				misc/results.txt
			       }
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Filter::Simple' =>
	{
	'MAINTAINER'	=> 'smueller',
	'DISTRIBUTION'	=> 'SMUELLER/Filter-Simple-0.84.tar.gz',
	'FILES'		=> q[dist/Filter-Simple],
	'EXCLUDED'	=> [ qw(Makefile.PL MANIFEST README META.yml),
			     qr{^demo/}
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'Filter::Util::Call' =>
	{
	'MAINTAINER'	=> 'pmqs',
	'DISTRIBUTION'	=> 'PMQS/Filter-1.37.tar.gz',
	'FILES'		=> q[cpan/Filter-Util-Call
			     t/lib/filter-util.pl
			     pod/perlfilter.pod
			    ],
	'EXCLUDED'	=> [ qr{^decrypt/},
			     qr{^examples/},
			     qr{^Exec/},
			     qr{^lib/Filter/},
			     qr{^tee/},
			     qw{ Call/Makefile.PL
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
			       }
			   ],
	'MAP'		=> { 'Call/'	      => 'cpan/Filter-Util-Call/',
			     'filter-util.pl' => 'cpan/Filter-Util-Call/filter-util.pl',
			     'perlfilter.pod' => 'pod/perlfilter.pod',
			     ''		      => 'cpan/Filter-Util-Call/',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Getopt::Long' =>
	{
	'MAINTAINER'	=> 'jv',
	'DISTRIBUTION'	=> 'JV/Getopt-Long-2.38.tar.gz',
	'FILES'		=> q[cpan/Getopt-Long
			     lib/newgetopt.pl
			    ],
	'EXCLUDED'	=> [ qr{^examples/},
			     qw{perl-Getopt-Long.spec},
			   ],
	'MAP'		=> { ''		       => 'cpan/Getopt-Long/',
			     'lib/newgetopt.pl' => 'lib/newgetopt.pl',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    # Sean has donated it to us.
    # Nothing has changed since his last CPAN release.
    # (not strictly true: there have been some trivial typo fixes; DAPM 6/2009)
    'I18N::LangTags' =>
	{
	'MAINTAINER'	=> 'p5p',
	'DISTRIBUTION'	=> 'SBURKE/I18N-LangTags-0.35.tar.gz',
	'FILES'		=> q[dist/I18N-LangTags],
	'CPAN'		=> 0,
	'UPSTREAM'	=> 'blead',
	},

    'if' =>
	{
	'MAINTAINER'	=> 'ilyaz',
	'DISTRIBUTION'	=> 'ILYAZ/modules/if-0.0401.tar.gz',
	'FILES'		=> q[cpan/if],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'IO' =>
	{
	'MAINTAINER'	=> 'p5p',
	'DISTRIBUTION'	=> 'GBARR/IO-1.25.tar.gz',
	'FILES'		=> q[dist/IO/],
	'EXCLUDED'	=> [ qw{t/test.pl}, ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'IO-Compress' =>
	{
	'MAINTAINER'	=> 'pmqs',
	'DISTRIBUTION'	=> 'PMQS/IO-Compress-2.021.tar.gz',
	'FILES'		=> q[cpan/IO-Compress],
	'EXCLUDED'	=> [ qr{t/Test/} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'IO::Zlib' =>
	{
	'MAINTAINER'	=> 'tomhughes',
	'DISTRIBUTION'	=> 'TOMHUGHES/IO-Zlib-1.10.tar.gz',
	'FILES'		=> q[cpan/IO-Zlib],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'IPC::Cmd' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'BINGOS/IPC-Cmd-0.54.tar.gz',
	'FILES'		=> q[cpan/IPC-Cmd],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'IPC::SysV' =>
	{
	'MAINTAINER'	=> 'mhx',
	'DISTRIBUTION'	=> 'MHX/IPC-SysV-2.01.tar.gz',
	'FILES'		=> q[cpan/IPC-SysV],
	'EXCLUDED'	=> [ qw{const-c.inc const-xs.inc} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'lib' =>
	{
	'MAINTAINER'	=> 'smueller',
	'DISTRIBUTION'	=> 'SMUELLER/lib-0.62.tar.gz',
	'FILES'		=> q[dist/lib/],
	'EXCLUDED'	=> [ qw{forPAUSE/lib.pm t/00pod.t} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'libnet' =>
	{
	'MAINTAINER'	=> 'gbarr',
	'DISTRIBUTION'	=> 'GBARR/libnet-1.22.tar.gz',
	'FILES'		=> q[cpan/libnet],
	'EXCLUDED'	=> [ qw{Configure install-nomake} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Locale-Codes' =>
	{
	'MAINTAINER'	=> 'sbeck',
	'DISTRIBUTION'	=> 'NEILB/Locale-Codes-2.07.tar.gz',
	'FILES'		=> q[cpan/Locale-Codes],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Locale::Maketext' =>
	{
	'MAINTAINER'	=> 'ferreira',
	'DISTRIBUTION'	=> 'FERREIRA/Locale-Maketext-1.13.tar.gz',
	'FILES'		=> q[dist/Locale-Maketext],
	'EXCLUDED'	=> [ qw{perlcriticrc t/00_load.t t/pod.t} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'Locale::Maketext::Simple' =>
	{
	'MAINTAINER'	=> 'audreyt',
	'DISTRIBUTION'	=> 'JESSE/Locale-Maketext-Simple-0.21.tar.gz',
	'FILES'		=> q[cpan/Locale-Maketext-Simple],
	'EXCLUDED'	=> [ qr{^inc/} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Log::Message' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'KANE/Log-Message-0.02.tar.gz',
	'FILES'		=> q[cpan/Log-Message],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Log::Message::Simple' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'BINGOS/Log-Message-Simple-0.06.tar.gz',
	'FILES'		=> q[cpan/Log-Message-Simple],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'mad' =>
	{
	'MAINTAINER'	=> 'lwall',
	'FILES'		=> q[mad],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'Math::BigInt' =>
	{
	'MAINTAINER'	=> 'tels',
	'DISTRIBUTION'	=> 'TELS/math/Math-BigInt-1.89.tar.gz',
	'FILES'		=> q[cpan/Math-BigInt],
	'EXCLUDED'	=> [ qr{^inc/},
			     qr{^examples/},
			     qw{t/pod.t
				t/pod_cov.t
			       }
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Math::BigInt::FastCalc' =>
	{
	'MAINTAINER'	=> 'tels',
	'DISTRIBUTION'	=> 'TELS/math/Math-BigInt-FastCalc-0.19.tar.gz',
	'FILES'		=> q[cpan/Math-BigInt-FastCalc],
	'EXCLUDED'	=> [ qr{^inc/},
			     qw{
				t/pod.t
				t/pod_cov.t
			       },
			     # instead we use the versions of these test
			     # files that come with Math::BigInt:
			     qw{t/bigfltpm.inc
				t/bigfltpm.t
				t/bigintpm.inc
				t/bigintpm.t
				t/mbimbf.inc
				t/mbimbf.t
			       },
			   ],
	'MAP'		=> { '' => 'cpan/Math-BigInt-FastCalc/',
			     'lib/Math/BigInt/FastCalc.pm'
				    => 'cpan/Math-BigInt-FastCalc/FastCalc.pm',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Math::BigRat' =>
	{
	'MAINTAINER'	=> 'tels',
	'DISTRIBUTION'	=> 'LETO/Math-BigRat-0.24.tar.gz',
	'FILES'		=> q[cpan/Math-BigRat],
	'EXCLUDED'	=> [ qr{^inc/},
			     qw{
				t/pod.t
				t/pod_cov.t
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Math::Complex' =>
	{
	'MAINTAINER'	=> 'zefram',
	'DISTRIBUTION'	=> 'JHI/Math-Complex-1.56.tar.gz',
	'FILES'		=> q[cpan/Math-Complex],
	'EXCLUDED'	=> [
			     qw{
				t/pod.t
				t/pod-coverage.t
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Memoize' =>
	{
	'MAINTAINER'	=> 'mjd',
	'DISTRIBUTION'	=> 'MJD/Memoize-1.01.tar.gz',
	'FILES'		=> q[cpan/Memoize],
	'EXCLUDED'	=> [
			     qw{
				article.html
				Memoize/Saves.pm
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'MIME::Base64' =>
	{
	'MAINTAINER'	=> 'gaas',
	'DISTRIBUTION'	=> 'GAAS/MIME-Base64-3.08.tar.gz',
	'FILES'		=> q[cpan/MIME-Base64],
	'EXCLUDED'	=> [ qw{ t/bad-sv.t }, ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Module::Build' =>
	{
	'MAINTAINER'	=> 'kwilliams',
	'DISTRIBUTION'	=> 'DAGOLDEN/Module-Build-0.3603.tar.gz',
	'FILES'		=> q[cpan/Module-Build],
	'EXCLUDED'	=> [ qw{ t/par.t t/signature.t },
			     qr!^contrib/!,  qr!^devtools! ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Module::CoreList' =>
	{
	'MAINTAINER'	=> 'rgarcia',
	'DISTRIBUTION'	=> 'BINGOS/Module-CoreList-2.36.tar.gz',
	'FILES'		=> q[dist/Module-CoreList],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'Module::Load' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'KANE/Module-Load-0.16.tar.gz',
	'FILES'		=> q[cpan/Module-Load],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Module::Load::Conditional' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'BINGOS/Module-Load-Conditional-0.38.tar.gz',
	'FILES'		=> q[cpan/Module-Load-Conditional],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Module::Loaded' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'BINGOS/Module-Loaded-0.06.tar.gz',
	'FILES'		=> q[cpan/Module-Loaded],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Module::Pluggable' =>
	{
	'MAINTAINER'	=> 'simonw',
	'DISTRIBUTION'	=> 'SIMONW/Module-Pluggable-3.9.tar.gz',
	'FILES'		=> q[cpan/Module-Pluggable],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Net::Ping' =>
	{
	'MAINTAINER'	=> 'smpeters',
	'DISTRIBUTION'	=> 'SMPETERS/Net-Ping-2.36.tar.gz',
	'FILES'		=> q[dist/Net-Ping],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'NEXT' =>
	{
	'MAINTAINER'	=> 'rafl',
	'DISTRIBUTION'	=> 'FLORA/NEXT-0.64.tar.gz',
	'FILES'		=> q[cpan/NEXT],
	'EXCLUDED'	=> [ qr{^demo/} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Object::Accessor' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'BINGOS/Object-Accessor-0.36.tar.gz',
	'FILES'		=> q[cpan/Object-Accessor],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Package::Constants' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'KANE/Package-Constants-0.02.tar.gz',
	'FILES'		=> q[cpan/Package-Constants],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Params::Check' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'KANE/Params-Check-0.26.tar.gz',
	# For some reason a file of this name appears within
	# the tarball. Russell's Paradox eat your heart out.
	'EXCLUDED'	=> [ qw( Params-Check-0.26.tar.gz ) ],
	'FILES'		=> q[cpan/Params-Check],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'parent' =>
	{
	'MAINTAINER'	=> 'corion',
	'DISTRIBUTION'	=> 'CORION/parent-0.223.tar.gz',
	'FILES'		=> q[cpan/parent],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Parse::CPAN::Meta' =>
	{
	'MAINTAINER'	=> 'smueller',
	'DISTRIBUTION'	=> 'SMUELLER/Parse-CPAN-Meta-1.40.tar.gz',
	'FILES'		=> q[cpan/Parse-CPAN-Meta],
	'EXCLUDED'	=> [ qw( t/97_meta.t t/98_pod.t t/99_pmv.t ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	# NOTE: 'perl uupacktool.pl t/data/utf_16_le_bom.yml.packed'
	# run by hand after import, as the core's test harness doesn't
	# run dists' "make test" steps
	},

    'PathTools' =>
	{
	'MAINTAINER'	=> 'kwilliams',
	'DISTRIBUTION'	=> 'SMUELLER/PathTools-3.31.tar.gz',
	'FILES'		=> q[cpan/Cwd],
	'EXCLUDED'	=> [ qr{^t/lib/Test/} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> "cpan",
	# NOTE: PathTools is in cpan/Cwd/ because it contains Cwd.xs and
	# something, possibly Makefile.SH, makes an assumption that the
	# leafname of some  file corresponds with the pathname of the directory.
	},

    'perlebcdic' =>
	{
	'MAINTAINER'	=> 'pvhp',
	'FILES'		=> q[pod/perlebcdic.pod],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'PerlIO' =>
	{
	'MAINTAINER'	=> 'p5p',
	'FILES'		=> q[ext/PerlIO],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'PerlIO::via::QuotedPrint' =>
	{
	'MAINTAINER'	=> 'elizabeth',
	'DISTRIBUTION'	=> 'ELIZABETH/PerlIO-via-QuotedPrint-0.06.tar.gz',
	'FILES'		=> q[cpan/PerlIO-via-QuotedPrint],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'perlpacktut' =>
	{
	'MAINTAINER'	=> 'laun',
	'FILES'		=> q[pod/perlpacktut.pod],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'perlpodspec' =>
	{
	'MAINTAINER'	=> 'sburke',
	'FILES'		=> q[pod/perlpodspec.pod],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'perlre' =>
	{
	'MAINTAINER'	=> 'abigail',
	'FILES'		=> q[pod/perlrecharclass.pod
			     pod/perlrebackslash.pod],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},


    'perlreapi' =>
	{
	MAINTAINER	=> 'avar',
	FILES		=> q[pod/perlreapi.pod],
	CPAN		=> 0,
	'UPSTREAM'	=> undef,
	},

    'perlreftut' =>
	{
	'MAINTAINER'	=> 'mjd',
	'FILES'		=> q[pod/perlreftut.pod],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'perlthrtut' =>
	{
	'MAINTAINER'	=> 'elizabeth',
	'FILES'		=> q[pod/perlthrtut.pod],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'Pod::Escapes' =>
	{
	'MAINTAINER'	=> 'arandal',
	'DISTRIBUTION'	=> 'SBURKE/Pod-Escapes-1.04.tar.gz',
	'FILES'		=> q[cpan/Pod-Escapes],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Pod::LaTeX' =>
	{
	'MAINTAINER'	=> 'tjenness',
	'DISTRIBUTION'	=> 'TJENNESS/Pod-LaTeX-0.58.tar.gz',
	'FILES'		=> q[cpan/Pod-LaTeX
			     pod/pod2latex.PL
			    ],
	'EXCLUDED'	=> [ qw( t/require.t ) ],
	'MAP'		=> { '' => 'cpan/Pod-LaTeX/',
			     'pod2latex.PL' => 'pod/pod2latex.PL',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Pod::Parser'	=> {
	'MAINTAINER'	=> 'marekr',

	# XXX Parser.pm in the 1.38 distribution identifies itself as
	# version 1.37!

	'DISTRIBUTION'	=> 'MAREKR/Pod-Parser-1.38.tar.gz',
	'FILES'		=> q[cpan/Pod-Parser
			     pod/pod{2usage,checker,select}.PL
			    ],
	'MAP'		=> { '' => 'cpan/Pod-Parser/',
			     'scripts/' => 'pod/',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Pod::Perldoc' =>
	{
	'MAINTAINER'	=> 'ferreira',
	'DISTRIBUTION'	=> 'FERREIRA/Pod-Perldoc-3.15_01.tar.gz',
	# I don't know whether it's conceptually cleaner to a rule to copy
	# ext/Pod-Perldoc/pod/perldoc.pod to pod/perldoc.pod at make time
	# (in 4 places), or leave it as 1 mapping here.
	'FILES'		=> q[dist/Pod-Perldoc
			     pod/perldoc.pod
			    ],
	# in blead, the perldoc executable is generated by perldoc.PL
	# instead
	# XXX We can and should fix this, but clean up the DRY-failure in utils
	# first
	'EXCLUDED'	=> [ qw( perldoc ) ],
	'MAP'		=> { '' => 'dist/Pod-Perldoc/',
			     'lib/perldoc.pod' => 'pod/perldoc.pod',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'Pod::Plainer' =>
	{
	'DISTRIBUTION'	=> 'RMBARKER/Pod-Plainer-1.02.tar.gz',
	'MAINTAINER'	=> 'rmbarker',
	'FILES'		=> q[dist/Pod-Plainer],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	'EXCLUDED'	=> [ qw(t/pod.t t/pod-coverage.t) ],
	'DEPRECATED'	=> 5.011,
	},

    'Pod::Simple' =>
	{
	'MAINTAINER'	=> 'arandal',
	'DISTRIBUTION'	=> 'DWHEELER/Pod-Simple-3.13.tar.gz',
	'FILES'		=> q[cpan/Pod-Simple],
	# XXX these two files correspond to similar ones in blead under
	# pod/, but the blead ones have newer changes, and also seem to
	# have been in blead a long time. I'm going to assume then that
	# the blead versions of these two files are authoritative - DAPM
	'EXCLUDED'	=> [ qw( lib/perlpod.pod lib/perlpodspec.pod ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'podlators' =>
	{
	'MAINTAINER'	=> 'rra',
	'DISTRIBUTION'	=> 'RRA/podlators-2.3.1.tar.gz',
	'FILES'		=> q[cpan/podlators
			     pod/pod2man.PL
			     pod/pod2text.PL
			    ],
	'MAP'		=> { '' => 'cpan/podlators/',
			     'scripts/' => 'pod/',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Safe' =>
	{
	'MAINTAINER'	=> 'rgarcia',
	'DISTRIBUTION'	=> 'RGARCIA/Safe-2.22.tar.gz',
	'FILES'		=> q[dist/Safe],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'Scalar-List-Utils' =>
	{
	'MAINTAINER'	=> 'gbarr',
	'DISTRIBUTION'	=> 'GBARR/Scalar-List-Utils-1.21.tar.gz',
	# Note that perl uses its own version of Makefile.PL
	'FILES'		=> q[cpan/List-Util],
	'EXCLUDED'	=> [ qr{^inc/Module/},
			     qr{^inc/Test/},
			     qw{ mytypemap },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'SelfLoader' =>
	{
	'MAINTAINER'	=> 'smueller',
	'DISTRIBUTION'	=> 'SMUELLER/SelfLoader-1.17.tar.gz',
	'FILES'		=> q[dist/SelfLoader],
	'EXCLUDED'	=> [ qw{ t/00pod.t } ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'Shell' =>
	{
	'MAINTAINER'	=> 'ferreira',
	'DISTRIBUTION'	=> 'FERREIRA/Shell-0.72_01.tar.gz',
	'FILES'		=> q[cpan/Shell],
	'EXCLUDED'	=> [ qw{ t/01_use.t t/99_pod.t } ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	'DEPRECATED'	=> 5.011,
	},

    'Storable' =>
	{
	'MAINTAINER'	=> 'ams',
	'DISTRIBUTION'	=> 'AMS/Storable-2.21.tar.gz',
	'FILES'		=> q[dist/Storable],
	'EXCLUDED'	=> [ qr{^t/Test/} ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'Switch' =>
	{
	'MAINTAINER'	=> 'rgarcia',
	'DISTRIBUTION'	=> 'RGARCIA/Switch-2.15.tar.gz',
	'FILES'		=> q[dist/Switch],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	'DEPRECATED'	=> 5.011,
	},

    'Sys::Syslog' =>
	{
	'MAINTAINER'	=> 'saper',
	'DISTRIBUTION'	=> 'SAPER/Sys-Syslog-0.27.tar.gz',
	'FILES'		=> q[cpan/Sys-Syslog],
	'EXCLUDED'	=> [ qr{^eg/},
			     qw{t/data-validation.t
			        t/distchk.t
				t/pod.t
				t/podcover.t
				t/podspell.t
				t/portfs.t
				win32/PerlLog.RES
			       },
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Term::ANSIColor' =>
	{
	'MAINTAINER'	=> 'rra',
	'DISTRIBUTION'	=> 'RRA/ANSIColor-2.02.tar.gz',
	'FILES'		=> q[cpan/Term-ANSIColor],
	'EXCLUDED'	=> [ qr{^tests/}, qw(t/pod-spelling.t t/pod.t) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Term::Cap' =>
	{
	'MAINTAINER'	=> 'jstowe',
	'DISTRIBUTION'	=> 'JSTOWE/Term-Cap-1.12.tar.gz',
	'FILES'		=> q[cpan/Term-Cap],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Term::UI' =>
	{
	'MAINTAINER'	=> 'kane',
	'DISTRIBUTION'	=> 'KANE/Term-UI-0.20.tar.gz',
	'FILES'		=> q[cpan/Term-UI],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Test' =>
	{
	'MAINTAINER'	=> 'jesse',
	'DISTRIBUTION'	=> 'JESSE/Test-1.25_02.tar.gz',
	'FILES'		=> q[cpan/Test],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Test::Harness' =>
	{
	'MAINTAINER'	=> 'andya',
	'DISTRIBUTION'	=> 'ANDYA/Test-Harness-3.17.tar.gz',
	'FILES'		=> q[cpan/Test-Harness],
	'EXCLUDED'	=> [ qr{^examples/},
			     qr{^inc/},
			     qr{^t/lib/Test/},
			     qr{^xt/},
			     qw{Changes-2.64
				HACKING.pod
				perlcriticrc
				t/lib/if.pm
			       }
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Test::Simple' =>
	{
	'MAINTAINER'	=> 'mschwern',
	'DISTRIBUTION'	=> 'MSCHWERN/Test-Simple-0.94.tar.gz',
	'FILES'		=> q[cpan/Test-Simple],
	'EXCLUDED'	=> [
			     qw{.perlcriticrc
				.perltidyrc
				t/00compile.t
				t/pod.t
				t/pod-coverage.t
				t/Builder/reset_outputs.t
				lib/Test/Builder/IO/Scalar.pm
			       }
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Text::Balanced' =>
	{
	'MAINTAINER'	=> 'dmanura',
	'DISTRIBUTION'	=> 'ADAMK/Text-Balanced-2.02.tar.gz',
	'FILES'		=> q[cpan/Text-Balanced],
	'EXCLUDED'	=> [ qw( t/97_meta.t t/98_pod.t t/99_pmv.t ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Text::ParseWords' =>
	{
	'MAINTAINER'	=> 'chorny',
	'DISTRIBUTION'	=> 'CHORNY/Text-ParseWords-3.27.zip',
	'FILES'		=> q[cpan/Text-ParseWords],
	'EXCLUDED'	=> [ qw( t/pod.t ) ],
	# For the benefit of make_ext.pl, we have to have this accessible:
	'MAP'		=> {
			     'ParseWords.pm' => 'cpan/Text-ParseWords/lib/Text/ParseWords.pm',
			     ''              => 'cpan/Text-ParseWords/',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Text::Soundex' =>
	{
	'MAINTAINER'	=> 'markm',
	'DISTRIBUTION'	=> 'MARKM/Text-Soundex-3.03.tar.gz',
	'FILES'		=> q[cpan/Text-Soundex],
	'MAP'		=> { ''               => 'cpan/Text-Soundex/',
			     # XXX these two files are clearly related,
			     # but they appear to have diverged
			     # considerably over the years
	                     'test.pl'        => 'cpan/Text-Soundex/t/Soundex.t',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Text-Tabs+Wrap' =>
	{
	'MAINTAINER'	=> 'muir',
	'DISTRIBUTION'	=> 'MUIR/modules/Text-Tabs+Wrap-2009.0305.tar.gz',
	'FILES'		=> q[cpan/Text-Tabs],
	'EXCLUDED'	=> [ qw( t/dnsparks.t ) ], # see af6492bf9e
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Thread::Queue' =>
	{
	'MAINTAINER'	=> 'jdhedden',
	'DISTRIBUTION'	=> 'JDHEDDEN/Thread-Queue-2.11.tar.gz',
	'FILES'		=> q[dist/Thread-Queue],
	'EXCLUDED'	=> [ qw(examples/queue.pl
				t/00_load.t
				t/99_pod.t
				t/test.pl
			       ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'Thread::Semaphore' =>
	{
	'MAINTAINER'	=> 'jdhedden',
	'DISTRIBUTION'	=> 'JDHEDDEN/Thread-Semaphore-2.09.tar.gz',
	'FILES'		=> q[dist/Thread-Semaphore],
	'EXCLUDED'	=> [ qw(examples/semaphore.pl
				t/00_load.t
				t/99_pod.t
				t/test.pl
			       ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'threads' =>
	{
	'MAINTAINER'	=> 'jdhedden',
	'DISTRIBUTION'	=> 'JDHEDDEN/threads-1.75.tar.gz',
	'FILES'		=> q[dist/threads],
	'EXCLUDED'	=> [ qw(examples/pool.pl
				t/pod.t
				t/test.pl
				threads.h
			       ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'threads::shared' =>
	{
	'MAINTAINER'	=> 'jdhedden',
	'DISTRIBUTION'	=> 'JDHEDDEN/threads-shared-1.32.tar.gz',
	'FILES'		=> q[dist/threads-shared],
	'EXCLUDED'	=> [ qw(examples/class.pl
				shared.h
				t/pod.t
				t/test.pl
			       ) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    'Tie::File' =>
	{
	'MAINTAINER'	=> 'mjd',
	'DISTRIBUTION'	=> 'MJD/Tie-File-0.96.tar.gz',
	'FILES'		=> q[cpan/Tie-File],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Tie::RefHash' =>
	{
	'MAINTAINER'	=> 'nuffin',
	'DISTRIBUTION'	=> 'NUFFIN/Tie-RefHash-1.38.tar.gz',
	'FILES'		=> q[cpan/Tie-RefHash],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Time::HiRes' =>
	{
	'MAINTAINER'	=> 'zefram',
	'DISTRIBUTION'	=> 'JHI/Time-HiRes-1.9719.tar.gz',
	'FILES'		=> q[cpan/Time-HiRes],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Time::Local' =>
	{
	'MAINTAINER'	=> 'drolsky',
	'DISTRIBUTION'	=> 'DROLSKY/Time-Local-1.1901.tar.gz',
	'FILES'		=> q[ext/Time-Local],
	'EXCLUDED'	=> [ qw(t/pod-coverage.t t/pod.t) ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	# Currently Time::Local is no longer backwards compatible with Pre-5.11 perls
	# the version in core has now deviated from the CPAN version. To re-dual-life
	# this module, we'll need to rewrite a hybrid version
	},

    'Time::Piece' =>
	{
	'MAINTAINER'	=> 'msergeant',
	'DISTRIBUTION'	=> 'MSERGEANT/Time-Piece-1.15.tar.gz',
	'FILES'		=> q[cpan/Time-Piece],
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'Unicode::Collate' =>
	{
	'MAINTAINER'	=> 'sadahiro',
	'DISTRIBUTION'	=> 'SADAHIRO/Unicode-Collate-0.52.tar.gz',
	'FILES'		=> q[cpan/Unicode-Collate],
			    # ignore experimental XS version
	'EXCLUDED'	=> [ qr{X$},
			     qw{disableXS enableXS }
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'first-come',
	},

    'Unicode::Normalize' =>
	{
	'MAINTAINER'	=> 'sadahiro',
	'DISTRIBUTION'	=> 'SADAHIRO/Unicode-Normalize-1.03.tar.gz',
	'FILES'		=> q[cpan/Unicode-Normalize],
	'EXCLUDED'	=> [ qw{MANIFEST.N Normalize.pmN disableXS enableXS }],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'first-come',
	},

    'version' =>
	{
	'MAINTAINER'	=> 'jpeacock',
	'DISTRIBUTION'	=> 'JPEACOCK/version-0.77.tar.gz',
	'FILES'		=> q[lib/version.pm lib/version.pod lib/version.t
			     lib/version],
	'EXCLUDED'	=> [ qr{^t/.*\.t$}, qw{t/survey_locales}, qr{^vutil/},
			     qw{lib/version/typemap},
			     qw{vperl/vpp.pm},
			   ],
	'MAP'		=> { 'lib/'	      => 'lib/',
			     't/coretests.pm' => 'lib/version.t',
			   },
	'CPAN'		=> 1,
	'UPSTREAM'	=> undef,
	},

    'vms' =>
	{
	'MAINTAINER'	=> 'craig',
	'FILES'		=> q[vms configure.com README.vms],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'VMS::DCLsym' =>
	{
	'MAINTAINER'	=> 'craig',
	'FILES'		=> q[ext/VMS-DCLsym],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'VMS::Stdio' =>
	{
	'MAINTAINER'	=> 'craig',
	'FILES'		=> q[ext/VMS-Stdio],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'warnings' =>
	{
	'MAINTAINER'	=> 'pmqs',
	'FILES'		=> q[warnings.pl
			     lib/warnings.{pm,t}
			     lib/warnings
			     t/lib/warnings
			    ],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'win32' =>
	{
	'MAINTAINER'	=> 'jand',
	'FILES'		=> q[win32 t/win32 README.win32 ext/Win32CORE],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},

    'Win32' =>
	{
	'MAINTAINER'	=> 'jand',
	'DISTRIBUTION'	=> "JDB/Win32-0.39.tar.gz",
	'FILES'		=> q[cpan/Win32],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'Win32API::File' =>
	{
	'MAINTAINER'	=> 'chorny',
	'DISTRIBUTION'	=> 'CHORNY/Win32API-File-0.1101.zip',
	'FILES'		=> q[cpan/Win32API-File],
	'EXCLUDED'	=> [ qr{^ex/},
			     qw{t/pod.t},
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'cpan',
	},

    'XS::APItest::KeywordRPN' =>
	{
	'MAINTAINER'	=> 'zefram',
	'FILES'		=> q[ext/XS-APItest-KeywordRPN],
	'CPAN'		=> 0,
	'UPSTREAM'	=> 'blead',
	},

    'XSLoader' =>
	{
	'MAINTAINER'	=> 'saper',
	'DISTRIBUTION'	=> 'SAPER/XSLoader-0.10.tar.gz',
	'FILES'		=> q[dist/XSLoader],
	'EXCLUDED'	=> [ qr{^eg/},
			     qw{t/pod.t
			        t/podcover.t
				t/portfs.t
				XSLoader.pm}, # we use XSLoader_pm.PL
			   ],
	'CPAN'		=> 1,
	'UPSTREAM'	=> 'blead',
	},

    's2p' =>
	{
	'MAINTAINER'	=> 'laun',
	'FILES'		=> q[x2p/s2p.PL],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},


    # this pseudo-module represents all the files under ext/ and lib/
    # that aren't otherwise claimed. This means that the following two
    # commands will check that every file under ext/ and lib/ is
    # accounted for, and that there are no duplicates:
    #
    #    perl Porting/Maintainers --checkmani lib ext
    #    perl Porting/Maintainers --checkmani

    '_PERLLIB' =>
	{
	'MAINTAINER'	=> 'p5p',
	'FILES'		=> q[
				ext/autouse/lib
				ext/autouse/t
				ext/B/B.pm
				ext/B/typemap
				ext/B/Makefile.PL
				ext/B/defsubs_h.PL
				ext/B/O.pm
				ext/B/B.xs
				ext/B/B/Terse.pm
				ext/B/B/Showlex.pm
				ext/B/B/Xref.pm
				ext/B/t/f_map
				ext/B/t/showlex.t
				ext/B/t/o.t
				ext/B/t/optree_varinit.t
				ext/B/t/concise-xs.t
				ext/B/t/optree_check.t
				ext/B/t/OptreeCheck.pm
				ext/B/t/optree_specials.t
				ext/B/t/f_sort.t
				ext/B/t/pragma.t
				ext/B/t/f_sort
				ext/B/t/b.t
				ext/B/t/optree_samples.t
				ext/B/t/optree_concise.t
				ext/B/t/optree_constants.t
				ext/B/t/optree_sort.t
				ext/B/t/terse.t
				ext/B/t/xref.t
				ext/B/t/f_map.t
				ext/B/t/optree_misc.t
				ext/B/hints/openbsd.pl
				ext/B/hints/darwin.pl

				ext/Devel-DProf/
				ext/Devel-Peek/
				ext/Devel-SelfStubber/
				ext/DynaLoader/
				    !ext/DynaLoader/t/XSLoader.t
				    !ext/DynaLoader/XSLoader_pm.PL
				ext/Errno
				ext/Fcntl/
				ext/File-Glob/
				ext/FileCache/lib
				ext/FileCache/t
				ext/GDBM_File/
				ext/Hash-Util-FieldHash/
				ext/Hash-Util/
				ext/I18N-Langinfo/
				ext/IPC-Open2/
				ext/IPC-Open3/
				ext/NDBM_File/
				ext/ODBM_File/
				ext/Opcode/
				ext/POSIX/
				ext/PerlIO-encoding/
				ext/PerlIO-scalar/
				ext/PerlIO-via/
				ext/SDBM_File/
				ext/Socket/
				ext/Sys-Hostname/
				ext/Tie-Memoize/
				ext/XS-APItest/
				ext/XS-Typemap/
				ext/attributes/
				ext/mro/
				ext/re/
				lib/AnyDBM_File.{pm,t}
				lib/Benchmark.{pm,t}
				lib/CORE.pod
				lib/Carp.{pm,t}
				lib/Carp/Heavy.pm
				lib/Class/Struct.{pm,t}
				lib/Config.t
				lib/Config_git.pl
				lib/Config/Extensions.{pm,t}
				lib/DB.{pm,t}
				lib/DBM_Filter.pm
				lib/DBM_Filter/
				lib/DirHandle.{pm,t}
				lib/Dumpvalue.{pm,t}
				lib/English.{pm,t}
				lib/Env.pm
				lib/Env/t/
				lib/ExtUtils/Embed.pm
				lib/ExtUtils/XSSymSet.pm
				lib/ExtUtils/t/Embed.t
				lib/ExtUtils/typemap
				lib/File/Basename.{pm,t}
				lib/File/CheckTree.{pm,t}
				lib/File/Compare.{pm,t}
				lib/File/Copy.{pm,t}
				lib/File/DosGlob.{pm,t}
				lib/File/Find.pm
				lib/File/Find/
				lib/File/stat.{pm,t}
				lib/FileHandle.{pm,t}
				lib/FindBin.{pm,t}
				lib/Getopt/Std.{pm,t}
				lib/I18N/Collate.{pm,t}
				lib/Internals.t
				lib/Module/Build/ConfigData.pm
				lib/Net/hostent.{pm,t}
				lib/Net/netent.{pm,t}
				lib/Net/protoent.{pm,t}
				lib/Net/servent.{pm,t}
				lib/PerlIO.pm
				lib/Pod/Functions.pm
				lib/Pod/Html.pm
				lib/Pod/t/Functions.t
				lib/Pod/t/InputObjects.t
				lib/Pod/t/Select.t
				lib/Pod/t/Usage.t
				lib/Pod/t/eol.t
				lib/Pod/t/html*
				lib/Pod/t/pod2html-lib.pl
				lib/Pod/t/utils.t
				lib/Search/Dict.{pm,t}
				lib/SelectSaver.{pm,t}
				lib/Symbol.{pm,t}
				lib/Term/Complete.{pm,t}
				lib/Term/ReadLine.{pm,t}
				lib/Text/Abbrev.{pm,t}
				lib/Thread.{pm,t}
				lib/Tie/Array.pm
				lib/Tie/Array/
				lib/Tie/Handle.pm
				lib/Tie/Handle/
				lib/Tie/Hash.pm
				lib/Tie/Hash/NamedCapture.pm
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
				lib/abbrev.pl
				lib/assert.pl
				lib/bigfloat{.pl,pl.t}
				lib/bigint{.pl,pl.t}
				lib/bigrat.pl
				lib/blib.{pm,t}
				lib/bytes.{pm,t}
				lib/bytes_heavy.pl
				lib/cacheout.pl
				lib/charnames.{pm,t}
				lib/complete.pl
				lib/ctime.pl
				lib/dbm_filter_util.pl
				lib/deprecate.pm
				lib/diagnostics.{pm,t}
				lib/dotsh.pl
				lib/dumpvar.{pl,t}
				lib/exceptions.pl
				lib/fastcwd.pl
				lib/feature.{pm,t}
				lib/feature/
				lib/filetest.{pm,t}
				lib/find.pl
				lib/finddepth.pl
				lib/flush.pl
				lib/getcwd.pl
				lib/getopt.pl
				lib/getopts.pl
				lib/h2ph.t
				lib/h2xs.t
				lib/hostname.pl
				lib/importenv.pl
				lib/integer.{pm,t}
				lib/legacy.{pm,t}
				lib/less.{pm,t}
				lib/locale.{pm,t}
				lib/look.pl
				lib/open.{pm,t}
				lib/open2.pl
				lib/open3.pl
				lib/overload{.pm,.t,64.t}
				lib/overload/numbers.pm
				lib/overloading.{pm,t}
				lib/perl5db.{pl,t}
				lib/perl5db/
				lib/pwd.pl
				lib/shellwords.pl
				lib/sigtrap.{pm,t}
				lib/sort.{pm,t}
				lib/stat.pl
				lib/strict.{pm,t}
				lib/subs.{pm,t}
				lib/syslog.pl
				lib/tainted.pl
				lib/termcap.pl
				lib/timelocal.pl
				lib/unicore/
				lib/utf8.{pm,t}
				lib/utf8_heavy.pl
				lib/validate.pl
				lib/vars{.pm,.t,_carp.t}
				lib/vmsish.{pm,t}
			    ],
	'CPAN'		=> 0,
	'UPSTREAM'	=> undef,
	},
);

1;
