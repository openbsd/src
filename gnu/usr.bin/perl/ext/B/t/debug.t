#!./perl

BEGIN {
    chdir 't' if -d 't';
    if ($^O eq 'MacOS') {
	@INC = qw(: ::lib ::macos:lib);
    } else {
	@INC = '.';
	push @INC, '../lib';
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

print "1..3\n";

my $test = 1;

sub ok { print "ok $test\n"; $test++ }


my $a;
my $Is_VMS = $^O eq 'VMS';
my $Is_MacOS = $^O eq 'MacOS';

my $path = join " ", map { qq["-I$_"] } @INC;
my $redir = $Is_MacOS ? "" : "2>&1";

$a = `$^X $path "-MO=Debug" -e 1 $redir`;
print "not " unless $a =~
/\bLISTOP\b.*\bOP\b.*\bCOP\b.*\bOP\b/s;
ok;


$a = `$^X $path "-MO=Terse" -e 1 $redir`;
print "not " unless $a =~
/\bLISTOP\b.*leave.*\n    OP\b.*enter.*\n    COP\b.*nextstate.*\n    OP\b.*null/s;
ok;

$a = `$^X $path "-MO=Terse" -ane "s/foo/bar/" $redir`;
$a =~ s/\(0x[^)]+\)//g;
$a =~ s/\[[^\]]+\]//g;
$a =~ s/-e syntax OK//;
$a =~ s/[^a-z ]+//g;
$a =~ s/\s+/ /g;
$a =~ s/\b(s|foo|bar|ullsv)\b\s?//g;
$a =~ s/^\s+//;
$a =~ s/\s+$//;
my $is_thread = $Config{use5005threads} && $Config{use5005threads} eq 'define';
if ($is_thread) {
    $b=<<EOF;
leave enter nextstate label leaveloop enterloop null and defined null
threadsv readline gv lineseq nextstate aassign null pushmark split pushre
threadsv const null pushmark rvav gv nextstate subst const unstack
EOF
} else {
    $b=<<EOF;
leave enter nextstate label leaveloop enterloop null and defined null
null gvsv readline gv lineseq nextstate aassign null pushmark split pushre
null gvsv const null pushmark rvav gv nextstate subst const unstack
EOF
}
$b=~s/\n/ /g;$b=~s/\s+/ /g;
$b =~ s/\s+$//;
print "# [$a]\n# vs\n# [$b]\nnot " if $a ne $b;
ok;

