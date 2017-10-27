all:base dll
dll:
	g++ -o base.so -shared -fPIC base.cpp
base:base.o
	#g++  -g myredis.cpp tserver.cpp -I./hiredis -I./lib_acl_cpp/include  -lpthread -L./ -lacl_cpp -lprotocol -lacl  -o tserver  -DLINUX2  
	g++  -g base.cpp  -lpthread -o base  
	#g++  -g myredis.cpp base.cpp -I./hiredis -I./lib_acl_cpp/include  -lpthread -L./ -lacl_cpp -lprotocol -lacl  -o base  -DLINUX2  
	#g++  -g myredis.cpp tserver.cpp -I./hiredis -I./acl/lib_acl/include  -I./acl/lib_acl_cpp/include  -lpthread -L./ -lacl -lprotocol -lacl_cpp -L./hiredis -lhiredis -o tserver -static -DLINUX2
%.o:%.cpp
	g++ -c $<  
.PHONY: clean
clean:
	rm -rf client.o client server server.o ini.o epollserver.o prob.o usermap.o MySql.o tserver.o myredis.o base base.o
