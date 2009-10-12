BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	unless (find PerlIO::Layer 'perlio') {
	    print "1..0 # Skip: PerlIO not used\n";
	    exit 0;
	}
}

use Test::More tests => 37;

use_ok('PerlIO');

my $txt = "txt$$";
my $bin = "bin$$";
my $utf = "utf$$";

my $txtfh;
my $binfh;
my $utffh;

ok(open($txtfh, ">:crlf", $txt));

ok(open($binfh, ">:raw",  $bin));

ok(open($utffh, ">:utf8", $utf));

print $txtfh "foo\n";
print $txtfh "bar\n";

ok(close($txtfh));

print $binfh "foo\n";
print $binfh "bar\n";

ok(close($binfh));

print $utffh "foo\x{ff}\n";
print $utffh "bar\x{abcd}\n";

ok(close($utffh));

ok(open($txtfh, "<:crlf", $txt));

ok(open($binfh, "<:raw",  $bin));


ok(open($utffh, "<:utf8", $utf));

is(scalar <$txtfh>, "foo\n");
is(scalar <$txtfh>, "bar\n");

is(scalar <$binfh>, "foo\n");
is(scalar <$binfh>, "bar\n");

is(scalar <$utffh>,  "foo\x{ff}\n");
is(scalar <$utffh>, "bar\x{abcd}\n");

ok(eof($txtfh));;

ok(eof($binfh));

ok(eof($utffh));

ok(close($txtfh));

ok(close($binfh));

ok(close($utffh));

# magic temporary file via 3 arg open with undef
{
    ok( open(my $x,"+<",undef), 'magic temp file via 3 arg open with undef');
    ok( defined fileno($x),     '       fileno' );

    select $x;
    ok( (print "ok\n"),         '       print' );

    select STDOUT;
    ok( seek($x,0,0),           '       seek' );
    is( scalar <$x>, "ok\n",    '       readline' );
    ok( tell($x) >= 3,          '       tell' );

    # test magic temp file over STDOUT
    open OLDOUT, ">&STDOUT" or die "cannot dup STDOUT: $!";
    my $status = open(STDOUT,"+<",undef);
    open STDOUT,  ">&OLDOUT" or die "cannot dup OLDOUT: $!";
    # report after STDOUT is restored
    ok($status, '       re-open STDOUT');
    close OLDOUT;
}

# in-memory open
SKIP: {
    eval { require PerlIO::scalar };
    unless (find PerlIO::Layer 'scalar') {
	skip("PerlIO::scalar not found", 8);
    }
    my $var;
    ok( open(my $x,"+<",\$var), 'magic in-memory file via 3 arg open with \\$var');
    ok( defined fileno($x),     '       fileno' );

    select $x;
    ok( (print "ok\n"),         '       print' );

    select STDOUT;
    ok( seek($x,0,0),           '       seek' );
    is( scalar <$x>, "ok\n",    '       readline' );
    ok( tell($x) >= 3,          '       tell' );

  TODO: {
        local $TODO = "broken";

        # test in-memory open over STDOUT
        open OLDOUT, ">&STDOUT" or die "cannot dup STDOUT: $!";
        #close STDOUT;
        my $status = open(STDOUT,">",\$var);
        my $error = "$!" unless $status; # remember the error
	close STDOUT unless $status;
        open STDOUT,  ">&OLDOUT" or die "cannot dup OLDOUT: $!";
        print "# $error\n" unless $status;
        # report after STDOUT is restored
        ok($status, '       open STDOUT into in-memory var');

        # test in-memory open over STDERR
        open OLDERR, ">&STDERR" or die "cannot dup STDERR: $!";
        #close STDERR;
        ok( open(STDERR,">",\$var), '       open STDERR into in-memory var');
        open STDERR,  ">&OLDERR" or die "cannot dup OLDERR: $!";
    }
}


END {
    1 while unlink $txt;
    1 while unlink $bin;
    1 while unlink $utf;
}

