#pragma once

class bm_search
{
  public:
    bm_search(const unsigned char *pattern, size_t plen);
    bm_search();
   ~bm_search();
    int set(const unsigned char *pattern, size_t plen);
    const unsigned char *search(const unsigned char *mem, size_t mlen);
  protected:
    int make(const unsigned char *pattern, size_t plen);
    long occ[0x100];
    const unsigned char *m_pattern;
    size_t m_plen;
    size_t *m_skip;
};
