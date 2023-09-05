#pragma once
#include <elfio/elfio.hpp>

struct ISectionNames
{
  virtual const char *find_sname(uint64_t) = 0;
};

struct RegNames
{
  virtual ~RegNames()
  {}
  virtual const char *reg_name(unsigned int) = 0;
  virtual const char *get_addr_type(unsigned int)
  {
    return nullptr;
  }
};

RegNames *get_regnames(ELFIO::Elf_Half mac);