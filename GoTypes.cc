#include <cstddef>
#include "TreeBuilder.h"

#define ARRAY_SIZE(a) (sizeof (a) / sizeof ((a)[0]))

// https://go.dev/src/runtime/typekind.go
static const char *const kind_names[] = {
 nullptr,
 "Bool", // 1
 "Int",
 "Int8",
 "Int16",
 "Int32",
 "Int64",
 "Uint",
 "Uint8",
 "Uint16",
 "Uint32",
 "Uint64",
 "Uintptr",
 "Float32",
 "Float64",
 "Complex64",
 "Complex128",
 "Array", // 17
 "Chan",
 "Func", // 19
 "Interface",
 "Map",
 "Ptr", // 22
 "Slice",
 "String",
 "Struct", // 25
 "UnsafePointer",
};

const char *get_go_kind(int k)
{
  if ( k < 0 || (size_t)k >= (ARRAY_SIZE(kind_names)) )
    return nullptr;
  return kind_names[k];
}

void TreeBuilder::SetGoKind(uint64_t id, int v)
{
  if ( !mark_has_go(id) ) return;
  auto giter = m_go_attrs.find(id);
  if ( giter == m_go_attrs.end() )
  {
    go_ext_attr ext;
    ext.kind = v;
    m_go_attrs[id] = ext;
  } else
    giter->second.kind = v;
}

void TreeBuilder::SetGoKey(uint64_t id, uint64_t v)
{
  if ( !mark_has_go(id) ) return;
  auto giter = m_go_attrs.find(id);
  if ( giter == m_go_attrs.end() )
  {
    go_ext_attr ext;
    ext.key = v;
    m_go_attrs[id] = ext;
  } else
    giter->second.key = v;
}

void TreeBuilder::SetGoDictIndex(uint64_t id, int v)
{
  if ( !mark_has_go(id) ) return;
  auto giter = m_go_attrs.find(id);
  if ( giter == m_go_attrs.end() )
  {
    go_ext_attr ext;
    ext.dict_index = v;
    m_go_attrs[id] = ext;
  } else
    giter->second.dict_index = v;
}

void TreeBuilder::SetGoElem(uint64_t id, uint64_t v)
{
  if ( !mark_has_go(id) ) return;
  auto giter = m_go_attrs.find(id);
  if ( giter == m_go_attrs.end() )
  {
    go_ext_attr ext;
    ext.elem = v;
    m_go_attrs[id] = ext;
  } else
    giter->second.elem = v;
}

void TreeBuilder::SetGoRType(uint64_t id, const void *v)
{
  if ( !mark_has_go(id) ) return;
  auto giter = m_go_attrs.find(id);
  if ( giter == m_go_attrs.end() )
  {
    go_ext_attr ext;
    ext.rt_type = v;
    m_go_attrs[id] = ext;
  } else
    giter->second.rt_type = v;
}