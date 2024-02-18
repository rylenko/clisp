#ifndef _GRAMMAR_H
#define _GRAMMAR_H

#include "mpc.h"

#define GRAMMAR \
	" \
		Program: /^/ <Expression>* /$/; \
		Expression: \
			<Number> \
			| <Symbol> \
			| <String> \
			| <Comment> \
			| <Sexpression> \
			| <Qexpression>; \
		Sexpression: '(' <Expression>* ')'; \
		Qexpression: '{' <Expression>* '}'; \
		Symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&|]+/; \
		String: /\"(\\\\.|[^\"])*\"/ ; \
		Number: /-?[0-9]+(\\.[0-9]+)?/; \
		Comment: /;[^\\r\\n]*/; \
	"

/* Parsers */
mpc_parser_t *Program,
	*Expression,
	*Sexpression,
	*Qexpression,
	*Symbol,
	*String,
	*Number,
	*Comment;

#endif /* _GRAMMAR_H */
