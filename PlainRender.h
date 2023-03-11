#pragma once
#include "TreeBuilder.h"

class PlainRender: public TreeBuilder
{
  public:
    PlainRender(): TreeBuilder()
    {}
    virtual ~PlainRender()
    {}
  protected:
   std::map<uint64_t, Element *> m_els;
   
   virtual void RenderUnit(int last);
   void dump_types();
   void dump_enums(Element *);
   void dump_fields(Element *);
   void dump_func(Element *);
   void dump_methods(Element *e);
   void dump_method(Method *e, std::string &res);
   std::string &render_one_enum(std::string &s, EnumItem &en);
   bool dump_type(uint64_t, std::string &, int level = 0);

};
