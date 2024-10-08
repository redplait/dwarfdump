#include <stdio.h>

class abstract;
typedef const char *(abstract::*M_ref)(int);
typedef const char *(*work_pfn)(abstract *a);

// forward decls
const char *do_work(abstract *a);
const char *do_work2(abstract *a);

class abstract
{
  public:
   abstract()
    : m_worker(do_work),
      bit_1(0),
      bit_2(1)
   {
     s_abs_cnt++;
   }
   virtual ~abstract()
   {
     s_abs_cnt--;
   }
   virtual void dump(FILE *fp)
   {
     fprintf(fp, "anstract");
   }
   virtual const char *get_name()
   {
     return "abstract";
   }
   virtual const char *set_field(int i)
   {
     bit_1 = i & 1;
     return get_name();
   }
   virtual const char *set_field2(int i)
   {
     bit_1 ^= i & 1;
     return get_name();
   }
   virtual M_ref get_ref()
   {
     return &abstract::set_field;
   }
   virtual M_ref get_ref2()
   {
     return bit_1 ? &abstract::set_field : &abstract::set_field2;
   }
   virtual void dump_2(FILE *fp)
   {
     fprintf(fp, "name %s bit_1 %d bit_2 %d\n", get_name(), bit_1, bit_2);
   }
   // fields
   int bit_1: 1;
   int bit_2: 2;
   work_pfn m_worker;
   static int s_abs_cnt;
};

int abstract::s_abs_cnt = 0;

class not_so_abstract: public virtual abstract
{
 public:
   not_so_abstract(): abstract()
   {
     bit_2 = 3;
   }
   virtual const char *get_name()
   {
     return "not_so_abstract";
   }
};

class derived: public not_so_abstract, public virtual abstract
{
  public:
   derived(): not_so_abstract()
   {
     m_worker = do_work2;
     bit_1 = 1;
   }
   virtual ~derived() = default;
   virtual void dump(FILE *fp)
   {
     fprintf(fp, "derived, cnt %d\n", s_abs_cnt);
   }
   virtual const char *get_name()
   {
     return "derived";
   }
   virtual const char *set_field(int i)
   {
     bit_2 = i & 3;
     return get_name();
   }
};

const char *do_work(abstract *a)
{
  a->dump(stdout);
  return a->get_name();
}

const char *do_work2(abstract *a)
{
  a->bit_1 = 0;
  return a->get_name();
}

int main(int argc, char **argv)
{
  abstract *a;
  if ( argc > 2 )
   a = new derived();
  else if ( argc > 1 )
   a = new not_so_abstract();
  else
    a = new abstract();
  a->m_worker(a);
  auto iref = a->get_ref2();
  printf("result %s\n", (a->*iref)(argc));
  a->dump_2(stdout);
  delete a;
}