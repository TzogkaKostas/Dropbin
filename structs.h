#include <netinet/in.h>
#include <stdint.h>
#include <semaphore.h>
#include <pthread.h>


#define STRING_SIZE 1024
#define PATH_SIZE 128


#define TRUE 1
#define FALSE 0

typedef struct ListNode_struct {
	unsigned long ip;
	unsigned short port;
	int request, fd;
	struct ListNode_struct *next;
}Client_List_Node;

typedef struct List_struct {
	Client_List_Node *head;
	int num_clients;
}Client_List;

typedef struct {
	char dir_name[STRING_SIZE], server_ip[STRING_SIZE];
	unsigned short port_num, server_port;
	int worker_threads, buffer_size;
}Arguments;

typedef struct Item_struct{
	char path_name[PATH_SIZE];
	int task_type;
	time_t version;
	unsigned long ip;
	unsigned short port;
}Item;

typedef struct Shared_struct{
	pthread_mutex_t buffer_mutex;
	pthread_mutex_t client_mutex;
	sem_t fillCount;
	sem_t emptyCount;
	int in, out;
	int buffer_size;
	Item *task_buffer;
	Client_List *client_list;
}Shared_Data;

char* get_dir_name(Arguments *args);
unsigned short get_port_num(Arguments *args);
int get_worker_threads(Arguments *args);
int get_buffer_size(Arguments *args);
unsigned short get_server_port(Arguments *args);
char* get_server_ip(Arguments *args);

	//Client_List functions
void initialize_list(Client_List **client_list);
Client_List_Node* insert(Client_List *client_list, unsigned long ip, unsigned short port);
void insert_request_to_list(Client_List *client_list, unsigned long ip, unsigned short port, int request);
void insert_request_to_list_by_fd(Client_List *client_list, int fd, int request);
void insert_fd_to_list(Client_List *client_list, unsigned long ip, unsigned short port, int fd);
int delete(Client_List *client_list, unsigned long ip, unsigned short port);
int exists(Client_List *client_list, unsigned long ip, unsigned short port);
Client_List_Node* search_node(Client_List *client_list, unsigned long ip, unsigned short port);
Client_List_Node* search_node_by_fd(Client_List *client_list, int fd);
void print(Client_List *client_list);
void print_info(Client_List_Node *client_list_node);
void destroy(Client_List **client_list);
void destroy_all_nodes(Client_List *client_list);
Client_List_Node* get_head(Client_List *client_list);
int get_num_clients(Client_List *client_list);
void set_head(Client_List *client_list,Client_List_Node *client_list_node);
void increase_num_clients(Client_List *client_list);
void decrease_num_clients(Client_List *client_list);


	//Client_List_Node functions
Client_List_Node* get_next(Client_List_Node *client_list_node);