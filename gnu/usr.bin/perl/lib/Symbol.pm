package Symbol;

=head1 NAME

Symbol - manipulate Perl symbols and their names

=head1 SYNOPSIS

    use Symbol;

    $sym = gensym;
    open($sym, "filename");
    $_ = <$sym>;
    # etc.

    ungensym $sym;      # no effect

    print qualify("x"), "\n";              # "Test::x"
    print qualify("x", "FOO"), "\n"        # "FOO::x"
    print qualify("BAR::x"), "\n";         # "BAR::x"
    print qualify("BAR::x", "FOO"), "\n";  # "BAR::x"
    print qualify("STDOUT", "FOO"), "\n";  # "main::STDOUT" (global)
    print qualify(\*x), "\n";              # returns \*x
    print qualify(\*x, "FOO"), "\n";       # returns \*x

=head1 DESCRIPTION

C<Symbol::gensym> creates an anonymous glob and returns a reference
to it.  Such a glob reference can be used as a file or directory
handle.

For backward compatibility with older implementations that didn't
support anonymous globs, C<Symbol::ungensym> is also provided.
But it doesn't do anything.

C<Symbol::qualify> turns unqualified symbol names into qualified
variable names (e.g. "myvar" -> "MyPackage::myvar").  If it is given a
second parameter, C<qualify> uses it as the default package;
otherwise, it uses the package of its caller.  Regardless, global
variable names (e.g. "STDOUT", "ENV", "SIG") are always qualfied with
"main::".

Qualification applies only to symbol names (strings).  References are
left unchanged under the assumption that they are glob references,
which are qualified by their nature.

=cut

BEGIN { require 5.002; }

require Exporter;
@ISA = qw(Exporter);

@EXPORT = qw(gensym ungensym qualify);

my $genpkg = "Symbol::";
my $genseq = 0;

my %global;
while (<DATA>) {
    chomp;
    $global{$_} = 1;
}
close DATA;

sub gensym () {
    my $name = "GEN" . $genseq++;
    local *{$genpkg . $name};
    \delete ${$genpkg}{$name};
}

sub ungensym ($) {}

sub qualify ($;$) {
    my ($name) = @_;
    if (!ref($name) && index($name, '::') == -1 && index($name, "'") == -1) {
	my $pkg;
	# Global names: special character, "^x", or other. 
	if ($name =~ /^([^a-z])|(\^[a-z])$/i || $global{$name}) {
	    $pkg = "main";
	}
	else {
	    $pkg = (@_ > 1) ? $_[1] : caller;
	}
	$name = $pkg . "::" . $name;
    }
    $name;
}

1;

__DATA__
ARGV
ARGVOUT
ENV
INC
SIG
STDERR
STDIN
STDOUT
