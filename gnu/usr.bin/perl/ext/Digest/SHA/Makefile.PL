require 5.006000;

use ExtUtils::MakeMaker;
use Getopt::Std;
use Config;

my %opts;
getopts('tx', \%opts);

my @defines = ('-DSHA_PERL_MODULE');

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

WriteMakefile(
	'NAME'		=> 'Digest::SHA',
	'VERSION_FROM'	=> 'SHA.pm',
	'PREREQ_PM'	=> {},
	'AUTHOR'	=> 'Mark Shelor <mshelor@cpan.org>',
	'LIBS'		=> [''],
        MAN3PODS        => {},      # Pods will be built by installman.
	'DEFINE'	=> $define,
	'INC'		=> '-I.',
	'NO_META'	=> 1,
);
