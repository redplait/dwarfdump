#include <cstddef>

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
 "Array",
 "Chan",
 "Func",
 "Interface",
 "Map",
 "Ptr",
 "Slice",
 "String",
 "Struct",
 "UnsafePointer",
};

const char *get_go_kind(int k)
{
  if ( k < 0 || (size_t)k >= (ARRAY_SIZE(kind_names)) )
    return nullptr;
  return kind_names[k];
}