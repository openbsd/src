#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    
}
print "1..1\n";

# there is an equivelent test in t/re/pat.t which does NOT fail
# its not clear why it doesnt fail, so this todo gets its own test
# file until we can work it out.

my $x; 
($x='abc')=~/(abc)/g; 
$x='123'; 

print "not " if $1 ne 'abc';
print "ok 1 # TODO safe match vars make /g slow\n";
