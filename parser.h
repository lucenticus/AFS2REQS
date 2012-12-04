/*
 * parser-afs.h - Parser from AFS to Recursive EQuations System
 *
 * Copyright (C) 2012 Evgeny Pavlov <lucenticus@gmail.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * This file is part of AFS2REQS.
 *
 * AFS2REQS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * AFS2REQS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with AFS2REQS.  If not, see <http://www.gnu.org/licenses/>.
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#ifndef PARSER_AFS_H
#define PARSER_AFS_H
#define NHASH (9997)
#define MAX_EQ (1024)
#include <stdlib.h>

enum NODETYPE {
	NODE_ID = 1000,
	NODE_CHAN_LIST,
	NODE_CHAN,
	NODE_COM,
	NODE_COM_LIST,
	NODE_FUNC_LIST,   /*5*/
	NODE_FUNC,
	NODE_GC_LIST,
	NODE_GC,
	NODE_PROGRAM,
	SEM_PROC,        /*10*/
	SEM_CPROC,
	SEM_IN,
	SEM_OUT,
	SEM_CIN,
	SEM_COUT,        /*15*/
	SEM_TIME,
	SEM_BREAK,
	SEM_EXIT,        
	SEM_TAU,
	SEM_B,           /*20*/
	SEM_F,
	SEM_T,
	SEM_COM,         
	SEM_COM_LIST,
	SEM_PAR,
	SEM_PARLL,
	SEM_GAMMA,
	SEM_NULL,
	SEM_EQ
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
	struct ast *first_comm;
};

struct subst_list {
	struct ast *p;
	struct subst_list *next;
};

int logging = 0;
struct ast *root;
struct ast *sem_root;
struct proc_list *processes;
struct proc_list *sem_processes;
struct subst_list *subst;
int last_eq_index = 1;
int curr_eq_index = 1;
struct ast *initial_equations[MAX_EQ];
struct ast *equations[MAX_EQ];

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


void remove_proc_node(struct ast *a, struct ast *parent);

void convert_min_fixed_point(struct ast *a, struct ast *curr_proc);
int convert_par_composition(struct ast *a, struct ast *parent);
int apply_equational_characterization(struct ast *a, struct ast *parent);
int apply_distributive_law(struct ast *a, struct ast *parent);
int apply_basis_axioms(struct ast *a, struct ast *parent);
int apply_communication_rule(struct ast *a, struct ast *parent);
int apply_axioms_for_communication(struct ast *a, struct ast *parent);
int apply_encapsulation_operation(struct ast *a, struct ast *parent);
int apply_axioms_for_ll_operation(struct ast *a, struct ast *parent);

void expand_substitutions(struct ast *a);
void reduce_substitutions(struct ast *a);
int is_equal_subtree(struct ast *a, struct ast *b);
int is_exist_communication_op(struct ast *a);
void get_proc_list(struct ast *a, struct proc_list **p);
void add_to_proc_list(struct proc_list **p, struct ast * a);
#endif /*PARSER_AFS_H*/