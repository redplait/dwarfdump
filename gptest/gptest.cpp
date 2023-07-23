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

int my_PLUGIN::dump_u_operand(const_rtx in_rtx, int idx, int level)
{
  if (XEXP (in_rtx, idx) != NULL)
  {
    rtx sub = XEXP (in_rtx, idx);
    enum rtx_code subc = GET_CODE (sub);
    if (GET_CODE (in_rtx) == LABEL_REF)
    {
      if (subc != CODE_LABEL)
      {
         return dump_e_operand(in_rtx, idx, level + 1);
      }
    }
  }
  return 0;  
}

int my_PLUGIN::dump_i_operand(const_rtx in_rtx, int idx, int level)
{
  if (idx == 4 && INSN_P (in_rtx))
  {
    const rtx_insn *in_insn = as_a <const rtx_insn *> (in_rtx);
    /*  Pretty-print insn locations.  Ignore scoping as it is mostly
        redundant with line number information and do not print anything
        when there is no location information available.  */
    if (INSN_HAS_LOCATION (in_insn))
    {
        expanded_location xloc = insn_location (in_insn);
        fprintf (m_outfp, " \"%s\":%i:%i", xloc.file, xloc.line, xloc.column);
    }
  }
  else if (idx == 6 && GET_CODE (in_rtx) == ASM_OPERANDS)
  {
    if (ASM_OPERANDS_SOURCE_LOCATION (in_rtx) != UNKNOWN_LOCATION)
    fprintf (m_outfp, " %s:%i",
        LOCATION_FILE (ASM_OPERANDS_SOURCE_LOCATION (in_rtx)),
        LOCATION_LINE (ASM_OPERANDS_SOURCE_LOCATION (in_rtx)));
    }
  else if (idx == 1 && GET_CODE (in_rtx) == ASM_INPUT)
  {
    if (ASM_INPUT_SOURCE_LOCATION (in_rtx) != UNKNOWN_LOCATION)
    fprintf (m_outfp, " %s:%i",
        LOCATION_FILE (ASM_INPUT_SOURCE_LOCATION (in_rtx)),
        LOCATION_LINE (ASM_INPUT_SOURCE_LOCATION (in_rtx)));
  } else {
    const char *name;
    int is_insn = INSN_P (in_rtx);
    if (is_insn && &INSN_CODE (in_rtx) == &XINT (in_rtx, idx)
          && XINT (in_rtx, idx) >= 0
          && (name = get_insn_name (XINT (in_rtx, idx))) != NULL)
      fprintf (m_outfp, " {%s}", name);
  }
  return 0;
}

int my_PLUGIN::dump_e_operand(const_rtx in_rtx, int idx, int level)
{
  auto e = XEXP (in_rtx, idx);
  if ( e )
  {
    dump_rtx(e, level + 1);
    return 1;
  }
  return 0;
}

int my_PLUGIN::dump_EV_code(const_rtx in_rtx, int idx, int level)
{
  int res = 0;
  if (XVEC (in_rtx, idx) != NULL)
  {
    int barrier = XVECLEN (in_rtx, idx);
    if (GET_CODE (in_rtx) == CONST_VECTOR
          && !GET_MODE_NUNITS (GET_MODE (in_rtx)).is_constant ())
      barrier = CONST_VECTOR_NPATTERNS (in_rtx);
    int len = XVECLEN (in_rtx, idx);
    fprintf(m_outfp, "XVECLEN %d\n", len);
    for (int j = 0; j < len; j++)
    {
      margin(level + 1);
      fprintf(m_outfp, "x[%d] ", j);
      dump_rtx (XVECEXP (in_rtx, idx, j), level + 1);
      res++;
    }
  }
  return res;
}

static void
print_poly_int (FILE *file, poly_int64 x)
{
  HOST_WIDE_INT const_x;
  if (x.is_constant (&const_x))
    fprintf (file, HOST_WIDE_INT_PRINT_DEC, const_x);
  else
    {
      fprintf (file, "[" HOST_WIDE_INT_PRINT_DEC, x.coeffs[0]);
      for (int i = 1; i < NUM_POLY_INT_COEFFS; ++i)
        fprintf (file, ", " HOST_WIDE_INT_PRINT_DEC, x.coeffs[i]);
      fprintf (file, "]");
    }
}

void my_PLUGIN::dump_rtx_operand(const_rtx in_rtx, char f, int idx, int level)
{
  int was_nl = 0;
  const char *str;
  margin(level + 2);
  fprintf(m_outfp, "[%d] ", idx);
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
        fputs ("(nil)", m_outfp);
      else
        fprintf (m_outfp, "(%s)", str);
      break;

   case 'i':
      was_nl = dump_i_operand(in_rtx, idx, level);
      break;

   case 'u':
      was_nl = dump_u_operand(in_rtx, idx, level);
      break;
    
   case 'e':
      was_nl = dump_e_operand(in_rtx, idx, level);
      break;

   case 'E':
   case 'V':
      was_nl = dump_EV_code(in_rtx, idx, level + 1);
      break;

   case 'n':
      fprintf (m_outfp, " %s", GET_NOTE_INSN_NAME (XINT (in_rtx, idx)));
      break;
 
   case 't':
      if (idx == 0 && GET_CODE (in_rtx) == DEBUG_IMPLICIT_PTR)
        // print_mem_expr (m_outfile, DEBUG_IMPLICIT_PTR_DECL (in_rtx));
        fprintf(m_outfp, "DEBUG_IMPLICIT_PTR_DECL");
      else if (idx == 0 && GET_CODE (in_rtx) == DEBUG_PARAMETER_REF)
        // print_mem_expr (m_outfile, DEBUG_PARAMETER_REF_DECL (in_rtx));
        fprintf(m_outfp, "DEBUG_PARAMETER_REF_DECL");
      else
        fprintf(m_outfp, "XTREE");
      break;

   case 'w':
      fprintf (m_outfp, HOST_WIDE_INT_PRINT_DEC, XWINT (in_rtx, idx));
      fprintf (m_outfp, " [" HOST_WIDE_INT_PRINT_HEX "]",
                 (unsigned HOST_WIDE_INT) XWINT (in_rtx, idx));
      break;

    case 'p':
      print_poly_int (m_outfp, SUBREG_BYTE (in_rtx));
      break;

  }
  if ( !was_nl )
    fputs("\n", m_outfp);
}

void my_PLUGIN::dump_rtx_hl(const_rtx in_rtx)
{
  if ( !in_rtx )
    return;
  rtx_code code = GET_CODE(in_rtx);
  if ( code > NUM_RTX_CODE )
    return;
  margin(1);
  dump_rtx(in_rtx, 1);
}

void my_PLUGIN::dump_mem_expr(const_tree expr)
{
  if ( expr == NULL_TREE )
    return;
  auto code = TREE_CODE(expr);
  auto name = get_tree_code_name(code);
  if ( name )
    fprintf(m_outfp, " %s", name);
  if ( code != COMPONENT_REF )
    return;
  auto op0 = TREE_OPERAND (expr, 0);
  if ( !op0 )
    return;
  char *str = ".";
  if ( (TREE_CODE (op0) == INDIRECT_REF
              || (TREE_CODE (op0) == MEM_REF
                  && TREE_CODE (TREE_OPERAND (op0, 0)) != ADDR_EXPR
                  && integer_zerop (TREE_OPERAND (op0, 1))
                  /* Dump the types of INTEGER_CSTs explicitly, for we
                     can't infer them and MEM_ATTR caching will share
                     MEM_REFs with differently-typed op0s.  */
                  && TREE_CODE (TREE_OPERAND (op0, 0)) != INTEGER_CST
                  /* Released SSA_NAMES have no TREE_TYPE.  */
                  && TREE_TYPE (TREE_OPERAND (op0, 0)) != NULL_TREE
                  /* Same pointer types, but ignoring POINTER_TYPE vs.
                     REFERENCE_TYPE.  */
                  && (TREE_TYPE (TREE_TYPE (TREE_OPERAND (op0, 0)))
                      == TREE_TYPE (TREE_TYPE (TREE_OPERAND (op0, 1))))
                  && (TYPE_MODE (TREE_TYPE (TREE_OPERAND (op0, 0)))
                      == TYPE_MODE (TREE_TYPE (TREE_OPERAND (op0, 1))))
                  && (TYPE_REF_CAN_ALIAS_ALL (TREE_TYPE (TREE_OPERAND (op0, 0)))
                      == TYPE_REF_CAN_ALIAS_ALL (TREE_TYPE (TREE_OPERAND (op0, 1))))
                  /* Same value types ignoring qualifiers.  */
                  && (TYPE_MAIN_VARIANT (TREE_TYPE (op0))
                      == TYPE_MAIN_VARIANT
                          (TREE_TYPE (TREE_TYPE (TREE_OPERAND (op0, 1)))))
                  && MR_DEPENDENCE_CLIQUE (op0) == 0)))
  {
    op0 = TREE_OPERAND (op0, 0);
    str = "->";
  }
  auto op1 = TREE_OPERAND (expr, 1);  
  code = TREE_CODE(op0);
  name = get_tree_code_name(code);
  if ( name )
    fprintf(m_outfp, " %s%s", str, name);
  if ( !op1 )
    return;    
  code = TREE_CODE(op1);
  name = get_tree_code_name(code);
  if ( name )
    fprintf(m_outfp, " 1:%s", name);
  if ( code != FIELD_DECL )
    return;
  // dump field name
  auto field_name = DECL_NAME(op1);
  if ( field_name )
    fprintf(m_outfp, " FName %s", IDENTIFIER_POINTER(field_name));
  auto ctx = DECL_CONTEXT(op1);
  if ( !ctx )
    return;
  code = TREE_CODE(ctx);
  name = get_tree_code_name(code);
  if ( name )
    fprintf(m_outfp, " %s", name);
  // record/union name
  auto t = TYPE_NAME(ctx);
  if ( t )
  {
    if ( DECL_NAME(t) )
      fprintf(m_outfp, " Name %s", IDENTIFIER_POINTER(DECL_NAME(t)) );
  } else {
    fprintf(m_outfp, " no_type");
  }   
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
  if ( INSN_CHAIN_CODE_P(code) )
    fprintf(m_outfp, "%d ", INSN_UID (in_rtx));
  fprintf(m_outfp, "%s", GET_RTX_NAME(code));
  // flags
  if (RTX_FLAG (in_rtx, in_struct))
    fputs ("/s", m_outfp);
  if (RTX_FLAG (in_rtx, volatil))
    fputs ("/v", m_outfp);
  if (RTX_FLAG (in_rtx, unchanging))
    fputs ("/u", m_outfp);
  if (RTX_FLAG (in_rtx, frame_related))
    fputs ("/f", m_outfp);
  if (RTX_FLAG (in_rtx, jump))
    fputs ("/j", m_outfp);
  if (RTX_FLAG (in_rtx, call))
    fputs ("/c", m_outfp);
  if (RTX_FLAG (in_rtx, return_val))
    fputs ("/i", m_outfp);

  fprintf(m_outfp, " %d lim %d", idx, limit);
  const char *format_ptr = GET_RTX_FORMAT(code);
  fprintf(m_outfp, " %s", format_ptr);

  if ( code == MEM && MEM_EXPR(in_rtx) )
  {
    fprintf(m_outfp, " MEM");
    dump_mem_expr(MEM_EXPR (in_rtx));
  }
  fputs("\n", m_outfp);  
  // dump operands
  for (; idx < limit; idx++)
    dump_rtx_operand (in_rtx, format_ptr[idx], idx, level + 1);  
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
          dump_rtx_hl(insn);
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