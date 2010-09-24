#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
use warnings;

open my $fh, ">", "0" or die "Can't open '0' for writing: $!\n";
print $fh <<'FILE0';
This file is here for testing

while(readdir $dir){...}
... while readdir $dir

etc
FILE0
close $fh;

plan 10;

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

END { 1 while unlink "0" }
