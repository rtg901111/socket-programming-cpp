#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define AUTH_SUCCESS 2
#define PASSWORD_FAILURE 1
#define USERNAME_FAILURE 0

#define LOGIN_ATTEMPT 3

#define PORT "25973" // serverM TCP PORT

/*
This function below 'heavily' uses code blocks from beej's programming guide:
https://beej.us/guide/bgnet/html/index-wide.html#cb84-15
*/

int authenticate() {

  int rv, sockfd, yes = 1;
  struct addrinfo hints, *servinfo, *p;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;       // ONLY IPv4 connections
  hints.ai_socktype = SOCK_STREAM; // TCP connection

  if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through the result of the getaddrinfo call and connect.
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("client: socket");
      continue;
    }

    // Allow socket reuse (not sure if I need to add it to client)
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }
    //Connect to the serverM
    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("client: connect");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    return 2;
  }

  printf("The client is up and running.\n");

  // Get the dynamically assigned port number for client
  // This code block comes from piazza (given by TA). https://piazza.com/class/l7dlij7ko7v4bv/post/188
  unsigned int clientPort;
  int getsock_check;
  struct sockaddr_in clientAddress;
  bzero(&clientAddress, sizeof clientAddress);
  socklen_t len = sizeof clientAddress;
  if ((getsock_check = getsockname(sockfd, (struct sockaddr *)&clientAddress,
                                   &len)) == -1) {
    perror("getsockname");
  }
  clientPort = ntohs(clientAddress.sin_port); // Client's port

  int curr_attempt = 0;
  int auth_result, numbytes;
  char username[1024], password[1024], course_code[1024], category[1024];
  //Loop for 3 times.
  while (curr_attempt < LOGIN_ATTEMPT) {
    int auth_result_numbytes;
    // char username[1024], password[1024];
    int auth_result;

    // Send the username and password to serverM
    std::cout << "Please enter the username: ";
    std::cin.getline(username, sizeof username);
    send(sockfd, username, strlen(username) + 1, 0);

    std::cout << "Please enter the password: ";
    std::cin.getline(password, sizeof password);
    send(sockfd, password, strlen(password) + 1, 0);

    printf("%s sent an authentication request to the main server.\n",
            username);

    // receive the auth result back from serverM
    if ((auth_result_numbytes =
              recv(sockfd, &auth_result, sizeof auth_result, 0)) == -1) {
      perror("client failed to receive auth result from serverM");
      exit(1);
    }

    // If auth succeeded
    if (auth_result == AUTH_SUCCESS) {
      printf("%s received the result of authentication using TCP over port "
              "%d. Authentication is successful\n",
              username, clientPort);
      break;
    }

    curr_attempt++;
    if (auth_result == USERNAME_FAILURE) {
      printf("%s received the result of authentication using TCP over port "
              "%d. Authentication failed: Username Does not exist\n",
              username, clientPort);
      printf("Attempts remaining: %d\n", LOGIN_ATTEMPT - curr_attempt);
    } else if (auth_result == PASSWORD_FAILURE) {
      printf("%s received the result of authentication using TCP over port "
              "%d. Authentication failed: Password does not match\n",
              username, clientPort);
      printf("Attempts remaining: %d\n", LOGIN_ATTEMPT - curr_attempt);
    }

    // If client fails all 3 times: exit(1);
    if (curr_attempt == LOGIN_ATTEMPT) {
      printf("Authentication Failed for 3 attempts. Client will shut down.\n");
      // close the client TCP socket after done.
      close(sockfd);
      exit(1);
    }
  }

  while (true) {
    // get course code query input
    std::cout << "Please enter the course code to query: ";
    std::cin.getline(course_code, sizeof course_code);
    send(sockfd, course_code, strlen(course_code) + 1, 0);

    std::cout << "Please enter the category (Credit / Professor / Days / "
                  "CourseName): ";
    std::cin.getline(category, sizeof category);
    send(sockfd, category, strlen(category) + 1, 0);

    printf("%s sent a request to the main server.\n", username);

    // Receive the query result from serverM
    char query_result[1024];
    if ((numbytes =
              recv(sockfd, query_result, 1024, 0)) == -1) {
      perror("client failed to receive query result from serverM");
      exit(1);
    }

    printf("The client received the response from the Main server using TCP over port %d.\n",
            clientPort);

    //course not found
    if (strcmp(query_result, "COURSE NOT FOUND") == 0) {
    //if (query_result == "COURSE NOT FOUND") {
      printf("Didn't find the course: %s.\n", course_code);
    }
    //course found
    else {
      printf("The %s of %s is %s.\n", category, course_code, query_result);
    }

    printf("\n-----Start a new request-----\n");

  }
}

int main() {
  authenticate();
  return 0;
}