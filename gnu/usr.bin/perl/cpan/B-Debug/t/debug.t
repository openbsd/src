#!./perl

BEGIN {
    delete $ENV{PERL_DL_NONLAZY} if $] < 5.005_58; #Perl_byterun problem
    if ($ENV{PERL_CORE}){
	chdir('t') if -d 't';
	if ($^O eq 'MacOS') {
	    @INC = qw(: ::lib ::macos:lib);
	} else {
	    @INC = '.';
	    push @INC, '../lib';
	}
    } else {
	unshift @INC, 't';
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
use Test::More tests => 8;
use B;
use B::Debug;

my $a;
my $Is_VMS = $^O eq 'VMS';
my $Is_MacOS = $^O eq 'MacOS';

my $path = join " ", map { qq["-I$_"] } @INC;
my $redir = $Is_MacOS ? "" : "2>&1";

$a = `$^X $path "-MO=Debug" -e 1 $redir`;
like($a, qr/\bLISTOP\b.*\bOP\b.*\bCOP\b.*\bOP\b/s);


$a = `$^X $path "-MO=Terse" -e 1 $redir`;
like($a, qr/\bLISTOP\b.*leave.*\n    OP\b.*enter.*\n    COP\b.*nextstate.*\n    OP\b.*null/s);

$a = `$^X $path "-MO=Terse" -ane "s/foo/bar/" $redir`;
$a =~ s/\(0x[^)]+\)//g;
$a =~ s/\[[^\]]+\]//g;
$a =~ s/-e syntax OK//;
$a =~ s/[^a-z ]+//g;
$a =~ s/\s+/ /g;
$a =~ s/\b(s|foo|bar|ullsv)\b\s?//g;
$a =~ s/^\s+//;
$a =~ s/\s+$//;
$a =~ s/\s+nextstate$//; # if $] < 5.008001; # 5.8.0 adds it. 5.8.8 not anymore
my $is_thread = $Config{use5005threads} && $Config{use5005threads} eq 'define';
if ($is_thread) {
    $b=<<EOF;
leave enter nextstate label leaveloop enterloop null and defined null
threadsv readline gv lineseq nextstate aassign null pushmark split pushre
threadsv const null pushmark rvav gv nextstate subst const unstack
EOF
} else {
  $b=<<EOF;
leave enter nextstate label leaveloop enterloop null and defined null null
gvsv readline gv lineseq nextstate aassign null pushmark split pushre null
gvsv const null pushmark rvav gv nextstate subst const unstack
EOF
}
#$b .= " nextstate" if $] < 5.008001; # ??
$b=~s/\n/ /g;$b=~s/\s+/ /g;
$b =~ s/\s+$//;
is($a, $b);

like(B::Debug::_printop(B::main_root),  qr/LISTOP\s+\[OP_LEAVE\]/);
like(B::Debug::_printop(B::main_start), qr/OP\s+\[OP_ENTER\]/);

$a = `$^X $path "-MO=Debug" -e "B::main_root->debug" $redir`;
like($a, qr/op_next\s+0x0/m);
$a = `$^X $path "-MO=Debug" -e "B::main_start->debug" $redir`;
like($a, qr/PL_ppaddr\[OP_ENTER\]/m);

# pass missing FETCHSIZE, fixed with 1.06
my $tmp = "tmp.pl";
open TMP, "> $tmp";
print TMP 'BEGIN{tie @a, __PACKAGE__;sub TIEARRAY {bless{}} sub FETCH{1}};
print $a[1]';
close TMP;
$a = `$^X $path "-MO=Debug" $tmp $redir`;
unlink $tmp;
unlike($a, qr/locate object method "FETCHSIZE"/m);
