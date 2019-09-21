#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "error_handle.h"


void server_argument_error(char *program_name) {
	printf("usage: %s -p portNum \n", program_name);
}

void client_argument_error(char *program_name) {
	printf("usage: %s -d dirName -p portNum -w workerThreads -b bufferSize -sp"
		"serverPort -sip serverIP \n", program_name);
}

void socket_error() {
	printf("socket error.%s\n", strerror(errno));
}

void listen_error(int socket_fd) {
	printf("listen error with socket %d.%s\n", socket_fd, strerror(errno));
}

void accept_error(int socket_fd) {
	printf("accept error with socket %d.%s\n", socket_fd, strerror(errno));
}

void gethostbyaddr_error(int addr) {
	printf("gethostbyaddr error with address %d.%s\n", addr, strerror(errno));
}

void fork_error() {
	printf("fork error.%s\n", strerror(errno));
}

void read_error(int socket_fd) {
	printf("read error with socket %d.%s\n", socket_fd, strerror(errno));
}

void signal_error() {
	printf("signal error:%s.\n", strerror(errno));
}

void connect_error(int socket_fd) {
	printf("connect error with socket %d.%s\n", socket_fd, strerror(errno));
}

void write_error(int socket_fd) {
	printf("write error with socket %d.%s\n", socket_fd, strerror(errno));
}

void bind_error(int socket_fd) {
	printf("bind error with socket %d.%s\n", socket_fd, strerror(errno));
}

void fcntl_error(int socket_fd) {
	printf("fcntl error with socket %d.%s\n", socket_fd, strerror(errno));
}

void poll_error() {
	printf("poll error:%s.\n", strerror(errno));
}

void revents_error(int revent) {
	printf("revents error with revent code:%d.%s.\n", revent, strerror(errno));
}
void opendir_error(char *name) {
	printf("opendir error :%s.%s.\n", name, strerror(errno));
}

void file_creat_error(char *name) {
	printf("file creat error:%s.%s.\n", name, strerror(errno));
}