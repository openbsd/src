# A simple listing of core files that have specific maintainers,
# or at least someone that can be called an "interested party".
# Also, a "module" does not necessarily mean a CPAN module, it
# might mean a file or files or a subdirectory.
# Most (but not all) of the modules have dual lives in the core
# and in CPAN.  Those that have a CPAN existence, have the CPAN
# attribute set to true.

package Maintainers;

%Maintainers =
	(
	'abergman'	=> 'Arthur Bergman <abergman@cpan.org>',
	'ams'		=> 'Abhijit Menon-Sen <ams@cpan.org>',
	'andk'		=> 'Andreas J. Koenig <andk@cpan.org>',
	'bbb'		=> 'Rob Brown <bbb@cpan.org>',
	'craig'		=> 'Craig Berry <craigberry@mac.com>',
	'dankogai'	=> 'Dan Kogai <dankogai@cpan.org>',
	'dconway'	=> 'Damian Conway <dconway@cpan.org>',
	'drolsky'	=> 'Dave Rolsky <drolsky@cpan.org>',
	'elizabeth'	=> 'Elizabeth Mattijsen <liz@dijkmat.nl>',
	'gbarr'		=> 'Graham Barr <gbarr@cpan.org>',
	'gaas'		=> 'Gisle Aas <gaas@cpan.org>',
	'gsar'		=> 'Gurusamy Sarathy <gsar@activestate.com>',
	'ilyam'		=> 'Ilya Martynov <ilyam@cpan.org>',
	'ilyaz'		=> 'Ilya Zakharevich <ilyaz@cpan.org>',
	'jhi'		=> 'Jarkko Hietaniemi <jhi@cpan.org>',
	'jstowe'	=> 'Jonathan Stowe <jstowe@cpan.org>',
	'jv'		=> 'Johan Vromans <jv@cpan.org>',
	'kwilliams'	=> 'Ken Williams <kwilliams@cpan.org>',
	'laun'		=> 'Wolfgang Laun <Wolfgang.Laun@alcatel.at>',
	'lstein'	=> 'Lincoln D. Stein <lds@cpan.org>',
	'marekr'	=> 'Marek Rouchal <marekr@cpan.org>',
	'mhx'		=> 'Marcus Holland-Moritz <mhx@cpan.org>',
	'mjd'		=> 'Mark-Jason Dominus <mjd@cpan.org>',
	'muir'		=> 'David Muir Sharnoff <muir@cpan.org>',
	'neilb'		=> 'Neil Bowers <neilb@cpan.org>',
	'ni-s'		=> 'Nick Ing-Simmons <nick@ing-simmons.net>',
	'p5p'		=> 'perl5-porters <perl5-porters@perl.org>',
	'perlfaq'	=> 'perlfaq-workers <perlfaq-workers@perl.org>',
	'perlref'	=> 'Mark-Jason Dominus <mjd@cpan.org>',
	'petdance'	=> 'Andy Lester <petdance@cpan.org>',
	'pmqs'		=> 'Paul Marquess <pmqs@cpan.org>',
	'pvhp'		=> 'Peter Prymmer <pvhp@best.com>',
	'rmbarker'	=> 'Robin Barker <rmbarker@cpan.org>',
	'rra'		=> 'Russ Allbery <rra@cpan.org>',
	'sadahiro'	=> 'SADAHIRO Tomoyuki <SADAHIRO@cpan.org>',
	'sburke'	=> 'Sean Burke <sburke@cpan.org>',
	'mschwern'	=> 'Michael Schwern <mschwern@cpan.org>',
	'smccam'	=> 'Stephen McCamant <smccam@cpan.org>',
	'tels'		=> 'perl_dummy a-t bloodgate.com',
	'tjenness'	=> 'Tim Jenness <tjenness@cpan.org>'
	);

# The FILES is either filenames, or glob patterns, or directory
# names to be recursed down.  The CPAN can be either 1 (get the
# latest one from CPAN) or 0 (there is no valid CPAN release).

%Modules = (

	'Attribute::Handlers' =>
		{
		'MAINTAINER'	=> 'abergman',
		'FILES'		=> q[lib/Attribute/Handlers.pm
				     lib/Attribute/Handlers],
		'CPAN'		=> 1,
		},

	'B::Concise' =>
		{
		'MAINTAINER'	=> 'smccam',
		'FILES'		=> q[ext/B/B/Concise.pm ext/B/t/concise.t],
		'CPAN'		=> 0,
		},

	'B::Deparse' =>
		{
		'MAINTAINER'	=> 'smccam',
		'FILES'		=> q[ext/B/B/Deparse.pm ext/B/t/deparse.t],
		'CPAN'		=> 0,
		},

	'bignum' =>
		{
		'MAINTAINER'	=> 'tels',
		'FILES'		=> q[lib/big{int,num,rat}.pm lib/bignum],
		'CPAN'		=> 1,
		},

	'CGI' =>
		{
		'MAINTAINER'	=> 'lstein',
		'FILES'		=> q[lib/CGI.pm lib/CGI],
		'CPAN'		=> 1,
		},

	'Class::ISA' =>
		{
		'MAINTAINER'	=> 'sburke',
		'FILES'		=> q[lib/Class/ISA.pm lib/Class/ISA],
		'CPAN'		=> 1,
		},

	'CPAN' =>
		{
		'MAINTAINER'	=> 'andk',
		'FILES'		=> q[lib/CPAN.pm lib/CPAN],
		'CPAN'		=> 1,
		},

	'Cwd' =>
		{
		'MAINTAINER'	=> 'kwilliams',
		'FILES'		=> q[ext/Cwd lib/Cwd.pm],
		'CPAN'		=> 1,
		},

	'Data::Dumper' =>
		{
		'MAINTAINER'	=> 'ilyam', # Not gsar.
		'FILES'		=> q[ext/Data/Dumper],
		'CPAN'		=> 1,
		},

	'DB::File' =>
		{
		'MAINTAINER'	=> 'pmqs',
		'FILES'		=> q[ext/DB_File],
		'CPAN'		=> 1,
		},

	'Devel::PPPort' =>
		{
		'MAINTAINER'	=> 'mhx',
		'FILES'		=> q[ext/Devel/PPPort],
		'CPAN'		=> 1,
		},

	'Digest' =>
		{
		'MAINTAINER'	=> 'gaas',
		'FILES'		=> q[lib/Digest.pm lib/Digest/base.pm lib/Digest],
		'CPAN'		=> 1,
		},

	'Digest::MD5' =>
		{
		'MAINTAINER'	=> 'gaas',
		'FILES'		=> q[ext/Digest/MD5],
		'CPAN'		=> 1,
		},

	'Encode' =>
		{
		'MAINTAINER'	=> 'dankogai',
		'FILES'		=> q[ext/Encode],
		'CPAN'		=> 1,
		},

	'Errno' =>
		{
		'MAINTAINER'	=> 'p5p', # Not gbarr.
		'FILES'		=> q[ext/Errno],
		'CPAN'		=> 0,
		},

	'ExtUtils::MakeMaker' =>
		{
		'MAINTAINER'	=> 'mschwern',
		'FILES'		=> q[lib/ExtUtils/{Command,Install,Installed,Liblist,MakeMaker,Manifest,Mkbootstrap,Mksymlists,MM*,MY,Packlist,testlib}.pm lib/ExtUtils/{Command,Liblist,MakeMaker}
				     lib/ExtUtils/t t/lib/MakeMaker t/lib/TieIn.pm t/lib/TieOut.pm],
		'CPAN'		=> 1,
		},

	'faq' =>
		{
		'MAINTAINER'	=> 'perlfaq',
		'FILES'		=> q[pod/perlfaq*],
		'CPAN'		=> 0,
		},

	'File::Spec' =>
		{
		'MAINTAINER'	=> 'kwilliams',
		'FILES'		=> q[lib/File/Spec.pm lib/File/Spec],
		'CPAN'		=> 1,
		},

	'File::Temp' =>
		{
		'MAINTAINER'	=> 'tjenness',
		'FILES'		=> q[lib/File/Temp.pm lib/File/Temp],
		'CPAN'		=> 1,
		},

	'Filter::Simple' =>
		{
		'MAINTAINER'	=> 'dconway',
		'FILES'		=> q[lib/Filter/Simple.pm lib/Filter/Simple
				     t/lib/Filter/Simple],
		'CPAN'		=> 1,
		},

	'Filter::Util::Call' =>
		{
		'MAINTAINER'	=> 'pmqs',
		'FILES'		=> q[ext/Filter/Util/Call ext/Filter/t/call.t
				     t/lib/filter-util.pl],
		'CPAN'		=> 1,
		},

	'Getopt::Long' =>
		{
		'MAINTAINER'	=> 'jv',
		'FILES'		=> q[lib/Getopt/Long.pm lib/Getopt/Long],
		'CPAN'		=> 1,
		},

	'I18N::LangTags' =>
		{
		'MAINTAINER'	=> 'sburke',
		'FILES'		=> q[lib/I18N/LangTags.pm lib/I18N/LangTags],
		'CPAN'		=> 1,
		},

	'if' =>
		{
		'MAINTAINER'	=> 'ilyaz',
		'FILES'		=> q[lib/if.{pm,t}],
		'CPAN'		=> 1,
		},

	'IO' =>
		{
		'MAINTAINER'	=> 'p5p', # Not gbarr.
		'FILES'		=> q[ext/IO],
		'CPAN'		=> 0,
		},

	'libnet' =>
		{
		'MAINTAINER'	=> 'gbarr',
		'FILES'		=>
			q[lib/Net/{Cmd,Config,Domain,FTP,Netrc,NNTP,POP3,SMTP,Time}.pm lib/Net/ChangeLog.libnet lib/Net/FTP lib/Net/*.eg lib/Net/libnetFAQ.pod lib/Net/README.libnet lib/Net/t],
		'CPAN'		=> 1,
		},

	'Scalar-List-Utils' =>
		{
		'MAINTAINER'	=> 'gbarr',
		'FILES'		=> q[ext/List/Util],
		'CPAN'		=> 1,
		},

	'Locale::Codes' =>
		{
		'MAINTAINER'	=> 'neilb',
		'FILES'		=> q[lib/Locale/{Codes,Constants,Country,Currency,Language,Script}*],
		'CPAN'		=> 1,
		},

	'Locale::Maketext' =>
		{
		'MAINTAINER'	=> 'sburke',
		'FILES'		=> q[lib/Locale/Maketext.pm lib/Locale/Maketext],
		'CPAN'		=> 1,
		},

	'Math::BigFloat' =>
		{
		'MAINTAINER'	=> 'tels',
		'FILES'		=> q[lib/Math/BigFloat.pm lib/Math/BigFloat],
		'CPAN'		=> 1,
		},

	'Math::BigInt' =>
		{
		'MAINTAINER'	=> 'tels',
		'FILES'		=> q[lib/Math/BigInt.pm lib/Math/BigInt
				     t/lib/Math],
		'CPAN'		=> 1,
		},

	'Math::BigRat' =>
		{
		'MAINTAINER'	=> 'tels',
		'FILES'		=> q[lib/Math/BigRat.pm lib/Math/BigRat],
		'CPAN'		=> 1,
		},

	'Memoize' =>
		{
		'MAINTAINER'	=> 'mjd',
		'FILES'		=> q[lib/Memoize.pm lib/Memoize],
		'CPAN'		=> 1,
		},

	'MIME::Base64' =>
		{
		'MAINTAINER'	=> 'gaas',
		'FILES'		=> q[ext/MIME/Base64],
		'CPAN'		=> 1,
		},

	'Net::Ping' =>
		{
		'MAINTAINER'	=> 'bbb',
		'FILES'		=> q[lib/Net/Ping.pm lib/Net/Ping],
		'CPAN'		=> 1,
		},

	'NEXT' =>
		{
		'MAINTAINER'	=> 'dconway',
		'FILES'		=> q[lib/NEXT.pm lib/NEXT],
		'CPAN'		=> 1,
		},

	'perlebcdic' =>
		{
		'MAINTAINER'	=> 'pvhp',
		'FILES'		=> q[pod/perlebcdic.pod],
		'CPAN'		=> 0,
		},

	'PerlIO' =>
		{
		'MAINTAINER'	=> 'p5p',
		'FILES'		=> q[ext/PerlIO],
		'CPAN'		=> 0,
		},

	'perlio-doc' =>
		{
		'MAINTAINER'	=> 'ni-s',
		'FILES'		=> q[pod/perlapio.pod
				     pod/perliol.pod
				     lib/PerlIO.pm],
		'CPAN'		=> 0,
		},

	'PerlIO::via::QuotedPrint' =>
		{
		'MAINTAINER'	=> 'elizabeth',
		'FILES'		=> q[lib/PerlIO/via/QuotedPrint.pm
				     lib/PerlIO/via/t/QuotedPrint.t],
		'CPAN'		=> 1,
		},

	'perlref' =>
		{
		'MAINTAINER'	=> 'mjd',
		'FILES'		=> q[pod/perlref.pod],
		'CPAN'		=> 0,
		},

	'perlpacktut' =>
		{
		'MAINTAINER'	=> 'laun',
		'FILES'		=> q[pod/perlpacktut.pod],
		'CPAN'		=> 0,
		},

	'perlpodspec' =>
		{
		'MAINTAINER'	=> 'sburke',
		'FILES'		=> q[pod/perlpodspec.pod],
		'CPAN'		=> 0,
		},

	'perlthrtut' =>
		{
		'MAINTAINER'	=> 'elizabeth',
		'FILES'		=> q[pod/perlthrtut.pod],
		'CPAN'		=> 0,
		},

	'PodParser' => {
		'MAINTAINER'	=> 'marekr',
		'FILES' => q[lib/Pod/{InputObjects,Parser,ParseUtils,Select,PlainText,Usage,Checker,Find}.pm pod/pod{select,2usage,checker}.PL t/pod/testcmp.pl t/pod/testp2pt.pl t/pod/testpchk.pl t/pod/emptycmd.* t/pod/find.t t/pod/for.* t/pod/headings.* t/pod/include.* t/pod/included.* t/pod/lref.* t/pod/multiline_items.* t/pod/nested_items.* t/pod/nested_seqs.* t/pod/oneline_cmds.* t/pod/poderrs.* t/pod/pod2usage.* t/pod/podselect.* t/pod/special_seqs.*],
		'CPAN'		=> 1,
		},

	'Pod::LaTeX' =>
		{
		'MAINTAINER'	=> 'tjenness',
		'FILES'		=> q[lib/Pod/LaTeX.pm lib/Pod/t/pod2latex.t],
		'CPAN'		=> 1,
		},

	'podlators' =>
		{
		'MAINTAINER'	=> 'rra',
		'FILES'		=> q[lib/Pod/{Html,Man,ParseLink,Text,Text/{Color,Overstrike,Termcap}}.pm pod/pod2man.PL pod/pod2text.PL lib/Pod/t/{basic.*,{man,parselink,text*}.t}],
		'CPAN'		=> 1,
		},

	'Pod::Perldoc' =>
		{
		'MAINTAINER'	=> 'sburke',
		'FILES'		=> q[lib/Pod/Perldoc.pm lib/Pod/Perldoc],
		'CPAN'		=> 1,
		},

	'Pod::Plainer' =>
		{
		'MAINTAINER'	=> 'rmbarker',
		'FILES'		=> q[lib/Pod/Plainer.pm t/pod/plainer.t],
		'CPAN'		=> 0,
		},

	'Safe' =>
		{
		'MAINTAINER'	=> 'abergman',
		'FILES'		=> q[ext/Safe],
		'CPAN'		=> 1,
		},

	'Storable' =>
		{
		'MAINTAINER'	=> 'ams',
		'FILES'		=> q[ext/Storable],
		'CPAN'		=> 1,
		},

	'Switch' =>
		{
		'MAINTAINER'	=> 'dconway',
		'FILES'		=> q[lib/Switch.pm lib/Switch],
		'CPAN'		=> 1,
		},

	'TabsWrap' =>
		{
		'MAINTAINER'	=> 'muir',
		'FILES'		=>
			q[lib/Text/{Tabs,Wrap}.pm lib/Text/TabsWrap],
		'CPAN'		=> 1,
		},

	'Text::Balanced' =>
		{
		'MAINTAINER'	=> 'dconway',
		'FILES'		=> q[lib/Text/Balanced.pm lib/Text/Balanced],
		'CPAN'		=> 1,
		},

	'Term::ANSIColor' =>
		{
		'MAINTAINER'	=> 'rra',
		'FILES'		=> q[lib/Term/ANSIColor.pm lib/Term/ANSIColor],
		'CPAN'		=> 1,
		},

	'Test' =>
		{
		'MAINTAINER'	=> 'sburke',
		'FILES'		=> q[lib/Test.pm lib/Test/t],
		'CPAN'		=> 1,
		},

	'Test::Builder' =>
		{
		'MAINTAINER'	=> 'mschwern',
		'FILES'		=> q[lib/Test/Builder.pm],
		'CPAN'		=> 1,
		},

	'Test::Harness' =>
		{
		'MAINTAINER'	=> 'petdance',
		'FILES'		=> q[lib/Test/Harness.pm lib/Test/Harness
				     t/lib/sample-tests],
		'CPAN'		=> 1,
		},

	'Test::More' =>
		{
		'MAINTAINER'	=> 'mschwern',
		'FILES'		=> q[lib/Test/More.pm],
		'CPAN'		=> 1,
		},

	'Test::Simple' =>
		{
		'MAINTAINER'	=> 'mschwern',
		'FILES'		=> q[lib/Test/Simple.pm lib/Test/Simple
				     t/lib/Test/Simple],
		'CPAN'		=> 1,
		},

	'Term::Cap' =>
		{
		'MAINTAINER'	=> 'jstowe',
		'FILES'		=> q[lib/Term/Cap.{pm,t}],
		'CPAN'		=> 1,
		},

	'threads' =>
		{
		'MAINTAINER' => 'abergman',
		'FILES'	 => q[ext/threads],
		'CPAN'		=> 0,
		},

	'Tie::File' =>
		{
		'MAINTAINER'	=> 'mjd',
		'FILES'		=> q[lib/Tie/File.pm lib/Tie/File],
		'CPAN'		=> 1,
		},

	'Time::HiRes' =>
		{
		'MAINTAINER'	=> 'jhi',
		'FILES'		=> q[ext/Time/HiRes],
		'CPAN'		=> 1,
		},

	'Time::Local' =>
		{
		'MAINTAINER'	=> 'drolsky',
		'FILES'		=> q[lib/Time/Local.{pm,t}],
		'CPAN'		=> 1,
		},

	'Unicode::Collate' =>
		{
		'MAINTAINER'	=> 'sadahiro',
		'FILES'		=> q[lib/Unicode/Collate.pm
				     lib/Unicode/Collate],
		'CPAN'		=> 1,
		},

	'Unicode::Normalize' =>
		{
		'MAINTAINER'	=> 'sadahiro',
		'FILES'		=> q[ext/Unicode/Normalize],
		'CPAN'		=> 1,
		},

	'vms' =>
		{
		'MAINTAINER'	=> 'craig',
		'FILES'		=> q[vms configure.com README.vms],
		'CPAN'		=> 0,
		},

	'warnings' =>
		{
		'MAINTAINER'	=> 'pmqs',
		'FILES'		=> q[warnings.pl lib/warnings.{pm,t}
				     lib/warnings t/lib/warnings],
		'CPAN'		=> 0,
		},

	'win32' =>
		{
		'MAINTAINER'	=> 'gsar',
		'FILES'		=> q[win32 README.win32 lib/Win32.pod t/win32],
		'CPAN'		=> 0,
		},

	's2p' =>
		{
		'MAINTAINER'	=> 'laun',
		'FILES'		=> q[x2p/s2p.PL],
		'CPAN'		=> 0,
		},

	);

1;
