#!./perl

# We have the following types of loop:
#
# 1a)  while(A) {B}
# 1b)  B while A;
#
# 2a)  until(A) {B}
# 2b)  B until A;
#
# 3a)  for(@A)  {B}
# 3b)  B for A;
#
# 4a)  for (A;B;C) {D}
#
# 5a)  { A }        # a bare block is a loop which runs once
#
# Loops of type (b) don't allow for next/last/redo style
#  control, so we ignore them here. Type (a) loops can
#  all be labelled, so there are ten possibilities (each
#  of 5 types, labelled/unlabelled). We therefore need
#  thirty tests to try the three control statements against
#  the ten types of loop. For the first four types it's useful
#  to distinguish the case where next re-iterates from the case
#  where it leaves the loop. That makes 38.
# All these tests rely on "last LABEL"
#  so if they've *all* failed, maybe you broke that...
#
# These tests are followed by an extra test of nested loops.
# Feel free to add more here.
#
#  -- .robin. <robin@kitsite.com>  2001-03-13

print "1..44\n";

my $ok;

## while() loop without a label

TEST1: { # redo

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  while($x--) {
    if (!$first_time) {
      $ok = 1;
      last TEST1;
    }
    $ok = 0;
    $first_time = 0;
    redo;
    last TEST1;
  }
  continue {
    $ok = 0;
    last TEST1;
  }
  $ok = 0;
}
print ($ok ? "ok 1\n" : "not ok 1\n");

TEST2: { # next (succesful)

  $ok = 0;

  my $x = 2;
  my $first_time = 1;
  my $been_in_continue = 0;
  while($x--) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST2;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST2;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
}
print ($ok ? "ok 2\n" : "not ok 2\n");

TEST3: { # next (unsuccesful)

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  while($x--) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST3;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST3;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
}
print ($ok ? "ok 3\n" : "not ok 3\n");

TEST4: { # last

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  while($x++) {
    if (!$first_time) {
      $ok = 0;
      last TEST4;
    }
    $ok = 0;
    $first_time = 0;
    last;
    last TEST4;
  }
  continue {
    $ok = 0;
    last TEST4;
  }
  $ok = 1;
}
print ($ok ? "ok 4\n" : "not ok 4\n");


## until() loop without a label

TEST5: { # redo

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  until($x++) {
    if (!$first_time) {
      $ok = 1;
      last TEST5;
    }
    $ok = 0;
    $first_time = 0;
    redo;
    last TEST5;
  }
  continue {
    $ok = 0;
    last TEST5;
  }
  $ok = 0;
}
print ($ok ? "ok 5\n" : "not ok 5\n");

TEST6: { # next (succesful)

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  my $been_in_continue = 0;
  until($x++ >= 2) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST6;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST6;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
}
print ($ok ? "ok 6\n" : "not ok 6\n");

TEST7: { # next (unsuccesful)

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  until($x++) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST7;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST7;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
}
print ($ok ? "ok 7\n" : "not ok 7\n");

TEST8: { # last

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  until($x++ == 10) {
    if (!$first_time) {
      $ok = 0;
      last TEST8;
    }
    $ok = 0;
    $first_time = 0;
    last;
    last TEST8;
  }
  continue {
    $ok = 0;
    last TEST8;
  }
  $ok = 1;
}
print ($ok ? "ok 8\n" : "not ok 8\n");

## for(@array) loop without a label

TEST9: { # redo

  $ok = 0;

  my $first_time = 1;
  for(1) {
    if (!$first_time) {
      $ok = 1;
      last TEST9;
    }
    $ok = 0;
    $first_time = 0;
    redo;
    last TEST9;
  }
  continue {
    $ok = 0;
    last TEST9;
  }
  $ok = 0;
}
print ($ok ? "ok 9\n" : "not ok 9\n");

TEST10: { # next (succesful)

  $ok = 0;

  my $first_time = 1;
  my $been_in_continue = 0;
  for(1,2) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST10;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST10;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
}
print ($ok ? "ok 10\n" : "not ok 10\n");

TEST11: { # next (unsuccesful)

  $ok = 0;

  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  for(1) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST11;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST11;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
}
print ($ok ? "ok 11\n" : "not ok 11\n");

TEST12: { # last

  $ok = 0;

  my $first_time = 1;
  for(1..10) {
    if (!$first_time) {
      $ok = 0;
      last TEST12;
    }
    $ok = 0;
    $first_time = 0;
    last;
    last TEST12;
  }
  continue {
    $ok=0;
    last TEST12;
  }
  $ok = 1;
}
print ($ok ? "ok 12\n" : "not ok 12\n");

## for(;;) loop without a label

TEST13: { # redo

  $ok = 0;

  for(my $first_time = 1; 1;) {
    if (!$first_time) {
      $ok = 1;
      last TEST13;
    }
    $ok = 0;
    $first_time=0;

    redo;
    last TEST13;
  }
  $ok = 0;
}
print ($ok ? "ok 13\n" : "not ok 13\n");

TEST14: { # next (successful)

  $ok = 0;

  for(my $first_time = 1; 1; $first_time=0) {
    if (!$first_time) {
      $ok = 1;
      last TEST14;
    }
    $ok = 0;
    next;
    last TEST14;
  }
  $ok = 0;
}
print ($ok ? "ok 14\n" : "not ok 14\n");

TEST15: { # next (unsuccesful)

  $ok = 0;

  my $x=1;
  my $been_in_loop = 0;
  for(my $first_time = 1; $x--;) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST15;
    }
    $ok = 0;
    $first_time = 0;
    next;
    last TEST15;
  }
  $ok = $been_in_loop;
}
print ($ok ? "ok 15\n" : "not ok 15\n");

TEST16: { # last

  $ok = 0;

  for(my $first_time = 1; 1; last TEST16) {
    if (!$first_time) {
      $ok = 0;
      last TEST16;
    }
    $ok = 0;
    $first_time = 0;
    last;
    last TEST16;
  }
  $ok = 1;
}
print ($ok ? "ok 16\n" : "not ok 16\n");

## bare block without a label

TEST17: { # redo

  $ok = 0;
  my $first_time = 1;

  {
    if (!$first_time) {
      $ok = 1;
      last TEST17;
    }
    $ok = 0;
    $first_time=0;

    redo;
    last TEST17;
  }
  continue {
    $ok = 0;
    last TEST17;
  }
  $ok = 0;
}
print ($ok ? "ok 17\n" : "not ok 17\n");

TEST18: { # next

  $ok = 0;
  {
    next;
    last TEST18;
  }
  continue {
    $ok = 1;
    last TEST18;
  }
  $ok = 0;
}
print ($ok ? "ok 18\n" : "not ok 18\n");

TEST19: { # last

  $ok = 0;
  {
    last;
    last TEST19;
  }
  continue {
    $ok = 0;
    last TEST19;
  }
  $ok = 1;
}
print ($ok ? "ok 19\n" : "not ok 19\n");


### Now do it all again with labels

## while() loop with a label

TEST20: { # redo

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  LABEL20: while($x--) {
    if (!$first_time) {
      $ok = 1;
      last TEST20;
    }
    $ok = 0;
    $first_time = 0;
    redo LABEL20;
    last TEST20;
  }
  continue {
    $ok = 0;
    last TEST20;
  }
  $ok = 0;
}
print ($ok ? "ok 20\n" : "not ok 20\n");

TEST21: { # next (succesful)

  $ok = 0;

  my $x = 2;
  my $first_time = 1;
  my $been_in_continue = 0;
  LABEL21: while($x--) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST21;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL21;
    last TEST21;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
}
print ($ok ? "ok 21\n" : "not ok 21\n");

TEST22: { # next (unsuccesful)

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  LABEL22: while($x--) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST22;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL22;
    last TEST22;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
}
print ($ok ? "ok 22\n" : "not ok 22\n");

TEST23: { # last

  $ok = 0;

  my $x = 1;
  my $first_time = 1;
  LABEL23: while($x++) {
    if (!$first_time) {
      $ok = 0;
      last TEST23;
    }
    $ok = 0;
    $first_time = 0;
    last LABEL23;
    last TEST23;
  }
  continue {
    $ok = 0;
    last TEST23;
  }
  $ok = 1;
}
print ($ok ? "ok 23\n" : "not ok 23\n");


## until() loop with a label

TEST24: { # redo

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  LABEL24: until($x++) {
    if (!$first_time) {
      $ok = 1;
      last TEST24;
    }
    $ok = 0;
    $first_time = 0;
    redo LABEL24;
    last TEST24;
  }
  continue {
    $ok = 0;
    last TEST24;
  }
  $ok = 0;
}
print ($ok ? "ok 24\n" : "not ok 24\n");

TEST25: { # next (succesful)

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  my $been_in_continue = 0;
  LABEL25: until($x++ >= 2) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST25;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL25;
    last TEST25;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
}
print ($ok ? "ok 25\n" : "not ok 25\n");

TEST26: { # next (unsuccesful)

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  LABEL26: until($x++) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST26;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL26;
    last TEST26;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
}
print ($ok ? "ok 26\n" : "not ok 26\n");

TEST27: { # last

  $ok = 0;

  my $x = 0;
  my $first_time = 1;
  LABEL27: until($x++ == 10) {
    if (!$first_time) {
      $ok = 0;
      last TEST27;
    }
    $ok = 0;
    $first_time = 0;
    last LABEL27;
    last TEST27;
  }
  continue {
    $ok = 0;
    last TEST8;
  }
  $ok = 1;
}
print ($ok ? "ok 27\n" : "not ok 27\n");

## for(@array) loop with a label

TEST28: { # redo

  $ok = 0;

  my $first_time = 1;
  LABEL28: for(1) {
    if (!$first_time) {
      $ok = 1;
      last TEST28;
    }
    $ok = 0;
    $first_time = 0;
    redo LABEL28;
    last TEST28;
  }
  continue {
    $ok = 0;
    last TEST28;
  }
  $ok = 0;
}
print ($ok ? "ok 28\n" : "not ok 28\n");

TEST29: { # next (succesful)

  $ok = 0;

  my $first_time = 1;
  my $been_in_continue = 0;
  LABEL29: for(1,2) {
    if (!$first_time) {
      $ok = $been_in_continue;
      last TEST29;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL29;
    last TEST29;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = 0;
}
print ($ok ? "ok 29\n" : "not ok 29\n");

TEST30: { # next (unsuccesful)

  $ok = 0;

  my $first_time = 1;
  my $been_in_loop = 0;
  my $been_in_continue = 0;
  LABEL30: for(1) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST30;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL30;
    last TEST30;
  }
  continue {
    $been_in_continue = 1;
  }
  $ok = $been_in_loop && $been_in_continue;
}
print ($ok ? "ok 30\n" : "not ok 30\n");

TEST31: { # last

  $ok = 0;

  my $first_time = 1;
  LABEL31: for(1..10) {
    if (!$first_time) {
      $ok = 0;
      last TEST31;
    }
    $ok = 0;
    $first_time = 0;
    last LABEL31;
    last TEST31;
  }
  continue {
    $ok=0;
    last TEST31;
  }
  $ok = 1;
}
print ($ok ? "ok 31\n" : "not ok 31\n");

## for(;;) loop with a label

TEST32: { # redo

  $ok = 0;

  LABEL32: for(my $first_time = 1; 1;) {
    if (!$first_time) {
      $ok = 1;
      last TEST32;
    }
    $ok = 0;
    $first_time=0;

    redo LABEL32;
    last TEST32;
  }
  $ok = 0;
}
print ($ok ? "ok 32\n" : "not ok 32\n");

TEST33: { # next (successful)

  $ok = 0;

  LABEL33: for(my $first_time = 1; 1; $first_time=0) {
    if (!$first_time) {
      $ok = 1;
      last TEST33;
    }
    $ok = 0;
    next LABEL33;
    last TEST33;
  }
  $ok = 0;
}
print ($ok ? "ok 33\n" : "not ok 33\n");

TEST34: { # next (unsuccesful)

  $ok = 0;

  my $x=1;
  my $been_in_loop = 0;
  LABEL34: for(my $first_time = 1; $x--;) {
    $been_in_loop = 1;
    if (!$first_time) {
      $ok = 0;
      last TEST34;
    }
    $ok = 0;
    $first_time = 0;
    next LABEL34;
    last TEST34;
  }
  $ok = $been_in_loop;
}
print ($ok ? "ok 34\n" : "not ok 34\n");

TEST35: { # last

  $ok = 0;

  LABEL35: for(my $first_time = 1; 1; last TEST16) {
    if (!$first_time) {
      $ok = 0;
      last TEST35;
    }
    $ok = 0;
    $first_time = 0;
    last LABEL35;
    last TEST35;
  }
  $ok = 1;
}
print ($ok ? "ok 35\n" : "not ok 35\n");

## bare block with a label

TEST36: { # redo

  $ok = 0;
  my $first_time = 1;

  LABEL36: {
    if (!$first_time) {
      $ok = 1;
      last TEST36;
    }
    $ok = 0;
    $first_time=0;

    redo LABEL36;
    last TEST36;
  }
  continue {
    $ok = 0;
    last TEST36;
  }
  $ok = 0;
}
print ($ok ? "ok 36\n" : "not ok 36\n");

TEST37: { # next

  $ok = 0;
  LABEL37: {
    next LABEL37;
    last TEST37;
  }
  continue {
    $ok = 1;
    last TEST37;
  }
  $ok = 0;
}
print ($ok ? "ok 37\n" : "not ok 37\n");

TEST38: { # last

  $ok = 0;
  LABEL38: {
    last LABEL38;
    last TEST38;
  }
  continue {
    $ok = 0;
    last TEST38;
  }
  $ok = 1;
}
print ($ok ? "ok 38\n" : "not ok 38\n");

### Now test nested constructs

TEST39: {
    $ok = 0;
    my ($x, $y, $z) = (1,1,1);
    one39: while ($x--) {
      $ok = 0;
      two39: while ($y--) {
        $ok = 0;
        three39: while ($z--) {
           next two39;
        }
        continue {
          $ok = 0;
          last TEST39;
        }
      }
      continue {
        $ok = 1;
        last TEST39;
      }
      $ok = 0;
    }
}
print ($ok ? "ok 39\n" : "not ok 39\n");


### Test that loop control is dynamicly scoped.

sub test_last_label { last TEST40 }

TEST40: {
    $ok = 1;
    test_last_label();
    $ok = 0;
}
print ($ok ? "ok 40\n" : "not ok 40\n");

sub test_last { last }

TEST41: {
    $ok = 1;
    test_last();
    $ok = 0;
}
print ($ok ? "ok 41\n" : "not ok 41\n");


# [perl #27206] Memory leak in continue loop
# Ensure that the temporary object is freed each time round the loop,
# rather then all 10 of them all being freed right at the end

{
    my $n=10; my $late_free = 0;
    sub X::DESTROY { $late_free++ if $n < 0 };
    {
	($n-- && bless {}, 'X') && redo;
    }
    print $late_free ? "not " : "", "ok 42 - redo memory leak\n";

    $n = 10; $late_free = 0;
    {
	($n-- && bless {}, 'X') && redo;
    }
    continue { }
    print $late_free ? "not " : "", "ok 43 - redo with continue memory leak\n";
}



{
    # [perl #37725]

    $a37725[3] = 1; # use package var
    $i = 2;
    for my $x (reverse @a37725) {
	$x = $i++;
    }
    print "@a37725" == "5 4 3 2" ? "" : "not ",
	"ok 44 - reverse with empty slots (@a37725)\n";
}

