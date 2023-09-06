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
    PlainRender(): TreeBuilder()
    {}
    virtual ~PlainRender()
    {}
  protected:
   std::map<uint64_t, Element *> m_els;
   std::map<uint64_t, std::list<Element *> > m_specs;
   std::list<Element *> m_vars;
   std::list<std::pair<struct cu, std::list<Element> > > m_all;

   virtual void RenderUnit(int last);
   void prepare(std::list<Element> &els);
   std::list<Element *> *get_specs(uint64_t);
   void dump_types(std::list<Element> &els, struct cu *);
   void dump_vars();
   void cmn_vars();
   void dump_var(Element *);
   void dump_enums(Element *);
   void dump_fields(Element *);
   void dump_func(Element *);
   void dump_methods(Element *e);
   void dump_method(Method *e, const Element *owner, std::string &res);
   void dump_spec(Element *e);
   void dump_complex_type(Element &e);
   int dump_parents(Element &e);
   std::string &render_one_enum(std::string &s, EnumItem &en);
   std::string &render_field(Element *e, std::string &s, int level);
   std::string &render_fields(Element *e, std::string &s, int level);
   std::string &render_params(IN Element *e, uint64_t this_arg, OUT std::string &s);
   bool dump_params_locations(std::vector<FormalParam> &, std::string &, int level = 0);
   bool dump_type(uint64_t, std::string &, named *, int level = 0);
   bool is_constructor(const Element *e, const Element *owner) const;
   bool need_add_var(const Element &e) const;
   bool add_var(Element &e);
};
