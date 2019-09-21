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
#include <netdb.h> 
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>


#include "client_header.h"
#include "error_handle.h"

int get_info_from_dropbox_server(Client_List *client_list, Arguments *arguments) {
	int dropbox_server_fd, ret_val;
	struct sockaddr_in server;
	unsigned long my_ip;

	dropbox_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (dropbox_server_fd == -1) {
		socket_error();
		exit(E_SOCKET);
	}

	memset(&server, 0, sizeof(struct sockaddr_in));
	inet_pton(AF_INET, get_server_ip(arguments), &server.sin_addr);
	server.sin_family = AF_INET;
	server.sin_port = htons(get_server_port(arguments));

	ret_val = connect(dropbox_server_fd, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
	if (ret_val == -1) {
		connect_error(dropbox_server_fd);
		exit(E_CONNECT);
	}

	get_my_ip_adress(&my_ip);
	send_log_on_message(dropbox_server_fd, my_ip, get_port_num(arguments));
	printf("LOG_ON message was sent to dropbox server.\n");
	send_get_clients_message(dropbox_server_fd);
	printf("GET_CLIENTS message was sent to dropbox server.\n");
	get_connected_clients(client_list, dropbox_server_fd);

	close(dropbox_server_fd);
	return dropbox_server_fd;
}

void send_log_on_message(int socket_fd, unsigned long ip, unsigned short port) {
	int msg = htonl(LOG_ON);
	write_to_socket(socket_fd, &msg, sizeof(int));

	unsigned long ip_msg = htonl(ip);
	write_to_socket(socket_fd, &ip_msg, sizeof(unsigned long));

	unsigned short port_msg = htons(port);
	write_to_socket(socket_fd, &port_msg, sizeof(unsigned short));
}

void send_log_off_message(Arguments *arguments) {
	int dropbox_server_fd, ret_val;
	struct sockaddr_in server;
	unsigned long my_ip;

	dropbox_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (dropbox_server_fd == -1) {
		socket_error();
		exit(E_SOCKET);
	}

	memset(&server, 0, sizeof(struct sockaddr_in));
	inet_pton(AF_INET, get_server_ip(arguments), &server.sin_addr);
	server.sin_family = AF_INET;
	server.sin_port = htons(get_server_port(arguments));

	ret_val = connect(dropbox_server_fd, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
	if (ret_val == -1) {
		connect_error(dropbox_server_fd);
		exit(E_CONNECT);
	}

	int msg = htonl(LOG_OFF);
	write_to_socket(dropbox_server_fd, &msg, sizeof(int));

	get_my_ip_adress(&my_ip);
	unsigned long ip_msg = htonl(my_ip);
	write_to_socket(dropbox_server_fd, &ip_msg, sizeof(unsigned long));

	unsigned short port_msg = htons(get_port_num(arguments));
	write_to_socket(dropbox_server_fd, &port_msg, sizeof(unsigned short));
	printf("i sent LOG_OFF message.My ip:%lu and port:%hu\n", my_ip, get_port_num(arguments));

	read_from_socket(dropbox_server_fd, &msg, sizeof(msg));
	msg = ntohl(msg);

	if (msg == ERROR_IP_PORT_NOT_FOUND_IN_LIST ) {
		printf("Server returned ERROR_IP_PORT_NOT_FOUND_IN_LIST message\n");
	}

	close(dropbox_server_fd);
}

void send_get_clients_message(int socket_fd) {
	int msg = htonl(GET_CLIENTS);
	write_to_socket(socket_fd, &msg, sizeof(int));
}

int handle_poll_event(struct pollfd fds[][MAX_FDS], int *nfds, int socket_fd, Client_List *client_list,
		int *compress_array_flag, char *dir, Shared_Data *shared_data) 
{

	int i, j, total_fds, close_conn, ret_val, bytes, stop_running_server;

	stop_running_server = FALSE;

	total_fds = *nfds;
	//traverse the array with fds to check which fds are readable
	for (i = 0; i < total_fds; i++) {
		if((*fds)[i].revents == 0) {continue;} //nothing happened to this fd
		if( !( (*fds)[i].revents & POLLIN ) ) {
			revents_error((*fds)[i].revents);
			return TRUE;
		}

		if ((*fds)[i].fd == socket_fd) {
			//printf("Listening socket is readable.\n");
			stop_running_server = handle_readable_listening_socket(socket_fd, fds, i, nfds);
		}
		else {
			close_conn = FALSE;
			if ((*fds)[i].revents & POLLIN) {
				//printf("Descriptor %d is readable.\n", (*fds)[i].fd);
				close_conn = handle_readable_fd((*fds)[i].fd, client_list, dir, shared_data);
			}
			if (close_conn == TRUE) {
				//printf("Connection closed\n");
				close((*fds)[i].fd);
				(*fds)[i].fd = -1;
				*compress_array_flag = TRUE;
			}	
		}
	}
	return stop_running_server;
}

int handle_readable_listening_socket(int socket_fd, struct pollfd fds[][MAX_FDS], int i, int *nfds) {
	int new_socket, stop_running_server;

	stop_running_server = FALSE;
	while (new_socket != -1) {
		new_socket = accept(socket_fd, NULL, NULL);
		if (new_socket < 0)	{
			if (errno != EWOULDBLOCK) {
				accept_error(socket_fd);
				stop_running_server = TRUE;
			}
			break;
	 	}
		//printf("New incoming connection with socket: %d\n", new_socket);
		(*fds)[*nfds].fd = new_socket;
		(*fds)[*nfds].events = POLLIN;		
		(*nfds)++;
	}
	return stop_running_server;
}

int handle_readable_fd(int fd, Client_List *client_list, char *dir, Shared_Data *shared_data) {
	char buffer[BUFFER_SIZE], file_name[STRING_SIZE], file_path[10*STRING_SIZE];
	int bytes, request, close_conn, socket_fd, ret_val, msg, string_length;
	unsigned long ip;
	unsigned short port;
	Item new_item;
	time_t version;
	struct stat statbuf;

	//read the request's code
	close_conn = read_from_socket(fd, &request, sizeof(request));
	if (close_conn == TRUE) {return TRUE;}
	request = ntohl(request);

	if (request == GET_FILE_LIST) {
		printf("CLIENT: GET_FILE_LIST request from fd: %d\n", fd);

		msg = htonl(FILE_LIST);
		write_to_socket(fd, &msg, sizeof(int)); //send the FILE_LIST code

		send_all_file_names(fd, dir);
		send_termination_message(fd);
	}
	else if (request == GET_FILE) {
		//read length of the file name
		read_from_socket(fd, &string_length, sizeof(int));
		string_length = ntohl(string_length);
		//read file name
		read_from_socket(fd, file_name, string_length);
		//read verstion of the file
		read_from_socket(fd, &version, sizeof(time_t));

		sprintf(file_path, "%s/%s", dir, file_name);
		if (file_exists(file_path) == TRUE) {
			if (stat(file_path, &statbuf) != 0) {
				printf("CLIENT: stat error: %s.%s.\n", file_path, strerror(errno));
				return TRUE;
			}

			//check if file's version has been changed
			if (version < statbuf.st_mtime) {
				//send the 'FILE_SIZE' code
				msg = htonl(FILE_SIZE);
				write_to_socket(fd, &msg, sizeof(int));

				//send file size
				msg = htonl(get_entry_size(file_path));
				write_to_socket(fd, &msg, sizeof(msg));

				send_file(fd, file_path, PIPE_SIZE);
				printf("CLIENT: File: %s was sent for the GET_FILE request.\n", file_name);
			}
			else {
				//send the 'FILE_UP_TO_DATE' code
				msg = htonl(FILE_UP_TO_DATE);
				write_to_socket(fd, &msg, sizeof(int));
				printf("CLIENT: FILE_UP_TO_DATE: %s \n", file_path);
			}
		}
		else {
			//send the 'FILE_NOT_FOUND' code
			msg = htonl(FILE_NOT_FOUND);
			write_to_socket(fd, &msg, sizeof(int));
			printf("CLIENT: The requested file: %s was not found.\n", file_path);
			return TRUE;
		}
	}
	else if (request == USER_ON) {
		close_conn = read_from_socket(fd, &ip, sizeof(ip)); //read client's ip
		if (close_conn == TRUE) {return TRUE;}
		//ip = ntohl(ip);

		close_conn = read_from_socket(fd, &port, sizeof(port)); //read client's port
		if (close_conn == TRUE) {return TRUE;}
		port = ntohs(port);

		insert_client_to_list(client_list, ntohl(ip), port);//if it is already inserted, nothing happens

		new_item.ip = ntohl(ip);
		new_item.port = port;
		new_item.task_type = -1;

		sem_wait(&shared_data->emptyCount);
		pthread_mutex_lock(&shared_data->buffer_mutex);

		//insert new logged on client to the task buffer
		shared_data->task_buffer[shared_data->in] = new_item;
		shared_data->in = (shared_data->in + 1)%shared_data->buffer_size;

		pthread_mutex_unlock(&shared_data->buffer_mutex);			
		sem_post(&shared_data->fillCount);

		inet_ntop(AF_INET, &ip, buffer, sizeof(buffer));
		printf("CLIENT: USER_ON message for client with ip:%s and port:%hu\n", buffer, port);
	}
	else if (request == USER_OFF) {
		close_conn = read_from_socket(fd, &ip, sizeof(ip)); //read client's ip
		if (close_conn == TRUE) {return TRUE;}
		//ip = ntohl(ip);

		close_conn = read_from_socket(fd, &port, sizeof(port)); //read client's port
		if (close_conn == TRUE) {return TRUE;}
		port = ntohs(port);

		delete_client_from_list(client_list, ntohl(ip), port);

		int temp_ip = ntohl(ip);
		inet_ntop(AF_INET, &temp_ip, buffer, sizeof(buffer));
		printf("CLIENT: USER_OFF message for client with ip:%s and port:%hu\n", buffer, port);
	}
	return TRUE;
}

int send_all_file_names(int socket_fd, char *directory) {
	DIR *dir;
	struct dirent *entry;
	char entry_name[STRING_SIZE], path[STRING_SIZE];
	int file_size;

	dir = opendir(directory);
	if (dir == NULL) {
		opendir_error(directory);
		exit(E_OPEN_DIR);
	}
	//DFS on input_dir
	while ((entry = readdir(dir)) != NULL) {
		if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0){
			continue;
		}
		sprintf(path,"%s/%s", directory, entry->d_name);
		if (entry->d_type == DT_REG) {
			//e.g. path:1_input1/dir1/dir2/f1 ,entry_name:dir1/dir2/f1 
			cut_first_part_of_the_path(path, entry_name);
			send_name_length(socket_fd, entry_name);
			send_name(socket_fd, entry_name);
			send_version(socket_fd, path);
			printf("CLIENT: file_name: %s was sent.\n", entry_name);
		}
		else {
			send_all_file_names(socket_fd, path);
		}
	}
	closedir(dir);
	return 0;
}

int send_name_length(int socket_fd, char *entry_name) {
	int bytes, entry_name_length;
	entry_name_length = strlen(entry_name) + 1;
	bytes = write_to_socket(socket_fd, &entry_name_length, sizeof(int));
	return bytes;
}

int send_name(int socket_fd, char *name) {
	return write_to_socket(socket_fd, name, strlen(name) + 1);
}

int send_version(int socket_fd, char *path) {
	struct stat statbuf;

	if (stat(path, &statbuf) == 0) {
		printf("i will send %ld version\n", statbuf.st_mtime);
		return write_to_socket(socket_fd, &(statbuf.st_mtime), sizeof(time_t));
	}
	else {
		printf("stat error: %s.%s.\n", path, strerror(errno));
		return -1;
	}	
}

void send_termination_message(int fifo_fd) {
	int termination_msg = TERMINATION_MESSAGE;
	write_to_socket(fifo_fd, &termination_msg, sizeof(int));
}

void insert_client_to_list(Client_List *client_list, unsigned long ip, unsigned short port) {
	if (exists(client_list, ip, port) == FALSE) {
		insert(client_list, ip, port);
	}
}

int delete_client_from_list(Client_List *client_list, unsigned long ip, unsigned short port) {
	return delete(client_list, ip, port);
}

void compress_array(struct pollfd fds[][MAX_FDS], int *nfds) {
	int i, j;
	for (i = 0; i < *nfds; i++) {
		if ((*fds)[i].fd == -1) {
			for(j = i; j < *nfds; j++) {
				(*fds)[j].fd = (*fds)[j+1].fd;
			}
			i--;
			*(nfds)--;
		}
	}
}

void close_all_fds(struct pollfd fds[][MAX_FDS], int nfds) {
	int i;
	for (i = 0; i < nfds; i++) {
		if((*fds)[i].fd >= 0) {
			close((*fds)[i].fd);
		}
	}
}

int server_setup(struct sockaddr_in *server, unsigned short port_num, int *reuse_addr) {
	int ret_val, socket_fd;

	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		socket_error();
		exit(E_SOCKET);
	}

	ret_val = setsockopt(socket_fd, SOL_SOCKET,  SO_REUSEADDR, (char *)reuse_addr, sizeof(int));
	if (ret_val < 0){
		perror("setsockopt() failed");
		close(socket_fd);
		exit(-1);
	}

	set_non_blocking_socket(socket_fd);

	memset(server, 0, sizeof(struct sockaddr_in));
	server->sin_family = AF_INET;
	server->sin_addr.s_addr = htonl(INADDR_ANY);
	server->sin_port = htons(port_num);

	ret_val = bind(socket_fd, (struct sockaddr *)server, sizeof(struct sockaddr_in));
	if (ret_val == -1) {
		bind_error(socket_fd);
		exit(E_BIND);
	}

	ret_val = listen(socket_fd, BACKLOG);	
	if (ret_val == -1) {
		listen_error(socket_fd);
		exit(E_LISTEN);
	}

	return socket_fd;
}

void set_non_blocking_socket(int socket) {
	int opts;

	opts = fcntl(socket,F_GETFL);
	if (opts < 0) {
		fcntl_error(socket);
		perror("fcntl(F_GETFL)");
		exit(E_FCNTL);
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(socket,F_SETFL,opts) < 0) {
		fcntl_error(socket);
		exit(E_FCNTL);
	}
}

void read_command_line_arguments(int argc, char **argv, Arguments *args) {
	int i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-d") == 0) {
			strcpy(args->dir_name, argv[i+1]);
			remove_slash(args->dir_name);
		}
		else if (strcmp(argv[i], "-p") == 0) {
			args->port_num = atoi(argv[i + 1]);
		}
		else if (strcmp(argv[i], "-w") == 0) {
			args->worker_threads = atoi(argv[i + 1]);	
		}
		else if (strcmp(argv[i], "-b") == 0) {
			args->buffer_size = atoi(argv[i + 1]);
		}
		else if (strcmp(argv[i], "-sp") == 0) {
			args->server_port = atoi(argv[i + 1]);
		}
		else if (strcmp(argv[i], "-sip") == 0) {
			strcpy(args->server_ip, argv[i + 1]);
		}
	}
}

void get_connected_clients(Client_List *client_list, int socket_fd) {
	unsigned long ip;
	unsigned short port;
	int i, num_of_clients, msg;

	read_from_socket(socket_fd, &msg, sizeof(int));
	msg = ntohl(msg);

	read_from_socket(socket_fd, &num_of_clients, sizeof(int));
	num_of_clients = ntohl(num_of_clients);

	for (i=0; i < num_of_clients; i++) {
		read_from_socket(socket_fd, &ip, sizeof(unsigned long));
		ip = ntohl(ip);
		read_from_socket(socket_fd, &port, sizeof(unsigned short));
		port = ntohs(port);

		insert(client_list, ip, port);
	}
}

void get_my_ip_adress(unsigned long *my_ip) {
	char hostbuffer[STRING_SIZE]; 
	char *ip_address_buffer; 
	struct hostent *host_entry;
	struct in_addr my_ip_address;

	gethostname(hostbuffer, sizeof(hostbuffer)); 

	host_entry = gethostbyname(hostbuffer); 

	ip_address_buffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])); 

	inet_pton(AF_INET, ip_address_buffer, &my_ip_address);

	*my_ip = ntohl(my_ip_address.s_addr);
}

void initialize_sigaction_structures(struct sigaction *sig_int_action) {
	memset(sig_int_action, 0, sizeof(struct sigaction));
	//memset(sig_chld_action, 0, sizeof(struct sigaction));
	set_sig_int_signal(sig_int_action, sig_int_handler);
	//set_sig_chld_signal(sig_chld_action, sig_chld_handler);
}

void set_sig_int_signal(struct sigaction *sig_action, void (*sig_int_handler)(int)) {
	int ret_val;
	sig_action->sa_handler = sig_int_handler;
	sig_action->sa_flags = SA_RESTART;
	ret_val = sigfillset(&sig_action->sa_mask);
	if (ret_val == -1) {
		signal_error();
		exit(E_SIGNAL);
	}
	ret_val = sigaction(SIGINT, sig_action, NULL);
	if (ret_val == -1) {
		signal_error();
		exit(E_SIGNAL);
	}	
}

void set_sig_chld_signal(struct sigaction *sa, void (*sig_chld_handler)(int, siginfo_t*, void*)) {
	int ret_val;
	sa->sa_sigaction = sig_chld_handler;
	sa->sa_flags = SA_RESTART | SA_SIGINFO | SA_NOCLDSTOP;
	ret_val = sigfillset(&sa->sa_mask);
	if (ret_val == -1) {
		signal_error();
		exit(E_SIGNAL);
	}
	ret_val = sigaction(SIGCHLD, sa, NULL);
	if (ret_val == -1) {
		signal_error();
		exit(E_SIGNAL);
	}	
}

void print_my_ip() {
	char hostbuffer[STRING_SIZE]; 
	char *ip_address_buffer; 
	struct hostent *host_entry; 

	gethostname(hostbuffer, sizeof(hostbuffer)); 

	host_entry = gethostbyname(hostbuffer); 

	ip_address_buffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])); 

	printf("My is IP: %s\n", ip_address_buffer); 
}

void print_my_hostname() {
	char hostbuffer[STRING_SIZE]; 
	char *ip_address_buffer; 
	struct hostent *host_entry; 

	gethostname(hostbuffer, sizeof(hostbuffer)); 

	host_entry = gethostbyname(hostbuffer); 
  
	ip_address_buffer = inet_ntoa( *((struct in_addr*)host_entry->h_addr_list[0])); 

	printf("My hostname is : %s\n", hostbuffer); 
}

int read_from_socket(int socket_fd, void *buffer, int length) {
	int bytes;
	bytes = read(socket_fd, buffer, length);
	if (bytes == -1){
		read_error(socket_fd);
		return E_READ;
	}
	else if (bytes == 0) {
		printf("0 bytes were read from fd: %d\n", socket_fd);
	}
	return bytes;
}

int write_to_socket(int socket_fd, void *buffer, int length) {
	int bytes;
	bytes = write(socket_fd, buffer, length);
	if (bytes == -1){
		write_error(socket_fd);
		exit(E_WRITE); 
	}
	else if (bytes == 0) {
		printf("0 bytes were written to fd: %d\n", socket_fd);
	}
	return bytes;
}

int cut_first_part_of_the_path(char *path, char *new_path) {
	//e.g. 1_input/dir1/dir2/dir3/file1 ---> dir1/dir2/dir3/file1
	char *ptr = strchr(path,'/');
	strcpy(new_path, ptr + 1);
}

int directory_exists(char *directory_name) {
	DIR *dir = opendir(directory_name);
	int exists;
	if (dir == NULL)  {
		exists = FALSE;
	}
	else {
		exists = TRUE;
	}
	closedir(dir);
	return exists;
}

int file_exists(char *filename) {
	if(access(filename,F_OK) == -1 ) {
		return FALSE;
	}
	else {
		return TRUE;
	}
}

void fill_task_buffer_with_connected_clients(Client_List *client_list, Shared_Data *shared_data) {
	Client_List_Node *temp = get_head(client_list);
	Item new_item;
	while(temp != NULL) {
		new_item.ip = temp->ip;
		new_item.port = temp->port;
		new_item.task_type = -1;

		sem_wait(&shared_data->emptyCount);
		pthread_mutex_lock(&shared_data->buffer_mutex);

		shared_data->task_buffer[shared_data->in] = new_item;
		shared_data->in = (shared_data->in + 1)%shared_data->buffer_size;

		pthread_mutex_unlock(&shared_data->buffer_mutex);			
		sem_post(&shared_data->fillCount);

		temp = get_next(temp);
	}
	return;	
}

void print_buffer(Item *buffer) {
	int i;
	for (i=0; i<10; i++) {
		printf("%d item: (%d, %lu, %hu)\n",i, buffer[i].task_type, buffer[i].ip, buffer[i].port);
	}
}

int create_directory(char *directory) {
	if (mkdir(directory,0777) == -1) {
		return E_CREATE_DIR;
	}
	else {
		return 0;
	}
}

int get_entry_size(char *entry_name) {
	struct stat st;
	int ret_val;
	ret_val = stat(entry_name, &st);
	if (ret_val == -1) {
		printf("fstat error with fd %s.%s.\n", entry_name, strerror(errno));
		exit(1);
	}
	return st.st_size;
}

int get_entry_size_by_fd(int fd) {
	struct stat st;
	int ret_val;
	ret_val = fstat(fd, &st);
	if (ret_val == -1) {
		printf("fstat error with fd %d.%s.\n", fd, strerror(errno) );
		exit(1);
	}
	return st.st_size;
}

void send_file(int fifo_fd, char *file_name, int pipe_size) {
	char pipe_buffer[pipe_size];
	int bytes, file_size, fd, written_bytes;

	fd = open(file_name, O_RDONLY);
	if (fd == -1 ) {
		printf("file open error %s.%s\n", file_name, strerror(errno));
		exit(-1);
	}

	file_size = get_entry_size_by_fd(fd);
	written_bytes = 0;
	while(written_bytes < file_size) {
		bytes = read_from_file(fd, pipe_buffer, pipe_size);
		write_to_socket(fifo_fd, pipe_buffer, bytes);
		written_bytes += bytes;
	}
	close(fd);
}

int read_from_file(int fd, void *buffer,int length) {
	int bytes;
	bytes = read(fd, buffer, length);
	if (bytes == -1){
		printf("file read error.%d.%s.\n", fd, strerror(errno));
		exit(-1); 
	}
	return bytes;
}

int remove_slash(char *string) {
	char *ptr;
	ptr = strstr(string, "/");
	if (ptr != NULL) {
		*ptr = '\0';
	}
}