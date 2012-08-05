%token BEGIN END NET CHAN ALL ANY FUN skip exit break wait read write
%token SEQ PAR ALT LOOP

%start translation_unit

%{
#include <stdio.h>
#include "lex.yy.c"
%}

%union {
    struct ast *a;
    char *id;
    int	  tok;
}
%%
pr      : NET can BEGIN fproc END 
can     : CHAN IDENTIFIER ':' ':' type '('IDENTIFIER')' ':' type '(' IDENTIFIER ')' 
        | 

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
