#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
    require 'test.pl';
}

plan( tests => 17 );

@oops = @ops = <op/*>;

if ($^O eq 'MSWin32') {
  map { $files{lc($_)}++ } <op/*>;
  map { delete $files{"op/$_"} } split /[\s\n]/, `dir /b /l op & dir /b /l /ah op 2>nul`,
}
elsif ($^O eq 'VMS') {
  map { $files{lc($_)}++ } <[.op]*>;
  map { s/;.*$//; delete $files{lc($_)}; } split /[\n]/, `directory/noheading/notrailing/versions=1 [.op]`,
}
else {
  map { $files{$_}++ } <op/*>;
  map { delete $files{$_} } split /\n/, `ls op/* | cat`;
}
ok( !(keys(%files)),'leftover op/* files' ) or diag(join(' ',sort keys %files));

cmp_ok($/,'eq',"\n",'sane input record separator');

$not = '';
while (<jskdfjskdfj* op/* jskdjfjkosvk*>) {
    $not = "not " unless $_ eq shift @ops;
    $not = "not at all " if $/ eq "\0";
}
ok(!$not,"glob amid garbage [$not]");

cmp_ok($/,'eq',"\n",'input record separator still sane');

$_ = "op/*";
@glops = glob $_;
cmp_ok("@glops",'eq',"@oops",'glob operator 1');

@glops = glob;
cmp_ok("@glops",'eq',"@oops",'glob operator 2');

# glob should still work even after the File::Glob stash has gone away
# (this used to dump core)
my $i = 0;
for (1..2) {
    eval "<.>";
    ok(!length($@),"eval'ed a glob $_");
    local %File::Glob::;
    ++$i;
}
cmp_ok($i,'==',2,'remove File::Glob stash');

# a more sinister version of the same test (crashes from 5.8 to 5.13.1)
{
    local %File::Glob::;
    local %CORE::GLOBAL::;
    eval "<.>";
    ok(!length($@),"remove File::Glob stash *and* CORE::GLOBAL::glob");
}
# Also try undeffing the typeglob itself, instead of hiding it
{
    local *CORE::GLOBAL::glob;
    ok eval  { glob("0"); 1 },
	'undefined *CORE::GLOBAL::glob{CODE} at run time';
}
# And hide the typeglob without hiding File::Glob (crashes from 5.8
# to 5.15.4)
{
    local %CORE::GLOBAL::;
    ok eval q{ glob("0"); 1 },
	'undefined *CORE::GLOBAL::glob{CODE} at compile time';
}

# ... while ($var = glob(...)) should test definedness not truth

SKIP: {
    skip('no File::Glob to emulate Unix-ism', 1)
	unless $INC{'File/Glob.pm'};
    my $ok = 0;
    $ok = 1 while my $var = glob("0");
    ok($ok,'define versus truth');
}

# The formerly-broken test for the situation above would accidentally
# test definedness for an assignment with a LOGOP on the right:
{
    my $f = 0;
    my $ok = 1;
    $ok = 0, undef $f while $x = $f||$f;
    ok($ok,'test definedness with LOGOP');
}

cmp_ok(scalar(@oops),'>',0,'glob globbed something');

SKIP: {
    skip "~ globbing returns nothing on VMS", 1 if $^O eq 'VMS';
    # This test exists mainly for miniperl, to test that external calls to
    # csh, which clear %ENV first, leave $ENV{HOME}.
    # On Windows, external glob uses File::DosGlob which returns "~", so
    # this should pass anyway.
    ok <~>, '~ works';
}

{
    my $called;
    local *CORE::GLOBAL::glob = sub { ++$called };
    eval 'CORE::glob("0")';
    ok !$called, 'CORE::glob bypasses overrides';
}
