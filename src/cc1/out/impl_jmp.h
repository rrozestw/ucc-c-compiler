#ifndef IMPL_JMP_H
#define IMPL_JMP_H

/* this is the implementation backend which handles flow control. the front
 * part of the backend (out.c and co) don't use this, it's just used by the
 * blk.c flow logic part */

void impl_jmp(FILE *f, const char *lbl);

#endif
