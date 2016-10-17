#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
//para imprimir o endereço
#include <stdint.h>
#define MAXLINE 4096

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
void Servaddr(struct sockaddr_in* servaddr, char* ip, char* port)
	{	
	bzero(servaddr, sizeof(*servaddr)); //zera os bits da estrutura com o endereço do servidor
   	servaddr->sin_family = AF_INET; //escolhe o protocolo
   	servaddr->sin_port   = htons(atoi(port)); //determina a porta e converte para o padrão big-endian
   	if (inet_pton(AF_INET, ip, &servaddr->sin_addr) <= 0) {
     	 perror("inet_pton error");
     	 exit(1);
   	}
	}

//Conecta o socket ao servidor
void Connect(int sockfd, struct sockaddr_in * servaddr)
	{
	if (connect(sockfd, (struct sockaddr *) servaddr, sizeof(*servaddr)) < 0) { //caso não consiga se conectar com o servidor
     	 perror("connect error");
     	 exit(1);
	}
	
	}

int main(int argc, char **argv) {
   int    sockfd, n;
   char   recvline[MAXLINE + 1];
   char	  sendline[MAXLINE + 1];
   char   error[MAXLINE + 1];
   struct sockaddr_in servaddr;
   //Para obter o endereço do socket
   socklen_t addrlen = sizeof(servaddr);
			
   if (argc != 3) { //Caso o usuário não tenha fornecido um IP e a porta para conexão
      strcpy(error,"uso: ");
      strcat(error,argv[0]);
      strcat(error," <IPaddress> <Port>");
      perror(error);
      exit(1);
   }
   
   //Inicializa o socket
   sockfd = Socket(AF_INET, SOCK_STREAM, 0);
   //Preenche informacoes relativas ao socket do servidor
   Servaddr(&servaddr, argv[1], argv[2]);
   //Conecta o socket ao servidor
   Connect(sockfd, &servaddr);
   
   //Imprime o endereço do socket local
   getsockname(sockfd,(struct sockaddr *)&servaddr, &addrlen);
   printf("Endereço local: %s:%u \n", inet_ntoa(servaddr.sin_addr),ntohs(servaddr.sin_port));
 
   //Imprime o endereço do socket remoto
   getpeername(sockfd,(struct sockaddr *)&servaddr, &addrlen);
   printf("Endereço do host: %s:%u \n",inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
            
              
   //Le entrada padrao e envia para servidor continuamente
   for ( ; ; ) {	
      fgets (sendline, MAXLINE - 1, stdin);
      write (sockfd, sendline, strlen(sendline));
	
      n = read(sockfd, recvline, MAXLINE);				
      recvline[n] = 0;
      printf("%s", recvline);      
	
      if (n < 0) { //caso o número de bits lido seja negativo, lança erro
         perror("read error");
         exit(1);
      }
   }	
   exit(0);
}
