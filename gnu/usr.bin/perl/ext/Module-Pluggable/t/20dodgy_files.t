#!perl -w

BEGIN {
    if ($^O eq 'VMS' || $^O eq 'VOS') {
        print "1..0 # Skip: can't handle misspelled plugin names\n";
        exit;
    }
}

use strict;
use FindBin;
use Test::More;
use lib (($FindBin::Bin."/lib")=~/^(.*)$/);
use File::Spec::Functions qw(catfile);


my ($dodgy_file) = (catfile($FindBin::Bin, "lib", "OddTest", "Plugin", "-Dodgy.pm")=~/^(.*)$/);
unless (-f $dodgy_file) {
        plan skip_all => "Can't handle misspelled plugin names\n";
} else {
        plan tests => 5;
}


my $foo;
ok($foo = OddTest->new());

my @plugins;
my @expected = ('OddTest::Plugin::-Dodgy', 'OddTest::Plugin::Foo');
ok(@plugins = sort $foo->plugins);
is_deeply(\@plugins, \@expected, "is deeply");

my @odd_plugins;
my @odd_expected = qw(OddTest::Plugin::Foo);
ok(@odd_plugins = sort $foo->odd_plugins);
is_deeply(\@odd_plugins, \@odd_expected, "is deeply");


package OddTest::Pluggable;

use Data::Dumper;
use base qw(Module::Pluggable::Object);


sub find_files { 
    my $self = shift;
    my @files = $self->SUPER::find_files(@_);
    return grep { !/(^|\/)-/ } $self->SUPER::find_files(@_) ;
}

package OddTest;

use strict;
use Module::Pluggable;


sub new {
    my $class = shift;
    return bless {}, $class;

}

sub odd_plugins {
    my $self = shift;
    my %opts;
    my ($pkg, $file) = caller; 
    # the default name for the method is 'plugins'
    my $sub          = $opts{'sub_name'}  || 'plugins';
    # get our package 
    my ($package)    = $opts{'package'} || "OddTest";
    $opts{filename}  = $file;
    $opts{package}   = $package;



    my $op   = OddTest::Pluggable->new( package => ref($self) );
    return $op->plugins(@_);
    

}


1;

