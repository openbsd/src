#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Test::More;

my $TB = Test::More->builder;

plan tests => 60;

# We're going to override rename() later on but Perl has to see an override
# at compile time to honor it.
BEGIN { *CORE::GLOBAL::rename = sub { CORE::rename($_[0], $_[1]) }; }


use File::Copy;
use Config;


foreach my $code ("copy()", "copy('arg')", "copy('arg', 'arg', 'arg', 'arg')",
                  "move()", "move('arg')", "move('arg', 'arg', 'arg')"
                 )
{
    eval $code;
    like $@, qr/^Usage: /;
}


for my $cross_partition_test (0..1) {
  {
    # Simulate a cross-partition copy/move by forcing rename to
    # fail.
    no warnings 'redefine';
    *CORE::GLOBAL::rename = sub { 0 } if $cross_partition_test;
  }

  # First we create a file
  open(F, ">file-$$") or die;
  binmode F; # for DOSISH platforms, because test 3 copies to stdout
  printf F "ok\n";
  close F;

  copy "file-$$", "copy-$$";

  open(F, "copy-$$") or die;
  $foo = <F>;
  close(F);

  is -s "file-$$", -s "copy-$$";

  is $foo, "ok\n";

  binmode STDOUT unless $^O eq 'VMS'; # Copy::copy works in binary mode
  # This outputs "ok" so its a test.
  copy "copy-$$", \*STDOUT;
  $TB->current_test($TB->current_test + 1);
  unlink "copy-$$" or die "unlink: $!";

  open(F,"file-$$");
  copy(*F, "copy-$$");
  open(R, "copy-$$") or die "open copy-$$: $!"; $foo = <R>; close(R);
  is $foo, "ok\n";
  unlink "copy-$$" or die "unlink: $!";

  open(F,"file-$$");
  copy(\*F, "copy-$$");
  close(F) or die "close: $!";
  open(R, "copy-$$") or die; $foo = <R>; close(R) or die "close: $!";
  is $foo, "ok\n";
  unlink "copy-$$" or die "unlink: $!";

  require IO::File;
  $fh = IO::File->new(">copy-$$") or die "Cannot open copy-$$:$!";
  binmode $fh or die;
  copy("file-$$",$fh);
  $fh->close or die "close: $!";
  open(R, "copy-$$") or die; $foo = <R>; close(R);
  is $foo, "ok\n";
  unlink "copy-$$" or die "unlink: $!";

  require FileHandle;
  my $fh = FileHandle->new(">copy-$$") or die "Cannot open copy-$$:$!";
  binmode $fh or die;
  copy("file-$$",$fh);
  $fh->close;
  open(R, "copy-$$") or die; $foo = <R>; close(R);
  is $foo, "ok\n";
  unlink "file-$$" or die "unlink: $!";

  ok !move("file-$$", "copy-$$"), "move on missing file";
  ok -e "copy-$$",                '  target still there';

  # Doesn't really matter what time it is as long as its not now.
  my $time = 1000000000;
  utime( $time, $time, "copy-$$" );

  # Recheck the mtime rather than rely on utime in case we're on a
  # system where utime doesn't work or there's no mtime at all.
  # The destination file will reflect the same difficulties.
  my $mtime = (stat("copy-$$"))[9];

  ok move("copy-$$", "file-$$"), 'move';
  ok -e "file-$$",              '  destination exists';
  ok !-e "copy-$$",              '  source does not';
  open(R, "file-$$") or die; $foo = <R>; close(R);
  is $foo, "ok\n";

  TODO: {
    local $TODO = 'mtime only preserved on ODS-5 with POSIX dates and DECC$EFS_FILE_TIMESTAMPS enabled' if $^O eq 'VMS';

    my $dest_mtime = (stat("file-$$"))[9];
    is $dest_mtime, $mtime,
      "mtime preserved by copy()". 
      ($cross_partition_test ? " while testing cross-partition" : "");
  }

  copy "file-$$", "lib";
  open(R, "lib/file-$$") or die; $foo = <R>; close(R);
  is $foo, "ok\n";
  unlink "lib/file-$$" or die "unlink: $!";

  # Do it twice to ensure copying over the same file works.
  copy "file-$$", "lib";
  open(R, "lib/file-$$") or die; $foo = <R>; close(R);
  is $foo, "ok\n";
  unlink "lib/file-$$" or die "unlink: $!";

  { 
    my $warnings = '';
    local $SIG{__WARN__} = sub { $warnings .= join '', @_ };
    ok copy("file-$$", "file-$$");

    like $warnings, qr/are identical/;
    ok -s "file-$$";
  }

  move "file-$$", "lib";
  open(R, "lib/file-$$") or die "open lib/file-$$: $!"; $foo = <R>; close(R);
  is $foo, "ok\n";
  ok !-e "file-$$";
  unlink "lib/file-$$" or die "unlink: $!";

  SKIP: {
    skip "Testing symlinks", 3 unless $Config{d_symlink};

    open(F, ">file-$$") or die $!;
    print F "dummy content\n";
    close F;
    symlink("file-$$", "symlink-$$") or die $!;

    my $warnings = '';
    local $SIG{__WARN__} = sub { $warnings .= join '', @_ };
    ok !copy("file-$$", "symlink-$$");

    like $warnings, qr/are identical/;
    ok !-z "file-$$", 
      'rt.perl.org 5196: copying to itself would truncate the file';

    unlink "symlink-$$";
    unlink "file-$$";
  }

  SKIP: {
    skip "Testing hard links", 3 if !$Config{d_link} or $^O eq 'MSWin32';

    open(F, ">file-$$") or die $!;
    print F "dummy content\n";
    close F;
    link("file-$$", "hardlink-$$") or die $!;

    my $warnings = '';
    local $SIG{__WARN__} = sub { $warnings .= join '', @_ };
    ok !copy("file-$$", "hardlink-$$");

    like $warnings, qr/are identical/;
    ok ! -z "file-$$",
      'rt.perl.org 5196: copying to itself would truncate the file';

    unlink "hardlink-$$";
    unlink "file-$$";
  }
}


END {
    1 while unlink "file-$$";
    1 while unlink "lib/file-$$";
}
