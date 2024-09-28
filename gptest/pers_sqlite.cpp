#include <stdio.h>
#include "fpers.h"
#include "sqlite_cmn.inc"

class pers_sqlite: public FPersistence, protected sqlite_cmn
{
  public:
   pers_sqlite(): sqlite_cmn(),
    m_has_func_id(false),
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
   virtual int func_start(const char *fn, int bcount) override;
   virtual void func_stop()
   {
     m_has_func_id = false;
   }
   virtual void add_xref(xref_kind, const char *, int arg_no = 0) override;
   virtual void add_literal(const char *, int size) override;
   virtual void report_error(const char *) override;
  protected:
   int add_func();
   int check_symbol(const char *);
   int check_literal(std::string &);
   void insert_xref(int id, char what, int arg_no);

   std::string m_fn;
   std::string m_func;
   bool m_has_func_id;
   int m_func_id;
   int m_bb;
   int m_bcount;
};

void pers_sqlite::disconnect()
{
  close();
}

pers_sqlite::~pers_sqlite()
{
  this->disconnect();
}


int pers_sqlite::func_start(const char *fn, int bcount)
{
  m_func = fn;
  m_bcount = bcount;
  m_has_func_id = false;
#ifdef CACHE_ALLSYMS
  bool is_func = check_func(fn, m_func_id);
  if ( m_func_id ) {
    m_has_func_id = true;
    if ( !is_func )
    {
      m_funcs.insert(m_func_id);
#else
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
#endif
      // this function was seen before but not processed yet - push filename for it
      // from https://www.sqlite.org/c3ref/bind_blob.html: The leftmost SQL parameter has an index of 1
      sqlite3_reset(m_update_fname);
      sqlite3_bind_int(m_update_fname, 3, m_func_id); // id in where clause - last
      sqlite3_bind_text(m_update_fname, 1, m_fn.c_str(), m_fn.size(), SQLITE_STATIC);
      sqlite3_bind_int(m_update_fname, 2, m_bcount);
      sqlite3_step(m_update_fname);
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
  sqlite3_bind_int(m_insert_sym, 4, m_bcount);
  sqlite3_step(m_insert_sym);
  cache_func<std::string &>(m_func, m_func_id);
  return 1;
}

int pers_sqlite::check_literal(std::string &l)
{
  int id = 0;
  if ( check<std::string &>(l, id) ) return id;
#ifndef CACHE_ALLSYMS
  // on big codebase this query very quickly becomes tooooo sloooooow
  sqlite3_reset(m_check_sym);
  sqlite3_bind_text(m_check_sym, 1, l.c_str(), l.size(), SQLITE_STATIC);
  int rc;
  if ((rc = sqlite3_step(m_check_sym)) == SQLITE_ROW )
  {
    // yes, we already have this symbol
    int id = sqlite3_column_int (m_check_sym, 0);
    add(l.c_str(), id);
    return id;
  }
#endif /* !CACHE_ALLSYMS */
  // insert new one
  sqlite3_reset(m_insert_sym);
  int res = ++max_id;
  sqlite3_bind_int(m_insert_sym, 1, res);
  sqlite3_bind_text(m_insert_sym, 2, l.c_str(), l.size(), SQLITE_STATIC);
  sqlite3_bind_text(m_insert_sym, 3, "", 0, SQLITE_STATIC);
  sqlite3_bind_int(m_insert_sym, 4, 0);
  sqlite3_step(m_insert_sym);
  add(l.c_str(), res);
  return res;
}

int pers_sqlite::check_symbol(const char *sname)
{
  int id = 0;
  if ( check(sname, id) ) return id;
#ifndef CACHE_ALLSYMS
  // on big codebase this query very quickly becomes tooooo sloooooow
  sqlite3_reset(m_check_sym);
  sqlite3_bind_text(m_check_sym, 1, sname, strlen(sname), SQLITE_STATIC);
  int rc;
  if ((rc = sqlite3_step(m_check_sym)) == SQLITE_ROW )
  {
    // yes, we already have this symbol
    int id = sqlite3_column_int (m_check_sym, 0);
    add(sname, id);
    return id;
  }
#endif /* !CACHE_ALLSYMS */
  // insert new one
  sqlite3_reset(m_insert_sym);
  int res = ++max_id;
  sqlite3_bind_int(m_insert_sym, 1, res);
  sqlite3_bind_text(m_insert_sym, 2, sname, strlen(sname), SQLITE_STATIC);
  sqlite3_bind_text(m_insert_sym, 3, "", 0, SQLITE_STATIC);
  sqlite3_bind_int(m_insert_sym, 4, 0);
  sqlite3_step(m_insert_sym);
  add(sname, res);
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
  insert_xref(check_literal(lit), 'l', 0);
}

void pers_sqlite::add_xref(xref_kind kind, const char *sym, int arg_no)
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
  insert_xref(check_symbol(sym), c, arg_no);
}

void pers_sqlite::insert_xref(int id, char what, int arg_no)
{
  sqlite3_reset(m_insert_xref);
  sqlite3_bind_int(m_insert_xref, 1, m_func_id);
  sqlite3_bind_int(m_insert_xref, 2, m_bb);
  sqlite3_bind_text(m_insert_xref, 3, &what, 1, SQLITE_STATIC);
  sqlite3_bind_int(m_insert_xref, 4, arg_no);
  sqlite3_bind_int(m_insert_xref, 5, id);
  sqlite3_step(m_insert_xref);
}

int pers_sqlite::connect(const char *dbname, const char *user, const char *pass)
{
  return open(dbname);
}

FPersistence *get_pers()
{
  return new pers_sqlite();
}