//SSL-Client.c
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
 
#define FAIL    -1
 
void registrationrequest(int localport, char* proxyhost, int proxyport ){
   int sockfd;
   struct sockaddr_in local_addr, proxy_addr;
   struct hostent *server;
   
   char buffer[1024];
   
   /* Create a socket point */
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   
   if (sockfd < 0){
      perror("ERROR opening socket");
      exit(1);
   }
     server = gethostbyname(proxyhost);

    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(localport);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    int yes = 1;
    if ( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ){
     	perror("setsockopt");
	exit(1);
    }
    if(bind(sockfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr)) == -1){
	perror("bind");
	exit(1);
    }
   
   bzero((char *) &proxy_addr, sizeof(proxy_addr));
   proxy_addr.sin_family = AF_INET;
   bcopy((char *)server->h_addr, (char *)&proxy_addr.sin_addr.s_addr, server->h_length);
   proxy_addr.sin_port = htons(proxyport);
	
   char outbuf[4];
   bzero(outbuf, 4);
   outbuf[0] = '0';
   outbuf[1] = '0';
   bzero(buffer,1024);
   	/* Now connect to the server */
   if (connect(sockfd, (struct sockaddr*)&proxy_addr, sizeof(proxy_addr)) < 0){
    	  perror("ERROR connecting to proxy");
    	  exit(1);
   	}
   
   /* Now ask for a message from the user, this message
   * will be read by server
   */

   
   /* Send message to the server */
   if(write(sockfd, outbuf, 4)<0){
	perror("Socket write error");
	exit(1);
   }
   
   /* Now read server response */
   if(read(sockfd, buffer, 1023)<0){
      perror("Socket write error");
      exit(1);
   }
   printf("Received message from Proxy: %s\n",buffer);
   close(sockfd);
}

int OpenConnection(const char *hostname, int clientport, int serverport)
{   int sd;
    struct hostent *host;
    struct sockaddr_in serv_addr, cli_addr;
 
    if ( (host = gethostbyname(hostname)) == NULL )
    {
        perror(hostname);
        exit(1);
    }
    sd = socket(PF_INET, SOCK_STREAM, 0);

    bzero(&serv_addr, sizeof(serv_addr));
    
    memset(&cli_addr, 0, sizeof(struct sockaddr_in));
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_port = htons(clientport);
    cli_addr.sin_addr.s_addr = INADDR_ANY;
    
    int yes = 1;
    if ( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 ){
     	perror("setsockopt");
	exit(1);
    }

    if(bind(sd, (struct sockaddr *)&cli_addr, sizeof(struct sockaddr)) == -1){
	perror("bind");
	exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverport);
    serv_addr.sin_addr.s_addr = *(long*)(host->h_addr);
    if ( connect(sd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0 )
    {
        close(sd);
        perror(hostname);
        abort();
    }
    return sd;
}
 
SSL_CTX* InitCTX(void)
{   SSL_METHOD *method;
    SSL_CTX *ctx;
 
    OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
    SSL_load_error_strings();   /* Bring in and register error messages */
    method = SSLv3_client_method();  /* Create new client-method instance */
    ctx = SSL_CTX_new(method);   /* Create new context */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        abort();
    }
    return ctx;
}
 
void ShowCerts(SSL* ssl)
{   X509 *cert;
    char *line;
 
    cert = SSL_get_peer_certificate(ssl); /* get the server's certificate */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);       /* free the malloc'ed string */
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);       /* free the malloc'ed string */
        X509_free(cert);     /* free the malloc'ed certificate copy */
    }
    else
        printf("No certificates.\n");
}
 
int main(int argc, char *argv[])
{   SSL_CTX *ctx;
    int server, localport, proxyport;
    SSL *ssl;
    char buf[1024];
    int bytes;
    char *proxyhost;
 
    if ( argc <3 )
    {
        printf("usage: localport proxyhost proxyport \n");
        exit(0);
    }
    SSL_library_init();
    localport=atoi(argv[1]);
    proxyhost=argv[2];
    proxyport = atoi(argv[3]);
    registrationrequest(localport, proxyhost, proxyport);

    while(1){
    char msg[1024];
    bzero(msg,1024);
    printf("Here is your message: ");
    fgets(msg,1024,stdin);

    ctx = InitCTX();
    server = OpenConnection(proxyhost, localport, proxyport);
    ssl = SSL_new(ctx);      /* create new SSL connection state */
    SSL_set_fd(ssl, server);    /* attach the socket descriptor */
    if ( SSL_connect(ssl) == FAIL )   /* perform the connection */
        ERR_print_errors_fp(stderr);
    else
    {   
 
        printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
        ShowCerts(ssl);        /* get any certs */
        SSL_write(ssl, msg, sizeof(msg));   /* encrypt & send message */
        bytes = SSL_read(ssl, buf, sizeof(buf)); /* get reply & decrypt */
        buf[bytes] = 0;
        printf("Received message from Server: %s\n", buf);
        SSL_free(ssl);        /* release connection state */
    }
    sleep(1);
    close(server);         /* close socket */
    SSL_CTX_free(ctx);        /* release context */
    }
    return 0;
}
