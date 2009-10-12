require 5.003000;

use ExtUtils::MakeMaker;
use Getopt::Std;
use Config;

my $PM = 'lib/Digest/SHA.pm';

my $PERL_CORE = 0;
$PERL_CORE = 1 if grep { $_ eq 'PERL_CORE=1' } @ARGV;

my %opts;
getopts('tx', \%opts);

my @defines;
push(@defines, '-DSHA_PERL_MODULE') if $] >= 5.004;
push(@defines, '-DSHA_THREAD_SAFE') if $opts{'t'};
push(@defines, '-DNO_SHA_384_512')  if $opts{'x'};

	# Configure SHA source to use static arrays for
	# message schedules if compiling on Intel platforms.
	# This seems to speed things up a bit.  However,
	# DON'T do this if thread-safe option is in force.
	
if ($Config{archname} =~ /^i[3456]86/) {
	push(@defines, '-DSHA_STO_CLASS=static') unless $opts{'t'};
}

my $define = join(' ', @defines);

my %att = (
	'NAME'		=> 'Digest::SHA',
	'VERSION_FROM'	=> $PM,
	'LIBS'		=> [''],
	'DEFINE'	=> $define,
	'INC'		=> '-I.',
	$PERL_CORE ? () : (
	    'EXE_FILES'		=> [ 'shasum' ],
	    'INSTALLDIRS'	=> ($] >= 5.010) ? 'perl' : 'site',
	),
);

my $MMversion = $ExtUtils::MakeMaker::VERSION || 0;
$att{NO_META} = 1 unless $MMversion < 6.10_03;

WriteMakefile(%att);
