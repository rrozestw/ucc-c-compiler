// http://stackoverflow.com/questions/5641836/c-preprocessor-recursive-macros
#define CAT_I(a, b) a ## b
#define CAT(a, b) CAT_I(a, b)

#define M_0 CAT(x, y)
#define M_1 whatever_else
#define M(a) CAT(M_, a)
M(0);       //  expands to CAT(x, y)

#define N_0() CAT(x, y)
#define N_1() whatever_else
#define N(a) CAT(N_, a)()
N(0);       //  expands to xy _OR_ CAT(x, y)





// and another:
#define X 10
1e-X // shouldn't be replaced
