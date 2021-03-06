D			[0-9]
L			[a-zA-Z_]
H			[a-fA-F0-9]
E			[Ee][+-]?{D}+
FS			(f|F|l|L)
IS			(u|U|l|L)*
%option noyywrap yylineno
%x IFILE

%{
#include <stdio.h>
#include "parser-afs.tab.h"
#include "parser.h"

%}
%%

"BEGIN"			{ count(); return (BEG); }
"END"			{ count(); return (END); }
"NET"			{ count(); return (NET); }
"CHAN"			{ count(); return (CHAN); }
"SHARED_VAR"		{ count(); return (SHARED_VAR); }
"ALL"			{ count(); return (ALL); }
"ANY"			{ count(); return (ANY); }
"FUN"			{ count(); return (FUN); }
"skip"			{ count(); return (SKIP); }
"exit"			{ count(); return (EXIT); }
"break"			{ count(); return (BREAK); }
"wait"			{ count(); return (WAIT); }
"read"			{ count(); return (READ); }
"write"			{ count(); return (WRITE); }
"get"			{ count(); return (GET); }
"set"			{ count(); return (SET); }
"SEQ"			{ count(); return (SEQ); }
"PAR"			{ count(); return (PAR); }
"ALT"			{ count(); return (ALT); }
"LOOP"			{ count(); return (LOOP); }
"tt"			{ count(); return (TRUE); }
"ff"			{ count(); return (FALSE); }
"b"			{ count(); return (BOOL); }
"a"			{ count(); return (COM); }


{L}({L}|{D})*		{
				yylval.id = strdup(yytext);
				count(); 
				return (IDENTIFIER);
			}

0[xX]{H}+{IS}?		{ count(); yylval.id = strdup(yytext); return (IDENTIFIER); }
0{D}+{IS}?		{ count(); yylval.id = strdup(yytext); return (IDENTIFIER); }
{D}+{IS}?		{ count(); yylval.id = strdup(yytext); return (IDENTIFIER); }
L?'(\\.|[^\\'])+'	{ count(); yylval.id = strdup(yytext); return (IDENTIFIER); }

{D}+{E}{FS}?		{ count(); yylval.id = strdup(yytext); return (IDENTIFIER); }
{D}*"."{D}+({E})?{FS}?	{ count(); yylval.id = strdup(yytext); return (IDENTIFIER); }
{D}+"."{D}*({E})?{FS}?	{ count(); yylval.id = strdup(yytext); return (IDENTIFIER); }


"->"			{ count(); return(NEXT); }
(";"|";;")		{ count(); yylval.tok = ';'; return(';'); }
","			{ count(); yylval.tok = ','; return(','); }
":"			{ count(); yylval.tok = ':'; return(':'); }
"="			{ count(); yylval.tok = '='; return('='); }
"("			{ count(); yylval.tok = '('; return('('); }
")"			{ count(); yylval.tok = ')'; return(')'); }
"["                     { count(); yylval.tok = '['; return('['); }
"]"                     { count(); yylval.tok = ']'; return(']'); }
"."			{ count(); yylval.tok = '.'; return('.'); }


[ \t\v\n\f]		{ count (); }

.			{ /* ignore bad characters */ }

%%

#include "parser.c"