BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	unless (find PerlIO::Layer 'perlio') {
	    print "1..0 # Skip: PerlIO not used\n";
	    exit 0;
	}
	require './test.pl';
}

plan tests => 42;

use_ok('PerlIO');

my $txt = "txt$$";
my $bin = "bin$$";
my $utf = "utf$$";
my $nonexistent = "nex$$";

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

    SKIP: {
      skip("TMPDIR not honored on this platform", 4)
        if !$Config{d_mkstemp}
        || $^O eq 'VMS' || $^O eq 'MSwin32' || $^O eq 'os2';
      local $ENV{TMPDIR} = $nonexistent;

      # hardcoded default temp path
      my $perlio_tmp_file_glob = '/tmp/PerlIO_??????';

      ok( open(my $x,"+<",undef), 'TMPDIR honored by magic temp file via 3 arg open with undef - works if TMPDIR points to a non-existent dir');

      my $filename = find_filename($x, $perlio_tmp_file_glob);
      is($filename, undef, "No tmp files leaked");
      unlink $filename if defined $filename;

      mkdir $ENV{TMPDIR};
      ok(open(my $x,"+<",undef), 'TMPDIR honored by magic temp file via 3 arg open with undef - works if TMPDIR points to an existent dir');

      $filename = find_filename($x, $perlio_tmp_file_glob);
      is($filename, undef, "No tmp files leaked");
      unlink $filename if defined $filename;
    }
}

sub find_filename {
    my ($fh, @globs) = @_;
    my ($dev, $inode) = stat $fh;
    die "Can't stat $fh: $!" unless defined $dev;

    foreach (@globs) {
	foreach my $file (glob $_) {
	    my ($this_dev, $this_inode) = stat $file;
	    next unless defined $this_dev;
	    return $file if $this_dev == $dev && $this_inode == $inode;
	}
    }
    return;
}

# in-memory open
SKIP: {
    eval { require PerlIO::scalar };
    unless (find PerlIO::Layer 'scalar') {
	skip("PerlIO::scalar not found", 9);
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


{ local $TODO = 'fails well back into 5.8.x';

	
sub read_fh_and_return_final_rv {
	my ($fh) = @_;
	my $buf = '';
	my $rv;
	for (1..3) {
		$rv = read($fh, $buf, 1, length($buf));
		next if $rv;
	}
	return $rv
}

open(my $no_perlio, '<', \'ab') or die; 
open(my $perlio, '<:crlf', \'ab') or die; 

is(read_fh_and_return_final_rv($perlio), read_fh_and_return_final_rv($no_perlio), "RT#69332 - perlio should return the same value as nonperlio after EOF");

close ($perlio);
close ($no_perlio);
}

}


END {
    1 while unlink $txt;
    1 while unlink $bin;
    1 while unlink $utf;
    rmdir $nonexistent;
}

