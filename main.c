#include <stdio.h>
#include "database/db_api.h"
#include "database/sql_struct.h"
#include "compiler/parser.tab.h"

typedef void *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *str);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern int yyparse(void);

int main()
{
    load_db(); // 启动时自动加载数据库
    if (!find_db("default"))
    {
        db_create_database("default");
    }
    db_use_database("default"); // 自动切换到 DEFAULT 数据库
    printf("Welcome to MiniDBMS Shell. Type SQL and press Enter.\n");
    char input[2048];
    while (1)
    {
        printf("MiniDBMS> ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin))
            break;
        // 跳过空行
        if (input[0] == '\0' || input[0] == '\n')
            continue;
        YY_BUFFER_STATE bp = yy_scan_string(input);
        yyparse();
        yy_delete_buffer(bp);
    }
    return 0;
}
