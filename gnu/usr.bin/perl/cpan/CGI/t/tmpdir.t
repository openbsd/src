#!perl
use Test::More tests => 9;
use strict;

my ($testdir, $testdir2);

BEGIN {
 $testdir = "CGItest";
 $testdir2 = "CGItest2";
 for ($testdir, $testdir2) {
 ( -d ) || mkdir $_;
 ( ! -w ) || chmod 0700, $_;
 }
 $CGITempFile::TMPDIRECTORY = $testdir;
 $ENV{TMPDIR} = $testdir2;
}

use CGI;
is($CGITempFile::TMPDIRECTORY, $testdir, "can pre-set \$CGITempFile::TMPDIRECTORY");
CGITempFile->new;
is($CGITempFile::TMPDIRECTORY, $testdir, "\$CGITempFile::TMPDIRECTORY unchanged");

TODO: {
 local $TODO = "figuring out why these tests fail on some platforms";
 ok(chmod 0500, $testdir, "revoking write access to $testdir");
 ok(! -w $testdir, "write access to $testdir revoked");
CGITempFile->new;
is($CGITempFile::TMPDIRECTORY, $testdir2,
 "unwritable \$CGITempFile::TMPDIRECTORY overridden");

ok(chmod 0500, $testdir2, "revoking write access to $testdir2");
ok(! -w $testdir, "write access to $testdir revoked");
CGITempFile->new;
isnt($CGITempFile::TMPDIRECTORY, $testdir2,
 "unwritable \$ENV{TMPDIR} overridden");
isnt($CGITempFile::TMPDIRECTORY, $testdir,
 "unwritable \$ENV{TMPDIR} not overridden with an unwritable \$CGITempFile::TMPDIRECTORY");
}

END { for ($testdir, $testdir2) { chmod 0700, $_; rmdir; } }
