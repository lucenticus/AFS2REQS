%token BEG END NET CHAN ALL ANY FUN SKIP EXIT BREAK WAIT READ WRITE
%token SEQ PAR ALT LOOP IDENTIFIER TRUE FALSE BOOL  NEXT COM

%start pr

%{
#include <stdio.h>
#include "lex.yy.c"
%}


%union {
	struct ast *a;
	char *id;
	int tok;
}

%type <a> pr chan type fproc c gc g
%type <id> IDENTIFIER
%type <tok> ALL ANY COM SKIP EXIT BREAK WAIT READ WRITE SEQ PAR ALT LOOP TRUE FALSE BOOL

%%

pr      : NET chan BEG fproc END 
	{ root = new_ast(NODE_PROGRAM, $2, $4); }

chan    : CHAN IDENTIFIER ':' ':' type '('IDENTIFIER')' ':' type '(' IDENTIFIER ')' 
	{ $$ = new_chan(new_id($2), $5, new_id($7), $10, new_id($12)); }

        | chan chan
	{ $$ = new_ast(NODE_CHAN_LIST, $1, $2); }

type    : ALL 
	{ $$ = new_ast($1, NULL, NULL); }
       
	| ANY 
	{ $$ = new_ast($1, NULL, NULL); }
	

fproc   : FUN IDENTIFIER ':' ':' c
	{ $$ = new_ast(NODE_FUNC, new_id($2), $5); }

	| fproc fproc
	{ $$ = new_ast(NODE_FUNC_LIST, $1, $2); }

c       : COM 
	{ $$ = new_ast($1, NULL, NULL); }

	| c ';'
	{ $$ = new_ast(NODE_COM_LIST, $1, NULL); }

	| c ';' c
	{ $$ = new_ast(NODE_COM_LIST, $1, $3); }

	| SKIP
	{ $$ = new_ast($1, NULL, NULL); }

	| EXIT
	{ $$ = new_ast($1, NULL, NULL); }

	| BREAK
	{ $$ = new_ast($1, NULL, NULL); }

	| WAIT '(' IDENTIFIER ')'
	{ $$ = new_ast($1, new_id($3), NULL); }

	| READ '(' IDENTIFIER ',' IDENTIFIER ')'
	{ $$ = new_ast($1, new_id($3), new_id($5)); }

	| WRITE '(' IDENTIFIER ',' IDENTIFIER ')'
	{ $$ = new_ast($1, new_id($3), new_id($5)); }

	| SEQ '(' c ')'
	{ $$ = new_ast($1, $3, NULL); }

	| SEQ '(' c ',' c ')'
	{ $$ = new_ast($1, $3, $5); }

	| PAR '(' c ')'
	{ $$ = new_ast($1, $3, NULL); }

	| PAR '(' c ',' c ')'
	{ $$ = new_ast($1, $3, $5); }

	| ALT '(' gc ')'
	{ $$ = new_ast($1, $3, NULL); }

	| LOOP '(' ALT '(' gc ')' ')'
	{ $$ = new_ast($1, $5, NULL); }

gc      : g NEXT c
	{ $$ = new_ast(NODE_GC, $1, $3); }

	| gc ';' gc
	{ $$ = new_ast(NODE_GC_LIST, $1, $3); }

g	: TRUE
	{ $$ = new_ast($1, NULL, NULL); }

	| FALSE
	{ $$ = new_ast($1, NULL, NULL); }

	| BOOL
	{ $$ = new_ast($1, NULL, NULL); }

	| WAIT '(' IDENTIFIER ')'
	{ $$ = new_ast($1, new_id($3), NULL); }

	| READ '(' IDENTIFIER ',' IDENTIFIER ')'
	{ $$ = new_ast($1, new_id($3), new_id($5)); }

	| WRITE '(' IDENTIFIER ',' IDENTIFIER ')' 
	{ $$ = new_ast($1, new_id($3), new_id($5)); }

%%


/*extern char yytext[];*/
extern int column;

int main(int argc, char *argv[]) 
{
	if (argc < 3) {
		printf("usage:%s <afs file> <output file>\n", argv[0]);
		return 1;
	} else { 
		FILE *in, *out; 
		if ((in = fopen(argv[1], "r")) == NULL) {
			printf("%s: can't open file: %s\n", argv[0], argv[1]);
			return 1;
		}
	 
		if ((out = fopen(argv[2], "w")) == NULL) {
			printf("%s: can't open file: %s\n", argv[0], argv[2]);
			return 1;
		}
		yyin = in;
		yyout = out;
		yyparse();
		calc_apriori_semantics(root);
		fclose(in);
		fclose(out);
	}
	return 1;
}
yyerror(char *s)
{
	fflush(stdout);
	fprintf(yyout, "\n%*s\n%*s\n", column, "^", column, s);
}
