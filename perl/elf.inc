struct IElf {
 int ref_cnt = 1;
 ELFIO::elfio *rdr;
 void add_ref() {
   ref_cnt++;
 }
 void release() {
   if ( !--ref_cnt ) delete this;
 };
 ~IElf() {
   if ( rdr ) delete rdr;
 }
};

IElf *extract(SV *);