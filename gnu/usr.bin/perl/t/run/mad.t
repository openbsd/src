#!./perl
#
# Tests for Perl mad environment
#
# $PERL_XMLDUMP

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    require './test.pl';
    skip_all_without_config('mad');
}

use File::Path;
use File::Spec;

my $tempdir = tempfile;

mkdir $tempdir, 0700 or die "Can't mkdir '$tempdir': $!";
unshift @INC, '../../lib';
my $cleanup = 1;

END {
    if ($cleanup) {
	rmtree($tempdir);
    }
}

plan tests => 4;

{
    delete local $ENV{$_} for keys %ENV;
    my $fn = File::Spec->catfile(File::Spec->curdir(), "withoutT.xml");
    $ENV{PERL_XMLDUMP} = $fn;
    fresh_perl_is('print q/hello/', '', {}, 'mad without -T');
    ok(-f $fn, "xml file created without -T as expected");
}

{
    delete local $ENV{$_} for keys %ENV;
    my $fn = File::Spec->catfile(File::Spec->curdir(), "withT.xml");
    fresh_perl_is('print q/hello/', 'hello', { switches => [ "-T" ] },
		  'mad with -T');
    ok(!-e $fn, "no xml file created with -T as expected");
}
