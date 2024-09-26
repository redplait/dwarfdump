#include <stdio.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/server/TThreadedServer.h>
#include "gen-cpp/Symref.h"

class ServiceHandler : public SymrefIf
{
 public:
  ServiceHandler() {
    m_ping = 0;
    printf("new ServiceHandler\n");
  }
 ~ServiceHandler() {
    printf("delete ServiceHandler\n");
  }
  virtual int32_t ping() {
    printf("this %p pung %d\n", this, m_ping);
    return m_ping++;
  }
  virtual void quit() {
    printf("quit called, this %p\n", this);
    exit(1);
  }
  virtual void add_func(const FFunc& f) {
  }
  virtual int32_t check_sym(const std::string& name) {
    return 0;
  }
  virtual void check_function(FCheck&res, const std::string&name) {
    res.id = check_sym(name);
    res.skip = 1;
  }
 protected:
  int m_ping;
};

int main(int argc, char **argv)
{
  int port = 17321;
  if ( argc > 1 ) {
    port = atoi(argv[1]);
    if ( !port ) {
      fprintf(stderr, "%s: bad port %s\n", argv[0], argv[1]);
      return 0;
    }
  }
// ripped from https://vorbrodt.blog/2019/03/10/thrift-or-how-to-rpc/
  apache::thrift::server::TThreadedServer server(
	std::make_shared<SymrefProcessor>(std::make_shared<ServiceHandler>()),
	std::make_shared<apache::thrift::transport::TServerSocket>(port),
	std::make_shared<apache::thrift::transport::TBufferedTransportFactory>(),
	std::make_shared<apache::thrift::protocol::TBinaryProtocolFactory>());
  server.serve();
  return 1;
}