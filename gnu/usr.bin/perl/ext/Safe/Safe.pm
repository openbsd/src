package Safe;

use vars qw($VERSION @ISA @EXPORT_OK);

require Exporter;
require DynaLoader;
use Carp;
$VERSION = "1.00";
@ISA = qw(Exporter DynaLoader);
@EXPORT_OK = qw(op_mask ops_to_mask mask_to_ops opcode opname opdesc
		MAXO emptymask fullmask);

=head1 NAME

Safe - Safe extension module for Perl

=head1 DESCRIPTION

The Safe extension module allows the creation of compartments
in which perl code can be evaluated. Each compartment has

=over 8

=item a new namespace

The "root" of the namespace (i.e. "main::") is changed to a
different package and code evaluated in the compartment cannot
refer to variables outside this namespace, even with run-time
glob lookups and other tricks. Code which is compiled outside
the compartment can choose to place variables into (or share
variables with) the compartment's namespace and only that
data will be visible to code evaluated in the compartment.

By default, the only variables shared with compartments are the
"underscore" variables $_ and @_ (and, technically, the much less
frequently used %_, the _ filehandle and so on). This is because
otherwise perl operators which default to $_ will not work and neither
will the assignment of arguments to @_ on subroutine entry.

=item an operator mask

Each compartment has an associated "operator mask". Recall that
perl code is compiled into an internal format before execution.
Evaluating perl code (e.g. via "eval" or "do 'file'") causes
the code to be compiled into an internal format and then,
provided there was no error in the compilation, executed.
Code evaulated in a compartment compiles subject to the
compartment's operator mask. Attempting to evaulate code in a
compartment which contains a masked operator will cause the
compilation to fail with an error. The code will not be executed.

By default, the operator mask for a newly created compartment masks
out all operations which give "access to the system" in some sense.
This includes masking off operators such as I<system>, I<open>,
I<chown>, and I<shmget> but does not mask off operators such as
I<print>, I<sysread> and I<E<lt>HANDLE<gt>>. Those file operators
are allowed since for the code in the compartment to have access
to a filehandle, the code outside the compartment must have explicitly
placed the filehandle variable inside the compartment.

Since it is only at the compilation stage that the operator mask
applies, controlled access to potentially unsafe operations can
be achieved by having a handle to a wrapper subroutine (written
outside the compartment) placed into the compartment. For example,

    $cpt = new Safe;
    sub wrapper {
        # vet arguments and perform potentially unsafe operations
    }
    $cpt->share('&wrapper');

=back

=head2 Operator masks

An operator mask exists at user-level as a string of bytes of length
MAXO, each of which is either 0x00 or 0x01. Here, MAXO is the number
of operators in the current version of perl. The subroutine MAXO()
(available for export by package Safe) returns the number of operators
in the current version of perl. Note that, unlike the beta versions of
the Safe extension, this is a reliable count of the number of
operators in the currently running perl executable. The presence of a
0x01 byte at offset B<n> of the string indicates that operator number
B<n> should be masked (i.e. disallowed).  The Safe extension makes
available routines for converting from operator names to operator
numbers (and I<vice versa>) and for converting from a list of operator
names to the corresponding mask (and I<vice versa>).

=head2 Methods in class Safe

To create a new compartment, use

    $cpt = new Safe;

Optional arguments are (NAMESPACE, MASK), where

=over 8

=item NAMESPACE

is the root namespace to use for the compartment (defaults to
"Safe::Root000000000", auto-incremented for each new compartment); and

=item MASK

is the operator mask to use (defaults to a fairly restrictive set).

=back

The following methods can then be used on the compartment
object returned by the above constructor. The object argument
is implicit in each case.

=over 8

=item root (NAMESPACE)

This is a get-or-set method for the compartment's namespace. With the
NAMESPACE argument present, it sets the root namespace for the
compartment. With no NAMESPACE argument present, it returns the
current root namespace of the compartment.

=item mask (MASK)

This is a get-or-set method for the compartment's operator mask.
With the MASK argument present, it sets the operator mask for the
compartment. With no MASK argument present, it returns the
current operator mask of the compartment.

=item trap (OP, ...)

This sets bits in the compartment's operator mask corresponding
to each operator named in the list of arguments. Each OP can be
either the name of an operation or its number. See opcode.h or
opcode.pl in the main perl distribution for a canonical list of
operator names.

=item untrap (OP, ...)

This resets bits in the compartment's operator mask corresponding
to each operator named in the list of arguments. Each OP can be
either the name of an operation or its number. See opcode.h or
opcode.pl in the main perl distribution for a canonical list of
operator names.

=item share (VARNAME, ...)

This shares the variable(s) in the argument list with the compartment.
Each VARNAME must be the B<name> of a variable with a leading type
identifier included. Examples of legal variable names are '$foo' for
a scalar, '@foo' for an array, '%foo' for a hash, '&foo' for a
subroutine and '*foo' for a glob (i.e. all symbol table entries
associated with "foo", including scalar, array, hash, sub and filehandle).

=item varglob (VARNAME)

This returns a glob for the symbol table entry of VARNAME in the package
of the compartment. VARNAME must be the B<name> of a variable without
any leading type marker. For example,

    $cpt = new Safe 'Root';
    $Root::foo = "Hello world";
    # Equivalent version which doesn't need to know $cpt's package name:
    ${$cpt->varglob('foo')} = "Hello world";


=item reval (STRING)

This evaluates STRING as perl code inside the compartment. The code
can only see the compartment's namespace (as returned by the B<root>
method). Any attempt by code in STRING to use an operator which is
in the compartment's mask will cause an error (at run-time of the
main program but at compile-time for the code in STRING). The error
is of the form "%s trapped by operation mask operation...". If an
operation is trapped in this way, then the code in STRING will not
be executed. If such a trapped operation occurs or any other
compile-time or return error, then $@ is set to the error message,
just as with an eval(). If there is no error, then the method returns
the value of the last expression evaluated, or a return statement may
be used, just as with subroutines and B<eval()>. Note that this
behaviour differs from the beta distribution of the Safe extension
where earlier versions of perl made it hard to mimic the return
behaviour of the eval() command.

=item rdo (FILENAME)

This evaluates the contents of file FILENAME inside the compartment.
See above documentation on the B<reval> method for further details.

=back

=head2 Subroutines in package Safe

The Safe package contains subroutines for manipulating operator
names and operator masks. All are available for export by the package.
The canonical list of operator names is the contents of the array
op_name defined and initialised in file F<opcode.h> of the Perl
source distribution.

=over 8

=item ops_to_mask (OP, ...)

This takes a list of operator names and returns an operator mask
with precisely those operators masked.

=item mask_to_ops (MASK)

This takes an operator mask and returns a list of operator names
corresponding to those operators which are masked in MASK.

=item opcode (OP, ...)

This takes a list of operator names and returns the corresponding
list of opcodes (which can then be used as byte offsets into a mask).

=item opname (OP, ...)

This takes a list of opcodes and returns the corresponding list of
operator names.

=item fullmask

This just returns a mask which has all operators masked.
It returns the string "\1" x MAXO().

=item emptymask

This just returns a mask which has all operators unmasked.
It returns the string "\0" x MAXO(). This is useful if you
want a compartment to make use of the namespace protection
features but do not want the default restrictive mask.

=item MAXO

This returns the number of operators (and hence the length of an
operator mask). Note that, unlike the beta distributions of the
Safe extension, this is derived from a genuine integer variable
in the perl executable and not from a preprocessor constant.
This means that the Safe extension is more robust in the presence
of mismatched versions of the perl executable and the Safe extension.

=item op_mask

This returns the operator mask which is actually in effect at the
time the invocation to the subroutine is compiled. In general,
this is probably not terribly useful.

=back

=head2 AUTHOR

Malcolm Beattie, mbeattie@sable.ox.ac.uk.

=cut

my $default_root = 'Root000000000';

my $default_mask;

sub new {
    my($class, $root, $mask) = @_;
    my $obj = {};
    bless $obj, $class;
    $obj->root(defined($root) ? $root : ("Safe::".$default_root++));
    $obj->mask(defined($mask) ? $mask : $default_mask);
    # We must share $_ and @_ with the compartment or else ops such
    # as split, length and so on won't default to $_ properly, nor
    # will passing argument to subroutines work (via @_). In fact,
    # for reasons I don't completely understand, we need to share
    # the whole glob *_ rather than $_ and @_ separately, otherwise
    # @_ in non default packages within the compartment don't work.
    *{$obj->root . "::_"} = *_;
    return $obj;
}

sub DESTROY {
    my($obj) = @_;
    my $root = $obj->root();
    if ($root =~ /^Safe::(Root\d+)$/){
	$root = $1;
	delete $ {"Safe::"}{"$root\::"};
    }
}

sub root {
    my $obj = shift;
    if (@_) {
	$obj->{Root} = $_[0];
    } else {
	return $obj->{Root};
    }
}

sub mask {
    my $obj = shift;
    if (@_) {
	$obj->{Mask} = verify_mask($_[0]);
    } else {
	return $obj->{Mask};
    }
}

sub verify_mask {
    my($mask) = @_;
    if (length($mask) != MAXO() || $mask !~ /^[\0\1]+$/) {
	croak("argument is not a mask");
    }
    return $mask;
}

sub trap {
    my $obj = shift;
    $obj->setmaskel("\1", @_);
}

sub untrap {
    my $obj = shift;
    $obj->setmaskel("\0", @_);
}

sub emptymask { "\0" x MAXO() }
sub fullmask { "\1" x MAXO() }

sub setmaskel {
    my $obj = shift;
    my $val = shift;
    croak("bad value for mask element") unless $val eq "\0" || $val eq "\1";
    my $maskref = \$obj->{Mask};
    my ($op, $opcode);
    foreach $op (@_) {
	$opcode = ($op =~ /^\d/) ? $op : opcode($op);
	substr($$maskref, $opcode, 1) = $val;
    }
}

sub share {
    my $obj = shift;
    my $root = $obj->root();
    my ($arg);
    foreach $arg (@_) {
	my $var;
	($var = $arg) =~ s/^(.)//;
	my $caller = caller;
	*{$root."::$var"} = ($1 eq '$') ? \${$caller."::$var"}
			  : ($1 eq '@') ? \@{$caller."::$var"}
			  : ($1 eq '%') ? \%{$caller."::$var"}
			  : ($1 eq '*') ? *{$caller."::$var"}
			  : ($1 eq '&') ? \&{$caller."::$var"}
			  : croak(qq(No such variable type for "$1$var"));
    }
}

sub varglob {
    my ($obj, $var) = @_;
    return *{$obj->root()."::$var"};
}

sub reval {
    my ($obj, $expr) = @_;
    my $root = $obj->{Root};
    my $mask = $obj->{Mask};
    verify_mask($mask);

    my $evalsub = eval sprintf(<<'EOT', $root);
	package %s;
	sub {
	    eval $expr;
	}
EOT
    return safe_call_sv($root, $mask, $evalsub);
}

sub rdo {
    my ($obj, $file) = @_;
    my $root = $obj->{Root};
    my $mask = $obj->{Mask};
    verify_mask($mask);

    $file =~ s/"/\\"/g; # just in case the filename contains any double quotes
    my $evalsub = eval sprintf(<<'EOT', $root, $file);
	package %s;
	sub {
	    do "%s";
	}
EOT
    return safe_call_sv($root, $mask, $evalsub);
}

bootstrap Safe $VERSION;

$default_mask = fullmask;
my $name;
while (defined ($name = <DATA>)) {
    chomp $name;
    next if $name =~ /^#/;
    my $code = opcode($name);
    substr($default_mask, $code, 1) = "\0";
}

1;

__DATA__
null
stub
scalar
pushmark
wantarray
const
gvsv
gv
gelem
padsv
padav
padhv
padany
pushre
rv2gv
rv2sv
av2arylen
rv2cv
anoncode
prototype
refgen
srefgen
ref
bless
glob
readline
rcatline
regcmaybe
regcomp
match
subst
substcont
trans
sassign
aassign
chop
schop
chomp
schomp
defined
undef
study
pos
preinc
i_preinc
predec
i_predec
postinc
i_postinc
postdec
i_postdec
pow
multiply
i_multiply
divide
i_divide
modulo
i_modulo
repeat
add
i_add
subtract
i_subtract
concat
stringify
left_shift
right_shift
lt
i_lt
gt
i_gt
le
i_le
ge
i_ge
eq
i_eq
ne
i_ne
ncmp
i_ncmp
slt
sgt
sle
sge
seq
sne
scmp
bit_and
bit_xor
bit_or
negate
i_negate
not
complement
atan2
sin
cos
rand
srand
exp
log
sqrt
int
hex
oct
abs
length
substr
vec
index
rindex
sprintf
formline
ord
chr
crypt
ucfirst
lcfirst
uc
lc
quotemeta
rv2av
aelemfast
aelem
aslice
each
values
keys
delete
exists
rv2hv
helem
hslice
split
join
list
lslice
anonlist
anonhash
splice
push
pop
shift
unshift
reverse
grepstart
grepwhile
mapstart
mapwhile
range
flip
flop
and
or
xor
cond_expr
andassign
orassign
method
entersub
leavesub
caller
warn
die
reset
lineseq
nextstate
dbstate
unstack
enter
leave
scope
enteriter
iter
enterloop
leaveloop
return
last
next
redo
goto
close
fileno
tie
untie
dbmopen
dbmclose
sselect
select
getc
read
enterwrite
leavewrite
prtf
print
sysread
syswrite
send
recv
eof
tell
seek
truncate
fcntl
ioctl
sockpair
bind
connect
listen
accept
shutdown
gsockopt
ssockopt
getsockname
ftrwrite
ftsvtx
open_dir
readdir
telldir
seekdir
rewinddir
kill
getppid
getpgrp
setpgrp
getpriority
setpriority
time
tms
localtime
alarm
dofile
entereval
leaveeval
entertry
leavetry
ghbyname
ghbyaddr
ghostent
gnbyname
gnbyaddr
gnetent
gpbyname
gpbynumber
gprotoent
gsbyname
gsbyport
gservent
shostent
snetent
sprotoent
sservent
ehostent
enetent
eprotoent
eservent
gpwnam
gpwuid
gpwent
spwent
epwent
ggrnam
ggrgid
ggrent
sgrent
egrent
