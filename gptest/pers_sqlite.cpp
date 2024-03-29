#include <stdio.h>
#include <sqlite3.h>
#include <string>
#include <map>
#include <string.h>
#include "fpers.h"

#define CACHE_ALLSYMS
// #define USE_INDEXES

class pers_sqlite: public FPersistence
{
  public:
   pers_sqlite():
    m_db(NULL),
    m_check_sym(NULL),
    m_update_fname(NULL),
    m_insert_sym(NULL),
    m_insert_xref(NULL),
    m_insert_err(NULL),
    m_del_xrefs(NULL),
    m_del_errs(NULL),
    m_has_func_id(false),
    max_id(0),
    m_func_id(0)
   {}
   virtual ~pers_sqlite();
   virtual int connect(const char *, const char *, const char *);
   virtual void disconnect();
   virtual void cu_start(const char *f)
   {
     // TODO: use realpath()
     m_fn = f;
   }
   virtual void bb_start(int idx)
   {
     m_bb = idx;
   }
   virtual int func_start(const char *fn);
   virtual void func_stop()
   {
     m_has_func_id = false;
   }
   virtual void add_xref(xref_kind, const char *);
   virtual void add_literal(const char *, int size);
   virtual void report_error(const char *);
  protected:
   int create_new_db(const char *);
   int prepare();
   int get_max_id();
   int add_func();
   int check_symbol(const char *);
   int check_literal(std::string &);
   void insert_xref(int id, char what);

   sqlite3 *m_db;
   // prepared statements
   sqlite3_stmt *m_check_sym;
   sqlite3_stmt *m_update_fname;
   sqlite3_stmt *m_insert_sym;
   sqlite3_stmt *m_insert_xref;
   sqlite3_stmt *m_insert_err;
   sqlite3_stmt *m_del_xrefs;
   sqlite3_stmt *m_del_errs;
   std::string m_fn;
   std::string m_func;
   bool m_has_func_id;
   int m_func_id;
   int m_bb;
   int max_id;
   // symbols cache
   std::map<std::string, int> m_cache;
};

void pers_sqlite::disconnect()
{
#define FIN_STMT(f) if ( f ) { sqlite3_finalize(f); f = NULL; }
  FIN_STMT( m_check_sym )
  FIN_STMT( m_update_fname )
  FIN_STMT( m_insert_sym )
  FIN_STMT( m_insert_xref )
  FIN_STMT( m_insert_err )
  FIN_STMT( m_del_xrefs )
  FIN_STMT( m_del_errs ) 
  if ( m_db )
  {
    sqlite3_close(m_db);
    m_db = NULL;
  }
}

pers_sqlite::~pers_sqlite()
{
  this->disconnect();  
}

// sample from https://github.com/wikibook/sqlite3/blob/master/2_4_5_sqlite3_CAPI_examples/CAPI_examples/sqlite3_capi_examples/sqlite3_capi_examples/capi_example_1.cpp
const char *cr_symtab = "CREATE TABLE IF NOT EXISTS symtab ("
 "id INTEGER PRIMARY KEY,"
 "name TEXT,"
 "fname TEXT"
 ");"
;

const char *idx_symtab = "CREATE UNIQUE INDEX idx_symtab ON symtab(name);";

const char *cr_error = "CREATE TABLE IF NOT EXISTS errlog ("
 "id INTEGER,"
 "bb INTEGER," 
 "msg TEXT"
 ");"
;

const char *idx_error = "CREATE INDEX idx_errlog ON symtab(id);";

const char *cr_xrefs = "CREATE TABLE IF NOT EXISTS xrefs ("
 "id INTEGER,"
 "bb INTEGER,"
 "kind  CHARACTER(1)," 
 "what INTEGER"
 ");"
;

const char *idx_xrefs = "CREATE INDEX idx_xrefs ON xrefs(id);";

struct sql_tab {
 const char *stmt;
 const char *name;
 int tab; // if 0 - index
};

static const sql_tab sq[] = {
  { cr_symtab, "symtab", 1 },
#ifndef CACHE_ALLSYMS
  { idx_symtab, "idx_symtab", 0},
#endif /* !CACHE_ALLSYMS */
  { cr_error, "errlog", 1 },
#ifdef USE_INDEXES  
  { idx_error, "idx_errlog", 0 },
#endif  
  { cr_xrefs, "xrefs", 1 },
#ifdef USE_INDEXES
  { idx_xrefs, "idx_xrefs", 0 },
#endif    
};

// called on creating db
int pers_sqlite::create_new_db(const char *dbname)
{
  int res = sqlite3_open_v2(dbname, &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
  if ( res != SQLITE_OK )
    return res;
  for ( size_t i = 0; i < sizeof(sq) / sizeof(sq[0]); ++i )
  {
    char* errmsg;
    res = sqlite3_exec(m_db, sq[i].stmt, NULL, NULL, &errmsg);
    if ( res != SQLITE_OK )
    {
      fprintf(stderr, "error %d while create %s %s: %s\n", 
        sq[i].tab ? "table" : "index", res, sq[i].name, errmsg);  
      return res;  
    }
  }
  return SQLITE_OK;
}

// various CRUD statements to prepare, params binded by index
const char *pr_all_sym = "SELECT id, fname FROM symtab;";
const char *pr_check_sym = "SELECT id, fname FROM symtab WHERE name = ?;";
const char *pr_update_fname = "UPDATE symtab SET fname = ? WHERE id = ?;";
const char *pr_insert_sym = "INSERT INTO symtab (id, name, fname) VALUES (?, ?, ?);";
const char *pr_insert_xref = "INSERT INTO xrefs (id, bb, kind, what) VALUES (?, ?, ?, ?);";
const char *pr_insert_err  = "INSERT INTO errlog (id, bb, msg) VALUES (?, ?, ?);";
const char *pr_del_xrefs   = "DELETE FROM xrefs WHERE id = ?;";
const char *pr_del_errs    = "DELETE FROM errlog WHERE id = ?;";
#define STMT(c) c, strlen(c)

int pers_sqlite::prepare()
{
  const char *tail = NULL;
  int res = sqlite3_prepare(m_db, STMT(pr_check_sym), &m_check_sym, &tail);
  if ( res )
  {
    fprintf(stderr, "error %d while prepare check_sym\n", res);  
    return res;  
  }
  res = sqlite3_prepare(m_db, STMT(pr_update_fname), &m_update_fname, &tail);
  if ( res )
  {
    fprintf(stderr, "error %d while prepare update_fname\n", res);  
    return res;  
  } 
  res = sqlite3_prepare(m_db, STMT(pr_insert_sym), &m_insert_sym, &tail);
  if ( res )
  {
    fprintf(stderr, "error %d while prepare insert_sym\n", res);  
    return res;  
  } 
  res = sqlite3_prepare(m_db, STMT(pr_insert_xref), &m_insert_xref, &tail);
  if ( res )
  {
    fprintf(stderr, "error %d while prepare insert_xref\n", res);  
    return res;  
  } 
  res = sqlite3_prepare(m_db, STMT(pr_insert_err), &m_insert_err, &tail);
  if ( res )
  {
    fprintf(stderr, "error %d while prepare insert_err\n", res);  
    return res;  
  } 
  res = sqlite3_prepare(m_db, STMT(pr_del_xrefs), &m_del_xrefs, &tail);
  if ( res )
  {
    fprintf(stderr, "error %d while prepare del_xrefs\n", res);  
    return res;  
  } 
  res = sqlite3_prepare(m_db, STMT(pr_del_errs), &m_del_errs, &tail);
  if ( res )
  {
    fprintf(stderr, "error %d while prepare del_errs\n", res);  
    return res;  
  }
  // finally extract maximal id from symtab
  return get_max_id();
}

int pers_sqlite::get_max_id()
{
  sqlite3_stmt *stmt;  
  const char *sql = "SELECT MAX(id) FROM symtab";
  int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, NULL);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "get_max_id error %d when prepare\n", rc);
    return rc;
  }
  if ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
    max_id = sqlite3_column_int (stmt, 0);
  sqlite3_finalize(stmt);
  return 0;
}

int pers_sqlite::func_start(const char *fn)
{
  m_func = fn;
  m_has_func_id = false;  
  // check if we already have this function
  sqlite3_reset(m_check_sym);
  sqlite3_bind_text(m_check_sym, 1, fn, strlen(fn), SQLITE_STATIC);
  int rc;
  if ((rc = sqlite3_step(m_check_sym)) == SQLITE_ROW )
  {
    // yes, we already have this function
    m_func_id = sqlite3_column_int (m_check_sym, 0);
    m_has_func_id = true;
    // check if it has fname
    auto fname = sqlite3_column_text(m_check_sym, 1);
    if ( !*fname )
    {
      // this function was seen before but not processed yet - push filename for it
      sqlite3_reset(m_update_fname);
      sqlite3_bind_int(m_check_sym, 1, m_func_id);
      sqlite3_bind_text(m_check_sym, 2, m_fn.c_str(), m_fn.size(), SQLITE_STATIC);
      sqlite3_step(m_check_sym); 
    } else {
      // we already processed this function - remove xrefs
      sqlite3_reset(m_del_xrefs);
      sqlite3_bind_int(m_del_xrefs, 1, m_func_id);
      sqlite3_step(m_del_xrefs);
      //  and errors
      sqlite3_reset(m_del_errs);
      sqlite3_bind_int(m_del_errs, 1, m_func_id);
      sqlite3_step(m_del_errs);
    }
  }
  return 1;
}

// called when we must really put some function to db
int pers_sqlite::add_func()
{
  if ( m_has_func_id )
    return 0;
  sqlite3_reset(m_insert_sym);
  m_func_id = ++max_id;
  m_has_func_id = true;
  sqlite3_bind_int(m_insert_sym, 1, m_func_id);
  sqlite3_bind_text(m_insert_sym, 2, m_func.c_str(), m_func.size(), SQLITE_STATIC);
  sqlite3_bind_text(m_insert_sym, 3, m_fn.c_str(), m_fn.size(), SQLITE_STATIC);
  sqlite3_step(m_insert_sym);
  m_cache[m_func] = m_func_id;
  return 1;
}

int pers_sqlite::check_literal(std::string &l)
{
  auto cached = m_cache.find(l);
  if ( cached != m_cache.end() )
    return cached->second;
#ifndef CACHE_ALLSYMS
  // on big codebase this query very quickly becomes tooooo sloooooow
  sqlite3_reset(m_check_sym);
  sqlite3_bind_text(m_check_sym, 1, l.c_str(), l.size(), SQLITE_STATIC);
  int rc;
  if ((rc = sqlite3_step(m_check_sym)) == SQLITE_ROW )
  {
    // yes, we already have this symbol
    int id = sqlite3_column_int (m_check_sym, 0);
    m_cache[l] = id;
    return id;
  }
#endif /* !CACHE_ALLSYMS */
  // insert new one
  sqlite3_reset(m_insert_sym);
  int res = ++max_id;
  sqlite3_bind_int(m_insert_sym, 1, res);
  sqlite3_bind_text(m_insert_sym, 2, l.c_str(), l.size(), SQLITE_STATIC);
  sqlite3_bind_text(m_insert_sym, 3, "", 0, SQLITE_STATIC);
  sqlite3_step(m_insert_sym);
  m_cache[l] = res;
  return res;
}

int pers_sqlite::check_symbol(const char *sname)
{
  auto cached = m_cache.find(sname);
  if ( cached != m_cache.end() )
    return cached->second;
#ifndef CACHE_ALLSYMS
  // on big codebase this query very quickly becomes tooooo sloooooow
  sqlite3_reset(m_check_sym);
  sqlite3_bind_text(m_check_sym, 1, sname, strlen(sname), SQLITE_STATIC);
  int rc;
  if ((rc = sqlite3_step(m_check_sym)) == SQLITE_ROW )
  {
    // yes, we already have this symbol
    int id = sqlite3_column_int (m_check_sym, 0);
    m_cache[sname] = id;
    return id;
  }
#endif /* !CACHE_ALLSYMS */
  // insert new one
  sqlite3_reset(m_insert_sym);
  int res = ++max_id;
  sqlite3_bind_int(m_insert_sym, 1, res);
  sqlite3_bind_text(m_insert_sym, 2, sname, strlen(sname), SQLITE_STATIC);
  sqlite3_bind_text(m_insert_sym, 3, "", 0, SQLITE_STATIC);
  sqlite3_step(m_insert_sym);
  m_cache[sname] = res;
  return res;
}

void pers_sqlite::report_error(const char *str)
{
  add_func();
  sqlite3_reset(m_insert_err);
  sqlite3_bind_int(m_insert_err, 1, m_func_id);
  sqlite3_bind_int(m_insert_err, 2, m_bb);
  sqlite3_bind_text(m_insert_err, 3, str, strlen(str), SQLITE_STATIC);
  sqlite3_step(m_insert_err);
}

void pers_sqlite::add_literal(const char *lc, int lc_size)
{
  std::string lit;
  if ( !lc[lc_size - 1] )
    lc_size--;
  lit.assign(lc, lc_size);
  add_func();
  insert_xref(check_literal(lit), 'l');
}

void pers_sqlite::add_xref(xref_kind kind, const char *sym)
{
  add_func();
  char c;
  switch(kind)
  {
    case xcall:
     c = 'c';
     break;
    case vcall:
     c = 'v';
     break;
    case xref:
     c = 'r';
     break;
    case field:
     c = 'f';
     break;
    case fconst:
     c = 'F';
     break;
    default: return; // wtf?
  }
  insert_xref(check_symbol(sym), c);
}

void pers_sqlite::insert_xref(int id, char what)
{
  sqlite3_reset(m_insert_xref);
  sqlite3_bind_int(m_insert_xref, 1, m_func_id);
  sqlite3_bind_int(m_insert_xref, 2, m_bb);
  sqlite3_bind_text(m_insert_xref, 3, &what, 1, SQLITE_STATIC);
  sqlite3_bind_int(m_insert_xref, 4, id);
  sqlite3_step(m_insert_xref);
}

int pers_sqlite::connect(const char *dbname, const char *user, const char *pass)
{
  int res = sqlite3_open_v2(dbname, &m_db, SQLITE_OPEN_READWRITE, NULL);
  if ( res == SQLITE_CANTOPEN )
    res = create_new_db(dbname);
  if ( res )
  {
    fprintf(stderr, "sqlite3_open res %d\n", res);
    return res;
  }
  res = prepare();
  if ( res )
    fprintf(stderr, "prepare res %d\n", res);
 #ifdef CACHE_ALLSYMS
   sqlite3_stmt *stmt;  
   res = sqlite3_prepare_v2(m_db, pr_all_sym, -1, &stmt, NULL);
  if (res != SQLITE_OK) {
    fprintf(stderr, "error %d when prepare all_sym\n", res);
    return res;
  }
  while ( sqlite3_step(stmt) == SQLITE_ROW )
  {
    int id = sqlite3_column_int (stmt, 0);
    const char *sname = (const char *)sqlite3_column_text(stmt, 1);
    if ( sname )
      m_cache[sname] = id;
  }
  sqlite3_finalize(stmt);
#endif /* CACHE_ALLSYMS */   
  return res;  
}

FPersistence *get_pers()
{
  return new pers_sqlite();  
}