#pragma once
#include "TreeBuilder.h"
#include "debug.h"

struct named
{
  named()
   : name_(nullptr),
     used_(false)
   {}
  named(const char *n)
   : name_(n),
     used_(false),
     no_ptr_(false)
   {}
  const char *name() const
  {
    if ( used_ )
      return nullptr;
    return name_;
  }
  const char *name_;
  bool used_;
  bool no_ptr_;
};

class PlainRender: public TreeBuilder
{
  public:
    PlainRender(ErrLog *e): TreeBuilder(e)
    {}
    virtual ~PlainRender()
    {}
  protected:
   std::unordered_map<uint64_t, Element *> m_els;
   std::unordered_map<uint64_t, std::list<Element *> > m_specs;
   std::vector<Element *> m_vars;
   std::list<std::pair<struct cu, std::list<Element> > > m_all;

   virtual void RenderUnit(int last);
   virtual bool conv2str(uint64_t key, std::string &);
   void prepare(std::list<Element> &els);
   std::list<Element *> *get_specs(uint64_t);
   void dump_types(std::list<Element> &els, struct cu *);
   void dump_vars();
   void dump_one_var(Element *, int local);
   void cmn_vars();
   void dump_const_expr(Element *);
   void dump_var(Element *, int local);
   void dump_enums(Element *);
   void dump_fields(Element *);
   void dump_func(Element *);
   void dump_lvars(Element *e);
   void dump_methods(Element *e);
   void dump_method(Method *e, const Element *owner, std::string &res);
   void dump_spec(Element *e);
   void dump_complex_type(Element &e);
   int dump_parents(Element &e);
   int form_var_fullname(Element *e, std::string &res);
   // rustc often puts abstract_origin for vars inside inlined subs for vars in frames somewhere above
   Element *try_find_in_frames(Element *e);
   std::string &render_one_enum(std::string &s, EnumItem &en, bool);
   std::string &render_field(Element *e, std::string &s, int level, int off = 0);
   std::string &render_fields(Element *e, std::string &s, int level, int off = 0);
   std::string &render_params(IN Element *e, uint64_t this_arg, OUT std::string &s);
   bool dump_params_locations(std::vector<FormalParam> &, std::string &, int level = 0);
   bool dump_type(uint64_t, std::string &, named *, int level = 0, int off = 0);
   bool is_constructor(const Element *e, const Element *owner) const;
   bool need_add_var(const Element &e) const;
   bool add_var(Element &e);
   uint64_t m_locsx = 0;
   uint64_t m_adj_locsx = 0;
   uint64_t m_locx_els = 0;
   uint64_t m_locx_red_els = 0;
};
