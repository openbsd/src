#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if (!$Config{d_setlocale} || $Config{ccflags} =~ /\bD?NO_LOCALE\b/) {
	print "1..0\n";
	exit;
    }
}

print "1..7\n";

use I18N::Collate;

print "ok 1\n";

$a = I18N::Collate->new("foo");

print "ok 2\n";

{
    use warnings;
    local $SIG{__WARN__} = sub { $@ = $_[0] };
    $b = I18N::Collate->new("foo");
    print "not " unless $@ =~ /\bHAS BEEN DEPRECATED\b/;
    print "ok 3\n";
    $@ = '';
}

print "not " unless $a eq $b;
print "ok 4\n";

$b = I18N::Collate->new("bar");
print "not " if $@ =~ /\bHAS BEEN DEPRECATED\b/;
print "ok 5\n";

print "not " if $a eq $b;
print "ok 6\n";

print "not " if $a lt $b == $a gt $b;
print "ok 7\n";

