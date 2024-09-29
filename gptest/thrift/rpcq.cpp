#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <thrift/transport/TSocket.h>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TTransportUtils.h>
#include "gen-cpp/Symref.h"
#include "defport.h"

int main(int argc, char **argv)
{
  int is_test = 0;
  int port = GPROC_DEFAULT_PORT;
  if ( argc > 1 ) {
    port = atoi(argv[1]);
    if ( !port ) {
      fprintf(stderr, "%s: bad port %s\n", argv[0], argv[1]);
      return 0;
    }
  }
  if ( argc > 2 && !strcmp(argv[2], "test") ) is_test = 1;
// ripped from https://vorbrodt.blog/2019/03/10/thrift-or-how-to-rpc/
  auto socket = std::make_shared<apache::thrift::transport::TSocket>("localhost", port);
  auto transport = std::make_shared<apache::thrift::transport::TBufferedTransport>(socket);
  auto protocol = std::make_shared<apache::thrift::protocol::TBinaryProtocol>(transport);
  auto m_clnt = new SymrefClient(protocol);
  try {
    transport->open();
    if ( is_test )
    {
      pid_t my_pid = getpid();
      while(1) {
        printf("pid %d res %d\n", my_pid, m_clnt->ping());
        sleep(3);
      }
    } else
      m_clnt->quit();
  } catch(apache::thrift::TException& tx)
  {
    fprintf(stderr, "ERROR: %s\n", tx.what());
    return 1;
  }
  if ( m_clnt ) delete m_clnt;
  return 0;
}