#include <stdlib.h>
#include <string.h>
#include "bm_search.h"
#include <algorithm>

static int suffix_match(const unsigned char *needle, size_t nlen, size_t offset, size_t suffixlen)
{
    if ( offset > suffixlen )
      return needle[offset - suffixlen - 1] != needle[nlen - suffixlen - 1] &&
        !memcmp(needle + nlen - suffixlen, needle + offset - suffixlen, suffixlen);
    else
      return !memcmp(needle + nlen - offset, needle, offset);
}

bm_search::bm_search()
  : m_pattern(NULL)
{
  m_skip = NULL;
  m_plen = 0;
}

bm_search::bm_search(const unsigned char* pattern, size_t plen)
 : m_pattern(pattern),
   m_plen(plen)
{
  make(pattern, plen);
}

int bm_search::make(const unsigned char* pattern, size_t plen)
{
  size_t i;
  /* build stop-symbols table */
  for( i = 0; i < 0x100; ++i )
     occ[i] = -1;
  for( i = 0; i < plen - 1; ++i )
     occ[pattern[i]] = i;
  m_skip = (size_t *)malloc(plen * sizeof(size_t)); /* suffixes table */
  if ( m_skip == NULL )
    return 0;
  for( i = 0; i < plen; ++i )
  {
     size_t offs = plen;
     while(offs && !suffix_match(pattern, plen, offs, i))
          --offs;
     m_skip[m_plen - i - 1] = plen - offs;
  }
  return 1;
}

int bm_search::set(const unsigned char* pattern, size_t plen)
{
  if ( m_skip != NULL )
  {
    free(m_skip);
    m_skip = NULL;
  }
  m_pattern = pattern;
  m_plen = plen;
  return make(pattern, plen);
}

bm_search::~bm_search()
{
  if ( m_skip != NULL )
  {
    free(m_skip);
    m_skip = NULL;
  }
}

const unsigned char* bm_search::search(const unsigned char* mem, size_t mlen)
{
  if ( (m_skip == NULL) || (m_pattern == NULL) )
    return NULL;
  size_t i;
  for (size_t hpos = 0; hpos <= mlen - m_plen; )
  {
    i = m_plen - 1;
    while(m_pattern[i] == mem[i + hpos])
    {
       if (i == 0)
          return mem + hpos;
        --i;
    }
    /* no matching */
    hpos += std::max(long(m_skip[i]), long(i - occ[mem[i + hpos]]));
  }
  return NULL;
}
