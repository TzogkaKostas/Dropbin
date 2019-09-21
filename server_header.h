#define BACKLOG 32
#define STRING_SIZE 1024
#define TIME_OUT_DURATION -1
#define BUFFER_SIZE 1024
#define MAX_FDS 200

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


#include "structs.h"

void insert_client_to_list(Client_List *client_list, unsigned long ip, unsigned short port);
void send_all_clients(Client_List *client_list, int socket_fd, unsigned long ip, unsigned short port);
void send_user_on_message(int socket_fd);
void send_user_off_message(int socket_fd);
void send_client_info(Client_List *client_list, int fd, unsigned long ip, unsigned short port);
void broadcast_logged_on_client(Client_List *client_list, unsigned long ip, unsigned short port);
void broadcast_logged_off_client(Client_List *client_list, unsigned long ip, unsigned short port);
int delete_client_from_list(Client_List *client_list, unsigned long ip, unsigned short port);
void sig_int_handler(int signum);
void sig_chld_handler(int signum, siginfo_t *info, void *ucontext);
void set_sig_int_signal(struct sigaction *sig_action, void (*sig_int_handler)(int));
void set_sig_chld_signal(struct sigaction *sa, void (*sig_chld_handler)(int, siginfo_t*, void*));
int read_from_socket(int socket_fd, void *buffer,int length);
int write_to_socket(int socket_fd, void *buffer, int length);
int server_setup(struct sockaddr_in *server, unsigned short port_num, int *reuse_addr);
void set_non_blocking_socket(int socket);
void initialize_sigaction_structures(struct sigaction *sig_int_action);
void compress_array(struct pollfd fds[][MAX_FDS], int *nfds);
int handle_poll_event(struct pollfd fds[][MAX_FDS], int *nfds, int socket_fd, Client_List *client_list,
		int *compress_array_flag);
int handle_readable_fd(int fd, Client_List *client_list);
int handle_readable_listening_socket(int socket_fd, struct pollfd fds[][MAX_FDS], int i, int *nfds);
void close_all_fds(struct pollfd fds[][MAX_FDS], int nfds);
int recv_from_socket(int socket_fd, void *buffer, int length);
void print_my_ip();
void print_my_hostname();