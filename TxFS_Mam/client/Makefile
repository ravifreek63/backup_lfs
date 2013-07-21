all: clients.so simulator transaction
	LD_PRELOAD=/btp/yaffs2-dir/TxFS_Mam/client/clients.so 
transaction:  libclient.a transaction.c
	gcc transaction.c -o transaction -L/btp/yaffs2-dir/TxFS_Mam/client -lclient  -ldl
backup: backup.c libclient.a
	gcc backup.c -o backup -L /btp/TxFS_Mam/client -lclient -ldl
libclient.a: libclient.o client.o
	ar rcs libclient.a libclient.o client.o
libclient.o:libclient.c  
	 gcc -c libclient.c -o libclient.o 
clients.so: client.o client_invoke.o
	ld -shared -o clients.so client_invoke.o client.o -ldl
client_invoke.o: client_invoke.c
	gcc -Wall -fPIC -DPIC -c -g client_invoke.c
client.o: client.c
	gcc -Wall -fPIC -DPIC -c -g client.c -ldl
simulator: libclient.a simulator.c
	gcc -g simulator.c -o simulator -L/btp/yaffs2-dir/TxFS_Mam/client -lclient -ldl


