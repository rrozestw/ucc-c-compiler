// RUN: %ucc -c %s
typedef int tdef_int;

tdef_int i;

typedef tdef_int *ptr_to_tdef_int;

ptr_to_tdef_int p;

typedef ptr_to_tdef_int (*func_returning_ptr_to_int)();

func_returning_ptr_to_int f;

typeof(f) *ptr_to_f_ret_ptr_to_int;

typeof(1 + 2 * 3) an_int;

typeof(q()) implicit_int;
