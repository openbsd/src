#!/usr/local/bin/perl -w
# Test for File::Temp - OO interface

use strict;
use Test::More tests => 18;
use File::Spec;

# Will need to check that all files were unlinked correctly
# Set up an END block here to do it

# Arrays containing list of dirs/files to test
my (@files, @dirs, @still_there);

# And a test for files that should still be around
# These are tidied up
END {
  foreach (@still_there) {
    ok( -f $_, "Check $_ exists" );
    ok( unlink( $_ ), "Unlinked $_" );
    ok( !(-f $_), "$_ no longer there");
  }
}

# Loop over an array hoping that the files dont exist
END { foreach (@files) { ok( !(-e $_), "File $_ should not be there" )} }

# And a test for directories
END { foreach (@dirs)  { ok( !(-d $_), "Directory $_ should not be there" ) } }

# Need to make sure that the END blocks are setup before
# the ones that File::Temp configures since END blocks are evaluated
# in reverse order and we need to check the files *after* File::Temp
# removes them
BEGIN {use_ok( "File::Temp" ); }

# Tempfile
# Open tempfile in some directory, unlink at end
my $fh = new File::Temp( SUFFIX => '.txt' );

ok( (-f "$fh"), "File $fh exists"  );
# Should still be around after closing
ok( close( $fh ), "Close file $fh" );
ok( (-f "$fh"), "File $fh still exists after close" );
# Check again at exit
push(@files, "$fh");

# TEMPDIR test
# Create temp directory in current dir
my $template = 'tmpdirXXXXXX';
print "# Template: $template\n";
my $tempdir = File::Temp::tempdir( $template ,
				   DIR => File::Spec->curdir,
				   CLEANUP => 1,
				 );

print "# TEMPDIR: $tempdir\n";

ok( (-d $tempdir), "Does $tempdir directory exist" );
push(@dirs, $tempdir);

# Create file in the temp dir
$fh = new File::Temp(
		     DIR => $tempdir,
		     SUFFIX => '.dat',
		    );

print "# TEMPFILE: Created $fh\n";

ok( (-f "$fh"), "File $fh exists in tempdir?");
push(@files, "$fh");

# Test tempfile
# ..and again (without unlinking it)
$fh = new File::Temp( DIR => $tempdir, UNLINK => 0 );

print "# TEMPFILE: Created $fh\n";
ok( (-f "$fh" ), "Second file $fh exists in tempdir [nounlink]?");
push(@files, "$fh");

# and another (with template)

$fh = new File::Temp( TEMPLATE => 'helloXXXXXXX',
		      DIR => $tempdir,
		      SUFFIX => '.dat',
		    );

print "# TEMPFILE: Created $fh\n";

ok( (-f "$fh"), "File $fh exists? [from template]" );
push(@files, "$fh");


# Create a temporary file that should stay around after
# it has been closed
$fh = new File::Temp( TEMPLATE => 'permXXXXXXX', UNLINK => 0);

print "# TEMPFILE: Created $fh\n";
ok( -f "$fh", "File $fh exists?" );
ok( close( $fh ), "Close file $fh" );
push( @still_there, "$fh"); # check at END

# Make sure destructors run
undef $fh;

# Now END block will execute to test the removal of directories
print "# End of tests. Execute END blocks\n";

