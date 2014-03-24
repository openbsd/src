#!/usr/bin/perl -w
# 
# Regenerate (overwriting only if changed):
#
#    pod/perldebguts.pod
#    regnodes.h
#
# from information stored in
#
#    regcomp.sym
#    regexp.h
#
# pod/perldebguts.pod is not completely regenerated.  Only the table of
# regexp nodes is replaced; other parts remain unchanged.
#
# Accepts the standard regen_lib -q and -v args.
#
# This script is normally invoked from regen.pl.

BEGIN {
    # Get function prototypes
    require 'regen/regen_lib.pl';
}
use strict;

open DESC, 'regcomp.sym';

my $ind = 0;
my (@name,@rest,@type,@code,@args,@flags,@longj,@cmnt);
my ($longest_name_length,$desc,$lastregop) = 0;
while (<DESC>) {
    # Special pod comments
    if (/^#\* ?/) { $cmnt[$ind] .= "# $'"; }
    # Truly blank lines possibly surrounding pod comments
    elsif (/^\s*$/) { $cmnt[$ind] .= "\n" }

    next if /^(?:#|\s*$)/;
    chomp; # No \z in 5.004
    s/\s*$//;
    if (/^-+\s*$/) {
        $lastregop= $ind;
        next;
    }
    unless ($lastregop) {
        ($name[$ind], $desc, $rest[$ind]) = /^(\S+)\s+([^\t]+?)\s*;\s*(.*)/;
        ($type[$ind], $code[$ind], $args[$ind], $flags[$ind], $longj[$ind])
          = split /[,\s]\s*/, $desc;
        $longest_name_length = length $name[$ind]
          if length $name[$ind] > $longest_name_length;
        ++$ind;
    } else {
        my ($type,@lists)=split /\s+/, $_;
        die "No list? $type" if !@lists;
        foreach my $list (@lists) {
            my ($names,$special)=split /:/, $list , 2;
            $special ||= "";
            foreach my $name (split /,/,$names) {
                my $real= $name eq 'resume' 
                        ? "resume_$type" 
                        : "${type}_$name";
                my @suffix;
                if (!$special) {
                   @suffix=("");
                } elsif ($special=~/\d/) {
                    @suffix=(1..$special);
                } elsif ($special eq 'FAIL') {
                    @suffix=("","_fail");
                } else {
                    die "unknown :type ':$special'";
                }
                foreach my $suffix (@suffix) {
                    $name[$ind]="$real$suffix";
                    $type[$ind]=$type;
                    $rest[$ind]="state for $type";
                    ++$ind;
                }
            }
        }
        
    }
}
# use fixed width to keep the diffs between regcomp.pl recompiles
# as small as possible.
my ($width,$rwidth,$twidth)=(22,12,9);
$lastregop ||= $ind;
my $tot = $ind;
close DESC;
die "Too many regexp/state opcodes! Maximum is 256, but there are $lastregop in file!"
    if $lastregop>256;

sub process_flags {
  my ($flag, $varname, $comment) = @_;
  $comment = '' unless defined $comment;

  $ind = 0;
  my @selected;
  my $bitmap = '';
  do {
    my $set = $flags[$ind] && $flags[$ind] eq $flag ? 1 : 0;
    # Whilst I could do this with vec, I'd prefer to do longhand the arithmetic
    # ops in the C code.
    my $current = do {
      local $^W;
      ord do {
	substr $bitmap, ($ind >> 3);
      }
    };
    substr($bitmap, ($ind >> 3), 1) = chr($current | ($set << ($ind & 7)));

    push @selected, $name[$ind] if $set;
  } while (++$ind < $lastregop);
  my $out_string = join ', ', @selected, 0;
  $out_string =~ s/(.{1,70},) /$1\n    /g;

  my $out_mask = join ', ', map {sprintf "0x%02X", ord $_} split '', $bitmap;

  return $comment . <<"EOP";
#define REGNODE_\U$varname\E(node) (PL_${varname}_bitmask[(node) >> 3] & (1 << ((node) & 7)))

#ifndef DOINIT
EXTCONST U8 PL_${varname}\[] __attribute__deprecated__;
#else
EXTCONST U8 PL_${varname}\[] __attribute__deprecated__ = {
    $out_string
};
#endif /* DOINIT */

#ifndef DOINIT
EXTCONST U8 PL_${varname}_bitmask[];
#else
EXTCONST U8 PL_${varname}_bitmask[] = {
    $out_mask
};
#endif /* DOINIT */
EOP
}

my $out = open_new('regnodes.h', '>',
		   { by => 'regen/regcomp.pl', from => 'regcomp.sym' });
printf $out <<EOP,
/* Regops and State definitions */

#define %*s\t%d
#define %*s\t%d

EOP
    -$width, REGNODE_MAX        => $lastregop - 1,
    -$width, REGMATCH_STATE_MAX => $tot - 1
;


for ($ind=0; $ind < $lastregop ; ++$ind) {
  printf $out "#define\t%*s\t%d\t/* %#04x %s */\n",
    -$width, $name[$ind], $ind, $ind, $rest[$ind];
}
print $out "\t/* ------------ States ------------- */\n";
for ( ; $ind < $tot ; $ind++) {
  printf $out "#define\t%*s\t(REGNODE_MAX + %d)\t/* %s */\n",
    -$width, $name[$ind], $ind - $lastregop + 1, $rest[$ind];
}

print $out <<EOP;

/* PL_regkind[] What type of regop or state is this. */

#ifndef DOINIT
EXTCONST U8 PL_regkind[];
#else
EXTCONST U8 PL_regkind[] = {
EOP

$ind = 0;
do {
  printf $out "\t%*s\t/* %*s */\n",
             -1-$twidth, "$type[$ind],", -$width, $name[$ind];
  print $out "\t/* ------------ States ------------- */\n"
    if $ind + 1 == $lastregop and $lastregop != $tot;
} while (++$ind < $tot);

print $out <<EOP;
};
#endif

/* regarglen[] - How large is the argument part of the node (in regnodes) */

#ifdef REG_COMP_C
static const U8 regarglen[] = {
EOP

$ind = 0;
do {
  my $size = 0;
  $size = "EXTRA_SIZE(struct regnode_$args[$ind])" if $args[$ind];
  
  printf $out "\t%*s\t/* %*s */\n",
	-37, "$size,",-$rwidth,$name[$ind];
} while (++$ind < $lastregop);

print $out <<EOP;
};

/* reg_off_by_arg[] - Which argument holds the offset to the next node */

static const char reg_off_by_arg[] = {
EOP

$ind = 0;
do {
  my $size = $longj[$ind] || 0;

  printf $out "\t%d,\t/* %*s */\n",
	$size, -$rwidth, $name[$ind]
} while (++$ind < $lastregop);

print $out <<EOP;
};

#endif /* REG_COMP_C */

/* reg_name[] - Opcode/state names in string form, for debugging */

#ifndef DOINIT
EXTCONST char * PL_reg_name[];
#else
EXTCONST char * const PL_reg_name[] = {
EOP

$ind = 0;
my $ofs = 0;
my $sym = "";
do {
  my $size = $longj[$ind] || 0;

  printf $out "\t%*s\t/* $sym%#04x */\n",
	-3-$width,qq("$name[$ind]",), $ind - $ofs;
  if ($ind + 1 == $lastregop and $lastregop != $tot) {
    print $out "\t/* ------------ States ------------- */\n";
    $ofs = $lastregop - 1;
    $sym = 'REGNODE_MAX +';
  }
    
} while (++$ind < $tot);

print $out <<EOP;
};
#endif /* DOINIT */

/* PL_reg_extflags_name[] - Opcode/state names in string form, for debugging */

#ifndef DOINIT
EXTCONST char * PL_reg_extflags_name[];
#else
EXTCONST char * const PL_reg_extflags_name[] = {
EOP

my %rxfv;
my %definitions;    # Remember what the symbol definitions are
my $val = 0;
my %reverse;
foreach my $file ("op_reg_common.h", "regexp.h") {
    open FH,"<$file" or die "Can't read $file: $!";
    while (<FH>) {

        # optional leading '_'.  Return symbol in $1, and strip it from
        # rest of line
        if (s/ \# \s* define \s+ ( _? RXf_ \w+ ) \s+ //xi) {
            chomp;
            my $define = $1;
            my $orig= $_;
            s{ /\* .*? \*/ }{ }x;    # Replace comments by a blank

            # Replace any prior defined symbols by their values
            foreach my $key (keys %definitions) {
                s/\b$key\b/$definitions{$key}/g;
            }

	    # Remove the U suffix from unsigned int literals
	    s/\b([0-9]+)U\b/$1/g;

            my $newval = eval $_;   # Get numeric definition

            $definitions{$define} = $newval;

            next unless $_ =~ /<</; # Bit defines use left shift
            if($val & $newval) {
                my @names=($define, $reverse{$newval});
                s/PMf_// for @names;
                if ($names[0] ne $names[1]) {
                    die sprintf "ERROR: both $define and $reverse{$newval} use 0x%08X (%s:%s)", $newval, $orig, $_;
                }
                next;
            }
            $val|=$newval;
            $rxfv{$define}= $newval;
            $reverse{$newval} = $define;
        }
    }
}
my %vrxf=reverse %rxfv;
printf $out "\t/* Bits in extflags defined: %s */\n", unpack 'B*', pack 'N', $val;
my %multibits;
for (0..31) {
    my $power_of_2 = 2**$_;
    my $n=$vrxf{$power_of_2};
    my $extra = "";
    if (! $n) {

        # Here, there was no name that matched exactly the bit.  It could be
        # either that it is unused, or the name matches multiple bits.
        if (! ($val & $power_of_2)) {
            $n = "UNUSED_BIT_$_";
        }
        else {

            # Here, must be because it matches multiple bits.  Look through
            # all possibilities until find one that matches this one.  Use
            # that name, and all the bits it matches
            foreach my $name (keys %rxfv) {
                if ($rxfv{$name} & $power_of_2) {
                    $n = $name . ( $multibits{$name}++ );
                    $extra= sprintf qq{ : "%s" - 0x%08x}, $name, $rxfv{$name}
                        if $power_of_2 != $rxfv{$name};
                    last;
                }
            }
        }
    }
    s/\bRXf_(PMf_)?// for $n, $extra;
    printf $out qq(\t%-20s/* 0x%08x%s */\n),
        qq("$n",),$power_of_2, $extra;
}  
 
print $out <<EOP;
};
#endif /* DOINIT */

EOP

print $out process_flags('V', 'varies', <<'EOC');
/* The following have no fixed length. U8 so we can do strchr() on it. */
EOC

print $out process_flags('S', 'simple', <<'EOC');

/* The following always have a length of 1. U8 we can do strchr() on it. */
/* (Note that length 1 means "one character" under UTF8, not "one octet".) */
EOC

read_only_bottom_close_and_rename($out);

my $guts = open_new('pod/perldebguts.pod', '>');

my $code;
my $name_fmt = '<' x ($longest_name_length-1);
my $descr_fmt = '<' x (58-$longest_name_length);
eval <<EOD;
format GuTS =
 ^*~~
 \$cmnt[\$_]
 ^$name_fmt ^<<<<<<<<< ^$descr_fmt~~
 \$name[\$_], \$code,  \$rest[\$_]
.
EOD

select +(select($guts), do {
    $~ = "GuTS";

    open my $oldguts, "pod/perldebguts.pod"
        or die "$0 cannot open pod/perldebguts.pod for reading: $!";
    while(<$oldguts>) {
        print;
        last if /=for regcomp.pl begin/;
    }

    print <<'end';

 # TYPE arg-description [num-args] [longjump-len] DESCRIPTION
end
    for (0..$lastregop-1) {
        $code = "$code[$_] ".($args[$_]||"");
        $code .= " $longj[$_]" if $longj[$_];
        if ($cmnt[$_] ||= "") {
            # Trim multiple blanks
            $cmnt[$_] =~ s/^\n\n+/\n/; $cmnt[$_] =~ s/\n\n+$/\n\n/
        }
        write;
    }
    print "\n";

    while(<$oldguts>) {
        last if /=for regcomp.pl end/;
    }
    do { print } while <$oldguts>;

})[0];

close_and_rename($guts);
