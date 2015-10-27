typedef _Bool bool;

double sqrt(double);
int printf(const char *, ...);

#ifdef __TINYC__

//This should be in sync with the declaration on our lib/libtcc1.c
/* GCC compatible definition of va_list. */
typedef struct {
    unsigned int gp_offset;
    unsigned int fp_offset;
    union {
        unsigned int overflow_offset;
        char *overflow_arg_area;
    };
    char *reg_save_area;
} __va_list_struct;

typedef __va_list_struct va_list[1];

void __va_start(__va_list_struct *ap, void *fp);
void *__va_arg(__va_list_struct *ap, int arg_type, int size, int align);

#define va_start(ap, last) __va_start(ap, __builtin_frame_address(0))
#define va_arg(ap, type)                                                \
    (*(type *)(__va_arg(ap, __builtin_va_arg_types(type), sizeof(type), __alignof__(type))))
#define va_copy(dest, src) (*(dest) = *(src))
#define va_end(ap)

#else

#  define va_start __builtin_va_start
#  define va_arg __builtin_va_arg
#  define va_end __builtin_va_end
typedef __builtin_va_list va_list;

#endif


double sub();

double one()
{
	return 1;
}

double zero()
{
	return sub(one(), one());
}

double nan()
{
	return one() / zero();
}

double add(double a, ...)
{
	va_list l;
	va_start(l, a);

	double x;
	while((x = va_arg(l, double)) != nan()){
		a += x;
	}

	va_end(l);

	return a;
}

double sub(double a, double b)
{
	return a - b;
}

double mul(double a, double b)
{
	return a * b;
} 

double div(double a, double b)
{
	return a / b;
}

bool equal(double a, double b)
{
	return a == b;
}

double phi()
{
	return div(add(one(), sqrt(add(one(), one(), one(), one(), one(), nan())), nan()),
			add(one(), one(), nan()));
}

double psi()
{
	return sub(one(), phi());
}

double exp(double a, double b)
{
	if(equal(b, zero())){
		return one();
	}else{
		return mul(a, exp(a, sub(b, one())));
	}
}

int fib(int n)
{
	return div(sub(exp(phi(), n), exp(psi(), n)), sub(phi(), psi()));
}

int main()
{
	for(int i = 0; i < 10; i++)
		printf("fib(%d) = %d\n", i, fib(i));
	return 0;
}
