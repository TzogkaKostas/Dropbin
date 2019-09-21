all: dropbox_server dropbox_client


dropbox_server: dropbox_server.o server_functions.o structs_implem.o error_handle.o server_header.h
	gcc dropbox_server.o server_functions.o structs_implem.o error_handle.o -o dropbox_server
dropbox_server.o: dropbox_server.c server_header.h error_handle.h
	gcc -c dropbox_server.c -o dropbox_server.o
server_functions.o: server_functions.c server_header.h
	gcc -c server_functions.c -o server_functions.o

dropbox_client: dropbox_client.o client_functions.o thread_functions.o structs_implem.o error_handle.o client_header.h
	gcc dropbox_client.o client_functions.o thread_functions.o structs_implem.o error_handle.o -o dropbox_client -lpthread
dropbox_client.o: dropbox_client.c client_header.h error_handle.h
	gcc -c dropbox_client.c -o dropbox_client.o 
client_functions.o: client_functions.c client_header.h
	gcc -c client_functions.c -o client_functions.o
thread_functions.o: thread_functions.c client_header.h
	gcc -c thread_functions.c -o thread_functions.o

structs_implem.o: structs_implem.c structs.h
	gcc -c structs_implem.c -o structs_implem.o
error_handle.o: error_handle.c error_handle.h
	gcc -c error_handle.c -o error_handle.o
clean :
	rm *.o dropbox_client dropbox_server