#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#include "server_header.h"
#include "error_handle.h"

//if a SIGINT signal is caught or an error occurs server stops running.
volatile sig_atomic_t stop_running_server = FALSE;


void sig_int_handler(int signum) {
	char msg[STRING_SIZE];
	sprintf(msg, "signal SIGINT was caught\n");
	write(1, msg, strlen(msg));
	stop_running_server = 0;
}

int main (int argc, char *argv[]) {
	Client_List *client_list;
	struct sigaction sig_int_action;
	struct sockaddr_in server;
	int listening_fd, new_socket, ret_val;
	unsigned short port_num;

	int reuse_addr = 1, timeout, nfds = 1, i, j;	
	int compress_array_flag;
	char buffer[BUFFER_SIZE];
	struct pollfd fds[MAX_FDS];

	if (argc != 3) {
		server_argument_error(argv[0]);
		exit(E_ARGUMENT);
	}
	else {
		port_num = atoi(argv[2]);		
	}
	
	//SIGINT handlers
	initialize_sigaction_structures(&sig_int_action);
	//list with clients's IP/ports
	initialize_list(&client_list);
	//socket, bind, listen are called in this function
	listening_fd = server_setup(&server, port_num, &reuse_addr);

	print_my_hostname();
	print_my_ip();
	
	//put the listening socket to the monitored fds
	memset(fds, 0 , sizeof(fds));
	fds[0].fd = listening_fd;
	fds[0].events = POLLIN;
	timeout = TIME_OUT_DURATION;

	//when an fd is closed, the array with the fd structures is compressed 
	compress_array_flag = FALSE; 
	while(stop_running_server == FALSE) {
		printf("Waiting on poll()...\n");
		ret_val = poll(fds, nfds, timeout);
		if (ret_val < 0) {
			if (errno != EINTR) { //a SIGINT signal interrupted the poll function
				poll_error();
			}
			break;
		}
		else if (ret_val == 0) {
			printf("TIMEOUT in poll.\n");
			break;
		}

		stop_running_server = handle_poll_event(&fds, &nfds, listening_fd, client_list, &compress_array_flag);
		
		if (compress_array_flag == TRUE) {
			//if a fd is closed, compress the array
			compress_array(&fds, &nfds);
			compress_array_flag = FALSE;
		}
	}
	close_all_fds(&fds, nfds);
	destroy(&client_list);
	exit(0);
}
