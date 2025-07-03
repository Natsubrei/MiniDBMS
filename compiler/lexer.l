%{
#include "parser.tab.h"
#include <string.h>
#include <stdlib.h>
%}

%%
--.*                                    ; // 跳过SQL注释
[ \t\r\n]+                              ; // 跳过空白符

[Cc][Rr][Ee][Aa][Tt][Ee]                {return CREATE;}
[Dd][Aa][Tt][Aa][Bb][Aa][Ss][Ee]        {return DATABASE;}
[Dd][Aa][Tt][Aa][Bb][Aa][Ss][Ee][Ss]    {return DATABASES;}
[Uu][Ss][Ee]                            {return USE;}
[Tt][Aa][Bb][Ll][Ee]                    {return TABLE;}
[Ss][Hh][Oo][Ww]                        {return SHOW;}
[Tt][Aa][Bb][Ll][Ee][Ss]                {return TABLES;}
[Ii][Nn][Ss][Ee][Rr][Tt]                {return INSERT;}
[Ii][Nn][Tt][Oo]                        {return INTO;}
[Vv][Aa][Ll][Uu][Ee][Ss]                {return VALUES;}
[Ss][Ee][Ll][Ee][Cc][Tt]                {return SELECT;}
[Ff][Rr][Oo][Mm]                        {return FROM;}
[Ww][Hh][Ee][Rr][Ee]                    {return WHERE;}
[Aa][Nn][Dd]                            {return AND;}
[Oo][Rr]                                {return OR;}
[Uu][Pp][Dd][Aa][Tt][Ee]                {return UPDATE;}
[Ss][Ee][Tt]                            {return SET;}
[Dd][Ee][Ll][Ee][Tt][Ee]                {return DELETE;}
[Dd][Rr][Oo][Pp]                        {return DROP;}
[Ee][Xx][Ii][Tt]                        {return EXIT;}

[Ii][Nn][Tt]                            { yylval.str = strdup("INT"); return INT; }
[Cc][Hh][Aa][Rr][ \t]*\([0-9]+\)        { yylval.str = strdup(yytext); return CHAR; }
[0-9]+                                  { yylval.num = atoi(yytext); return NUMBER; }
'[^']*'                                 { yylval.str = strdup(yytext + 1); yylval.str[strlen(yylval.str) - 1] = 0; return STRING; }
[a-zA-Z_][a-zA-Z0-9_]*                  { yylval.str = strdup(yytext); return IDENTIFIER; }

"="                                     {return '=';}
"<>"                                    {return NEQ;}
"!="                                    {return NEQ;}
">="                                    {return GEQ;}
"<="                                    {return LEQ;}
">"                                     {return '>';}
"<"                                     {return '<';}
","                                     { return ','; }
";"                                     { return ';'; }
"("                                     { return '('; }
")"                                     { return ')'; }

.                                       { return yytext[0]; }
%%

int yywrap(void) {
    return 1;
}
