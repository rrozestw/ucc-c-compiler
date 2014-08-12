
static void out_memcpy_single(void)
{
	static type *t1;

	if(!t1)
		t1 = type_nav_btype(cc1_type_nav, type_intptr_t);

	/* ds */

	out_swap(); // sd
	out_dup();  // sdd
	out_pulltop(2); // dds

	out_dup();      /* ddss */
	out_deref();    /* dds. */
	out_pulltop(2); /* ds.d */
	out_swap();     /* dsd. */
	out_store();    /* ds. */
	out_pop();      /* ds */

	out_push_l(t1, 1); /* ds1 */
	out_op(op_plus);   /* dS */

	out_swap();        /* Sd */
	out_push_l(t1, 1); /* Sd1 */
	out_op(op_plus);   /* SD */

	out_swap(); /* DS */
}

void out_memcpy(unsigned long bytes)
{
	type *tptr;
	unsigned tptr_sz;
	unsigned counter_off, counter_space;
	type *ty_ulong;

	out_comment("builtin-memcpy(%ld)", bytes);

	if(bytes == 0)
		return;

	tptr = type_ptr_to(type_nav_MAX_FOR(cc1_type_nav, bytes));
	tptr_sz = type_size(tptr, NULL);

	ty_ulong = type_nav_btype(cc1_type_nav, type_ulong);

	/* currently ds (= dest,src) */

	/* save dest pointer */
	out_swap(), /* sd */
		out_dup(), /* sdd */
		out_pulltop(2); /* implicit(d) ds */

	counter_space = v_alloc_stack(
			platform_word_size(),
			"memcpy_counter");

	counter_off = v_stack_sz();
	vpush(type_ptr_to(ty_ulong));
	v_set_stack(vtop, NULL, -(long)counter_off, /*lval:*/0);
	out_dup(); /* sdii */

	out_push_l(ty_ulong, 0); /* sdii0 */
	out_store(); /* sdiI */
	out_pop(); /* sdi */

	out_pulltop(2); /* sid */
	out_pulltop(2); /* ids */

	while(bytes > 0){
		char *lbl = out_label_code("builtin_memcpy");
		unsigned long nloops = bytes / tptr_sz;

		/* as many copies as we can */
		out_change_type(tptr);
		out_swap();
		out_change_type(tptr);
		out_swap();

		out_label(lbl);

		out_memcpy_single();

		out_pulltop(2); /* dsi */
		out_dup(); /* dsii */
		out_dup(); /* dsii */
		out_deref(); /* dsiiI */
		out_push_l(ty_ulong, 1); /* dsiiI1 */
		out_op(op_plus); /* dsiiI */
		out_store(); /* dsiI */

		out_push_l(ty_ulong, nloops); /* dsiIN */
		out_comment("comparing with nloops=%d", (int)nloops);

		out_op(op_lt); /* dsiB */
		out_jtrue(lbl); /* dsi */

		out_pulltop(2); /* sid */
		/* TODO: increment dest pointer */
		out_pulltop(2); /* ids */
		/* TODO: increment src pointer */

		bytes %= tptr_sz;

		if(bytes > 0){
			tptr_sz /= 2;
			tptr = type_ptr_to(type_nav_MAX_FOR(cc1_type_nav, tptr_sz));
		}

		free(lbl);
	}
	/* ids */
	out_pulltop(2); /* dsi */
	out_pop(); /* ds */

	v_dealloc_stack(counter_space);

	/* restore saved dest pointer */
	out_swap(); /* sd */
	out_pop(); /* s (implicit dest from before) */
}
