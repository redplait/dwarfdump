#include <sqlite3.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <string.h>

#define CACHE_ALLSYMS
// #define USE_INDEXES

// base classes for cache
struct map_cache {
  std::unordered_set<int> m_funcs;
  std::unordered_map<std::string, int> m_cache;
  // unified interface
  template <typename T>
  bool check(T func, int &id) const {
   auto cached = m_cache.find(func);
    if ( cached != m_cache.end() ) {
     id = cached->second;
     return true;
    }
    return false;
  }
  void add(const char *func, int id) {
    m_cache[func] = id;
  }
  // for functions
  template <typename T>
  void cache_func(T func, int id)
  {
    m_cache[func] = id;
    m_funcs.insert(id);
  }
  bool check_func(const char *f, int &id) {
   id = 0;
   auto cached = m_cache.find(f);
    if ( cached != m_cache.end() ) {
     id = cached->second;
     auto fi = m_funcs.find(id);
     return fi != m_funcs.end();
    }
    return false;
  }
};

class sqlite_cmn: protected map_cache {
  public:
   sqlite_cmn(): map_cache(),
    m_db(NULL),
    m_check_sym(NULL),
    m_update_fname(NULL),
    m_insert_sym(NULL),
    m_insert_xref(NULL),
    m_insert_err(NULL),
    m_del_xrefs(NULL),
    m_del_errs(NULL),
    max_id(0)
   {}
 protected:
   int create_new_db(const char *);
   int prepare();
   int get_max_id();
   void close();
   int open(const char *dbname);

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
   int max_id;
};

void sqlite_cmn::close()
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

// sample from https://github.com/wikibook/sqlite3/blob/master/2_4_5_sqlite3_CAPI_examples/CAPI_examples/sqlite3_capi_examples/sqlite3_capi_examples/capi_example_1.cpp
const char *cr_symtab = "CREATE TABLE IF NOT EXISTS symtab ("
 "id INTEGER PRIMARY KEY,"
 "name TEXT,"
 "fname TEXT,"
 "bcount INTEGER DEFAULT 0"
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
 "arg INTEGER,"
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
int sqlite_cmn::create_new_db(const char *dbname)
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
const char *pr_all_sym = "SELECT id, name, fname FROM symtab;";
const char *pr_check_sym = "SELECT id, fname FROM symtab WHERE name = ?;";
const char *pr_update_fname = "UPDATE symtab SET fname = ?,bcount =? WHERE id = ?;";
const char *pr_insert_sym = "INSERT INTO symtab (id, name, fname, bcount) VALUES (?, ?, ?, ?);";
const char *pr_insert_xref = "INSERT INTO xrefs (id, bb, kind, arg, what) VALUES (?, ?, ?, ?, ?);";
const char *pr_insert_err  = "INSERT INTO errlog (id, bb, msg) VALUES (?, ?, ?);";
const char *pr_del_xrefs   = "DELETE FROM xrefs WHERE id = ?;";
const char *pr_del_errs    = "DELETE FROM errlog WHERE id = ?;";
#define STMT(c) c, strlen(c)

int sqlite_cmn::prepare()
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

int sqlite_cmn::get_max_id()
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

int sqlite_cmn::open(const char *dbname)
{
  int was_new = 0;
  sqlite3_config(SQLITE_CONFIG_SINGLETHREAD);
  int res = sqlite3_open_v2(dbname, &m_db, SQLITE_OPEN_READWRITE, NULL);
  if ( res == SQLITE_CANTOPEN ) {
    res = create_new_db(dbname);
    was_new = 1;
  }
  if ( res )
  {
    fprintf(stderr, "sqlite3_open res %d\n", res);
    return res;
  }
  res = prepare();
  if ( res )
    fprintf(stderr, "prepare res %d\n", res);
  // from https://stackoverflow.com/questions/4210463/how-do-i-get-sqlite-to-run-faster-with-pragma-synchronous-off-by-default
  char *perr_str = NULL;
  int perr = sqlite3_exec(m_db, "PRAGMA synchronous=OFF", NULL, NULL, &perr_str);
  if ( perr && perr_str )
    fprintf(stderr, "pragma fauled, code %d: %s\n", perr, perr_str);
  if ( was_new ) return res;
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
    const char *fname = (const char *)sqlite3_column_text(stmt, 2);
    if ( sname ) {
      if ( *fname )
        cache_func(sname, id);
      else
        add(sname, id);
    }
  }
  sqlite3_finalize(stmt);
#endif /* CACHE_ALLSYMS */
  return res;
}