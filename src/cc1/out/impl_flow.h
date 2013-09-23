#ifndef IMPL_FLOW_H
#define IMPL_FLOW_H

void impl_lbl(FILE *, const char *);
void impl_jmp(FILE *, const char *lbl);

void impl_jcond(
		FILE *, const struct vstack *,
		const char *ltrue, const char *lfalse);

#endif
