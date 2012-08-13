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

void afs_to_sem(struct ast *a)
{
	if (a == NULL)
		return;
	if (a->nodetype == NODE_ID) {
		/*if (((struct term_id *) a)->name != NULL)
		  printf("id:%s\n",((struct term_id *) a)->name);*/
	} else if (a->nodetype == NODE_CHAN) {
		struct term_chan * ch = (struct term_chan *) a;
		fprintf(yyout, "K_%s = (IN(", ch->id->name);
		fprintf(yyout, "%s, %s) * ", 
			ch->id->name, ch->in_id->name);
		fprintf(yyout, "OUT(%s, %s))+", 
			ch->id->name, ch->out_id->name);
		fprintf(yyout, "\n");
	} else if (a->nodetype == NODE_FUNC) {
		fprintf(yyout, "P_%s = ", ((struct term_id *)a->l)->name);
		afs_to_sem(a->r);  
		printf("PROC:");
		print_tree(a->r);
		fprintf(yyout, "\n");
	} else if (a->nodetype == COM) {
		fprintf(yyout, "A");
	} else if (a->nodetype == TRUE) {
		fprintf(yyout, "T");
	} else if (a->nodetype == FALSE) {
		fprintf(yyout, "F");
	} else if (a->nodetype == BOOL) {
		fprintf(yyout, "B");
	} else if (a->nodetype == SKIP) {
		fprintf(yyout, "tau");
	} else if (a->nodetype == EXIT) {
		fprintf(yyout, "EXIT");
	} else if (a->nodetype == BREAK) {
		fprintf(yyout, "BREAK");
	} else if (a->nodetype == WAIT) {
		fprintf(yyout, "TIME");
	} else if (a->nodetype == READ) {
		fprintf(yyout, "IN");
		fprintf(yyout, "(%s, %s)", 
			((struct term_id *)a->l)->name, 
			((struct term_id *)a->r)->name);
	} else if (a->nodetype == WRITE) {
		fprintf(yyout, "OUT");
		fprintf(yyout, "(%s, %s)", 
			((struct term_id *)a->l)->name, 
			((struct term_id *)a->r)->name);
	} else if (a->nodetype == SEQ) {
		afs_to_sem(a->l);
		fprintf(yyout, " * ");
		afs_to_sem(a->r);       
	} else if (a->nodetype == PAR) {
		afs_to_sem(a->l);
		fprintf(yyout, " U ");
		afs_to_sem(a->r);       
	} else if (a->nodetype == ALT) {
		afs_to_sem(a->l);
		if (a->r) {
			fprintf(yyout, " + ");
			afs_to_sem(a->r);       
		}
	} else if (a->nodetype == LOOP) {
		fprintf(yyout, "(");
		afs_to_sem(a->l);
		fprintf(yyout, ")+");
	} else if (a->nodetype == NODE_GC) {
		afs_to_sem(a->l);
		fprintf(yyout, " ^ ");
		afs_to_sem(a->r); 
	} else if (a->nodetype == NODE_GC_LIST) {
		afs_to_sem(a->l);
		fprintf(yyout, " + ");
		afs_to_sem(a->r);       		
	}
}
void calc_apriori_semantics(struct ast *r) 
{
	search_processes(r);
	struct proc_list *tmp = processes;
	
	while (tmp) {
		/*printf ("PROCESS: \n");
		print_tree(tmp->proc);*/
		afs_to_sem(tmp->proc);
		tmp = tmp->next;
	}
}
