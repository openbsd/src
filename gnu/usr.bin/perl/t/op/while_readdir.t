#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
use warnings;

plan 10;

# Need to run this in a quiet private directory as it assumes that it can read
# the contents twice and get the same result.
my $tempdir = tempfile;

mkdir $tempdir, 0700 or die "Can't mkdir '$tempdir': $!";
chdir $tempdir or die die "Can't chdir '$tempdir': $!";

my $cleanup = 1;
my %tempfiles;

END {
    if ($cleanup) {
	foreach my $file (keys %tempfiles) {
	    # We only wrote each of these once so 1 delete should work:
	    if (unlink $file) {
		warn "unlink tempfile '$file' passed but it's still there"
		    if -e $file;
	    } else {
		warn "Couldn't unlink tempfile '$file': $!";
	    }
	}
	chdir '..' or die "Couldn't chdir .. for cleanup: $!";
	rmdir $tempdir or die "Couldn't unlink tempdir '$tempdir': $!";
    }
}

# This is intentionally not random (per run), but intentionally will try to
# give different file names for different people running this test.
srand $< * $];

my @chars = ('A' .. 'Z', 'a' .. 'z', 0 .. 9);

sub make_file {
    my $name = shift;

    return if $tempfiles{$name}++;

    print "# Writing to $name in $tempdir\n";

    open my $fh, '>', $name or die "Can't open '$name' for writing: $!\n";
    print $fh <<'FILE0';
This file is here for testing

while(readdir $dir){...}
... while readdir $dir

etc
FILE0
    close $fh or die "Can't close '$name': $!";
}

sub make_some_files {
    for (1..int rand 10) {
	my $name;
	$name .= $chars[rand $#chars] for 1..int(10 + rand 5);
	make_file($name);
    }
}

make_some_files();
make_file('0');
make_some_files();

ok(-f '0', "'0' file is here");

opendir my $dirhandle, '.'
    or die "Failed test: unable to open directory: $!\n";

my @dir = readdir $dirhandle;
rewinddir $dirhandle;

{
    my @list;
    while(readdir $dirhandle){
	push @list, $_;
    }
    ok( eq_array( \@dir, \@list ), 'while(readdir){push}' );
    rewinddir $dirhandle;
}

{
    my @list;
    push @list, $_ while readdir $dirhandle;
    ok( eq_array( \@dir, \@list ), 'push while readdir' );
    rewinddir $dirhandle;
}

{
    my $tmp;
    my @list;
    push @list, $tmp while $tmp = readdir $dirhandle;
    ok( eq_array( \@dir, \@list ), 'push $dir while $dir = readdir' );
    rewinddir $dirhandle;
}

{
    my @list;
    while( my $dir = readdir $dirhandle){
	push @list, $dir;
    }
    ok( eq_array( \@dir, \@list ), 'while($dir=readdir){push}' );
    rewinddir $dirhandle;
}


{
    my @list;
    my $sub = sub{
	push @list, $_;
    };
    $sub->($_) while readdir $dirhandle;
    ok( eq_array( \@dir, \@list ), '$sub->($_) while readdir' );
    rewinddir $dirhandle;
}

{
    my $works = 0;
    while(readdir $dirhandle){
        $_ =~ s/\.$// if defined $_ && $^O eq 'VMS'; # may have zero-length extension
        if( defined $_ && $_ eq '0'){
            $works = 1;
            last;
        }
    }
    ok( $works, 'while(readdir){} with file named "0"' );
    rewinddir $dirhandle;
}

{
    my $works = 0;
    my $sub = sub{
        $_ =~ s/\.$// if defined $_ && $^O eq 'VMS'; # may have zero-length extension
        if( defined $_ && $_ eq '0' ){
            $works = 1;
        }
    };
    $sub->($_) while readdir $dirhandle;
    ok( $works, '$sub->($_) while readdir; with file named "0"' );
    rewinddir $dirhandle;
}

{
    my $works = 0;
    while( my $dir = readdir $dirhandle ){
        $dir =~ s/\.$// if defined $dir && $^O eq 'VMS'; # may have zero-length extension
        if( defined $dir && $dir eq '0'){
            $works = 1;
            last;
        }
    }
    ok( $works, 'while($dir=readdir){} with file named "0"');
    rewinddir $dirhandle;
}

{
    my $tmp;
    my $ok;
    my @list;
    while( $tmp = readdir $dirhandle ){
        $tmp =~ s/\.$// if defined $tmp && $^O eq 'VMS'; # may have zero-length extension
        last if defined($tmp)&& !$tmp && ($ok=1) 
    }
    ok( $ok, '$dir while $dir = readdir; with file named "0"'  );
    rewinddir $dirhandle;
}

closedir $dirhandle;
