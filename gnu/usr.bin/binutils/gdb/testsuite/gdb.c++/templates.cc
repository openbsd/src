/* This test code is from Wendell Baker (wbaker@comet.berkeley.edu) */

#include <stddef.h>

int a_i;
char a_c;
double a_d;

typedef void *Pix;

int
f(int i)
{ return 0; }

int
f(int i, char c)
{ return 0; }

int
f(int i, char c, double d)
{ return 0; }

int
f(int i, char c, double d, char *cs)
{ return 0; }

int
f(int i, char c, double d, char *cs, void (*fig)(int, char))
{ return 0; }

int
f(int i, char c, double d, char *cs, void (*fig)(char, int))
{ return 0; }

class R {
public:
    int i;
};
class S {
public:
    int i;
};
class T {
public:
    int i;
};

char g(char, const char, volatile char)
{ return 'c'; }
char g(R, char&, const char&, volatile char&)
{ return 'c'; }
char g(char*, const char*, volatile char*)
{ return 'c'; }
char g(S, char*&, const char*&, volatile char*&)
{ return 'c'; }

signed char g(T,signed char, const signed char, volatile signed char)
{ return 'c'; }
signed char g(T, R, signed char&, const signed char&, volatile signed char&)
{ return 'c'; }
signed char g(T, signed char*, const signed char*, volatile signed char*)
{ return 'c'; }
signed char g(T, S, signed char*&, const signed char*&, volatile signed char*&)
{ return 'c'; }

unsigned char g(unsigned char, const unsigned char, volatile unsigned char)
{ return 'c'; }
unsigned char g(R, unsigned char&, const unsigned char&, volatile unsigned char&)
{ return 'c'; }
unsigned char g(unsigned char*, const unsigned char*, volatile unsigned char*)
{ return 'c'; }
unsigned char g(S, unsigned char*&, const unsigned char*&, volatile unsigned char*&)
{ return 'c'; }

short g(short, const short, volatile short)
{ return 0; }
short g(R, short&, const short&, volatile short&)
{ return 0; }
short g(short*, const short*, volatile short*)
{ return 0; }
short g(S, short*&, const short*&, volatile short*&)
{ return 0; }

signed short g(T, signed short, const signed short, volatile signed short)
{ return 0; }
signed short g(T, R, signed short&, const signed short&, volatile signed short&)
{ return 0; }
signed short g(T, signed short*, const signed short*, volatile signed short*)
{ return 0; }
signed short g(T, S, double, signed short*&, const signed short*&, volatile signed short*&)
{ return 0; }

unsigned short g(unsigned short, const unsigned short, volatile unsigned short)
{ return 0; }
unsigned short g(R, unsigned short&, const unsigned short&, volatile unsigned short&)
{ return 0; }
unsigned short g(unsigned short*, const unsigned short*, volatile unsigned short*)
{ return 0; }
unsigned short g(S, unsigned short*&, const unsigned short*&, volatile unsigned short*&)
{ return 0; }

int g(int, const int, volatile int)
{ return 0; }
int g(R, int&, const int&, volatile int&)
{ return 0; }
int g(int*, const int*, volatile int*)
{ return 0; }
int g(S, int*&, const int*&, volatile int*&)
{ return 0; }

signed int g(T, signed int, const signed int, volatile signed int)
{ return 0; }
signed int g(T, R, signed int&, const signed int&, volatile signed int&)
{ return 0; }
signed int g(T, signed int*, const signed int*, volatile signed int*)
{ return 0; }
signed int g(T, S, signed int*&, const signed int*&, volatile signed int*&)
{ return 0; }

unsigned int g(unsigned int, const unsigned int, volatile unsigned int)
{ return 0; }
unsigned int g(R, unsigned int&, const unsigned int&, volatile unsigned int&)
{ return 0; }
unsigned int g(unsigned int*, const unsigned int*, volatile unsigned int*)
{ return 0; }
unsigned int g(S, unsigned int*&, const unsigned int*&, volatile unsigned int*&)
{ return 0; }

long g(long, const long, volatile long)
{ return 0; }
long g(R, long&, const long&, volatile long&)
{ return 0; }
long g(long*, const long*, volatile long*)
{ return 0; }
long g(S, long*&, const long*&, volatile long*&)
{ return 0; }

signed long g(T, signed long, const signed long, volatile signed long)
{ return 0; }
signed long g(T, R, signed long&, const signed long&, volatile signed long&)
{ return 0; }
signed long g(T, signed long*, const signed long*, volatile signed long*)
{ return 0; }
signed long g(T, S, signed long*&, const signed long*&, volatile signed long*&)
{ return 0; }

unsigned long g(unsigned long, const unsigned long, volatile unsigned long)
{ return 0; }
unsigned long g(S, unsigned long&, const unsigned long&, volatile unsigned long&)
{ return 0; }
unsigned long g(unsigned long*, const unsigned long*, volatile unsigned long*)
{ return 0; }
unsigned long g(S, unsigned long*&, const unsigned long*&, volatile unsigned long*&)
{ return 0; }

#ifdef __GNUC__
long long g(long long, const long long, volatile long long)
{ return 0; }
long long g(S, long long&, const long long&, volatile long long&)
{ return 0; }
long long g(long long*, const long long*, volatile long long*)
{ return 0; }
long long g(R, long long*&, const long long*&, volatile long long*&)
{ return 0; }

signed long long g(T, signed long long, const signed long long, volatile signed long long)
{ return 0; }
signed long long g(T, R, signed long long&, const signed long long&, volatile signed long long&)
{ return 0; }
signed long long g(T, signed long long*, const signed long long*, volatile signed long long*)
{ return 0; }
signed long long g(T, S, signed long long*&, const signed long long*&, volatile signed long long*&)
{ return 0; }

unsigned long long g(unsigned long long, const unsigned long long, volatile unsigned long long)
{ return 0; }
unsigned long long g(R, unsigned long long*, const unsigned long long*, volatile unsigned long long*)
{ return 0; }
unsigned long long g(unsigned long long&, const unsigned long long&, volatile unsigned long long&)
{ return 0; }
unsigned long long g(S, unsigned long long*&, const unsigned long long*&, volatile unsigned long long*&)
{ return 0; }
#endif

float g(float, const float, volatile float)
{ return 0; }
float g(char, float&, const float&, volatile float&)
{ return 0; }
float g(float*, const float*, volatile float*)
{ return 0; }
float g(char, float*&, const float*&, volatile float*&)
{ return 0; }

double g(double, const double, volatile double)
{ return 0; }
double g(char, double&, const double&, volatile double&)
{ return 0; }
double g(double*, const double*, volatile double*)
{ return 0; }
double g(char, double*&, const double*&, volatile double*&)
{ return 0; }

#ifdef __GNUC__
long double g(long double, const long double, volatile long double)
{ return 0; }
long double g(char, long double&, const long double&, volatile long double&)
{ return 0; }
long double g(long double*, const long double*, volatile long double*)
{ return 0; }
long double g(char, long double*&, const long double*&, volatile long double*&)
{ return 0; }
#endif

class c {
public:
    c(int) {};
    int i;
};

class c g(c, const c, volatile c)
{ return 0; }
c g(char, c&, const c&, volatile c&)
{ return 0; }
c g(c*, const c*, volatile c*)
{ return 0; }
c g(char, c*&, const c*&, volatile c*&)
{ return 0; }

void h(char = 'a')
{ }
void h(char, signed char = 'a')
{ }
void h(unsigned char = 'a')
{ }

void h(short = 43)
{ }
void h(char, signed short = 43)
{ }
void h(unsigned short = 43)
{ }

void h(int = 43)
{ }
void h(char, signed int = 43)
{ }
void h(unsigned int = 43)
{ }

void h(long = 43)
{ }
void h(char, signed long = 43)
{ }
void h(unsigned long = 43)
{ }

#ifdef __GNUC__
void h(long long = 43)
{ }
void h(char, signed long long = 43)
{ }
void h(unsigned long long = 43)
{ }
#endif

void h(float = 4.3e-10)
{ }
void h(double = 4.3)
{ }
#ifdef __GNUC__
void h(long double = 4.33e33)
{ }
#endif

void printf(const char *format, ... )
{
    // elipsis
}

class T1 {
public:
    static void* operator new(size_t);
    static void operator delete(void *pointer);

    void operator=(const T1&);
    T1& operator=(int);

    int operator==(int) const;
    int operator==(const T1&) const;
    int operator!=(int) const;
    int operator!=(const T1&) const;

    int operator<=(int) const;
    int operator<=(const T1&) const;
    int operator<(int) const;
    int operator<(const T1&) const;
    int operator>=(int) const;
    int operator>=(const T1&) const;
    int operator>(int) const;
    int operator>(const T1&) const;

    void operator+(int) const;
    T1& operator+(const T1&) const;
    void operator+=(int) const;
    T1& operator+=(const T1&) const;

    T1& operator++() const;

    void operator-(int) const;
    T1& operator-(const T1&) const;
    void operator-=(int) const;
    T1& operator-=(const T1&) const;

    T1& operator--() const;

    void operator*(int) const;
    T1& operator*(const T1&) const;
    void operator*=(int) const;
    T1& operator*=(const T1&) const;

    void operator/(int) const;
    T1& operator/(const T1&) const;
    void operator/=(int) const;
    T1& operator/=(const T1&) const;

    void operator%(int) const;
    T1& operator%(const T1&) const;
    void operator%=(int) const;
    T1& operator%=(const T1&) const;

    void operator&&(int) const;
    T1& operator&&(const T1&) const;

    void operator||(int) const;
    T1& operator||(const T1&) const;

    void operator&(int) const;
    T1& operator&(const T1&) const;
    void operator&=(int) const;
    T1& operator&=(const T1&) const;

    void operator|(int) const;
    T1& operator|(const T1&) const;
    void operator|=(int) const;
    T1& operator|=(const T1&) const;

    void operator^(int) const;
    T1& operator^(const T1&) const;
    void operator^=(int) const;
    T1& operator^=(const T1&) const;

    T1& operator!() const;
    T1& operator~() const;
};

void* 
T1::operator new(size_t)
{ return 0; }

void
T1::operator delete(void *pointer)
{ }

class T2 {
public:
    T2(int i): integer(i)
	{ }
    int integer;
};

int operator==(const T2&, const T2&)
{ return 0; }
int operator==(const T2&, char)
{ return 0; }
int operator!=(const T2&, const T2&)
{ return 0; }
int operator!=(const T2&, char)
{ return 0; }

int operator<=(const T2&, const T2&)
{ return 0; }
int operator<=(const T2&, char)
{ return 0; }
int operator<(const T2&, const T2&)
{ return 0; }
int operator<(const T2&, char)
{ return 0; }
int operator>=(const T2&, const T2&)
{ return 0; }
int operator>=(const T2&, char)
{ return 0; }
int operator>(const T2&, const T2&)
{ return 0; }
int operator>(const T2&, char)
{ return 0; }

T2 operator+(const T2 t, int i)
{ return t.integer + i; }
T2 operator+(const T2 a, const T2& b)
{ return a.integer + b.integer; }
T2& operator+=(T2& t, int i)
{ t.integer += i; return t; }
T2& operator+=(T2& a, const T2& b)
{ a.integer += b.integer; return a; }

T2 operator-(const T2 t, int i)
{ return t.integer - i; }
T2 operator-(const T2 a, const T2& b)
{ return a.integer - b.integer; }
T2& operator-=(T2& t, int i)
{ t.integer -= i; return t; }
T2& operator-=(T2& a, const T2& b)
{ a.integer -= b.integer; return a; }

T2 operator*(const T2 t, int i)
{ return t.integer * i; }
T2 operator*(const T2 a, const T2& b)
{ return a.integer * b.integer; }
T2& operator*=(T2& t, int i)
{ t.integer *= i; return t; }
T2& operator*=(T2& a, const T2& b)
{ a.integer *= b.integer; return a; }

T2 operator/(const T2 t, int i)
{ return t.integer / i; }
T2 operator/(const T2 a, const T2& b)
{ return a.integer / b.integer; }
T2& operator/=(T2& t, int i)
{ t.integer /= i; return t; }
T2& operator/=(T2& a, const T2& b)
{ a.integer /= b.integer; return a; }

T2 operator%(const T2 t, int i)
{ return t.integer % i; }
T2 operator%(const T2 a, const T2& b)
{ return a.integer % b.integer; }
T2& operator%=(T2& t, int i)
{ t.integer %= i; return t; }
T2& operator%=(T2& a, const T2& b)
{ a.integer %= b.integer; return a; }

template<class T>
class T5 {
public:
    T5(int);
    T5(const T5<T>&);
    ~T5();
    static void* operator new(size_t);
    static void operator delete(void *pointer);
    int value();
    
    static T X;
    T x;
    int val;
};

template<class T>
T5<T>::T5(int v)
{ val = v; }

template<class T>
T5<T>::T5(const T5<T>&)
{}

template<class T>
T5<T>::~T5()
{}

template<class T>
void*
T5<T>::operator new(size_t)
{ return 0; }

template<class T>
void
T5<T>::operator delete(void *pointer)
{ }

template<class T>
int
T5<T>::value()
{ return val; }

#if ! defined(__GNUC__) || defined(GCC_BUG)
template<class T>
T5<T>::T T5<T>::X;
#endif

T5<char> t5c(1);
T5<int> t5i(2);
T5<int (*)(char, void *)> t5fi1(3);
T5<int (*)(int, double **, void *)> t5fi2(4);

class x {
public:
    int (*manage[5])(double,
		     void *(*malloc)(unsigned size),
		     void (*free)(void *pointer));
    int (*device[5])(int open(const char *, unsigned mode, unsigned perms, int extra = 0), 
		     int *(*read)(int fd, void *place, unsigned size),
		     int *(*write)(int fd, void *place, unsigned size),
		     void (*close)(int fd));
};
T5<x> t5x(5);

#if !defined(__GNUC__) || (__GNUC__ >= 2 && __GNUC_MINOR__ >= 6)
template class T5<char>;
template class T5<int>;
template class T5<int (*)(char, void *)>;
template class T5<int (*)(int, double **, void *)>;
template class T5<x>;
#endif

class T7 {
public:
    static int get();
    static void put(int);
};

int
T7::get()
{ return 1; }

void
T7::put(int i)
{
    // nothing
}

main()
{
    int i;
#ifdef usestubs
    set_debug_traps();
    breakpoint();
#endif
    i = i + 1;
}
