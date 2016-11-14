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
#include <sys/wait.h>

#define MAXLINE 4096
#define MAXDATASIZE 100

//Funcao para tratar processos zumbis:

void sig_child(int signo){
  pid_t pid;
  int stat;
  while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
    printf("child %d terminated\n", pid);
  }
  return;
}

//Handler do sinal SIGCHILD

typedef void Sigfunc(int);

Sigfunc * Signal (int signo, Sigfunc *func){
  struct sigaction act, oact;
  act.sa_handler = func;
  sigemptyset (&act.sa_mask); /* Outros sinais não são bloqueados*/
  act.sa_flags = 0;
  if (signo == SIGALRM) { /* Para reiniciar chamadas interrompidas */
  #ifdef SA_INTERRUPT
    act.sa_flags |= SA_INTERRUPT; /* SunOS 4.x */
  #endif
  } else {
    #ifdef SA_RESTART
    act.sa_flags |= SA_RESTART; /* SVR4, 4.4BSD */
    #endif
  }
  if (sigaction (signo, &act, &oact) < 0)
    return (SIG_ERR);
  return (oact.sa_handler);
}

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
void Listen(int listenfd, int listenQ)
{
  if (listen(listenfd, listenQ) == -1) { 
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

  if (argc != 3) { //caso o usuário não tenha fornecido um IP e a porta para conexão
    strcpy(error,"uso: ");
    strcat(error,argv[0]);
    strcat(error," <Porta> <listen Backlog value");
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
  Listen(listenfd, atoi(argv[2]));
  //Registra funcao de handler
  Signal(SIGSHLD, sig_child);
  
  //Prepara arquivo de log

  time_t now;
  char timeString[256];
  FILE *log;
  
  while (1){
    //Retarda a remoção dos sockets da fila de conexões completas
    sleep(10);
    //Aceita uma conexao e atribui a um socket
    if ((connfd = Accept(listenfd, &clientaddr, &addrlen)) < 0){
      if (errno = EINTR){
	continue;
      }
      else{
	err_sys("accept error");
      }
    }

    //Registra conexao
    
    time(&now);
    strftime (timeString, 256, "%Y-%m-%d %H:%M:%S", localtime(&now));

    log = fopen("log.txt","a");
    if (log == NULL){
      printf ("Failed to log entry.\n");
    }
    else{
      fprintf(log,"[%s] %s:%u conectou-se\n",
	      timeString,
	      inet_ntoa(clientaddr.sin_addr),
	      ntohs(clientaddr.sin_port));
    }
    fclose(log);


    // Cria subprocesso que lidara com essa conexao
    
    if ((child_pid = fork()) == 0) { //Processo filho
      //Fecha socket para ouvir novas conexoes
      close(listenfd);
      //Imprime o endereço do socket local
      getsockname(connfd,(struct sockaddr *)&servaddr, &addrlen);
      printf("Endereço local: %s:%u \n",inet_ntoa(servaddr.sin_addr), ntohs(servaddr.sin_port));
     
      //Imprime o endereço do socket remoto
      printf("Endereço do cliente: %s:%u \n",
	     inet_ntoa(clientaddr.sin_addr),
	     ntohs(clientaddr.sin_port));
      
      while(1) {
        //Executa comandos enviados pelo cliente
	n = read(connfd, recvline, MAXLINE);

	if (n == 0){
	  // socket disconnected
	  break;
	}

	if (strcmp(recvline,"bye\n") == 0){
	  break;
	}
	else {
	  char outputbuffer[MAXLINE + 1];

	  recvline[n] = 0;
	  
	  FILE* p = popen(recvline,"r");
	
	  if (p == NULL){
	    printf("Erro executando comando.\n Valor de retorno: %d\n",
		   pclose(p) / 256);
	  }
	  else{

	    sprintf(outputbuffer, "(%s:%u) %s",
		    inet_ntoa(clientaddr.sin_addr),
		    ntohs(clientaddr.sin_port),
		    recvline);

	    printf("%s",outputbuffer);
	    	    
	    while (fgets(outputbuffer,MAXLINE+1,p)){
	      //printf("%s",outputbuffer);
	      write(connfd, outputbuffer, strlen(outputbuffer));
	    }
	    pclose(p);
	  }
	}
      }

      
      // Registra desconexao

      time(&now);
      strftime (timeString, 256, "%Y-%m-%d %H:%M:%S", localtime(&now));

      
      log = fopen("log.txt","a");
      if (log == NULL){
	printf ("Falha no registro\n");
      }
      else{
	fprintf(log,"[%s] %s:%u desconectou-se\n",
		timeString,
		inet_ntoa(clientaddr.sin_addr),
		ntohs(clientaddr.sin_port));
      }
      fclose(log);


      
      
      //Fecha socket da conexao

      printf("Cliente %s:%u desconectou-se.\n",
	     inet_ntoa(clientaddr.sin_addr),
	     ntohs(clientaddr.sin_port));

     
      close(connfd);	
      return(0);
    }
    printf("Numero do processo filho: %d\n", child_pid);
    close(connfd);
  }
}
