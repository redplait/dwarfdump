#include <stdio.h>

class abstract;
typedef const char *(abstract::*M_ref)(int);

class abstract
{
  public:
   virtual ~abstract() {}
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
   virtual M_ref get_ref()
   {
     return &abstract::set_field;
   }
   // fields
   int bit_1: 1;
   int bit_2: 2;
};

class derived: public abstract
{
  public:
   virtual void dump(FILE *fp)
   {
     fprintf(fp, "derived");
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

int main(int argc, char **argv)
{
  abstract *a;
  if ( argc > 1 )
    a = new derived();
  else
    a = new abstract();
  do_work(a);
  auto iref = a->get_ref();
  printf("result %s\n", (a->*iref)(argc));
  delete a;
}