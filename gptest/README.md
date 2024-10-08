## supported versions
I was able to build plugin for gcc 9, 12 & 14 - probably it will work on earlier versions too
For cross-compiler you must install plugin-dev - for example for gcc-9-cross-base-mipsen you need
```sh
apt-get install gcc-9-plugin-dev-mips-linux-gnu
```
Then in Makefile patch TGCC to peek your cross-compiler, [like](https://github.com/redplait/dwarfdump/blob/main/gptest/Makefile.mips)
```make
TGCC=mips-linux-gnu-gcc-9
```
Supported languages:
* c
* c++
* fortran

I am too lazy and never been big fan of agricultural company "bitten apple", so I don't know if plugin will work for objective c.
Also in gcc 14 go & rust are highly experimental and literally unusable

## plugin options
* -fplugin-arg-gptest-db=path to file/db connection string
* -fplugin-arg-gptest-user=db user.
  Currently not used (but passed as argument to FPersistence::connect)
* -fplugin-arg-gptest-password=password for db access.
  Currently not used (but passed as argument to FPersistence::connect)
* -fplugin-arg-gptest-asmproto
* -fplugin-arg-gptest-dumprtl - almost like -fdump-rtl-final
* -fplugin-arg-gptest-ic to dump integer constants, optionally you can peek config filename - see [details](https://redplait.blogspot.com/2023/09/gcc-plugin-to-collect-cross-references.html#more)
* -fplugin-arg-gptest-verbose

## Storage backends
Currently I implemented 3:
1. plain text files - mostly for debugging. To build add file pers_text.cpp to PLUGIN_SOURCE_FILES.
   db is just path to text file
2. sqlite. To build add file pers_sqlite.cpp to PLUGIN_SOURCE_FILES and library -lsqlite3.
   db is just path to sqlite db file 
3. rpc [client](https://redplait.blogspot.com/2024/09/gcc-plugin-to-collect-cross-references.html) via Apache Thrift. To build add to PLUGIN_SOURCE_FILES files
```make
pers_rpc.o thrift/gen-cpp/Symref.o thrift/gen-cpp/gptest_types.o
```
 and library -lthrift. db is server_name:port. Source code of server located in sub-directory [thrift](https://github.com/redplait/dwarfdump/tree/main/gptest/thrift)

## what I tested plugin on
Notwithstanding gcc has testsuite - they are mostly regressions tests and don't cover totally all possibilities
So I rebuild with my plugin following:
* gcc 12 & 14
* binutils
* R 4.3.1 - mainly to test fortran
* linux kernel
* boost library 1.86
* gptest plugin itself
  
## Known issues
* unnamed records. Really - structures can be unnamed like
```c++
struct {
 int somefield;
 float otherfield;
} g;
...
g.somefield;
```
plugin will give error "no type_name for ctx XX", where XX is 0x11 for structure and 0x12 for union
* pointer to member - despite the fact that in file cp/cp-tree.def there are OFFSET_REF & PTRMEM_CST in real RTL they are just integer constants
* [similalry](http://redplait.blogspot.com/2023/08/gcc-plugin-to-collect-cross-references_19.html)  pointer to method
* DEBUG_EXPR_DECL - plugin usually will give error something like "unknown something 0x29"
* TLS are indistinguishable from regular global vars. [Details](https://redplait.blogspot.com/2024/10/tls-in-gcc-rtl.html)
* to be continued 

