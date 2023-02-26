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
   bool dump_type(uint64_t, std::string &);

};
