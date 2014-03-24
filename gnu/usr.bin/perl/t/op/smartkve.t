#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}
use strict;
use warnings;
no warnings 'deprecated';
use vars qw($data $array $values $hash $errpat);

plan 'no_plan';

sub j { join(":",@_) }

# NOTE
#
# Hash insertion is currently unstable, in that
# %hash= %otherhash will not necessarily result in
# the same internal ordering of the data in the hash.
# For instance when keys collide the copy may not
# match the inserted order. So we declare one hash
# and then make all our copies from that, which should
# mean all the copies have the same internal structure.
#
# And these days, even if all that weren't true, we now
# per-hash randomize keys/values. So, we cant expect two
# hashes with the same internal structure to return the
# same thing at all. All we *can* expect is that keys()
# and values() use the same ordering.
our %base_hash;

BEGIN { # in BEGIN for "use constant ..." later
  # values match keys here so we can easily check that keys(%hash) == values(%hash)
  %base_hash= (  pi => 'pi', e => 'e', i => 'i' );
  $array = [ qw(pi e i) ];
  $values = [ qw(pi e i) ];
  $hash  = { %base_hash } ;
  $data = {
    hash => { %base_hash },
    array => [ @$array ],
  };
}

package Foo;
sub new {
  my $self = {
    hash => { %base_hash },
    array => [@{$main::array}]
  };
  bless $self, shift;
}
sub hash { no overloading; $_[0]->{hash} };
sub array { no overloading; $_[0]->{array} };

package Foo::Overload::Array;
sub new { return bless [ qw/foo bar/ ], shift }
use overload '@{}' => sub { $main::array }, fallback => 1;

package Foo::Overload::Hash;
sub new { return bless { qw/foo bar/ }, shift }
use overload '%{}' => sub { $main::hash }, fallback => 1;

package Foo::Overload::Both;
sub new { return bless { qw/foo bar/ }, shift }
use overload  '%{}' => sub { $main::hash },
              '@{}' => sub { $main::array }, fallback => 1;

package Foo::Overload::HashOnArray;
sub new { return bless [ qw/foo bar/ ], shift }
use overload '%{}' => sub { $main::hash }, fallback => 1;

package Foo::Overload::ArrayOnHash;
sub new { return bless { qw/foo bar/ }, shift }
use overload '@{}' => sub { $main::array }, fallback => 1;

package main;

use constant CONST_HASH => { %base_hash };
use constant CONST_ARRAY => [ @$array ];

my %a_hash = %base_hash;
my @an_array = @$array;
sub hash_sub { return \%a_hash; }
sub array_sub { return \@an_array; }

my $obj = Foo->new;

my ($empty, $h_expect, $a_expect, @tmp, @tmp2, $k, $v);

# Keys -- void

keys $hash;             pass('Void: keys $hash;');
keys $data->{hash};     pass('Void: keys $data->{hash};');
keys CONST_HASH;        pass('Void: keys CONST_HASH;');
keys CONST_HASH();      pass('Void: keys CONST_HASH();');
keys hash_sub();        pass('Void: keys hash_sub();');
keys hash_sub;          pass('Void: keys hash_sub;');
keys $obj->hash;        pass('Void: keys $obj->hash;');
keys $array;            pass('Void: keys $array;');
keys $data->{array};    pass('Void: keys $data->{array};');
keys CONST_ARRAY;       pass('Void: keys CONST_ARRAY;');
keys CONST_ARRAY();     pass('Void: keys CONST_ARRAY();');
keys array_sub;         pass('Void: keys array_sub;');
keys array_sub();       pass('Void: keys array_sub();');
keys $obj->array;       pass('Void: keys $obj->array;');

# Keys -- scalar

is(keys $hash           ,3, 'Scalar: keys $hash');
is(keys $data->{hash}   ,3, 'Scalar: keys $data->{hash}');
is(keys CONST_HASH      ,3, 'Scalar: keys CONST_HASH');
is(keys CONST_HASH()    ,3, 'Scalar: keys CONST_HASH()');
is(keys hash_sub        ,3, 'Scalar: keys hash_sub');
is(keys hash_sub()      ,3, 'Scalar: keys hash_sub()');
is(keys $obj->hash      ,3, 'Scalar: keys $obj->hash');
is(keys $array          ,3, 'Scalar: keys $array');
is(keys $data->{array}  ,3, 'Scalar: keys $data->{array}');
is(keys CONST_ARRAY     ,3, 'Scalar: keys CONST_ARRAY');
is(keys CONST_ARRAY()   ,3, 'Scalar: keys CONST_ARRAY()');
is(keys array_sub       ,3, 'Scalar: keys array_sub');
is(keys array_sub()     ,3, 'Scalar: keys array_sub()');
is(keys $obj->array     ,3, 'Scalar: keys $obj->array');

# Keys -- list

$h_expect = j(sort keys %base_hash);
$a_expect = j(keys @$array);

is(j(sort keys $hash)           ,$h_expect, 'List: sort keys $hash');
is(j(sort keys $data->{hash})   ,$h_expect, 'List: sort keys $data->{hash}');
is(j(sort keys CONST_HASH)      ,$h_expect, 'List: sort keys CONST_HASH');
is(j(sort keys CONST_HASH())    ,$h_expect, 'List: sort keys CONST_HASH()');
is(j(sort keys hash_sub)        ,$h_expect, 'List: sort keys hash_sub');
is(j(sort keys hash_sub())      ,$h_expect, 'List: sort keys hash_sub()');
is(j(sort keys $obj->hash)      ,$h_expect, 'List: sort keys $obj->hash');

is(j(keys $hash)                ,j(values $hash),           'List: keys $hash == values $hash');
is(j(keys $data->{hash})        ,j(values $data->{hash}),   'List: keys $data->{hash} == values $data->{hash}');
is(j(keys CONST_HASH)           ,j(values CONST_HASH),      'List: keys CONST_HASH == values CONST_HASH');
is(j(keys CONST_HASH())         ,j(values CONST_HASH()),    'List: keys CONST_HASH() == values CONST_HASH()');
is(j(keys hash_sub)             ,j(values hash_sub),        'List: keys hash_sub == values hash_sub');
is(j(keys hash_sub())           ,j(values hash_sub()),      'List: keys hash_sub() == values hash_sub()');
is(j(keys $obj->hash)           ,j(values $obj->hash),      'List: keys $obj->hash == values obj->hash');

is(j(keys $array)               ,$a_expect, 'List: keys $array');
is(j(keys $data->{array})       ,$a_expect, 'List: keys $data->{array}');
is(j(keys CONST_ARRAY)          ,$a_expect, 'List: keys CONST_ARRAY');
is(j(keys CONST_ARRAY())        ,$a_expect, 'List: keys CONST_ARRAY()');
is(j(keys array_sub)            ,$a_expect, 'List: keys array_sub');
is(j(keys array_sub())          ,$a_expect, 'List: keys array_sub()');
is(j(keys $obj->array)          ,$a_expect, 'List: keys $obj->array');

# Keys -- vivification
undef $empty;
eval { keys $empty->{hash} };
ok(defined $empty,
  'Vivify: $empty (after keys $empty->{hash}) is HASHREF');
ok(!defined $empty->{hash}      ,   'Vivify: $empty->{hash} is undef');

# Keys -- lvalue
$_{foo} = "bar";
keys \%_ = 65;
is scalar %_, '1/128', 'keys $hashref as lvalue';
eval 'keys \@_ = 65';
like $@, qr/Can't modify keys on reference in scalar assignment/,
  'keys $arrayref as lvalue dies';

# Keys -- errors
$errpat = qr/
 (?-x:Type of argument to keys on reference must be unblessed hashref or)
 (?-x: arrayref)
/x;

eval "keys undef";
ok($@ =~ $errpat,
  'Errors: keys undef throws error'
);

undef $empty;
eval q"keys $empty";
ok($@ =~ $errpat,
  'Errors: keys $undef throws error'
);

is($empty, undef, 'keys $undef does not vivify $undef');

eval "keys 3";
ok($@ =~ qr/Type of arg 1 to keys must be hash/,
  'Errors: keys CONSTANT throws error'
);

eval "keys qr/foo/";
ok($@ =~ $errpat,
  'Errors: keys qr/foo/ throws error'
);

eval q"keys $hash qw/fo bar/";
ok($@ =~ qr/syntax error/,
  'Errors: keys $hash, @stuff throws error'
) or print "# Got: $@";

# Values -- void

values $hash;             pass('Void: values $hash;');
values $data->{hash};     pass('Void: values $data->{hash};');
values CONST_HASH;        pass('Void: values CONST_HASH;');
values CONST_HASH();      pass('Void: values CONST_HASH();');
values hash_sub();        pass('Void: values hash_sub();');
values hash_sub;          pass('Void: values hash_sub;');
values $obj->hash;        pass('Void: values $obj->hash;');
values $array;            pass('Void: values $array;');
values $data->{array};    pass('Void: values $data->{array};');
values CONST_ARRAY;       pass('Void: values CONST_ARRAY;');
values CONST_ARRAY();     pass('Void: values CONST_ARRAY();');
values array_sub;         pass('Void: values array_sub;');
values array_sub();       pass('Void: values array_sub();');
values $obj->array;       pass('Void: values $obj->array;');

# Values -- scalar

is(values $hash           ,3, 'Scalar: values $hash');
is(values $data->{hash}   ,3, 'Scalar: values $data->{hash}');
is(values CONST_HASH      ,3, 'Scalar: values CONST_HASH');
is(values CONST_HASH()    ,3, 'Scalar: values CONST_HASH()');
is(values hash_sub        ,3, 'Scalar: values hash_sub');
is(values hash_sub()      ,3, 'Scalar: values hash_sub()');
is(values $obj->hash      ,3, 'Scalar: values $obj->hash');
is(values $array          ,3, 'Scalar: values $array');
is(values $data->{array}  ,3, 'Scalar: values $data->{array}');
is(values CONST_ARRAY     ,3, 'Scalar: values CONST_ARRAY');
is(values CONST_ARRAY()   ,3, 'Scalar: values CONST_ARRAY()');
is(values array_sub       ,3, 'Scalar: values array_sub');
is(values array_sub()     ,3, 'Scalar: values array_sub()');
is(values $obj->array     ,3, 'Scalar: values $obj->array');

# Values -- list

$h_expect = j(sort values %base_hash);
$a_expect = j(values @$array);

is(j(sort values $hash)                ,$h_expect, 'List: sort values $hash');
is(j(sort values $data->{hash})        ,$h_expect, 'List: sort values $data->{hash}');
is(j(sort values CONST_HASH)           ,$h_expect, 'List: sort values CONST_HASH');
is(j(sort values CONST_HASH())         ,$h_expect, 'List: sort values CONST_HASH()');
is(j(sort values hash_sub)             ,$h_expect, 'List: sort values hash_sub');
is(j(sort values hash_sub())           ,$h_expect, 'List: sort values hash_sub()');
is(j(sort values $obj->hash)           ,$h_expect, 'List: sort values $obj->hash');

is(j(values $hash)                ,j(keys $hash),           'List: values $hash == keys $hash');
is(j(values $data->{hash})        ,j(keys $data->{hash}),   'List: values $data->{hash} == keys $data->{hash}');
is(j(values CONST_HASH)           ,j(keys CONST_HASH),      'List: values CONST_HASH == keys CONST_HASH');
is(j(values CONST_HASH())         ,j(keys CONST_HASH()),    'List: values CONST_HASH() == keys CONST_HASH()');
is(j(values hash_sub)             ,j(keys hash_sub),        'List: values hash_sub == keys hash_sub');
is(j(values hash_sub())           ,j(keys hash_sub()),      'List: values hash_sub() == keys hash_sub()');
is(j(values $obj->hash)           ,j(keys $obj->hash),      'List: values $obj->hash == keys $obj->hash');

is(j(values $array)               ,$a_expect, 'List: values $array');
is(j(values $data->{array})       ,$a_expect, 'List: values $data->{array}');
is(j(values CONST_ARRAY)          ,$a_expect, 'List: values CONST_ARRAY');
is(j(values CONST_ARRAY())        ,$a_expect, 'List: values CONST_ARRAY()');
is(j(values array_sub)            ,$a_expect, 'List: values array_sub');
is(j(values array_sub())          ,$a_expect, 'List: values array_sub()');
is(j(values $obj->array)          ,$a_expect, 'List: values $obj->array');

# Values -- vivification
undef $empty;
eval { values $empty->{hash} };
ok(defined $empty,
  'Vivify: $empty (after values $empty->{hash}) is HASHREF');
ok(!defined $empty->{hash}      ,   'Vivify: $empty->{hash} is undef');

# Values -- errors
$errpat = qr/
 (?-x:Type of argument to values on reference must be unblessed hashref or)
 (?-x: arrayref)
/x;

eval "values undef";
ok($@ =~ $errpat,
  'Errors: values undef throws error'
);

undef $empty;
eval q"values $empty";
ok($@ =~ $errpat,
  'Errors: values $undef throws error'
);

is($empty, undef, 'values $undef does not vivify $undef');

eval "values 3";
ok($@ =~ qr/Type of arg 1 to values must be hash/,
  'Errors: values CONSTANT throws error'
);

eval "values qr/foo/";
ok($@ =~ $errpat,
  'Errors: values qr/foo/ throws error'
);

eval q"values $hash qw/fo bar/";
ok($@ =~ qr/syntax error/,
  'Errors: values $hash, @stuff throws error'
) or print "# Got: $@";

# Each -- void

each $hash;             pass('Void: each $hash');
each $data->{hash};     pass('Void: each $data->{hash}');
each CONST_HASH;        pass('Void: each CONST_HASH');
each CONST_HASH();      pass('Void: each CONST_HASH()');
each hash_sub();        pass('Void: each hash_sub()');
each hash_sub;          pass('Void: each hash_sub');
each $obj->hash;        pass('Void: each $obj->hash');
each $array;            pass('Void: each $array');
each $data->{array};    pass('Void: each $data->{array}');
each CONST_ARRAY;       pass('Void: each CONST_ARRAY');
each CONST_ARRAY();     pass('Void: each CONST_ARRAY()');
each array_sub;         pass('Void: each array_sub');
each array_sub();       pass('Void: each array_sub()');
each $obj->array;       pass('Void: each $obj->array');

# Reset iterators

keys $hash;
keys $data->{hash};
keys CONST_HASH;
keys CONST_HASH();
keys hash_sub();
keys hash_sub;
keys $obj->hash;
keys $array;
keys $data->{array};
keys CONST_ARRAY;
keys CONST_ARRAY();
keys array_sub;
keys array_sub();
keys $obj->array;

# Each -- scalar

@tmp=(); while(defined( $k = each $hash)) {push @tmp,$k}; is(j(@tmp),j(keys $hash), 'Scalar: each $hash');
@tmp=(); while(defined( $k = each $data->{hash})){push @tmp,$k}; is(j(@tmp),j(keys $data->{hash}), 'Scalar: each $data->{hash}');
@tmp=(); while(defined( $k = each CONST_HASH)){push @tmp,$k}; is(j(@tmp),j(keys CONST_HASH), 'Scalar: each CONST_HASH');
@tmp=(); while(defined( $k = each CONST_HASH())){push @tmp,$k}; is(j(@tmp),j(keys CONST_HASH()), 'Scalar: each CONST_HASH()');
@tmp=(); while(defined( $k = each hash_sub())){push @tmp,$k}; is(j(@tmp),j(keys hash_sub()), 'Scalar: each hash_sub()');
@tmp=(); while(defined( $k = each hash_sub)){push @tmp,$k}; is(j(@tmp),j(keys hash_sub), 'Scalar: each hash_sub');
@tmp=(); while(defined( $k = each $obj->hash)){push @tmp,$k}; is(j(@tmp),j(keys $obj->hash), 'Scalar: each $obj->hash');
@tmp=(); while(defined( $k = each $array)){push @tmp,$k}; is(j(@tmp),j(keys $array), 'Scalar: each $array');
@tmp=(); while(defined( $k = each $data->{array})){push @tmp,$k}; is(j(@tmp),j(keys $data->{array}), 'Scalar: each $data->{array}');
@tmp=(); while(defined( $k = each CONST_ARRAY)){push @tmp,$k}; is(j(@tmp),j(keys CONST_ARRAY), 'Scalar: each CONST_ARRAY');
@tmp=(); while(defined( $k = each CONST_ARRAY())){push @tmp,$k}; is(j(@tmp),j(keys CONST_ARRAY()), 'Scalar: each CONST_ARRAY()');
@tmp=(); while(defined( $k = each array_sub)){push @tmp,$k}; is(j(@tmp),j(keys array_sub), 'Scalar: each array_sub');
@tmp=(); while(defined( $k = each array_sub())){push @tmp,$k}; is(j(@tmp),j(keys array_sub()), 'Scalar: each array_sub()');
@tmp=(); while(defined( $k = each $obj->array)){push @tmp,$k}; is(j(@tmp),j(keys $obj->array), 'Scalar: each $obj->array');

# Each -- list

@tmp=@tmp2=(); while(($k,$v) = each $hash) {push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys $hash, values $hash), 'List: each $hash');
@tmp=@tmp2=(); while(($k,$v) = each $data->{hash}){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys $data->{hash}, values $data->{hash}), 'List: each $data->{hash}');
@tmp=@tmp2=(); while(($k,$v) = each CONST_HASH){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys CONST_HASH, values CONST_HASH), 'List: each CONST_HASH');
@tmp=@tmp2=(); while(($k,$v) = each CONST_HASH()){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys CONST_HASH(), values CONST_HASH()), 'List: each CONST_HASH()');
@tmp=@tmp2=(); while(($k,$v) = each hash_sub()){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys hash_sub(), values hash_sub()), 'List: each hash_sub()');
@tmp=@tmp2=(); while(($k,$v) = each hash_sub){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys hash_sub, values hash_sub), 'List: each hash_sub');
@tmp=@tmp2=(); while(($k,$v) = each $obj->hash){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys $obj->hash, values $obj->hash), 'List: each $obj->hash');
@tmp=@tmp2=(); while(($k,$v) = each $array){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys $array, values $array), 'List: each $array');
@tmp=@tmp2=(); while(($k,$v) = each $data->{array}){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys $data->{array}, values $data->{array}), 'List: each $data->{array}');
@tmp=@tmp2=(); while(($k,$v) = each CONST_ARRAY){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys CONST_ARRAY, values CONST_ARRAY), 'List: each CONST_ARRAY');
@tmp=@tmp2=(); while(($k,$v) = each CONST_ARRAY()){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys CONST_ARRAY(), values CONST_ARRAY()), 'List: each CONST_ARRAY()');
@tmp=@tmp2=(); while(($k,$v) = each array_sub){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys array_sub, values array_sub), 'List: each array_sub');
@tmp=@tmp2=(); while(($k,$v) = each array_sub()){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys array_sub(), values array_sub()), 'List: each array_sub()');
@tmp=@tmp2=(); while(($k,$v) = each $obj->array){push @tmp,$k; push @tmp2,$v}; is(j(@tmp,@tmp2),j(keys $obj->array, values $obj->array), 'List: each $obj->array');

# Each -- vivification
undef $empty;
eval { each $empty->{hash} };
ok(defined $empty,
  'Vivify: $empty (after each $empty->{hash}) is HASHREF');
ok(!defined $empty->{hash}      ,   'Vivify: $empty->{hash} is undef');

# Each -- errors
$errpat = qr/
 (?-x:Type of argument to each on reference must be unblessed hashref or)
 (?-x: arrayref)
/x;

eval "each undef";
ok($@ =~ $errpat,
  'Errors: each undef throws error'
);

undef $empty;
eval q"each $empty";
ok($@ =~ $errpat,
  'Errors: each $undef throws error'
);

is($empty, undef, 'each $undef does not vivify $undef');

eval "each 3";
ok($@ =~ qr/Type of arg 1 to each must be hash/,
  'Errors: each CONSTANT throws error'
);

eval "each qr/foo/";
ok($@ =~ $errpat,
  'Errors: each qr/foo/ throws error'
);

eval q"each $hash qw/foo bar/";
ok($@ =~ qr/syntax error/,
  'Errors: each $hash, @stuff throws error'
) or print "# Got: $@";

# Overloaded objects
my $over_a = Foo::Overload::Array->new;
my $over_h = Foo::Overload::Hash->new;
my $over_b = Foo::Overload::Both->new;
my $over_h_a = Foo::Overload::HashOnArray->new;
my $over_a_h = Foo::Overload::ArrayOnHash->new;

{
  my $warn = '';
  local $SIG{__WARN__} = sub { $warn = shift };

  $errpat = qr/
   (?-x:Type of argument to keys on reference must be unblessed hashref or)
   (?-x: arrayref)
  /x;

  eval { keys $over_a };
  like($@, $errpat, "Overload: array dereference");
  is($warn, '', "no warning issued"); $warn = '';

  eval { keys $over_h };
  like($@, $errpat, "Overload: hash dereference");
  is($warn, '', "no warning issued"); $warn = '';

  eval { keys $over_b };
  like($@, $errpat, "Overload: ambiguous dereference (both)");
  is($warn, '', "no warning issued"); $warn = '';

  eval { keys $over_h_a };
  like($@, $errpat, "Overload: ambiguous dereference");
  is($warn, '', "no warning issued"); $warn = '';

  eval { keys $over_a_h };
  like($@, $errpat, "Overload: ambiguous dereference");
  is($warn, '', "no warning issued"); $warn = '';
}
