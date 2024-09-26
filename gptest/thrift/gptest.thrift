struct FCheck {
 1: i32 id,
 2: i32 skip
}

/* one error record */
struct FErrRec {
 1: i32 bid,
 2: string msg
}

/* one xref item */
struct FRefs {
 1: i32 bid,
 2: i32 arg,
 3: i32 what,
 4: i8 kind
}

/* body of function, name added in check_function */
struct FFunc {
 1: i32 id,
 2: i32 bbcount,
 3: string fname, /* file name */
 4: list<FErrRec> errors,
 5: list<FRefs> refs
}

service Symref {
  FCheck check_function(1: string funcname),
  i32 check_sym(1: string name),
  void add_func(1: FFunc f),
  void quit(),
  // for tests
  i32 ping()
}