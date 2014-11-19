#define NULL 0
#define UCC_ASSERT(b, ...) do if(!(b)) ICE(__VA_ARGS__); while(0)
#define ICE(...) ice(__FILE__, __LINE__, __func__, __VA_ARGS__)

f(char *new)
{
	UCC_ASSERT(new, "dynarray_nochk_add(): adding NULL");
}
