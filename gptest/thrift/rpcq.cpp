#include <stdio.h>
#include <stdlib.h>
#include <thrift/transport/TSocket.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TTransportUtils.h>
#include "gen-cpp/Symref.h"

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
  auto socket = std::make_shared<apache::thrift::transport::TSocket>("localhost", port);
  auto transport = std::make_shared<apache::thrift::transport::TBufferedTransport>(socket);
  auto protocol = std::make_shared<apache::thrift::protocol::TBinaryProtocol>(transport);
  auto m_clnt = new SymrefClient(protocol);
  try {
    transport->open();
    m_clnt->quit();
    delete m_clnt;
  } catch(apache::thrift::TException& tx)
  {
    fprintf(stderr, "ERROR: %s\n", tx.what());
    return 1;
  }
  return 0;
}