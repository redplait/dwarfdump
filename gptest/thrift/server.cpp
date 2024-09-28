#include <stdio.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/server/TThreadedServer.h>
#include "gen-cpp/Symref.h"
#include "../sqlite_cmn.inc"
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <queue>
#include <atomic>

class ServiceHandler: public SymrefIf, protected sqlite_cmn
{
 public:
  ServiceHandler(): sqlite_cmn() {
    m_ping = 0;
    m_quit = false;
  }
 ~ServiceHandler() {
    close();
  }
  int open_db(const char *db)
  {
    int res = open(db);
    if ( res ) return res;
    std::thread t([&]() {
      while(1) {
        std::unique_lock lk(m_db_lock);
        m_cv.wait(lk, [&]() { return m_quit || !m_q.empty(); });
        while( !m_q.empty() )
        {
          m_q.front()();
          m_q.pop();
        }
        if ( m_quit ) {
          close();
          exit(0);
        }
      }
    });
    t.detach();
    return 0;
  }
  virtual int32_t ping() {
    printf("this %p pung %d\n", this, m_ping.load());
    return m_ping++;
  }
  virtual void quit() {
    m_quit = true;
    m_cv.notify_one();
  }
  virtual void add_func(const FFunc& f);
  virtual int32_t check_sym(const std::string& name);
  virtual void check_function(FCheck&res, const std::string&name);
 protected:
  template <typename T, typename W>
  void add_sym(T s, int res, W push)
  {
    std::string n(s);
    auto func = [=]() {
     sqlite3_reset(m_insert_sym);
     sqlite3_bind_int(m_insert_sym, 1, res);
     sqlite3_bind_text(m_insert_sym, 2, n.c_str(), n.size(), SQLITE_STATIC);
     sqlite3_bind_text(m_insert_sym, 3, "", 0, SQLITE_STATIC);
     sqlite3_bind_int(m_insert_sym, 4, 0);
     sqlite3_step(m_insert_sym);
   };
   push(func);
  }
  void clear_func(int id)
  {
    auto d1 = [=]() {
     sqlite3_reset(m_del_xrefs);
     sqlite3_bind_int(m_del_xrefs, 1, id);
     sqlite3_step(m_del_xrefs);
    };
    db_push(d1);
    auto d2 = [=]() {
     sqlite3_reset(m_del_errs);
     sqlite3_bind_int(m_del_errs, 1, id);
     sqlite3_step(m_del_errs);
    };
    db_push_wakeup(d2);
  }
  template <typename T>
  void db_push(T &func) {
    std::unique_lock l(m_db_lock);
    m_q.push(func);
  }
  template <typename T>
  void db_push_wakeup(T &func) {
    db_push(func);
    m_cv.notify_one();
  }
  // cache sync
  std::shared_mutex m_cache_lock;
  // db logic in separate thread
  std::mutex m_db_lock;
  std::queue<std::function<void()> > m_q;
  std::condition_variable m_cv;
  std::atomic<int> m_ping;
  volatile bool m_quit;
};

void ServiceHandler::check_function(FCheck&res, const std::string&name)
{
  res.skip = 0;
  res.id = 0;
  bool del;
  {
    std::shared_lock l(m_cache_lock);
    del = check_func(name.c_str(), res.id);
    if ( !del && res.id ) return;
  }
  if ( del ) {
    clear_func(res.id);
    return;
  }
  {
    std::unique_lock l(m_cache_lock);
    del = check_func(name.c_str(), res.id);
    if ( !del && res.id ) return;
    if ( del ) {
      res.skip = 1;
      return;
    }
    // ok, store func
    res.id = ++max_id;
    // store in cache
    add(name.c_str(), res.id);
  }
  add_sym(name, res.id, [&](auto &t) { db_push_wakeup(t); });
}

void ServiceHandler::add_func(const FFunc& f)
{
  {
    std::unique_lock l(m_cache_lock);
    m_funcs.insert(f.id);
  }
  int empty = f.errors.empty() && f.refs.empty();
  // update func in db
  auto u = [=]() {
    sqlite3_reset(m_update_fname);
    sqlite3_bind_int(m_update_fname, 3, f.id); // id in where clause - last
    sqlite3_bind_text(m_update_fname, 1, f.fname.c_str(), f.fname.size(), SQLITE_STATIC);
    sqlite3_bind_int(m_update_fname, 2, f.bbcount);
    sqlite3_step(m_update_fname);
  };
  db_push(u);
  if ( empty ) return;
  for ( const auto er: f.errors )
  {
    auto ie = [=]() {
      sqlite3_reset(m_insert_err);
      sqlite3_bind_int(m_insert_err, 1, f.id);
      sqlite3_bind_int(m_insert_err, 2, er.bid);
      sqlite3_bind_text(m_insert_err, 3, er.msg.c_str(), er.msg.size(), SQLITE_STATIC);
      sqlite3_step(m_insert_err);
    };
    db_push(ie);
  }
  for ( const auto x: f.refs )
  {
    auto ins = [=]() {
     sqlite3_reset(m_insert_xref);
     sqlite3_bind_int(m_insert_xref, 1, f.id);
     sqlite3_bind_int(m_insert_xref, 2, x.bid);
     sqlite3_bind_text(m_insert_xref, 3, (const char *)&x.kind, 1, SQLITE_STATIC);
     sqlite3_bind_int(m_insert_xref, 4, x.arg);
     sqlite3_bind_int(m_insert_xref, 5, x.what);
     sqlite3_step(m_insert_xref);
    };
    db_push(ins);
  }
  m_cv.notify_one();
}

int32_t ServiceHandler::check_sym(const std::string& name) {
  int32_t res = 0;
  // classical double fetch
  {
    std::shared_lock l(m_cache_lock);
    if ( check<const std::string&>(name, res) )
      return res;
  }
  {
    std::unique_lock l(m_cache_lock);
    if ( check<const std::string&>(name, res) )
      return res;
    res = ++max_id;
    // store in cache
    add(name.c_str(), res);
  }
  add_sym(name, res, [&](auto &t) { db_push_wakeup(t); });
  return res;
}

int main(int argc, char **argv)
{
  if ( argc < 2 ) {
    printf("%s: db_path [port]\n", argv[0]);
    return 6;
  }
  int port = 17321;
  if ( argc > 2 ) {
    port = atoi(argv[2]);
    if ( !port ) {
      fprintf(stderr, "%s: bad port %s\n", argv[0], argv[2]);
      return 0;
    }
  }
// ripped from https://vorbrodt.blog/2019/03/10/thrift-or-how-to-rpc/
  ServiceHandler srv;
  int res = srv.open_db(argv[1]);
  if ( res ) {
    fprintf(stderr, "cannot start server on %s, err %d\n", argv[1], res);
    return res;
  }
  apache::thrift::server::TThreadedServer server(
	std::make_shared<SymrefProcessor>(std::shared_ptr<SymrefIf>(&srv)),
	std::make_shared<apache::thrift::transport::TServerSocket>(port),
	std::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
	std::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>());
  server.serve();
  return 1;
}