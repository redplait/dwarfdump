#include <stdio.h>
#include <time.h>
#include <string>
#include <unordered_map>
#include "fpers.h"
#include <thrift/transport/TSocket.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TTransportUtils.h>
#include "thrift/gen-cpp/Symref.h"

class pers_rpc: public FPersistence
{
 public:
  pers_rpc() {
    m_clnt = nullptr;
    bb_idx = 0;
  }
  virtual ~pers_rpc() {
    if ( m_clnt ) delete m_clnt;
  }
  virtual int connect(const char *, const char *, const char *) override;
  virtual void cu_start(const char *f) {
    m_func.fname = f;
  }
  virtual void bb_start(int idx) {
    bb_idx = idx;
  }
  virtual int func_start(const char *fn, int block_count) override
  {
    func_name = fn;
    m_clnt->check_function(m_check, func_name);
    if ( m_check.skip ) {
      m_func.id = 0;
      return 0;
    }
    // cleanup m_func
    m_func.errors.clear();
    m_func.refs.clear();
    m_func.bbcount = block_count;
    m_func.id = m_check.id;
    // store received id in cache
    m_syms[fn] = m_check.id;
    return 1;
  }
  virtual void func_stop() override
  {
    // this is where whole content of function is sent
    if ( !m_func.id || ( m_func.refs.empty() && m_func.errors.empty() ) ) return;
    m_clnt->add_func(m_func);
    m_func.id = 0;
  }
  // add error record to m_func.errors
  virtual void report_error(const char *msg) override {
    FErrRec rec;
    rec.bid = bb_idx;
    rec.msg = msg;
    m_func.errors.push_back(rec);
  }
  virtual void add_xref(xref_kind, const char *, int arg_no = 0) override;
  virtual void add_literal(const char *s, int slen) override {
    std::string lit(s, slen);
    FRefs ref;
    ref.bid = bb_idx;
    ref.arg = 0;
    ref.kind = 'l';
    ref.what = check_sym(lit);
    m_func.refs.push_back(ref);
  }
 protected:
  int check_sym(std::string &);
  int bb_idx;
  // current function name
  std::string func_name;
  // cached results of client->check_sym
  std::unordered_map<std::string, int> m_syms;
  FCheck m_check;
  FFunc m_func;
  SymrefClient *m_clnt;
};

void pers_rpc::add_xref(xref_kind x, const char *s, int arg_no)
{
  FRefs ref;
  ref.bid = bb_idx;
  ref.arg = arg_no;
  switch(x)
  {
    case xcall:
     ref.kind = 'c';
     break;
    case vcall:
     ref.kind = 'v';
     break;
    case xref:
     ref.kind = 'r';
     break;
    case field:
     ref.kind = 'f';
     break;
    case fconst:
     ref.kind = 'F';
     break;
    default: return; // wtf?
  }
  std::string sym(s);
  ref.what = check_sym(sym);
  m_func.refs.push_back(ref);
}

int pers_rpc::check_sym(std::string &s)
{
  auto ci = m_syms.find(s);
  if ( ci != m_syms.end() ) return ci->second;
  // ok - call server
  int res = m_clnt->check_sym(s);
  m_syms[s] = res;
  return res;
}

// addr has format hostname:port
int pers_rpc::connect(const char *addr, const char *user, const char *pass)
{
  // extract port
  auto colon = strrchr(addr, ':');
  if ( !colon ) {
    fprintf(stderr, "invalud address %s\n", addr);
    return 0;
  }
  int port = atoi(colon + 1);
  if ( !port ) {
    fprintf(stderr, "bad port %s\n", colon + 1);
    return 0;
  }
  // ripped from https://vorbrodt.blog/2019/03/10/thrift-or-how-to-rpc/
  std::string host(addr, colon - addr);
  auto socket = std::make_shared<apache::thrift::transport::TSocket>(host, port);
  auto transport = std::make_shared<apache::thrift::transport::TBufferedTransport>(socket);
  auto protocol = std::make_shared<apache::thrift::protocol::TBinaryProtocol>(transport);
  m_clnt = new SymrefClient(protocol);
  try {
    transport->open();
  } catch(apache::thrift::TException& tx)
  {
    fprintf(stderr, "ERROR: %s\n", tx.what());
    return 0;
  }
  return 1;
}

FPersistence *get_pers()
{
  return new pers_rpc();
}
