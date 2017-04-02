#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "../util/util.h"
#include "../util/alloc.h"

#include "parse_attr.h"

#include "tokenise.h"
#include "tokconv.h"

#include "fold.h"

#include "cc1_where.h"
#include "warn.h"

#include "fold.h"

#include "parse_expr.h"

static void parse_attr_bracket_chomp(int had_open_paren);

static attribute *parse_attr_format()
{
	/* __attribute__((format (printf, fmtarg, firstvararg))) */
	attribute *da;
	char *func;
	enum fmt_type fmt;

	EAT(token_open_paren);

	func = token_current_spel();
	EAT(token_identifier);

	/* TODO: token_current_spel()
	 * and token_get_current_str(..,..)
	 * checks everywhere */
	if(!func)
		return NULL;

#define CHECK(s) !strcmp(func, s) || !strcmp(func, "__" s "__")
	if(CHECK("printf")){
		fmt = attr_fmt_printf;
	}else if(CHECK("scanf")){
		fmt = attr_fmt_scanf;
	}else{
		cc1_warn_at(NULL, attr_format_unknown,
				"unknown format func \"%s\"", func);
		parse_attr_bracket_chomp(1);
		return NULL;
	}

	da = attribute_new(attr_format);
	da->bits.format.fmt_func = fmt;

	EAT(token_comma);

	da->bits.format.fmt_idx = currentval.val.i - 1;
	EAT(token_integer);

	EAT(token_comma);

	da->bits.format.var_idx = currentval.val.i - 1;
	EAT(token_integer);

	EAT(token_close_paren);

	return da;
}

static attribute *parse_attr_section()
{
	/* __attribute__((section ("sectionname"))) */
	attribute *da;
	char *func;
	size_t len, i;

	EAT(token_open_paren);

	if(curtok != token_string)
		die_at(NULL, "string expected for section");

	token_get_current_str(&func, &len, NULL, NULL);
	EAT(token_string);

	for(i = 0; i < len; i++)
		if(!isprint(func[i])){
			if(i < len - 1 || func[i] != '\0')
				cc1_warn_at(NULL, attr_section_badchar,
						"character 0x%x detected in section", func[i]);
			break;
		}

	da = attribute_new(attr_section);

	da->bits.section = func;

	EAT(token_close_paren);

	return da;
}

static attribute *parse_attr_nonnull()
{
	/* __attribute__((nonnull(1, 2, 3, 4...)))
	 * or
	 * __attribute__((nonnull)) - all args
	 */
	attribute *da = attribute_new(attr_nonnull);
	unsigned long l = 0;
	int had_error = 0;

	if(accept(token_open_paren)){
		while(curtok != token_close_paren){
			if(curtok == token_integer){
				int n = currentval.val.i;
				if(n <= 0){
					/* shouldn't ever be negative */
					cc1_warn_at(NULL,
							attr_nonnull_bad,
							"%s nonnull argument ignored", n < 0 ? "negative" : "zero");
					had_error = 1;
				}else{
					/* implicitly disallow functions with >32 args */
					/* n-1, since we convert from 1-base to 0-base */
					l |= 1 << (n - 1);
				}
			}else{
				EAT(token_integer); /* raise error */
			}
			EAT(curtok);

			if(accept(token_comma))
				continue;
			break;
		}
		EAT(token_close_paren);
	}

	/* if we had an error, go with what we've got, (even if it's nothing), to avoid spurious warnings */
	da->bits.nonnull_args = (l || had_error) ? l : ~0UL; /* all if 0 */

	return da;
}

static expr *optional_parened_expr(symtable *scope)
{
	if(accept(token_open_paren)){
		expr *e;

		if(accept(token_close_paren))
			goto out;

		e = PARSE_EXPR_NO_COMMA(scope, 0);
		FOLD_EXPR(e, scope);

		FOLD_EXPR(e, scope);

		EAT(token_close_paren);

		return e;
	}
out:
	return NULL;
}

static attribute *parse_attr_sentinel(symtable *scope, const char *ident)
{
	attribute *da = attribute_new(attr_sentinel);

	(void)ident;

  da->bits.sentinel = optional_parened_expr(scope);

	return da;
}

static attribute *parse_attr_aligned(symtable *scope, const char *ident)
{
	attribute *da = attribute_new(attr_aligned);

	(void)ident;

  da->bits.align = optional_parened_expr(scope);

	return da;
}

static attribute *parse_attr_cleanup(symtable *scope, const char *ident)
{
	decl *d;
	char *sp;
	where ident_loc;
	attribute *attr;

	(void)ident;

	EAT(token_open_paren);

	if(curtok != token_identifier)
		die_at(NULL, "identifier expected for cleanup function");

	where_cc1_current(&ident_loc);
	sp = token_current_spel();
	EAT(token_identifier);

	d = symtab_search_d(scope, sp, NULL);
	if(!d)
		die_at(&ident_loc, "function '%s' not found", sp);

	attr = attribute_new(attr_cleanup);
	attr->bits.cleanup = d;

	EAT(token_close_paren);

	return attr;
}

#define EMPTY(t)                                \
static attribute *parse_ ## t(                  \
		symtable *symtab, const char *ident)        \
{                                               \
	(void)symtab;                                 \
	(void)ident;                                  \
	return attribute_new(t);                      \
}

EMPTY(attr_unused)
EMPTY(attr_warn_unused)
EMPTY(attr_enum_bitmask)
EMPTY(attr_noreturn)
EMPTY(attr_noderef)
EMPTY(attr_packed)
EMPTY(attr_weak)
EMPTY(attr_always_inline)
EMPTY(attr_noinline)
EMPTY(attr_ucc_debug)
EMPTY(attr_desig_init)

#undef EMPTY

static attribute *parse_attr_call_conv(symtable *symtab, const char *ident)
{
	attribute *a = attribute_new(attr_call_conv);

	(void)symtab;

	/**/ if(!strcmp(ident, "cdecl"))
		a->bits.conv = conv_cdecl;
	else if(!strcmp(ident, "stdcall"))
		a->bits.conv = conv_stdcall;
	else if(!strcmp(ident, "fastcall"))
		a->bits.conv = conv_fastcall;
	else
		assert(0 && "unreachable");

	return a;
}

static struct
{
	const char *ident;
	attribute *(*parser)(symtable *, const char *ident);
} attrs[] = {
#define NAME(x) { #x, parse_attr_ ## x },
#define ALIAS(s, x) { s, parse_attr_ ## x },
#define EXTRA_ALIAS(s, x) { s, parse_attr_ ## x},
	ATTRIBUTES
#undef NAME
#undef ALIAS
#undef EXTRA_ALIAS

	{ NULL, NULL },
};
#define MAX_FMT_LEN 16

static void parse_attr_bracket_chomp(int had_open_paren)
{
	if(had_open_paren || accept(token_open_paren)){
		for(;;){
			if(accept(token_open_paren))
				parse_attr_bracket_chomp(1); /* nest */

			if(accept(token_close_paren))
				break;
			else if(curtok == token_eof)
				break; /* failsafe */

			EAT(curtok);
		}
	}
}

static attribute *parse_attr_single(const char *ident, symtable *scope)
{
	symtable_global *glob;
	int i;

	for(i = 0; attrs[i].ident; i++){
		char buf[MAX_FMT_LEN];
		if(!strcmp(attrs[i].ident, ident)
		|| (snprintf(buf, sizeof buf, "__%s__", attrs[i].ident), !strcmp(buf, ident)))
		{
			return attrs[i].parser(scope, attrs[i].ident);
		}
	}

	/* unrecognised - only do the warning (and map checking) if non system-header */
	if(!where_in_sysheader(where_cc1_current(NULL))){
		glob = symtab_global(scope);
		if(!dynmap_exists(char *, glob->unrecog_attrs, (char *)ident)){
			char *dup = ustrdup(ident);

			if(!glob->unrecog_attrs)
				glob->unrecog_attrs = dynmap_new(char *, strcmp, dynmap_strhash);

			dynmap_set(char *, void *, glob->unrecog_attrs, dup, NULL);

			cc1_warn_at(NULL, attr_unknown,
					"ignoring unrecognised attribute \"%s\"", ident);
		}
	}

	/* if there are brackets, eat them all */
	parse_attr_bracket_chomp(0);

	return NULL;
}

attribute *parse_attr(symtable *scope)
{
	attribute *attr = NULL, **next = &attr;

	for(;;){
		attribute *this;
		where w;
		int alloc;
		char *ident = curtok_to_identifier(&alloc);

		if(!ident){
			parse_had_error = 1;
			warn_at_print_error(NULL,
					"identifier expected for attribute (got %s)",
					token_to_str(curtok));
			EAT(curtok);
			goto comma;
		}

		where_cc1_current(&w);
		where_cc1_adj_identifier(&w, ident);

		EAT(curtok);

		if((this = *next = parse_attr_single(ident, scope))){
			memcpy_safe(&this->where, &w);
			next = &(*next)->next;
		}

		if(alloc)
			free(ident);

comma:
		if(!accept(token_comma))
			break;
	}

	return attr;
}
