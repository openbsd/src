/* $OpenBSD: math_ec2n.h,v 1.7 2004/05/23 18:17:56 hshoexer Exp $	 */
/* $EOM: math_ec2n.h,v 1.4 1999/04/17 23:20:37 niklas Exp $	 */

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _MATH_EC2N_H
#define _MATH_EC2N_H_

/* Definitions for points on an elliptic curve */

typedef struct {
	int             inf;	/* Are we the point at infinity ? */
	b2n_t           x, y;
}               _ec2n_point;

typedef _ec2n_point *ec2np_ptr;
typedef _ec2n_point ec2np_t[1];

#define EC2NP_SWAP(k,n) do \
  { \
    int _i_; \
\
    _i_ = (k)->inf; \
    (k)->inf = (n)->inf; \
    (n)->inf = _i_; \
    B2N_SWAP ((k)->x, (n)->x); \
    B2N_SWAP ((k)->y, (n)->y); \
  } \
while (0)

void	ec2np_init(ec2np_ptr);
void	ec2np_clear(ec2np_ptr);
int	ec2np_set(ec2np_ptr, ec2np_ptr);

#define ec2np_set_x_ui(n, y) b2n_set_ui ((n)->x, y)
#define ec2np_set_y_ui(n, x) b2n_set_ui ((n)->y, x)
#define ec2np_set_x_str(n, y) b2n_set_str ((n)->x, y)
#define ec2np_set_y_str(n, x) b2n_set_str ((n)->y, x)

/* Definitions for the group to which the points to belong to.  */

typedef struct {
	b2n_t           a, b, p;
}               _ec2n_group;

typedef _ec2n_group *ec2ng_ptr;
typedef _ec2n_group ec2ng_t[1];

void	ec2ng_init(ec2ng_ptr);
void	ec2ng_clear(ec2ng_ptr);
int	ec2ng_set(ec2ng_ptr, ec2ng_ptr);

#define ec2ng_set_a_ui(n, x) b2n_set_ui ((n)->a, x)
#define ec2ng_set_b_ui(n, x) b2n_set_ui ((n)->b, x)
#define ec2ng_set_p_ui(n, x) b2n_set_ui ((n)->p, x)
#define ec2ng_set_a_str(n, x) b2n_set_str ((n)->a, x)
#define ec2ng_set_b_str(n, x) b2n_set_str ((n)->b, x)
#define ec2ng_set_p_str(n, x) b2n_set_str ((n)->p, x)

/* Functions for computing on the elliptic group.  */

int	ec2np_add(ec2np_ptr, ec2np_ptr, ec2np_ptr, ec2ng_ptr);
int	ec2np_find_y(ec2np_ptr, ec2ng_ptr);
int	ec2np_ison(ec2np_ptr, ec2ng_ptr);
int	ec2np_mul(ec2np_ptr, ec2np_ptr, b2n_ptr, ec2ng_ptr);
int	ec2np_right(b2n_ptr n, ec2np_ptr, ec2ng_ptr);

#endif				/* _MATH_2N_H_ */
