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
#include <libgen.h>
#include <pthread.h>
#include <utime.h>

#include "client_header.h"
#include "error_handle.h"

#define PIPE_SIZE 1024

volatile sig_atomic_t stop_running_thread = FALSE;

void clean_thread(void *arg) {
	stop_running_thread = TRUE;
	printf("THREAD: cleanup handler\n");
}

void* thread_function(void *arg) {
	Item *task_buffer, item;
	Shared_Data *shared_data;
	shared_data = (Shared_Data*)arg;
	task_buffer = shared_data->task_buffer;

	pthread_cleanup_push(clean_thread, NULL);

	while (stop_running_thread == FALSE) {		
		sem_wait(&shared_data->fillCount);
		pthread_mutex_lock(&shared_data->buffer_mutex);

		item = task_buffer[shared_data->out];
		shared_data->out = (shared_data->out + 1)%shared_data->buffer_size;

		pthread_mutex_unlock(&shared_data->buffer_mutex);
		sem_post(&shared_data->emptyCount);

		handle_buffer_item(item, shared_data);
	}

	pthread_cleanup_pop(0);
	pthread_exit(NULL);
}

void handle_buffer_item(Item received_item, Shared_Data *shared_data) {

	int socket_fd, ret_val, file_size;
	struct sockaddr_in client;
	unsigned long my_ip;
	int entry_name_length;
	char entry_name[STRING_SIZE], buffer[STRING_SIZE], client_name[10*STRING_SIZE];
	char path[20*STRING_SIZE], *temp_buff;
	Item new_item;
	time_t version;
	struct stat statbuf;

	int temp = ntohl(received_item.ip);
	inet_ntop(AF_INET, &temp, buffer, sizeof(buffer));

	//search if task's ip/port exist in client list
	pthread_mutex_lock(&shared_data->buffer_mutex);
	if (search_node(shared_data->client_list, received_item.ip, received_item.port) == NULL) {
		printf("THREAD: There is no client with ip:%s and port:%hu \n", buffer, received_item.port);
		pthread_mutex_unlock(&shared_data->buffer_mutex);
		return;
	}
	pthread_mutex_unlock(&shared_data->buffer_mutex);

	if (received_item.task_type == -1) {
		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd == -1) {
			socket_error();
			exit(E_SOCKET);
		}

		memset(&client, 0, sizeof(struct sockaddr_in));
		client.sin_family = AF_INET;
		client.sin_addr.s_addr = htonl(received_item.ip);
		client.sin_port = htons(received_item.port);

		ret_val = connect(socket_fd, (struct sockaddr*)&client, sizeof(struct sockaddr_in));
		if (ret_val == -1) {
			connect_error(socket_fd);
			exit(E_CONNECT);
		}

		//send the request code
		int msg = htonl(GET_FILE_LIST);
		write_to_socket(socket_fd, &msg, sizeof(int));
		printf("THREAD: GET_FILE_LIST request was sent to ip:%s - port: %hu\n", buffer, received_item.port);

		//receive FILE_LIST code
		read_from_socket(socket_fd, &msg, sizeof(int));

		//receive first file's name length
		read_from_socket(socket_fd, &entry_name_length, sizeof(int));
		entry_name_length = ntohl(entry_name_length);
		
		//receive all file names until '0' is sent
		while(entry_name_length != TERMINATION_MESSAGE) {
			read_from_socket(socket_fd, entry_name, entry_name_length);
			read_from_socket(socket_fd, &version, sizeof(time_t));
			printf("THREAD: file_name: %s with version %ld was received.\n", entry_name, version);

			new_item.ip = received_item.ip;
			new_item.port = received_item.port;
			strcpy(new_item.path_name, entry_name);
			new_item.version = version;
			new_item.task_type = 1;

			sem_wait(&shared_data->emptyCount);
			pthread_mutex_lock(&shared_data->buffer_mutex);

			shared_data->task_buffer[shared_data->in] = new_item;
			shared_data->in = (shared_data->in + 1)%shared_data->buffer_size;

			pthread_mutex_unlock(&shared_data->buffer_mutex);			
			sem_post(&shared_data->fillCount);

			entry_name_length = 0;
			read_from_socket(socket_fd, &entry_name_length, sizeof(int));
		}
		printf("THREAD: GET_FILE_LIST request has ended for ip:%s - port: %hu\n", buffer, received_item.port);
		close(socket_fd);
	}
	else {
		socket_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (socket_fd == -1) {
			socket_error();
			exit(E_SOCKET);
		}
		memset(&client, 0, sizeof(struct sockaddr_in));
		client.sin_family = AF_INET;
		client.sin_addr.s_addr = htonl(received_item.ip);
		client.sin_port = htons(received_item.port);

		ret_val = connect(socket_fd, (struct sockaddr*)&client, sizeof(struct sockaddr_in));
		if (ret_val == -1) {
			connect_error(socket_fd);
			exit(E_CONNECT);
		}

		//send the request code
		int msg = htonl(GET_FILE);
		write_to_socket(socket_fd, &msg, sizeof(int));

		//send length of the file name
		entry_name_length = htonl(strlen(received_item.path_name) + 1);
		write_to_socket(socket_fd, &entry_name_length, sizeof(int));

		//send the file name
		write_to_socket(socket_fd, received_item.path_name, ntohl(entry_name_length));

		sprintf(client_name, "%s_%hu", buffer, received_item.port);
		if (strstr(received_item.path_name, "/") == 0) {
			sprintf(path, "%s/%s", client_name, received_item.path_name);
		}
		else {
			temp_buff = basename(received_item.path_name);
			sprintf(path, "%s/%s", client_name, temp_buff);
		}

		//send the version of the file
		if (stat(path, &statbuf) == 0) {
			version = statbuf.st_mtime;
			write_to_socket(socket_fd, &version, sizeof(time_t));
		}
		else {
			if (errno != ENOENT) {
				printf("stat error: %s.%s.\n", path, strerror(errno));
				return;
			}
			else {
				version = 0;
				write_to_socket(socket_fd, &version, sizeof(time_t));
			}
		}

		//receive the FILE_SIZE or FILE_NOT_FOUND or FILE_UP_TO_DATE code
		read_from_socket(socket_fd, &msg, sizeof(int));
		msg = ntohl(msg);
		if (msg == FILE_NOT_FOUND) {
			printf("THREAD: FILE_NOT_FOUND: %s\n", received_item.path_name);
			close(socket_fd);
			return;
		}
		else if (msg == FILE_UP_TO_DATE) {
			printf("THREAD: FILE_UP_TO_DATE: %s\n", received_item.path_name);
			close(socket_fd);
			return;
		}

		//receive length of the file
		read_from_socket(socket_fd, &file_size, sizeof(int));
		file_size = ntohl(file_size);

		receive_file(socket_fd, client_name, received_item.path_name, file_size, received_item.version, PIPE_SIZE);
		printf("THREAD: file %s was received from ip:%s - port: %hu\n", received_item.path_name,
				buffer, received_item.port);
		close(socket_fd);
	}
}

int receive_file(int socket_fd, char *client, char *file_name, int file_size, time_t version, 
			int pipe_size) {

	char buffer[pipe_size];
	char received_file_path[STRING_SIZE];
	int bytes, read_bytes, fd, ret_val;
	char *temp;
	struct utimbuf utimeStruct;

	mkdir(client,0777);

	if (strstr(file_name, "/") == 0) {
		sprintf(received_file_path, "%s/%s", client, file_name);
	}
	else {
		temp = basename(file_name);
		sprintf(received_file_path, "%s/%s", client, temp);
	}

	printf("THREAD: file: %s was created.\n", received_file_path);
	fd = creat(received_file_path, S_IRWXU);
	if (fd == -1 ) {
		printf("file create error:%s.%s.\n", received_file_path, strerror(errno));
		return E_CREATE_FILE;
	}

	read_bytes = 0;
	while(read_bytes < file_size) {
		if (file_size - read_bytes >= pipe_size) {
			bytes = read_from_socket(socket_fd, buffer, pipe_size);
		}
		else {
			bytes = read_from_socket(socket_fd, buffer, file_size - read_bytes);
		}
		ret_val = write_to_file(fd, buffer, bytes);
		read_bytes += bytes;
	}
	close(fd);
	utimeStruct.actime = version;
	utimeStruct.modtime = version;
	utime(received_file_path, &utimeStruct);
	return 0;
}

int write_to_file(int fd, void *buffer, int length) {
	int bytes;
	bytes = write(fd, buffer, length);
	if (bytes == -1){
		printf("erro in write to %d\n",fd);
		return -1;
	}
	return bytes;
}