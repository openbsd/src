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
use Test::More tests => 22;
use Encode q(:all);

my $original = '';
my $nofallback  = '';
my ($fallenback, $quiet, $perlqq, $htmlcref, $xmlcref);
for my $i (0x20..0x7e){
    $original .= chr($i);
}
$fallenback = $quiet = 
$perlqq = $htmlcref = $xmlcref = $nofallback = $original;

my $residue = '';
for my $i (0x80..0xff){
    $original   .= chr($i);
    $residue    .= chr($i);
    $fallenback .= '?';
    $perlqq     .= sprintf("\\x{%04x}", $i);
    $htmlcref    .= sprintf("&#%d;", $i);
    $xmlcref    .= sprintf("&#x%x;", $i);
}
utf8::upgrade($original);
my $meth   = find_encoding('ascii');

my $src = $original;
my $dst = $meth->encode($src, FB_DEFAULT);
is($dst, $fallenback, "FB_DEFAULT");
is($src, $original,   "FB_DEFAULT residue");

$src = $original;
eval{ $dst = $meth->encode($src, FB_CROAK) };
like($@, qr/does not map to ascii/o, "FB_CROAK");
is($src, $original, "FB_CROAK residue");

$src = $original;
eval{ $dst = $meth->encode($src, FB_CROAK) };
like($@, qr/does not map to ascii/o, "FB_CROAK");
is($src, $original, "FB_CROAK residue");


$src = $nofallback;
eval{ $dst = $meth->encode($src, FB_CROAK) };
is($@, '', "FB_CROAK on success");
is($src, '', "FB_CROAK on success residue");

$src = $original;
$dst = $meth->encode($src, FB_QUIET);
is($dst, $quiet,   "FB_QUIET");
is($src, $residue, "FB_QUIET residue");

{
    my $message;
    local $SIG{__WARN__} = sub { $message = $_[0] };
    $src = $original;
    $dst = $meth->encode($src, FB_WARN);
    is($dst, $quiet,   "FB_WARN");
    is($src, $residue, "FB_WARN residue");
    like($message, qr/does not map to ascii/o, "FB_WARN message");

    $message = '';

    $src = $original;
    $dst = $meth->encode($src, WARN_ON_ERR);

    is($dst, $fallenback, "WARN_ON_ERR");
    is($src, '',  "WARN_ON_ERR residue");
    like($message, qr/does not map to ascii/o, "WARN_ON_ERR message");
}

$src = $original;
$dst = $meth->encode($src, FB_PERLQQ);
is($dst, $perlqq,   "FB_PERLQQ");
is($src, '', "FB_PERLQQ residue");

$src = $original;
$dst = $meth->encode($src, FB_HTMLCREF);
is($dst, $htmlcref,   "FB_HTMLCREF");
is($src, '', "FB_HTMLCREF residue");

$src = $original;
$dst = $meth->encode($src, FB_XMLCREF);
is($dst, $xmlcref,   "FB_XMLCREF");
is($src, '', "FB_XMLCREF residue");
