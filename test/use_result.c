/*
  Copyright (c) 2001, 2012, Oracle and/or its affiliates. All rights reserved.
                2013 MontyProgram AB

  The MySQL Connector/ODBC is licensed under the terms of the GPLv2
  <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>, like most
  MySQL Connectors. There are special exceptions to the terms and
  conditions of the GPLv2 as it is applied to this software, see the
  FLOSS License Exception
  <http://www.mysql.com/about/legal/licensing/foss-exception.html>.
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; version 2 of the License.
  
  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
  for more details.
  
  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "tap.h"

SQLINTEGER my_max_rows= 100;


/* making use of mysql_use_result */
ODBC_TEST(t_use_result)
{
  SQLINTEGER i, row_count= 0;
  SQLCHAR    ch[]= "MySQL AB";
  SQLRETURN  rc;

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS t_use_result");
  OK_SIMPLE_STMT(Stmt, "CREATE TABLE t_use_result (id INT, name CHAR(10))");

  CHECK_STMT_RC(Stmt, SQLPrepare(Stmt, (SQLCHAR *)
                            "INSERT INTO t_use_result VALUES (?,?)", SQL_NTS));

  CHECK_STMT_RC(Stmt, SQLBindParameter(Stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG,
                                  SQL_INTEGER, 0, 0, &i, 0, NULL));

  CHECK_STMT_RC(Stmt, SQLBindParameter(Stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR,
                                  SQL_CHAR, 0, 0, ch, sizeof(ch), NULL));

  for (i= 1; i <= my_max_rows; i++)
    CHECK_STMT_RC(Stmt, SQLExecute(Stmt));

  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_RESET_PARAMS));
  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_CLOSE));

  OK_SIMPLE_STMT(Stmt, "SELECT * FROM t_use_result");

  rc= SQLFetch(Stmt);
  while (rc == SQL_SUCCESS || rc == SQL_SUCCESS_WITH_INFO)
  {
    row_count++;
    rc= SQLFetch(Stmt);
  }

  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_UNBIND));
  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_CLOSE));

  is_num(row_count, my_max_rows);

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS t_use_result");

  return OK;
}


/**
 Bug #4657: "Don't Cache Results" crashes when using catalog functions
*/
ODBC_TEST(t_bug4657)
{
  SQLCHAR     name[10];
  SQLSMALLINT column_count;
  SQLLEN      name_length;

  CHECK_STMT_RC(Stmt, SQLSetStmtAttr(Stmt, SQL_ATTR_CURSOR_TYPE,
                                (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY, 0));

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS t_bug4657");
  OK_SIMPLE_STMT(Stmt, "CREATE TABLE t_bug4657 (a INT)");

  CHECK_STMT_RC(Stmt, SQLTables(Stmt, (SQLCHAR *)"", SQL_NTS,
                           (SQLCHAR *)"", SQL_NTS,
                           (SQLCHAR *)"", SQL_NTS,
                           (SQLCHAR *)"UNKNOWN", SQL_NTS));

  CHECK_STMT_RC(Stmt, SQLNumResultCols(Stmt, &column_count));
  is_num(column_count, 5);

  CHECK_STMT_RC(Stmt, SQLBindCol(Stmt, 3, SQL_C_CHAR, name, sizeof(name),
                            &name_length));
  FAIL_IF(SQLFetch(Stmt) != SQL_NO_DATA_FOUND, "expected end of data");

  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_UNBIND));
  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_CLOSE));

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS t_bug4657");

  return OK;
}


/**
 Bug #39878: No error signaled if timeout during fetching data.
*/
ODBC_TEST(t_bug39878)
{
  int         i;
  SQLINTEGER  row_count= 0;
  SQLRETURN   rc;

  CHECK_STMT_RC(Stmt, SQLSetStmtAttr(Stmt, SQL_ATTR_CURSOR_TYPE,
                                (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY, 0));

  diag("Creating table t_bug39878");

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS t_bug39878");
  OK_SIMPLE_STMT(Stmt, "CREATE TABLE t_bug39878 (a INT)");

  // Fill table with data
  diag("Filling table with data...");

  OK_SIMPLE_STMT(Stmt, "INSERT INTO t_bug39878 VALUES (0), (1)");

  CHECK_STMT_RC(Stmt, SQLPrepare(Stmt, (SQLCHAR *)
                            "INSERT INTO t_bug39878 SELECT a+? FROM t_bug39878", SQL_NTS));

  CHECK_STMT_RC(Stmt, SQLBindParameter(Stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG,
                                  SQL_INTEGER, 0, 0, &row_count, 0, NULL));

  for (i=1, row_count= 2; i < 5; ++i, row_count *= 2)
    CHECK_STMT_RC(Stmt, SQLExecute(Stmt));

  diag("inserted %d rows.", row_count);

  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_RESET_PARAMS));
  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_CLOSE));

  diag("Setting net_write_timeout to 1");
  OK_SIMPLE_STMT(Stmt, "SET net_write_timeout=1");

  // Table scan 

  OK_SIMPLE_STMT(Stmt, "SELECT * FROM t_bug39878");
  diag("Started table scan, sleeping 3sec ...");
  Sleep(3000);

  diag("Fetching rows...");

  while (SQL_SUCCEEDED(rc= SQLFetch(Stmt)))
  {
    row_count--;
    if (!(row_count % 1000)) diag("%d", row_count);
  }

  //print_diag(rc, SQL_HANDLE_STMT, Stmt, "SQLFetch()", __FILE__, __LINE__);
  diag("Scan interrupted, %d rows left in the table.", row_count);

  {
    char *rc_name;
    switch(rc)
    {
    case SQL_SUCCESS:           rc_name= "SQL_SUCCESS";           break;
    case SQL_SUCCESS_WITH_INFO: rc_name= "SQL_SUCCESS_WITH_INFO"; break;
    case SQL_NO_DATA:           rc_name= "SQL_NO_DATA";           break;
    case SQL_STILL_EXECUTING:   rc_name= "SQL_STILL_EXECUTING";   break;
    case SQL_ERROR:             rc_name= "SQL_ERROR";             break;
    case SQL_INVALID_HANDLE:    rc_name= "SQL_INVALID_HANDLE";    break;
    default:                    rc_name= "<unknown>";             break;
    }
    diag("Last SQLFetch() returned: %s\nRows fetched: %d", rc_name, row_count);
  }

  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_UNBIND));
  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_CLOSE));

  FAIL_IF(row_count > 0 && rc != SQL_ERROR, "rows == 0 or SQLError expected");

  // We re-connect to drop the table (as connection might be broken)
  ODBC_Disconnect(Env, Connection, Stmt);
  ODBC_Connect(&Env, &Connection, &Stmt);
  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS t_bug39878");

  return OK;
}


MA_ODBC_TESTS my_tests[]=
{
  {t_use_result, "t_use_result"},
  {t_bug4657, "t_bug4657"},
  {t_bug39878, "t_bug39878"},
  {NULL, NULL}
};

int main(int argc, char **argv)
{
  int tests= sizeof(my_tests)/sizeof(MA_ODBC_TESTS) - 1;
  get_options(argc, argv);
  plan(tests);
  return run_tests(my_tests);
}
