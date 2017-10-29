use strict;

BEGIN {
    require Time::HiRes;
    require Test::More;
    require File::Temp;
    unless(&Time::HiRes::d_hires_utime) {
	Test::More::plan(skip_all => "no hires_utime");
    }
    unless (&Time::HiRes::d_futimens) {
	Test::More::plan(skip_all => "no futimens()");
    }
    unless (&Time::HiRes::d_utimensat) {
	Test::More::plan(skip_all => "no utimensat()");
    }
    if ($^O eq 'gnukfreebsd') {
	Test::More::plan(skip_all => "futimens() and utimensat() not working in $^O");
    }
    if ($^O eq 'linux' && -e '/proc/mounts') {
        # The linux might be wrong when ext3
        # is available in other operating systems,
        # but then we need other methods for detecting
        # the filesystem type of the tempfiles.
        my ($fh, $fn) = File::Temp::tempfile( "Time-HiRes-utime-XXXXXXXXX", UNLINK => 1);
        sub getfstype {
            my ($fn) = @_;
            my $cmd = "df $fn";
            open(my $df, "$cmd |") or die "$cmd: $!";
             my @df = <$df>;  # Assume $df[0] is header line.
             my $dev = +(split(" ", $df[1]))[0];
             open(my $mounts, "/proc/mounts") or die "/proc/mounts: $!";
             while (<$mounts>) {
                 my @m = split(" ");
                 if ($m[0] eq $dev) { return $m[2] }
             }
             return;
          }
          my $fstype = getfstype($fn);
          unless (defined $fstype) {
              warn "Unknown fstype for $fn\n";
          } else {
              print "# fstype = $fstype\n";
              if ($fstype eq 'ext3' || $fstype eq 'ext2') {
                  Test::More::plan(skip_all => "fstype $fstype has no subsecond timestamps in $^O");
            }
        }
    }
}

use Test::More tests => 18;
use t::Watchdog;
use File::Temp qw( tempfile );

use Config;

# Hope initially for nanosecond accuracy.
my $atime = 1.111111111;
my $mtime = 2.222222222;

if ($^O eq 'cygwin') {
   # Cygwin timestamps have less precision.
   $atime = 1.1111111;
   $mtime = 2.2222222;
}
print "# \$^O = $^O, atime = $atime, mtime = $mtime\n";

print "# utime \$fh\n";
{
	my ($fh, $filename) = tempfile( "Time-HiRes-utime-XXXXXXXXX", UNLINK => 1 );
	is Time::HiRes::utime($atime, $mtime, $fh), 1, "One file changed";
	my ($got_atime, $got_mtime) = ( Time::HiRes::stat($filename) )[8, 9];
	is $got_atime, $atime, "atime set correctly";
	is $got_mtime, $mtime, "mtime set correctly";
};

print "#utime \$filename\n";
{
	my ($fh, $filename) = tempfile( "Time-HiRes-utime-XXXXXXXXX", UNLINK => 1 );
	is Time::HiRes::utime($atime, $mtime, $filename), 1, "One file changed";
	my ($got_atime, $got_mtime) = ( Time::HiRes::stat($fh) )[8, 9];
	is $got_atime, $atime, "atime set correctly";
	is $got_mtime, $mtime, "mtime set correctly";
};

print "utime \$filename and \$fh\n";
{
	my ($fh1, $filename1) = tempfile( "Time-HiRes-utime-XXXXXXXXX", UNLINK => 1 );
	my ($fh2, $filename2) = tempfile( "Time-HiRes-utime-XXXXXXXXX", UNLINK => 1 );
	is Time::HiRes::utime($atime, $mtime, $filename1, $fh2), 2, "Two files changed";
	{
		my ($got_atime, $got_mtime) = ( Time::HiRes::stat($fh1) )[8, 9];
		is $got_atime, $atime, "File 1 atime set correctly";
		is $got_mtime, $mtime, "File 1 mtime set correctly";
	}
	{
		my ($got_atime, $got_mtime) = ( Time::HiRes::stat($filename2) )[8, 9];
		is $got_atime, $atime, "File 2 atime set correctly";
		is $got_mtime, $mtime, "File 2 mtime set correctly";
	}
};

print "# utime undef sets time to now\n";
{
	my ($fh1, $filename1) = tempfile( "Time-HiRes-utime-XXXXXXXXX", UNLINK => 1 );
	my ($fh2, $filename2) = tempfile( "Time-HiRes-utime-XXXXXXXXX", UNLINK => 1 );

	my $now = Time::HiRes::time;
	is Time::HiRes::utime(undef, undef, $filename1, $fh2), 2, "Two files changed";

	{
		my ($got_atime, $got_mtime) = ( Time::HiRes::stat($fh1) )[8, 9];
		cmp_ok abs( $got_atime - $now), '<', 0.1, "File 1 atime set correctly";
		cmp_ok abs( $got_mtime - $now), '<', 0.1, "File 1 mtime set correctly";
	}
	{
		my ($got_atime, $got_mtime) = ( Time::HiRes::stat($filename2) )[8, 9];
		cmp_ok abs( $got_atime - $now), '<', 0.1, "File 2 atime set correctly";
		cmp_ok abs( $got_mtime - $now), '<', 0.1, "File 2 mtime set correctly";
	}
};

print "# negative atime dies\n";
{
	eval { Time::HiRes::utime(-4, $mtime) };
	like $@, qr/::utime\(-4, 2\.22222\): negative time not invented yet/,
		"negative time error";
};

print "# negative mtime dies;\n";
{
	eval { Time::HiRes::utime($atime, -4) };
	like $@, qr/::utime\(1.11111, -4\): negative time not invented yet/,
		"negative time error";
};

done_testing;

1;
