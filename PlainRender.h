#pragma once
#include "TreeBuilder.h"

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
   
   virtual void RenderUnit(int last);
   std::list<Element *> *get_specs(uint64_t);
   void dump_types();
   void dump_enums(Element *);
   void dump_fields(Element *);
   void dump_func(Element *);
   void dump_methods(Element *e);
   void dump_method(Method *e, const Element *owner, std::string &res);
   void dump_spec(Element *e);
   std::string &render_one_enum(std::string &s, EnumItem &en);
   std::string &render_field(Element *e, std::string &s, int level);
   std::string &render_fields(Element *e, std::string &s, int level);
   std::string &render_params(Element *e, uint64_t this_arg, std::string &s);
   bool dump_type(uint64_t, std::string &, named *, int level = 0);
   bool is_constructor(const Element *e, const Element *owner) const;
};
