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
		fprintf(yyout, "K_%s = (CIN(", ch->id->name);
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
		fprintf(yyout, "P_%s = ", ((struct term_id *)a->l)->name);
		struct ast *proc = malloc(sizeof(struct ast));
		proc->nodetype = SEM_PROC;
		proc->r = afs_to_sem(a->r); 
		proc->l = a->l;
		fprintf(yyout, "\n");
		return proc;
	} else if (a->nodetype == COM) {
		fprintf(yyout, "A_%s", ((struct term_id *)a->l)->name);
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
			if (parent->nodetype == '+') {
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
			if (parent->nodetype == '+') {
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
			// (X + Y) * Z = X * Z + Y *Z
			// TODO
			fprintf(yyout, "(X + Y) * Z = X * Z + Y *Z");
			return 0;
		} else if (a->l && a->l->nodetype == '^') {
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
int convert_par_composition(struct ast *a, struct ast *parent) 
{
	if (a == NULL)
		return 0;
	
	if (a->nodetype == SEM_PAR) {
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
		if(a->l != NULL && a->l->nodetype == '^') {
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
			struct ast * proc_comp = malloc(sizeof(struct ast));
			proc_comp->nodetype = '*';
			proc_comp->l = tmp->r;
			struct ast * proc = malloc(sizeof(struct ast));
			proc->nodetype = curr_proc->nodetype;
			proc->l = curr_proc->l;
			proc->r = NULL;
			proc_comp->r = proc;
			tmp->r = proc_comp;
		} else if (a->l != NULL) {			
			struct ast * proc = malloc(sizeof(struct ast));
			proc->nodetype = curr_proc->nodetype;
			proc->l = curr_proc->l;

			proc->r = NULL;
			a->r = proc;
			a->nodetype = '*';
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
	convert_min_fixed_point(sem_root, NULL);
	fprintf(yyout, " = \n\n = ");
	print_sem_equation(sem_root);	

	remove_proc_node(sem_root, NULL);
	
	while(convert_par_composition(sem_root, NULL)) {
		fprintf(yyout, " = \n\n+++ Convert parallel composition +++\n\n = ");
		print_sem_equation(sem_root);
		apply_distributive_law(sem_root, NULL);
		fprintf(yyout, " = \n\n+++ Apply distibutive rule +++\n\n = ");
		print_sem_equation(sem_root);
		apply_axioms_for_communication(sem_root, NULL);
		fprintf(yyout, " = \n\n+++ Apply axioms for communication +++\n\n = ");
		print_sem_equation(sem_root);
		apply_communication_rule(sem_root, NULL);
		fprintf(yyout, " = \n\n+++ Apply communication rule +++\n\n = ");
		print_sem_equation(sem_root);
		int retval = 0;
		do {
			fprintf(yyout, " = \n\n+++ Apply basis axiom : ");
			retval = apply_basis_axioms(sem_root, NULL);
			fprintf(yyout, " +++\n\n = ");
			print_sem_equation(sem_root);
		} while (retval);
		
		apply_axioms_for_ll_operation(sem_root, NULL);
		fprintf(yyout, " = \n\n+++ Apply axioms for operation LL +++\n\n = ");
		print_sem_equation(sem_root);
		
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
				fprintf(yyout, "P_%s", ((struct term_id*)a->l)->name);
			} else {
				fprintf(yyout, "K_%s", ((struct term_id*)a->l)->name);
			}
		} else {
			fprintf(yyout, "(");
			/*print_sem_equation(a->l);*/
			print_sem_equation(a->r);
			fprintf(yyout, ")");
		}
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
		fprintf(yyout, " || \n || ");
		if (a->r && a->r->nodetype == '+') {
			fprintf(yyout, "( ");
			print_sem_equation(a->r);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->r);
	} else if (a->nodetype == SEM_PARLL) {
		if (a->l && a->l->nodetype == '+') {
			fprintf(yyout, "( ");
			print_sem_equation(a->l);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->l);
		fprintf(yyout, " LL \n LL ");
		if (a->r && a->r->nodetype == '+') {
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
	} /*else if (a->nodetype == '^') {
		if (a->l && 
		    (a->l->nodetype == '|' ||
		     a->l->nodetype == SEM_PAR)
		    ) {
			fprintf(yyout, "( ");
			print_sem_equation(a->l);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->l);
		fprintf(yyout, " %c ", a->nodetype);
		if (a->r && 
		    (a->r->nodetype == '|' ||
		     a->r->nodetype == SEM_PAR)
		    ) {
			fprintf(yyout, "( ");
			print_sem_equation(a->r);
			fprintf(yyout, " )");
		} else 
			print_sem_equation(a->r);
	}*/ else if (a->nodetype == '^' ||
		   a->nodetype == '+' ||
		   a->nodetype == '*' ||
		   a->nodetype == 'U' ) {
		print_sem_equation(a->l);
		fprintf(yyout, " %c ", a->nodetype);
		print_sem_equation(a->r);
	}


}
