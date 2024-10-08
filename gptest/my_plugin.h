#pragma once

#include <gcc-plugin.h>
#include <context.h>
#include <basic-block.h>
#include <rtl.h>
#include <tree-pass.h>
#include <tree.h>

#include <stdio.h>
#include <vector>
#include <map>
#include <set>

#include "fpers.h"

// comment this define to enable profiling
// #define GPPROF
// don't produce output to stderr - for gcc testsuite
#define GP_SILENCE

#ifdef GPPROF
#include "prof.h"
#endif

struct aux_type_clutch
{
  aux_type_clutch()
   : off(0), has_off(false), is_lvar(false),
     level(0),
     completed(false),
     last(NULL_TREE),
     left_name(nullptr)
  { }
  aux_type_clutch(const aux_type_clutch &r)
   : off(r.off), has_off(r.has_off), is_lvar(r.is_lvar),
     level(r.level),
     completed(false),
     last(NULL_TREE),
     left_name(r.left_name)
  { }
  aux_type_clutch(const_rtx);
  xref_kind field_xref() const {
    return is_lvar ? gfield: field;
  }
  // data
  HOST_WIDE_INT off; // in param
  std::string txt;
  int level;
  bool completed;
  bool has_off;
  bool is_lvar;
  const_tree last;
  const char *left_name;
};

class rtl_plugin_with_args: public rtl_opt_pass
{
  public:
    rtl_plugin_with_args(gcc::context *ctxt, const char *fname, const struct pass_data &pd, struct plugin_argument *arguments, int argcounter);
  protected:
    FILE *m_outfp;
#ifdef GPPROF
    int profiled;
#endif
    int argc;
    struct plugin_argument *args;
    bool m_verbose;
    inline bool need_dump() const
    {
      return m_outfp != NULL;
    }
    // plugin options
    const char* findArgumentValue(const char* key);
    bool existsArgument(const char *key) const;
};

class st_labels: public rtl_plugin_with_args
{
 public:
   st_labels(gcc::context *ctxt, struct plugin_argument *arguments, int argcounter);
   unsigned int execute(function *fun);
  protected:
   void rec_check_labels(const_tree);
   std::map<int, std::pair<rtx, int> > m_st;
   std::string m_funcname;
};

class my_PLUGIN : public rtl_plugin_with_args
{
 public:
  my_PLUGIN(gcc::context *ctxt, const char *full_name, struct plugin_argument *arguments, int argcounter);
  // inherited from opt_pass
  virtual ~my_PLUGIN();
//  bool gate(function *fun);
  unsigned int execute(function *fun);
  // public interface for db connection
  int connect();
  void start_file(const char *);
  void stop_file();
 private:
  int count_blocks(function *) const;
  int fill_blocks(function *);
  const char *find_uid(unsigned int);
  void read_ic_config(const char *);
  int ic_filter() const;
  void margin(int) const;
  int is_vptr(const_tree);
  int add_fref_from_equal(int);
  void store_aux(aux_type_clutch &);
  void dump_type_tree(const_tree expr);
  void dump_field_decl(const_tree expr);
  void try_nameless(const_tree expr, aux_type_clutch &);
  void dump_array_ref(const_tree expr, aux_type_clutch &);
  void claim_unknown(tree_code, const char *);
  // for hard-debugging of where fields are added
  void report_fref(const char *where) const
  {
    if ( need_dump() )
     fprintf(m_outfp, " addfield(%s)", where);
  }
  void dump_mem_ref(const_tree expr, aux_type_clutch &);
  void dump_mem_expr(const_tree expr, const_rtx);
  void dump_rmem_expr(const_tree expr, const_rtx);
  void dump_ssa_name(const_tree expr, aux_type_clutch &);
  void dump_comp_ref(const_tree expr, aux_type_clutch &);
  void dump_addr_expr(const_tree expr, aux_type_clutch &);
  void dump_method(const_tree expr);
  void dump_ftype(const_tree expr);
  void dump_tree_MF(const_tree expr);
  void dump_rtx(const_rtx, int level = 0);
  void dump_rtx_hl(const_rtx);
  void dump_func_tree(const_tree, int level = 0);
  int check_var_initial(const_tree);
  const_tree check_arg(const_tree);
  const_tree try_class_rec(const_tree binfo, const_tree igo, const_tree exp, tree *base, tree *found);
  const_tree dump_class_rec(const_tree, const_tree igo, int level);
  const char *is_cliteral(const_rtx, int &len);
  void dump_section(const_tree expr);
  int dump_0_operand(const_rtx in_rtx, int idx, int level);
  int dump_e_operand(const_rtx in_rtx, int idx, int level);
  int dump_u_operand(const_rtx in_rtx, int idx, int level);
  int dump_i_operand(const_rtx in_rtx, int idx, int level);
  int dump_r_operand(const_rtx in_rtx, int idx, int level);
  int dump_EV_code(const_rtx in_rtx, int idx, int level);
  void dump_rtx_operand(const_rtx in_rtx, char f, int idx, int level);
  // helper for errors
  void pass_error(const char *fmt, ...);
  // helpers to detect kind of xref
  int is_symref() const;
  int is_call() const;
  int is_write() const;
  int is_symref_call() const;
  int inside_if() const;
  int is_eh_num() const;
  int is_sb() const;
  int is_set_reg() const;
  int is_set_mem() const;
  int is_var_loc() const;
  int is_plus() const;
  // expr stack
  void expr_push(const_rtx, int idx);
  inline void expr_pop()
  {
    m_rtexpr.pop_back();
  }
  void dump_exprs() const;
  void make_expr_cmt(const_rtx in_rtx, std::string &);
  void dump_known_uids() const;
  // args
  bool m_dump_rtl;
  bool m_asmproto;
  bool m_dump_ic; // add integer constants
  const char *m_db_str;
  // db
  FPersistence *m_db;
  // expressions stack - rtx class and current index of expression
  struct rtx_item {
    rtx_item(enum rtx_class c, int i):
      m_ce(c), m_idx(i), m_sb(false)
    { }
    rtx_item(enum rtx_class c, int i, bool s):
      m_ce(c), m_idx(i), m_sb(s)
    { }
    rtx_item(rtx_item &&) = default;
    rtx_item& operator=(rtx_item &&) = default;
    // data
    enum rtx_class m_ce;
    int m_idx;
    bool m_sb;
  };
  // stack of expressions
  std::vector<rtx_item> m_rtexpr;
  // func args - index (starting with 1, sorry - 0 in add_xref means no argument) and type of union/record
  std::map<const_tree, std::pair<int, const_tree> > m_args;
  int m_arg_no = 0;
  bool m_requal; // if currently processing rtx is expr_list with REG_EQUAL
  bool m_requiv; // if currently processing rtx is expr_list with REG_EQUIV
  const char *m_scall; // if call symbol_ref already was added for rtx 's'
  const_tree m_rbase;  // when m_requal this is base type of referred argument
  // for BB with single in-edge, key is BB index, value is index of parent BB
  std::map<int, int> m_blocks;
  // uid types, key is UID and block index
  std::map<std::pair<unsigned int, int>, std::string> m_known_uids;
  // current basic_block number
  int bb_index;
  int in_pe; // current insn in prologue/epilogue
  std::set<enum rtx_class> ic_allowed;
  std::set<enum rtx_class> ic_denied;
};
