// This is the first gcc header to be included
#include "gcc-plugin.h"
#include "c-family/c-pretty-print.h"
#include "plugin-version.h"
#include "print-rtl.h"
#include "memmodel.h"
#include "rtl.h"
#include "emit-rtl.h"
#include "langhooks.h"

#include <iostream>
#include <fstream>
#include <string>
#include <stdarg.h>
#include <algorithm>

#include "my_plugin.h"

const struct pass_data my_PLUGIN_pass_data =
{
    .type = RTL_PASS,
    .name = "gptest",
    .optinfo_flags = OPTGROUP_NONE,
    .tv_id = TV_NONE,
    .properties_required = PROP_rtl, // | PROP_cfglayout),
    .properties_provided = 0,
    .properties_destroyed = 0,
    .todo_flags_start = 0,
    .todo_flags_finish = 0,
};

/* plugin parameters:
    -fplugin-arg-gptest-db=path to file/db connection string
    -fplugin-arg-gptest-user=db user
    -fplugin-arg-gptest-password=password for db access
    -fplugin-arg-gptest-asmproto
    -fplugin-arg-gptest-dumprtl
    -fplugin-arg-gptest-ic to dump imteger constants, optionally you can peek config filename
    -fplugin-arg-gptest-verbose
 */
const char *pname_verbose = "verbose";

rtl_plugin_with_args::rtl_plugin_with_args(gcc::context *ctxt, const char *full_name, 
  const struct pass_data &pd, struct plugin_argument *arguments, int argcounter)
 : rtl_opt_pass(pd, ctxt)
{
  argc = argcounter;   // number of arguments
  args = arguments;    // array containing arrguments (key,value)
  m_verbose = existsArgument(pname_verbose);
}

st_labels::st_labels(gcc::context *ctxt, struct plugin_argument *arguments, int argcounter)
 : rtl_plugin_with_args(ctxt, NULL, my_PLUGIN_pass_data, arguments, argcounter)
{
  m_outfp = m_verbose ? stdout : NULL;
}

my_PLUGIN::my_PLUGIN(gcc::context *ctxt, const char *full_name ,struct plugin_argument *arguments, int argcounter)
 : rtl_plugin_with_args(ctxt, full_name, my_PLUGIN_pass_data, arguments, argcounter),
   m_db(NULL)
{
    m_outfp = stdout;
    m_asmproto = existsArgument("asmproto"); 
    m_dump_rtl = existsArgument("dumprtl");
    m_dump_ic = existsArgument("ic");
    if ( m_dump_ic )
    {
      const char *ic_name = findArgumentValue("ic");
      if ( ic_name )
        read_ic_config(ic_name);
    }
    m_db_str = findArgumentValue("db");
    if ( m_db_str )
      m_db = get_pers();
    if ( m_db && !m_dump_rtl )
      m_outfp = NULL;
#ifdef GPPROF
    profiled = start_prof(full_name, "gptest.so");
#endif
}

my_PLUGIN::~my_PLUGIN()
{
  std::cerr << "~my_PLUGIN\n";  
  if ( m_outfp != NULL && m_outfp != stdout )
  {
    fclose(m_outfp);
    m_outfp = NULL;
  }
  if ( m_db )
  {
    delete m_db;
    m_db = NULL;
  }
#ifdef GPPROF
    if ( profiled ) stop_prof();
#endif
}

// return 0 if db connected
int my_PLUGIN::connect()
{
  if ( !m_db_str || !m_db )
    return 0;
  return m_db->connect(m_db_str, findArgumentValue("user"), findArgumentValue("password"));
}

void my_PLUGIN::start_file(const char *fn)
{
  if ( m_db )
    m_db->cu_start(fn);
}

void my_PLUGIN::stop_file()
{
  if ( m_db )
    m_db->cu_stop();
}

bool rtl_plugin_with_args::existsArgument(const char* key) const
{
   for (int i=0; i< argc; i++)
   {
      if (!strcmp(args[i].key, key))
        return true;
   }
   return false;
}

const char* rtl_plugin_with_args::findArgumentValue(const char* key)
{
   for (int i=0; i< argc; i++)
   {
      if (!strcmp(args[i].key, key))
        return args[i].value;
   }
   return NULL;
}

extern void print_declaration (pretty_printer *, tree, int, dump_flags_t);
extern int dump_generic_node (pretty_printer *, tree, int, dump_flags_t, bool);
extern void dump_function_header(FILE *, tree, dump_flags_t);
extern void make_decl_rtl (tree);
extern char *print_generic_expr_to_str (tree);
extern tree get_identifier (const char *);
extern tree gimple_get_virt_method_for_vtable (HOST_WIDE_INT, tree, unsigned HOST_WIDE_INT, bool *can_refer = NULL);
extern bool vtable_pointer_value_to_vtable (const_tree, tree *, unsigned HOST_WIDE_INT *);
extern tree get_identifier_with_length(const char *, size_t);

#include "attribs.h"

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
#define DEF_EDGE_FLAG(NAME,IDX)                \
  do {                                         \
    if (e->flags & EDGE_##NAME)                \
      {                                        \
       if (seen_flag)                          \
          fprintf (outfile, " | ");            \
        fprintf (outfile, "%s", (#NAME));      \
        seen_flag = true;                      \
      }                                        \
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
  if ( DECL_UID(arg) )
    fprintf(outfile, " UID %d", DECL_UID(arg));
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
  if ( !need_dump() )
    return;
  fputc(';', m_outfp);
  for ( int i = 0; i < level; i++ )
    fputc(' ', m_outfp);
}

int type_has_name(const_tree rt)
{
  if ( TREE_CODE(rt) == TYPE_DECL )
  {
    auto tn = DECL_NAME(rt);
    if ( !tn ) return 0;
    auto code = TREE_CODE(tn);
    if ( code == IDENTIFIER_NODE ) return 1;
    return !DECL_NAMELESS(tn);
  }
  return 0;
}

const char *my_PLUGIN::is_cliteral(const_rtx in_rtx, int &csize)
{
  if ( GET_CODE(in_rtx) != SYMBOL_REF )
    return NULL;
  tree decl = SYMBOL_REF_DECL(in_rtx);
  if ( !decl )
    return NULL;
  if ( TREE_CODE(decl) != VAR_DECL )
    return NULL;
  auto v = DECL_INITIAL(decl);
  if ( !v || v == error_mark_node )
    return NULL;
  if ( TREE_CODE(v) != STRING_CST )
    return NULL;
  csize = TREE_STRING_LENGTH(v);
  return TREE_STRING_POINTER(v);
}

int my_PLUGIN::dump_0_operand(const_rtx in_rtx, int idx, int level)
{
  if ( 1 == idx && GET_CODE(in_rtx) == SYMBOL_REF )
  {
    int flags = SYMBOL_REF_FLAGS(in_rtx);
    if ( flags && need_dump() )
      fprintf(m_outfp, "symbol_flags %X ", flags);
    tree decl = SYMBOL_REF_DECL(in_rtx);
    if ( decl && need_dump() )
    {
      auto code = TREE_CODE(decl);
      auto name = get_tree_code_name(code);
      if ( name )
        fprintf(m_outfp, "decl %s ", name);
      if ( code == VAR_DECL )
      {
        if ( DECL_IN_TEXT_SECTION(decl) )
          fprintf(m_outfp, "in_text ");
        if ( DECL_IN_CONSTANT_POOL(decl) )
          fprintf(m_outfp, "in_cpool ");
        if ( DECL_BY_REFERENCE(decl) )
          fprintf(m_outfp, "byref ");
        if ( DECL_HAS_VALUE_EXPR_P(decl) )
          fprintf(m_outfp, "has_value ");
        dump_section(decl);
        auto v = DECL_INITIAL(decl);
        if ( v && v != error_mark_node )
        {
          code = TREE_CODE(v);
          name = get_tree_code_name(code);
          if ( name )
            fprintf(m_outfp, "initial %s ", name);
          dump_tree_MF(v);
        } 
      }
    }
    return 0;
  } else if ( idx == 3 && NOTE_P(in_rtx) )
  {
    switch(NOTE_KIND(in_rtx))
    {
      case NOTE_INSN_EH_REGION_BEG:
         if ( need_dump() )
           fprintf(m_outfp, "EH_REGION_BEG %d", NOTE_EH_HANDLER(in_rtx));
        break;
      case NOTE_INSN_EH_REGION_END:
         if ( need_dump() )
           fprintf(m_outfp, "EH_REGION_END %d", NOTE_EH_HANDLER(in_rtx));
        break;
      case NOTE_INSN_VAR_LOCATION: {
          auto vl = NOTE_VAR_LOCATION(in_rtx);
          if ( need_dump() )
            fprintf(m_outfp, "VAR_LOC ");
          expr_push(vl, idx);
          dump_rtx(vl, level + 1);
          expr_pop();
          return 1;
         }
        break;
    }
  } else if ( idx == 7 && JUMP_P(in_rtx) )
  {
    const_rtx jl = JUMP_LABEL(in_rtx);
    if ( !jl )
      return 0;
    auto code = GET_CODE(jl);
    if ( need_dump() )
    {
      auto jcode = GET_CODE(jl);
      if ( jcode == RETURN || jcode == SIMPLE_RETURN )
        fprintf(m_outfp, "jl %s\n", GET_RTX_NAME(code));
      else 
        fprintf(m_outfp, "jl %s to %d\n", GET_RTX_NAME(code), INSN_UID(jl));
    }
/*    if ( code == CODE_LABEL )
    {
      dump_rtx(jl, 1 + level);
      return 1;
    } */
  }
  return 0; 
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
    if ( need_dump() && idx <= 1 && !m_rtexpr.empty() )
    {
      // dump_exprs();
      fprintf(m_outfp, "%d", INSN_UID(sub));
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

void my_PLUGIN::pass_error(const char *fmt, ...)
{
  if ( !m_db )
    return;
  va_list ap;
  va_start(ap, fmt);
  char err_buf[1024];
  vsnprintf(err_buf, sizeof(err_buf) - 1, fmt, ap);
  err_buf[sizeof(err_buf) - 1] = 0;
  m_db->report_error(err_buf);
  va_end(ap);  
}

aux_type_clutch::aux_type_clutch(const_rtx in_rtx)
 : completed(false),
   level(0),
   last(NULL_TREE),
   left_name(nullptr)
{
  off = 0;
  has_off = is_lvar = false;
  auto code = GET_CODE(in_rtx);
  if ( code == DEBUG_PARAMETER_REF || code == DEBUG_IMPLICIT_PTR )
    return;
  if ( MEM_OFFSET_KNOWN_P(in_rtx) )
  {
    has_off = true;
    auto x = MEM_OFFSET(in_rtx);
    HOST_WIDE_INT const_x;
    if (x.is_constant (&const_x))
      off = const_x;
    else
      off = x.coeffs[0];  
  }
}

// typical sequence in stack for call looks like
// call:N mem or reg:0 - if 1 this is arguments for call
int my_PLUGIN::is_call() const
{
  if ( m_rtexpr.empty() )
    return 0;
  int idx = -1;
  for ( auto r = m_rtexpr.rbegin(); r != m_rtexpr.rend(); ++r )
  {
    if ( r->m_ce == CALL )
      return !idx;
    idx = r->m_idx;
  }
  return 0;
}

// scan for set:N X:0
int my_PLUGIN::is_write() const
{
  if ( m_rtexpr.empty() )
    return 0;
  int idx = -1;
  for ( auto r = m_rtexpr.rbegin(); r != m_rtexpr.rend(); ++r )
  {
    if ( r->m_ce == SET )
      return !idx;
    idx = r->m_idx;
  }
  return 0;
}

// set:X reg:0
int my_PLUGIN::is_set_reg() const
{
  if ( m_rtexpr.empty() )
    return 0;
  auto r = m_rtexpr.rbegin();
  if ( r->m_ce != REG || r->m_idx )
    return 0;
  ++r;
  if ( r == m_rtexpr.rend() )
    return 0;
  return r->m_ce == SET;
}

// set:X mem:X
int my_PLUGIN::is_set_mem() const
{
  if ( m_rtexpr.empty() )
    return 0;
  int state = 0;
  for ( auto r = m_rtexpr.rbegin(); r != m_rtexpr.rend(); ++r )
  {
    if ( r->m_ce == MEM && !state ) { state++; continue; }
    if ( state && r->m_ce == SET ) return 1;
  }
  return 0; 
}

int my_PLUGIN::is_plus() const
{
  if ( m_rtexpr.empty() )
    return 0;
  for ( auto r = m_rtexpr.rbegin(); r != m_rtexpr.rend(); ++r )
    if ( r->m_ce == PLUS ) return 1;
  return 0;
}

// var_location
int my_PLUGIN::is_var_loc() const
{
  if ( m_rtexpr.empty() )
    return 0;
  for ( auto r = m_rtexpr.rbegin(); r != m_rtexpr.rend(); ++r )
  {
    if ( r->m_ce == VAR_LOCATION )
      return 1;
  }
  return 0;
}

int my_PLUGIN::inside_if() const
{
  if ( m_rtexpr.empty() )
    return 0;
  for ( auto r = m_rtexpr.rbegin(); r != m_rtexpr.rend(); ++r )
  {
    if ( r->m_ce == IF_THEN_ELSE )
      return 1;
  }
  return 0; 
}

int my_PLUGIN::is_sb() const
{
  if ( m_rtexpr.empty() )
    return 0;
  auto r = m_rtexpr.rbegin();
  if ( r->m_ce == CONST_INT )
  {
    r++;
    if ( r == m_rtexpr.rend() )
      return 0;
  }
  return r->m_sb;
}

// when in stack presents expr_list with index 6 (and probably insn_list or int_list too) 
// then this is not integer const but just EH block index
// for sure good also to check GET_MODE - it should be < REG_NOTE_MAX and actually is not machine_mode 
int my_PLUGIN::is_eh_num() const
{
  if ( m_rtexpr.empty() )
    return 0;
  auto r = m_rtexpr.rbegin();
  if ( r->m_ce == CONST_INT )
  {
    r++;
    if ( r == m_rtexpr.rend() )
      return 0;
  }
  return (r->m_ce == EXPR_LIST) && (r->m_idx == 6);
}

int my_PLUGIN::is_symref() const
{
  if ( m_rtexpr.empty() )
    return 0;
  auto r = m_rtexpr.rbegin();
  return ( r->m_ce == SYMBOL_REF );  
}

int my_PLUGIN::is_symref_call() const
{
  if ( m_rtexpr.empty() )
    return 0;
  int idx = -1;
  auto r = m_rtexpr.rbegin();
  if ( r->m_ce != SYMBOL_REF )
    return 0;  
  for ( ++r; r != m_rtexpr.rend(); ++r )
  {
    if ( r->m_ce == CALL )
      return !idx;
    idx = r->m_idx;
  }
  return 0;
}

void my_PLUGIN::expr_push(const_rtx in_rtx, int idx)
{
  bool is_sb = RTX_FLAG (in_rtx, frame_related);
  if ( is_sb )
  {
    // mark previous state as stack based too
    if ( !m_rtexpr.empty() )
      m_rtexpr.rbegin()->m_sb = true;
  }
  m_rtexpr.push_back( { (enum rtx_class)GET_CODE(in_rtx), idx, is_sb } );  
}

void my_PLUGIN::dump_known_uids()
{
  if ( !need_dump() || m_known_uids.empty() )
    return;
  fprintf(m_outfp, "; known uids:\n");
  for ( auto &u: m_known_uids )
  {
    fprintf(m_outfp, ";  %x - %s\n", u.first, u.second.c_str());
  }
}

void my_PLUGIN::make_expr_cmt(const_rtx in_rtx, std::string &cmt)
{
  if ( m_rtexpr.empty() )
    return;
  rtx_code code = GET_CODE(in_rtx);
  if ( INSN_CHAIN_CODE_P(code) )
    cmt = std::to_string(INSN_UID (in_rtx));
  else
    cmt.clear();
  for ( auto &rt: m_rtexpr )
  {
    cmt += ' ';
    cmt += GET_RTX_NAME(rt.m_ce);
    cmt += ':';
    cmt += std::to_string(rt.m_idx);
  }
}

void my_PLUGIN::dump_exprs()
{
  if ( !need_dump() || m_rtexpr.empty() )
    return;
  fprintf(m_outfp, "[stack");
  for ( auto &rt: m_rtexpr )
  {
    fprintf(m_outfp, " %s:%d", GET_RTX_NAME(rt.m_ce), rt.m_idx);
    if ( rt.m_sb )
     fprintf(m_outfp, "/f");
  }
  fprintf(m_outfp, "]");
}

int my_PLUGIN::dump_r_operand(const_rtx in_rtx, int idx, int level)
{
  unsigned int regno = REGNO (in_rtx);
  if ( need_dump() )
  {
    if ( regno <= LAST_VIRTUAL_REGISTER )
      fprintf (m_outfp, " %d", regno);
    if (regno < FIRST_PSEUDO_REGISTER)
      fprintf (m_outfp, " %s", reg_names[regno]);
  }
  if (REG_ATTRS (in_rtx))
  {
    if (REG_EXPR (in_rtx))
    {
      if ( need_dump() )  
        fprintf(m_outfp, " RMEM");
      dump_rmem_expr(REG_EXPR (in_rtx), in_rtx);
    }
  }
  return 0; 
}

int my_PLUGIN::dump_e_operand(const_rtx in_rtx, int idx, int level)
{
  if ( !in_rtx )
    return 0;
  auto e = XEXP (in_rtx, idx);
  if ( e )
  {
    expr_push(e, idx);
    dump_rtx(e, level + 1);
    expr_pop();
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
    if ( need_dump() )
      fprintf(m_outfp, "XVECLEN %d\n", len);
    for (int j = 0; j < len; j++)
    {
      auto xelem = XVECEXP (in_rtx, idx, j);
      if ( xelem )
      {
        expr_push(xelem, j);
        if ( need_dump() )
        {  
          margin(level + 1);
          fprintf(m_outfp, "x[%d] ", j);
        }
        dump_rtx (xelem, level + 1);
        expr_pop();
        res++;
      }
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
  int was_nl = 0, 
      add_field = 0;
  const char *str;
  margin(level + 2);
  if ( need_dump() )
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
      {
        if ( need_dump() )
          fputs ("(nil)", m_outfp);
      } else {
          dump_exprs();
          int len = 0;
          const char *cres = is_cliteral(in_rtx, len);
          if ( need_dump() )
          {
            if ( cres )
              fprintf (m_outfp, " (%s) len %d", str, len);
            else
              fprintf (m_outfp, " (%s)", str);
          }
          if ( m_db && is_symref() )
          {
            xref_kind kind = xref;
            if ( is_symref_call() )
              kind = xcall;
            if ( cres )
              m_db->add_literal(cres, len);
            else
              m_db->add_xref(kind, str);
          }
        }
      break;

   case 'i':
      if ( need_dump() )
        was_nl = dump_i_operand(in_rtx, idx, level);
      break;

   case 'r':
      was_nl = dump_r_operand(in_rtx, idx, level);
      break;

   case '0':
      was_nl = dump_0_operand(in_rtx, idx, level);
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
      if ( need_dump() )
        fprintf (m_outfp, " %s", GET_NOTE_INSN_NAME (XINT (in_rtx, idx)));
      break;
 
   case 't':
      if (idx == 0 && GET_CODE (in_rtx) == DEBUG_IMPLICIT_PTR)
      {
        if ( need_dump() )
          fprintf(m_outfp, "DEBUG_IMPLICIT_PTR_DECL ");
        expr_push(in_rtx, idx);
        dump_mem_expr(DEBUG_IMPLICIT_PTR_DECL (in_rtx), in_rtx);
        expr_pop();
      } else if ( idx == 0 && GET_CODE (in_rtx) == DEBUG_PARAMETER_REF )
      {
        if ( need_dump() )
          fprintf(m_outfp, "DEBUG_PARAMETER_REF_DECL ");
        expr_push(in_rtx, idx);
        dump_mem_expr(DEBUG_PARAMETER_REF_DECL (in_rtx), in_rtx);
        expr_pop();
      } else {
        if ( need_dump() )
          fprintf(m_outfp, "XTREE");
      }
      break;

   case 'w':
      if ( need_dump() )
      {
        fprintf (m_outfp, HOST_WIDE_INT_PRINT_DEC, XWINT (in_rtx, idx));
        fprintf (m_outfp, " " HOST_WIDE_INT_PRINT_HEX,
                   (unsigned HOST_WIDE_INT) XWINT (in_rtx, idx));
      }
      if ( m_requal && m_rbase && GET_CODE (in_rtx) == CONST_INT && is_plus() )
        add_field = add_fref_from_equal(XWINT(in_rtx, idx));
      if ( !add_field && m_dump_ic && !in_pe && GET_CODE (in_rtx) == CONST_INT )
      {
        if ( !inside_if() && !is_sb() && !is_eh_num() && XWINT(in_rtx, idx) && ic_filter() )
        {
          if ( need_dump() )
            dump_exprs();
          if ( m_db )
          {
            std::string cmt;
            make_expr_cmt(in_rtx, cmt);
            if ( !cmt.empty() )
            {
              // fprintf(stdout, "%s\n", cmt.c_str());
              m_db->add_comment(cmt.c_str());
            }
            m_db->add_ic(XWINT(in_rtx, idx));
          }
        }
      }
      break;

    case 'p':
      if ( need_dump() )
        print_poly_int (m_outfp, SUBREG_BYTE (in_rtx));
      break;

    case 'B':
       if ( need_dump() && XBBDEF(in_rtx, idx) )
         fprintf(m_outfp, "block %d", XBBDEF(in_rtx, idx)->index);
      break;

  }
  if ( !was_nl && need_dump() )
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

inline bool need_deref_compref0(const_tree op0)
{
  return (TREE_CODE (op0) == INDIRECT_REF
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
                  && MR_DEPENDENCE_CLIQUE (op0) == 0)
        );
}

void my_PLUGIN::dump_rmem_expr(const_tree expr, const_rtx in_rtx)
{
  if ( expr == NULL_TREE || expr == error_mark_node )
    return;
  dump_exprs();
  auto code = TREE_CODE(expr);
  auto name = get_tree_code_name(code);
  if ( name && need_dump() )
    fprintf(m_outfp, " %s", name);
  aux_type_clutch clutch(in_rtx);  
  if ( code == COMPONENT_REF )
  {
    dump_comp_ref(expr, clutch);
    return;
  }
  if ( code == SSA_NAME )
  {
    dump_ssa_name(expr, clutch);
    return;
  }
  if ( code == METHOD_TYPE )
  {
    dump_method(expr);
    return;
  }
  if ( code == PARM_DECL )
  {
    auto ai = m_args.find(expr);
    if ( ai != m_args.end() )
    {
      if ( need_dump() ) fprintf(m_outfp, " Arg%d", ai->second.first);
      if ( is_set_mem() ) m_arg_no = ai->second.first;
    }
    return;
  }
  // too noisy - for debug only
#ifdef DEBUG
  if ( code == RESULT_DECL || code == VAR_DECL ) return;
  if ( is_var_loc() ) return;
  claim_unknown(code, "rmem");
#endif
}

// seems DECL_VINDEX returns function_decl.vindex so we can extract it only from FUNCTION_DECL
HOST_WIDE_INT extract_vindex(const_tree expr)
{
  auto fdecl = FUNCTION_DECL_CHECK(expr);
  if ( !fdecl || fdecl == error_mark_node )
    return 0;
  const_tree vi = DECL_VINDEX(fdecl);
  if ( !vi || vi == error_mark_node )
    return 0;
  if ( TREE_CODE(vi) != INTEGER_CST )
    return 0;
  return tree_to_shwi(vi);
}

void my_PLUGIN::dump_ftype(const_tree expr)
{
  if ( need_dump() )
    fprintf(m_outfp, " uid %x", TYPE_UID(expr));  
  tree parent_type = TYPE_METHOD_BASETYPE(expr);
  if ( parent_type )
  {
    dump_method(expr);
    return;
  }
  if ( TREE_CODE(expr) == METHOD_TYPE && DECL_VIRTUAL_P(expr) )
  {
    if ( need_dump() )
      fprintf(m_outfp, " findex" HOST_WIDE_INT_PRINT_DEC, extract_vindex(expr));
  }
  // dump args
  tree arg = TYPE_ARG_TYPES(expr);
  while (arg && arg != void_list_node && arg != error_mark_node)
  {
    dump_tree_MF(arg);
    arg = TREE_CHAIN(arg);
  }
  if ( TYPE_CONTEXT(expr) )
  {
    tree ctx = TYPE_CONTEXT(expr);
    auto code = TREE_CODE(ctx);
    auto name = get_tree_code_name(code);
    if ( name && need_dump() )
      fprintf(m_outfp, " ctx %s", name);
  }
  dump_tree_MF(expr);
}

void my_PLUGIN::dump_tree_MF(const_tree expr)
{
  if ( !need_dump() )
    return;
  char *dumped = print_generic_expr_to_str(CONST_CAST_TREE(expr));
  fprintf(m_outfp, " %s", dumped);
  free(dumped);  
}

void my_PLUGIN::dump_method(const_tree expr)
{
  if ( !TYPE_P(expr) )
    return;
// Achtung! bcs we have METHOD_TYPE here all DECL_XXX is not work
//  const_tree fdecl = FUNCTION_DECL_CHECK(expr);
//  if ( !fdecl || fdecl == error_mark_node )
//    return;
/*
  const_tree vi = DECL_VINDEX(fdecl);
  if ( !vi )
    return;
  if ( vi == error_mark_node )
  {
    if ( m_db )
      pass_error("error decl_vindex, type of function_decl %d", TREE_CODE(fdecl));
    return;
  }
  auto code = TREE_CODE(vi);
  if ( code == INTEGER_CST )
  {
    if ( need_dump() && tree_fits_shwi_p(vi) )
      fprintf(m_outfp, " vindex " HOST_WIDE_INT_PRINT_DEC, tree_to_shwi(vi));
  } else {
    auto name = get_tree_code_name(code);
    if ( name && need_dump() )
      fprintf(m_outfp, " vindex_type %s", name);
  }
  HOST_WIDE_INT vi0 = extract_vindex(expr);
*/
  auto t = TREE_TYPE(expr);
  if ( t )
  {
    if ( TYPE_NAME(t) && need_dump() )
      fprintf(m_outfp, " MName %s", IDENTIFIER_POINTER(TYPE_NAME(t)) );
    tree class_type = TYPE_METHOD_BASETYPE(expr);
    if ( class_type )
    {
      auto base = TYPE_NAME(class_type);
      if ( DECL_NAME(base) && need_dump() )
        fprintf(m_outfp, " basetype %s", IDENTIFIER_POINTER(DECL_NAME(base)));
      
      tree found = NULL_TREE;
      // lets try find virtual method with it's type - it stored in expr
      for ( tree f = TYPE_FIELDS(class_type); f; f = TREE_CHAIN(f) )
      {
        // skip vars
        if ( TREE_CODE(f) == FIELD_DECL )
          continue;
        // method not neseccary must be virtual  
        // if ( !DECL_VIRTUAL_P(f) )
        //  continue;
        if ( TREE_TYPE(f) != expr )
          continue;
        found = f;
        if ( DECL_NAME(f) && need_dump() )
          fprintf(m_outfp, " method %s", IDENTIFIER_POINTER(DECL_NAME(f)));
        break;
      }
      /* we have type of base class in parent_type and method_type - try to find name of this method in base classes */
      if ( !found )
      {
        dump_class_rec(TYPE_BINFO(class_type), TYPE_BINFO(class_type), 0);
        try_class_rec(TYPE_BINFO(class_type), TYPE_BINFO(class_type), expr, &class_type, &found);
        if ( found )
          base = TYPE_NAME(class_type);
      }
      if ( m_db && DECL_NAME(base) && found && DECL_NAME(found) )
      {
        xref_kind kind = xref;
        std::string pers_arg = IDENTIFIER_POINTER(DECL_NAME(base));
        pers_arg += ".";
        pers_arg += IDENTIFIER_POINTER(DECL_NAME(found));
        if ( is_call() )
          kind = vcall;
        m_db->add_xref(kind, pers_arg.c_str());
      }
      if ( !found )
      {
        if ( need_dump() )
        {
          fprintf(m_outfp, "no_method_found %X", TREE_CODE(expr));
          // dump_tree_MF(class_type);      
        }
        if ( m_db )
          pass_error("cannot find method for type %d", TREE_CODE(class_type));
      }
    }
    dump_tree_MF(expr);
  } else {
    if ( need_dump() )
      fprintf(m_outfp, " no_typename");
    if ( m_db )
    {
      pass_error("dump_method: no type for %d", TREE_CODE(expr));
    }
  }
}

bool is_known_ssa_type(const_tree t)
{
  auto code = TREE_CODE(t);  
  return (code == VOID_TYPE) ||
         (code == NULLPTR_TYPE) ||
         (code == BOOLEAN_TYPE) ||
         (code == INTEGER_TYPE) ||
         (code == ENUMERAL_TYPE) ||
         (code == REAL_TYPE)    ||
         (code == COMPLEX_TYPE) ||
         (code == VECTOR_TYPE)  ||
         (code == ARRAY_TYPE)
  ;
}

// ripped from is_vptr_store
int my_PLUGIN::is_vptr(const_tree expr)
{
  return (TREE_CODE(expr) == FIELD_DECL) &&
         DECL_VIRTUAL_P(expr);
}

// store result from clutch if last type was field -> function
void my_PLUGIN::store_aux(aux_type_clutch &clutch)
{
  if ( !clutch.completed || !clutch.last )
    return;
  if ( is_vptr(clutch.last) )
    return;
  auto code = TREE_CODE(clutch.last);
  auto name = get_tree_code_name(code);
  if ( !name )
    return;
  if ( need_dump() )
    fprintf(m_outfp, " store_aux %s", name);
  if ( FIELD_DECL != TREE_CODE(clutch.last) )
    return;
  auto t = TREE_TYPE(clutch.last);
  if ( !t )
    return;
  if ( ARRAY_TYPE == TREE_CODE(t) )
  {
    t = TREE_TYPE(t);
    code = TREE_CODE(t);
    if ( need_dump() )
    {
      name = get_tree_code_name(code);
      if ( name )
        fprintf(m_outfp, " arr_type %s", name);
    }
  }
  if ( !POINTER_TYPE_P(t) )
    return;
  while( POINTER_TYPE_P(t))
    t = TREE_TYPE(t);
  if ( FUNCTION_TYPE == TREE_CODE(t) )
  {
    auto uid = TYPE_UID(t);
    m_known_uids[ { uid, bb_index } ] = clutch.txt;
    if ( need_dump() )
    {
      fprintf(m_outfp, " store fptr uid %x", uid);
    }
  }
}

// stealed from class.cc dump_class_hierarchy_r
const_tree my_PLUGIN::try_class_rec(const_tree binfo, const_tree igo, const_tree expr, tree *base, tree *found)
{
  if ( binfo != igo )
    return NULL_TREE;
  igo = TREE_CHAIN(binfo);
  tree base_binfo;
  for ( int i = 0; BINFO_BASE_ITERATE(binfo, i, base_binfo); ++i )
  {
    auto type = BINFO_TYPE(base_binfo);
    auto rt = TYPE_NAME(type);
//    if ( need_dump() && rt && type_has_name(rt) )
//      fprintf(m_outfp, "try_class_rec %X for %s ", TREE_CODE(base_binfo), IDENTIFIER_POINTER(DECL_NAME(rt)) );
    for ( tree f = TYPE_FIELDS(type); f; f = TREE_CHAIN(f) )
    {
      // skip vars
      if ( TREE_CODE(f) == FIELD_DECL )
        continue;
      // method not neseccary must be virtual
      // if ( !DECL_VIRTUAL_P(f) )
      //  continue;
      if ( TREE_TYPE(f) == expr )
      {
        *found = f;
        *base = type;
        return NULL_TREE;
      }
/*    if ( need_dump() )
      {
        if ( rt && type_has_name(rt) )
          fprintf(m_outfp, "%s ", IDENTIFIER_POINTER(DECL_NAME(rt)) );
        fprintf(m_outfp, "vm %p expr %p\n", TREE_TYPE(f), expr);
      } */
    }
    igo = try_class_rec(base_binfo, igo, expr, base, found);
    if ( *found )
      return NULL_TREE;
  }
  return igo;
}

const_tree my_PLUGIN::dump_class_rec(const_tree binfo, const_tree igo, int level)
{
  if ( !need_dump() )
    return NULL_TREE;
  auto type = BINFO_TYPE(binfo);
  if ( binfo != igo )
    return NULL_TREE;
  if ( !level )
    fprintf(m_outfp, "\n");
  margin(level);
  if ( BINFO_VIRTUAL_P(binfo) )
    fprintf(m_outfp, "virtual ");
  auto rt = TYPE_NAME(type);
  if ( rt )
  {
    if ( type_has_name(rt) )
      fprintf(m_outfp, "%s ", IDENTIFIER_POINTER(DECL_NAME(rt)) );
  }
  if ( BINFO_SUBVTT_INDEX(binfo) )
  {
    auto sv = BINFO_SUBVTT_INDEX(binfo);
    auto code = TREE_CODE(sv);
    auto name = get_tree_code_name(code);
    fprintf(m_outfp, "subvttidx %s ", name);
  }
  if ( BINFO_VPTR_INDEX(binfo) )
  {
    auto vi = BINFO_VPTR_INDEX(binfo);
    auto code = TREE_CODE(vi);
    auto name = get_tree_code_name(code);
    fprintf(m_outfp, "vptridx %s ", name);
  }
  if ( BINFO_VPTR_FIELD(binfo) )
  {
    auto vi = BINFO_VPTR_FIELD(binfo);
    auto code = TREE_CODE(vi);
    auto name = get_tree_code_name(code);
    fprintf(m_outfp, "vbaseoffset %s ", name);
  }
  if ( BINFO_VTABLE(binfo) )
  {
    auto vi = BINFO_VTABLE(binfo);
    auto code = TREE_CODE(vi);
    auto name = get_tree_code_name(code);
    fprintf(m_outfp, "vptr %s ", name);
  }
  fprintf(m_outfp, "\n");
  igo = TREE_CHAIN(binfo);
  tree base_binfo;
  for ( int i = 0; BINFO_BASE_ITERATE(binfo, i, base_binfo); ++i )
    igo = dump_class_rec(base_binfo, igo, level + 1);
  return igo;
}

/* type tree are tree_type_non_common which contains embedded tree
    tree_type_with_lang_specific
     field common is tree_type_common
       tree_common
       size
       size_unit
       attributes
       pointer_to
       reference_to
       canonical - TYPE_CANONICAL
       next_variant - TYPE_NEXT_VARIANT
       main_variant - TYPE_MAIN_VARIANT
       context - TYPE_CONTEXT
       name - TYPE_NAME
   tree fields:
     values - actually this is list of fields, see TYPE_FIELDS
     minval - TYPE_MIN_VALUE
     maxval - TYPE_MAX_VALUE
     lang_1 - TYPE_LANG_SLOT_1
*/
void my_PLUGIN::dump_type_tree(const_tree in_t)
{
  if ( !need_dump() || !in_t || !TYPE_P(in_t) )
    return;
  const_tree t;
  bool need_close = false;
#define DUMP_NODE(name) if ( t && t != error_mark_node ) {            \
  if ( !need_close ) fputc('(', m_outfp);                             \
  need_close = true;                                                  \
  fprintf(m_outfp, "(%s %s", name, get_tree_code_name(TREE_CODE(t))); \
  if ( TYPE_P(t) && TYPE_UID(t) ) fprintf(m_outfp, " uid %d", TYPE_UID(t));        \
  fputc(')', m_outfp); }

  t = get_containing_scope(in_t);
  DUMP_NODE("scope");
//  t = TYPE_POINTER_TO(in_t);
//  DUMP_NODE("pointer_to");
//  t = TYPE_REFERENCE_TO(in_t);
//  DUMP_NODE("reference_to");
  t = TYPE_CANONICAL(in_t);
  DUMP_NODE("canonical");
  t = TYPE_NEXT_VARIANT(in_t);
  DUMP_NODE("next_variant");
  t = TYPE_MAIN_VARIANT(in_t);
  DUMP_NODE("main_variant");
  t = TYPE_CONTEXT(in_t);
  DUMP_NODE("context");
  auto in_code = TREE_CODE(in_t);
  if ( in_code == INTEGER_TYPE || in_code == REAL_TYPE || in_code == FIXED_POINT_TYPE )
  {
    t = TYPE_MIN_VALUE(in_t);
    DUMP_NODE("min_value");
    t = TYPE_MAX_VALUE(in_t);
    DUMP_NODE("max_value");
  }
  t = TYPE_LANG_SLOT_1(in_t);
  DUMP_NODE("lang_1");
  if ( need_close )
    fputc(')', m_outfp);
}

/* tree_field_decl contains embedded tree_decl_common with
     size_unit - DECL_SIZE_UNIT
     initial - DECL_INITIAL
     attributes
     abstract_origin - DECL_ABSTRACT_ORIGIN
   tree fields:
     offset - DECL_FIELD_OFFSET
     bit_field_type - DECL_BIT_FIELD_TYPE
     qualifier - DECL_QUALIFIER
     bit_offset - DECL_FIELD_BIT_OFFSET
     fcontext - DECL_FCONTEXT
*/ 
void my_PLUGIN::dump_field_decl(const_tree in_t)
{
  if ( !need_dump() || !in_t || !DECL_P(in_t) )
    return;
  const_tree t;
  bool need_close = false;
  t = DECL_INITIAL(in_t);
  DUMP_NODE("initial");
  t = DECL_ABSTRACT_ORIGIN(in_t);
  DUMP_NODE("abstract_origin");
  t = DECL_QUALIFIER(in_t);
  DUMP_NODE("qualifier");
  t = DECL_FCONTEXT(in_t);
  DUMP_NODE("fcontext");
  if ( need_close )
    fputc(')', m_outfp);
}

static void append_name(aux_type_clutch &clutch, const char *name)
{
  if ( !name || !*name ) return;
  // check if this is nested struct like struct1.struct2.field
  if ( clutch.txt.empty() ) {
    clutch.txt = name;
    return;
  }
  clutch.txt += "::";
  clutch.txt += name;
}

// f - field decl
void my_PLUGIN::try_nameless(const_tree f, aux_type_clutch &clutch)
{
  if ( !f )
    return;
  auto code = TREE_CODE(f);
  if ( code == IDENTIFIER_NODE ) {
    if ( need_dump() )
      fprintf(m_outfp, " Name %s", IDENTIFIER_POINTER(f) );
    clutch.last = f;
    append_name(clutch, IDENTIFIER_POINTER(f));
    return;
  }
  // check ssa name - ripped from tree-pretty-print.cc
  if ( TREE_CODE(f) == SSA_NAME && SSA_NAME_IDENTIFIER(f) )
  {
    auto sn = SSA_NAME_VAR(f);
    if ( !sn || DECL_NAMELESS(sn) ) return;
    auto si = SSA_NAME_IDENTIFIER(f);
    if ( need_dump() )
      fprintf(m_outfp, " Name %s", IDENTIFIER_POINTER(si) );
    clutch.last = f;
    append_name(clutch, IDENTIFIER_POINTER(si));
    return;
  }
  // check that we have field name
  auto fn = DECL_NAME(f);
  if ( !fn ) return;
  code = TREE_CODE(fn);
  if ( code == IDENTIFIER_NODE ) {
    if ( need_dump() )
      fprintf(m_outfp, " Name %s", IDENTIFIER_POINTER(fn) );
    clutch.last = f;
    append_name(clutch, IDENTIFIER_POINTER(fn));
    return;
  }
  if ( DECL_NAMELESS(fn) ) return;
  dump_field_decl(f);
}

const_tree my_PLUGIN::check_arg(const_tree t)
{
  if ( !t ) return nullptr;
  t = TREE_TYPE(t);
  if ( !t ) return nullptr;
  auto ct = TREE_CODE(t);
  if ( ct == REFERENCE_TYPE || ct == POINTER_TYPE )
  {
    if ( ct == REFERENCE_TYPE )
      t = TREE_TYPE(t);
    while( POINTER_TYPE_P(t))
      t = TREE_TYPE(t);
    if ( t == error_mark_node )
      return nullptr;
    if ( RECORD_OR_UNION_TYPE_P(t) ) return t;
  }
  return nullptr;
}

void my_PLUGIN::dump_ssa_name(const_tree op0, aux_type_clutch &clutch)
{
  auto t = TREE_TYPE(op0);
  if ( !t || t == error_mark_node )
    return;
  auto ct0 = TREE_CODE(t);
  auto name = get_tree_code_name(ct0);
  if ( !name )
    return;
  // ripped from tree-pretty-print.cc function dump_generic_node
  if ( SSA_NAME_IDENTIFIER(op0) )
  {
    auto si = SSA_NAME_IDENTIFIER(op0);
    if ( need_dump() )
      fprintf(m_outfp, "(%s", IDENTIFIER_POINTER(si) );
    auto sv = SSA_NAME_VAR(op0);
    if ( sv ) {
      auto sname = get_tree_code_name(TREE_CODE(sv));
      if ( sname && need_dump() )
        fprintf(m_outfp, " %s", sname);
      auto ai = m_args.find(sv);
      if ( ai != m_args.end() ) {
        m_arg_no = ai->second.first;
        if ( need_dump() )
         fprintf(m_outfp, " Arg%d", m_arg_no);
      }
    }
    if ( need_dump() ) fputc(')', m_outfp);
  }
  if ( need_dump() )
    fprintf(m_outfp, " %s", name);
  // known types - pointer_type & reference_type
  if ( ct0 == REFERENCE_TYPE || ct0 == POINTER_TYPE )
  {
      if ( ct0 == REFERENCE_TYPE )
        t = TREE_TYPE(t);
      while( POINTER_TYPE_P(t))
        t = TREE_TYPE(t);
      if ( t == error_mark_node )
        return;
      ct0 = TREE_CODE(t);
      name = get_tree_code_name(ct0);
      if ( name )
      {
        if ( need_dump() )
        {
          fprintf(m_outfp, " ptr2 %s", name);
          fflush(m_outfp);
        }
        if ( RECORD_OR_UNION_TYPE_P(t) )
        {
          auto rt = TYPE_NAME(t);
          if ( rt && DECL_P(rt) )
          {
            if ( type_has_name(rt) )
            {
              clutch.last = t;
              if ( !clutch.left_name ) {
                clutch.left_name = IDENTIFIER_POINTER(DECL_NAME(rt));
                /// printf("left %s\n", clutch.left_name);
              }
              if ( need_dump() )
                fprintf(m_outfp, " SSAName %s", IDENTIFIER_POINTER(DECL_NAME(rt)) );
            } else {
              if ( need_dump() )
                fprintf(m_outfp, " tree_name_code %s uid %d", get_tree_code_name(TREE_CODE(rt)), TYPE_UID(t));
              clutch.last = t;
              dump_type_tree(t);
            }
          }
        } else if ( ct0 == METHOD_TYPE )
        {
          clutch.last = t;
          // don`t need type method when just assign in to register
          // anyway type and name will be gathered for second part of SET
          if ( !is_set_reg() )
            dump_method(t);
        } else if ( ct0 == FUNCTION_TYPE )
        {
          clutch.last = t;
          dump_ftype(t);  
        } else if ( !is_known_ssa_type(t) ) 
        {
          if ( need_dump() )
            fprintf(m_outfp, " UNKNOWN_SSA %d", ct0);
          if ( m_db )
            pass_error("UNKNOWN_SSA %d", ct0);
        }
      }
  }
}

void my_PLUGIN::dump_array_ref(const_tree expr, aux_type_clutch &clutch)
{
  auto op0 = TREE_OPERAND(expr, 0);
  auto op1 = TREE_OPERAND(expr, 1);
  auto code = TREE_CODE(op0);
  auto name = get_tree_code_name(code);
  if ( name && need_dump() )
    fprintf(m_outfp, " base0 %s", name);
  if ( code == COMPONENT_REF )
    dump_comp_ref(op0, clutch);
  code = TREE_CODE(op1);
  name = get_tree_code_name(code);
  if ( name && need_dump() )
    fprintf(m_outfp, " base1 %s", name);
  if ( code == SSA_NAME )
  {
    if ( need_dump() )
      fprintf(m_outfp, "(");  
    dump_ssa_name(op1, clutch);
    if ( need_dump() )
      fprintf(m_outfp, ")");
  } else if ( code != INTEGER_CST )
    claim_unknown(code, "arr_base1");
}

void my_PLUGIN::claim_unknown(tree_code code, const char *what)
{
  if ( need_dump() )
    fprintf(m_outfp, " unknown %s 0x%X", what, code);
  if ( m_db )
    pass_error("unknown %s 0x%X", what, code);
}

const char *my_PLUGIN::find_uid(unsigned int uid)
{
  std::pair<unsigned int, int> key{ uid, bb_index };
  auto iter = m_known_uids.find(key);
  if ( iter != m_known_uids.end() )
    return iter->second.c_str();
  while(1)
  {
    auto parent = m_blocks.find(key.second);
    if ( parent == m_blocks.end() )
      break;
    key.second = parent->second;
    iter = m_known_uids.find(key);
    if ( iter != m_known_uids.end() )
      return iter->second.c_str();
  }
  return nullptr;
}

int my_PLUGIN::add_fref_from_equal(int off)
{
  // if ( need_dump() ) fprintf(m_outfp, " add_fref");
  for ( tree f = TYPE_FIELDS(m_rbase); f; f = TREE_CHAIN(f) )
  {
    if ( TREE_CODE(f) != FIELD_DECL )
      continue;
    if ( DECL_VIRTUAL_P(f) )
      continue;
    if ( DECL_FIELD_OFFSET(f) && int_byte_position(f) == off )
    {  
      if ( DECL_NAME(f) )
      {
        if ( need_dump() )
          fprintf(m_outfp, " field_off %s", IDENTIFIER_POINTER(DECL_NAME(f)));
        if ( m_db ) {
          std::string fname = IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(m_rbase)));
          fname += ".";
          fname += IDENTIFIER_POINTER(DECL_NAME(f));
          m_db->add_xref(field, fname.c_str(), m_arg_no);
        }
        m_arg_no = 0;
        return 1;
      } else {
        tree t = TREE_TYPE(f);
        if ( need_dump() )
          fprintf(m_outfp, " no_name (%s)", get_tree_code_name(TREE_CODE(t)));
        if ( TREE_CODE(t) == RECORD_TYPE && TYPE_NAME(t)) {
          if ( need_dump() )
            fprintf(m_outfp, " record %s", IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(t))));
          if ( m_db ) {
            std::string fname = IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(m_rbase)));
            fname += "::";
            fname += IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(t)));
            m_db->add_xref(field, fname.c_str(), m_arg_no);
          }
          m_arg_no = 0;
          return 1;
        }
      }
      return 0;
    }
  }
  if ( need_dump() ) {
     fprintf(m_outfp, " not_found");
     dump_class_rec(TYPE_BINFO(m_rbase), TYPE_BINFO(m_rbase), 0);
  }
  return 0;
}

void my_PLUGIN::dump_mem_ref(const_tree expr, aux_type_clutch &clutch)
{
  const_tree base = nullptr, off = nullptr;
  if ( TREE_CODE(expr) == TARGET_MEM_REF )
  {
    base = TMR_BASE(expr);
    off = TMR_OFFSET(expr);
  } else { // see comment about TARGET_MEM_REF in tree.h
    base = TREE_OPERAND(expr, 0);
    off = TREE_OPERAND(expr, 1);
  }
  if ( base )
  {
    auto code = TREE_CODE(base);
    auto name = get_tree_code_name(code);
    if ( name && need_dump() )
      fprintf(m_outfp, " base %s", name);
    if ( code == SSA_NAME )
    {
      if ( need_dump() )
        fprintf(m_outfp, "(");  
      dump_ssa_name(base, clutch);
      if ( need_dump() )
        fprintf(m_outfp, ")");
      // check if we seen uid from dump_ssa_name
      if ( !clutch.completed && clutch.last )
      {
        auto uid = TYPE_UID(clutch.last);
        auto uid_name = find_uid(uid);
        if ( uid_name )
        {
          if ( need_dump() )
            fprintf(m_outfp, " uid %x: %s", uid, uid_name);
          clutch.completed = true;
          clutch.txt = uid_name;
          xref_kind ref_ = field;
          if ( is_call() )
            ref_ = xcall;
          if ( m_db )
            m_db->add_xref(ref_, clutch.txt.c_str());
          if ( ref_ == field ) report_fref("dump_mem_ref");
        }
      }
      // probably we can use TMR_OFFSET
      if ( !clutch.completed && clutch.last && RECORD_OR_UNION_TYPE_P(clutch.last) && off )
      {
        // what to do if we have both TMR_OFFSET and MEM_OFFSET from rtx?
        // should we sum them or ignore one ?
        auto code = TREE_CODE(off);
        if ( code == INTEGER_CST && tree_fits_shwi_p(off) )
        {
          int tmr_off = tree_to_shwi(off);
          if ( tmr_off )
          {
            clutch.off = tmr_off;
            clutch.has_off = true;
          }
        }
      }
      // case when ssa_name return record/union and clutch.off is non-zero, like
      // mem_ref base ssa_name( pointer_type ptr2 record_type SSAName abstract) off integer_cst 0 +8
      if ( !clutch.completed && clutch.has_off && clutch.last && RECORD_OR_UNION_TYPE_P(clutch.last) )
      {
        tree found = NULL_TREE;
        // lets try find field member with offset clutch.off
        for ( tree f = TYPE_FIELDS(clutch.last); f; f = TREE_CHAIN(f) )
        {
          if ( TREE_CODE(f) != FIELD_DECL )
            continue;
          if ( DECL_VIRTUAL_P(f) )
            continue;
          if ( DECL_FIELD_OFFSET(f) && int_byte_position(f) == clutch.off )
          {  
            found = f;
            if ( DECL_NAME(f) && need_dump() )
              fprintf(m_outfp, " field_off %s", IDENTIFIER_POINTER(DECL_NAME(f)));
            break;
          }
        }
        if ( TYPE_NAME(clutch.last) && found && DECL_NAME(found) )
        {
          clutch.completed = true;
          if ( type_has_name(TYPE_NAME(clutch.last)) )
          {
            clutch.txt = IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(clutch.last)));
            clutch.txt += ".";
            clutch.txt += IDENTIFIER_POINTER(DECL_NAME(found));
          } else {
            clutch.txt = IDENTIFIER_POINTER(DECL_NAME(found));
            dump_type_tree(clutch.last);
          }
          if ( m_db )
            m_db->add_xref(field, clutch.txt.c_str());
          report_fref("dump_mem_ref clutch");
        }
      }
    } else if ( code == ADDR_EXPR )
       dump_addr_expr(base, clutch);
    else if ( code == OBJ_TYPE_REF )
    {
      auto obj = OBJ_TYPE_REF_OBJECT(base);
      auto token = OBJ_TYPE_REF_TOKEN(base);
      auto ref_expr = OBJ_TYPE_REF_EXPR(base);
      auto base_class = obj_type_ref_class(base);
      if ( base_class )
      {
        code = TREE_CODE(base_class);
        name = get_tree_code_name(code);
        if ( name && need_dump() )
          fprintf(m_outfp, " class %s", name);
        if ( RECORD_OR_UNION_TYPE_P(base_class) )
        {
          if ( type_has_name(TYPE_NAME(base_class)) && need_dump() )
            fprintf(m_outfp, " %s", IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(base_class))));
          clutch.last = base_class;
          HOST_WIDE_INT vindex = tree_to_uhwi(token);
          for ( tree f = TYPE_FIELDS(base_class); f; f = TREE_CHAIN(f) )
          {
            if ( !DECL_VIRTUAL_P(f) )
              continue;
            if ( TREE_CODE(f) != FUNCTION_DECL )
              continue;
            tree vi = DECL_VINDEX(f);
            if ( !vi || vi == error_mark_node )
              continue;
            // fprintf(m_outfp, "fname %s vindex %p code %X\n", IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(f))), vi, TREE_CODE(vi)); fflush(m_outfp);  
            if ( extract_vindex(f) == vindex )
            {  
              clutch.completed = true;
              if ( type_has_name(TYPE_NAME(base_class)) )
              {
                clutch.txt = IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(base_class)));
                clutch.txt += ".";
                clutch.txt += IDENTIFIER_POINTER(DECL_NAME(f));
              } else
                clutch.txt = IDENTIFIER_POINTER(DECL_NAME(f));
              if ( m_db )
                m_db->add_xref(vcall, clutch.txt.c_str());
              if ( need_dump() )
                fprintf(m_outfp, " vcall %s", clutch.txt.c_str());
              return;
            }
          }
        } else {
          claim_unknown(code, "otr_class");
        }
      }
      if ( token )
      {
        code = TREE_CODE(token);
        name = get_tree_code_name(code);
        if ( name && need_dump() )
          fprintf(m_outfp, " token %s", name);
        if ( code == INTEGER_CST && tree_fits_shwi_p(token) )
          fprintf(m_outfp, " " HOST_WIDE_INT_PRINT_DEC, tree_to_shwi(token));
      }
      if ( ref_expr )
      {
        code = TREE_CODE(ref_expr);
        name = get_tree_code_name(code);
        if ( name && need_dump() )
          fprintf(m_outfp, " ref_expr %s", name);
        if ( code == SSA_NAME )
          dump_ssa_name(ref_expr, clutch);
      }
      if ( obj )
      {
        code = TREE_CODE(obj);
        name = get_tree_code_name(code);
        if ( name && need_dump() )
          fprintf(m_outfp, " obj %s", name);
        if ( code == SSA_NAME )
          dump_ssa_name(base, clutch);
        else if ( code == ADDR_EXPR )
        {
          dump_addr_expr(obj, clutch);
        } else {
          claim_unknown(code, "obj_type_ref");
        }
      }
    }
  }
  if ( off && off != error_mark_node && need_dump() )
  {
    auto code = TREE_CODE(off);
    auto name = get_tree_code_name(code);
    if ( name )
    {
      if ( code == INTEGER_CST )
      {
        fprintf(m_outfp, " off");
        if ( tree_fits_shwi_p(off) )
          fprintf(m_outfp, " " HOST_WIDE_INT_PRINT_DEC, tree_to_shwi(off));
      } else
        fprintf(m_outfp, " off %s", name);
    }
  }
}

void my_PLUGIN::dump_section(const_tree expr)
{
  tree sec = lookup_attribute("section", DECL_ATTRIBUTES(expr));
  if ( !sec ) return;
  auto *sname = TREE_STRING_POINTER(TREE_VALUE(TREE_VALUE(sec)));
  if ( sname ) fprintf(m_outfp, "(Section %s) ", sname);
}

void my_PLUGIN::dump_addr_expr(const_tree expr, aux_type_clutch &clutch)
{
  auto obj = TREE_OPERAND(expr, 0);
  auto code = TREE_CODE(obj);
  auto name = get_tree_code_name(code);
  if ( name && need_dump() )
    fprintf(m_outfp, " addr_type %X %s", code, name);
  if ( code == COMPONENT_REF )
    dump_comp_ref(obj, clutch);
  else if ( code == MEM_REF )
    dump_mem_ref(obj, clutch);
  else if ( code == PARM_DECL )
  {
    tree name = DECL_NAME(obj);
    if ( name && need_dump() )
      fprintf(m_outfp, " parm_name %s", IDENTIFIER_POINTER(name));
  } else if ( code == VAR_DECL )
  {
    tree name = DECL_NAME(obj);
    if ( name && need_dump() )
      fprintf(m_outfp, " var_name %s", IDENTIFIER_POINTER(name));
  } else if ( code != RESULT_DECL )
    claim_unknown(code, "addr_type");
}

void my_PLUGIN::dump_mem_expr(const_tree expr, const_rtx in_rtx)
{
  if ( expr == NULL_TREE || !in_rtx )
    return;
  dump_exprs();  
  auto code = TREE_CODE(expr);
  auto name = get_tree_code_name(code);
  if ( name && need_dump() )
    fprintf(m_outfp, " %s", name);
  if ( need_dump() && is_var_loc() )
  {
    if ( code == PARM_DECL || code == VAR_DECL )
    {
      tree name = DECL_NAME(expr);
      if ( name )
        fprintf(m_outfp, " %s", IDENTIFIER_POINTER(name));
      else
        fprintf(m_outfp, " UID %d", DECL_UID(expr));
    }
  }
  if ( code == PARM_DECL )
  {
    auto ai = m_args.find(expr);
    if ( need_dump() ) {
      if ( ai != m_args.end() ) fprintf(m_outfp, " Arg%d", ai->second.first);
      else fprintf(m_outfp, " uid %d", DECL_UID(expr));
    }
    if ( ai != m_args.end() && m_requal ) {
      m_arg_no = ai->second.first;
      m_rbase = ai->second.second;
      if ( need_dump() ) printf(" rbase");
    }
    return;
  }
  aux_type_clutch clutch(in_rtx);  
  if ( code == MEM_REF )
  {
    dump_mem_ref(expr, clutch);
    return;
  }
  if ( code == ARRAY_REF )
  {
    dump_array_ref(expr, clutch);
    store_aux(clutch);
    return;
  }
  if ( code != COMPONENT_REF )
    return;
  dump_comp_ref(expr, clutch);
  store_aux(clutch);
}

/* lets assume that we have some record with lots of nested types and then somewhere in code access like
     field0.field1, ... .fieldN
   tree for this case will contain lots of COMPONENT_REF
level 0                                 comp_ref N
                                        /     \
level 1                  comp_ref N - 1       field N
                            /       \
level 2            comp_ref N - 2    field N - 1
                      /          \
                     /           field N - 2
                   ...
level N      comp_ref 0
             /      \
         type 0     field 0

   So we need type only in deepest level and this expression will be completed at level 0
   As you my notice at bottom level aux_type_clutch.txt is empty
*/
struct comp_level
{
  comp_level(aux_type_clutch &clutch)
   : m_clutch(clutch)
  {
    m_clutch.level++;
  }
  ~comp_level()
  {
    m_clutch.level--;
  }
  bool is_top() const
  {
     return !(m_clutch.level - 1);
  }
  aux_type_clutch &m_clutch; 
};

void my_PLUGIN::dump_comp_ref(const_tree expr, aux_type_clutch &clutch)
{
  comp_level lvl(clutch);
  auto op0 = TREE_OPERAND (expr, 0);
  if ( !op0 )
    return;
  char *str = ".";
  if ( need_deref_compref0(op0) )
  {
    op0 = TREE_OPERAND (op0, 0);
    str = "->";
  }
  auto op1 = TREE_OPERAND (expr, 1);  
  auto code = TREE_CODE(op0);
  name = get_tree_code_name(code);
  if ( name )
  {
    if ( need_dump() )
      fprintf(m_outfp, " (l%s%s", str, name);
    if ( SSA_NAME == code )
    {
      dump_ssa_name(op0, clutch);
    } else if ( COMPONENT_REF == code )
    {
       dump_comp_ref(op0, clutch);
    } else if ( code == MEM_REF )
    {
      dump_mem_ref(op0, clutch);
    } else if ( code == ARRAY_REF )
    {
      dump_array_ref(op0, clutch);
    } else if ( code == PARM_DECL )
    {
      auto ai = m_args.find(op0);
      if ( need_dump() )
      {
        if ( ai != m_args.end() ) fprintf(m_outfp, " Arg%d", ai->second);
        else fprintf(m_outfp, " uid %d", DECL_UID(op0));
      }
      if ( ai != m_args.end() ) m_arg_no = ai->second.first;
    }
    if ( need_dump() )
      fprintf(m_outfp, ")");
    if ( code == VAR_DECL )
    {
      clutch.is_lvar = true;
      return;
    }
  }
  if ( !op1 )
    return;    
  code = TREE_CODE(op1);
  name = get_tree_code_name(code);
  if ( name && need_dump() )
    fprintf(m_outfp, " r:%s", name);
  if ( code != FIELD_DECL )
    return;
  // dump field name
  auto field_name = DECL_NAME(op1);
  if ( field_name && need_dump() )
    fprintf(m_outfp, " FName %s", IDENTIFIER_POINTER(field_name));
  auto ctx = DECL_CONTEXT(op1);
  if ( !ctx )
    return;
  code = TREE_CODE(ctx);
  name = get_tree_code_name(code);
  if ( name && need_dump() )
    fprintf(m_outfp, " %s", name);
  // record/union name
  auto tn = TYPE_NAME(ctx);
  const char* type_name = NULL;
  if ( tn && type_has_name(tn) )
  {
    type_name = IDENTIFIER_POINTER(DECL_NAME(tn));
    if ( need_dump() )
      fprintf(m_outfp, " Name %s", type_name );
    clutch.last = op1; // store field
  } else if (tn) {
    if ( need_dump() )
      fprintf(m_outfp, " tree_nameless_code %s uid %d", get_tree_code_name(TREE_CODE(tn)), TYPE_UID(ctx));
    clutch.last = ctx;
    try_nameless(tn, clutch);
  }
  int was_left = clutch.left_name != nullptr;
  if ( field_name )
  {
    if ( was_left && !tn ) {
      clutch.txt = clutch.left_name;
      clutch.left_name = nullptr;
    }
    // do we at deepest level?
    if ( clutch.txt.empty() )
    {
      if ( NULL == type_name )
      {
        clutch.txt = IDENTIFIER_POINTER(field_name);
      } else {
        clutch.txt = type_name;
        clutch.txt += ".";
        clutch.txt += IDENTIFIER_POINTER(field_name);      
      }
    } else {
      clutch.txt += ".";
      clutch.txt += IDENTIFIER_POINTER(field_name);
    }
  }
  if ( !tn && !was_left )
  {
    if ( need_dump() )
    {
      if ( clutch.last && clutch.last != NULL_TREE )
        fprintf(m_outfp, " no type_name(%s)", get_tree_code_name(TREE_CODE(clutch.last)) );
      else
        fprintf(m_outfp, " no type_name");
      dump_type_tree(ctx);
    }
    if ( m_db )
      pass_error("dump_comp_ref: no type_name for ctx %X", code);
  }
//  if ( !clutch.txt.empty() )
//    fprintf(m_outfp, "dump_comp_ref level %d %s", clutch.level, clutch.txt.c_str());
  if ( lvl.is_top() && !clutch.txt.empty() )
  {
    clutch.completed = true;
    if ( m_db )
      m_db->add_xref(field, clutch.txt.c_str(), m_arg_no);
    m_arg_no = 0;
    report_fref("dump_comp_ref");
  }
}

// based on print-rtl.cc method print_rtx
void my_PLUGIN::dump_rtx(const_rtx in_rtx, int level)
{
  int idx = 0;
  if ( !in_rtx )
    return;
  rtx_code code = GET_CODE(in_rtx);
  if ( code > NUM_RTX_CODE )
    return;
  int limit = GET_RTX_LENGTH(code);
  if ( code == VAR_LOCATION )
  {
    if ( TREE_CODE (PAT_VAR_LOCATION_DECL (in_rtx)) != STRING_CST )
      idx = GET_RTX_LENGTH (VAR_LOCATION);
  }
  if ( CONST_DOUBLE_AS_FLOAT_P(in_rtx) )
    idx = 5;

  const char *format_ptr = GET_RTX_FORMAT(code);
  // check expr_list with REG_EQUAL
  if ( code == EXPR_LIST )
  {
    int mod = (int)GET_MODE(in_rtx);
    m_requal = mod == REG_EQUAL;
  }
  if ( need_dump() )
  {
    if ( INSN_CHAIN_CODE_P(code) )
      fprintf(m_outfp, "%d ", INSN_UID (in_rtx));
    fprintf(m_outfp, "%s", GET_RTX_NAME(code));
    // print reg_note
    if ( code == EXPR_LIST )
    {
      int mod = (int)GET_MODE(in_rtx);
      if ( mod < REG_NOTE_MAX )
        fprintf(m_outfp, ":%s", GET_REG_NOTE_NAME(mod));
    }
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

    fprintf(m_outfp, " %d lim %d %s", idx, limit, format_ptr);
  }

  if ( code == VAR_LOCATION )
  {
    if ( TREE_CODE (PAT_VAR_LOCATION_DECL (in_rtx)) != STRING_CST )
    {
      if ( need_dump() )
        fprintf(m_outfp, " VMEM");
      dump_mem_expr(PAT_VAR_LOCATION_DECL(in_rtx), in_rtx);
      auto loc = PAT_VAR_LOCATION_LOC(in_rtx);
      if ( loc )
      {
        expr_push(loc, 0);
        if ( need_dump() )
        {
          fprintf(m_outfp, "\n");
          margin(level);
        }
        dump_rtx (loc, level + 1);
        expr_pop();
      } else if ( need_dump() )
        fprintf(m_outfp, "\n");
      return;
    }
  } else if ( code == MEM )
  {
    if ( MEM_EXPR(in_rtx) )
    {
      // process next level first. this expr already placed in stack in dump_e_operand
      if ( need_dump() ) fputs("\n", m_outfp);
      // save old m_arg_no
      auto old_arg_no = m_arg_no;
      for (; idx < limit; idx++)
        dump_rtx_operand (in_rtx, format_ptr[idx], idx, level + 1);
      if ( need_dump() ) {
        margin(level);
        fprintf(m_outfp, " MEM");
      }
      dump_mem_expr(MEM_EXPR (in_rtx), in_rtx);
      // restore m_arg_no
      if ( !m_requal )
        m_arg_no = old_arg_no;
      if ( need_dump() ) fputs("\n", m_outfp);
      return;
    }
    if ( MEM_OFFSET_KNOWN_P(in_rtx) && need_dump() )
    {
      fprintf(m_outfp, " +");
      print_poly_int (m_outfp, MEM_OFFSET(in_rtx));
    }
  } else if ( code == CONST_WIDE_INT && need_dump() )
  {
    fprintf(m_outfp, " WIDE_INT ");
    cwi_output_hex(m_outfp, in_rtx);
  } else if ( code == CONST_DOUBLE && FLOAT_MODE_P (GET_MODE (in_rtx)))
  {
    char s[60];
    real_to_decimal (s, CONST_DOUBLE_REAL_VALUE (in_rtx), sizeof (s), 0, 1);
    if ( need_dump() )
    {
      fprintf (m_outfp, " CONST_DOUBLE %s", s);
//      real_to_hexadecimal (s, CONST_DOUBLE_REAL_VALUE (in_rtx), sizeof (s), 0, 1);
//      fprintf (m_outfp, " [%s]", s);
    }
    if ( m_db )
      m_db->add_xref(fconst, s);
  }

  if ( need_dump() )
    fputs("\n", m_outfp);
  // dump operands
  for (; idx < limit; idx++)
    dump_rtx_operand (in_rtx, format_ptr[idx], idx, level + 1);  
}

void my_PLUGIN::dump_func_tree(const_tree t, int level)
{
  if ( !t )
    return;
  margin(level);
  auto code = TREE_CODE(t);
  auto name = get_tree_code_name(code);
  if ( name )
  {
    if ( code == BLOCK )
      fprintf(m_outfp, "%s %d", name, BLOCK_NUMBER(t));
    else {
      if ( DECL_P(t) )
        fprintf(m_outfp, "%s %d", name, DECL_UID(t));
      else
        fprintf(m_outfp, "%s", name); 
    }
  }
  if ( code == BLOCK )
  {
    fprintf(m_outfp, "\n");
    // code ripped from decls_for_scope
    // for ( ; t != NULL; t = BLOCK_CHAIN(t) )
    {
      for ( auto decl = BLOCK_VARS(t); decl != NULL; decl = DECL_CHAIN(decl) )
        dump_func_tree(decl, level + 1);
      for ( int i = 0; i < BLOCK_NUM_NONLOCALIZED_VARS(t); i++ )
      {
        auto d = BLOCK_NONLOCALIZED_VAR(t, i);
        if ( d == current_function_decl )
          continue;
        dump_func_tree(d, level + 1);
      }
      tree subblock;
      for ( subblock = BLOCK_SUBBLOCKS(t); subblock != NULL_TREE; subblock = BLOCK_CHAIN(subblock) )
         dump_func_tree(subblock, level + 1);
    }
  } else if ( VAR_P(t) && DECL_HAS_DEBUG_EXPR_P(t) )
  {
    auto deb = DECL_DEBUG_EXPR(CONST_CAST_TREE(t));
    if ( deb )
    {
      code = TREE_CODE(deb);
      auto name = get_tree_code_name(code);
      fprintf(m_outfp, "DEBUG_EXPR %s", name);
    }
  } else if ( code == LABEL_DECL )
  {
    rtx r = DECL_RTL_IF_SET( CONST_CAST_TREE(t) );
    if ( r )
      fprintf(m_outfp, " %d %d ", LABEL_DECL_UID(t), INSN_UID( r ));
    else
      fprintf(m_outfp, " %d ", LABEL_DECL_UID(t) );
    
    fprintf(m_outfp, "\n");
  } else
    fprintf(m_outfp, "\n");
}

void st_labels::rec_check_labels(const_tree t)
{
  auto code = TREE_CODE(t);
  if ( code == BLOCK )
  {
    // code ripped from decls_for_scope
    // for ( ; t != NULL; t = BLOCK_CHAIN(t) )
    {
      for ( auto decl = BLOCK_VARS(t); decl != NULL; decl = DECL_CHAIN(decl) )
        rec_check_labels(decl);
      for ( int i = 0; i < BLOCK_NUM_NONLOCALIZED_VARS(t); i++ )
      {
        auto d = BLOCK_NONLOCALIZED_VAR(t, i);
        if ( d == current_function_decl )
          continue;
        rec_check_labels(d);
      }
      tree subblock;
      for ( subblock = BLOCK_SUBBLOCKS(t); subblock != NULL_TREE; subblock = BLOCK_CHAIN(subblock) )
         rec_check_labels(subblock);
    }
  } else if ( code == LABEL_DECL )
  {
    rtx r = DECL_RTL_IF_SET( CONST_CAST_TREE(t) );
    if ( r )
    {
      int r_id = INSN_UID(r);
      auto f = m_st.find(r_id);
      if ( f != m_st.end() )
        f->second.second = 1;
    }
  }
}

unsigned int st_labels::execute(function *fun)
{
  m_st.clear();
  char* funName = (char*)IDENTIFIER_POINTER (DECL_NAME (current_function_decl) );
  tree fdecl = fun->decl;
  const char *aname = funName;
  if (DECL_ASSEMBLER_NAME_SET_P(fdecl) )
    aname = (IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME (fdecl)));
  m_funcname = aname;  
  // check if t
  rtx_insn* insn;
  for ( insn = get_insns(); insn; insn = NEXT_INSN(insn) )
  {
    if ( LABEL_P(insn) )
    {
      auto jt = jump_table_for_label((const rtx_code_label *)insn);
      if ( jt && JUMP_TABLE_DATA_P(jt) )
        m_st[INSN_UID(insn)] = { insn, 0 };
    }
  }
  if ( m_st.empty() )
    return 0;
  if ( need_dump() )
    fprintf(m_outfp, "st: %s has %ld tables\n", aname, m_st.size());
  // check labels
  tree f_block = DECL_INITIAL(current_function_decl);
  if ( !f_block )
    return 0;
  rec_check_labels(f_block);
  auto cnt = std::count_if(m_st.begin(), m_st.end(), [](const auto &c) { return 0 == c.second.second;});
  if ( !cnt )
    return 0;
  // find last var in f_block
  tree last_var = NULL_TREE;
  for ( auto decl = BLOCK_VARS(f_block); decl != NULL; decl = DECL_CHAIN(decl) )
  {
    if ( NULL_TREE == DECL_CHAIN(decl) )
      last_var = decl;
  }
  for ( auto &c: m_st )
  {
    if ( c.second.second )
      continue;
    // fprintf(stderr, "need add label for %d\n", c.first);
    // make label name
    std::string lname = m_funcname;
    lname += "_jt";
    lname += std::to_string(c.first);
    tree lt = build_decl(DECL_SOURCE_LOCATION(current_function_decl), LABEL_DECL, get_identifier(lname.c_str()), void_type_node);
    SET_DECL_RTL(lt, c.second.first);
    LABEL_DECL_UID(lt) = c.first;
    if ( last_var )
      DECL_CHAIN(last_var) = lt;
    else
      BLOCK_VARS(f_block) = lt;
    last_var = lt;  
  } 
  return 0;
}

void my_PLUGIN::fill_blocks(function *fun)
{
  m_blocks.clear();
  m_known_uids.clear();
  basic_block bb;
  FOR_ALL_BB_FN(bb, fun)
  {
    if ( bb->index == ENTRY_BLOCK || bb->index == EXIT_BLOCK )
      continue;
    if ( !single_pred_p(bb) )
      continue;
    auto idx = single_pred(bb)->index;
    // avoid dead-loops when the only pred block is the same block
    if ( idx == bb->index )
      continue;
    m_blocks[bb->index] = idx;
  }
}

unsigned int my_PLUGIN::execute(function *fun)
{
  // find the name of the function
  char* funName = (char*)IDENTIFIER_POINTER (DECL_NAME (current_function_decl) );
  tree fdecl = fun->decl;
  if ( m_db )
  {
    const char *aname = funName;
    if (DECL_ASSEMBLER_NAME_SET_P(fdecl) )
      aname = (IDENTIFIER_POINTER(DECL_ASSEMBLER_NAME (fdecl)));
    if ( !m_db->func_start(aname) )
      return 0;
  }
  if ( m_db )
  {
    pretty_printer pp;
    print_declaration(&pp, fdecl, 0, m_asmproto ? (TDF_MEMSYMS | TDF_ASMNAME) : TDF_MEMSYMS); 
    // dump_generic_node(&pp, fdecl, 0, m_asmproto ? (TDF_MEMSYMS | TDF_ASMNAME) : TDF_MEMSYMS, false);
    m_db->func_proto(pp_formatted_text(&pp));
  }
  if ( m_verbose )
  {
    const char *dname = lang_hooks.decl_printable_name (fdecl, 1);
    std::cerr << "execute on " << funName << " (" << dname << ") file " << main_input_filename << "\n";
  }
  rtx_reuse_manager r;
  rtx_writer w (m_outfp, 0, false, false, &r);
  if ( need_dump() )
  {
    dump_function_header(m_outfp, fun->decl, (dump_flags_t)0);
    // dump params
    for (tree arg = DECL_ARGUMENTS (fdecl); arg; arg = DECL_CHAIN (arg))
      print_param (m_outfp, w, arg);
  }
  // store args of functions
  m_args.clear();
  int idx = 1;
  for (tree arg = DECL_ARGUMENTS (fdecl); arg; ++idx, arg = DECL_CHAIN (arg))
  {
    auto a = DECL_RTL_IF_SET (arg);
    if ( a && REG_EXPR(a) )
    {
      auto rec_type = check_arg(REG_EXPR(a));
      if ( rec_type )
        m_args[REG_EXPR(a)] = { idx, rec_type };
    }
  }
  // try find table datas
  rtx_insn* insn;
  for ( insn = get_insns(); insn; insn = NEXT_INSN(insn) )
  {
    if ( JUMP_TABLE_DATA_P(insn) )
    {
      dump_rtx_hl(insn);
      if ( m_dump_rtl )  
        w.print_rtl_single_with_indent(insn, 0);
    }
  }

  // dump tree types
  if ( need_dump() )
  {
    fprintf(m_outfp, "; function decls\n");
    dump_func_tree(DECL_INITIAL(current_function_decl));
  }

  in_pe = 1; // wait for note with INSN_FUNCTION_BEG for first block
  fill_blocks(fun);
  basic_block bb;
  FOR_ALL_BB_FN(bb, fun)
  { // Loop over all Basic Blocks in the function, fun = current function
      bb_index = bb->index;
      if ( bb_index == 2 )
        in_pe = 1;
      if ( need_dump() )
      {
        fprintf(m_outfp,"BB: %d\n", bb->index);
        begin_any_block(m_outfp, bb);
      }
      if ( m_db )
        m_db->bb_start(bb_index);
      FOR_BB_INSNS(bb, insn)
      {
        // reset state
        m_arg_no = 0;
        m_requal = false;
        m_rbase = nullptr;
        if ( NONDEBUG_INSN_P(insn) || LABEL_P(insn) )
          dump_rtx_hl(insn);
        else if ( NOTE_P(insn) )
        {
          auto nk = NOTE_KIND(insn);
          if ( nk == NOTE_INSN_FUNCTION_BEG )
            in_pe = 0;
          else if ( nk == NOTE_INSN_EPILOGUE_BEG )
            in_pe = 1;
          else if ( nk == NOTE_INSN_EH_REGION_BEG ||
                    nk == NOTE_INSN_EH_REGION_END ||
                    nk == NOTE_INSN_VAR_LOCATION
                  )
            dump_rtx_hl(insn); 
        }
        if ( m_dump_rtl )  
          w.print_rtl_single_with_indent(insn, 0);
      }
      if ( need_dump() )
      {
        end_any_block (m_outfp, bb);
        fprintf(m_outfp,"\n----------------------------------------------------------------\n\n");
      }
      // prepare for processing of next block
      in_pe = 0;
      if ( m_db )
        m_db->bb_stop(bb_index);
  }
  dump_known_uids();
  if ( m_db )
    m_db->func_stop(); 
  return 0;
}

static void callback_start_unit(void *gcc_data, void *user_data)
{
  std::cerr << " *** A translation unit " << main_input_filename << " has been started\n";
  my_PLUGIN *mp = (my_PLUGIN *)user_data;
  if ( mp )
    mp->start_file(main_input_filename);
}

static void callback_finish_unit(void *gcc_data, void *user_data)
{
  std::cerr << " *** A translation unit has been finished\n";
  my_PLUGIN *mp = (my_PLUGIN *)user_data;
  if ( mp )
    mp->stop_file();
}

// We must assert that this plugin is GPL compatible
int plugin_is_GPL_compatible = 1;

int plugin_init (struct plugin_name_args *plugin_info, struct plugin_gcc_version *version)
{
    // We check the current gcc loading this plugin against the gcc we used to
    // created this plugin
    if (!plugin_default_version_check (version, &gcc_version))
    {
      std::cerr << "This GCC plugin is for version " << GCCPLUGIN_VERSION_MAJOR << "." << GCCPLUGIN_VERSION_MINOR << "\n";
	    return 1;
    }
    int i;
    int verbose = 0;
    for (i = 0; i < plugin_info->argc; i++)
    {
      if ( !strcmp(plugin_info->argv[i].key, pname_verbose) )
      {
        verbose = 1;
        break;
      }  
    }

    if ( verbose )
    {
      // Let's print all the information given to this plugin
      std::cerr << "Plugin info\n===========\n";
      std::cerr << "Base name: " << plugin_info->base_name << "\n";
      std::cerr << "Full name: " << plugin_info->full_name << "\n";
      std::cerr << "Number of arguments of this plugin:" << plugin_info->argc << "\n";

      for (i = 0; i < plugin_info->argc; i++)
      {
        std::cerr << "Argument " << i << ": Key: " << plugin_info->argv[i].key;
        if ( plugin_info->argv[i].value )
           std::cerr << ". Value: " << plugin_info->argv[i].value;
        std::cerr << "\n";
      }

      std::cerr << "\nVersion info\n============\n";
      std::cerr << "Base version: " << version->basever << "\n";
      std::cerr << "Date stamp: " << version->datestamp << "\n";
      std::cerr << "Dev phase: " << version->devphase << "\n";
      std::cerr << "Revision: " << version->devphase << "\n";
      std::cerr << "Configuration arguments: " << version->configuration_arguments << "\n\n";
    }

    struct register_pass_info pass;
    my_PLUGIN *mp = new my_PLUGIN(g, plugin_info->full_name, plugin_info->argv, plugin_info->argc);
    int conn_err = mp->connect();
    if ( conn_err )
    {
      delete mp;
      std::cerr << "connect failed, code " << conn_err << "\n";
      return 1;
    }
    pass.pass = mp;
    pass.reference_pass_name = "final"; // "dwarf2";
    pass.ref_pass_instance_number = 1;
    pass.pos_op = PASS_POS_INSERT_BEFORE;

    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass);

    struct register_pass_info pass2;
    pass2.pass = new st_labels(g, plugin_info->argv, plugin_info->argc);
    pass2.reference_pass_name = "dwarf2";
    pass2.ref_pass_instance_number = 1;
    pass2.pos_op = PASS_POS_INSERT_BEFORE;

    register_callback(plugin_info->base_name, PLUGIN_PASS_MANAGER_SETUP, NULL, &pass2);

    register_callback(plugin_info->base_name, PLUGIN_START_UNIT,
        callback_start_unit, /* user_data */ mp);

    register_callback(plugin_info->base_name, PLUGIN_FINISH_UNIT,
        callback_finish_unit, /* user_data */ mp);

    std::cerr << "Plugin " << plugin_info->base_name << " successfully initialized, pid " << getpid() << "\n";

    return 0;
}

// return 1 if some integer constant should be recorded
int my_PLUGIN::ic_filter()
{
  if ( ic_allowed.empty() && ic_denied.empty() )
    return 1;
  // first check ic_denied
  if ( !ic_denied.empty() )
  {
    for ( auto r = m_rtexpr.begin(); r != m_rtexpr.end(); ++r )
    {
      auto den = ic_denied.find(r->m_ce);
      if ( den != ic_denied.end() )
        return 0;
    }
  }
  if ( ic_allowed.empty() )
    return 1;
  for ( auto r = m_rtexpr.begin(); r != m_rtexpr.end(); ++r )  
  {
    auto den = ic_allowed.find(r->m_ce);
    if ( den != ic_allowed.end() )
      return 1;
  }
  return 0;
}

void my_PLUGIN::read_ic_config(const char *fname)
{
  std::ifstream ifs(fname);
  if ( !ifs.is_open() )
  {
    fprintf(stderr, "cannot open IC config file %s\n", fname);
    return;
  }
  struct cmpStrings {
    bool operator()(const char *a, const char *b) const {
      return strcasecmp(a, b) < 0;
    }
  };
  std::map<const char *, enum rtx_class, cmpStrings> rtx_names;
  for ( int i = 0; i < NUM_RTX_CODE; ++i )
    rtx_names[rtx_name[i]] = (enum rtx_class)i;
  // read file
  int line_no = 1;
  while( ifs )
  {
    std::string line;
    std::getline(ifs, line);
    line_no++;
    // trim spaces
    const char *s = line.c_str();
    while( *s )
    {
      if ( !isspace(*s) )
        break;
      ++s;
    }
    if ( !*s || *s == ';' ) // commants start with ;
      continue;
    auto *rset = &ic_allowed;
    if ( *s == '+' )
      ++s;
    else if ( *s == '-' )
    {
      ++s;
      rset = &ic_denied;
    }
    auto what = rtx_names.find(s);
    if ( what == rtx_names.end() )
    {
      fprintf(stderr, "bad keyword %s on line %d\n", s, line_no);
      continue;
    }
    rset->insert(what->second);
  }
}