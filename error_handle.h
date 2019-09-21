#define E_ARGUMENT 1
#define E_SOCKET 2
#define E_LISTEN 3
#define E_ACCEPT 4
#define E_GETHOSTBYADDR 5
#define E_FORK 6
#define E_READ 7
#define E_SIGNAL 8
#define E_CONNECT 9
#define E_WRITE 10
#define E_BIND 11
#define E_FCNTL 12
#define E_POLL 13
#define E_OPEN_DIR 14
#define E_CREATE_DIR 15
#define E_CREATE_FILE 16

void server_argument_error(char *program_name);
void client_argument_error(char *program_name);
void socket_error();
void listen_error(int socket_fd);
void accept_error(int socket_fd);
void gethostbyaddr_error(int addr);
void fork_error();
void read_error(int socket_fd);
void signal_error();
void connect_error(int socket_fd);
void write_error(int socket_fd);
void bind_error(int socket_fd);
void fcntl_error(int socket_fd);
void poll_error();
void revents_error(int revent);
void opendir_error(char *name);