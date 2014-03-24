#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require "./test.pl";
}

use Config;

my $Is_VMSish = ($^O eq 'VMS');

if (($^O eq 'MSWin32') || ($^O eq 'NetWare')) {
    $wd = `cd`;
}
elsif ($^O eq 'VMS') {
    $wd = `show default`;
}
else {
    $wd = `pwd`;
}
chomp($wd);

my $has_link            = $Config{d_link};
my $accurate_timestamps =
    !($^O eq 'MSWin32' || $^O eq 'NetWare' ||
      $^O eq 'dos'     || $^O eq 'os2'     ||
      $^O eq 'cygwin'  || $^O eq 'amigaos' ||
	  $wd =~ m#$Config{afsroot}/#
     );

if (defined &Win32::IsWinNT && Win32::IsWinNT()) {
    if (Win32::FsType() eq 'NTFS') {
        $has_link            = 1;
        $accurate_timestamps = 1;
    }
}

my $needs_fh_reopen =
    $^O eq 'dos'
    # Not needed on HPFS, but needed on HPFS386 ?!
    || $^O eq 'os2';

$needs_fh_reopen = 1 if (defined &Win32::IsWin95 && Win32::IsWin95());

my $skip_mode_checks =
    $^O eq 'cygwin' && $ENV{CYGWIN} !~ /ntsec/;

plan tests => 52;

my $tmpdir = tempfile();
my $tmpdir1 = tempfile();

if (($^O eq 'MSWin32') || ($^O eq 'NetWare')) {
    `rmdir /s /q $tmpdir 2>nul`;
    `mkdir $tmpdir`;
}
elsif ($^O eq 'VMS') {
    `if f\$search("[.$tmpdir]*.*") .nes. "" then delete/nolog/noconfirm [.$tmpdir]*.*.*`;
    `if f\$search("$tmpdir.dir") .nes. "" then set file/prot=o:rwed $tmpdir.dir;`;
    `if f\$search("$tmpdir.dir") .nes. "" then delete/nolog/noconfirm $tmpdir.dir;`;
    `create/directory [.$tmpdir]`;
}
else {
    `rm -f $tmpdir 2>/dev/null; mkdir $tmpdir 2>/dev/null`;
}

chdir $tmpdir;

`/bin/rm -rf a b c x` if -x '/bin/rm';

umask(022);

SKIP: {
    skip "bogus umask", 1 if ($^O eq 'MSWin32') || ($^O eq 'NetWare');

    is((umask(0)&0777), 022, 'umask'),
}

open(FH,'>x') || die "Can't create x";
close(FH);
open(FH,'>a') || die "Can't create a";
close(FH);

my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks,$a_mode);

SKIP: {
    skip("no link", 4) unless $has_link;

    ok(link('a','b'), "link a b");
    ok(link('b','c'), "link b c");

    $a_mode = (stat('a'))[2];

    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
     $blksize,$blocks) = stat('c');

    SKIP: {
        skip "no nlink", 1 if $Config{dont_use_nlink};

        is($nlink, 3, "link count of triply-linked file");
    }

    SKIP: {
        skip "hard links not that hard in $^O", 1 if $^O eq 'amigaos';
	skip "no mode checks", 1 if $skip_mode_checks;

#      if ($^O eq 'cygwin') { # new files on cygwin get rwx instead of rw-
#          is($mode & 0777, 0777, "mode of triply-linked file");
#      } else {
            is(sprintf("0%o", $mode & 0777), 
               sprintf("0%o", $a_mode & 0777), 
               "mode of triply-linked file");
#      }
    }
}

$newmode = (($^O eq 'MSWin32') || ($^O eq 'NetWare')) ? 0444 : 0777;

is(chmod($newmode,'a'), 1, "chmod succeeding");

SKIP: {
    skip("no link", 7) unless $has_link;

    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
     $blksize,$blocks) = stat('c');

    SKIP: {
	skip "no mode checks", 1 if $skip_mode_checks;

        is($mode & 0777, $newmode, "chmod going through");
    }

    $newmode = 0700;
    chmod 0444, 'x';
    $newmode = 0666;

    is(chmod($newmode,'c','x'), 2, "chmod two files");

    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
     $blksize,$blocks) = stat('c');

    SKIP: {
	skip "no mode checks", 1 if $skip_mode_checks;

        is($mode & 0777, $newmode, "chmod going through to c");
    }

    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
     $blksize,$blocks) = stat('x');

    SKIP: {
	skip "no mode checks", 1 if $skip_mode_checks;

        is($mode & 0777, $newmode, "chmod going through to x");
    }

    is(unlink('b','x'), 2, "unlink two files");

    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
     $blksize,$blocks) = stat('b');

    is($ino, undef, "ino of removed file b should be undef");

    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
     $blksize,$blocks) = stat('x');

    is($ino, undef, "ino of removed file x should be undef");
}

SKIP: {
    skip "no fchmod", 5 unless ($Config{d_fchmod} || "") eq "define";
    ok(open(my $fh, "<", "a"), "open a");
    is(chmod(0, $fh), 1, "fchmod");
    $mode = (stat "a")[2];
    SKIP: {
        skip "no mode checks", 1 if $skip_mode_checks;
        is($mode & 0777, 0, "perm reset");
    }
    is(chmod($newmode, "a"), 1, "fchmod");
    $mode = (stat $fh)[2];
    SKIP: { 
        skip "no mode checks", 1 if $skip_mode_checks;
        is($mode & 0777, $newmode, "perm restored");
    }
}

SKIP: {
    skip "no fchown", 1 unless ($Config{d_fchown} || "") eq "define";
    open(my $fh, "<", "a");
    is(chown(-1, -1, $fh), 1, "fchown");
}

SKIP: {
    skip "has fchmod", 1 if ($Config{d_fchmod} || "") eq "define";
    open(my $fh, "<", "a");
    eval { chmod(0777, $fh); };
    like($@, qr/^The fchmod function is unimplemented at/, "fchmod is unimplemented");
}

SKIP: {
    skip "has fchown", 1 if ($Config{d_fchown} || "") eq "define";
    open(my $fh, "<", "a");
    eval { chown(0, 0, $fh); };
    like($@, qr/^The f?chown function is unimplemented at/, "fchown is unimplemented");
}

is(rename('a','b'), 1, "rename a b");

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
 $blksize,$blocks) = stat('a');

is($ino, undef, "ino of renamed file a should be undef");

$delta = $accurate_timestamps ? 1 : 2;	# Granularity of time on the filesystem
chmod 0777, 'b';

$foo = (utime 500000000,500000000 + $delta,'b');
is($foo, 1, "utime");
check_utime_result();

utime undef, undef, 'b';
($atime,$mtime) = (stat 'b')[8,9];
print "# utime undef, undef --> $atime, $mtime\n";
isnt($atime, 500000000, 'atime');
isnt($mtime, 500000000 + $delta, 'mtime');

SKIP: {
    skip "no futimes", 4 unless ($Config{d_futimes} || "") eq "define";
    open(my $fh, "<", 'b');
    $foo = (utime 500000000,500000000 + $delta, $fh);
    is($foo, 1, "futime");
    check_utime_result();
}


sub check_utime_result {
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
     $blksize,$blocks) = stat('b');

 SKIP: {
	skip "bogus inode num", 1 if ($^O eq 'MSWin32') || ($^O eq 'NetWare');

	ok($ino,    'non-zero inode num');
    }

 SKIP: {
	skip "filesystem atime/mtime granularity too low", 2
	    unless $accurate_timestamps;

	print "# atime - $atime  mtime - $mtime  delta - $delta\n";
	if($atime == 500000000 && $mtime == 500000000 + $delta) {
	    pass('atime');
	    pass('mtime');
	}
	else {
	    if ($^O =~ /\blinux\b/i) {
		print "# Maybe stat() cannot get the correct atime, ".
		    "as happens via NFS on linux?\n";
		$foo = (utime 400000000,500000000 + 2*$delta,'b');
		my ($new_atime, $new_mtime) = (stat('b'))[8,9];
		print "# newatime - $new_atime  nemtime - $new_mtime\n";
		if ($new_atime == $atime && $new_mtime - $mtime == $delta) {
		    pass("atime - accounted for possible NFS/glibc2.2 bug on linux");
		    pass("mtime - accounted for possible NFS/glibc2.2 bug on linux");
		}
		else {
		    fail("atime - $atime/$new_atime $mtime/$new_mtime");
		    fail("mtime - $atime/$new_atime $mtime/$new_mtime");
		}
	    }
	    elsif ($^O eq 'VMS') {
		# why is this 1 second off?
		is( $atime, 500000001,          'atime' );
		is( $mtime, 500000000 + $delta, 'mtime' );
	    }
	    elsif ($^O eq 'haiku') {
            SKIP: {
		    skip "atime not updated", 1;
		}
		is($mtime, 500000001, 'mtime');
	    }
	    else {
		fail("atime");
		fail("mtime");
	    }
	}
    }
}

SKIP: {
    skip "has futimes", 1 if ($Config{d_futimes} || "") eq "define";
    open(my $fh, "<", "b") || die;
    eval { utime(undef, undef, $fh); };
    like($@, qr/^The futimes function is unimplemented at/, "futimes is unimplemented");
}

is(unlink('b'), 1, "unlink b");

($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,$ctime,
    $blksize,$blocks) = stat('b');
is($ino, undef, "ino of unlinked file b should be undef");
unlink 'c';

chdir $wd || die "Can't cd back to $wd";

# Yet another way to look for links (perhaps those that cannot be
# created by perl?).  Hopefully there is an ls utility in your
# %PATH%. N.B. that $^O is 'cygwin' on Cygwin.

SKIP: {
    skip "Win32/Netware specific test", 2
      unless ($^O eq 'MSWin32') || ($^O eq 'NetWare');
    skip "No symbolic links found to test with", 2
      unless  `ls -l perl 2>nul` =~ /^l.*->/;

    system("cp TEST TEST$$");
    # we have to copy because e.g. GNU grep gets huffy if we have
    # a symlink forest to another disk (it complains about too many
    # levels of symbolic links, even if we have only two)
    is(symlink("TEST$$","c"), 1, "symlink");
    $foo = `grep perl c 2>&1`;
    ok($foo, "found perl in c");
    unlink 'c';
    unlink("TEST$$");
}

my $tmpfile = tempfile();
open IOFSCOM, ">$tmpfile" or die "Could not write IOfs.tmp: $!";
print IOFSCOM 'helloworld';
close(IOFSCOM);

# TODO: pp_truncate needs to be taught about F_CHSIZE and F_FREESP,
# as per UNIX FAQ.

SKIP: {
# Check truncating a closed file.
    eval { truncate $tmpfile, 5; };

    skip("no truncate - $@", 8) if $@;

    is(-s $tmpfile, 5, "truncation to five bytes");

    truncate $tmpfile, 0;

    ok(-z $tmpfile,    "truncation to zero bytes");

#these steps are necessary to check if file is really truncated
#On Win95, FH is updated, but file properties aren't
    open(FH, ">$tmpfile") or die "Can't create $tmpfile";
    print FH "x\n" x 200;
    close FH;

# Check truncating an open file.
    open(FH, ">>$tmpfile") or die "Can't open $tmpfile for appending";

    binmode FH;
    select FH;
    $| = 1;
    select STDOUT;

    {
	use strict;
	print FH "x\n" x 200;
	ok(truncate(FH, 200), "fh resize to 200");
    }

    if ($needs_fh_reopen) {
	close (FH); open (FH, ">>$tmpfile") or die "Can't reopen $tmpfile";
    }

    SKIP: {
        if ($^O eq 'vos') {
	    skip ("# TODO - hit VOS bug posix-973 - cannot resize an open file below the current file pos.", 6);
	}

	is(-s $tmpfile, 200, "fh resize to 200 working (filename check)");

	ok(truncate(FH, 0), "fh resize to zero");

	if ($needs_fh_reopen) {
	    close (FH); open (FH, ">>$tmpfile") or die "Can't reopen $tmpfile";
	}

	ok(-z $tmpfile, "fh resize to zero working (filename check)");

	close FH;

	open(FH, ">>$tmpfile") or die "Can't open $tmpfile for appending";

	binmode FH;
	select FH;
	$| = 1;
	select STDOUT;

	{
	    use strict;
	    print FH "x\n" x 200;
	    ok(truncate(*FH{IO}, 100), "fh resize by IO slot");
	}

	if ($needs_fh_reopen) {
	    close (FH); open (FH, ">>$tmpfile") or die "Can't reopen $tmpfile";
	}

	is(-s $tmpfile, 100, "fh resize by IO slot working");

	close FH;

	my $n = "for_fs_dot_t$$";
	open FH, ">$n" or die "open $n: $!";
	print FH "bloh blah bla\n";
	close FH or die "close $n: $!";
	eval "truncate $n, 0; 1" or die;
	ok !-z $n, 'truncate(word) does not fall back to file name';
	unlink $n;
    }
}

# check if rename() can be used to just change case of filename
SKIP: {
    skip "Works in Cygwin only if check_case is set to relaxed", 1
      if ($ENV{'CYGWIN'} && ($ENV{'CYGWIN'} =~ /check_case:(?:adjust|strict)/));

    chdir "./$tmpdir";
    open(FH,'>x') || die "Can't create x";
    close(FH);
    rename('x', 'X');

    # this works on win32 only, because fs isn't casesensitive
    ok(-e 'X', "rename working");

    unlink_all 'X';
    chdir $wd || die "Can't cd back to $wd";
}

# check if rename() works on directories
if ($^O eq 'VMS') {
    # must have delete access to rename a directory
    `set file $tmpdir.dir/protection=o:d`;
    ok(rename("$tmpdir.dir", "$tmpdir1.dir"), "rename on directories") ||
      print "# errno: $!\n";
}
else {
    ok(rename($tmpdir, $tmpdir1), "rename on directories");
}

ok(-d $tmpdir1, "rename on directories working");

{
    # Change 26011: Re: A surprising segfault
    # to make sure only that these obfuscated sentences will not crash.

    map chmod(+()), ('')x68;
    ok(1, "extend sp in pp_chmod");

    map chown(+()), ('')x68;
    ok(1, "extend sp in pp_chown");
}

# need to remove $tmpdir if rename() in test 28 failed!
END { rmdir $tmpdir1; rmdir $tmpdir; }
