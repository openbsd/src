#  -*- perl -*-
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl File-Temp.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test::More tests => 7;
BEGIN { use_ok('File::Temp') };

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

# make sure we can create a tmp file...
$tmp = File::Temp->new;
isa_ok( $tmp, 'File::Temp' );
isa_ok( $tmp, 'IO::Handle' );
isa_ok( $tmp, 'IO::Seekable' );

# make sure the seek method is available...
ok( File::Temp->can('seek'), 'tmp can seek' );

# make sure IO::Handle methods are still there...
ok( File::Temp->can('print'), 'tmp can print' );

# let's see what we're exporting...
$c = scalar @File::Temp::EXPORT;
$l = join ' ', @File::Temp::EXPORT;
ok( $c == 9, "really exporting $c: $l" );
