#ifndef PARSER_AFS_H
#define PARSER_AFS_H

#define NHASH (9997)

struct ast {
	int nodetype;
	struct ast *l;
	struct ast *r;
};


struct term_id {
	int nodetype;
	struct ast *l;
	struct ast *r;
	char *name;
};

struct ref {
	struct ref *next;
	int type;
};

struct symbol {
	char *name;
	struct ref *reflist;
};

struct ast *root;

struct symbol symtab[NHASH];

struct symbol *lookup(char*);
void addref(char*, int);

struct ast *new_ast(int nodetype, struct ast *l, struct ast *r);
struct ast *new_id(char *id);

void count();
enum NODETYPE {
	NODE_ID,
	NODE_COMMAND,   /*0*/
	NODE_CHANNEL,
	NODE_FUNC
};
#endif /*PARSER_AFS_H*/
