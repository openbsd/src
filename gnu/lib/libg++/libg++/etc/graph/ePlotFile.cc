#pragma implementation
#include "ePlotFile.h"

ePlotFile& ePlotFile:: alabel (alabel_xadj x_adjust,
			       alabel_yadj y_adjust, const char *s)
{
  cmd ('T');
  cmd (x_adjust);
  cmd (y_adjust);
  *this << s;
  *this << "\n";
  return *this;
};
