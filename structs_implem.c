#include <stdio.h>
#include <stdlib.h>

#include "structs.h"

	//Client_List functions
void initialize_list(Client_List **client_list) {
	*client_list = (Client_List*)malloc(sizeof(Client_List));
	(*client_list)->head = NULL;
	(*client_list)->num_clients = 0;
}

Client_List_Node* insert(Client_List *client_list, unsigned long ip, unsigned short port) {
	Client_List_Node *temp;
	temp = (Client_List_Node*)malloc(sizeof(Client_List_Node));

	temp->next = get_head(client_list);
	temp->ip = ip;
	temp->port = port;
	set_head(client_list,temp);
	increase_num_clients(client_list);
	return temp;
}

void insert_request_to_list(Client_List *client_list, unsigned long ip, unsigned short port, int request) {
	Client_List_Node *client = search_node(client_list, ip, port);
	client->request = request;
}

void insert_request_to_list_by_fd(Client_List *client_list, int fd, int request) {
	Client_List_Node *client = search_node_by_fd(client_list, fd);
	client->request = request;
}

void insert_fd_to_list(Client_List *client_list, unsigned long ip, unsigned short port, int fd) {
	Client_List_Node *client = search_node(client_list, ip, port);
	client->fd = fd;
}

int delete(Client_List *client_list, unsigned long ip, unsigned short port) {
	Client_List_Node *cur, *prev;
	cur = get_head(client_list);
	if (cur == NULL) return -1;

	if (cur->ip == ip) {
		set_head(client_list,cur->next);
		free(cur);
		decrease_num_clients(client_list);
		return 0;
	}

	while (cur != NULL) {
		if (cur->ip == ip) {
			prev->next = cur->next;
			prev->next = cur->next;
			free(cur);
			decrease_num_clients(client_list);
			return 0;
		}
		else {
			prev = cur;
			cur = cur->next;
		}
	}
	return -1;
}

int exists(Client_List *client_list, unsigned long ip, unsigned short port) {
	if (search_node(client_list, ip, port) != NULL){
		return TRUE;
	}
	else {
		return FALSE;
	}
}

Client_List_Node* search_node(Client_List *client_list, unsigned long ip, unsigned short port) {
	Client_List_Node *temp = get_head(client_list);

	while(temp != NULL) {
		if (temp->ip == ip && temp->port == port) {
			return temp;
		}
		temp = get_next(temp);
	}
	return NULL;
}

Client_List_Node* search_node_by_fd(Client_List *client_list, int fd) {
	Client_List_Node *temp = get_head(client_list);
	while(temp != NULL) {
		if (temp->fd == fd) {
			return temp;
		}
		temp = get_next(temp);
	}
	return NULL;
}


void print(Client_List *client_list) {
	Client_List_Node *temp = get_head(client_list);
	while(temp != NULL) {
		print_info(temp);
		printf(" \n");
		temp = get_next(temp);
	}
	printf("\n");
}

void destroy(Client_List **client_list) {
	destroy_all_nodes(*client_list);
	free(*client_list);
}

void destroy_all_nodes(Client_List *client_list) {
	Client_List_Node *temp;
	while(get_head(client_list) != NULL) {
		temp = get_head(client_list);
		set_head(client_list, get_next(get_head(client_list)));
		free(temp);
	}
}

Client_List_Node* get_head(Client_List *client_list) {
	return client_list->head;
}

int get_num_clients(Client_List *client_list) {
	return client_list->num_clients;
}

void set_head(Client_List *client_list,Client_List_Node *client_list_node) {
	client_list->head = client_list_node;
}

void increase_num_clients(Client_List *client_list) {
	(client_list->num_clients)++;
}

void decrease_num_clients(Client_List *client_list) {
	(client_list->num_clients)--;
}


	//Client_List_Node functions
Client_List_Node* get_next(Client_List_Node *client_list_node) {
	return client_list_node->next;
}

void print_info(Client_List_Node *client_list_node) {
	printf("(ip:%ld, port:%hu)", client_list_node->ip, client_list_node->port);
}


	//Arguments functions
char* get_dir_name(Arguments *args) {
	return args->dir_name;
}

unsigned short get_port_num(Arguments *args) {
	return args->port_num;
}

int get_worker_threads(Arguments *args) {
	return args->worker_threads;
}

int get_buffer_size(Arguments *args) {
	return args->buffer_size;
}

unsigned short get_server_port(Arguments *args) {
	return args->server_port;
}

char* get_server_ip(Arguments *args) {
	return args->server_ip;
}