/*
 * parser-afs.c - Parser from language of Asynchronous Functional Schemes 
 * to Recursive EQuations System
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

#include <stdlib.h>
int column = 0;

void count()
{
	int i;

	for (i = 0; yytext[i] != '\0'; i++)
		if (yytext[i] == '\n')
			column = 0;
		else if (yytext[i] == '\t')
			column += 8 - (column % 8);
		else
			column++;
	//if (logging)
		ECHO;
}
static unsigned symhash(char *sym) 
{
	unsigned int hash = 0;
	unsigned c;
	while (c = *sym++) hash = hash * 9 ^ c;
	return hash;
}

struct symbol* lookup(char *sym) 
{
	struct symbol *sp = &symtab[symhash(sym)%NHASH];
	int scount = NHASH;
	
	while (--scount > 0) {
		if (sp->name && !strcmp(sp->name, sym)) 
			return sp;
		if (!sp->name) {
			sp->name = strdup(sym);
			sp->reflist = 0;
			return sp;
		}
		if (++sp >= symtab + NHASH)
			sp = symtab;
	}
	if (logging)
		fputs("symbol table overflow\n", stderr);
	abort();
}

void addref(char *word, int type)
{

	struct ref *r;
	struct symbol *sp = lookup(word);
	if (sp->reflist) 
		return;
	r = malloc(sizeof(struct ref));
	if (!r) {
		if (logging)
			fputs("out of space\n",stderr);
		abort();
	}
	r->next = sp->reflist;
	r->type = type;
	sp->reflist = r;
}

struct ast * new_ast(int nodetype, struct ast *l, struct ast *r) 
{
	struct ast *a = malloc(sizeof(struct ast));
    
	if (!a) {
		if (logging)
			yyerror("out of memory");
		exit(0);
	}
	a->nodetype = nodetype;
	a->l = l;
	a->r = r;
	return a;
}

struct ast *new_id(char *id) 
{
	struct term_id *a = malloc(sizeof(struct term_id));
	if (!a) {
		if (logging)
			yyerror("out of memory");
		exit(0);
	}
	a->nodetype = NODE_ID;
	a->name = strdup(id);
	/* add to symtable */
	a->l = NULL;
	a->r = NULL;
	return ((struct ast *) a);
}

struct ast *new_chan(struct ast *chan_id, 
		     struct ast *in_type, 
		     struct ast *in_id, 
		     struct ast *out_type, 
		     struct ast *out_id) 
{
	struct term_chan *ch = malloc(sizeof(struct term_chan));
	if (!ch) {
		if (logging)
			yyerror("out of memory");
		exit(0);
	}
	ch->nodetype = NODE_CHAN;
	ch->id = (struct term_id*) chan_id;

	ch->in_id = (struct term_id*) in_id;
	ch->in_type = in_type;

	ch->out_id = (struct term_id*) out_id;
	ch->out_type = out_type;

	ch->l = NULL;
	ch->r = NULL;

	return ((struct ast *) ch);
}

void print_tree(struct ast *a) 
{
	if (a == NULL)
		return;
	printf("nodetype:%d\n", a->nodetype);
	if (a->nodetype == NODE_ID) {
		if (((struct term_id *) a)->name != NULL)
			printf("id:%s\n",((struct term_id *) a)->name);
	} else if (a->nodetype == NODE_CHAN) {
		struct term_chan * ch = (struct term_chan *) a;
		if (ch->id != NULL)
			print_tree((struct ast *)ch->id);
		if (ch->in_id != NULL)
			print_tree((struct ast *)ch->in_id);
		if (ch->in_type != NULL)
			print_tree(ch->in_type);
		if (ch->out_id != NULL)
			print_tree((struct ast *)ch->out_id);
		if (ch->out_type != NULL)
			print_tree(ch->out_type);
	}
	print_tree(a->l);
	print_tree(a->r);
}

struct ast * get_sem_tree_copy(struct ast *node) 
{
	if (node == NULL)
		return NULL;
	if (node->nodetype == NODE_ID) {
		struct term_id *id = (struct term_id *) node;
		return new_id(id->name);
	}
	struct ast *node_copy = new_ast(node->nodetype, 
				    get_sem_tree_copy(node->l),
				    get_sem_tree_copy(node->r));
	return node_copy;
}
void search_processes(struct ast *a)
{
	if (a == NULL)
		return;
	if (a->nodetype == NODE_CHAN || a->nodetype == NODE_FUNC) {
		struct proc_list *node = malloc(sizeof(struct proc_list));
		if (!node) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		node->proc = a;
		node->next = NULL;
		if (processes) {
			node->next = processes;
			processes = node;
		} else {
			processes = node;
		}
	}
	search_processes(a->l);
	search_processes(a->r);
}

struct ast* afs_to_sem(struct ast *a)
{
	if (a == NULL)
		return NULL;
	if (a->nodetype == NODE_ID) {
		/*if (((struct term_id *) a)->name != NULL)
		  printf("id:%s\n",((struct term_id *) a)->name);*/
		return a;
	} else if (a->nodetype == NODE_CHAN) {
		struct term_chan * ch = (struct term_chan *) a;
		if (logging) {
			 fprintf(yyout, "K%s = (CIN(", ch->id->name);
			 fprintf(yyout, "%s, %s) * ", 
				 ch->id->name, ch->in_id->name);
			 fprintf(yyout, "COUT(%s, %s))#", 
				 ch->id->name, ch->out_id->name);
			 fprintf(yyout, "\n");
		}
		struct ast *proc = malloc(sizeof(struct ast));
		if (!proc) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		proc->nodetype = SEM_CPROC;
		proc->l = (struct ast*) ch->id;
		struct ast *fix_op = malloc(sizeof(struct ast));
		if (!fix_op) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		fix_op->nodetype = '#';
		struct ast *comp_op = malloc(sizeof(struct ast));
		if (!comp_op) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		comp_op->nodetype = '*';
		struct ast *in = malloc(sizeof(struct ast));
		if (!in) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		in->nodetype = SEM_CIN;
		in->l = (struct ast*) ch->id;
		in->r = (struct ast*) ch->in_id;

		struct ast *out = malloc(sizeof(struct ast));
		if (!out) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		out->nodetype = SEM_COUT;
		out->l = (struct ast*) ch->id;
		out->r = (struct ast*) ch->out_id;
		
		comp_op->l = in;
		comp_op->r = out;
		fix_op->l = comp_op;
		fix_op->r = NULL;
		proc->r = fix_op;
		return proc;
	} else if (a->nodetype == NODE_FUNC) {
		if (logging) {
			fprintf(yyout, "P%s = ", ((struct term_id *)a->l)->name);
		}
		struct ast *proc = malloc(sizeof(struct ast));
		if (!proc) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		proc->nodetype = SEM_PROC;
		proc->r = afs_to_sem(a->r); 
		proc->l = a->l;
		if (logging) {
			fprintf(yyout, "\n");
		}
		return proc;
	} else if (a->nodetype == COM) {
		if (logging) {
			fprintf(yyout, "A%s", ((struct term_id *)a->l)->name);
		}
		struct ast *com = malloc(sizeof(struct ast));
		if (!com) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		com->nodetype = SEM_COM;
		com->l = a->l;
		com->r = NULL;
		return com;
	} else if (a->nodetype == NODE_COM_LIST) {
		struct ast *com = malloc(sizeof(struct ast));
		if (!com) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		com->nodetype = '*';
		com->l = afs_to_sem(a->l);
		if (logging) {
			fprintf(yyout, " * ");
		}
		com->r = afs_to_sem(a->r);
		return com;
	} else if (a->nodetype == TRUE) {
		if (logging) {
			fprintf(yyout, "T");
		}
		struct ast *t = malloc(sizeof(struct ast));
		if (!t) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		t->nodetype = SEM_T;
		t->l = NULL;
		t->r = NULL;
		return t;
	} else if (a->nodetype == FALSE) {
		if (logging) {
			fprintf(yyout, "F");
		}
		struct ast *f = malloc(sizeof(struct ast));
		if (!f) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		f->nodetype = SEM_F;
		f->l = NULL;
		f->r = NULL;
		return f;
	} else if (a->nodetype == BOOL) {
		if (logging) {
			fprintf(yyout, "B");
		}
		struct ast *b = malloc(sizeof(struct ast));
		if (!b) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		b->nodetype = SEM_B;
		b->l = a->l;
		b->r = NULL;
		return b;
	} else if (a->nodetype == SKIP) {
		if (logging) {
			fprintf(yyout, "tau");
		}
		struct ast *tau = malloc(sizeof(struct ast));
		if (!tau) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		tau->nodetype = SEM_TAU;
		tau->l = NULL;
		tau->r = NULL;
		return tau;
	} else if (a->nodetype == EXIT) {
		if (logging) {
			fprintf(yyout, "EXIT");
		}
		struct ast *exit = malloc(sizeof(struct ast));
		if (!exit) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		exit->nodetype = SEM_EXIT;
		exit->l = NULL;
		exit->r = NULL;
		return exit;
	} else if (a->nodetype == BREAK) {
		if (logging) {
			fprintf(yyout, "BREAK");
		}
		struct ast *br = malloc(sizeof(struct ast));
		if (!br) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		br->nodetype = SEM_BREAK;
		br->l = NULL;
		br->r = NULL;
		return br;
	} else if (a->nodetype == WAIT) {
		if (logging) {
			fprintf(yyout, "TIME");
		}
		struct ast *time = malloc(sizeof(struct ast));
		if (!time) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		time->nodetype = SEM_TIME;
		time->l = NULL;
		time->r = NULL;
		return time;
	} else if (a->nodetype == READ) {
		if (logging) {
			fprintf(yyout, "IN");
			fprintf(yyout, "(%s, %s)", 
				((struct term_id *)a->l)->name, 
				((struct term_id *)a->r)->name);
		}
		struct ast *in = malloc(sizeof(struct ast));
		if (!in) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		in->nodetype = SEM_IN;
		in->l = a->l;
		in->r = a->r;
		return in;
	} else if (a->nodetype == WRITE) {
		if (logging) {
			fprintf(yyout, "OUT");
			fprintf(yyout, "(%s, %s)", 
				((struct term_id *)a->l)->name, 
				((struct term_id *)a->r)->name);
		}
		struct ast *out = malloc(sizeof(struct ast));
		if (!out) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		out->nodetype = SEM_OUT;
		out->l = a->l;
		out->r = a->r;
		return out;
	} else if (a->nodetype == SEQ) {		       
		return afs_to_sem(a->l); 
	} else if (a->nodetype == PAR) {
		struct ast *par_op = malloc(sizeof(struct ast));
		if (!par_op) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		par_op->nodetype = 'U';
		par_op->l = afs_to_sem(a->l);
		if (logging) {
			fprintf(yyout, " U ");
		}
		par_op->r = afs_to_sem(a->r);
		return par_op;
	} else if (a->nodetype == ALT) {
		struct ast *comp_op = malloc(sizeof(struct ast));
		if (!comp_op) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		if (a->r) {
			comp_op->nodetype = '+';
			comp_op->l = afs_to_sem(a->l);
			if (logging) {
				fprintf(yyout, " + ");
			}
			comp_op->l = afs_to_sem(a->r);       
		} else {
			comp_op = afs_to_sem(a->l);
		}
		return comp_op;
	} else if (a->nodetype == LOOP) {
		struct ast *fix_op = malloc(sizeof(struct ast));
		if (!fix_op) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		fix_op->nodetype = '#';
		if (logging) {
			fprintf(yyout, "(");
		}
		fix_op->l = afs_to_sem(a->l);
		fix_op->r = afs_to_sem(a->r);
		if (logging) {
			fprintf(yyout, ")#");
		}
		return fix_op;
	} else if (a->nodetype == NODE_GC) {
		struct ast *comp_op = malloc(sizeof(struct ast));
		if (!comp_op) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		comp_op->nodetype = '^';
		comp_op->l = afs_to_sem(a->l);
		if (logging) {
			fprintf(yyout, " ^ ");
		}
		comp_op->r =afs_to_sem(a->r); 
		return comp_op;
	} else if (a->nodetype == NODE_GC_LIST) {
		struct ast *comp_op = malloc(sizeof(struct ast));
		if (!comp_op) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		comp_op->nodetype = '+';
		comp_op->l = afs_to_sem(a->l);
		if (logging) {
			fprintf(yyout, " + ");
		}
		comp_op->r = afs_to_sem(a->r);       		
		return comp_op;
	}
}

void proc_sem_to_par_comp()
{
	struct proc_list *tmp = sem_processes;
	sem_root = NULL;
	if (!tmp)
		return;	
	while (tmp) {
		if (!sem_root && tmp->next) {
			struct ast *proc = malloc(sizeof(struct ast));
			if (!proc) {
				if (logging)
					yyerror("out of memory");
				exit(0);
			}
			proc->nodetype = SEM_PAR;
			proc->l = tmp->proc;
			proc->r = tmp->next->proc;
			sem_root = proc;
			tmp = tmp->next->next;
		} else if (sem_root) {
			struct ast *proc = malloc(sizeof(struct ast));
			if (!proc) {
				if (logging)
					yyerror("out of memory");
				exit(0);
			}
			proc->nodetype = SEM_PAR;
			proc->l = sem_root;
			proc->r = tmp->proc;
			sem_root = proc;
			tmp = tmp->next;
		} else {
			tmp = tmp->next;
		}
	}
}

int apply_distributive_law(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return 0;


	if (a->nodetype == SEM_PARLL ||
	    a->nodetype == '|') {
		if (a->l && a->l->nodetype == '+') {
			if (parent && parent->nodetype == '+') {
				if (parent->l == a) {
					struct ast *n1 =
						malloc(sizeof(struct ast));
					if (!n1) {
						if (logging)
							yyerror("out of memory");
						exit(0);
					}
					n1->nodetype = '+';
					n1->r = parent->r;
					n1->l = new_ast(a->nodetype, 
							a->l->r, 
							a->r);
					parent->r = n1;
					parent->l = new_ast(a->nodetype, 
							    a->l->l, 
							    a->r);
					free(a);
					a = parent;
					//printf("1");
				} else {
					struct ast *n1 = 
						malloc(sizeof(struct ast));
					if (!n1) {
						if (logging)
							yyerror("out of memory");
						exit(0);
					}
					n1->nodetype = '+';
					n1->l = new_ast(a->nodetype, 
							a->l->l, 
							a->r);
					n1->r = new_ast(a->nodetype, 
							a->l->r, 
							a->r);
					parent->r = n1;
					free(a);
					a = parent;
					//printf("2");
				}
			}
		} else if (a->r && a->r->nodetype == '+') {
			if (parent && parent->nodetype == '+') {
				if (parent->l == a) {
					struct ast *n1 = 
						malloc(sizeof(struct ast));
					if (!n1) {
						if (logging)
							yyerror("out of memory");
						exit(0);
					}
					n1->nodetype = '+';
					n1->r = parent->r;
					n1->l = new_ast(a->nodetype, 
							a->l, 
							a->r->r);
					parent->r = n1;
					parent->l = new_ast(a->nodetype, 
							    a->l, 
							    a->r->l);
					free(a);
					a = parent;
					//printf("3");
				} else {
					struct ast *n1 = 
						malloc(sizeof(struct ast));
					if (!n1) {
						if (logging)
							yyerror("out of memory");
						exit(0);
					}
					n1->nodetype = '+';
					n1->l = new_ast(a->nodetype, 
							a->l, 
							a->r->l);
					n1->r = new_ast(a->nodetype, 
							a->l, 
							a->r->r);
					parent->r = n1;
					free(a);
					a = parent;
					//printf("4");
				} 
			}		
		}
	} 
	
	apply_distributive_law(a->l, a);
	apply_distributive_law(a->r, a);
	return 0;
}
int apply_basis_axioms(struct ast *a, struct ast *parent)  
{
	if (a == NULL)
		return 0;
	if (a->nodetype == '*') {
		if (a->l && a->l->nodetype == SEM_NULL) {
			// @ * X = @

			if (logging) {
				fprintf(yyout, "@ * X = @");
			}
			a->nodetype = SEM_NULL;
			a->l = NULL;
			a->r = NULL;
			return 1;
		} else if (a->l && a->l->nodetype == SEM_TAU) {
			// tau * X = X
			if (logging) {
				fprintf(yyout, "tau * X = X");
			}
			a->nodetype = a->r->nodetype;
			a->l = a->r->l;
			a->r = a->r->r;
			return 1;
		} else if (a->r && a->r->nodetype == SEM_TAU) {
			// X * tau = X			
			if (logging) {
				fprintf(yyout, "X * tau = X");
			}
			a->nodetype = a->l->nodetype;
			a->r = a->l->r;
			a->l = a->l->l;

			return 1;
		} else if (a->l && a->l->nodetype == '+') {
			// (X + Y) * Z = X * Z + Y * Z
			if (logging) {
				fprintf(yyout, "(X + Y) * Z = X * Z + Y * Z");
			}
			struct ast *left_add = new_ast(a->nodetype, 
						       a->l->l, 
						       a->r);
			struct ast *right_add = new_ast(a->nodetype, 
							a->l->r, 
							a->r);
			a->nodetype = '+';
			a->l = left_add;
			a->r = right_add;		       
			return 1;
		} /*else if (a->r && a->r->nodetype == '+') {
			// X * (Y + Z) = X * Y + X * Z
			fprintf(yyout, "X * (Y + Z) = X * Y + X * Z");
			struct ast *left_add = 
			new_ast(a->nodetype, a->l, a->r->l);
			struct ast *right_add = 
			new_ast(a->nodetype, a->l, a->r->r);
			a->nodetype = '+';
			a->l = left_add;
			a->r = right_add;		       
			return 1;
		} */ else if (a->l && a->l->nodetype == '^') {
			// (b ^ X) * Y = b ^ (X * Y)
			if (logging) {
				fprintf(yyout, "(b ^ X) * Y = b ^ (X * Y)");
			}
			// TODO
			return 0;
		}
	} else if (a->nodetype == '^') {
		if (a->l && 
		    (a->l->nodetype == SEM_NULL || a->l->nodetype == SEM_F)) {
			// F ^ X = @
			if (logging) {
				fprintf(yyout, "F ^ X = @ or ");
			}
			// @ ^ X = @
			if (logging) {
				fprintf(yyout, "@ ^ X = @");
			}
			a->nodetype = SEM_NULL;
			a->l = NULL;
			a->r = NULL;
			return 1;
		} 
	} else if (a->nodetype == '+') {
		if (a->l && a->l->nodetype == SEM_NULL) {
			// @ + X = X			
			if (logging) {
				fprintf(yyout, "@ + X = X");
			}
			a->nodetype = a->r->nodetype;
			a->l = a->r->l;
			a->r = a->r->r;

			return 1;
		} else if (a->r && a->r->nodetype == SEM_NULL) {
			// X + @ = X
			if (logging) {
				fprintf(yyout, "X + @ = X");
			}
			a->nodetype = a->l->nodetype;
			a->r = a->l->r;
			a->l = a->l->l;

			return 1;
		} else if (a->l && a->l->nodetype == '^' &&  
			   a->r && a->r->nodetype == '^' || 
			   a->l && a->l->nodetype == '*' &&  
			   a->r && a->r->nodetype == '*') {
			// b ^ X + b ^ Y = b ^ (X + Y)
			// a * X + a * Y = a * (X + Y)

			if (is_equal_subtree(a->l->l, a->r->l)) {
				if (a->l->nodetype == '^') {
					if (logging) {
						fprintf(yyout, "b ^ X + b ^ Y = b ^ (X + Y)");
					}
				} else if (a->l->nodetype == '*') {
					if (logging) {
						fprintf(yyout, "a * X + a * Y = a * (X + Y)");
					}
				}
				a->nodetype = a->l->nodetype;
				a->r = new_ast('+', a->l->r, a->r->r);
				a->l = a->l->l;
				return 1;
			}
		} else if (a->l && a->l->nodetype == '^' && 
			   a->r && a->r->nodetype == '+' &&
			   a->r->l && a->r->l->nodetype == '^' || 
			   a->l && a->l->nodetype == '*' && a->r && 
			   a->r->nodetype == '+' &&
			   a->r->l && a->r->l->nodetype == '*' ) {
			// b ^ X + b ^ Y = b ^ (X + Y)
			// a * X + a * Y = a * (X + Y)

			if (is_equal_subtree(a->l->l, a->r->l->l)) {
				if (a->l->nodetype == '^') {
					if (logging) {
						fprintf(yyout, "b ^ X + b ^ Y = b ^ (X + Y)");
					}
				} else if (a->l->nodetype == '*') {
					if (logging) {
						fprintf(yyout, "a * X + a * Y = a * (X + Y)");
					}
				}
				a->l->r = new_ast(a->nodetype, a->l->r, a->r->l->r);
				a->r = a->r->r;
				return 1;
			}
		}
	} else if (a->nodetype == SEM_PAR) {
		if (a->l && a->l->nodetype == SEM_TAU) {
			// tau || X = X
			if (logging) {
				fprintf(yyout, "tau || X = X");
			}
			a->nodetype = a->r->nodetype;
			a->l = a->r->l;
			a->r = a->r->r;
			return 1;
		}
		if (a->r && a->r->nodetype == SEM_TAU) {
			// X || tau = X
			if (logging) {
				fprintf(yyout, "X || tau = X");
			}
			a->nodetype = a->l->nodetype;
			a->r = a->l->r;
			a->l = a->l->l;
			return 1;
		}
	}
	int retval = apply_basis_axioms(a->l, a);
	if (retval == 0)
		retval = apply_basis_axioms(a->r, a);
	return retval;
}
int apply_axioms_for_communication(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return 0;
	if (a->nodetype == '|') {
		if (a->l && a->l->nodetype == '^' &&
		    a->r && a->r->nodetype == '^') {
			//  (b1 ^ X) | (b2 ^ Y) = (b1 | b2) ^ (X || Y)
			struct ast * left = new_ast('|', a->l->l, a->r->l);
			struct ast * right = new_ast(SEM_PAR, a->l->r, a->r->r);
			a->nodetype = '^';
			a->l = left;
			a->r = right;			
			return 1;
		} if (a->l && a->l->nodetype == '*' &&
		    a->r && a->r->nodetype == '*') {
			// (a1 * X) | (a2 * Y) = (a1 | a2) * (X || Y)
			struct ast * left = new_ast('|', a->l->l, a->r->l);
			struct ast * right = new_ast(SEM_PAR, a->l->r, a->r->r);
			a->nodetype = '*';
			a->l = left;
			a->r = right;			
			return 1;
		} if (a->l && a->l->nodetype == '*' &&
		      a->r && a->r->nodetype == '^' ||
		      a->l && a->l->nodetype == '^' &&
		      a->r && a->r->nodetype == '*') {
			// (a * X) | (b ^ Y) = (a | b) ^ (X || Y)
			struct ast * left = new_ast('|', a->l->l, a->r->l);
			struct ast * right = new_ast(SEM_PAR, a->l->r, a->r->r);
			a->nodetype = '^';
			a->l = left;
			a->r = right;
			return 1;			
		} else if (a->l && a->l->nodetype == '*' ||
			   a->r && a->r->nodetype == '*') {
			if (a->l && a->l->nodetype == '*') {
				// (a1 * X) | a2 = (a1 | a2) * X
				struct ast * left = new_ast('|', a->l->l, a->r);
				struct ast * right = a->l->r;
				a->nodetype = '*';
				a->l = left;
				a->r = right;
				return 1;
			} else {
				// a1 | (a2 * X) = (a1 | a2) * X
				struct ast * left = new_ast('|', a->l, a->r->l);
				struct ast * right = a->r->r;
				a->nodetype = '*';
				a->l = left;
				a->r = right;
				return 1;
			}
		} else if (a->l && a->l->nodetype == '^' ||
			   a->r && a->r->nodetype == '^') {
			if (a->l && a->l->nodetype == '^') {
				// (b ^ X) | a = (a | b) ^ X
				struct ast * left = new_ast('|', a->l->l, a->r);
				struct ast * right = a->l->r;
				a->nodetype = '^';
				a->l = left;
				a->r = right;
				return 1;
			} else {
				// a | (b ^ X) = (a | b) ^ X
				struct ast * left = new_ast('|', a->l, a->r->l);
				struct ast * right = a->r->r;
				a->nodetype = '^';
				a->l = left;
				a->r = right;
				return 1;
			}
		}
	}
	int retval = apply_axioms_for_communication(a->l, a);
	if (retval == 0)
		retval = apply_axioms_for_communication(a->r, a);
	return retval;	
}
int apply_communication_rule(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return 0;
	if (a->nodetype == '|') {
		if (a->l && a->r && (
		    a->l->nodetype == SEM_IN   && a->r->nodetype == SEM_COUT ||
		    a->l->nodetype == SEM_CIN  && a->r->nodetype == SEM_OUT  ||
		    a->l->nodetype == SEM_OUT  && a->r->nodetype == SEM_CIN  ||
		    a->l->nodetype == SEM_COUT && a->r->nodetype == SEM_IN)) {
			struct term_id *l1 = (struct term_id *) a->l->l;
			struct term_id *l2 = (struct term_id *) a->l->r;
			struct term_id *r1 = (struct term_id *) a->r->l;
			struct term_id *r2 = (struct term_id *) a->r->r;
			if (strcmp(l1->name, r1->name) == 0 &&
			    strcmp(l2->name, r2->name) == 0) {
				a->nodetype = SEM_GAMMA;
				a->l = new_id(l1->name);
				a->r = new_id(l2->name);
			} else {
				a->nodetype = SEM_NULL;
				a->l = NULL;
				a->r = NULL;
			}
		} else {
			a->nodetype = SEM_NULL;
			a->l = NULL;
			a->r = NULL;
		}
		return 1;
	}
	int retval = apply_communication_rule(a->l, a);
	if (retval == 0)
		retval = apply_communication_rule(a->r, a);
	return retval;	

}
int apply_axioms_for_ll_operation(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return 0;
	if (a->nodetype == SEM_PARLL && 
	    a->l && a->l->nodetype == SEM_NULL) {
		a->nodetype = SEM_NULL;
		a->l = NULL;
		a->r = NULL;
		return 1;
	}	       
	if (a->nodetype == SEM_PARLL) {	       
		if (a->l && a->l->nodetype == '^' ||
		    a->l && a->l->nodetype == '*' ) {
			struct ast *n = new_ast(SEM_PAR, a->l->r, a->r);
			a->nodetype = a->l->nodetype;
			a->l = a->l->l;
			a->r = n;
			return 1;
		} else if (a->l && 
			   (a->l->nodetype == SEM_IN ||
			    a->l->nodetype == SEM_CIN ||
			    a->l->nodetype == SEM_OUT ||
			    a->l->nodetype == SEM_COUT || 
			    a->l->nodetype == SEM_GAMMA || 
			    a->l->nodetype == SEM_COM ||
			    a->l->nodetype == SEM_TAU ||
			    a->l->nodetype == SEM_NULL )) {
			a->nodetype = '*';
			return 1;
			
		}
	}
	int retval = apply_axioms_for_ll_operation(a->l, a);
	if (retval)
		return retval;
	retval = apply_axioms_for_ll_operation(a->r, a);
	return retval;
}

int apply_encapsulation_operation(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return 0;
	if (parent == NULL) {
		if (a->nodetype == '^' ||  a->nodetype == '*') {
			if (a->l && 
			    (a->l->nodetype == SEM_IN ||
			     a->l->nodetype == SEM_CIN ||
			     a->l->nodetype == SEM_OUT ||
			     a->l->nodetype == SEM_COUT)) {
				struct ast *n = new_ast(SEM_NULL, NULL, NULL);
				a->l = n;
			}
		}
		
	}
	if (a->nodetype == '+') {
		if (a->l && (a->l->nodetype == '^' ||
			     a->l->nodetype == '*')) {
			if (a->l->l && 
			    (a->l->l->nodetype == SEM_IN ||
			     a->l->l->nodetype == SEM_CIN ||
			     a->l->l->nodetype == SEM_OUT ||
			     a->l->l->nodetype == SEM_COUT)) {
				struct ast *n = new_ast(SEM_NULL, NULL, NULL);
				a->l->l = n;
			}
		}
		if (a->r && (a->r->nodetype == '^' ||
			     a->r->nodetype == '*')) {
			if (a->r->l && 
			    (a->r->l->nodetype == SEM_IN ||
			     a->r->l->nodetype == SEM_CIN ||
			     a->r->l->nodetype == SEM_OUT ||
			     a->r->l->nodetype == SEM_COUT)) {
				struct ast *n = new_ast(SEM_NULL, NULL, NULL);
				a->r->l = n;
			}
		}

	}
	
	apply_encapsulation_operation(a->l, a);
	apply_encapsulation_operation(a->r, a);	
	return 0;
}
void print_proc_list(struct ast *a) 
{
	struct proc_list *p = NULL;
	get_proc_list(a, &p);
	if (logging) {
		fprintf(yyout, "\n Proc_list \n");
	}
	while(p) {
		if (logging) {
			fprintf(yyout, "\n ++++++++++ \n");		
			print_sem_equation(p->proc);
		}
		p = p->next;
	}
	if (logging) {
		fprintf(yyout, "\n End proc list \n");
	}
}
void add_to_proc_list(struct proc_list **p, struct ast * a) 
{
	if (*p == NULL) {				
		*p = malloc(sizeof(struct proc_list));
		if (!(*p)) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		(*p)->proc = a;
		(*p)->next = NULL;
	} else {
		struct proc_list *n = malloc(sizeof(struct proc_list));
		if (!n) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		n->proc = a;
		n->next = *p;
		*p = n;
	}
}
void get_proc_list(struct ast *a, struct proc_list **p)  
{
	if (a == NULL || a->nodetype != SEM_PAR)
		return;
	if (a->nodetype == SEM_PAR) {
		if (a->l && a->l->nodetype != SEM_PAR) {
			add_to_proc_list(p, a->l);
		}
		if (a->r && a->r->nodetype != SEM_PAR) {
			add_to_proc_list(p, a->r);
		}
	} 
	if (a->l && a->l->nodetype == SEM_PAR) {
		get_proc_list(a->l, p);
	}
	if (a->r  && a->r->nodetype == SEM_PAR) {
		get_proc_list(a->r, p);
	}
}

void get_full_proc_list(struct ast *a, struct proc_list **p)
{
	if (a == NULL)
		return;
	if (a->nodetype == SEM_PAR) {
		if (a->l && a->l->nodetype == SEM_PAR) {
			get_proc_list(a->l, p);
		} else {
			add_to_proc_list(p, a->l);
		}
		if (a->r && a->r->nodetype == SEM_PAR) {
			get_proc_list(a->r, p);
		} else {
			add_to_proc_list(p, a->r);
		}
	} 
}
int get_list_size(struct proc_list *p)
{
	struct proc_list *tmp = p;
	int i = 0;
	while (tmp) {
		i++;
		tmp = tmp->next;
	}
	return i;
}
void print_list(struct proc_list *p) 
{
	struct proc_list *t = p;
	if (logging) {
		fprintf(yyout, 
			"\n ////////////////////////////////////////////////");
	}
	while (t) {
		if (logging) {
			fprintf(yyout, "\n //////// ");	
			print_sem_equation(t->proc);
		}
		t = t->next;
		if (logging) {
			fprintf(yyout, "\n //////// ");
		}
	}
	if (logging) {
		fprintf(yyout, 
			"\n ///////////////////////////////////////////////");
	}
	
}
int compare_proc_list(struct ast *a) 
{
	struct proc_list *p = NULL;
	get_full_proc_list(a, &p);
	if (!p) {
		p = malloc(sizeof(struct proc_list));
		if (!p) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		p->proc = a;
		p->next = NULL;
	}
	int i = 1;
	for (i = 1; i <= last_eq_index; i++) {
		if (i == curr_eq_index)
			continue;
		if (is_equal_subtree(a, equations[i])) 
			return i;
		struct proc_list *tmp = p;		
		struct proc_list *tmp2 = NULL;

		if (i > curr_eq_index) {
			get_full_proc_list(equations[i], &tmp2);
			/*if (!tmp2) {
				tmp2 = malloc(sizeof(struct proc_list));
				if (!node) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				tmp->proc = equations[i];
				tmp->next = NULL;
				}*/
		} else {
			get_full_proc_list(initial_equations[i], &tmp2);
			/*if (!tmp2) {
				tmp2 = malloc(sizeof(struct proc_list));
				if (!tmp2) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				tmp->proc = initial_equations[i];
				tmp->next = NULL;
			 }*/
		}
		print_list(tmp);
		print_list(tmp2);
		int is_eq = 1;
		if (get_list_size(p) != get_list_size(tmp2)) {
			is_eq = 0;
		}
		
		while(tmp && is_eq) {
			struct proc_list *t = tmp2;
			while (t) {
				if (is_equal_subtree(tmp->proc, t->proc)) {
					break;
				} else{
					t = t->next;
				}
			}
			if (t) {
				tmp = tmp->next;
			} else {
				is_eq = 0;			
			}
		}
		if (is_eq) { 
			if (logging) {
				fprintf(yyout, " \n\n <<<<<< ");
				print_sem_equation(a);
				fprintf(yyout, " == ");
				if (i > curr_eq_index)
					print_sem_equation(equations[i]);
				else
					print_sem_equation(initial_equations[i]);
					fprintf(yyout, " >>>>>> (%d)\n", i);
			}			
			return i;
		} else {
			if (logging) {

				fprintf(yyout, " \n\n <<<<<< ");
				print_sem_equation(a);
				fprintf(yyout, " != ");
				if (i > curr_eq_index)
					print_sem_equation(equations[i]);
				else
					print_sem_equation(initial_equations[i]);
					
					fprintf(yyout, " >>>>>> (%d)\n", i);
			}			
		}
	}
	return -1;
}
int designate_equation(struct ast **a) 
{
	
	/*char buf[10] = {0};
	sprintf(buf, "%d" , ++last_eq_index);
	struct ast *id = new_id(buf);
	struct ast *n = new_ast(SEM_EQ, id, *a);
	equations[last_eq_index] = get_sem_tree_copy(*a);
	*a = n;
	initial_equations[indx] = get_sem_tree_copy(*a);*/
	int indx = compare_proc_list(*a);
	if (indx == -1) {
		char buf[10] = {0};
		sprintf(buf, "%d" , ++last_eq_index);
		struct ast *id = new_id(buf);
		struct ast *n = new_ast(SEM_EQ, id, *a);
		equations[last_eq_index] = get_sem_tree_copy(*a);

		*a = n;
		struct proc_list *p = NULL;
		get_proc_list(*a, &p);
		if (p) {
			initial_equations[last_eq_index] = p->proc;
		}
	} else {
		char buf[10] = {0};
		sprintf(buf, "%d" , indx);
		struct ast *id = new_id(buf);
		struct ast *n = new_ast(SEM_EQ, id, *a);
		*a = n;
		if (last_eq_index != curr_eq_index) {
			initial_equations[last_eq_index] = 
				initial_equations[indx];
		}
	}

	return 1;
}
int apply_equational_characterization(struct ast *a, struct ast *parent)
{
	if (a == NULL)
		return 0;

	if (a->nodetype == '+') {
		if (a->l && 
		    (a->l->nodetype == '^' || a->l->nodetype == '*')) {
			if (a->l->r && a->l->r->nodetype == '+') {
				designate_equation(&a->l->r->l);
				designate_equation(&a->l->r->r);
			} else
				designate_equation(&a->l->r);
		} 
		if (a->r && 
		    (a->r->nodetype == '^' || a->r->nodetype == '*')) {
			if (a->r->r && a->r->r->nodetype == '+') {
				designate_equation(&a->r->r->l);
				designate_equation(&a->r->r->r);
			} else
				designate_equation(&a->r->r);
		}
	} else if (parent == NULL && 
		   (a->nodetype == '^' || a->nodetype == '*')) {
		if (a->r && a->r->nodetype == '+') {
			designate_equation(&a->r->l);
			designate_equation(&a->r->r);
		} else 
			designate_equation(&a->r);
		
	}
	apply_equational_characterization(a->l, a);
	apply_equational_characterization(a->r, a);
	return 0;
}
struct ast * find_first_communication_node(struct ast *a) 
{
	if (a == NULL)
		return NULL;
	if (a->nodetype == SEM_CIN ||
	    a->nodetype == SEM_COUT ||
	    a->nodetype == SEM_IN || 
	    a->nodetype == SEM_OUT ) {
		return a;
	}
	struct ast *tmp = NULL;
	if ((tmp = find_first_communication_node(a->l)) != NULL)
		return tmp;
	if ((tmp = find_first_communication_node(a->r)) != NULL)
		return tmp;
	
}
int is_can_communication(struct ast *a, struct ast *b)
{
	if ((a->nodetype == SEM_CIN && b->nodetype == SEM_OUT ||
	     b->nodetype == SEM_CIN && a->nodetype == SEM_OUT ||
	     b->nodetype == SEM_COUT && a->nodetype == SEM_IN ||
	     a->nodetype == SEM_COUT && b->nodetype == SEM_IN) &&
	    is_equal_subtree(a->l, b->l) &&
	    is_equal_subtree(a->r, b->r))  {
		return 1;
	}

	return 0;
}
struct ast *build_optimizing_tree(struct proc_list *p) 
{
	struct proc_list *tmp1 = p;
	struct proc_list *first = NULL;
	struct proc_list *second = NULL;
	while (tmp1 && !first) {
		if (!tmp1->first_comm) {
			tmp1 = tmp1->next;
			continue;
		}			
		struct proc_list *tmp2 = p;
		while (tmp2 && !first) {
			if (!tmp2->first_comm ||
			    tmp1 == tmp2) {
				tmp2 = tmp2->next;
				continue;
			}
			
			if (is_can_communication(tmp1->first_comm, 
						 tmp2->first_comm)) {
				first = tmp1;
				second = tmp2;
			}
			tmp2 = tmp2->next;
		}
		tmp1 = tmp1->next;
	}
	if (first) {
		if (logging) {
			fprintf(yyout, "\n{{ ");
			print_sem_equation(first->first_comm);
			fprintf(yyout, ",  ");
			print_sem_equation(second->first_comm);
			fprintf(yyout, " }}\n");
		}
		
		struct ast *new_tree = NULL;
		struct ast *tmp_node = NULL;
		struct proc_list *tmp = p;
		tmp_node = malloc(sizeof(struct ast));
		if (!tmp_node) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		tmp_node->nodetype = SEM_PAR;
		tmp_node->l = first->proc;
		tmp_node->r = second->proc;
		new_tree = tmp_node;
		while (tmp) {
			if (tmp != first && tmp != second ) {
				struct ast *proc = malloc(sizeof(struct ast));
				if (!proc) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				proc->nodetype = SEM_PAR;				
				proc->r = tmp->proc;
				proc->l = tmp_node->r;
				tmp_node->r = proc;
				tmp_node = proc;
			}
			tmp = tmp->next;	
		}
		if (logging) {
			fprintf(yyout, "\n{{ ");
			print_sem_equation(new_tree);
			fprintf(yyout, " }}\n");
		}
		return new_tree;
	} else if (get_list_size(p) >= 2) {
		struct ast *new_tree = NULL;
		struct ast *tmp_node = NULL;
		struct proc_list *tmp = p;
		tmp_node = malloc(sizeof(struct ast));
		if (!tmp_node) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		tmp_node->nodetype = SEM_PAR;
		tmp_node->l = tmp->proc;
		tmp = tmp->next;
		tmp_node->r = tmp->proc;
		new_tree = tmp_node;
		tmp = tmp->next;
		while (tmp) {			
			struct ast *proc = malloc(sizeof(struct ast));
			if (!proc) {
				if (logging)
					yyerror("out of memory");
				exit(0);
			}
			proc->nodetype = SEM_PAR;				
			proc->r = tmp->proc;
			proc->l = tmp_node->r;
			tmp_node->r = proc;
			tmp_node = proc;			
			tmp = tmp->next;	
		}
		return new_tree;
	} else 
		  return NULL;
}

struct ast * combining_par_composition(struct ast *a) 
{
	struct proc_list *p = NULL;
	get_proc_list(a, &p);      
	struct proc_list *t = p;
	if (logging) {
		fprintf(yyout, 
			"\n ////////////////////////////////////////////////");
		while (t) {
			fprintf(yyout, "\n //////// ");
			print_sem_equation(t->proc);
			t = t->next;
			fprintf(yyout, "\n //////// ");		
		}
		fprintf(yyout, 
			"\n ///////////////////////////////////////////////");
	}
	struct proc_list *tmp = p;
	while (tmp) {
		struct ast *comm_node = find_first_communication_node(tmp->proc);
		if (comm_node == NULL) {
			expand_substitutions(tmp->proc);
			comm_node = find_first_communication_node(tmp->proc);
		}
		tmp->first_comm = comm_node;
		if (logging) {
			fprintf(yyout, "\n ||| ");
			print_sem_equation(comm_node);
			fprintf(yyout, " ||| \n");
		}
		tmp = tmp->next;
	}

	return build_optimizing_tree(p);
}

int convert_par_composition(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return 0;
	
	if (a->nodetype == SEM_PAR) {
		if (a->l && a->l->nodetype == SEM_PAR) {
			struct ast *t = a->r;
			a->r = a->l;
			a->l = t;
		}
		struct ast *add1 = malloc(sizeof(struct ast));
		if (!add1) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		add1->nodetype = '+';

		struct ast *parll1 = malloc(sizeof(struct ast));
		if (!parll1) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		parll1->nodetype = SEM_PARLL;
		parll1->l = a->l;
		parll1->r = a->r;
		
		add1->l = parll1;

		struct ast *add2 = malloc(sizeof(struct ast));
		if (!add2) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		add2->nodetype = '+';
		
		struct ast *parll2 = malloc(sizeof(struct ast));
		if (!parll2) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		parll2->nodetype = SEM_PARLL;
		parll2->l = a->r;
		parll2->r = a->l;
		
		struct ast *parl = malloc(sizeof(struct ast));
		if (!parl) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		parl->nodetype = '|';
		parl->l = a->l;
		parl->r = a->r;
		
		add2->l = parll2;
		add2->r = parl;
		
		add1->r = add2;

		if (a->r && a->r->nodetype == SEM_PAR) {
			parll1->r = a->r->l;
			parll2->l = a->r->l;
			parl->r = a->r->l;
			a->nodetype = a->r->nodetype;		
			a->l = add1;
			a->r = a->r->r;
		} else {			
			a->nodetype = add1->nodetype;		
			a->l = add1->l;
			a->r = add1->r;
		}
		return 1;
	} else if (a->nodetype == '+') {
		int retval = 0;
		retval = convert_par_composition(a->l, a);
		retval = convert_par_composition(a->r, a);
		return retval;	
	}
	int retval = 0;
	if ((retval = convert_par_composition(a->l, a)) != 0)
		return retval;
	return 0;
}

void convert_min_fixed_point(struct ast *a, struct ast *curr_proc) 
{
	if (a == NULL)
		return;
	if (a->nodetype == SEM_PROC ||
	    a->nodetype == SEM_CPROC) {
		curr_proc = a;
	} else if (a->nodetype == '#') {
		if(a->l && a->l->nodetype == '^') {
			if (a->l->l != NULL && 
			    a->l->l->nodetype == SEM_B) {
				struct ast *comp = malloc(sizeof(struct ast));
				if (!comp) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				comp->nodetype = '^';
				struct ast *T = malloc(sizeof(struct ast));
				if (!T) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				T->nodetype = SEM_T;
				T->l = NULL;
				T->r = NULL;
				struct ast *tau = malloc(sizeof(struct ast));
				if (!tau) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				tau->nodetype = SEM_TAU;
				tau->l = NULL;
				tau->r = NULL;
				comp->l = T;
				comp->r = tau;
				a->r = comp;
				a->nodetype = '+';
			       
			} 
			struct ast *tmp = a->l->r;
			while (tmp && 
			       (tmp->nodetype == '*' || 
				tmp->nodetype == '+' || 
				tmp->nodetype == '^')) {
				tmp = tmp->r;					
			} 
			if (tmp == NULL) {
				printf("\nERROR in convert_min_fixed_point: \
					can't find right operand");
				return;
			}
			struct ast * pr = new_ast(curr_proc->nodetype, 
						  curr_proc->l, 
						  NULL);
			struct ast * cp = get_sem_tree_copy(tmp);
			tmp->nodetype = '*';
			tmp->r = pr;
			tmp->l = cp;

				
			if (subst == NULL) {
				subst = malloc(sizeof(struct subst_list));
				if (!subst) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				if (a->nodetype == '+')
					subst->p = new_ast(pr->nodetype, 
							   pr->l, 
							   get_sem_tree_copy(a));
				else
					subst->p = new_ast(pr->nodetype, 
							   pr->l, 
							   get_sem_tree_copy(a->l));
				subst->next = NULL;
			} else {
				struct subst_list *n = 
					malloc(sizeof(struct subst_list));
				if (!n) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				if (a->nodetype == '+')
					n->p = new_ast(pr->nodetype, 
						       pr->l, 
						       get_sem_tree_copy(a));
				else
					n->p = new_ast(pr->nodetype, 
						       pr->l, 
						       get_sem_tree_copy(a->l));
				n->next = subst;
				subst = n;
			}
			
		} else if (a->l && a->l->nodetype == '*') {			
			struct ast *tmp = a->l;
			while (tmp->r != NULL && 
			       (tmp->r->nodetype == '*' || 
				tmp->r->nodetype == '+' || 
				tmp->r->nodetype == '^')) {
				tmp = tmp->r;					
			} 
			if (tmp == NULL) {
				printf("\nERROR in convert_min_fixed_point: \
					can't find right operand");
				return;
			}
			struct  ast* pr = new_ast(curr_proc->nodetype, 
						  curr_proc->l, NULL);
			struct ast * proc_comp = new_ast('*', tmp->r, pr);
			
			proc_comp->r = pr;
			tmp->r = proc_comp;
			
			if (subst == NULL) {
				subst = malloc(sizeof(struct subst_list));
				if (!subst) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				subst->p = new_ast(pr->nodetype, 
						   pr->l, 
						   get_sem_tree_copy(a->l));
				subst->next = NULL;
			} else {
				struct subst_list *n = 
					malloc(sizeof(struct subst_list));
				if (!n) {
					if (logging)
						yyerror("out of memory");
					exit(0);
				}
				n->p = new_ast(pr->nodetype, 
					       pr->l, 
					       get_sem_tree_copy(a->l));
				n->next = subst;
				subst = n;
			}
		}
		if (a->nodetype == '#') {
			a->nodetype = a->l->nodetype;
			a->r = a->l->r;
			a->l = a->l->l;
		}
	}
	convert_min_fixed_point(a->l, curr_proc);	
	convert_min_fixed_point(a->r, curr_proc);
}
void expand_needed_equations(struct ast *a) 
{
	if (a == NULL)
		return;
	if (a->nodetype == SEM_PAR) {
		if (a->l && !is_exist_communication_op(a->l)) {
			expand_substitutions(a->l);
		}
		if (a->r && 
		    a->r->nodetype != SEM_PAR && 
		    !is_exist_communication_op(a->r)) {
			expand_substitutions(a->r);
		}
	}
 	expand_needed_equations(a->l); 
	expand_needed_equations(a->r); 
}
int is_exist_communication_op(struct ast *a) 
{
	if (a == NULL)
		return 0;
	if (a->nodetype == SEM_IN || 
	    a->nodetype == SEM_CIN || 
	    a->nodetype == SEM_OUT || 
	    a->nodetype == SEM_COUT) {
		return 1;
	}
	if (is_exist_communication_op(a->l))
		return 1;
	if (is_exist_communication_op(a->r))
		return 1;
	return 0;
}
void expand_substitutions(struct ast *a) 
{
	if (a == NULL)
		return;
	if (a->nodetype == SEM_PROC || a->nodetype == SEM_CPROC) {
		struct subst_list *tmp = subst;
		while (tmp) {
			if (tmp->p && tmp->p->nodetype == a->nodetype &&
			    tmp->p->l && tmp->p->r && a->l) {
				if (strcmp(((struct term_id *)tmp->p->l)->name, 
					   ((struct term_id *)a->l)->name) == 0)
					{
					
					a->nodetype = tmp->p->r->nodetype;
					a->l = get_sem_tree_copy(tmp->p->r->l);
					a->r = get_sem_tree_copy(tmp->p->r->r);
				}
			}
			tmp = tmp->next;
		}
		return;
	}
	expand_substitutions(a->l); 
	expand_substitutions(a->r);
}
void reduce_substitutions(struct ast *a) 
{
	if (a == NULL)
		return;
	struct subst_list *tmp = subst;
	while (tmp) {
	
		if (tmp->p && tmp->p->r->nodetype == a->nodetype) {
			if (is_equal_subtree(tmp->p->r, a)) { 
				a->nodetype = tmp->p->nodetype;
				a->l = tmp->p->l;
				a->r = NULL;
			}
		}
		tmp = tmp->next;
	}
	reduce_substitutions(a->l);
	reduce_substitutions(a->r);
}
int is_equal_subtree(struct ast *a, struct ast *b) {
	if (a == NULL && b == NULL)
		return 1;

	if (a == NULL && b != NULL || a != NULL && b == NULL) {
		return 0;
	}

	
	if (a->nodetype != b->nodetype) {
		return 0;
	}
	if (a->nodetype == b->nodetype &&
	    a->nodetype == NODE_ID &&
	    strcmp(((struct term_id *)a)->name,
		   ((struct term_id *)b)->name) != 0) {
		return 0;
	}
	
	if (is_equal_subtree(a->l, b->l) == 0)
		return 0;

	if (is_equal_subtree(a->r, b->r) == 0)
		return 0;
	return 1;
}
int apply_exit_rule(struct ast *a) 
{
	if (!a)
		return 0;
	if (a->nodetype == '*' &&
	    a->l && a->l->nodetype == SEM_EXIT) {
		a->nodetype = SEM_TAU;
		a->l = NULL;
		a->r = NULL;
	} else if (a->nodetype == SEM_EXIT) {
		a->nodetype = SEM_TAU;
		a->l = NULL;
		a->r = NULL;
	}
	apply_exit_rule(a->l);
	apply_exit_rule(a->r);	
}

int calc_apriori_semantics(struct ast *r) 
{
	search_processes(r);
	struct proc_list *tmp = processes;
	sem_processes = NULL;
	int proc_count = 0;
	while (tmp) {
		struct ast *sem_proc = afs_to_sem(tmp->proc);
		apply_exit_rule(sem_proc);
		struct proc_list *proc = malloc(sizeof(struct proc_list));
		if (!proc) {
			if (logging)
				yyerror("out of memory");
			exit(0);
		}
		proc->proc = sem_proc;
		proc->next = sem_processes;
		sem_processes = proc;
		tmp = tmp->next;
		proc_count++;
	} 
	if (proc_count > MAX_PROC) {
		fprintf(yyout, 
			"\nFile wasn't parsed, process count == %d\n", 
			proc_count);
		return 1;
	}
	proc_sem_to_par_comp();
	if (logging) {
		fprintf(yyout, "\nP = ");
		print_sem_equation(sem_root);
	}
	subst = NULL;
	convert_min_fixed_point(sem_root, NULL);
	remove_proc_node(sem_root, NULL);
	if (logging) {
		fprintf(yyout, " = \n\n = ");
		print_sem_equation(sem_root);	
	}
	
	equations[0] = sem_root;
	int i = 1;
	int iter = 0;
	equations[1] = get_sem_tree_copy(sem_root);
	printf("%d", i);
	while (i <= last_eq_index && i < MAX_EQ) {
		curr_eq_index = i;
		printf("%d\n", i);
		initial_equations[i] = get_sem_tree_copy(equations[i]);
		reduce_substitutions(initial_equations[i]);
		if (logging) {
			fprintf(yyout, " \nP(%d) = ", i);
			print_sem_equation(equations[i]);
		}
		
		struct ast * t = combining_par_composition(equations[i]);
		if (t)
			equations[i] = t;

		if (logging) {
			fprintf(yyout, " = \n\n+++ Optimize parallel composition +++\n\n = ");
			print_sem_equation(equations[i]);
		}
		
		if (i > 1) 
		  expand_needed_equations(equations[i]);
		int retval = 0;
		int par_iter = 0;
		while(convert_par_composition(equations[i], NULL) && 
		      par_iter++ < MAX_ITER) {
			
			apply_distributive_law(equations[i], NULL);
			if (logging) {
				fprintf(yyout, " = \n\n+++ Apply distibutive rule +++\n\n = ");			
				print_sem_equation(equations[i]);
			}
			if (logging) {
				fprintf(yyout, " = \n\n+++ Convert parallel composition +++\n\n = ");
				print_sem_equation(equations[i]);
			}
			iter = 0;
			do {
				if (logging) {
					fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
				}
				retval = apply_basis_axioms(equations[i], NULL);
				if (logging) {
					fprintf(yyout, " +++\n\n = ");
					print_sem_equation(equations[i]);
				}
			} while (retval && iter++ < MAX_ITER);
			apply_distributive_law(equations[i], NULL);
			if (logging) {
				fprintf(yyout, " = \n\n+++ Apply distibutive rule +++\n\n = ");			
				print_sem_equation(equations[i]);
			}
			
			iter = 0;
			do {
				if (logging) {
					fprintf(yyout, " = \n\n+++ Apply axioms for communication +++\n\n = ");
				}
				retval = apply_axioms_for_communication(equations[i], NULL);
				if (logging) {
					fprintf(yyout, " +++\n\n = ");
					print_sem_equation(equations[i]);
				}
			} while (retval && iter++ < MAX_ITER);

			iter = 0;
			do {
				if (logging) {
					fprintf(yyout, " = \n\n+++ Apply communication rule +++\n\n = ");
				}
				retval = apply_communication_rule(equations[i], NULL);
				if (logging) {
					fprintf(yyout, " +++\n\n = ");
					print_sem_equation(equations[i]);
				}
			} while (retval && iter++ < MAX_ITER);
			iter = 0;
			do {
				if (logging) {
					fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
				}
				retval = apply_basis_axioms(equations[i], NULL);
				if (logging) {
					fprintf(yyout, " +++\n\n = ");
					print_sem_equation(equations[i]);
				}
			} while (retval && iter++ < MAX_ITER);

			iter = 0;
			do {
				retval = apply_axioms_for_ll_operation(equations[i], NULL);
				if (logging) {
					fprintf(yyout, " = \n\n+++ Apply axioms for operation LL +++\n\n = ");
					print_sem_equation(equations[i]);
				}			
			} while (retval && iter++ < MAX_ITER);
			
			/* apply_encapsulation_operation(equations[i], NULL); */
			/* if (logging) { */
			/* 	fprintf(yyout, " = \n\n+++ Apply encapsulation operation +++\n\n = "); */
			/* 	print_sem_equation(equations[i]); */
			/* } */
			
			/* iter = 0; */
			/* do { */
			/* 	if (logging) { */
			/* 		fprintf(yyout, " = \n\n+++ Apply basis axiom : "); */
			/* } */
			/* 	retval = apply_basis_axioms(equations[i], NULL); */
			/* 	if (logging) { */
			/* 		fprintf(yyout, " +++\n\n = "); */
			/* 		print_sem_equation(equations[i]); */
			/* 	} */
			/* } while (retval && iter++ < MAX_ITER); */
		
		}
			
		reduce_substitutions(equations[i]);
		if (logging) {
			fprintf(yyout, " = \n\n+++ Reduce equations  +++\n\n");
			print_sem_equation(equations[i]);
		}
		
		iter = 0;
		do {
			if (logging) {
				fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
			}
			retval = apply_basis_axioms(equations[i], NULL);
			if (logging) {
				fprintf(yyout, " +++\n\n = ");			
				print_sem_equation(equations[i]);
			}
		} while (retval && iter++ < MAX_ITER);
		
		iter = 0;
		do {
			retval = apply_axioms_for_ll_operation(equations[i], NULL);
			if (logging) {
				fprintf(yyout, " = \n\n+++ Apply axioms for operation LL +++\n\n = ");
				print_sem_equation(equations[i]);			
			}
		} while (retval && iter++ < MAX_ITER);
		
		iter = 0;
		do {
			if (logging) {
				fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
			}
			retval = apply_basis_axioms(equations[i], NULL);
			if (logging) {
				fprintf(yyout, " +++\n\n = ");			
				print_sem_equation(equations[i]);
			}
		} while (retval && iter++ < MAX_ITER);
		apply_encapsulation_operation(equations[i], NULL);
		if (logging) {
			fprintf(yyout, " = \n\n+++ Apply encapsulation operation +++\n\n = ");
			print_sem_equation(equations[i]);
		}
		
		iter = 0;
		do {
			if (logging) {
				fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
			}
			retval = apply_basis_axioms(equations[i], NULL);
			if (logging) {
				fprintf(yyout, " +++\n\n = ");
				print_sem_equation(equations[i]);
			}
		} while (retval && iter++ < MAX_ITER);
			

	
		apply_equational_characterization(equations[i], NULL);
		
		if (logging) {
			fprintf(yyout, " = \n\n+++ Apply equational_characterization +++\n\nP(%d) = ", i);
			print_sem_equation(initial_equations[i]);

			fprintf(yyout, " = ");

			print_sem_equation(equations[i]);

			fprintf(yyout, " \n\nlast index = %d \n", last_eq_index);
			fprintf(yyout, " \n/////////////////////////////// \n\n", i);
			}
		i++;
	}
	
	if (logging) {
		fprintf(yyout, " = \n\n +++ Initial equations  +++\n\n");
	}
	i = 1;
	while (i <= last_eq_index && i < MAX_EQ) {
		fprintf(yyout, "\nP(%d) = ", i);
		/* print_sem_equation(initial_equations[i]); */

		/* fprintf(yyout, " = "); */

		print_sem_equation(equations[i]);
		fprintf(yyout, "\n");
		i++;
	}
	return 0;
}

void remove_proc_node(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return;
	if ((a->nodetype == SEM_PROC || a->nodetype == SEM_CPROC) &&
	    a->r != NULL) {
		if (parent == NULL) {
			sem_root = a->r;
		} else {
			if (parent->l == a) {
				parent->l = a->r;
			} else
				parent->r = a->r;
		}
		remove_proc_node(a->r, parent);
		free(a);	
		return;
	}
	remove_proc_node(a->l, a);
	remove_proc_node(a->r, a);
}
void print_sem_equation(struct ast *a) 
{
	if (a == NULL)
		return;

	if (a->nodetype == NODE_ID) {
		if (((struct term_id *) a)->name != NULL)
			fprintf(yyout, "%s",((struct term_id *) a)->name);
	} else if (a->nodetype == SEM_PROC ||
		   a->nodetype == SEM_CPROC) {
		if (a->r == NULL) {
			if (a->nodetype == SEM_PROC) {
				fprintf(yyout, "P%s", 
					((struct term_id*)a->l)->name);
			} else {
				fprintf(yyout, "K%s", 
					((struct term_id*)a->l)->name);
			}
		} else
			print_sem_equation(a->r);
		
	} else if (a->nodetype == SEM_EQ) {
		fprintf(yyout, "P");
		fprintf(yyout, "(");
		print_sem_equation(a->l);
		fprintf(yyout, ")");
	} else if (a->nodetype == SEM_IN) {
		fprintf(yyout, "IN");
		fprintf(yyout, "(");
		print_sem_equation(a->l);
		fprintf(yyout, ", ");
		print_sem_equation(a->r);
		fprintf(yyout, ")");
	} else if (a->nodetype == SEM_OUT) {
		fprintf(yyout, "OUT");
		fprintf(yyout, "(");
		print_sem_equation(a->l);
		fprintf(yyout, ", ");
		print_sem_equation(a->r);
		fprintf(yyout, ")");
	} else if (a->nodetype == SEM_CIN) {
		fprintf(yyout, "CIN");
		fprintf(yyout, "(");
		print_sem_equation(a->l);
		fprintf(yyout, ", ");
		print_sem_equation(a->r);
		fprintf(yyout, ")");
	} else if (a->nodetype == SEM_COUT) {
		fprintf(yyout, "COUT");
		fprintf(yyout, "(");
		print_sem_equation(a->l);
		fprintf(yyout, ", ");
		print_sem_equation(a->r);
		fprintf(yyout, ")");
	} else if (a->nodetype == SEM_GAMMA) {
		fprintf(yyout, "gamma");
		fprintf(yyout, "(");
		print_sem_equation(a->l);
		fprintf(yyout, ", ");
		print_sem_equation(a->r);
		fprintf(yyout, ")");
	} else if (a->nodetype == SEM_NULL) {
		fprintf(yyout, "@");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_TIME) {
		fprintf(yyout, "TIME");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_BREAK) {
		fprintf(yyout, "BREAK");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_EXIT) {
		fprintf(yyout, "EXIT");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_TAU) {
		fprintf(yyout, "tau");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_B) {
		fprintf(yyout, "B");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_F) {
		fprintf(yyout, "F");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_T) {
		fprintf(yyout, "T");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_COM_LIST) {
		print_sem_equation(a->l);
		fprintf(yyout, "*");
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_COM) {
		fprintf(yyout, "A");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
	} else if (a->nodetype == SEM_PAR) {
		if (a->l && a->l->nodetype == '+') {
			fprintf(yyout, "( ");
			print_sem_equation(a->l);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->l);
		
		//fprintf(yyout, " || \n || ");
		fprintf(yyout, " || ");
		if (a->r && a->r->nodetype == '+') {
			fprintf(yyout, "( ");
			print_sem_equation(a->r);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->r);
	} else if (a->nodetype == SEM_PARLL) {
		if (a->l && a->l->nodetype == '+' ||
		    a->l && a->l->nodetype == '*' ) {			
			fprintf(yyout, "( ");
			print_sem_equation(a->l);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->l);
		fprintf(yyout, " LL \n LL ");
		if (a->r && a->r->nodetype == '+' ||
		    a->r && a->r->nodetype == '*') {
			fprintf(yyout, "( ");
			print_sem_equation(a->r);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->r);
	} else if (a->nodetype == '#') {
		fprintf(yyout, "(");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
		fprintf(yyout, ")");
		fprintf(yyout, "%c", a->nodetype);
	} else if (a->nodetype == '|') {
		if (a->l && a->l->nodetype == '+') {
			fprintf(yyout, "( ");
			print_sem_equation(a->l);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->l);
		fprintf(yyout, " %c \n %c ", a->nodetype, a->nodetype);
		if (a->r && a->r->nodetype == '+') {
			fprintf(yyout, "( ");
			print_sem_equation(a->r);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->r);
	} else if (a->nodetype == '^' ||
		    a->nodetype == '*') {
		if (a->l && 
		    (a->l->nodetype == '|' ||
		     a->l->nodetype == SEM_PAR ||
		     a->l->nodetype == SEM_PARLL ||
		     a->l->nodetype == '+' )
		    ) {
			fprintf(yyout, "( ");
			print_sem_equation(a->l);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->l);
		fprintf(yyout, " %c ", a->nodetype);
		if (a->r && 
		    (a->r->nodetype == '|' ||
		     a->r->nodetype == SEM_PAR ||
		     a->r->nodetype == SEM_PARLL ||
		     a->r->nodetype == '+' )
		    ) {
			fprintf(yyout, "( ");
			print_sem_equation(a->r);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->r);
	} else if (a->nodetype == '+' ||
		   a->nodetype == 'U' ) {
		
		print_sem_equation(a->l);
		fprintf(yyout, " %c ", a->nodetype);
		print_sem_equation(a->r);
	} else {
		fprintf(yyout, " Unknown nodetype: %d", a->nodetype);
		
	}
}
