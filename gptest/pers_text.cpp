#include <stdio.h>
#include <time.h>
#include <string>
#include "fpers.h"

class pers_text: public FPersistence
{
  public:
   pers_text():
    m_fp(NULL),
    m_func_dunped(false),
    m_bb_dumped(false)
   {}
   virtual ~pers_text()
   {
    if ( m_fp )
    {
      fclose(m_fp);
      m_fp = NULL;
    }
   }
   // implementation of FPersistence
   virtual void cu_start(const char *cu_name)
   {
     if ( m_fp )
     {
       char time_str[150];
       struct tm *tmp;
       time_t t = time(NULL);
       auto tm = localtime(&t);
       if ( !tm )
         fprintf(m_fp, "# %s\n", cu_name);
       else {
         strftime(time_str, sizeof(time_str), "%F %H:%M:%S", tm);
         fprintf(m_fp, "# %s %s\n", time_str, cu_name);
       }
     }
   }
   virtual int connect(const char *, const char *, const char *);
   virtual int func_start(const char *fn, int block_count) override
   {
     m_fn = fn;
     m_bb_count = block_count;
     m_func_proto.clear();
     m_func_dunped = false;
     return 1;
   }
   virtual void func_proto(const char *p) override
   {
     m_func_proto = p;
   }
   virtual void bb_start(int idx)
   {
     m_bb = idx;
     m_bb_dumped = false;
   }
   // main method
   virtual void add_xref(xref_kind, const char *, int arg_no = 0) override;
   virtual void add_literal(const char *, int) override;
   virtual void add_ic(int);
   virtual void add_comment(const char *);
   // store errors
   virtual void report_error(const char *) override;
  protected:
   void check();

   FILE *m_fp;
   std::string m_fn;
   std::string m_func_proto;
   int m_bb_count;
   int m_bb;
   bool m_bb_dumped;
   bool m_func_dunped;
};

int pers_text::connect(const char *fn, const char *u, const char *p)
{
  if ( m_fp )
  {
    fclose(m_fp);
    m_fp = NULL;
  }
  m_fp = fopen(fn, "a+t");
  if ( m_fp == NULL )
  {
    fprintf(stderr, "cannot open file %s, error %d\n", fn, errno);
    return errno;
  }
  return 0;
}

void pers_text::add_xref(xref_kind kind, const char *what, int arg_no)
{
  if ( !m_fp )
    return;
  check();
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
  if ( kind == field && arg_no )
    fprintf(m_fp, "  %c arg%d %s\n", c, arg_no, what);
  else
    fprintf(m_fp, "  %c %s\n", c, what);
}

void pers_text::add_literal(const char *what, int len)
{
  // ripped from print_node from print-tree.cc for STRING_CST
  if ( !m_fp )
    return;
  check();
  fprintf(m_fp, "  l ");
  for ( int i = 0; i < len; i++ )
  {
    if ( !what[i] && (i == len - 1) )
      break;
    if ( what[i] >= ' ' )
      fputc(what[i], m_fp);
    else
      fprintf(m_fp, "\\x%02x", (unsigned char)what[i]);
  }
  fputc('\n', m_fp);
}

void pers_text::add_comment(const char *cmt)
{
  if ( !m_fp )
    return;
  check();
  fprintf(m_fp, "# %s\n", cmt);
}

void pers_text::add_ic(int ic)
{
  if ( !m_fp )
    return;
  check();
  fprintf(m_fp, "  i %d\n", ic);
}

void pers_text::report_error(const char *err)
{
  if ( !err )
    return;
  if ( !m_fp )
  {
    fprintf(stderr, "Error: %s\n", err);
    return;
  }
  fprintf(m_fp, "#! ");
  if ( !m_func_dunped )
    fprintf(m_fp, "Func %s ", m_fn.c_str());
  if ( !m_bb_dumped )
    fprintf(m_fp, "BB %d ", m_bb);
  fprintf(m_fp, "Err: %s\n", err);
}

void pers_text::check()
{
  if ( !m_func_dunped )
  {
    fprintf(m_fp, "func %s\n", m_fn.c_str());
    if ( !m_func_proto.empty() )
      fprintf(m_fp, "proto %s %d blocks\n", m_func_proto.c_str(), m_bb_count);
    m_func_dunped = true;
  }
  if ( !m_bb_dumped )
  {
    fprintf(m_fp, " bb %d\n", m_bb);
    m_bb_dumped = true;
  }
}

FPersistence *get_pers()
{
  return new pers_text();
}