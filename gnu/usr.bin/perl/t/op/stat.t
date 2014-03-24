#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';	# for which_perl() etc
}

use Config;

my ($Null, $Curdir);
if(eval {require File::Spec; 1}) {
    $Null = File::Spec->devnull;
    $Curdir = File::Spec->curdir;
} else {
    die $@ unless is_miniperl();
    $Curdir = '.';
    diag("miniperl failed to load File::Spec, error is:\n$@");
    diag("\ncontinuing, assuming '.' for current directory. Some tests will be skipped.");
}


plan tests => 113;

my $Perl = which_perl();

$ENV{LC_ALL}   = 'C';		# Forge English error messages.
$ENV{LANGUAGE} = 'C';		# Ditto in GNU.

$Is_Amiga   = $^O eq 'amigaos';
$Is_Cygwin  = $^O eq 'cygwin';
$Is_Darwin  = $^O eq 'darwin';
$Is_Dos     = $^O eq 'dos';
$Is_MSWin32 = $^O eq 'MSWin32';
$Is_NetWare = $^O eq 'NetWare';
$Is_OS2     = $^O eq 'os2';
$Is_Solaris = $^O eq 'solaris';
$Is_VMS     = $^O eq 'VMS';
$Is_DGUX    = $^O eq 'dgux';
$Is_MPRAS   = $^O =~ /svr4/ && -f '/etc/.relid';

$Is_Dosish  = $Is_Dos || $Is_OS2 || $Is_MSWin32 || $Is_NetWare;

$Is_UFS     = $Is_Darwin && (() = `df -t ufs . 2>/dev/null`) == 2;

if ($Is_Cygwin) {
  require Win32;
  Win32->import;
}

my($DEV, $INO, $MODE, $NLINK, $UID, $GID, $RDEV, $SIZE,
   $ATIME, $MTIME, $CTIME, $BLKSIZE, $BLOCKS) = (0..12);

my $tmpfile = tempfile();
my $tmpfile_link = tempfile();

chmod 0666, $tmpfile;
unlink_all $tmpfile;
open(FOO, ">$tmpfile") || DIE("Can't open temp test file: $!");
close FOO;

open(FOO, ">$tmpfile") || DIE("Can't open temp test file: $!");

my($nlink, $mtime, $ctime) = (stat(FOO))[$NLINK, $MTIME, $CTIME];

# The clock on a network filesystem might be different from the
# system clock.
my $Filesystem_Time_Offset = abs($mtime - time); 

#nlink should if link support configured in Perl.
SKIP: {
    skip "No link count - Hard link support not built in.", 1
	unless $Config{d_link};

    is($nlink, 1, 'nlink on regular file');
}

SKIP: {
  skip "mtime and ctime not reliable", 2
    if $Is_MSWin32 or $Is_NetWare or $Is_Cygwin or $Is_Dos or $Is_Darwin;

  ok( $mtime,           'mtime' );
  is( $mtime, $ctime,   'mtime == ctime' );
}


# Cygwin seems to have a 3 second granularity on its timestamps.
my $funky_FAT_timestamps = $Is_Cygwin;
sleep 3 if $funky_FAT_timestamps;

print FOO "Now is the time for all good men to come to.\n";
close(FOO);

sleep 2;


SKIP: {
    unlink $tmpfile_link;
    my $lnk_result = eval { link $tmpfile, $tmpfile_link };
    skip "link() unimplemented", 6 if $@ =~ /unimplemented/;

    is( $@, '',         'link() implemented' );
    ok( $lnk_result,    'linked tmp testfile' );
    ok( chmod(0644, $tmpfile),             'chmoded tmp testfile' );

    my($nlink, $mtime, $ctime) = (stat($tmpfile))[$NLINK, $MTIME, $CTIME];

    SKIP: {
        skip "No link count", 1 if $Config{dont_use_nlink};
        skip "Cygwin9X fakes hard links by copying", 1
          if $Config{myuname} =~ /^cygwin_(?:9\d|me)\b/i;

        is($nlink, 2,     'Link count on hard linked file' );
    }

    SKIP: {
	skip_if_miniperl("File::Spec not built for minitest", 2);
        my $cwd = File::Spec->rel2abs($Curdir);
        skip "Solaris tmpfs has different mtime/ctime link semantics", 2
                                     if $Is_Solaris and $cwd =~ m#^/tmp# and
                                        $mtime && $mtime == $ctime;
        skip "AFS has different mtime/ctime link semantics", 2
                                     if $cwd =~ m#$Config{'afsroot'}/#;
        skip "AmigaOS has different mtime/ctime link semantics", 2
                                     if $Is_Amiga;
        # Win32 could pass $mtime test but as FAT and NTFS have
        # no ctime concept $ctime is ALWAYS == $mtime
        # expect netware to be the same ...
        skip "No ctime concept on this OS", 2
                                     if $Is_MSWin32 || 
                                        ($Is_Darwin && $Is_UFS);

        if( !ok($mtime, 'hard link mtime') ||
            !isnt($mtime, $ctime, 'hard link ctime != mtime') ) {
            print STDERR <<DIAG;
# Check if you are on a tmpfs of some sort.  Building in /tmp sometimes
# has this problem.  Building on the ClearCase VOBS filesystem may also
# cause this failure.
#
# Darwin's UFS doesn't have a ctime concept, and thus is expected to fail
# this test.
DIAG
        }
    }

}

# truncate and touch $tmpfile.
open(F, ">$tmpfile") || DIE("Can't open temp test file: $!");
ok(-z \*F,     '-z on empty filehandle');
ok(! -s \*F,   '   and -s');
close F;

ok(-z $tmpfile,     '-z on empty file');
ok(! -s $tmpfile,   '   and -s');

open(F, ">$tmpfile") || DIE("Can't open temp test file: $!");
print F "hi\n";
close F;

open(F, "<$tmpfile") || DIE("Can't open temp test file: $!");
ok(!-z *F,     '-z on empty filehandle');
ok( -s *F,   '   and -s');
close F;

ok(! -z $tmpfile,   '-z on non-empty file');
ok(-s $tmpfile,     '   and -s');


# Strip all access rights from the file.
ok( chmod(0000, $tmpfile),     'chmod 0000' );

SKIP: {
    skip "-r, -w and -x have different meanings on VMS", 3 if $Is_VMS;

    SKIP: {
        # Going to try to switch away from root.  Might not work.
        my $olduid = $>;
        eval { $> = 1; };
        skip "Can't test -r or -w meaningfully if you're superuser", 2
          if ($Is_Cygwin ? Win32::IsAdminUser : $> == 0);

        SKIP: {
            skip "Can't test -r meaningfully?", 1 if $Is_Dos;
            ok(!-r $tmpfile,    "   -r");
        }

        ok(!-w $tmpfile,    "   -w");

        # switch uid back (may not be implemented)
        eval { $> = $olduid; };
    }

    ok(! -x $tmpfile,   '   -x');
}



ok(chmod(0700,$tmpfile),    'chmod 0700');
ok(-r $tmpfile,     '   -r');
ok(-w $tmpfile,     '   -w');

SKIP: {
    skip "-x simply determines if a file ends in an executable suffix", 1
      if $Is_Dosish;

    ok(-x $tmpfile,     '   -x');
}

ok(  -f $tmpfile,   '   -f');
ok(! -d $tmpfile,   '   !-d');

# Is this portable?
ok(  -d '.',          '-d cwd' );
ok(! -f '.',          '!-f cwd' );


SKIP: {
    unlink($tmpfile_link);
    my $symlink_rslt = eval { symlink $tmpfile, $tmpfile_link };
    skip "symlink not implemented", 3 if $@ =~ /unimplemented/;

    is( $@, '',     'symlink() implemented' );
    ok( $symlink_rslt,      'symlink() ok' );
    ok(-l $tmpfile_link,    '-l');
}

ok(-o $tmpfile,     '-o');

ok(-e $tmpfile,     '-e');

unlink($tmpfile_link);
ok(! -e $tmpfile_link,  '   -e on unlinked file');

SKIP: {
    skip "No character, socket or block special files", 6
      if $Is_MSWin32 || $Is_NetWare || $Is_Dos;
    skip "/dev isn't available to test against", 6
      unless -d '/dev' && -r '/dev' && -x '/dev';
    skip "Skipping: unexpected ls output in MP-RAS", 6
      if $Is_MPRAS;

    # VMS problem:  If GNV or other UNIX like tool is installed, then
    # sometimes Perl will find /bin/ls, and will try to run it.
    # But since Perl on VMS does not know to run it under Bash, it will
    # try to run the DCL verb LS.  And if the VMS product Language
    # Sensitive Editor is installed, or some other LS verb, that will
    # be run instead.  So do not do this until we can teach Perl
    # when to use BASH on VMS.
    skip "ls command not available to Perl in OpenVMS right now.", 6
      if $Is_VMS;

    delete $ENV{CLICOLOR_FORCE};
    my $LS  = $Config{d_readlink} ? "ls -lL" : "ls -l";
    my $CMD = "$LS /dev 2>/dev/null";
    my $DEV = qx($CMD);

    skip "$CMD failed", 6 if $DEV eq '';

    my @DEV = do { my $dev; opendir($dev, "/dev") ? readdir($dev) : () };

    skip "opendir failed: $!", 6 if @DEV == 0;

    # /dev/stdout might be either character special or a named pipe,
    # or a symlink, or a socket, depending on which OS and how are
    # you running the test, so let's censor that one away.
    # Similar remarks hold for stderr.
    $DEV =~ s{^[cpls].+?\sstdout$}{}m;
    @DEV =  grep { $_ ne 'stdout' } @DEV;
    $DEV =~ s{^[cpls].+?\sstderr$}{}m;
    @DEV =  grep { $_ ne 'stderr' } @DEV;

    # /dev/printer is also naughty: in IRIX it shows up as
    # Srwx-----, not srwx------.
    $DEV =~ s{^.+?\sprinter$}{}m;
    @DEV =  grep { $_ ne 'printer' } @DEV;

    # If running as root, we will see .files in the ls result,
    # and readdir() will see them always.  Potential for conflict,
    # so let's weed them out.
    $DEV =~ s{^.+?\s\..+?$}{}m;
    @DEV =  grep { ! m{^\..+$} } @DEV;

    # Irix ls -l marks sockets with 'S' while 's' is a 'XENIX semaphore'.
    if ($^O eq 'irix') {
        $DEV =~ s{^S(.+?)}{s$1}mg;
    }

    my $try = sub {
	my @c1 = eval qq[\$DEV =~ /^$_[0].*/mg];
	my @c2 = eval qq[grep { $_[1] "/dev/\$_" } \@DEV];
	my $c1 = scalar @c1;
	my $c2 = scalar @c2;
	is($c1, $c2, "ls and $_[1] agreeing on /dev ($c1 $c2)");
    };

SKIP: {
    skip("DG/UX ls -L broken", 3) if $Is_DGUX;

    $try->('b', '-b');
    $try->('c', '-c');
    $try->('s', '-S');

}

ok(! -b $Curdir,    '!-b cwd');
ok(! -c $Curdir,    '!-c cwd');
ok(! -S $Curdir,    '!-S cwd');

}

SKIP: {
    my($cnt, $uid);
    $cnt = $uid = 0;

    # Find a set of directories that's very likely to have setuid files
    # but not likely to be *all* setuid files.
    my @bin = grep {-d && -r && -x} qw(/sbin /usr/sbin /bin /usr/bin);
    skip "Can't find a setuid file to test with", 3 unless @bin;

    for my $bin (@bin) {
        opendir BIN, $bin or die "Can't opendir $bin: $!";
        while (defined($_ = readdir BIN)) {
            $_ = "$bin/$_";
            $cnt++;
            $uid++ if -u;
            last if $uid && $uid < $cnt;
        }
    }
    closedir BIN;

    skip "No setuid programs", 3 if $uid == 0;

    isnt($cnt, 0,    'found some programs');
    isnt($uid, 0,    '  found some setuid programs');
    ok($uid < $cnt,  "    they're not all setuid");
}


# To assist in automated testing when a controlling terminal (/dev/tty)
# may not be available (at, cron  rsh etc), the PERL_SKIP_TTY_TEST env var
# can be set to skip the tests that need a tty.
SKIP: {
    skip "These tests require a TTY", 4 if $ENV{PERL_SKIP_TTY_TEST};

    my $TTY = "/dev/tty";

    SKIP: {
        skip "Test uses unixisms", 2 if $Is_MSWin32 || $Is_NetWare;
        skip "No TTY to test -t with", 2 unless -e $TTY;

        open(TTY, $TTY) ||
          warn "Can't open $TTY--run t/TEST outside of make.\n";
        ok(-t TTY,  '-t');
        ok(-c TTY,  'tty is -c');
        close(TTY);
    }
    ok(! -t TTY,    '!-t on closed TTY filehandle');

    {
        local $TODO = 'STDIN not a tty when output is to pipe' if $Is_VMS;
        ok(-t,          '-t on STDIN');
    }
}

SKIP: {
    skip "No null device to test with", 1 unless -e $Null;
    skip "We know Win32 thinks '$Null' is a TTY", 1 if $Is_MSWin32;

    open(NULL, $Null) or DIE("Can't open $Null: $!");
    ok(! -t NULL,   'null device is not a TTY');
    close(NULL);
}


# These aren't strictly "stat" calls, but so what?
my $statfile = './op/stat.t';
ok(  -T $statfile,    '-T');
ok(! -B $statfile,    '!-B');

SKIP: {
     skip("DG/UX", 1) if $Is_DGUX;
ok(-B $Perl,      '-B');
}

ok(! -T $Perl,    '!-T');

open(FOO,$statfile);
SKIP: {
    eval { -T FOO; };
    skip "-T/B on filehandle not implemented", 15 if $@ =~ /not implemented/;

    is( $@, '',     '-T on filehandle causes no errors' );

    ok(-T FOO,      '   -T');
    ok(! -B FOO,    '   !-B');

    $_ = <FOO>;
    like($_, qr/perl/, 'after readline');
    ok(-T FOO,      '   still -T');
    ok(! -B FOO,    '   still -B');
    close(FOO);

    open(FOO,$statfile);
    $_ = <FOO>;
    like($_, qr/perl/,      'reopened and after readline');
    ok(-T FOO,      '   still -T');
    ok(! -B FOO,    '   still !-B');

    ok(seek(FOO,0,0),   'after seek');
    ok(-T FOO,          '   still -T');
    ok(! -B FOO,        '   still !-B');

    # It's documented this way in perlfunc *shrug*
    () = <FOO>;
    ok(eof FOO,         'at EOF');
    ok(-T FOO,          '   still -T');
    ok(-B FOO,          '   now -B');
}
close(FOO);


SKIP: {
    skip "No null device to test with", 2 unless -e $Null;

    ok(-T $Null,  'null device is -T');
    ok(-B $Null,  '    and -B');
}


# and now, a few parsing tests:
$_ = $tmpfile;
ok(-f,      'bare -f   uses $_');
ok(-f(),    '     -f() "');

unlink $tmpfile or print "# unlink failed: $!\n";

# bug id 20011101.069
my @r = \stat($Curdir);
is(scalar @r, 13,   'stat returns full 13 elements');

stat $0;
eval { lstat _ };
like( $@, qr/^The stat preceding lstat\(\) wasn't an lstat/,
    'lstat _ croaks after stat' );
eval { lstat *_ };
like( $@, qr/^The stat preceding lstat\(\) wasn't an lstat/,
    'lstat *_ croaks after stat' );
eval { lstat \*_ };
like( $@, qr/^The stat preceding lstat\(\) wasn't an lstat/,
    'lstat \*_ croaks after stat' );
eval { -l _ };
like( $@, qr/^The stat preceding -l _ wasn't an lstat/,
    '-l _ croaks after stat' );

lstat $0;
eval { lstat _ };
is( "$@", "", "lstat _ ok after lstat" );
eval { -l _ };
is( "$@", "", "-l _ ok after lstat" );

eval { lstat "test.pl" };
{
    open my $fh, "test.pl";
    stat *$fh{IO};
    eval { lstat _ }
}
like $@, qr/^The stat preceding lstat\(\) wasn't an lstat at /,
'stat $ioref resets stat type';

{
    my @statbuf = stat STDOUT;
    stat "test.pl";
    my @lstatbuf = lstat *STDOUT{IO};
    is "@lstatbuf", "@statbuf", 'lstat $ioref reverts to regular fstat';
}
  
SKIP: {
    skip "No lstat", 2 unless $Config{d_lstat};

    # bug id 20020124.004
    # If we have d_lstat, we should have symlink()
    my $linkname = 'stat-' . rand =~ y/.//dr;
    my $target = $Perl;
    $target =~ s/;\d+\z// if $Is_VMS; # symlinks don't like version numbers
    symlink $target, $linkname or die "# Can't symlink $0: $!";
    lstat $linkname;
    -T _;
    eval { lstat _ };
    like( $@, qr/^The stat preceding lstat\(\) wasn't an lstat/,
	'lstat croaks after -T _' );
    eval { -l _ };
    like( $@, qr/^The stat preceding -l _ wasn't an lstat/,
	'-l _ croaks after -T _' );
    unlink $linkname or print "# unlink $linkname failed: $!\n";
}

SKIP: {
    skip "Too much clock skew between system and filesystem", 5
	if ($Filesystem_Time_Offset > 5);
    print "# Zzz...\n";
    sleep($Filesystem_Time_Offset+1);
    my $f = 'tstamp.tmp';
    unlink $f;
    ok (open(S, "> $f"), 'can create tmp file');
    close S or die;
    my @a = stat $f;
    print "# time=$^T, stat=(@a)\n";
    my @b = (-M _, -A _, -C _);
    print "# -MAC=(@b)\n";
    ok( (-M _) < 0, 'negative -M works');
    ok( (-A _) < 0, 'negative -A works');
    ok( (-C _) < 0, 'negative -C works');
    ok(unlink($f), 'unlink tmp file');
}

# [perl #4253]
{
    ok(open(F, ">", $tmpfile), 'can create temp file');
    close F;
    chmod 0077, $tmpfile;
    my @a = stat($tmpfile);
    my $s1 = -s _;
    -T _;
    my $s2 = -s _;
    is($s1, $s2, q(-T _ doesn't break the statbuffer));
    SKIP: {
	skip "No lstat", 1 unless $Config{d_lstat};
	skip "uid=0", 1 unless $<&&$>;
	skip "Readable by group/other means readable by me", 1 if $^O eq 'VMS';
	lstat($tmpfile);
	-T _;
	ok(eval { lstat _ },
	   q(-T _ doesn't break lstat for unreadable file));
    }
    unlink $tmpfile;
}

SKIP: {
    skip "No dirfd()", 9 unless $Config{d_dirfd} || $Config{d_dir_dd_fd};
    ok(opendir(DIR, "."), 'Can open "." dir') || diag "Can't open '.':  $!";
    ok(stat(DIR), "stat() on dirhandle works"); 
    ok(-d -r _ , "chained -x's on dirhandle"); 
    ok(-d DIR, "-d on a dirhandle works");

    # And now for the ambiguous bareword case
    {
	no warnings 'deprecated';
	ok(open(DIR, "TEST"), 'Can open "TEST" dir')
	    || diag "Can't open 'TEST':  $!";
    }
    my $size = (stat(DIR))[7];
    ok(defined $size, "stat() on bareword works");
    is($size, -s "TEST", "size returned by stat of bareword is for the file");
    ok(-f _, "ambiguous bareword uses file handle, not dir handle");
    ok(-f DIR);
    closedir DIR or die $!;
    close DIR or die $!;
}

{
    # RT #8244: *FILE{IO} does not behave like *FILE for stat() and -X() operators
    ok(open(F, ">", $tmpfile), 'can create temp file');
    my @thwap = stat *F{IO};
    ok(@thwap, "stat(*F{IO}) works");    
    ok( -f *F{IO} , "single file tests work with *F{IO}");
    close F;
    unlink $tmpfile;

    #PVIO's hold dirhandle information, so let's test them too.

    SKIP: {
        skip "No dirfd()", 9 unless $Config{d_dirfd} || $Config{d_dir_dd_fd};
        ok(opendir(DIR, "."), 'Can open "." dir') || diag "Can't open '.':  $!";
        ok(stat(*DIR{IO}), "stat() on *DIR{IO} works");
	ok(-d _ , "The special file handle _ is set correctly"); 
        ok(-d -r *DIR{IO} , "chained -x's on *DIR{IO}");

	# And now for the ambiguous bareword case
	{
	    no warnings 'deprecated';
	    ok(open(DIR, "TEST"), 'Can open "TEST" dir')
		|| diag "Can't open 'TEST':  $!";
	}
	my $size = (stat(*DIR{IO}))[7];
	ok(defined $size, "stat() on *THINGY{IO} works");
	is($size, -s "TEST",
	   "size returned by stat of *THINGY{IO} is for the file");
	ok(-f _, "ambiguous *THINGY{IO} uses file handle, not dir handle");
	ok(-f *DIR{IO});
	closedir DIR or die $!;
	close DIR or die $!;
    }
}

# [perl #71002]
{
    local $^W = 1;
    my $w;
    local $SIG{__WARN__} = sub { warn shift; ++$w };
    stat 'prepeinamehyparcheiarcheiometoonomaavto';
    stat _;
    is $w, undef, 'no unopened warning from stat _';
}

END {
    chmod 0666, $tmpfile;
    unlink_all $tmpfile;
}
