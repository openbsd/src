#!perl -w

use Test::More;
use FindBin;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use Module::Pluggable::Object;
use File::Spec::Functions qw(catfile);

my ($dodgy_file) = (catfile($FindBin::Bin,"lib", "EditorJunk", "Plugin", "#Bar.pm#")=~/^(.*)$/);
unless (-f $dodgy_file) {
        plan skip_all => "Can't handle plugin names with octothorpes\n";
} else {
        plan tests => 4;
}



my $foo;
ok($foo = EditorJunk->new());

my @plugins;
my @expected = qw(EditorJunk::Plugin::Bar EditorJunk::Plugin::Foo);
ok(@plugins = sort $foo->plugins);

is_deeply(\@plugins, \@expected, "is deeply");


my $mpo = Module::Pluggable::Object->new(
    package             => 'EditorJunk',
    filename            => __FILE__,
    include_editor_junk => 1,
);

@expected = ('EditorJunk::Plugin::.#Bar', 'EditorJunk::Plugin::Bar', 'EditorJunk::Plugin::Foo');
@plugins = sort $mpo->plugins();
is_deeply(\@plugins, \@expected, "is deeply");



package EditorJunk;

use strict;
use Module::Pluggable;


sub new {
    my $class = shift;
    return bless {}, $class;

}
1;


