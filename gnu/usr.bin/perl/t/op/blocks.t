#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
}

plan tests => 22;

my @expect = qw(
b1
b2
b3
b4
b6-c
b7
u6
u5-c
u1
c3
c2-c
c1
i1
i2
b5
u2
u3
u4
b6-r
u5-r
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
"a" =~ /(?{UNITCHECK {print ":u5-c"};
	   CHECK {print ":c2-c"};
	   BEGIN {print ":b6-c"}})/x;
{
    use re 'eval';
    my $runtime = q{
    (?{UNITCHECK {print ":u5-r"};
	       CHECK {print ":c2-r"};
	       BEGIN {print ":b6-r"}})/
    };
    "a" =~ /$runtime/x;
}
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

fresh_perl_is(<<'SCRIPT70614', "still here",{switches => [''], stdin => '', stderr => 1 },'eval-UNITCHECK-eval (bug 70614)');
eval "UNITCHECK { eval 0 }"; print "still here";
SCRIPT70614

# [perl #78634] Make sure block names can be used as constants.
use constant INIT => 5;
::is INIT, 5, 'constant named after a special block';

# [perl #108794] context
fresh_perl_is(<<'SCRIPT3', <<expEct,{stderr => 1 },'context');
sub context {
    print qw[void scalar list][wantarray + defined wantarray], "\n"
}
BEGIN     {context}
UNITCHECK {context}
CHECK     {context}
INIT      {context}
END       {context}
SCRIPT3
void
void
void
void
void
expEct

fresh_perl_is('END { print "ok\n" } INIT { bless {} and exit }', "ok\n",
	       {}, 'null PL_curcop in newGP');

# [perl #2754] exit(0) didn't exit from inside a UNITCHECK or CHECK block

my $testblocks =
    join(" ",
        "BEGIN { \$| = 1; }",
        (map { "@{[uc($_)]} { print \"$_\\n\"; }" }
            qw(begin unitcheck check init end)),
        "print \"main\\n\";"
    );

fresh_perl_is(
    $testblocks,
    "begin\nunitcheck\ncheck\ninit\nmain\nend",
    {},
    'blocks execute in right order'
);

SKIP: {
    skip "VMS doesn't have the perl #2754 bug", 3 if $^O eq 'VMS';
    fresh_perl_is(
        "$testblocks BEGIN { exit 0; }",
        "begin\nunitcheck\ncheck\ninit\nend",
        {},
        "BEGIN{exit 0} doesn't exit yet"
    );

    fresh_perl_is(
        "$testblocks UNITCHECK { exit 0; }",
        "begin\nunitcheck\ncheck\ninit\nmain\nend",
        {},
        "UNITCHECK{exit 0} doesn't exit yet"
    );

    fresh_perl_is(
        "$testblocks CHECK { exit 0; }",
        "begin\nunitcheck\ncheck\ninit\nmain\nend",
        {},
        "CHECK{exit 0} doesn't exit yet"
    );
}


SKIP: {
    if ($^O =~ /^(MSWin32|NetWare|os2)$/) {
        skip "non_UNIX plafforms and PERL_EXIT_DESTRUCT_END (RT #132863)", 6;
    }

    fresh_perl_is(
        "$testblocks BEGIN { exit 1; }",
        "begin\nunitcheck\ncheck\nend",
        {},
        "BEGIN{exit 1} should exit"
    );

    fresh_perl_like(
        "$testblocks BEGIN { die; }",
        qr/\Abegin\nDied[^\n]*\.\nBEGIN failed[^\n]*\.\nunitcheck\ncheck\nend\z/,
        {},
        "BEGIN{die} should exit"
    );

    fresh_perl_is(
        "$testblocks UNITCHECK { exit 1; }",
        "begin\nunitcheck\ncheck\nend",
        {},
        "UNITCHECK{exit 1} should exit"
    );

    fresh_perl_like(
        "$testblocks UNITCHECK { die; }",
        qr/\Abegin\nDied[^\n]*\.\nUNITCHECK failed[^\n]*\.\nunitcheck\ncheck\nend\z/,
        {},
        "UNITCHECK{die} should exit"
    );


    fresh_perl_is(
        "$testblocks CHECK { exit 1; }",
        "begin\nunitcheck\ncheck\nend",
        {},
        "CHECK{exit 1} should exit"
    );

    fresh_perl_like(
        "$testblocks CHECK { die; }",
        qr/\Abegin\nunitcheck\nDied[^\n]*\.\nCHECK failed[^\n]*\.\ncheck\nend\z/,
        {},
        "CHECK{die} should exit"
    );
}

fresh_perl_is(
    "$testblocks INIT { exit 0; }",
    "begin\nunitcheck\ncheck\ninit\nend",
    {},
    "INIT{exit 0} should exit"
);

fresh_perl_is(
    "$testblocks INIT { exit 1; }",
    "begin\nunitcheck\ncheck\ninit\nend",
    {},
    "INIT{exit 1} should exit"
);

fresh_perl_like(
    "$testblocks INIT { die; }",
    qr/\Abegin\nunitcheck\ncheck\ninit\nDied[^\n]*\.\nINIT failed[^\n]*\.\nend\z/,
    {},
    "INIT{die} should exit"
);

TODO: {
    local $TODO = 'RT #2917: INIT{} in eval is wrongly considered too late';
    fresh_perl_is('eval "INIT { print qq(in init); };";', 'in init', {}, 'RT #2917: No constraint on how late INIT blocks can run');
}

fresh_perl_is('eval "BEGIN {goto end}"; end:', '', {}, 'RT #113934: goto out of BEGIN causes assertion failure');

