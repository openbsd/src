#!perl -w

# tests for Win32::GetLongPathName()

$^O =~ /^MSWin/ or print("1..0 # not win32\n" ), exit;

my @paths = qw(
    /
    //
    .
    ..
    c:
    c:/
    c:./
    c:/.
    c:/..
    c:./..
    //./
    //.
    //..
    //./..
);
push @paths, map { my $x = $_; $x =~ s,/,\\,g; $x } @paths;
push @paths, qw(
    ../\
    c:.\\../\
    c:/\..//
    c://.\/./\
    \\.\\../\
    //\..//
    //.\/./\
);

my $drive = $ENV{SystemDrive};
if ($drive) {
    for (@paths) {
	s/^c:/$drive/;
    }
    push @paths, $ENV{SystemRoot} if $ENV{SystemRoot};
}
my %expect;
@expect{@paths} = map { my $x = $_; $x =~ s,(.[/\\])[/\\]+,$1,g; $x } @paths;

print "1.." . @paths . "\n";
my $i = 1;
for (@paths) {
    my $got = Win32::GetLongPathName($_);
    print "# '$_' => expect '$expect{$_}' => got '$got'\n";
    print "not " unless $expect{$_} eq $got;
    print "ok $i\n";
    ++$i;
}
