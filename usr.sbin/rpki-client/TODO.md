
The following are unclear to me.

- Following up on validating AS numbers for certificates.  The
  specification is not clear on what happens with empty AS extensions in
  a chain of certificates.  Do we consider that inheritence?  If so,
  what's the point of having an inheritence clause?

- I get that ASid 0 has special meaning for ROAs (see RFC 6483 sec 4),
  but it doesn't make sense that some top-level certificates (e.g.,
  Afrinic) have a range inclusive of zero, since it's reserved.  In this
  system, I let the range through but don't let a specific ASid of 0 in
  certificates---only ROAs.

- VRP duplication.  When run as-is, there are duplicate VRPs and
  that doesn't seem right.  It happens when two ROAs have their validity
  period overlap.  I need to see if there's a more programmatic way to
  check before commiting the routes to output.

- (**Important**.) Stipulating `X509_V_FLAG_IGNORE_CRITICAL` might be
  dangerous.  Which extensions are being ignored should be
  double-checked.
