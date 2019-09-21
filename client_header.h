#include "structs.h"

#define BACKLOG 32
#define STRING_SIZE 1024
#define TIME_OUT_DURATION -1
#define MAX_FDS 200
#define BUFFER_SIZE 1024
#define TERMINATION_MESSAGE 00
#define PIPE_SIZE 1024


#define LOG_ON 1
#define GET_CLIENTS 2
#define LOG_OFF 3
#define CLIENT_LIST 4
#define USER_ON 5
#define USER_OFF 6
#define GET_FILE_LIST 7
#define GET_FILE 8
#define FILE_LIST 9
#define FILE_NOT_FOUND 10
#define FILE_UP_TO_DATE 11
#define FILE_SIZE 12
#define ERROR_IP_PORT_NOT_FOUND_IN_LIST 13
#define ERROR_IP_PORT_FOUND_IN_LIST 14

void read_command_line_arguments(int argc, char **argv, Arguments *args);
void send_log_on_message(int socket_fd, unsigned long ip, unsigned short port);
void send_log_off_message(Arguments *arguments);
void send_get_clients_message(int socket_fd);
void get_connected_clients(Client_List *client_list, int socket_fd);
int read_from_socket(int socket_fd, void *buffer, int length);
int write_to_socket(int socket_fd, void *buffer, int length);
void get_my_ip_adress(unsigned long *my_ip);
void print_my_ip();
void print_my_hostname();
int handle_poll_event(struct pollfd fds[][MAX_FDS], int *nfds, int socket_fd, Client_List *client_list,
		int *compress_array_flag, char *dir, Shared_Data *shared_data);
int handle_readable_listening_socket(int socket_fd, struct pollfd fds[][MAX_FDS], int i, int *nfds);
int handle_readable_fd(int fd, Client_List *client_list, char *dir, Shared_Data *shared_data);
void compress_array(struct pollfd fds[][MAX_FDS], int *nfds);
void close_all_fds(struct pollfd fds[][MAX_FDS], int nfds);
int server_setup(struct sockaddr_in *server, unsigned short port_num, int *reuse_addr);
void set_non_blocking_socket(int socket);
int recv_from_socket(int socket_fd, void *buffer, int length);
int get_info_from_dropbox_server(Client_List *client_list, Arguments *arguments);
void insert_client_to_list(Client_List *client_list, unsigned long ip, unsigned short port);
int delete_client_from_list(Client_List *client_list, unsigned long ip, unsigned short port);
void initialize_sigaction_structures(struct sigaction *sig_int_action);
void set_sig_int_signal(struct sigaction *sig_action, void (*sig_int_handler)(int));
void set_sig_chld_signal(struct sigaction *sa, void (*sig_chld_handler)(int, siginfo_t*, void*));
void sig_int_handler(int signum);
int send_all_file_names(int socket_fd, char *directory);
int send_name_length(int socket_fd, char *entry_name);
int send_name(int socket_fd, char *name);
void send_termination_message(int fifo_fd);
int cut_first_part_of_the_path(char *path, char *new_path);
void* thread_function(void *arg);
void handle_buffer_item(Item received_item, Shared_Data *shared_data);
int directory_exists(char *directory_name);
int file_exists(char *filename);
void fill_task_buffer_with_connected_clients(Client_List *client_list, Shared_Data *shared_data);
void print_buffer(Item *buffer);
int receive_file(int socket_fd, char *client, char *file_name, int file_size, time_t version, 
		int pipe_size);
int create_directory(char *directory);
int directory_exists(char *directory_name);
int write_to_file(int fd, void *buffer, int length);
void send_file(int fifo_fd, char *file_name, int pipe_size);
int get_entry_size(char *entry_name);
int get_entry_size_by_fd(int fd);
int read_from_file(int fd, void *buffer,int length);
int send_version(int socket_fd, char *path);
int remove_slash(char *string);
