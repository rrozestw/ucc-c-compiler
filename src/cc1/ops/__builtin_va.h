#ifndef __BUILTIN_VA_H
#define __BUILTIN_VA_H

#define BUILTIN_VA(nam) expr *parse_va_ ##nam(void);
#  include "__builtin_va.def"
#undef BUILTIN_VA

#endif
