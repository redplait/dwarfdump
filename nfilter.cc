#include <set>
#include "nfilter.h"
#include <strings.h>

struct cmpStrings {
  bool operator()(const char *a, const char *b) const {
    return strcasecmp(a, b) < 0;
  }
};

static std::set<const char *, cmpStrings> s_allowed;
static std::set<const char *, cmpStrings> s_denied;

void add_filter(const char *s)
{
  if ( !s )
   return;
  if ( !*s )
   return;
  if ( *s == '-' )
  {
    s++;
    if ( !*s )
     return;
    s_denied.insert(s);
  } else
   s_allowed.insert(s);
}

bool need_dump(const char *f)
{
  if ( !f )
    return true;
  if ( s_allowed.empty() && s_denied.empty() )
    return true;
  auto d = s_denied.find(f);
  if ( d != s_denied.end() )
    return false;
  if ( s_allowed.empty() )
    return true;
  auto a = s_allowed.find(f);
  return (a != s_allowed.end());
}
