#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

$|  = 1;
use warnings;
use Config;

plan tests => 119;

my $Perl = which_perl();

my $afile = tempfile();
{
    unlink($afile) if -f $afile;

    $! = 0;  # the -f above will set $! if $afile doesn't exist.
    ok( open(my $f,"+>$afile"),  'open(my $f, "+>...")' );

    binmode $f;
    ok( -f $afile,              '       its a file');
    ok( (print $f "SomeData\n"),  '       we can print to it');
    is( tell($f), 9,            '       tell()' );
    ok( seek($f,0,0),           '       seek set' );

    $b = <$f>;
    is( $b, "SomeData\n",       '       readline' );
    ok( -f $f,                  '       still a file' );

    eval  { die "Message" };
    like( $@, qr/<\$f> line 1/, '       die message correct' );
    
    ok( close($f),              '       close()' );
    ok( unlink($afile),         '       unlink()' );
}

{
    ok( open(my $f,'>', $afile),        "open(my \$f, '>', $afile)" );
    ok( (print $f "a row\n"),           '       print');
    ok( close($f),                      '       close' );
    ok( -s $afile < 10,                 '       -s' );
}

{
    ok( open(my $f,'>>', $afile),       "open(my \$f, '>>', $afile)" );
    ok( (print $f "a row\n"),           '       print' );
    ok( close($f),                      '       close' );
    ok( -s $afile > 10,                 '       -s'    );
}

{
    ok( open(my $f, '<', $afile),       "open(my \$f, '<', $afile)" );
    my @rows = <$f>;
    is( scalar @rows, 2,                '       readline, list context' );
    is( $rows[0], "a row\n",            '       first line read' );
    is( $rows[1], "a row\n",            '       second line' );
    ok( close($f),                      '       close' );
}

{
    ok( -s $afile < 20,                 '-s' );

    ok( open(my $f, '+<', $afile),      'open +<' );
    my @rows = <$f>;
    is( scalar @rows, 2,                '       readline, list context' );
    ok( seek($f, 0, 1),                 '       seek cur' );
    ok( (print $f "yet another row\n"), '       print' );
    ok( close($f),                      '       close' );
    ok( -s $afile > 20,                 '       -s' );

    unlink($afile);
}
{
    ok( open(my $f, '-|', <<EOC),     'open -|' );
    $Perl -e "print qq(a row\\n); print qq(another row\\n)"
EOC

    my @rows = <$f>;
    is( scalar @rows, 2,                '       readline, list context' );
    ok( close($f),                      '       close' );
}
{
    ok( open(my $f, '|-', <<EOC),     'open |-' );
    $Perl -pe "s/^not //"
EOC

    my @rows = <$f>;
    my $test = curr_test;
    print $f "not ok $test - piped in\n";
    next_test;

    $test = curr_test;
    print $f "not ok $test - piped in\n";
    next_test;
    ok( close($f),                      '       close' );
    sleep 1;
    pass('flushing');
}


ok( !eval { open my $f, '<&', $afile; 1; },    '<& on a non-filehandle' );
like( $@, qr/Bad filehandle:\s+$afile/,          '       right error' );

ok( !eval { *some_glob = 1; open my $f, '<&', *some_glob; 1; },    '<& on a non-filehandle glob' );
like( $@, qr/Bad filehandle:\s+some_glob/,          '       right error' );

{
    use utf8;
    use open qw( :utf8 :std );
    ok( !eval { use utf8; *ǡﬁlḛ = 1; open my $f, '<&', *ǡﬁlḛ; 1; },    '<& on a non-filehandle glob' );
    like( $@, qr/Bad filehandle:\s+ǡﬁlḛ/u,          '       right error' );
}

# local $file tests
{
    unlink($afile) if -f $afile;

    ok( open(local $f,"+>$afile"),       'open local $f, "+>", ...' );
    binmode $f;

    ok( -f $afile,                      '       -f' );
    ok( (print $f "SomeData\n"),        '       print' );
    is( tell($f), 9,                    '       tell' );
    ok( seek($f,0,0),                   '       seek set' );

    $b = <$f>;
    is( $b, "SomeData\n",               '       readline' );
    ok( -f $f,                          '       still a file' );

    eval  { die "Message" };
    like( $@, qr/<\$f> line 1/,         '       proper die message' );
    ok( close($f),                      '       close' );

    unlink($afile);
}

{
    ok( open(local $f,'>', $afile),     'open local $f, ">", ...' );
    ok( (print $f "a row\n"),           '       print');
    ok( close($f),                      '       close');
    ok( -s $afile < 10,                 '       -s' );
}

{
    ok( open(local $f,'>>', $afile),    'open local $f, ">>", ...' );
    ok( (print $f "a row\n"),           '       print');
    ok( close($f),                      '       close');
    ok( -s $afile > 10,                 '       -s' );
}

{
    ok( open(local $f, '<', $afile),    'open local $f, "<", ...' );
    my @rows = <$f>;
    is( scalar @rows, 2,                '       readline list context' );
    ok( close($f),                      '       close' );
}

ok( -s $afile < 20,                     '       -s' );

{
    ok( open(local $f, '+<', $afile),  'open local $f, "+<", ...' );
    my @rows = <$f>;
    is( scalar @rows, 2,                '       readline list context' );
    ok( seek($f, 0, 1),                 '       seek cur' );
    ok( (print $f "yet another row\n"), '       print' );
    ok( close($f),                      '       close' );
    ok( -s $afile > 20,                 '       -s' );

    unlink($afile);
}

{
    ok( open(local $f, '-|', <<EOC),  'open local $f, "-|", ...' );
    $Perl -e "print qq(a row\\n); print qq(another row\\n)"
EOC
    my @rows = <$f>;

    is( scalar @rows, 2,                '       readline list context' );
    ok( close($f),                      '       close' );
}

{
    ok( open(local $f, '|-', <<EOC),  'open local $f, "|-", ...' );
    $Perl -pe "s/^not //"
EOC

    my @rows = <$f>;
    my $test = curr_test;
    print $f "not ok $test - piping\n";
    next_test;

    $test = curr_test;
    print $f "not ok $test - piping\n";
    next_test;
    ok( close($f),                      '       close' );
    sleep 1;
    pass("Flush");
}


ok( !eval { open local $f, '<&', $afile; 1 },  'local <& on non-filehandle');
like( $@, qr/Bad filehandle:\s+$afile/,          '       right error' );

{
    local *F;
    for (1..2) {
	ok( open(F, qq{$Perl -le "print 'ok'"|}), 'open to pipe' );
	is(scalar <F>, "ok\n",  '       readline');
	ok( close F,            '       close' );
    }

    for (1..2) {
	ok( open(F, "-|", qq{$Perl -le "print 'ok'"}), 'open -|');
	is( scalar <F>, "ok\n", '       readline');
	ok( close F,            '       close' );
    }
}


# other dupping techniques
{
    ok( open(my $stdout, ">&", \*STDOUT),       'dup \*STDOUT into lexical fh');
    ok( open(STDOUT,     ">&", $stdout),        'restore dupped STDOUT from lexical fh');

    {
	use strict; # the below should not warn
	ok( open(my $stdout, ">&", STDOUT),         'dup STDOUT into lexical fh');
    }

    # used to try to open a file [perl #17830]
    ok( open(my $stdin,  "<&", fileno STDIN),   'dup fileno(STDIN) into lexical fh') or _diag $!;
}

SKIP: {
    skip "This perl uses perlio", 1 if $Config{useperlio};
    skip_if_miniperl("miniperl can't rely on loading %Errno", 1);
    # Force the reference to %! to be run time by writing ! as {"!"}
    skip "This system doesn't understand EINVAL", 1
	unless exists ${"!"}{EINVAL};

    no warnings 'io';
    ok(!open(F,'>',\my $s) && ${"!"}{EINVAL}, 'open(reference) raises EINVAL');
}

{
    ok( !eval { open F, "BAR", "QUUX" },       'Unknown open() mode' );
    like( $@, qr/\QUnknown open() mode 'BAR'/, '       right error' );
}

{
    local $SIG{__WARN__} = sub { $@ = shift };

    sub gimme {
        my $tmphandle = shift;
	my $line = scalar <$tmphandle>;
	warn "gimme";
	return $line;
    }

    open($fh0[0], "TEST");
    gimme($fh0[0]);
    like($@, qr/<\$fh0\[...\]> line 1\./, "autoviv fh package aelem");

    open($fh1{k}, "TEST");
    gimme($fh1{k});
    like($@, qr/<\$fh1{...}> line 1\./, "autoviv fh package helem");

    my @fh2;
    open($fh2[0], "TEST");
    gimme($fh2[0]);
    like($@, qr/<\$fh2\[...\]> line 1\./, "autoviv fh lexical aelem");

    my %fh3;
    open($fh3{k}, "TEST");
    gimme($fh3{k});
    like($@, qr/<\$fh3{...}> line 1\./, "autoviv fh lexical helem");
}
    
SKIP: {
    skip("These tests use perlio", 5) unless $Config{useperlio};
    my $w;
    use warnings 'layer';
    local $SIG{__WARN__} = sub { $w = shift };

    eval { open(F, ">>>", $afile) };
    like($w, qr/Invalid separator character '>' in PerlIO layer spec/,
	 "bad open (>>>) warning");
    like($@, qr/Unknown open\(\) mode '>>>'/,
	 "bad open (>>>) failure");

    eval { open(F, ">:u", $afile ) };
    like($w, qr/Unknown PerlIO layer "u"/,
	 'bad layer ">:u" warning');
    eval { open(F, "<:u", $afile ) };
    like($w, qr/Unknown PerlIO layer "u"/,
	 'bad layer "<:u" warning');
    eval { open(F, ":c", $afile ) };
    like($@, qr/Unknown open\(\) mode ':c'/,
	 'bad layer ":c" failure');
}

# [perl #28986] "open m" crashes Perl

fresh_perl_like('open m', qr/^Search pattern not terminated at/,
	{ stderr => 1 }, 'open m test');

fresh_perl_is(
    'sub f { open(my $fh, "xxx"); $fh = "f"; } f; f;print "ok"',
    'ok', { stderr => 1 },
    '#29102: Crash on assignment to lexical filehandle');

# [perl #31767] Using $1 as a filehandle via open $1, "file" doesn't raise
# an exception

eval { open $99, "foo" };
like($@, qr/Modification of a read-only value attempted/, "readonly fh");
# But we do not want that exception applying to close(), since it does not
# modify the fh.
eval {
   no warnings "uninitialized";
   # make sure $+ is undefined
   "a" =~ /(b)?/;
   close $+
};
is($@, '', 'no "Modification of a read-only value" when closing');

# [perl#73626] mg_get wasn't run on the pipe arg

{
    package p73626;
    sub TIESCALAR { bless {} }
    sub FETCH { "$Perl -e 1"}

    tie my $p, 'p73626';

    package main;

    ok( open(my $f, '-|', $p),     'open -| magic');
}

# [perl #77492] Crash when stringifying a glob, a reference to which has
#               been opened and written to.
fresh_perl_is(
    '
      open my $fh, ">", \*STDOUT;
      print $fh "hello";
     "".*STDOUT;
      print "ok";
      close $fh;
      unlink \*STDOUT;
    ',
    'ok', { stderr => 1 },
    '[perl #77492]: open $fh, ">", \*glob causes SEGV');

# [perl #77684] Opening a reference to a glob copy.
SKIP: {
    skip_if_miniperl("no dynamic loading on miniperl, so can't load PerlIO::scalar", 1);
    my $var = *STDOUT;
    open my $fh, ">", \$var;
    print $fh "hello";
    is $var, "hello", '[perl #77684]: open $fh, ">", \$glob_copy'
        # when this fails, it leaves an extra file:
        or unlink \*STDOUT;
}

# check that we can call methods on filehandles auto-magically
# and have IO::File loaded for us
SKIP: {
    skip_if_miniperl("no dynamic loading on miniperl, so can't load IO::File", 3);
    is( $INC{'IO/File.pm'}, undef, "IO::File not loaded" );
    my $var = "";
    open my $fh, ">", \$var;
    ok( eval { $fh->autoflush(1); 1 }, '$fh->autoflush(1) lives' );
    ok( $INC{'IO/File.pm'}, "IO::File now loaded" );
}
