{
  my $__ntest;

  sub ok ($;$$) {
    local($\,$,);
    my $ok = 0;
    my $result = shift;
    if (@_ == 0) {
      $ok = $result;
    } else {
      $expected = shift;
      if (!defined $expected) {
        $ok = !defined $result;
      } elsif (!defined $result) {
        $ok = 0;
      } elsif (ref($expected) eq 'Regexp') {
        $ok = $result =~ /$expected/;
      } else {
        $ok = $result eq $expected;
      }
    }
    ++$__ntest;
    if ($ok) {
      print "ok $__ntest\n"
    }
    else {
      print "not ok $__ntest\n"
    }
  }
}

1;
