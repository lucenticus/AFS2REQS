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
#include "parser-afs.h"
%}
%%

"BEGIN"			{ count(); return(BEGIN); }
"END"			{ count(); return(END); }
"NET"			{ count(); return(NET); }
"CHAN"			{ count(); return(CHAN); }
"ALL"			{ count(); return(ALL); }
"ANY"			{ count(); return(ANY); }
"FUN"			{ count(); return(FUN); }
"skip"			{ count(); return(skip); }
"exit"			{ count(); return(exit); }
"break"			{ count(); return(break); }
"wait"			{ count(); return(wait); }
"read"			{ count(); return(read); }
"write"			{ count(); return(write); }
"SEQ"			{ count(); return(SEQ); }
"PAR"			{ count(); return(PAR); }
"ALT"			{ count(); return(ALT); }
"LOOP"			{ count(); return(LOOP); }
"tt"			{ count(); return(TRUE); }
"ff"			{ count(); return(FALSE); }
"b"			{ count(); return(BOOL); }

{L}({L}|{D})*		{
				yylval.id = strdup(yytext);
				count(); 
				return (IDENTIFIER);
			}

0[xX]{H}+{IS}?		{ count(); yylval.id = strdup(yytext); return(IDENTIFIER); }
0{D}+{IS}?		{ count(); yylval.id = strdup(yytext); return(IDENTIFIER); }
{D}+{IS}?		{ count(); yylval.id = strdup(yytext); return(IDENTIFIER); }
L?'(\\.|[^\\'])+'	{ count(); yylval.id = strdup(yytext); return(IDENTIFIER); }

{D}+{E}{FS}?		{ count(); yylval.id = strdup(yytext); return(IDENTIFIER); }
{D}*"."{D}+({E})?{FS}?	{ count(); yylval.id = strdup(yytext); return(IDENTIFIER); }
{D}+"."{D}*({E})?{FS}?	{ count(); yylval.id = strdup(yytext); return(IDENTIFIER); }


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

#include "parser-afs.c"