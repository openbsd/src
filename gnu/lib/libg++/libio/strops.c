/* 
Copyright (C) 1993 Free Software Foundation

This file is part of the GNU IO Library.  This library is free
software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option)
any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

As a special exception, if you link this library with files
compiled with a GNU compiler to produce an executable, this does not cause
the resulting executable to be covered by the GNU General Public License.
This exception does not however invalidate any other reasons why
the executable file might be covered by the GNU General Public License. */

#include "strfile.h"
#include "libioP.h"
#include <string.h>

#define LEN(fp) (((_IO_strfile*)(fp))->_s._len)

#ifdef TODO
/* An "unbounded buffer" is when a buffer is supplied, but with no
   specified length.  An example is the buffer argument to sprintf.
   */
#endif

void
DEFUN(_IO_str_init_static, (fp, ptr, size, pstart),
      _IO_FILE *fp AND char *ptr AND int size AND char *pstart)
{
  if (size == 0)
    size = strlen(ptr);
  else if (size < 0)
    {
      /* If size is negative 'the characters are assumed to
	 continue indefinitely.'  This is kind of messy ... */
#if 1
      int s;
      size = 512;
      /* Try increasing powers of 2, as long as we don't wrap around.
	 This can lose in pathological cases (ptr near the end
	 of the address space).  A better solution might be to
	 adjust the size on underflow/overflow.  FIXME. */
      for (s; s = 2*size, s > 0 && ptr + s > ptr && s < 0x4000000L; )
	size = s;
      size = s;
#else
      /* The following semi-portable kludge assumes that
	 sizeof(unsigned long) == sizeof(char*). Hence,
	 (unsigned long)(-1) should be the largest possible address. */
      unsigned long highest = (unsigned long)(-1);
      /* Pointers are signed on some brain-damaged systems, in
	 which case we divide by two to get the maximum signed address. */
      if  ((char*)highest < ptr)
	highest >>= 1;
      size = (char*)highest - ptr;
#endif
    }
  _IO_setb(fp, ptr, ptr+size, 0);

  fp->_IO_write_base = ptr;
  fp->_IO_read_base = ptr;
  fp->_IO_read_ptr = ptr;
  if (pstart)
    {
      fp->_IO_write_ptr = pstart;
      fp->_IO_write_end = ptr+size;
      fp->_IO_read_end = pstart;
    }
  else
    {
      fp->_IO_write_ptr = ptr;
      fp->_IO_write_end = ptr;
      fp->_IO_read_end = ptr+size;
    }
  LEN(fp) = size;
  /* A null _allocate_buffer function flags the strfile as being static. */
  (((_IO_strfile*)(fp))->_s._allocate_buffer) =  (_IO_alloc_type)0;
}

void
DEFUN(_IO_str_init_readonly, (fp, ptr, size),
      _IO_FILE *fp AND const char *ptr AND int size)
{
  _IO_str_init_static (fp, (char*)ptr, size, NULL);
  fp->_IO_file_flags |= _IO_NO_WRITES;
}

int
DEFUN(_IO_str_overflow, (fp, c),
      register _IO_FILE* fp AND int c)
{
  int flush_only = c == EOF;
  _IO_size_t pos = fp->_IO_write_ptr - fp->_IO_write_base;
  _IO_size_t get_pos = fp->_IO_read_ptr - fp->_IO_read_base;
  if (fp->_flags & _IO_NO_WRITES)
      return flush_only ? 0 : EOF;
  if (pos > LEN(fp)) LEN(fp) = pos;
  if ((fp->_flags & _IO_TIED_PUT_GET) && !(fp->_flags & _IO_CURRENTLY_PUTTING))
    {
      pos = get_pos;
      fp->_flags |= _IO_CURRENTLY_PUTTING;
      get_pos = LEN(fp);
    }
  if (pos >= _IO_blen(fp) + flush_only)
    {
      if (fp->_flags & _IO_USER_BUF) /* not allowed to enlarge */
	{
#ifdef TODO
	  if (indefinite size)
	    {
	      fp->_IO_buf_end += 512;
	    }
	  else
#endif
	  return EOF;
	}
      else
	{
	  char *new_buf;
	  _IO_size_t new_size = 2 * _IO_blen(fp);
	  new_buf
	    = (char*)(*((_IO_strfile*)fp)->_s._allocate_buffer)(new_size);
	  if (new_buf == NULL)
	    {
	      /*	  __ferror(fp) = 1; */
	      return EOF;
	    }
	  memcpy(new_buf, fp->_IO_buf_base, _IO_blen(fp));
#if 0
	  if (lenp == &LEN(fp)) /* use '\0'-filling */
	      memset(new_buf + pos, 0, blen() - pos);
#endif
	  if (fp->_IO_buf_base)
	    {
	      (*((_IO_strfile*)fp)->_s._free_buffer)(fp->_IO_buf_base);
	      /* Make sure _IO_setb won't try to delete _IO_buf_base. */
	      fp->_IO_buf_base = NULL;
	    }
	  _IO_setb(fp, new_buf, new_buf + new_size, 1);
	  fp->_IO_write_base = new_buf;
	}
      fp->_IO_write_end = fp->_IO_buf_end;
    }

  fp->_IO_write_ptr = fp->_IO_buf_base + pos;

  fp->_IO_read_base = fp->_IO_buf_base;
  fp->_IO_read_ptr = fp->_IO_buf_base + get_pos;
  fp->_IO_read_end = fp->_IO_buf_base + LEN(fp);

  if (!flush_only)
    *fp->_IO_write_ptr++ = (unsigned char) c;
  return c;
}

int
DEFUN(_IO_str_underflow, (fp),
      register _IO_FILE* fp)
{
  _IO_size_t ppos = fp->_IO_write_ptr - fp->_IO_write_base;
  if (ppos > LEN(fp)) LEN(fp) = ppos;
  if ((fp->_flags & _IO_TIED_PUT_GET) && (fp->_flags & _IO_CURRENTLY_PUTTING))
    {
      fp->_flags &= ~_IO_CURRENTLY_PUTTING;
      fp->_IO_write_ptr = fp->_IO_write_end;
    }
  fp->_IO_read_end = fp->_IO_read_base + LEN(fp);
  if (fp->_IO_read_ptr < fp->_IO_read_end)
    return *fp->_IO_read_ptr;
  else
    return EOF;
}

_IO_ssize_t
DEFUN(_IO_str_count, (fp),
      register _IO_FILE *fp)
{
  _IO_ssize_t put_len = fp->_IO_write_ptr - fp->_IO_write_base;
  if (put_len < LEN(fp))
    put_len = LEN(fp);
  return put_len;
}     

_IO_pos_t
DEFUN(_IO_str_seekoff, (fp, offset, dir, mode),
      register _IO_FILE *fp AND _IO_off_t offset AND int dir AND int mode)
{
  _IO_ssize_t cur_size = _IO_str_count(fp);
  _IO_pos_t new_pos = EOF;

  /* Move the get pointer, if requested. */
  if (mode & _IOS_INPUT)
    {
      switch (dir)
	{
	case _IO_seek_end:
	  offset += cur_size;
	  break;
	case _IO_seek_cur:
	  offset += fp->_IO_read_ptr - fp->_IO_read_base;
	  break;
	default: /* case _IO_seek_set: */
	  break;
	}
      if (offset < 0 || (_IO_size_t)offset > cur_size)
	return EOF;
      fp->_IO_read_ptr = fp->_IO_read_base + offset;
      fp->_IO_read_end = fp->_IO_read_base + cur_size;
      new_pos = offset;
    }

  /* Move the put pointer, if requested. */
  if (mode & _IOS_OUTPUT)
    {
      switch (dir)
	{
	case _IO_seek_end:
	  offset += cur_size;
	  break;
	case _IO_seek_cur:
	  offset += fp->_IO_write_ptr - fp->_IO_write_base;
	  break;
	default: /* case _IO_seek_set: */
	  break;
	}
      if (offset < 0 || (_IO_size_t)offset > cur_size)
	return EOF;
      LEN(fp) = cur_size;
      fp->_IO_write_ptr = fp->_IO_write_base + offset;
      new_pos = offset;
    }
  return new_pos;
}

int
DEFUN(_IO_str_pbackfail, (fp, c),
      register _IO_FILE *fp AND int c)
{
  if ((fp->_flags & _IO_NO_WRITES) && c != EOF)
    return EOF;
  return _IO_default_pbackfail(fp, c);
}

void
DEFUN (_IO_str_finish, (fp),
      register _IO_FILE* fp)
{
  if (fp->_IO_buf_base && !(fp->_flags & _IO_USER_BUF))
    (((_IO_strfile*)fp)->_s._free_buffer)(fp->_IO_buf_base);
  fp->_IO_buf_base = NULL;

  _IO_default_finish(fp);
}

struct _IO_jump_t _IO_str_jumps = {
  JUMP_INIT_DUMMY,
  JUMP_INIT(finish, _IO_str_finish),
  JUMP_INIT(overflow, _IO_str_overflow),
  JUMP_INIT(underflow, _IO_str_underflow),
  JUMP_INIT(uflow, _IO_default_uflow),
  JUMP_INIT(pbackfail, _IO_str_pbackfail),
  JUMP_INIT(xsputn, _IO_default_xsputn),
  JUMP_INIT(xsgetn, _IO_default_xsgetn),
  JUMP_INIT(seekoff, _IO_str_seekoff),
  JUMP_INIT(seekpos, _IO_default_seekpos),
  JUMP_INIT(setbuf, _IO_default_setbuf),
  JUMP_INIT(sync, _IO_default_sync),
  JUMP_INIT(doallocate, _IO_default_doallocate),
  JUMP_INIT(read, _IO_default_read),
  JUMP_INIT(write, _IO_default_write),
  JUMP_INIT(seek, _IO_default_seek),
  JUMP_INIT(close, _IO_default_close),
  JUMP_INIT(stat, _IO_default_stat)
};
