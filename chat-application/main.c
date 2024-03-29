#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <ifaddrs.h>
#include <sys/stat.h>
#include <sys/types.h>


#define MAX_CLIENT              50
#define BUFF_SIZE               125


pthread_t recieve_thr[MAX_CLIENT], create_listen_thread, connect_thr, recieve_thr_client[MAX_CLIENT];
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;


int master_socket, client_socket[MAX_CLIENT], connected_socket[MAX_CLIENT];
int pipe_client_empty[2];
int pipe_server_empty[2];
int temp_client = 0;
int temp_server = 0;

int num_of_client_empty = 0;
int num_of_server_empty = 0;

char *command_name[] = {"help", "myip", "myport", "connect", "list", "terminate", "send", "exit"};

struct sockaddr_in server_address, client_address[MAX_CLIENT], connected_address[MAX_CLIENT];

bool list_check = false;

int quan_var_envr(char *cmd)
{
    int num = 0;

    if (*cmd == '\0') {
        return num;
    }else num++;
    
    int i = 0;

    while (cmd[i] != '\0')
    {
        if ((cmd[i] == ' ') && (cmd[i + 1] != '\0')) {
            num++;
        }
        i++;
    }

    return num;
}

void slipt_string(char *src, char *dest, const char *begin, const char *end, int offset)
{
    int i, j = 0;
    char buff[100];
    char *result;
    strcpy(buff, src);

    if (offset == 0)
    {
        result = strtok(buff, end);
        strcpy(dest, result);
    }else{
        result = strtok(buff, begin);
        offset -= 1;
        while(offset)
        {
            result = strtok(NULL, begin);
            offset -= 1;
        }

        result = strtok(NULL, end);
        strcpy(dest, result);
    }
    
    return;
}

int get_type_cmd(char *cmd){
    for(int i = 0; i < 8; i++)
    {
        if (strcmp(command_name[i], cmd) == 0) {
            return i+1;
        }
    }
    return -1;
}

void *listen_mess_from_client(void *args)
{
    int opt, num_read, num_write;
    char buff[BUFF_SIZE];
    char address[BUFF_SIZE];
    char id_empty[10];

    opt = *((int *)args);
    inet_ntop(AF_INET, &(client_address[opt].sin_addr), address, sizeof(address));
    
    list_check = false;

    pthread_detach(pthread_self());
    printf("Connect with client: %s at port: %d\n", address, htons(client_address[opt].sin_port));
    while (1)
    {
        num_read = read(client_socket[opt], buff, sizeof(buff));
        if (strcmp(buff, "exit") == 0) {
            close(client_socket[opt]);
            client_socket[opt] = -1;
            sprintf(id_empty, "%d", opt);
            num_write = write(pipe_client_empty[1], id_empty, strlen(id_empty));
            num_of_client_empty++;
            printf("\nDisconnect at ip: %s, port: %d\n", address, htons(client_address[opt].sin_port));
            pthread_exit(NULL);
        }

        printf("\nMessage received from: %s\n", address);
        printf("Sender's Port: %d\n", htons(client_address[opt].sin_port));
        printf("Message: %s\n", buff);
    }
}

void *listen_mess_from_connected(void *args)
{
    int opt, num_read, num_write;
    char buff[BUFF_SIZE];
    char address[BUFF_SIZE];
    char id_empty[10];
    
    opt = *((int *)args);
    inet_ntop(AF_INET, &(connected_address[opt].sin_addr), address, sizeof(address));

    list_check = false;
    
    pthread_detach(pthread_self());
    while (1)
    {
        num_read = read(connected_socket[opt], buff, sizeof(buff));
        if (strcmp(buff, "exit") == 0) {
            close(connected_socket[opt]);
            connected_socket[opt] = -1;
            sprintf(id_empty, "%d", opt);
            num_write = write(pipe_server_empty[1], id_empty, strlen(id_empty));
            num_of_server_empty++;
            printf("\nDisconnect at ip: %s, port: %d\n", address, htons(connected_address[opt].sin_port));
            pthread_exit(NULL);
        }

        printf("\nMessage received from: %s\n", address);
        printf("Sender's Port: %d\n", ntohs(connected_address[opt].sin_port));
        printf("Message: %s\n", buff);
    }
}

void print_help(void)
{
    printf("Chat function:\n");
    printf("# myip : display IP address.\n");
    printf("# myport : display the port.\n");
    printf("# connect <destination> <port no> : connect to server with IP address (destination) and port(port no).\n");
    printf("# list : display numbered list of all the connections.\n");
    printf("# terminate <connection id> : terminate the connection listed.\n");
    printf("# send <connection id> <message> : send a message to a client or server with id.\n");
    printf("# exit : close all connection.\n");
}

void print_myip(void)
{
    struct ifaddrs *addrs, *tmp;

    if (getifaddrs(&addrs) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    for (tmp = addrs; tmp != NULL; tmp = tmp->ifa_next) {
        if (tmp->ifa_addr != NULL && tmp->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *pAddr = (struct sockaddr_in *)tmp->ifa_addr;
            if (strcmp(tmp->ifa_name, "ens33") == 0)
            {
                printf("My IP: %s\n", inet_ntoa(pAddr->sin_addr));
            }
        }
    }

    freeifaddrs(addrs);
    return;
}

void print_myport(struct sockaddr_in server_address)
{
    printf("My port: %d\n", ntohs(server_address.sin_port));
    return;
}

void connect_handle(char *cur_cmd, int *cur_server)
{
    char ip[100];
    char port_num[10], id_empty[10];
    int port_no_connect;

    if (quan_var_envr(cur_cmd) < 3)
    {
        printf("%s: command not found\n", cur_cmd);
        printf("This you mean: connect <ip> <port number>\n");
        return;
    }
    
    slipt_string(cur_cmd, ip, " ", " ", 1);
    slipt_string(cur_cmd, port_num, " ", "\n", 2);
    port_no_connect = atoi(port_num);
    
    if (num_of_server_empty == 0) {
        temp_server = *cur_server;
        (*cur_server)++;
    }else {
        temp_server = read(pipe_server_empty[0], id_empty, sizeof(id_empty));
        temp_server = atoi(id_empty);
        num_of_server_empty -= 1;
    }

    connected_address[temp_server].sin_family = AF_INET;
    connected_address[temp_server].sin_port = htons(port_no_connect);
    if (inet_pton(AF_INET, ip, &connected_address[temp_server].sin_addr) == -1) {
        perror("inet_pton()");
        return;
    }

    connected_socket[temp_server] = socket(AF_INET, SOCK_STREAM, 0);

    if (connected_socket[temp_server] == -1)    perror("socket()");

    if (connect(connected_socket[temp_server], (struct sockaddr*)(&connected_address[temp_server]), sizeof(connected_address[temp_server])) == -1)
    {
        perror("connect()");
        return;
    }
    
    int ret = pthread_create(&recieve_thr_client[temp_server], NULL, &listen_mess_from_connected, (void*)&temp_server);
    
    if (ret == -1) {
        perror("pthread_create()");
        return;
    }
    printf("Connect successfully with host: %s, port: %d\n", ip, port_no_connect);
    return;
}

void print_list(int *cur_client, int *cur_server)
{
    char buff[100];
    printf("id: IP_address                  Port No.\n");
    for (int i = 0; i < *cur_client; i++)
    {
        inet_ntop(AF_INET, &(client_address[i].sin_addr), buff, sizeof(buff));
        printf("%d   %s              %d",i+1 ,buff, ntohs(client_address[i].sin_port));
        if (client_socket[i] == -1) {
            printf("(terminated)");
        }
        printf("\n");
    }

    for (int i = 0; i < *cur_server; i++)
    {
        inet_ntop(AF_INET, &(connected_address[i].sin_addr), buff, sizeof(buff));
        printf("%d   %s              %d",i+ *cur_client +1 ,buff, ntohs(connected_address[i].sin_port));
        if (connected_socket[i] == -1) {
            printf("(terminated)");
        }
        printf("\n");

    }
    list_check = true;
}

void terminate_handle(char *cur_cmd, int *cur_client)
{
    char buff[10];
    char exit_mess[10];
    char id_empty[10];
    int opt;
    int num_write;

    if (quan_var_envr(cur_cmd) < 2) {
        printf("%s: command not found\n", cur_cmd);
        printf("This you mean: terminate <id>\n");
        return;
    }

    if (!list_check)
    {
        printf("Please call : list to check\n");
        return;
    }

    slipt_string(cur_cmd, buff, " ", "\n", 1);
    opt = atoi(buff);
    strcpy(exit_mess, "exit");

    if (opt <= *cur_client)
    {
        pthread_cancel(recieve_thr[opt - 1]);

        if ((num_write = write(client_socket[opt-1], exit_mess, strlen(exit_mess) + 1)) == -1) {
            perror("write()");
            return;
        }

        close(client_socket[opt - 1]);
        client_socket[opt - 1] = -1;
        sprintf(id_empty, "%d", (opt - 1));
        
        num_write = write(pipe_client_empty[1], id_empty, strlen(id_empty));
        num_of_client_empty++;
        printf("Terminated channel %d completed.\n", opt);
        list_check = false;
        return;
    }else {
        pthread_cancel(recieve_thr_client[opt - *cur_client -1]);

        if ((num_write = write(connected_socket[opt - *cur_client -1], exit_mess, strlen(exit_mess) + 1)) == -1) {
            perror("write()");
            return;
        }

        close(connected_socket[opt - *cur_client -1]);
        connected_socket[opt - *cur_client - 1] = -1;
        sprintf(id_empty, "%d", (opt - *cur_client - 1));
        
        num_write = write(pipe_server_empty[1], id_empty, strlen(id_empty));
        num_of_server_empty++;
        printf("Terminated channel %d completed.\n", opt);
        list_check = false;
        return;
    }
}

void send_handle(char *cur_cmd, int *cur_client, int *cur_server)
{
    if (!list_check)   
    {
        printf("Please call : list to check.\n");
        return;
    }

    if (quan_var_envr(cur_cmd) < 3)
    {
        printf("%s: command not found\n", cur_cmd);
        printf("This you mean: send <id> <message>\n");
        return;
    }

    char buff[100];
    char address[10];
    int num_channel;
    int num_write;

    slipt_string(cur_cmd, address, " ", " ", 1);
    slipt_string(cur_cmd, buff, " ", "\n", 2);
    
    num_channel = atoi(address);

    if (num_channel > *cur_client) {
        num_channel = num_channel - *cur_client - 1;
        for (int i = 0; i < *cur_server; i++)
        {
            if (num_channel == i) {
                num_write = write(connected_socket[i], buff, strlen(buff));
                if (num_write == -1)    perror("write()");
                return;
            }
        }
        return;
    }
    
    num_channel -= 1;
    for (int i = 0; i < *cur_client; i++)
    {
        if (num_channel == i) {
            num_write = write(client_socket[i], buff, strlen(buff));
            if (num_write == -1)    perror("write()");
            return;
        }
    }
}

void exit_handle(int *cur_client, int *cur_server)
{
    char exit_mess[] = "exit";
    int num_write;

    for (int i = 0; i < *cur_client; i++)
    {
        pthread_cancel(recieve_thr[i]);
        num_write = write(client_socket[i], exit_mess, strlen(exit_mess) + 1);
        close(client_socket[i]);
    }

    for (int i = 0; i < *cur_server; i++)
    {
        pthread_cancel(recieve_thr_client[i]);
        num_write = write(connected_socket[i], exit_mess, strlen(exit_mess) + 1);
        close(connected_socket[i]);
    }

    close(pipe_client_empty[0]);
    close(pipe_client_empty[1]);
    close(pipe_server_empty[0]);
    close(pipe_server_empty[1]);
    close(master_socket);

    printf("Goodbye!!!\n");
    
    return;
}

void *connect_create(void *args)
{
    int len, num_read;
    char id_empty[10];
    int opt;
    int temp = -1;
    struct stat pipe_client;
    pthread_detach(pthread_self());
    while (1)
    {
        temp = *((int *)args);

        if (num_of_client_empty == 0) {
            opt = temp;
            temp += 1;
        }else{
            num_read = read(pipe_client_empty[0], id_empty, sizeof(id_empty));
            opt = atoi(id_empty);
            num_of_client_empty -= 1;
        }

        len = sizeof(client_address[opt]);
        
        client_socket[opt] = accept(master_socket, (struct sockaddr*)(&(client_address[opt])), (socklen_t*)&len);
        
        len = sizeof(client_address[opt]);
        if (getsockname(client_socket[opt], (struct sockaddr*)&(client_address[opt]), (socklen_t *)&len) == -1) {
            perror("getsockname()");
        }
        temp_client = opt;
        pthread_cond_signal(&cond1);
        memcpy(args, &temp, sizeof(temp));
    }
}



void *create_listen_message(void *args)
{
    int opt, ret;
    pthread_detach(pthread_self());
    while(1)
    {
        pthread_mutex_lock(&lock1);
        pthread_cond_wait(&cond1, &lock1);
        
        opt = *((int *)args);
        ret = pthread_create(&recieve_thr[opt], NULL, &listen_mess_from_client, args);

        if (ret == -1) {
            perror("pthread_create()");
        }
        
        pthread_mutex_unlock(&lock1);
    }
}


int main(int argc, char const *argv[])
{
    int port_no, opt, ret;
    int cur_client = 0;
    int cur_server = 0;
    
    char buffer[BUFF_SIZE], type_buff[BUFF_SIZE];
    

    if (argc < 2) {
        printf("No port provided.\nCommand: ./chat <port number> \n");
        exit(EXIT_FAILURE);
    }else {
        port_no = atoi(argv[1]);
    }

    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    memset(&server_address, 0, sizeof(struct sockaddr_in));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port_no);

    if (bind(master_socket, (struct sockaddr *)(&server_address), sizeof(server_address)) == -1) {
        perror("bind()");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1) {
        perror("setsockopt()");
        exit(EXIT_FAILURE);
    }

    if (listen(master_socket, MAX_CLIENT) == -1) {
        perror("listen()");
        exit(EXIT_FAILURE);
    }
    
    if (pipe(pipe_client_empty) < 0)
    {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }

    if (pipe(pipe_server_empty) < 0) {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&connect_thr, NULL, &connect_create, (void *)(&cur_client));
    if (ret == -1)
    {
        perror("pthread_create()");
        exit(EXIT_FAILURE);
    }

    ret = pthread_create(&create_listen_thread, NULL, &create_listen_message, (void *)(&temp_client));
    if (ret == -1)
    {
        perror("pthread_create()");
        exit(EXIT_FAILURE);
    }

    
    while (1)
    {
        printf("Enter your command: ");
        fgets(buffer, BUFF_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';
        slipt_string(buffer, type_buff, " ", " ", 0);
        
        opt = get_type_cmd(type_buff);

        switch (opt) {
        case 1:
            print_help();
            break;
        case 2:
            print_myip();
            break;
        case 3:
            print_myport(server_address);
            break;
        case 4:
            connect_handle(buffer, &cur_server);
            break;
        case 5:
            print_list(&cur_client, &cur_server);
            break;
        case 6:
            terminate_handle(buffer, &cur_client);
            break;
        case 7:
            send_handle(buffer, &cur_client, &cur_server);
            break;
        case 8:
            exit_handle(&cur_client, &cur_server);
            pthread_cancel(connect_thr);
            pthread_cancel(create_listen_thread);
            exit(EXIT_SUCCESS);
            break;
        default:
            printf("%s: command not found\n", buffer);
            printf("<help> : for more information.\n");
            break;
        }
    }

    return 0;
}
