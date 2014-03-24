#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
no warnings 'misc', 'experimental::lexical_topic';

$_ = 'global';
is($_, 'global', '$_ initial value');
s/oba/abo/;
is($_, 'glabol', 's/// on global $_');

{
    my $_ = 'local';
    is($_, 'local', 'my $_ initial value');
    s/oca/aco/;
    is($_, 'lacol', 's/// on my $_');
    /(..)/;
    is($1, 'la', '// on my $_');
    cmp_ok(tr/c/d/, '==', 1, 'tr/// on my $_ counts correctly' );
    is($_, 'ladol', 'tr/// on my $_');
    {
	my $_ = 'nested';
	is($_, 'nested', 'my $_ nested');
	chop;
	is($_, 'neste', 'chop on my $_');
    }
    {
	our $_;
	is($_, 'glabol', 'gains access to our global $_');
    }
    is($_, 'ladol', 'my $_ restored');
}
is($_, 'glabol', 'global $_ restored');
s/abo/oba/;
is($_, 'global', 's/// on global $_ again');
{
    my $_ = 11;
    our $_ = 22;
    is($_, 22, 'our $_ is seen explicitly');
    chop;
    is($_, 2, '...default chop chops our $_');
    /(.)/;
    is($1, 2, '...default match sees our $_');
}

$_ = "global";
{
    my $_ = 'local';
    for my $_ ("foo") {
	is($_, "foo", 'for my $_');
	/(.)/;
	is($1, "f", '...m// in for my $_');
	is(our $_, 'global', '...our $_ inside for my $_');
    }
    is($_, 'local', '...my $_ restored outside for my $_');
    is(our $_, 'global', '...our $_ restored outside for my $_');
}
{
    my $_ = 'local';
    for ("implicit foo") { # implicit "my $_"
	is($_, "implicit foo", 'for implicit my $_');
	/(.)/;
	is($1, "i", '...m// in for implicit my $_');
	is(our $_, 'global', '...our $_ inside for implicit my $_');
    }
    is($_, 'local', '...my $_ restored outside for implicit my $_');
    is(our $_, 'global', '...our $_ restored outside for implicit my $_');
}
{
    my $_ = 'local';
    is($_, "postfix foo", 'postfix for' ) for 'postfix foo';
    is($_, 'local', '...my $_ restored outside postfix for');
    is(our $_, 'global', '...our $_ restored outside postfix for');
}
{
    for our $_ ("bar") {
	is($_, "bar", 'for our $_');
	/(.)/;
	is($1, "b", '...m// in for our $_');
    }
    is($_, 'global', '...our $_ restored outside for our $_');
}

{
    my $buf = '';
    sub tmap1 { /(.)/; $buf .= $1 } # uses our $_
    my $_ = 'x';
    sub tmap2 { /(.)/; $buf .= $1 } # uses my $_
    map {
	tmap1();
	tmap2();
	ok( /^[67]\z/, 'local lexical $_ is seen in map' );
	{ is(our $_, 'global', 'our $_ still visible'); }
	ok( $_ == 6 || $_ == 7, 'local lexical $_ is still seen in map' );
	{ my $_ ; is($_, undef, 'nested my $_ is undefined'); }
    } 6, 7;
    is($buf, 'gxgx', q/...map doesn't modify outer lexical $_/);
    is($_, 'x', '...my $_ restored outside map');
    is(our $_, 'global', '...our $_ restored outside map');
    map { my $_; is($_, undef, 'redeclaring $_ in map block undefs it'); } 1;
}
{ map { my $_; is($_, undef, 'declaring $_ in map block undefs it'); } 1; }
{
    sub tmap3 () { return $_ };
    my $_ = 'local';
    sub tmap4 () { return $_ };
    my $x = join '-', map $_.tmap3.tmap4, 1 .. 2;
    is($x, '1globallocal-2globallocal', 'map without {}');
}
{
    for my $_ (1) {
	my $x = map $_, qw(a b);
	is($x, 2, 'map in scalar context');
    }
}
{
    my $buf = '';
    sub tgrep1 { /(.)/; $buf .= $1 }
    my $_ = 'y';
    sub tgrep2 { /(.)/; $buf .= $1 }
    grep {
	tgrep1();
	tgrep2();
	ok( /^[89]\z/, 'local lexical $_ is seen in grep' );
	{ is(our $_, 'global', 'our $_ still visible'); }
	ok( $_ == 8 || $_ == 9, 'local lexical $_ is still seen in grep' );
    } 8, 9;
    is($buf, 'gygy', q/...grep doesn't modify outer lexical $_/);
    is($_, 'y', '...my $_ restored outside grep');
    is(our $_, 'global', '...our $_ restored outside grep');
}
{
    sub tgrep3 () { return $_ };
    my $_ = 'local';
    sub tgrep4 () { return $_ };
    my $x = join '-', grep $_=$_.tgrep3.tgrep4, 1 .. 2;
    is($x, '1globallocal-2globallocal', 'grep without {} with side-effect');
    is($_, 'local', '...but without extraneous side-effects');
}
{
    for my $_ (1) {
	my $x = grep $_, qw(a b);
	is($x, 2, 'grep in scalar context');
    }
}
{
    my $s = "toto";
    my $_ = "titi";
    my $r;
    {
	local $::TODO = 'Marked as todo since test was added in 59f00321bbc2d046';
	$r = $s =~ /to(?{ is($_, 'toto', 'my $_ in code-match' ) })to/;
    }
    ok($r, "\$s=$s should match!");
    is(our $_, 'global', '...our $_ restored outside code-match');
}

{
    my $_ = "abc";
    my $x = reverse;
    is($x, "cba", 'reverse without arguments picks up $_');
}

{
    package notmain;
    our $_ = 'notmain';
    ::is($::_, 'notmain', 'our $_ forced into main::');
    /(.*)/;
    ::is($1, 'notmain', '...m// defaults to our $_ in main::');
}

my $file = tempfile();
{
    open my $_, '>', $file or die "Can't open $file: $!";
    print $_ "hello\n";
    close $_;
    cmp_ok(-s $file, '>', 5, 'writing to filehandle $_ works');
}
{
    open my $_, $file or die "Can't open $file: $!";
    my $x = <$_>;
    is($x, "hello\n", 'reading from <$_> works');
    close $_;
}

{
    $fqdb::_ = 'fqdb';
    is($fqdb::_, 'fqdb', 'fully qualified $_ is not in main' );
    is(eval q/$fqdb::_/, 'fqdb', 'fully qualified, evaled $_ is not in main' );
    package fqdb;
    ::isnt($_, 'fqdb', 'unqualified $_ is in main' );
    ::isnt(eval q/$_/, 'fqdb', 'unqualified, evaled $_ is in main');
}

{
    $clank_est::qunckkk = 3;
    our $qunckkk;
    $qunckkk = 4;
    package clank_est;
    our $qunckkk;
    ::is($qunckkk, 3, 'regular variables are not forced to main');
}

{
    $whack::_ = 3;
    our $_;
    $_ = 4;
    package whack;
    our $_;
    ::is($_, 4, '$_ is "special", and always forced to main');
}

done_testing();
