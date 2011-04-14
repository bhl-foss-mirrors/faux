/*
 * param.h
 */
#include "clish/param.h"

/*---------------------------------------------------------
 * PRIVATE TYPES
 *--------------------------------------------------------- */

struct clish_paramv_s {
	unsigned paramc;
	clish_param_t **paramv;
};

struct clish_param_s {
	char *name;
	char *text;
	char *value;
	clish_ptype_t *ptype;	/* The type of this parameter */
	char *defval;		/* default value to use for this parameter */
	clish_paramv_t *paramv;
	clish_param_mode_e mode;
	bool_t optional;
	bool_t hidden;
	char *test; /* the condition */
	clish_var_expand_fn_t *var_expand_fn;
};
