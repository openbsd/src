BEGIN {
    if ($ENV{'PERL_CORE'}){
        chdir 't';
        unshift @INC, '../lib';
    }
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bEncode\b/) {
      print "1..0 # Skip: Encode was not built\n";
      exit 0;
    }
    if (ord("A") == 193) {
	print "1..0 # Skip: EBCDIC\n";
	exit 0;
    }
    $| = 1;
}

use strict;
#use Test::More qw(no_plan);
use Test::More tests => 36;
use Encode q(:all);

my $uo = '';
my $nf  = '';
my ($af, $aq, $ap, $ah, $ax, $uf, $uq, $up, $uh, $ux);
for my $i (0x20..0x7e){
    $uo .= chr($i);
}
$af = $aq = $ap = $ah = $ax = 
$uf = $uq = $up = $uh = $ux = 
$nf = $uo;

my $residue = '';
for my $i (0x80..0xff){
    $uo   .= chr($i);
    $residue    .= chr($i);
    $af .= '?';
    $uf .= "\x{FFFD}";
    $ap .= sprintf("\\x{%04x}", $i);
    $up .= sprintf("\\x%02X", $i);
    $ah .= sprintf("&#%d;", $i);
    $uh .= sprintf("&#%d;", $i);
    $ax .= sprintf("&#x%x;", $i);
    $ux .= sprintf("&#x%x;", $i);
}

my $ao = $uo;
utf8::upgrade($uo);

my $ascii  = find_encoding('ascii');
my $utf8   = find_encoding('utf8');

my $src = $uo;
my $dst = $ascii->encode($src, FB_DEFAULT);
is($dst, $af, "FB_DEFAULT ascii");
is($src, $uo, "FB_DEFAULT residue ascii");

$src = $ao;
$dst = $utf8->decode($src, FB_DEFAULT);
is($dst, $uf, "FB_DEFAULT utf8");
is($src, $ao, "FB_DEFAULT residue utf8");

$src = $uo;
eval{ $dst = $ascii->encode($src, FB_CROAK) };
like($@, qr/does not map to ascii/o, "FB_CROAK ascii");
is($src, $uo, "FB_CROAK residue ascii");

$src = $ao;
eval{ $dst = $utf8->decode($src, FB_CROAK) };
like($@, qr/does not map to Unicode/o, "FB_CROAK utf8");
is($src, $ao, "FB_CROAK residue utf8");

$src = $nf;
eval{ $dst = $ascii->encode($src, FB_CROAK) };
is($@, '', "FB_CROAK on success ascii");
is($src, '', "FB_CROAK on success residue ascii");

$src = $nf;
eval{ $dst = $utf8->decode($src, FB_CROAK) };
is($@, '', "FB_CROAK on success utf8");
is($src, '', "FB_CROAK on success residue utf8");

$src = $uo;
$dst = $ascii->encode($src, FB_QUIET);
is($dst, $aq,   "FB_QUIET ascii");
is($src, $residue, "FB_QUIET residue ascii");

$src = $ao;
$dst = $utf8->decode($src, FB_QUIET);
is($dst, $uq,   "FB_QUIET utf8");
is($src, $residue, "FB_QUIET residue utf8");

{
    my $message = '';
    local $SIG{__WARN__} = sub { $message = $_[0] };

    $src = $uo;
    $dst = $ascii->encode($src, FB_WARN);
    is($dst, $aq,   "FB_WARN ascii");
    is($src, $residue, "FB_WARN residue ascii");
    like($message, qr/does not map to ascii/o, "FB_WARN message ascii");

    $message = '';
    $src = $ao;
    $dst = $utf8->decode($src, FB_WARN);
    is($dst, $uq,   "FB_WARN utf8");
    is($src, $residue, "FB_WARN residue utf8");
    like($message, qr/does not map to Unicode/o, "FB_WARN message utf8");

    $message = '';
    $src = $uo;
    $dst = $ascii->encode($src, WARN_ON_ERR);
    is($dst, $af, "WARN_ON_ERR ascii");
    is($src, '',  "WARN_ON_ERR residue ascii");
    like($message, qr/does not map to ascii/o, "WARN_ON_ERR message ascii");

    $message = '';
    $src = $ao;
    $dst = $utf8->decode($src, WARN_ON_ERR);
    is($dst, $uf, "WARN_ON_ERR utf8");
    is($src, '',  "WARN_ON_ERR residue utf8");
    like($message, qr/does not map to Unicode/o, "WARN_ON_ERR message ascii");
}

$src = $uo;
$dst = $ascii->encode($src, FB_PERLQQ);
is($dst, $ap,   "FB_PERLQQ ascii");
is($src, '', "FB_PERLQQ residue ascii");

$src = $ao;
$dst = $utf8->decode($src, FB_PERLQQ);
is($dst, $up,   "FB_PERLQQ utf8");
is($src, '', "FB_PERLQQ residue utf8");

$src = $uo;
$dst = $ascii->encode($src, FB_HTMLCREF);
is($dst, $ah,   "FB_HTMLCREF ascii");
is($src, '', "FB_HTMLCREF residue ascii");

#$src = $ao;
#$dst = $utf8->decode($src, FB_HTMLCREF);
#is($dst, $uh,   "FB_HTMLCREF utf8");
#is($src, '', "FB_HTMLCREF residue utf8");

$src = $uo;
$dst = $ascii->encode($src, FB_XMLCREF);
is($dst, $ax,   "FB_XMLCREF ascii");
is($src, '', "FB_XMLCREF residue ascii");

#$src = $ao;
#$dst = $utf8->decode($src, FB_XMLCREF);
#is($dst, $ax,   "FB_XMLCREF utf8");
#is($src, '', "FB_XMLCREF residue utf8");
