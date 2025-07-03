%{
#include "../database/db_api.h"
#include "../database/sql_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void yyerror(const char *s);
int yylex(void);
%}

%union {
    char* str;
    int num;
    struct ColumnDef* coldef;
    struct ColumnDef* coldefs;
    struct ColumnList* collist;
    struct Value* value;
    struct Value* vlist;
    struct SelectList* sellist;
    struct Condition* cond;
    struct SetItem* setitem;
    struct SetItem* setlist;
}

// =====================
// 词法/语法类型声明
// =====================
%token <str> IDENTIFIER STRING CHAR INT
%token <num> NUMBER
%token CREATE DATABASE DATABASES USE TABLE SHOW TABLES INSERT INTO VALUES SELECT FROM WHERE UPDATE SET DELETE DROP EXIT
%token NEQ GEQ LEQ AND OR

// 语法规则的值类型声明
%type <coldef> column_def                               // 单个列定义
%type <coldefs> column_defs                             // 列定义链表
%type <collist> column_list opt_column_list table_list  // 列名/表名链表
%type <value> value                                     // 单个值
%type <vlist> value_list                                // 值链表
%type <sellist> select_list select_items                // SELECT字段链表
%type <cond> where_clause_opt condition predicate       // 条件表达式
%type <setitem> set_item                                // SET项
%type <setlist> set_list                                // SET项链表

%%

// =====================
// 输入与主控规则
// =====================
input:
    /* empty */
  | input statement
  ;

statement:
    show_databases_stmt
  | create_database_stmt
  | use_database_stmt
  | show_tables_stmt
  | create_table_stmt
  | insert_stmt
  | select_stmt
  | update_stmt
  | delete_stmt
  | drop_table_stmt
  | drop_database_stmt
  | exit_stmt
  ;

show_databases_stmt:
    SHOW DATABASES ';'
    { db_show_databases(); }
  ;

create_database_stmt:
    CREATE DATABASE IDENTIFIER ';'
    { db_create_database($3); free($3); }
  ;

use_database_stmt:
    USE IDENTIFIER ';'
    { db_use_database($2); free($2); }
  ;

drop_database_stmt:
    DROP DATABASE IDENTIFIER ';'
    { db_drop_database($3); free($3); }
  ;

show_tables_stmt:
    SHOW TABLES ';'
    { db_show_tables(); }
  ;

create_table_stmt:
    CREATE TABLE IDENTIFIER '(' column_defs ')' ';'
    { db_create_table($3, $5); free($3); free_column_defs($5); }
  ;

column_defs:
    column_def                    { $$ = create_column_defs(NULL, $1); }
  | column_defs ',' column_def    { $$ = create_column_defs($1, $3); }
  ;

column_def:
    IDENTIFIER INT    { $$ = create_column_def($1, $2); free($1); free($2); }
  | IDENTIFIER CHAR   { $$ = create_column_def($1, $2); free($1); free($2); }
  ;

drop_table_stmt:
    DROP TABLE IDENTIFIER ';'
    { db_drop_table($3); free($3); }
  ;

insert_stmt:
    INSERT INTO IDENTIFIER opt_column_list VALUES value_list ';'
    { db_insert($3, $4, $6); free($3); }
  ;

opt_column_list:
    /* empty */ { $$ = NULL; }
  | '(' column_list ')' { $$ = $2; }
  ;

column_list:
    IDENTIFIER { $$ = create_column_list($1, NULL); free($1); }
  | column_list ',' IDENTIFIER {
        struct ColumnList* tail = $1;
        while (tail->next) tail = tail->next;
        tail->next = create_column_list($3, NULL);
        $$ = $1;
        free($3);
    }
  ;

value_list:
    '(' value_list ')'    { $$ = $2; }
  | value                 { $$ = create_value_list($1, NULL); }
  | value_list ',' value {
        struct Value* tail = $1;
        while (tail->next) tail = tail->next;
        tail->next = $3;
        $$ = $1;
    }
  ;

value:
    NUMBER    { $$ = create_value_int($1); }
  | STRING    { $$ = create_value_str($1); free($1); }
  ;

select_stmt:
    SELECT select_list FROM table_list where_clause_opt ';'
    { db_select($4, $2, $5); free_select_list($2); free_column_list($4); free_condition($5); }
  ;

select_list:
    '*'             { $$ = NULL; }
  | select_items    { $$ = $1; }
  ;

select_items:
    IDENTIFIER { $$ = create_select_list($1, NULL); free($1); }
  | select_items ',' IDENTIFIER {
        struct SelectList* tail = $1;
        while (tail->next) tail = tail->next;
        tail->next = create_select_list($3, NULL);
        $$ = $1;
        free($3);
    }
  ;

table_list:
    IDENTIFIER { $$ = create_column_list($1, NULL); free($1); }
  | table_list ',' IDENTIFIER {
        struct ColumnList* tail = $1;
        while (tail->next) tail = tail->next;
        tail->next = create_column_list($3, NULL);
        $$ = $1;
        free($3);
    }
  ;

where_clause_opt:
    /* empty */ { $$ = NULL; }
  | WHERE condition { $$ = $2; }
  ;

condition:
    predicate                 { $$ = $1; }
  | '(' condition ')'         { $$ = $2; }
  | condition AND condition   { $$ = create_condition_and($1, $3); }
  | condition OR condition    { $$ = create_condition_or($1, $3); }
  ;

predicate:
    IDENTIFIER '=' value { $$ = create_condition($1, EQ, $3); free($1); }
  | IDENTIFIER NEQ value { $$ = create_condition($1, NEQ_OP, $3); free($1); }
  | IDENTIFIER '>' value { $$ = create_condition($1, GT, $3); free($1); }
  | IDENTIFIER '<' value { $$ = create_condition($1, LT, $3); free($1); }
  | IDENTIFIER GEQ value { $$ = create_condition($1, GE, $3); free($1); }
  | IDENTIFIER LEQ value { $$ = create_condition($1, LE, $3); free($1); }
  ;

update_stmt:
    UPDATE IDENTIFIER SET set_list where_clause_opt ';'
    { db_update($2, $4, $5); free($2); free_set_list($4); free_condition($5); }
  ;

set_list:
    set_item { $$ = create_set_list($1, NULL); }
  | set_list ',' set_item {
        struct SetItem* tail = $1;
        while (tail->next) tail = tail->next;
        tail->next = $3;
        $$ = $1;
    }
  ;

set_item:
    IDENTIFIER '=' value { $$ = create_set_item($1, $3); free($1); }
  ;

delete_stmt:
    DELETE FROM IDENTIFIER where_clause_opt ';'
    { db_delete($3, $4); free($3); free_condition($4); }
  ;

exit_stmt:
    EXIT ';'
    { db_exit(); }
  ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Syntax error: %s\n", s);
}