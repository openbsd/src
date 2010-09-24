#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;

plan 5;

my $err = "Unimplemented at $0 line " . ( __LINE__ + 2 ) . ".\n";

eval { ... };

is $@, $err;


#
# Regression tests, making sure ... is still parsable as an operator.
#
my @lines = split /\n/ => <<'--';

# Check simple range operator.
my @arr = 'A' ... 'D';

# Range operator with print.
print 'D' ... 'A';

# Without quotes, 'D' could be a file handle.
print  D  ...  A ;

# Another possible interaction with a file handle.
print ${\"D"}  ...  A ;
--

foreach my $line (@lines) {
    next if $line =~ /^\s*#/ || $line !~ /\S/;
    my $mess = qq {Parsing '...' in "$line" as a range operator};
    eval qq {
       {local *STDOUT; no strict "subs"; $line;}
        pass \$mess;
        1;
    } or do {
        my $err = $@;
        $err =~ s/\n//g;
        fail "$mess ($err)";
    }
}
