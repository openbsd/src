/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef NINT_H
#define NINT_H

#pragma pack(1)

class nint16
{
private:
	int16_t value;

public:
	nint16()
	{
	}

	nint16(int16_t x)
	{
		value = htons(x);
	};

	nint16(const nint16& x)
	{
		value = x.value;
	};

	operator int16_t() const
	{
		return ntohs(value);
	}

	nint16& operator +=(int16_t x)
	{
		value = htons(ntohs(value) + x);
		return *this;
	}

	void setraw(int16_t x)
	{
		value = x;
	}

	int16_t getraw() const
	{
		return value;
	}
};

class nuint16
{
private:
	u_int16_t value;

public:
	nuint16()
	{
	}

	nuint16(u_int16_t x)
	{
		value = htons(x);
	};

	nuint16(const nuint16& x)
	{
		value = x.value;
	};

	nuint16& operator +=(u_int16_t x)
	{
		value = htons(ntohs(value) + x);
		return *this;
	}

	operator u_int16_t() const
	{
		return ntohs(value);
	}

	void setraw(u_int16_t x)
	{
		value = x;
	}

	u_int16_t getraw() const
	{
		return value;
	}
};

class nint32
{
private:
	int32_t value;

public:
	nint32()
	{
	}

	nint32(int32_t x)
	{
		value = htonl(x);
	};

	nint32(const nint32& x)
	{
		value = x.value;
	};

	operator int32_t() const
	{
		return ntohl(value);
	}

	void setraw(int32_t x)
	{
		value = x;
	}

	int32_t getraw() const
	{
		return value;
	}
};

class nuint32
{
private:
	u_int32_t value;

public:
	nuint32()
	{
	}

	nuint32(u_int32_t x)
	{
		value = htonl(x);
	};

	nuint32(const nuint32& x)
	{
		value = x.value;
	};

	operator u_int32_t() const
	{
		return ntohl(value);
	}

	void setraw(u_int32_t x)
	{
		value = x;
	}

	u_int32_t getraw() const
	{
		return value;
	}
};

#pragma pack()

#endif // NINT_H
