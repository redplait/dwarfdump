#pragma once

#include <gcc-plugin.h>
#include <context.h>
#include <basic-block.h>
#include <rtl.h>
#include <tree-pass.h>
#include <tree.h>

#include <stdio.h>
#include <list>

class my_PLUGIN : public rtl_opt_pass
{
 public:
  my_PLUGIN(gcc::context *ctxt, struct plugin_argument *arguments, int argcounter);
  // inherited from opt_pass
  virtual ~my_PLUGIN();
//  bool gate(function *fun);
  unsigned int execute(function *fun);
 private:
  void margin(int);
  void dump_mem_expr(const_tree expr);
  void dump_rmem_expr(const_tree expr);
  void dump_ssa_name(const_tree expr);
  void dump_comp_ref(const_tree expr);
  void dump_method(const_tree expr);
  void dump_rtx(const_rtx, int level = 0);
  void dump_rtx_hl(const_rtx);
  int dump_e_operand(const_rtx in_rtx, int idx, int level);
  int dump_u_operand(const_rtx in_rtx, int idx, int level);
  int dump_i_operand(const_rtx in_rtx, int idx, int level);
  int dump_r_operand(const_rtx in_rtx, int idx, int level);
  int dump_EV_code(const_rtx in_rtx, int idx, int level);
  void dump_rtx_operand(const_rtx in_rtx, char f, int idx, int level);
  // expr stack
  void expr_push(const_rtx, int idx);
  void expr_pop()
  {
    m_rtexpr.pop_back();
  }
  void dump_exprs();
  // plugin options
  const char* findArgumentValue(const char* key);
  bool existsArgument(const char *key) const;
  inline bool need_dump() const
  {
    return m_outfp != NULL;
  }

  int argc;
  struct plugin_argument *args;
  FILE *m_outfp;
  // args
  bool m_dump_rtl;
  bool m_verbose;
  std::string m_db_str;
  // expressions stack - rtx class and current index of expression
  std::list<std::pair<enum rtx_class, int> > m_rtexpr;
  // current basic_block number
  int bb_index;
};
