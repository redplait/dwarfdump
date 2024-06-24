#pragma once
#include "TreeBuilder.h"

class JsonRender: public TreeBuilder
{
  public:
    std::string GenerateJson();
  protected:
    virtual void RenderUnit(int last);
    void RenderGoAttrs(std::string &, uint64_t id);
    static std::string EscapeJsonString(const char* str);
    std::string GenerateJson(Element &);
    template <class T>
    std::string &put(std::string &, const char *, T);
    void render_location(std::string &, param_loc &);
};