# For testing Test::Simple;
package Test::Simple::Catch;

use Symbol;
my($out_fh, $err_fh) = (gensym, gensym);
my $out = tie *$out_fh, __PACKAGE__;
my $err = tie *$err_fh, __PACKAGE__;

use Test::Builder;
my $t = Test::Builder->new;
$t->output($out_fh);
$t->failure_output($err_fh);
$t->todo_output($err_fh);

sub caught { return($out, $err) }

sub PRINT  {
    my $self = shift;
    $$self .= join '', @_;
}

sub TIEHANDLE {
    my $class = shift;
    my $self = '';
    return bless \$self, $class;
}
sub READ {}
sub READLINE {}
sub GETC {}
sub FILENO {}

1;
