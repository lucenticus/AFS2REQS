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
		fprintf(yyout, "K_%s = (IN(", ch->id->name);
		fprintf(yyout, "%s, %s) * ", 
			ch->id->name, ch->in_id->name);
		fprintf(yyout, "OUT(%s, %s))#", 
			ch->id->name, ch->out_id->name);
		fprintf(yyout, "\n");
		struct ast *proc = malloc(sizeof(struct ast));
		proc->nodetype = SEM_PROC;
		proc->l = (struct ast*) ch->id;
		struct ast *fix_op = malloc(sizeof(struct ast));
		fix_op->nodetype = '#';
		struct ast *comp_op = malloc(sizeof(struct ast));
		comp_op->nodetype = '*';
		struct ast *in = malloc(sizeof(struct ast));
		in->nodetype = SEM_IN;
		in->l = (struct ast*) ch->id;
		in->r = (struct ast*) ch->in_id;

		struct ast *out = malloc(sizeof(struct ast));
		out->nodetype = SEM_OUT;
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
		fprintf(yyout, "A");
		struct ast *com = malloc(sizeof(struct ast));
		com->nodetype = SEM_COM;
		com->l = NULL;
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
}

void print_sem_equation(struct ast *a) 
{
	if (a == NULL)
		return;

	if (a->nodetype == NODE_ID) {
		if (((struct term_id *) a)->name != NULL)
			fprintf(yyout, "%s",((struct term_id *) a)->name);
	} else if (a->nodetype == SEM_PROC) {
		fprintf(yyout, "(");
		/*print_sem_equation(a->l);*/
		print_sem_equation(a->r);
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
		print_sem_equation(a->l);
		fprintf(yyout, "||");
		print_sem_equation(a->r);
	} else if (a->nodetype == '#') {
		fprintf(yyout, "(");
		print_sem_equation(a->l);
		print_sem_equation(a->r);
		fprintf(yyout, ")");
		fprintf(yyout, "%c", a->nodetype);
	} else if (a->nodetype == '+' ||
		   a->nodetype == '^' ||
		   a->nodetype == '*' ||
		   a->nodetype == 'U') {
		print_sem_equation(a->l);
		fprintf(yyout, "%c", a->nodetype);
		print_sem_equation(a->r);
	}


}
