#ifndef PARSER_AFS_H
#define PARSER_AFS_H
struct ast {
	int nodetype;
	struct ast *l;
	struct ast *r;
};

void count();

#endif /*PARSER_AFS_H*/
