#pragma once
#include "TreeBuilder.h"

class JsonRender: public TreeBuilder
{
  public:
    std::string GenerateJson();
  protected:
    virtual void RenderUnit(int last);
    static std::string EscapeJsonString(const char* str);
    std::string GenerateJson(Element &);
};