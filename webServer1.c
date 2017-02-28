/*
NAME:        webServer1
DESCRIPTION:    The program creates a stream socket in the inet 
                domain, binds it to port 8888 and receives any HTTP request
                arrived to the socket and response.
*/

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

/* HTTP response and header for a successful request.  */

static char *ok_response = 
		"HTTP/1.1 200 OK\n"
		"Content-type: %s\n"
		"\n";

/* HTTP response, header, and body indicating that the we didn't
   understand the request.  */

static char* bad_request_response = 
    "HTTP/1.1 400 Bad Request\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Bad Request</h1>\n"
    "  <p>This server did not understand your request.</p>\n"
    " </body>\n"
    "</html>\n";

/* HTTP response, header, and body template indicating that the
   requested document was not found.  */

static char* not_found_response_template = 
    "HTTP/1.1 404 Not Found\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Not Found</h1>\n"
    "  <p>The requested URL %s was not found on this server.</p>\n"
    " </body>\n"
    "</html>\n";

/* HTTP response, header, and body template indicating that the
   method was not understood.  */

static char* bad_method_response_template = 
    "HTTP/1.1 501 Method Not Implemented\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Method Not Implemented</h1>\n"
    "  <p>The method %s is not implemented by this server.</p>\n"
    " </body>\n"
    "</html>\n";

static char* forbidden_response_template = 
    "HTTP/1.1 403 Forbidden\n"
    "Content-type: text/html\n"
    "\n"
    "<html>\n"
    " <body>\n"
    "  <h1>Forbidden</h1>\n"
    "  <p>You don't have permission.</p>\n"
    " </body>\n"
    "</html>\n";

pthread_mutex_t m_acc;

void get_resource(int newSocket, char* url){
	char *file_type, *header_type;
	int file_fd;
  	long  ret, len;
  	int BUFSIZE = 8096;
  	char buffer[BUFSIZE]; 
  	/* buffer needs to be zero-filled */
  	memset( buffer, 0, sizeof(buffer) ); 

	/* ---- get the file extension ---- */
	file_type = strrchr(url, '.');
	/* if null, use default html; otherwise, point to the next position */
	if(!file_type){
		file_type = "html";
	}else{
		file_type++;
	}
	/* Match the header type with the file type in generating the response. */
	/* Only support JEPG, GIF, PNG, CSS, Js, and HTML */
	if(strcasecmp(file_type, "jpg") == 0 || strcasecmp(file_type, "jepg") == 0){
		header_type = "image/jepg";
	}else if(strcasecmp(file_type, "gif") == 0){
		header_type = "image/gif";
	}else if(strcasecmp(file_type, "png") == 0){
		header_type = "image/png";
	}else if(strcasecmp(file_type, "css") == 0){
		header_type = "text/css";
	}else if(strcasecmp(file_type, "js") == 0){
		header_type = "text/js";
	}else{
		header_type = "text/html";
	}

	printf("Retrieving resource %s\n", url);
		 
	/* ---- Determine if target file exists and if permissions are set properly (return error otherwise) ---- */
	if (access(++url, R_OK) != 0)
	{
		printf("Does not have permission.\n");
	    write(newSocket, forbidden_response_template, strlen(forbidden_response_template));
	    return;
	}

  	/* open the file for reading */
  	if(( file_fd = open(url, O_RDONLY)) == -1) {  
    	printf("Fail to open the file.\n");
    	snprintf(buffer, sizeof(buffer), not_found_response_template, url);
    	write(newSocket, buffer, strlen(buffer));
    	return;
  	}

  	/* ---- We have to find out the size of the image ----*/
  	/* lseek to the file end to find the length */
  	len = (long)lseek(file_fd, 0, SEEK_END);
  	/* lseek back to the file start ready for reading */ 
 	lseek(file_fd, 0, SEEK_SET); 
  	
  	/* ---- send the response back to the client: OK, file type, and the content ---- */
 	/* concatenate the strings */
 	sprintf(buffer,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", header_type, len ); 
  	write(newSocket, buffer, strlen(buffer)); // Notice the strlen here, I used to use sizeof(buffer), and it is wrong. It takes me 1 day to solve it.

  	/* ---- Read the image file and send it to the browser ---- */
  	while (  (ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
    	write(newSocket,buffer,ret);
  	}
	
}

void do_service(int newSocket){
	char buf[512];
	ssize_t bytes_read;

	/* ---- Parse HTTP request, ensure well-formed request(return error otherwise) ---- */
	/* how to parse HTTP request */
	bytes_read = read(newSocket, buf, sizeof(buf)-1);
	printf("Received: %s\n", buf);
	if(bytes_read>0){
		char method[sizeof(buf)];
		char url[sizeof(buf)];
		char protocol[sizeof(buf)];

		/* ---- We want to use string operation, so we need to add '\0' at the end of the char array ---- */
		buf[bytes_read] = '\0';

		/* ---- parse the first line: method, request page, protocol version ---- */
		/* use sscanf to parse the string */
		sscanf( buf, "%s %s %s", method, url, protocol);

		/* check every field */
		if(strcasecmp(protocol, "HTTP/1.0") && strcasecmp(protocol, "HTTP/1.1")){
			/* we only support 1.0 and 1.1 */
			write(newSocket, bad_request_response, strlen(bad_request_response));
			printf("We only support get. You are using %s, url is %s, and protocol is %s\n", method, url, protocol);
		}else if(strcasecmp(method, "GET")){
			// We only support get
			char response[1024];
			memset( response, 0, sizeof(response) );  
			snprintf(response, sizeof(response), bad_method_response_template, method);
			write(newSocket, response, strlen(response));
			printf("We only support get\n");
		}else{
			/* vaild request */
			get_resource( newSocket, url );
		}
	}else{
		printf("The client closed the connection before sending any data\n");
	}
}

void *run(void *arg){
	int welcomeSocket = (int) arg;
	int newSocket;
	int client_len;
    struct sockaddr_in client;
	

	client_len = sizeof(client);
	
	while(1){
		/* ---- This is to lock the state ---- */
		pthread_mutex_lock(&m_acc);
		/* ---- Accept call creates a new socket for the incoming connection ---- */
		newSocket = accept(welcomeSocket, (struct sockaddr *) &client, (unsigned int *) &client_len);
		pthread_mutex_unlock(&m_acc);
		if(newSocket<0){
			printf("Error on accept\n");
		}
		printf("The client is connected ...\n");

		do_service(newSocket);

		/* ---- close the connection ---- */
		close(newSocket);
		printf("Closing socket ...\n");
	}

}

int main(int argCount, char *argValues[]){
	int port;
	int welcomeSocket;
	struct sockaddr_in server;
    const int MAXNTHREAD = 100;
	pthread_t tid[MAXNTHREAD];

	for(int i = 0; i<sizeof(argValues); i++){
		if(strcmp("-port", argValues[i]) == 0){
			port = atoi(argValues[i+1]);
			printf("Get port: %s\n", argValues[i+1]);
			break;
		}
	}

	/* ---- Create the socket. The three arguments are: ---- */
	/* 1) Internet Domain 2) Stream socket 3) Default protocol (TCP in this case) */
	welcomeSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(welcomeSocket<0){
		printf("Error opening socket\n");
	}
	memset(&server, '0', sizeof(server));

	/* ---- Configure settings of the server address struct ---- */
	/* Address family = Internet */
	server.sin_family = AF_INET;
	/* Set port number, using htons to use proper btye order */
	server.sin_port = htons(port);
	/* Set IP address to localhost */
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	/* ---- bind info to socket ---- */
	if(bind( welcomeSocket, (struct sockaddr *) &server, sizeof(server))){
		printf("Error on binding\n");
	}
	
	/* ---- Listen to the socket, with 5 max connection requests queued ---- */
	listen(welcomeSocket, 10);

	/* ---- Use mutex to control threads and limited them to MAXNTHREAD ---- */
	/* multi-thread lock init */
	pthread_mutex_init(&m_acc, 0);
	/* create MAXNTHREAD */
	for(int i =0; i<MAXNTHREAD; i++){
		pthread_create(&tid[i], 0, run, (void *)welcomeSocket);
	}

	pause();
	return 0;
}










