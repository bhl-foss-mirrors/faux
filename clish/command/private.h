/*
 * command.h
 */
#include "clish/command.h"

/*---------------------------------------------------------
 * PRIVATE TYPES
 *--------------------------------------------------------- */
struct clish_command_s {
	lub_bintree_node_t bt_node;
	char *name;
	char *text;
	clish_paramv_t *paramv;
	char *action;
	clish_view_t *view;
	char *viewid;
	char *detail;
	char *escape_chars;
	clish_param_t *args;
	const struct clish_command_s * link;
	clish_view_t *alias_view;
	char *alias;
	clish_view_t *pview;
	bool_t lock;
	bool_t interrupt;
	bool_t dynamic; /* Is command dynamically created */
	clish_var_expand_fn_t *var_expand_fn;

	/* ACTION params: */
	char *builtin;
	char *shebang;

	/* CONFIG params:
	 * TODO: create special structure for CONFIG params.
	 */
	clish_config_operation_t cfg_op;
	unsigned short priority;
	char *pattern;
	char *file;
	bool_t splitter;
	char *seq;
	bool_t unique;
	char *cfg_depth;
};
