#!./perl

BEGIN {
    chdir 't' if -d 't';
    if ($^O eq 'MacOS') {
	@INC = qw(: ::lib ::macos:lib);
    } else {
	@INC = '../lib';
    }
    require Config;
    if (($Config::Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
}

$|  = 1;
use warnings;
use strict;
use Config;

print "1..1\n";

my $test = 1;

sub ok { print "ok $test\n"; $test++ }

my $a;
my $Is_VMS = $^O eq 'VMS';
my $Is_MacOS = $^O eq 'MacOS';

my $path = join " ", map { qq["-I$_"] } @INC;
$path = '"-I../lib" "-Iperl_root:[lib]"' if $Is_VMS;   # gets too long otherwise
my $redir = $Is_MacOS ? "" : "2>&1";
my $is_thread = $Config{use5005threads} && $Config{use5005threads} eq 'define';

if ($is_thread) {
    print "# use5005threads: test $test skipped\n";
} else {
    $a = `$^X $path "-MO=Showlex" -e "my \@one" $redir`;
    print "# [$a]\nnot " unless $a =~ /sv_undef.*PVNV.*\@one.*sv_undef.*AV/s;
}
ok;
