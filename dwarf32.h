#pragma once
#include <stdint.h>

namespace Dwarf32 {
  struct CompilationUnitHdr {
    uint32_t unit_length;
    uint16_t version;
    uint32_t debug_abbrev_offset;
    uint8_t address_size;
  } __attribute__((packed, aligned(1)));

  enum Tag {
    DW_TAG_padding = 0x00,
    DW_TAG_array_type = 0x01,
    DW_TAG_class_type = 0x02,
    DW_TAG_entry_point = 0x03,
    DW_TAG_enumeration_type = 0x04,
    DW_TAG_formal_parameter = 0x05,
    DW_TAG_imported_declaration = 0x08,
    DW_TAG_label = 0x0a,
    DW_TAG_lexical_block = 0x0b,
    DW_TAG_member = 0x0d,
    DW_TAG_pointer_type = 0x0f,
    DW_TAG_reference_type = 0x10,
    DW_TAG_compile_unit = 0x11,
    DW_TAG_string_type = 0x12,
    DW_TAG_structure_type = 0x13,
    DW_TAG_subroutine_type = 0x15,
    DW_TAG_typedef = 0x16,
    DW_TAG_union_type = 0x17,
    DW_TAG_unspecified_parameters = 0x18,
    DW_TAG_variant = 0x19,
    DW_TAG_common_block = 0x1a,
    DW_TAG_common_inclusion = 0x1b,
    DW_TAG_inheritance = 0x1c,
    DW_TAG_inlined_subroutine = 0x1d,
    DW_TAG_module = 0x1e,
    DW_TAG_ptr_to_member_type = 0x1f,
    DW_TAG_set_type = 0x20,
    DW_TAG_subrange_type = 0x21,
    DW_TAG_with_stmt = 0x22,
    DW_TAG_access_declaration = 0x23,
    DW_TAG_base_type = 0x24,
    DW_TAG_catch_block = 0x25,
    DW_TAG_const_type = 0x26,
    DW_TAG_constant = 0x27,
    DW_TAG_enumerator = 0x28,
    DW_TAG_file_type = 0x29,
    DW_TAG_friend = 0x2a,
    DW_TAG_namelist = 0x2b,
    DW_TAG_namelist_item = 0x2c,
    DW_TAG_packed_type = 0x2d,
    DW_TAG_subprogram = 0x2e,
    DW_TAG_template_type_param = 0x2f,
    DW_TAG_template_value_param = 0x30,
    DW_TAG_thrown_type = 0x31,
    DW_TAG_try_block = 0x32,
    DW_TAG_variant_part = 0x33,
    DW_TAG_variable = 0x34,
    DW_TAG_volatile_type = 0x35,

    // DWARF 3
    DW_TAG_dwarf_procedure = 0x36,
    DW_TAG_restrict_type = 0x37,
    DW_TAG_interface_type = 0x38,
    DW_TAG_namespace = 0x39,
    DW_TAG_imported_module = 0x3a,
    DW_TAG_unspecified_type = 0x3b,
    DW_TAG_partial_unit = 0x3c,
    DW_TAG_imported_unit = 0x3d,
    DW_TAG_condition = 0x3f,
    DW_TAG_shared_type = 0x40,

    // DWARF 4
    DW_TAG_type_unit = 0x41,
    DW_TAG_rvalue_reference_type = 0x42,
    DW_TAG_template_alias = 0x43,

    // DWARF 5
    DW_TAG_atomic_type = 0x47,

    DW_TAG_lo_user = 0x4080,
    DW_TAG_hi_user = 0xffff
  };

  enum Attribute {
    DW_AT_sibling = 0x01,
    DW_AT_location = 0x02,
    DW_AT_name = 0x03,
    DW_AT_ordering = 0x09,
    DW_AT_subscr_data = 0x0a,
    DW_AT_byte_size = 0x0b,
    DW_AT_bit_offset = 0x0c,
    DW_AT_bit_size = 0x0d,
    DW_AT_element_list = 0x0f,
    DW_AT_stmt_list = 0x10,
    DW_AT_low_pc = 0x11,
    DW_AT_high_pc = 0x12,
    DW_AT_language = 0x13,
    DW_AT_member = 0x14,
    DW_AT_discr = 0x15,
    DW_AT_discr_value = 0x16,
    DW_AT_visibility = 0x17,
    DW_AT_import = 0x18,
    DW_AT_string_length = 0x19,
    DW_AT_common_reference = 0x1a,
    DW_AT_comp_dir = 0x1b,
    DW_AT_const_value = 0x1c,
    DW_AT_containing_type = 0x1d,
    DW_AT_default_value = 0x1e,
    DW_AT_inline = 0x20,
    DW_AT_is_optional = 0x21,
    DW_AT_lower_bound = 0x22,
    DW_AT_producer = 0x25,
    DW_AT_prototyped = 0x27,
    DW_AT_return_addr = 0x2a,
    DW_AT_start_scope = 0x2c,
    DW_AT_bit_stride = 0x2e,
    DW_AT_upper_bound = 0x2f,
    DW_AT_abstract_origin = 0x31,
    DW_AT_accessibility = 0x32,
    DW_AT_address_class = 0x33,
    DW_AT_artificial = 0x34,
    DW_AT_base_types = 0x35,
    DW_AT_calling_convention = 0x36,
    DW_AT_count = 0x37,
    DW_AT_data_member_location = 0x38,
    DW_AT_decl_column = 0x39,
    DW_AT_decl_file = 0x3a,
    DW_AT_decl_line = 0x3b,
    DW_AT_declaration = 0x3c,
    DW_AT_discr_list = 0x3d,
    DW_AT_encoding = 0x3e,
    DW_AT_external = 0x3f,
    DW_AT_frame_base = 0x40,
    DW_AT_friend = 0x41,
    DW_AT_identifier_case = 0x42,
    DW_AT_macro_info = 0x43,
    DW_AT_namelist_items = 0x44,
    DW_AT_priority = 0x45,
    DW_AT_segment = 0x46,
    DW_AT_specification = 0x47,
    DW_AT_static_link = 0x48,
    DW_AT_type = 0x49,
    DW_AT_use_location = 0x4a,
    DW_AT_variable_parameter = 0x4b,
    DW_AT_virtuality = 0x4c,
    DW_AT_vtable_elem_location = 0x4d,

    // DWARF 3
    DW_AT_allocated = 0x4e,
    DW_AT_associated = 0x4f,
    DW_AT_data_location = 0x50,
    DW_AT_byte_stride = 0x51,
    DW_AT_entry_pc = 0x52,
    DW_AT_use_UTF8 = 0x53,
    DW_AT_extension = 0x54,
    DW_AT_ranges = 0x55,
    DW_AT_trampoline = 0x56,
    DW_AT_call_column = 0x57,
    DW_AT_call_file = 0x58,
    DW_AT_call_line = 0x59,
    DW_AT_description = 0x5a,
    DW_AT_binary_scale = 0x5b,
    DW_AT_decimal_scale = 0x5c,
    DW_AT_small = 0x5d,
    DW_AT_decimal_sign = 0x5e,
    DW_AT_digit_count = 0x5f,
    DW_AT_picture_string = 0x60,
    DW_AT_mutable = 0x61,
    DW_AT_threads_scaled = 0x62,
    DW_AT_explicit = 0x63,
    DW_AT_object_pointer = 0x64,
    DW_AT_endianity = 0x65,
    DW_AT_elemental = 0x66,
    DW_AT_pure = 0x67,
    DW_AT_recursive = 0x68,

    // DWARF 4
    DW_AT_signature = 0x69,
    DW_AT_main_subprogram = 0x6a,
    DW_AT_data_bit_offset = 0x6b,
    DW_AT_const_expr = 0x6c,
    DW_AT_enum_class = 0x6d,
    DW_AT_linkage_name = 0x6e,

    // DWARF 5
    DW_AT_noreturn = 0x87,
    DW_AT_alignment = 0x88,
    DW_AT_export_symbols = 0x89,
    DW_AT_deleted = 0x8a,
    DW_AT_defaulted = 0x8b,

    DW_AT_lo_user = 0x2000,
    DW_AT_hi_user = 0x3fff
  };

  enum Form {
    DW_FORM_addr = 0x01,    // address
    DW_FORM_block2 = 0x03,  // block
    DW_FORM_block4,         // block
    DW_FORM_data2,          // constant
    DW_FORM_data4,          // constant
    DW_FORM_data8,          // constant
    DW_FORM_string,         // string
    DW_FORM_block,          // block
    DW_FORM_block1,         // block
    DW_FORM_data1,          // constant
    DW_FORM_flag,           // flag
    DW_FORM_sdata,          // constant
    DW_FORM_strp,           // string
    DW_FORM_udata,          // constant
    DW_FORM_ref_addr,       // reference
    DW_FORM_ref1,           // reference
    DW_FORM_ref2,           // reference
    DW_FORM_ref4,           // reference
    DW_FORM_ref8,           // reference
    DW_FORM_ref_udata,      // reference
    DW_FORM_indirect,       // indirect

    // DWARF 4
    DW_FORM_sec_offset,     // lineptr...
    DW_FORM_exprloc,        // exprloc
    DW_FORM_flag_present,   // flag
    DW_FORM_ref_sig8 = 0x20,// reference
    // DWARF 5
    DW_FORM_strx = 0x1a,
    DW_FORM_addrx = 0x1b,
    DW_FORM_data16 = 0x1e,
    DW_FORM_implicit_const = 0x21,
    DW_FORM_strx1 = 0x25,
    DW_FORM_strx2 = 0x26,
    DW_FORM_strx3 = 0x27,
    DW_FORM_strx4 = 0x28,
  };

  enum Accessibility {
    DW_ACCESS_public = 0x01,
    DW_ACCESS_protected = 0x02,
    DW_ACCESS_private = 0x03
  };

  enum Virtuality {
    DW_VIRTUALITY_none = 0,
    DW_VIRTUALITY_virtual = 1,
    DW_VIRTUALITY_pure_virtual = 2
  };
/* Source language names and codes.  */
  enum dwarf_source_language {
    DW_LANG_C89 = 0x0001,
    DW_LANG_C = 0x0002,
    DW_LANG_Ada83 = 0x0003,
    DW_LANG_C_plus_plus = 0x0004,
    DW_LANG_Cobol74 = 0x0005,
    DW_LANG_Cobol85 = 0x0006,
    DW_LANG_Fortran77 = 0x0007,
    DW_LANG_Fortran90 = 0x0008,
    DW_LANG_Pascal83 = 0x0009,
    DW_LANG_Modula2 = 0x000a,
    /* DWARF 3.  */
    DW_LANG_Java = 0x000b,
    DW_LANG_C99 = 0x000c,
    DW_LANG_Ada95 = 0x000d,
    DW_LANG_Fortran95 = 0x000e,
    DW_LANG_PLI = 0x000f,
    DW_LANG_ObjC = 0x0010,
    DW_LANG_ObjC_plus_plus = 0x0011,
    DW_LANG_UPC = 0x0012,
    DW_LANG_D = 0x0013,
    /* DWARF 4.  */
    DW_LANG_Python = 0x0014,
    /* DWARF 5.  */
    DW_LANG_OpenCL = 0x0015,
    DW_LANG_Go = 0x0016,
    DW_LANG_Modula3 = 0x0017,
    DW_LANG_Haskell = 0x0018,
    DW_LANG_C_plus_plus_03 = 0x0019,
    DW_LANG_C_plus_plus_11 = 0x001a,
    DW_LANG_OCaml = 0x001b,
    DW_LANG_Rust = 0x001c,
    DW_LANG_C11 = 0x001d,
    DW_LANG_Swift = 0x001e,
    DW_LANG_Julia = 0x001f,
    DW_LANG_Dylan = 0x0020,
    DW_LANG_C_plus_plus_14 = 0x0021,
    DW_LANG_Fortran03 = 0x0022,
    DW_LANG_Fortran08 = 0x0023,
    DW_LANG_RenderScript = 0x0024,
    // from https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/BinaryFormat/Dwarf.def
    DW_LANG_BLISS = 0x25,
    DW_LANG_Kotlin = 0x26,
    DW_LANG_Zig = 0x27,
    DW_LANG_Crystal = 0x28,
    DW_LANG_C_plus_plus_17 = 0x2a,
    DW_LANG_C_plus_plus_20 = 0x2b,
    DW_LANG_C17 = 0x2c,
    DW_LANG_Fortran18 = 0x2d,
    DW_LANG_Ada2005 = 0x2e,
    DW_LANG_Ada2012 = 0x2f,

    DW_LANG_lo_user = 0x8000,	/* Implementation-defined range start.  */
    DW_LANG_hi_user = 0xffff,	/* Implementation-defined range start.  */

    /* MIPS.  */
    DW_LANG_Mips_Assembler = 0x8001,
    /* UPC.  */
    DW_LANG_Upc = 0x8765,
    /* HP extensions.  */
    DW_LANG_HP_Bliss     = 0x8003,
    DW_LANG_HP_Basic91   = 0x8004,
    DW_LANG_HP_Pascal91  = 0x8005,
    DW_LANG_HP_IMacro    = 0x8006,
    DW_LANG_HP_Assembler = 0x8007,
    DW_LANG_GOOGLE_RenderScript = 0x8e57,
    DW_LANG_BORLAND_Delphi = 0xb000,

    /* Rust extension, but replaced in DWARF 5.  */
    DW_LANG_Rust_old = 0x9000
  };

#define DW_OP(name, value)  name = value,
  /* ripped from dwarf2.def */
  enum dwarf_ops {
DW_OP (DW_OP_addr, 0x03)
DW_OP (DW_OP_deref, 0x06)
DW_OP (DW_OP_const1u, 0x08)
DW_OP (DW_OP_const1s, 0x09)
DW_OP (DW_OP_const2u, 0x0a)
DW_OP (DW_OP_const2s, 0x0b)
DW_OP (DW_OP_const4u, 0x0c)
DW_OP (DW_OP_const4s, 0x0d)
DW_OP (DW_OP_const8u, 0x0e)
DW_OP (DW_OP_const8s, 0x0f)
DW_OP (DW_OP_constu, 0x10)
DW_OP (DW_OP_consts, 0x11)
DW_OP (DW_OP_dup, 0x12)
DW_OP (DW_OP_drop, 0x13)
DW_OP (DW_OP_over, 0x14)
DW_OP (DW_OP_pick, 0x15)
DW_OP (DW_OP_swap, 0x16)
DW_OP (DW_OP_rot, 0x17)
DW_OP (DW_OP_xderef, 0x18)
DW_OP (DW_OP_abs, 0x19)
DW_OP (DW_OP_and, 0x1a)
DW_OP (DW_OP_div, 0x1b)
DW_OP (DW_OP_minus, 0x1c)
DW_OP (DW_OP_mod, 0x1d)
DW_OP (DW_OP_mul, 0x1e)
DW_OP (DW_OP_neg, 0x1f)
DW_OP (DW_OP_not, 0x20)
DW_OP (DW_OP_or, 0x21)
DW_OP (DW_OP_plus, 0x22)
DW_OP (DW_OP_plus_uconst, 0x23)
DW_OP (DW_OP_shl, 0x24)
DW_OP (DW_OP_shr, 0x25)
DW_OP (DW_OP_shra, 0x26)
DW_OP (DW_OP_xor, 0x27)
DW_OP (DW_OP_bra, 0x28)
DW_OP (DW_OP_eq, 0x29)
DW_OP (DW_OP_ge, 0x2a)
DW_OP (DW_OP_gt, 0x2b)
DW_OP (DW_OP_le, 0x2c)
DW_OP (DW_OP_lt, 0x2d)
DW_OP (DW_OP_ne, 0x2e)
DW_OP (DW_OP_skip, 0x2f)
DW_OP (DW_OP_lit0, 0x30)
DW_OP (DW_OP_lit1, 0x31)
DW_OP (DW_OP_lit2, 0x32)
DW_OP (DW_OP_lit3, 0x33)
DW_OP (DW_OP_lit4, 0x34)
DW_OP (DW_OP_lit5, 0x35)
DW_OP (DW_OP_lit6, 0x36)
DW_OP (DW_OP_lit7, 0x37)
DW_OP (DW_OP_lit8, 0x38)
DW_OP (DW_OP_lit9, 0x39)
DW_OP (DW_OP_lit10, 0x3a)
DW_OP (DW_OP_lit11, 0x3b)
DW_OP (DW_OP_lit12, 0x3c)
DW_OP (DW_OP_lit13, 0x3d)
DW_OP (DW_OP_lit14, 0x3e)
DW_OP (DW_OP_lit15, 0x3f)
DW_OP (DW_OP_lit16, 0x40)
DW_OP (DW_OP_lit17, 0x41)
DW_OP (DW_OP_lit18, 0x42)
DW_OP (DW_OP_lit19, 0x43)
DW_OP (DW_OP_lit20, 0x44)
DW_OP (DW_OP_lit21, 0x45)
DW_OP (DW_OP_lit22, 0x46)
DW_OP (DW_OP_lit23, 0x47)
DW_OP (DW_OP_lit24, 0x48)
DW_OP (DW_OP_lit25, 0x49)
DW_OP (DW_OP_lit26, 0x4a)
DW_OP (DW_OP_lit27, 0x4b)
DW_OP (DW_OP_lit28, 0x4c)
DW_OP (DW_OP_lit29, 0x4d)
DW_OP (DW_OP_lit30, 0x4e)
DW_OP (DW_OP_lit31, 0x4f)
DW_OP (DW_OP_reg0, 0x50)
DW_OP (DW_OP_reg1, 0x51)
DW_OP (DW_OP_reg2, 0x52)
DW_OP (DW_OP_reg3, 0x53)
DW_OP (DW_OP_reg4, 0x54)
DW_OP (DW_OP_reg5, 0x55)
DW_OP (DW_OP_reg6, 0x56)
DW_OP (DW_OP_reg7, 0x57)
DW_OP (DW_OP_reg8, 0x58)
DW_OP (DW_OP_reg9, 0x59)
DW_OP (DW_OP_reg10, 0x5a)
DW_OP (DW_OP_reg11, 0x5b)
DW_OP (DW_OP_reg12, 0x5c)
DW_OP (DW_OP_reg13, 0x5d)
DW_OP (DW_OP_reg14, 0x5e)
DW_OP (DW_OP_reg15, 0x5f)
DW_OP (DW_OP_reg16, 0x60)
DW_OP (DW_OP_reg17, 0x61)
DW_OP (DW_OP_reg18, 0x62)
DW_OP (DW_OP_reg19, 0x63)
DW_OP (DW_OP_reg20, 0x64)
DW_OP (DW_OP_reg21, 0x65)
DW_OP (DW_OP_reg22, 0x66)
DW_OP (DW_OP_reg23, 0x67)
DW_OP (DW_OP_reg24, 0x68)
DW_OP (DW_OP_reg25, 0x69)
DW_OP (DW_OP_reg26, 0x6a)
DW_OP (DW_OP_reg27, 0x6b)
DW_OP (DW_OP_reg28, 0x6c)
DW_OP (DW_OP_reg29, 0x6d)
DW_OP (DW_OP_reg30, 0x6e)
DW_OP (DW_OP_reg31, 0x6f)
DW_OP (DW_OP_breg0, 0x70)
DW_OP (DW_OP_breg1, 0x71)
DW_OP (DW_OP_breg2, 0x72)
DW_OP (DW_OP_breg3, 0x73)
DW_OP (DW_OP_breg4, 0x74)
DW_OP (DW_OP_breg5, 0x75)
DW_OP (DW_OP_breg6, 0x76)
DW_OP (DW_OP_breg7, 0x77)
DW_OP (DW_OP_breg8, 0x78)
DW_OP (DW_OP_breg9, 0x79)
DW_OP (DW_OP_breg10, 0x7a)
DW_OP (DW_OP_breg11, 0x7b)
DW_OP (DW_OP_breg12, 0x7c)
DW_OP (DW_OP_breg13, 0x7d)
DW_OP (DW_OP_breg14, 0x7e)
DW_OP (DW_OP_breg15, 0x7f)
DW_OP (DW_OP_breg16, 0x80)
DW_OP (DW_OP_breg17, 0x81)
DW_OP (DW_OP_breg18, 0x82)
DW_OP (DW_OP_breg19, 0x83)
DW_OP (DW_OP_breg20, 0x84)
DW_OP (DW_OP_breg21, 0x85)
DW_OP (DW_OP_breg22, 0x86)
DW_OP (DW_OP_breg23, 0x87)
DW_OP (DW_OP_breg24, 0x88)
DW_OP (DW_OP_breg25, 0x89)
DW_OP (DW_OP_breg26, 0x8a)
DW_OP (DW_OP_breg27, 0x8b)
DW_OP (DW_OP_breg28, 0x8c)
DW_OP (DW_OP_breg29, 0x8d)
DW_OP (DW_OP_breg30, 0x8e)
DW_OP (DW_OP_breg31, 0x8f)
DW_OP (DW_OP_regx, 0x90)
DW_OP (DW_OP_fbreg, 0x91)
DW_OP (DW_OP_bregx, 0x92)
DW_OP (DW_OP_piece, 0x93)
DW_OP (DW_OP_deref_size, 0x94)
DW_OP (DW_OP_xderef_size, 0x95)
DW_OP (DW_OP_nop, 0x96)
/* DWARF 3 extensions.  */
DW_OP (DW_OP_push_object_address, 0x97)
DW_OP (DW_OP_call2, 0x98)
DW_OP (DW_OP_call4, 0x99)
DW_OP (DW_OP_call_ref, 0x9a)
DW_OP (DW_OP_form_tls_address, 0x9b)
DW_OP (DW_OP_call_frame_cfa, 0x9c)
DW_OP (DW_OP_bit_piece, 0x9d)

/* DWARF 4 extensions.  */
DW_OP (DW_OP_implicit_value, 0x9e)
DW_OP (DW_OP_stack_value, 0x9f)
/* DWARF 5 extensions.  */
DW_OP (DW_OP_implicit_pointer, 0xa0)
DW_OP (DW_OP_addrx, 0xa1)
DW_OP (DW_OP_constx, 0xa2)
DW_OP (DW_OP_entry_value, 0xa3)
DW_OP (DW_OP_const_type, 0xa4)
DW_OP (DW_OP_regval_type, 0xa5)
DW_OP (DW_OP_deref_type, 0xa6)
DW_OP (DW_OP_xderef_type, 0xa7)
DW_OP (DW_OP_convert, 0xa8)
DW_OP (DW_OP_reinterpret, 0xa9)
/* GNU extensions.  */
DW_OP (DW_OP_GNU_push_tls_address, 0xe0)
DW_OP (DW_OP_GNU_implicit_pointer, 0xf2)
  };
};