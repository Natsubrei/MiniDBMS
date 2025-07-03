cd .\compiler\
bison -d parser.y
flex lexer.l
cd ..
gcc -o MiniDBMS main.c compiler/parser.tab.c compiler/lex.yy.c  database/sql_struct.c database/db_api.c
