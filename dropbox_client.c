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
#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>

#include "client_header.h"
#include "error_handle.h"

volatile sig_atomic_t stop_running_server = FALSE;

void sig_int_handler(int signum) {
	char msg[STRING_SIZE];
	sprintf(msg, "signal SIGINT was caught\n");
	write(1, msg, strlen(msg));
	stop_running_server = 0;
}

int main(int argc, char **argv) {
	Client_List *client_list;
	Arguments arguments;
	struct sigaction sig_int_action;
	struct sockaddr_in dropbox_server;
	int socket_fd, dropbox_server_fd, new_socket, ret_val, err;
	unsigned long my_ip;
	Shared_Data *shared_data;

	int reuse_addr = 1, timeout, nfds = 1, i, j;	
	int compress_array_flag;
	char buffer[BUFFER_SIZE];
	struct pollfd fds[MAX_FDS];

	if (argc < 11) {
		client_argument_error(argv[0]);
		exit(E_ARGUMENT);
	}

	read_command_line_arguments(argc, argv, &arguments);
		//SIGINT, SIGCHLD handlers
	initialize_sigaction_structures(&sig_int_action);
	//list with clients's IP/ports
	initialize_list(&client_list);

	//socket, bind, listen are called in this function
	socket_fd = server_setup(&dropbox_server, get_port_num(&arguments), &reuse_addr);

	dropbox_server_fd = get_info_from_dropbox_server(client_list, &arguments);

	get_my_ip_adress(&my_ip);
	print_my_hostname();
	print_my_ip();

	//initialization of shared data
	shared_data = (Shared_Data*)malloc(sizeof(Shared_Data));
	shared_data->buffer_size = get_buffer_size(&arguments);
	shared_data->task_buffer = (Item*)malloc(sizeof(Item)*get_buffer_size(&arguments));
	pthread_mutex_init(&(shared_data->buffer_mutex), NULL);
	pthread_mutex_init(&(shared_data->client_mutex), NULL);
	sem_init(&(shared_data->fillCount), 0, 0);
	sem_init(&(shared_data->emptyCount), 0, shared_data->buffer_size);
	shared_data->in = 0;
	shared_data->out = 0;
	shared_data->client_list = client_list;

	pthread_t thread_id[get_worker_threads(&arguments)];
	for (i = 0; i < get_worker_threads(&arguments); i++) {
		if (err = pthread_create(&thread_id[i], NULL, thread_function, (void *)shared_data)) {
			printf("pthread_create error\n");
			exit(1);
		}
	}

	fill_task_buffer_with_connected_clients(client_list, shared_data);

	//put the listening socket to the monitored fds
	memset(fds, 0 , sizeof(fds));
	fds[0].fd = socket_fd;
	fds[0].events = POLLIN;
	timeout = TIME_OUT_DURATION;

	//when an fd is closed, the array with the fd structures is compressed 
	compress_array_flag = FALSE; 
	while(stop_running_server == FALSE) {
		printf("Waiting on poll()...\n");
		ret_val = poll(fds, nfds, timeout);
		if (ret_val < 0) {
			if (errno != EINTR) {
				poll_error();
			}
			break;
		}
		else if (ret_val == 0) {
			printf("TIMEOUT in poll.\n");
			break;
		}

		stop_running_server = handle_poll_event(&fds, &nfds, socket_fd, 
				client_list, &compress_array_flag, get_dir_name(&arguments), shared_data);
		
		if (compress_array_flag == TRUE) {
			compress_array(&fds, &nfds);
			compress_array_flag = FALSE;
		}
	}
	send_log_off_message(&arguments);

	close_all_fds(&fds, nfds);

	for (i = 0; i < get_worker_threads(&arguments); i++) {
		pthread_cancel(thread_id[i]);
	}
	for (i = 0; i < get_worker_threads(&arguments); i++) {
		pthread_join(thread_id[i], NULL);
	}

	destroy(&client_list);
	free(shared_data->task_buffer);
	free(shared_data);
	exit(0);
}