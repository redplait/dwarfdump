#pragma once
#include <elfio/elfio.hpp>
#include <list>

struct LocListXItem;

struct IGetLoclistX
{
  virtual bool get_loclistx(uint64_t off, std::list<LocListXItem> &, uint64_t) = 0;
  virtual bool get_rnglistx(int64_t off, uint64_t base_addr, unsigned char addr_size,
   /* out param */ std::list<std::pair<uint64_t, uint64_t> > &) = 0;
  virtual bool find_dfa(uint64_t pc, uint64_t &res) = 0;
};

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