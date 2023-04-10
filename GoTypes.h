#pragma once

// see https://github.com/golang/go/blob/master/src/cmd/internal/dwarf/dwarf.go#L312
struct go_ext_attr
{
  int kind = 0;       // value of DW_AT_go_kind
  uint64_t key = 0;   // value of DW_AT_go_key
  uint64_t elem = 0;  // DW_AT_go_elem - tag id
  const void *rt_type = nullptr; // DW_AT_go_runtime_type
  int emb_field = 0;  // value of DW_AT_go_embedded_field - DW_FORM_flag
  int dict_index = 0; // value of DW_AT_go_dict_index
};

const char *get_go_kind(int);
