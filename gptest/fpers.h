#pragma once
// interface for gpt plugin results persistence

enum xref_kind {
  xcall, // just direct call function/method
  vcall, // virtual method call
  xref,  // some reference
  field, // xref to some structure field
  fconst, // floating point const
};

class FPersistence
{
  public:
    virtual ~FPersistence() {}
    // 0 if connection was established
    virtual int connect(const char *path, const char *username, const char *password) = 0;
    virtual void disconnect() {}
    // compilation unit
    virtual void cu_start(const char *) {}
    virtual void cu_stop() {}
    // process some function, 0 if this function should not be processed (like already stored in some db)
    virtual int func_start(const char *) { return 1; }
    virtual void func_proto(const char *) {}
    virtual void func_stop() {}
    // process some basic block
    virtual void bb_start(int idx) {}
    virtual void bb_stop(int idx) {}
    // main method
    virtual void add_xref(xref_kind, const char *) {}
    virtual void add_literal(const char *, int size) {}
    virtual void add_ic(int) {}
    // put comment
    virtual void add_comment(const char *) {}
    // report errors
    virtual void report_error(const char *) {}
};

// somewhere
FPersistence *get_pers();
