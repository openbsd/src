#!./perl -w

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
}

#
# A couple of simple classes to use as struct elements.
#
package aClass;
sub new { bless {}, shift }
sub meth { 42 }

package RecClass;
sub new { bless {}, shift }

#
# The first of our Class::Struct based objects.
#
package MyObj;
use Class::Struct;
use Class::Struct 'struct'; # test out both forms
use Class::Struct SomeClass => { SomeElem => '$' };

struct( s => '$', a => '@', h => '%', c => 'aClass' );

#
# The second Class::Struct objects:
# test the 'compile-time without package name' feature.
#
package MyOther;
use Class::Struct s => '$', a => '@', h => '%', c => 'aClass';

#
# back to main...
#
package main;

use Test::More tests => 24;

my $obj = MyObj->new;
isa_ok $obj, 'MyObj';

$obj->s('foo');
is $obj->s(), 'foo';

isa_ok $obj->a, 'ARRAY';
$obj->a(2, 'secundus');
is $obj->a(2), 'secundus';

$obj->a([4,5,6]);
is $obj->a(1), 5;

isa_ok $obj->h, 'HASH';
$obj->h('x', 10);
is $obj->h('x'), 10;

$obj->h({h=>7,r=>8,f=>9});
is $obj->h('r'), 8;

is $obj->c, undef;

$obj = MyObj->new( c => aClass->new );
isa_ok $obj->c, 'aClass';
is $obj->c->meth(), 42;


$obj = MyOther->new;
isa_ok $obj, 'MyOther';

$obj->s('foo');
is $obj->s(), 'foo';

isa_ok $obj->a, 'ARRAY';
$obj->a(2, 'secundus');
is $obj->a(2), 'secundus';

$obj->a([4,5,6]);
is $obj->a(1), 5;

isa_ok $obj->h, 'HASH';
$obj->h('x', 10);
is $obj->h('x'), 10;

$obj->h({h=>7,r=>8,f=>9});
is $obj->h('r'), 8;

is $obj->c, undef;

$obj = MyOther->new( c => aClass->new );
isa_ok $obj->c, 'aClass';
is $obj->c->meth(), 42;



my $obk = SomeClass->new();
$obk->SomeElem(123);
is $obk->SomeElem(), 123;

my $recobj = RecClass->new();
isa_ok $recobj, 'RecClass';

