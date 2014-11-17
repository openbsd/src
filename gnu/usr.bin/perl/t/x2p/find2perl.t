#!./perl

# Based on ideas from x2p/s2p.t
#
# This doesn't currently test -exec etc, just the default -print on
# the platforms below.

BEGIN {
    chdir 't' if -d 't';
    @INC = ( '../lib' );
}

use strict;
use warnings;
use File::Path 'remove_tree';
use File::Spec;
require "./test.pl";

# add more platforms if you feel like it, but make sure the
# tests below are portable to the find(1) for any new platform,
# or that they skip on that platform
$^O =~ /^(?:linux|\w+bsd|darwin)$/
    or skip_all("Need something vaguely POSIX");

my $VERBOSE = grep $_ eq '-v', @ARGV;

my $tmpdir = tempfile();
my $script = tempfile();
mkdir $tmpdir
    or die "Cannot make temp dir $tmpdir: $!";

# test file names shouldn't contain any shell special characters,
# and for portability, probably shouldn't contain any high ascii or
# Unicode characters
#
# handling Unicode here would be nice, but I think handling of Unicode
# in perl's file system interfaces (open, unlink, readdir) etc needs to
# be more regular before we can expect interoperability between find2perl
# and a system find(1)
#
# keys for the test file list:
#   name - required
#   type - type of file to create:
#      "f" regular file, "d" directory, "l" link to target,
#      "s" symlink to target
#   atime, mtime - file times (default now)
#   mode - file mode (default per umask)
#   content - file content for type f files
#   target - target for link for type l and s
#
# I could have simply written code to create all the files, but I think
# this makes the file tree a little more obvious
use constant HOUR => 3600; # an hour in seconds
my @test_files =
    (
        { name => "abc" },
        { name => "acc", mtime => time() - HOUR * 48 },
        { name => "ac", content => "x" x 10 },
        { name => "somedir", type => "d" },
        { name => "link", type => "l", target => "abc" },
        { name => "symlink", type => "s", target => "brokenlink" },
    );
# make some files to search
for my $spec (@test_files) {
    my $file = File::Spec->catfile($tmpdir, split '/', $spec->{name});
    my $type = $spec->{type} || "f";
    if ($type eq "f") {
        open my $fh, ">", $file
            or die "Cannot create test file $file: $!";
        if ($spec->{content}) {
            binmode $fh;
            print $fh $spec->{content};
        }
        close $fh
            or die "Cannot close $file: $!";
    }
    elsif ($type eq "d") {
        mkdir $file
            or die "Cannot create test directory $file: $!";
    }
    elsif ($type eq "l") {
        my $target = File::Spec->catfile($tmpdir, split '/', $spec->{target});
        link $target, $file
            or die "Cannot create test link $file: $!";
    }
    elsif ($type eq "s") {
        my $target = File::Spec->catfile($tmpdir, split '/', $spec->{target});
        symlink $target, $file
            or die "Cannot create test symlink $file: $!";
    }
    if ($spec->{mode}) {
        chmod $spec->{mode}, $file
            or die "Cannot set mode of test file $file: $!";
    }
    if ($spec->{mtime} || $spec->{atime}) {
        # default the times to now, since we just created the files
        my $mtime = $spec->{mtime} || time();
        my $atime = $spec->{atime} || time();
        utime $atime, $mtime, $file
            or die "Cannot set times of test file $file: $!";
    }
}

# do we have a vaguely sane find(1)?
# BusyBox find is non-POSIX - it doesn't have -links
my @files = sort `find '$tmpdir' '(' -name 'abc' -o -name 'acc' ')' -a -links +0`;
@files == 2 && $files[0] =~ /\babc\n\z/ && $files[1] =~ /\bacc\n\z/
    or skip_all("doesn't appear to be a sane find(1)");

# required keys:
#   args - find search spec as an array ref
# optional:
#   name - short description of the test (defaults to args)
#   expect - an array ref of files expected to be found (skips the find(1) call)
#   TODO - why this test is TODO (if it is), if a code reference that is
#          called to check if the test is TODO (and why)
#   SKIP - return a message for why to skip
my @testcases =
    (
        {
            name => "all files",
            args => [],
        },
        {
            name => "mapping of *",
            args => [ "-name", "a*c" ],
        },
        {
            args => [ "-type", "d" ],
            expect => [ "", "somedir" ],
        },
        {
            args => [ "-type", "f" ],
        },
        {
            args => [ "-mtime", "+1" ],
            expect => [ "acc" ],
        },
        {
            args => [ "-mtime", "-1" ],
        },
        {
            args => [ "-size", "10c" ],
            expect => [ "ac" ],
        },
        {
            args => [ "-links", "2" ],
        },
        {
            name => "[perl #113054] mapping of ?",
            args => [ "-name", "a?c" ],
        },
    );

plan(tests => 1 + 4 * @testcases);

my $find2perl = File::Spec->catfile(File::Spec->updir(), "x2p", "find2perl");
ok (-x $find2perl, "find2perl exists");
our $TODO;

for my $test (@testcases) {
 SKIP:
    {
        local $TODO = $test->{TODO};
        $TODO = $TODO->() if ref $TODO;
        my $args = $test->{args}
            or die "Missing test args";
        my $name = $test->{name} || "@$args";

        my $skip = $test->{SKIP} && $test->{SKIP}->();
        $skip
            and skip($skip, 4);

        my $code = runperl(args => [ $find2perl, $tmpdir, @$args ]);

        ok($code, "$name: run findperl")
            or skip("", 3);

        open my $script_fh, ">", $script
            or die "Cannot create $script: $!";
        print $script_fh $code;
        close $script_fh
            or die "Cannot close $script: $!";

        my $files = runperl(progfile => $script);

        ok(length $files, "$name: run output script")
            or skip("", 2);

        my $find_files;
        my $source;
        if ($test->{expect}) {
            $find_files = join "\n",
                map { $_ eq "" ? $tmpdir : "$tmpdir/$_" }
                @{$test->{expect}};
            $source = "expected";
            # to balance the ok() in the other branch
            pass("$name: got files ok");
        }
        else {
            my $findcmd = "find $tmpdir ". join " ", map "'$_'", @$args;

            # make sure PERL_UNICODE doesn't reinterpret the output of find
            use open IN => ':raw';
            $find_files = `$findcmd`;
            ok(length $find_files, "$name: run find")
                or skip("", 1);
            $source = "find";
        }

        # is the order from find (or find2perl) guaranteed?
        # assume it isn't
        $files = join("\n", sort split /\n/, $files);
        $find_files = join("\n", sort split /\n/, $find_files);

        if ($VERBOSE) {
            note("script:\n$code");
            note("args:\n@$args");
            note("find2perl:\n$files");
            note("find:\n$find_files");
        }

        is($files, $find_files, "$name: find2perl matches $source");
    }
}

END {
    remove_tree($tmpdir);
}
