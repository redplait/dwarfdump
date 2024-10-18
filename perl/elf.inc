struct IElf {
 ELFIO::elfio *rdr;
 void add_ref() {
   ref_cnt++;
 }
 void release() {
   if ( !--ref_cnt ) delete this;
 };
 ~IElf();
protected:
  int ref_cnt = 1;
};

IElf *extract(SV *);
const ELFIO::section *find_section(IElf *e, unsigned long addr);
