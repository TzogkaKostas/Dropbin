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

#include "server_header.h"
#include "error_handle.h"

int handle_poll_event(struct pollfd fds[][MAX_FDS], int *nfds, int listening_fd, Client_List *client_list,
		int *compress_array_flag) 
{
	int i, j, total_fds, close_conn, ret_val, bytes, stop_running_server;

	stop_running_server = FALSE;

	total_fds = *nfds;
	//traverse the array with fds to check which fd has caused an event
	for (i = 0; i < total_fds; i++) {
		if((*fds)[i].revents == 0) {continue;} //nothing happened to this fd
		if( !( (*fds)[i].revents & POLLIN ) ) {
			revents_error((*fds)[i].revents);
			return TRUE;
		}

		//new connection
		if ((*fds)[i].fd == listening_fd) {
			//printf("Listening socket is readable.\n");
			stop_running_server = handle_readable_listening_socket(listening_fd, fds, i, nfds);
		}
		else {
			close_conn = FALSE;
			if ((*fds)[i].revents & POLLIN) {
				//printf("Descriptor %d is readable.\n", (*fds)[i].fd);
				close_conn = handle_readable_fd((*fds)[i].fd, client_list);
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

int handle_readable_listening_socket(int listening_fd, struct pollfd fds[][MAX_FDS], int i, int *nfds) {
	int new_socket, stop_running_server;

	stop_running_server = FALSE;

	while (new_socket != -1) {
		new_socket = accept(listening_fd, NULL, NULL);
		if (new_socket < 0)	{
			if (errno != EWOULDBLOCK) {
				accept_error(listening_fd);
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

int handle_readable_fd(int fd, Client_List *client_list) {
	char buffer[BUFFER_SIZE];
	int bytes, request, close_conn, ret_val, msg;
	unsigned long ip;
	unsigned short port;
	Client_List_Node *client_node;

	//read the request's code
	close_conn = recv_from_socket(fd, &request, sizeof(request));
	if (close_conn == TRUE) {return TRUE;}
	request = ntohl(request);

	if (request == LOG_ON) {
		close_conn = recv_from_socket(fd, &ip, sizeof(ip)); //read client's ip
		if (close_conn == TRUE) {return TRUE;}
		//ip = ntohl(ip);

		close_conn = recv_from_socket(fd, &port, sizeof(port)); //read client's port
		if (close_conn == TRUE) {return TRUE;}
		port = ntohs(port);

		insert_client_to_list(client_list, ntohl(ip), port);//if it is already inserted, nothing happens
		insert_fd_to_list(client_list, ntohl(ip), port, fd);
		broadcast_logged_on_client(client_list, ntohl(ip), port);

		inet_ntop(AF_INET, &ip, buffer, sizeof(buffer));
		printf("LOG_ON message from client with ip:%s and port:%hu\n", buffer, port);
	}
	else if (request == GET_CLIENTS) {
		client_node = search_node_by_fd(client_list, fd);
		send_all_clients(client_list, fd, client_node->ip, client_node->port);
		printf("GET_CLIENTS\n");
		return TRUE;
	}
	else if (request == LOG_OFF) {
		close_conn = recv_from_socket(fd, &ip, sizeof(ip)); //read client's ip
		if (close_conn == TRUE) {return TRUE;}
		//ip = ntohl(ip);

		close_conn = recv_from_socket(fd, &port, sizeof(port)); //read client's port
		if (close_conn == TRUE) {return TRUE;}
		port = ntohs(port);

		ret_val = delete_client_from_list(client_list, ntohl(ip), port);
		if (ret_val != -1) {
			msg = htonl(ERROR_IP_PORT_FOUND_IN_LIST);
			write_to_socket(fd, &msg, sizeof(msg));

			broadcast_logged_off_client(client_list, ip, port);
			inet_ntop(AF_INET, &ip, buffer, sizeof(buffer));
			printf("i broadcasted logged off client with ip %s - port %hu\n", buffer, port);
		}
		else {
			msg = htonl(ERROR_IP_PORT_NOT_FOUND_IN_LIST);
			write_to_socket(fd, &msg, sizeof(msg));
		}
		printf("LOG_OFF message from client ip:%lu - port:%d\n", ip, port);
		return TRUE;
	}
}

void compress_array(struct pollfd fds[][MAX_FDS], int *nfds) {
	int i, j;
	for (i = 0; i < *nfds; i++) {
		if ((*fds)[i].fd == -1) {
			for(j = i; j < *nfds; j++) {
				(*fds)[j].fd = (*fds)[j+1].fd;
			}
			i--;
			(*nfds)--;
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

void insert_client_to_list(Client_List *client_list, unsigned long ip, unsigned short port) {
	if (exists(client_list, ip, port) == FALSE) {
		insert(client_list, ip, port);
	}
}

void send_all_clients(Client_List *client_list, int socket_fd, unsigned long ip, unsigned short port) {
	int msg;
	unsigned long ip_msg;
	unsigned short port_msg;

	Client_List_Node *temp;

	msg = htonl(CLIENT_LIST);
	write_to_socket(socket_fd, &msg, sizeof(int));

	msg = htonl(get_num_clients(client_list) - 1);
	write_to_socket(socket_fd, &msg, sizeof(int));

	temp = get_head(client_list);

	while(temp != NULL) {
		if (temp->ip == ip && temp->port == port) {
			temp = get_next(temp);
			continue;
		}
		ip_msg = htonl(temp->ip);
		write_to_socket(socket_fd, &ip_msg, sizeof(unsigned long));

		port_msg = htons(temp->port);
		write_to_socket(socket_fd, &port_msg, sizeof(unsigned short));

		temp = get_next(temp);
	}
}

void broadcast_logged_on_client(Client_List *client_list, unsigned long ip, unsigned short port) {
	struct sockaddr_in client;
	Client_List_Node *client_node;
	int ret_val;
	int socket_fd;
	char buffer[1000];
	unsigned long temp_ip;

	client_node = client_list->head;

	while(client_node != NULL) {
		//send the recently connected client to everyone except that client
		if (client_node->ip == ip && client_node->port == port ) {
			client_node = client_node->next;
			continue;
		}

		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd == -1) {
			socket_error();
			exit(E_SOCKET);
		}

		memset(&client, 0, sizeof(struct sockaddr_in));
		client.sin_family = AF_INET;
		client.sin_addr.s_addr = htonl(client_node->ip);
		client.sin_port = htons(client_node->port);

		temp_ip = htonl(client_node->ip);
		inet_ntop(AF_INET, &temp_ip, buffer, sizeof(buffer));

		ret_val = connect(socket_fd, (struct sockaddr*)&client, sizeof(struct sockaddr_in));
		if (ret_val == -1) {
			connect_error(socket_fd);
			exit(E_CONNECT);
		}

		send_user_on_message(socket_fd);
		send_client_info(client_list, socket_fd, ip, port);

		client_node = client_node->next;
	}
}

void broadcast_logged_off_client(Client_List *client_list, unsigned long ip, unsigned short port) {
	struct sockaddr_in client;
	Client_List_Node *client_node;
	int ret_val, socket_fd;

	client_node = client_list->head;

	while(client_node != NULL) {
		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd == -1) {
			socket_error();
			exit(E_SOCKET);
		}

		memset(&client, 0, sizeof(struct sockaddr_in));
		client.sin_family = AF_INET;
		client.sin_addr.s_addr = htonl(client_node->ip);
		client.sin_port = htons(client_node->port);

		ret_val = connect(socket_fd, (struct sockaddr*)&client, sizeof(struct sockaddr_in));
		if (ret_val == -1) {
			connect_error(socket_fd);
			exit(E_CONNECT);
		}

		send_user_off_message(socket_fd);
		send_client_info(client_list, socket_fd, ip, port);

		client_node = client_node->next;
	}
}

void send_user_on_message(int socket_fd) {
	int msg = htonl(USER_ON);
	write_to_socket(socket_fd, &msg, sizeof(int));
}

void send_user_off_message(int socket_fd) {
	int msg = htonl(USER_OFF);
	write_to_socket(socket_fd, &msg, sizeof(int));
}

void send_client_info(Client_List *client_list, int fd, unsigned long ip, unsigned short port) {
	unsigned long ip_msg;
	unsigned short port_msg;

	ip_msg = htonl(ip);
	write_to_socket(fd, &ip_msg, sizeof(unsigned long));


	port_msg = htons(port);
	write_to_socket(fd, &port_msg, sizeof(unsigned short));
}

int delete_client_from_list(Client_List *client_list, unsigned long ip, unsigned short port) {
	return delete(client_list, ip, port);
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
		printf("setsockopt error: %d.\n",socket_fd);
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

void initialize_sigaction_structures(struct sigaction *sig_int_action) {
	memset(sig_int_action, 0, sizeof(struct sigaction));
	set_sig_int_signal(sig_int_action, sig_int_handler);
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

int read_from_socket(int socket_fd, void *buffer, int length) {
	int bytes;
	bytes = read(socket_fd, buffer, length);
	if (bytes == -1){
		read_error(socket_fd);
		return E_READ;
	}
	return bytes;
}

int recv_from_socket(int socket_fd, void *buffer, int length) {
	int bytes;

	bytes = read(socket_fd, buffer, length);
	if (bytes < 0) {
		if (errno != EWOULDBLOCK) {
			read_error(socket_fd);
			return TRUE;
		}
		return FALSE;
	}
	if (bytes == 0) {
		return TRUE;
	}
	return FALSE;
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

void print_my_ip() {
	char hostbuffer[STRING_SIZE]; 
	char *ip_address_buffer; 
	struct hostent *host_entry; 

	gethostname(hostbuffer, sizeof(hostbuffer)); 

	host_entry = gethostbyname(hostbuffer); 

	ip_address_buffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])); 

	printf("My IP is: %s\n", ip_address_buffer); 
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