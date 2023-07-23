#pragma once

#include <gcc-plugin.h>
#include <context.h>
#include <basic-block.h>
#include <rtl.h>
#include <tree-pass.h>
#include <tree.h>

#include <stdio.h>


class my_PLUGIN : public rtl_opt_pass
{
 public:
  my_PLUGIN(gcc::context *ctxt, struct plugin_argument *arguments, int argcounter);
//  bool gate(function *fun);
  unsigned int execute(function *fun);
 private:
  void margin(int);
  void dump_rtx(const_rtx, int level = 0);
  void dump_e_operand(const_rtx in_rtx, int idx, int level);
  void dump_u_operand(const_rtx in_rtx, int idx, int level);
  void dump_EV_code(const_rtx in_rtx, int idx, int level);
  void dump_rtx_operand(const_rtx in_rtx, char f, int idx, int level);
  const char* findArgumentValue(const char* key);

  int argc;
  struct plugin_argument *args;
  FILE *m_outfp;
};
