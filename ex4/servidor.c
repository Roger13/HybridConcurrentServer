#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAXLINE 4096
#define LISTENQ 10
#define MAXDATASIZE 100

//Cria e retorna um socket descriptor
int Socket(int family, int type, int flags)
{
  int sockfd;
  if ((sockfd = socket(family, type, flags)) < 0) {
    perror("socket");
    exit(1);
  } else
    return sockfd;
}

//Preenche dados referentes ao endereco do servidor
void Servaddr(struct sockaddr_in* servaddr, char* port)
{
  bzero(servaddr, sizeof(*servaddr)); //zera os bits da estrutura com o endereço do servidor
  servaddr->sin_family = AF_INET; //escolhe o protocolo
  servaddr->sin_addr.s_addr = htonl(INADDR_ANY); //endereço IP
  servaddr->sin_port   = htons(atoi(port)); //determina a porta e converte para o padrão big-endian
}

//Associa o socket com um IP e porta
void Bind(int listenfd, struct sockaddr_in* servaddr) 
{
  if (bind(listenfd, (struct sockaddr *)servaddr, sizeof(*servaddr)) == -1) {       	 perror("bind");
    exit(1);
  }
}

//Diz para o socket ouvir conexoes
void Listen(int listenfd)
{
  if (listen(listenfd, LISTENQ) == -1) { 
    perror("listen");
    exit(1);
  }
}

//Aceita uma conexao e atribui a um socket
int Accept(int listenfd, struct sockaddr_in* clientaddr, socklen_t* addrlen) 
{
  int connfd;
  if ((connfd = accept(listenfd, (struct sockaddr *)clientaddr, addrlen)) == -1 ) { //Aceita uma conexao vindo de um listening socket
    perror("accept");
    exit(1);
  }
  return connfd;
}

int main (int argc, char **argv) {
  int    listenfd, connfd, n;
  struct sockaddr_in servaddr, clientaddr;
  
  //char   buf[MAXDATASIZE];
  char   recvline[MAXLINE + 1];
  
  //time_t ticks;
  char   error[MAXLINE + 1];
  
  //para obter o endereço do socket
  socklen_t addrlen = sizeof(clientaddr);

  //valor temporario para o id dos processos filhos
  pid_t child_pid;

  if (argc != 2) { //caso o usuário não tenha fornecido um IP e a porta para conexão
    strcpy(error,"uso: ");
    strcat(error,argv[0]);
    strcat(error," <Porta>");
    perror(error);
    exit(1);
  }
   
  //Inicializa o socket
  listenfd = Socket(AF_INET, SOCK_STREAM, 0);
  //Preenche informacoes relativas ao socket do servidor
  Servaddr(&servaddr, argv[1]);
  //Associa socket com um ip e uma porta
  Bind(listenfd, &servaddr);
  //Diz para o socket ouvir conexoes
  Listen(listenfd);
  while (1){
    //Aceita uma conexao e atribui a um socket
    connfd = Accept(listenfd, &clientaddr, &addrlen);
    
    if ((child_pid = fork()) == 0) { //Processo filho
      //Fecha socket para ouvir novas conexoes
      close(listenfd);
      //Imprime o endereço do socket local
      getsockname(listenfd,(struct sockaddr *)&servaddr, &addrlen);
      printf("Endereço local: %s:%u \n",inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
     
      //Imprime o endereço do socket remoto
      printf("Endereço do cliente: %s:%u \n",
	     inet_ntoa(clientaddr.sin_addr),
	     ntohs(clientaddr.sin_port));
      
      while(1) {
        //Executa comandos enviados pelo cliente
	n = read(connfd, recvline, MAXLINE);
	recvline[n] = 0;
	int r = system(recvline);
	if(r){
	  printf("Return value: %d\n", r);
	};
       
	write(connfd, recvline, strlen(recvline));	
      }
      //Fecha socket da conexao
      close(connfd);	
      return(0);
    }
    printf("Numero do processo filho: %d\n", child_pid);
    close(connfd);
  }
}
