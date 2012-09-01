#ifndef PARSER_AFS_H
#define PARSER_AFS_H

#define NHASH (9997)

enum NODETYPE {
	NODE_ID = 1000,
	NODE_CHAN_LIST,
	NODE_CHAN,
	NODE_COM_LIST,
	NODE_COM,
	NODE_FUNC_LIST,   /*5*/
	NODE_FUNC,
	NODE_GC_LIST,
	NODE_GC,
	NODE_PROGRAM,
	SEM_PROC,        /*10*/
	SEM_IN,
	SEM_OUT,
	SEM_TIME,
	SEM_BREAK,
	SEM_EXIT,        /*15*/
	SEM_TAU,
	SEM_B,
	SEM_F,
	SEM_T,
	SEM_COM,         /*20*/
	SEM_PAR
};

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
struct term_chan {
	int nodetype;
	struct ast *l;
	struct ast *r;
	struct term_id *id;
	struct ast *in_type;
	struct term_id *in_id;
	struct ast *out_type;
	struct term_id *out_id;
};
struct ref {
	struct ref *next;
	int type;
};

struct symbol {
	char *name;
	struct ref *reflist;
};

struct proc_list {
	struct ast *proc;
	struct proc_list *next;
};

struct ast *root;
struct ast *sem_root;
struct proc_list *processes;
struct proc_list *sem_processes;
struct symbol symtab[NHASH];

struct symbol *lookup(char*);
void addref(char*, int);

struct ast *new_ast(int nodetype, struct ast *l, struct ast *r);
struct ast *new_id(char *id);
struct ast *new_chan(struct ast *chan_id, 
		     struct ast *in_type, 
		     struct ast *in_id, 
		     struct ast *out_type, 
		     struct ast *out_id);
void count();
void calc_apriori_semantics(struct ast *r);
void proc_sem_to_par_comp();
void search_processes(struct ast *a);
struct ast* afs_to_sem(struct ast *a);
void print_tree(struct ast *a);
void print_sem_equation(struct ast *a);
#endif /*PARSER_AFS_H*/
