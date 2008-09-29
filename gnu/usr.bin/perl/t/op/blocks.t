#!./perl

BEGIN {
    chdir 't';
    @INC = '../lib';
    require './test.pl';
}

plan tests => 3;

my @expect = qw(
b1
b2
b3
b4
b6
u5
b7
u6
u1
c3
c2
c1
i1
i2
b5
u2
u3
u4
e2
e1
		);
my $expect = ":" . join(":", @expect);

fresh_perl_is(<<'SCRIPT', $expect,{switches => [''], stdin => '', stderr => 1 },'Order of execution of special blocks');
BEGIN {print ":b1"}
END {print ":e1"}
BEGIN {print ":b2"}
{
    BEGIN {BEGIN {print ":b3"}; print ":b4"}
}
CHECK {print ":c1"}
INIT {print ":i1"}
UNITCHECK {print ":u1"}
eval 'BEGIN {print ":b5"}';
eval 'UNITCHECK {print ":u2"}';
eval 'UNITCHECK {print ":u3"; UNITCHECK {print ":u4"}}';
"a" =~ /(?{UNITCHECK {print ":u5"};
	   CHECK {print ":c2"};
	   BEGIN {print ":b6"}})/x;
eval {BEGIN {print ":b7"}};
eval {UNITCHECK {print ":u6"}};
eval {INIT {print ":i2"}};
eval {CHECK {print ":c3"}};
END {print ":e2"}
SCRIPT

@expect =(
# BEGIN
qw( main bar myfoo foo ),
# UNITCHECK
qw( foo myfoo bar main ),
# CHECK
qw( foo myfoo bar main ),
# INIT
qw( main bar myfoo foo ),
# END
qw(foo myfoo bar main  ));

$expect = ":" . join(":", @expect);
fresh_perl_is(<<'SCRIPT2', $expect,{switches => [''], stdin => '', stderr => 1 },'blocks interact with packages/scopes');
BEGIN {$f = 'main'; print ":$f"}
UNITCHECK {print ":$f"}
CHECK {print ":$f"}
INIT {print ":$f"}
END {print ":$f"}
package bar;
BEGIN {$f = 'bar';print ":$f"}
UNITCHECK {print ":$f"}
CHECK {print ":$f"}
INIT {print ":$f"}
END {print ":$f"}
package foo;
{
    my $f;
    BEGIN {$f = 'myfoo'; print ":$f"}
    UNITCHECK {print ":$f"}
    CHECK {print ":$f"}
    INIT {print ":$f"}
    END {print ":$f"}
}
BEGIN {$f = "foo";print ":$f"}
UNITCHECK {print ":$f"}
CHECK {print ":$f"}
INIT {print ":$f"}
END {print ":$f"}
SCRIPT2

@expect = qw(begin unitcheck check init end);
$expect = ":" . join(":", @expect);
fresh_perl_is(<<'SCRIPT3', $expect,{switches => [''], stdin => '', stderr => 1 },'can name blocks as sub FOO');
sub BEGIN {print ":begin"}
sub UNITCHECK {print ":unitcheck"}
sub CHECK {print ":check"}
sub INIT {print ":init"}
sub END {print ":end"}
SCRIPT3
