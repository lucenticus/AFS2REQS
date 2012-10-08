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
	/*ECHO;*/
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
		fprintf(yyout, "K%s = (CIN(", ch->id->name);
		fprintf(yyout, "%s, %s) * ", 
			ch->id->name, ch->in_id->name);
		fprintf(yyout, "COUT(%s, %s))#", 
			ch->id->name, ch->out_id->name);
		fprintf(yyout, "\n");
		struct ast *proc = malloc(sizeof(struct ast));
		proc->nodetype = SEM_CPROC;
		proc->l = (struct ast*) ch->id;
		struct ast *fix_op = malloc(sizeof(struct ast));
		fix_op->nodetype = '#';
		struct ast *comp_op = malloc(sizeof(struct ast));
		comp_op->nodetype = '*';
		struct ast *in = malloc(sizeof(struct ast));
		in->nodetype = SEM_CIN;
		in->l = (struct ast*) ch->id;
		in->r = (struct ast*) ch->in_id;

		struct ast *out = malloc(sizeof(struct ast));
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
		fprintf(yyout, "P%s = ", ((struct term_id *)a->l)->name);
		struct ast *proc = malloc(sizeof(struct ast));
		proc->nodetype = SEM_PROC;
		proc->r = afs_to_sem(a->r); 
		proc->l = a->l;
		fprintf(yyout, "\n");
		return proc;
	} else if (a->nodetype == COM) {
		fprintf(yyout, "A%s", ((struct term_id *)a->l)->name);
		struct ast *com = malloc(sizeof(struct ast));
		com->nodetype = SEM_COM;
		com->l = a->l;
		com->r = NULL;
		return com;
	} else if (a->nodetype == TRUE) {
		fprintf(yyout, "T");
		struct ast *t = malloc(sizeof(struct ast));
		t->nodetype = SEM_T;
		t->l = NULL;
		t->r = NULL;
		return t;
	} else if (a->nodetype == FALSE) {
		fprintf(yyout, "F");
		struct ast *f = malloc(sizeof(struct ast));
		f->nodetype = SEM_F;
		f->l = NULL;
		f->r = NULL;
		return f;
	} else if (a->nodetype == BOOL) {
		fprintf(yyout, "B");
		struct ast *b = malloc(sizeof(struct ast));
		b->nodetype = SEM_B;
		b->l = NULL;
		b->r = NULL;
		return b;
	} else if (a->nodetype == SKIP) {
		fprintf(yyout, "tau");
		struct ast *tau = malloc(sizeof(struct ast));
		tau->nodetype = SEM_TAU;
		tau->l = NULL;
		tau->r = NULL;
		return tau;
	} else if (a->nodetype == EXIT) {
		fprintf(yyout, "EXIT");
		struct ast *exit = malloc(sizeof(struct ast));
		exit->nodetype = SEM_EXIT;
		exit->l = NULL;
		exit->r = NULL;
		return exit;
	} else if (a->nodetype == BREAK) {
		fprintf(yyout, "BREAK");
		struct ast *br = malloc(sizeof(struct ast));
		br->nodetype = SEM_BREAK;
		br->l = NULL;
		br->r = NULL;
		return br;
	} else if (a->nodetype == WAIT) {
		fprintf(yyout, "TIME");
		struct ast *time = malloc(sizeof(struct ast));
		time->nodetype = SEM_TIME;
		time->l = NULL;
		time->r = NULL;
		return time;
	} else if (a->nodetype == READ) {
		fprintf(yyout, "IN");
		fprintf(yyout, "(%s, %s)", 
			((struct term_id *)a->l)->name, 
			((struct term_id *)a->r)->name);
		struct ast *in = malloc(sizeof(struct ast));
		in->nodetype = SEM_IN;
		in->l = a->l;
		in->r = a->r;
		return in;
	} else if (a->nodetype == WRITE) {
		fprintf(yyout, "OUT");
		fprintf(yyout, "(%s, %s)", 
			((struct term_id *)a->l)->name, 
			((struct term_id *)a->r)->name);
		struct ast *out = malloc(sizeof(struct ast));
		out->nodetype = SEM_OUT;
		out->l = a->l;
		out->r = a->r;
		return out;
	} else if (a->nodetype == SEQ) {
		struct ast *comp_op = malloc(sizeof(struct ast));
		comp_op->nodetype = '*';
		comp_op->l = afs_to_sem(a->l);
		fprintf(yyout, " * ");
		comp_op->r = afs_to_sem(a->r);       
		return comp_op;
	} else if (a->nodetype == PAR) {
		struct ast *par_op = malloc(sizeof(struct ast));
		par_op->nodetype = 'U';
		par_op->l = afs_to_sem(a->l);
		fprintf(yyout, " U ");
		par_op->r = afs_to_sem(a->r);
		return par_op;
	} else if (a->nodetype == ALT) {
		struct ast *comp_op = malloc(sizeof(struct ast));
		if (a->r) {
			comp_op->nodetype = '+';
			comp_op->l = afs_to_sem(a->l);
			fprintf(yyout, " + ");
			comp_op->l = afs_to_sem(a->r);       
		} else {
			comp_op = afs_to_sem(a->l);
		}
		return comp_op;
	} else if (a->nodetype == LOOP) {
		struct ast *fix_op = malloc(sizeof(struct ast));
		fix_op->nodetype = '#';
		fprintf(yyout, "(");
		fix_op->l = afs_to_sem(a->l);
		fix_op->r = afs_to_sem(a->r);
		fprintf(yyout, ")#");
		return fix_op;
	} else if (a->nodetype == NODE_GC) {
		struct ast *comp_op = malloc(sizeof(struct ast));
		comp_op->nodetype = '^';
		comp_op->l = afs_to_sem(a->l);
		fprintf(yyout, " ^ ");
		comp_op->r =afs_to_sem(a->r); 
		return comp_op;
	} else if (a->nodetype == NODE_GC_LIST) {
		struct ast *comp_op = malloc(sizeof(struct ast));
		comp_op->nodetype = '+';
		comp_op->l = afs_to_sem(a->l);
		fprintf(yyout, " + ");
		comp_op->r = afs_to_sem(a->r);       		
		return comp_op;
	}
}

void proc_sem_to_par_comp()
{
	struct proc_list *tmp = sem_processes;
	sem_root = NULL;
	while (tmp) {
		if (!sem_root && tmp->next) {
			struct ast *proc = malloc(sizeof(struct ast));
			proc->nodetype = SEM_PAR;
			proc->r = tmp->proc;
			proc->l = tmp->next->proc;
			sem_root = proc;
			tmp = tmp->next->next;
		} else if (sem_root) {
			struct ast *proc = malloc(sizeof(struct ast));
			proc->nodetype = SEM_PAR;
			proc->r = sem_root;
			proc->l = tmp->proc;
			sem_root = proc;
			tmp = tmp->next;
		}
	}
	/*print_tree(sem_root);*/
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
					struct ast *n1 = malloc(sizeof(struct ast));
					n1->nodetype = '+';
					n1->r = parent->r;
					n1->l = new_ast(a->nodetype, a->l->r, a->r);
					parent->r = n1;
					parent->l = new_ast(a->nodetype, a->l->l, a->r);
					a = parent;
				} else {
					struct ast *n1 = malloc(sizeof(struct ast));
					n1->nodetype = '+';
					n1->l = new_ast(a->nodetype, a->l->l, a->r);
					n1->r = new_ast(a->nodetype, a->l->r, a->r);
					parent->r = n1;
					a = parent;
				}
			}
		} else if (a->r && a->r->nodetype == '+') {
			if (parent && parent->nodetype == '+') {
				if (parent->l == a) {
					struct ast *n1 = malloc(sizeof(struct ast));
					n1->nodetype = '+';
					n1->r = parent->r;
					n1->l = new_ast(a->nodetype, a->l, a->r->r);
					parent->r = n1;
					parent->l = new_ast(a->nodetype, a->l, a->r->l);
					a = parent;
				} else {
					struct ast *n1 = malloc(sizeof(struct ast));
					n1->nodetype = '+';
					n1->l = new_ast(a->nodetype, a->l, a->r->l);
					n1->r = new_ast(a->nodetype, a->l, a->r->r);
					parent->r = n1;
					a = parent;
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
			fprintf(yyout, "@ * X = @");
			a->nodetype = SEM_NULL;
			a->l = NULL;
			a->r = NULL;
			return 1;
		} else if (a->l && a->l->nodetype == SEM_TAU) {
			// tau * X = X
			fprintf(yyout, "tau * X = X");
			a->nodetype = a->r->nodetype;
			a->l = a->r->l;
			a->r = a->r->r;
			return 1;
		} else if (a->r && a->r->nodetype == SEM_TAU) {
			// X * tau = X			
			fprintf(yyout, "X * tau = X");
			a->nodetype = a->l->nodetype;
			a->r = a->l->r;
			a->l = a->l->l;

			return 1;
		} else if (a->l && a->l->nodetype == '+') {
			// (X + Y) * Z = X * Z + Y * Z
			fprintf(yyout, "(X + Y) * Z = X * Z + Y * Z");
			struct ast *left_add = new_ast(a->nodetype, a->l->l, a->r);
			struct ast *right_add = new_ast(a->nodetype, a->l->r, a->r);
			a->nodetype = '+';
			a->l = left_add;
			a->r = right_add;		       
			return 1;
		} /*else if (a->r && a->r->nodetype == '+') {
			// X * (Y + Z) = X * Y + X * Z
			fprintf(yyout, "X * (Y + Z) = X * Y + X * Z");
			struct ast *left_add = new_ast(a->nodetype, a->l, a->r->l);
			struct ast *right_add = new_ast(a->nodetype, a->l, a->r->r);
			a->nodetype = '+';
			a->l = left_add;
			a->r = right_add;		       
			return 1;
		} */ else if (a->l && a->l->nodetype == '^') {
			// (b ^ X) * Y = b ^ (X * Y)
			fprintf(yyout, "(b ^ X) * Y = b ^ (X * Y)");
			// TODO
			return 0;
		}
	} else if (a->nodetype == '^') {
		if (a->l && 
		    (a->l->nodetype == SEM_NULL || a->l->nodetype == SEM_F)) {
			// F ^ X = @
			fprintf(yyout, "F ^ X = @ or ");
			// @ ^ X = @
			fprintf(yyout, "@ ^ X = @");
			a->nodetype = SEM_NULL;
			a->l = NULL;
			a->r = NULL;
			return 1;
		} 
	} else if (a->nodetype == '+') {
		if (a->l && a->l->nodetype == SEM_NULL) {
			// @ + X = X			
			fprintf(yyout, "@ + X = X");
			a->nodetype = a->r->nodetype;
			a->l = a->r->l;
			a->r = a->r->r;

			return 1;
		} else if (a->r && a->r->nodetype == SEM_NULL) {
			// X + @ = X
			fprintf(yyout, "X + @ = X");
			a->nodetype = a->l->nodetype;
			a->r = a->l->r;
			a->l = a->l->l;

			return 1;
		} else if (a->l && a->l->nodetype == '^' &&  a->r && a->r->nodetype == '^' || 
			   a->l && a->l->nodetype == '*' &&  a->r && a->r->nodetype == '*') {
			// b ^ X + b ^ Y = b ^ (X + Y)
			// a * X + a * Y = a * (X + Y)

			if (is_equal_subtree(a->l->l, a->r->l)) {
				if (a->l->nodetype == '^')
					fprintf(yyout, "b ^ X + b ^ Y = b ^ (X + Y)");
				else if (a->l->nodetype == '*')
					fprintf(yyout, "a * X + a * Y = a * (X + Y)");
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
				if (a->l->nodetype == '^')
					fprintf(yyout, "b ^ X + b ^ Y = b ^ (X + Y)");
				else if (a->l->nodetype == '*')
					fprintf(yyout, "a * X + a * Y = a * (X + Y)");
				a->l->r = new_ast(a->nodetype, a->l->r, a->r->l->r);
				a->r = a->r->r;
				return 1;
			}
		}
	} else if (a->nodetype == SEM_PAR) {
		if (a->l && a->l->nodetype == SEM_TAU) {
			// tau || X = X
			fprintf(yyout, "tau || X = X");
			a->nodetype = a->r->nodetype;
			a->l = a->r->l;
			a->r = a->r->r;
			return 1;
		}
		if (a->r && a->r->nodetype == SEM_TAU) {
			// X || tau = X
			fprintf(yyout, "X || tau = X");
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
		} if (a->l && a->l->nodetype == '*' &&
		    a->r && a->r->nodetype == '*') {
			// (a1 * X) | (a2 * Y) = (a1 | a2) * (X || Y)
			struct ast * left = new_ast('|', a->l->l, a->r->l);
			struct ast * right = new_ast(SEM_PAR, a->l->r, a->r->r);
			a->nodetype = '*';
			a->l = left;
			a->r = right;			
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
		} else if (a->l && a->l->nodetype == '*' ||
			   a->r && a->r->nodetype == '*') {
			if (a->l && a->l->nodetype == '*') {
				// (a1 * X) | a2 = (a1 | a2) * X
				struct ast * left = new_ast('|', a->l->l, a->r);
				struct ast * right = a->l->r;
				a->nodetype = '*';
				a->l = left;
				a->r = right;
			} else {
				// a1 | (a2 * X) = (a1 | a2) * X
				struct ast * left = new_ast('|', a->l, a->r->l);
				struct ast * right = a->r->r;
				a->nodetype = '*';
				a->l = left;
				a->r = right;
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
			} else {
				// a | (b ^ X) = (a | b) ^ X
				struct ast * left = new_ast('|', a->l, a->r->l);
				struct ast * right = a->r->r;
				a->nodetype = '^';
				a->l = left;
				a->r = right;
			}
		}
	}
	apply_axioms_for_communication(a->l, a);
	apply_axioms_for_communication(a->r, a);
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
				a->r = new_id(r1->name);
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
	}
	apply_communication_rule(a->l, a);
	apply_communication_rule(a->r, a);
	return 0;
}
int apply_axioms_for_ll_operation(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return 0;
	if (a->nodetype == SEM_PARLL) {
		if (a->l && a->l->nodetype == '^' ||
		    a->l && a->l->nodetype == '*' ) {
			struct ast *n = new_ast(SEM_PAR, a->l->r, a->r);
			a->nodetype = a->l->nodetype;
			a->l = a->l->l;
			a->r = n;
		}
	}
	apply_axioms_for_ll_operation(a->l, a);
	apply_axioms_for_ll_operation(a->r, a);
	return 0;
}

int apply_encapsulation_operation(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return 0;
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
	fprintf(yyout, "\n Proc_list \n");
	while(p) {
		fprintf(yyout, "\n ++++++++++ \n");
		print_sem_equation(p->proc);
		p = p->next;
	}
	fprintf(yyout, "\n End proc list \n");
}
void add_to_proc_list(struct proc_list **p, struct ast * a) 
{
	if (*p == NULL) {				
		*p = malloc(sizeof(struct proc_list));
		(*p)->proc = a;
		(*p)->next = NULL;
	} else {
		struct proc_list *n = malloc(sizeof(struct proc_list));
		n->proc = a;
		n->next = *p;
		*p = n;
	}
}
void get_proc_list(struct ast *a, struct proc_list **p)  
{
	if (a == NULL)
		return;
	if (a->nodetype == SEM_PAR) {
		if (a->l && a->l->nodetype != SEM_PAR) {
			add_to_proc_list(p, a->l);
		}
		if (a->r && a->r->nodetype != SEM_PAR) {
			add_to_proc_list(p, a->r);
		}
	}
	get_proc_list(a->l, p);
	get_proc_list(a->r, p);
}

int compare_proc_list(struct ast *a) 
{
	struct proc_list *p = NULL;
	get_proc_list(a, &p);
	
	int i = 1;
	for (i = 1; i < last_eq_index; i++) {		
		struct proc_list *tmp = p;
		
		struct proc_list *tmp2 = NULL;
		get_proc_list(initial_equations[i], &tmp2);
		int is_eq = 1;
		while(tmp && is_eq) {
			while (tmp2) {
				if (is_equal_subtree(tmp->proc, tmp2->proc))
					tmp2 = tmp2->next;
				else
					break;
			}
			if (is_equal_subtree(tmp->proc, tmp2->proc))
				tmp = tmp->next;
			else
				is_eq = 0;			
		}
		if (is_eq) {
			printf("%d", i);
			return i;
		}
	}
	return -1;
}
int designate_equation(struct ast **a) 
{
	
	char buf[10] = {0};
	sprintf(buf, "%d" , ++last_eq_index);
	struct ast *id = new_id(buf);
	struct ast *n = new_ast(SEM_EQ, id, *a);
	equations[last_eq_index] = get_sem_tree_copy(*a);
	*a = n;
	//initial_equations[indx] = get_sem_tree_copy(*a);
	/*if (indx == -1) {
		char buf[10] = {0};
		sprintf(buf, "%d" , ++last_eq_index);
		struct ast *id = new_id(buf);
		struct ast *n = new_ast(SEM_EQ, id, *a);
		indx = last_eq_index;
		equations[indx] = get_sem_tree_copy(*a);
		*a = n;
		struct proc_list *p = NULL;
		get_proc_list(*a, &p);
		initial_equations[indx] = p;
	} else {
		char buf[10] = {0};
		sprintf(buf, "%d" , indx);
		struct ast *id = new_id(buf);
		struct ast *n = new_ast(SEM_EQ, id, *a);
		*a = n;
		initial_equations[last_eq_index] = initial_equations[indx];
	}*/

	return 1;
}
int apply_equational_characterization(struct ast *a, struct ast *parent)
{
	if (a == NULL)
		return 0;

	if (a->nodetype == '+') {
		if (a->l && (a->l->nodetype == '^' || a->l->nodetype == '*')) {		
			designate_equation(&a->l->r);
		} 
		if (a->r && (a->r->nodetype == '^' || a->r->nodetype == '*')) {
			designate_equation(&a->r->r);
		}
	} else if (parent == NULL && (a->nodetype == '^' || a->nodetype == '*')) {
			designate_equation(&a->r);
	}
	apply_equational_characterization(a->l, a);
	apply_equational_characterization(a->r, a);
	return 0;
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
		add1->nodetype = '+';

		struct ast *parll1 = malloc(sizeof(struct ast));
		parll1->nodetype = SEM_PARLL;
		parll1->l = a->l;
		parll1->r = a->r;
		
		add1->l = parll1;

		struct ast *add2 = malloc(sizeof(struct ast));
		add2->nodetype = '+';
		
		struct ast *parll2 = malloc(sizeof(struct ast));
		parll2->nodetype = SEM_PARLL;
		parll2->l = a->r;
		parll2->r = a->l;
		
		struct ast *parl = malloc(sizeof(struct ast));
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
	} else {
		int retval = 0;
		if ((retval = convert_par_composition(a->l, a)) != 0)
			return retval;
		if ((retval = convert_par_composition(a->l, a)) != 0)
			return retval;
	}
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
				comp->nodetype = '^';
				struct ast *T = malloc(sizeof(struct ast));
				T->nodetype = SEM_T;
				T->l = NULL;
				T->r = NULL;
				struct ast *tau = malloc(sizeof(struct ast));
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
				printf("\nERROR in convert_min_fixed_point: can't find right operand");
				return;
			}
			struct ast * pr = new_ast(curr_proc->nodetype, curr_proc->l, NULL);
			struct ast * cp = get_sem_tree_copy(tmp);
			tmp->nodetype = '*';
			tmp->r = pr;
			tmp->l = cp;

				
			if (subst == NULL) {
				subst = malloc(sizeof(struct subst_list));
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
				struct subst_list *n = malloc(sizeof(struct subst_list));
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
				printf("\nERROR in convert_min_fixed_point: can't find right operand");
				return;
			}
			struct  ast* pr = new_ast(curr_proc->nodetype, curr_proc->l, NULL);						
			struct ast * proc_comp = new_ast('*', tmp->r, pr);
			
			proc_comp->r = pr;
			tmp->r = proc_comp;
			
			if (subst == NULL) {
				subst = malloc(sizeof(struct subst_list));
				subst->p = new_ast(pr->nodetype, 
						   pr->l, 
						   get_sem_tree_copy(a->l));
				subst->next = NULL;
			} else {
				struct subst_list *n = malloc(sizeof(struct subst_list));
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
		if (a->r && a->r->nodetype != SEM_PAR && !is_exist_communication_op(a->r)) {
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
					   ((struct term_id *)a->l)->name) == 0) {
					
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
	    strcmp(((struct term_id *)a)->name,((struct term_id *)b)->name) != 0) {
		return 0;
	}
	
	if (is_equal_subtree(a->l, b->l) == 0)
		return 0;

	if (is_equal_subtree(a->r, b->r) == 0)
		return 0;
	return 1;
}
void calc_apriori_semantics(struct ast *r) 
{
	search_processes(r);
	struct proc_list *tmp = processes;
	sem_processes = NULL;
	while (tmp) {
		struct ast *sem_proc = afs_to_sem(tmp->proc);
		struct proc_list *proc = malloc(sizeof(struct proc_list));
		proc->proc = sem_proc;
		proc->next = sem_processes;
		sem_processes = proc;
		tmp = tmp->next;
	} 
	proc_sem_to_par_comp();
	/*print_tree(sem_root);*/
	fprintf(yyout, "\nP = ");
	print_sem_equation(sem_root);
	subst = NULL;
	convert_min_fixed_point(sem_root, NULL);
	remove_proc_node(sem_root, NULL);
	fprintf(yyout, " = \n\n = ");
	print_sem_equation(sem_root);	
	
	equations[0] = sem_root;
	int i = 1;
	equations[1] = get_sem_tree_copy(sem_root);

	for (i = 1; i <= 16; i ++) {
		initial_equations[i] = get_sem_tree_copy(equations[i]);
		reduce_substitutions(initial_equations[i]);
		if (i > 1) 
			expand_needed_equations(equations[i]);
		fprintf(yyout, " \nP(%d) = ", i);
		print_sem_equation(equations[i]);
		int retval = 0;
		while(convert_par_composition(equations[i], NULL)) {			
			
			fprintf(yyout, " = \n\n+++ Convert parallel composition +++\n\n = ");
			print_sem_equation(equations[i]);

			do {
				fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
				retval = apply_basis_axioms(equations[i], NULL);
				fprintf(yyout, " +++\n\n = ");
				print_sem_equation(equations[i]);
			} while (retval);
			apply_distributive_law(equations[i], NULL);
			fprintf(yyout, " = \n\n+++ Apply distibutive rule +++\n\n = ");
			print_sem_equation(equations[i]);
			apply_axioms_for_communication(equations[i], NULL);
			fprintf(yyout, " = \n\n+++ Apply axioms for communication +++\n\n = ");
			print_sem_equation(equations[i]);
			apply_communication_rule(equations[i], NULL);
			fprintf(yyout, " = \n\n+++ Apply communication rule +++\n\n = ");
			print_sem_equation(equations[i]);
			do {
				fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
				retval = apply_basis_axioms(equations[i], NULL);
				fprintf(yyout, " +++\n\n = ");
				print_sem_equation(equations[i]);
			} while (retval);
			
			apply_axioms_for_ll_operation(equations[i], NULL);
			fprintf(yyout, " = \n\n+++ Apply axioms for operation LL +++\n\n = ");
			print_sem_equation(equations[i]);
			
			apply_encapsulation_operation(equations[i], NULL);
			fprintf(yyout, " = \n\n+++ Apply encapsulation operation +++\n\n = ");
			print_sem_equation(equations[i]);
			
			do {
				fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
				retval = apply_basis_axioms(equations[i], NULL);
				fprintf(yyout, " +++\n\n = ");
				print_sem_equation(equations[i]);
			} while (retval);
		}

		reduce_substitutions(equations[i]);
		fprintf(yyout, " = \n\n+++ Reduce equations  +++\n\n");
		print_sem_equation(equations[i]);
		
		do {
			fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
			retval = apply_basis_axioms(equations[i], NULL);
			fprintf(yyout, " +++\n\n = ");
			print_sem_equation(equations[i]);
		} while (retval);
		apply_axioms_for_ll_operation(equations[i], NULL);
		fprintf(yyout, " = \n\n+++ Apply axioms for operation LL +++\n\n = ");
		print_sem_equation(equations[i]);
		
		apply_equational_characterization(equations[i], NULL);
		fprintf(yyout, " = \n\n+++ Apply equational_characterization +++\n\nP(%d) = ", i);
		print_sem_equation(equations[i]);
		fprintf(yyout, " \n\nlast index = %d \n", last_eq_index);
		fprintf(yyout, " \n//////////////////////////////////////////////////////////// \n\n", i);
	}
	fprintf(yyout, " = \n\n +++ Initial equations  +++\n\n");
	for (i = 1; i <=16; i++) {
		fprintf(yyout, "\n++++++++++++++\nP(%d) = ", i);
		print_sem_equation(initial_equations[i]);
		fprintf(yyout, " = ");
		print_sem_equation(equations[i]);
	}
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
				fprintf(yyout, "P%s", ((struct term_id*)a->l)->name);
			} else {
				fprintf(yyout, "K%s", ((struct term_id*)a->l)->name);
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
	}


}
