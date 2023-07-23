// This is the first gcc header to be included
#include "gcc-plugin.h"
#include "plugin-version.h"
#include "print-rtl.h"
#include "langhooks.h"
#include "my_plugin.h"

#include <iostream>

const struct pass_data my_PLUGIN_pass_data =
{
    .type = RTL_PASS,
    .name = "myPlugin",
    .optinfo_flags = OPTGROUP_NONE,
    .tv_id = TV_NONE,
    .properties_required = PROP_rtl, // | PROP_cfglayout),
    .properties_provided = 0,
    .properties_destroyed = 0,
    .todo_flags_start = 0,
    .todo_flags_finish = 0,
};

my_PLUGIN::my_PLUGIN(gcc::context *ctxt, struct plugin_argument *arguments, int argcounter)
 : rtl_opt_pass(my_PLUGIN_pass_data, ctxt)
{
    argc = argcounter;   // number of arguments
    args = arguments;    // array containing arrguments (key,value)
    m_outfp = stdout;
}

extern void dump_function_header(FILE *, tree, dump_flags_t);
extern void make_decl_rtl (tree);

// ripped from print-rtl-function.cc
static void
print_edge (FILE *outfile, edge e, bool from)
{
  fprintf (outfile, "      (%s ", from ? "edge-from" : "edge-to");
  basic_block bb = from ? e->src : e->dest;
  gcc_assert (bb);
  switch (bb->index)
    {
    case ENTRY_BLOCK:
      fprintf (outfile, "entry");
      break;
    case EXIT_BLOCK:
      fprintf (outfile, "exit");
      break;
    default:
      fprintf (outfile, "%i", bb->index);
      break;
    }

  /* Express edge flags as a string with " | " separator.
     e.g. (flags "FALLTHRU | DFS_BACK").  */
  if (e->flags)
    {
      fprintf (outfile, " (flags \"");
      bool seen_flag = false;
#define DEF_EDGE_FLAG(NAME,IDX)                 \
  do {                                          \
    if (e->flags & EDGE_##NAME)                 \
      {                                         \
       if (seen_flag)                          \
          fprintf (outfile, " | ");             \
        fprintf (outfile, "%s", (#NAME));       \
        seen_flag = true;                       \
      }                                         \
  } while (0);
#include "cfg-flags.def"
#undef DEF_EDGE_FLAG

      fprintf (outfile, "\")");
    }

  fprintf (outfile, ")\n");
}

/* If BB is non-NULL, print the start of a "(block)" directive for it
   to OUTFILE, otherwise do nothing.  */

static void
begin_any_block (FILE *outfile, basic_block bb)
{
  if (!bb)
    return;

  edge e;
  edge_iterator ei;

  fprintf (outfile, "    (block %i\n", bb->index);
  FOR_EACH_EDGE (e, ei, bb->preds)
    print_edge (outfile, e, true);
}

static void
end_any_block (FILE *outfile, basic_block bb)
{
  if (!bb)
    return;

  edge e;
  edge_iterator ei;

  FOR_EACH_EDGE (e, ei, bb->succs)
    print_edge (outfile, e, false);
  fprintf (outfile, "    ) ;; block %i\n", bb->index);
}

/* Determine if INSN is of a kind that can have a basic block.  */

static bool
can_have_basic_block_p (const rtx_insn *insn)
{
  rtx_code code = GET_CODE (insn);
  if (code == BARRIER)
    return false;
  gcc_assert (GET_RTX_FORMAT (code)[2] == 'B');
  return true;
}

static void
print_any_param_name (FILE *outfile, tree arg)
{
  if (DECL_NAME (arg))
    fprintf (outfile, " \"%s\"", IDENTIFIER_POINTER (DECL_NAME (arg)));
}

/* Print a "(param)" directive for ARG to OUTFILE.  */

static void
print_param (FILE *outfile, rtx_writer &w, tree arg)
{
  fprintf (outfile, "  (param");
  print_any_param_name (outfile, arg);
  fprintf (outfile, "\n");

  /* Print the value of DECL_RTL (without lazy-evaluation).  */
  fprintf (outfile, "    (DECL_RTL ");
  w.print_rtx (DECL_RTL_IF_SET (arg));
  w.finish_directive ();

  /* Print DECL_INCOMING_RTL.  */
  fprintf (outfile, "    (DECL_RTL_INCOMING ");
  w.print_rtx (DECL_INCOMING_RTL (arg));
  fprintf (outfile, ")");

  w.finish_directive ();
}

void my_PLUGIN::margin(int level)
{
  fputc(';', m_outfp);
  for ( int i = 0; i < level; i++ )
    fputc(' ', m_outfp);
}

void my_PLUGIN::dump_u_operand(const_rtx in_rtx, int idx, int level)
{
  if (XEXP (in_rtx, idx) != NULL)
  {
    rtx sub = XEXP (in_rtx, idx);
    enum rtx_code subc = GET_CODE (sub);
    if (GET_CODE (in_rtx) == LABEL_REF)
    {
      if (subc != CODE_LABEL)
      {
         dump_e_operand(in_rtx, idx, level + 1);
         return;
      }
    }
  }  
}

void my_PLUGIN::dump_e_operand(const_rtx in_rtx, int idx, int level)
{
  auto e = XEXP (in_rtx, idx);
  if ( e )
  dump_rtx(e, level + 1);
}

void my_PLUGIN::dump_EV_code(const_rtx in_rtx, int idx, int level)
{
  if (XVEC (in_rtx, idx) != NULL)
  {
    int barrier = XVECLEN (in_rtx, idx);
    if (GET_CODE (in_rtx) == CONST_VECTOR
          && !GET_MODE_NUNITS (GET_MODE (in_rtx)).is_constant ())
      barrier = CONST_VECTOR_NPATTERNS (in_rtx);
    int len = XVECLEN (in_rtx, idx);
    fprintf(m_outfp, " len %d\n", len);
    for (int j = 0; j < XVECLEN (in_rtx, idx); j++)
    {
      dump_rtx (XVECEXP (in_rtx, idx, j), level + 1);
    }
  }
}

void my_PLUGIN::dump_rtx_operand(const_rtx in_rtx, char f, int idx, int level)
{
  const char *str;
  margin(level + 2);
  fprintf(m_outfp, "[%d]", idx);
  switch(f)
  {
    case 'T':
      str = XTMPL (in_rtx, idx);
      goto string;

    case 'S':
    case 's':
      str = XSTR (in_rtx, idx);
    string:

      if (str == 0)
        fputs (" (nil)", m_outfp);
      else
        fprintf (m_outfp, " (\"%s\")", str);
      break;

   case 'u':
      dump_u_operand(in_rtx, idx, level);
      break;
    
   case 'e':
      dump_e_operand(in_rtx, idx, level);
      break;

   case 'E':
   case 'V':
      dump_EV_code(in_rtx, idx, level + 1);
      break;

   case 'n':
      fprintf (m_outfp, " %s", GET_NOTE_INSN_NAME (XINT (in_rtx, idx)));
      break;

  }
  fputs("\n", m_outfp);
}

void my_PLUGIN::dump_rtx(const_rtx in_rtx, int level)
{
  int idx = 0;
  if ( !in_rtx )
    return;
  rtx_code code = GET_CODE(in_rtx);
  if ( code > NUM_RTX_CODE )
    return;
  int limit = GET_RTX_LENGTH(code);
  if ( CONST_DOUBLE_AS_FLOAT_P(in_rtx) )
    idx = 5;
  margin(level + 1);
  if ( INSN_CHAIN_CODE_P(code) )  
    fprintf(m_outfp, "%d %s %d lim %d", INSN_UID (in_rtx), GET_RTX_NAME(code), idx, limit);
  else 
    fprintf(m_outfp, "%s %d lim %d", GET_RTX_NAME(code), idx, limit);
  const char *format_ptr = GET_RTX_FORMAT(code);
  fprintf(m_outfp, " %s\n", format_ptr);
  // dump operands
  for (; idx < limit; idx++)
    dump_rtx_operand (in_rtx, format_ptr[idx], idx, level + 1);  
  if ( code == MEM && MEM_EXPR(in_rtx) )
    fprintf(m_outfp, "MEM");
}

unsigned int my_PLUGIN::execute(function *fun)
{
  rtx_reuse_manager r;
  rtx_writer w (m_outfp, 0, false, false, &r);  
  // 1) Find the name of the function
  char* funName = (char*)IDENTIFIER_POINTER (DECL_NAME (current_function_decl) );
  tree fdecl = fun->decl;
  const char *dname = lang_hooks.decl_printable_name (fdecl, 1);
  std::cerr << "execute on " << funName << " (" << dname << ") file " << main_input_filename << "\n";
  dump_function_header(m_outfp, fun->decl, (dump_flags_t)0);
  // dump params
  /* Params.  */
  for (tree arg = DECL_ARGUMENTS (fdecl); arg; arg = DECL_CHAIN (arg))
    print_param (m_outfp, w, arg);

  basic_block bb;
  FOR_ALL_BB_FN(bb, fun)
  { // Loop over all Basic Blocks in the function, cfun = current function
      fprintf(m_outfp,"BB: %d\n", bb->index);
      begin_any_block(m_outfp, bb);
      rtx_insn* insn;
      FOR_BB_INSNS(bb, insn)
      {
        if ( NONDEBUG_INSN_P(insn) )
          dump_rtx(insn);
        w.print_rtl_single_with_indent(insn, 0);
      }
      end_any_block (m_outfp, bb);
      fprintf(m_outfp,"\n----------------------------------------------------------------\n\n");
   }
  return 0;
}

// We must assert that this plugin is GPL compatible
int plugin_is_GPL_compatible;

int plugin_init (struct plugin_name_args *plugin_info, struct plugin_gcc_version *version)
{
    // We check the current gcc loading this plugin against the gcc we used to
    // created this plugin
    if (!plugin_default_version_check (version, &gcc_version))
    {
      std::cerr << "This GCC plugin is for version " << GCCPLUGIN_VERSION_MAJOR << "." << GCCPLUGIN_VERSION_MINOR << "\n";
	  return 1;
    }

    // Let's print all the information given to this plugin!

    std::cerr << "Plugin info\n===========\n";
    std::cerr << "Base name: " << plugin_info->base_name << "\n";
    std::cerr << "Full name: " << plugin_info->full_name << "\n";
    std::cerr << "Number of arguments of this plugin:" << plugin_info->argc << "\n";

    for (int i = 0; i < plugin_info->argc; i++)
    {
        std::cerr << "Argument " << i << ": Key: " << plugin_info->argv[i].key << ". Value: " << plugin_info->argv[i].value<< "\n";
    }

    std::cerr << "\nVersion info\n============\n";
    std::cerr << "Base version: " << version->basever << "\n";
    std::cerr << "Date stamp: " << version->datestamp << "\n";
    std::cerr << "Dev phase: " << version->devphase << "\n";
    std::cerr << "Revision: " << version->devphase << "\n";
    std::cerr << "Configuration arguments: " << version->configuration_arguments << "\n";
    std::cerr << "\n";

    struct register_pass_info pass;
    pass.pass = new my_PLUGIN(g, plugin_info->argv, plugin_info->argc);
    pass.reference_pass_name = "final";
    pass.ref_pass_instance_number = 1;
    pass.pos_op = PASS_POS_INSERT_BEFORE;

    register_callback("myPlugin", PLUGIN_PASS_MANAGER_SETUP, NULL, &pass);
    std::cerr << "Plugin successfully initialized\n";

    return 0;
}