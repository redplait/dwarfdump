#pragma once
#include <elfio/elfio.hpp>

struct RegNames
{
  virtual ~RegNames()
  {}
  virtual const char *reg_name(unsigned int) = 0;
};

RegNames *get_regnames(ELFIO::Elf_Half mac);