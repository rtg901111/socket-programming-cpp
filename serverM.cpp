#include <arpa/inet.h>
#include <assert.h>
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

#define TCP_PORT "25973" // serverM TCP PORT

#define serverC_PORT "21973"  // serverC UDP PORT
#define serverCS_PORT "22973" // serverCS UDP PORT
#define serverEE_PORT "23973" // serverEE UDP PORT

#define serverM_UDP_PORT "24973" // serverM UDP PORT

#define BACKLOG 5

/*
This function below 'heavily' uses code blocks from beej's programming guide:
https://beej.us/guide/bgnet/html/index-wide.html#cb84-15
*/

void sigchld_handler(int s) {
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = saved_errno;
}

/*
This function below 'heavily' uses code blocks from beej's programming guide:
https://beej.us/guide/bgnet/html/index-wide.html#cb84-15
*/
//Connects to serverC
int connect_serverC(char *encrypted_username, char *encrypted_password) {
  int rv, sockfd, yes = 1;
  struct addrinfo hints, *servinfo, *p;
  int numbytes;
  int serverM_rv;
  struct addrinfo serverM_hints, *serverM_info;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET; // IPv4 Only
  hints.ai_socktype = SOCK_DGRAM;

  memset(&serverM_hints, 0, sizeof serverM_hints);
  serverM_hints.ai_family = AF_INET;
  serverM_hints.ai_socktype = SOCK_DGRAM;

  // Get address info for the serverC UDP server.
  if ((rv = getaddrinfo("127.0.0.1", serverC_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through the result from getaddrinfo to create a UDP socket.
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("talker: socket");
      continue;
    }
    // Get address info for the serverM UDP port and address to bind the local
    // port.
    if ((serverM_rv = getaddrinfo("127.0.0.1", serverM_UDP_PORT, &serverM_hints,
                                  &serverM_info)) != 0) {
      fprintf(stderr, "getaddrinfo_serverM_UDP: %s\n",
              gai_strerror(serverM_rv));
      return 1;
    }
    // reuse the UDP port
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    // bind the serverM UDP socket to the local port and address.
    if (bind(sockfd, serverM_info->ai_addr, serverM_info->ai_addrlen) == -1) {
      close(sockfd);
      perror("serverM_UDP to serverC: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "talker: failed to create socket\n");
    return 2;
  }

  // send encrypted_username to serverC.
  if ((numbytes =
           sendto(sockfd, encrypted_username, strlen(encrypted_username) + 1, 0,
                  p->ai_addr, p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }

  // send encrypted_password to serverC.
  if ((numbytes =
           sendto(sockfd, encrypted_password, strlen(encrypted_password) + 1, 0,
                  p->ai_addr, p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }

  printf("The main server sent an authentication request to serverC.\n");

  // Receive result of authentication from serverC
  int auth_result_numbytes;
  int auth_result;
  if ((auth_result_numbytes =
           recvfrom(sockfd, &auth_result, sizeof(auth_result), 0, p->ai_addr,
                    &p->ai_addrlen)) == -1) {
    perror("recvfrom: authresult from serverC");
    exit(1);
  }

  printf("The main server received the result of authentication request from "
         "ServerC using UDP over port %s.\n",
         serverM_UDP_PORT);

  // close the UDP socket with serverC.
  close(sockfd);

  return auth_result;
}

/*
This function below 'heavily' uses code blocks from beej's programming guide:
https://beej.us/guide/bgnet/html/index-wide.html#cb84-15
*/
//Connects to serverCS/EE
std::string connect_serverDEP(char *course_code, char *category) {
  int rv, sockfd, yes = 1;
  struct addrinfo hints, *servinfo, *p;
  int numbytes;
  int serverM_rv;
  struct addrinfo serverM_hints, *serverM_info;
  const char *dep_PORT;
  char query_result[1024];

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET; // IPv4 Only
  hints.ai_socktype = SOCK_DGRAM;

  memset(&serverM_hints, 0, sizeof serverM_hints);
  serverM_hints.ai_family = AF_INET;
  serverM_hints.ai_socktype = SOCK_DGRAM;

  // If the course query is for EE:
  if (course_code[0] == 'E' && course_code[1] == 'E') {
    dep_PORT = serverEE_PORT;
  }
  // If the course query is for CS:
  else if (course_code[0] == 'C' && course_code[1] == 'S') {
    dep_PORT = serverCS_PORT;
  }

  // Get address info for the department (EE or CS) UDP server.
  if ((rv = getaddrinfo("127.0.0.1", dep_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through the result from getaddrinfo to create a UDP socket.
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("talker: socket");
      continue;
    }
    // Get address info for the serverM UDP port and address to bind the local
    // port.
    if ((serverM_rv = getaddrinfo("127.0.0.1", serverM_UDP_PORT, &serverM_hints,
                                  &serverM_info)) != 0) {
      fprintf(stderr, "getaddrinfo_serverM_UDP: %s\n",
              gai_strerror(serverM_rv));
      exit(1);
    }
    // reuse the UDP port
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    // bind the serverM UDP socket to the local port and address.
    if (bind(sockfd, serverM_info->ai_addr, serverM_info->ai_addrlen) == -1) {
      close(sockfd);
      perror("serverM_UDP to serverDEP: bind");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "talker: failed to create socket\n");
    exit(1);
  }

  // send course_code to department server.
  if ((numbytes = sendto(sockfd, course_code, strlen(course_code) + 1, 0,
                         p->ai_addr, p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }

  // send category to department server.
  if ((numbytes = sendto(sockfd, category, strlen(category) + 1, 0, p->ai_addr,
                         p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }

  if (course_code[0] == 'E' && course_code[1] == 'E') {
    printf("The main server sent a request to serverEE.\n");
  }
  else if (course_code[0] == 'C' && course_code[1] == 'S') {
    printf("The main server sent a request to serverCS.\n");
  }

  // Receive query result from the department (EE/CS) server
  if ((numbytes =
           recvfrom(sockfd, query_result, 1024, 0, p->ai_addr,
                    &p->ai_addrlen)) == -1) {
    perror("recvfrom: authresult from serverC");
    exit(1);
  }

  if (course_code[0] == 'E' && course_code[1] == 'E') {
    printf("The main server received the response from serverEE using UDP over port %s.\n",
           serverM_UDP_PORT);
  }
  else if (course_code[0] == 'C' && course_code[1] == 'S') {
    printf("The main server received the response from serverCS using UDP over port %s.\n",
           serverM_UDP_PORT);
  }

  std::string query_result_str = query_result;
  return query_result_str;

}

/*
This function below 'heavily' uses code blocks from beej's programming guide:
https://beej.us/guide/bgnet/html/index-wide.html#cb84-15
*/
//main function
int authenticate(void) {

  struct sigaction sa;

  int rv, sockfd, new_fd, yes = 1;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  socklen_t sin_size;

  printf("The main server is up and running.\n");

  // TCP receiving data
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;       // Only IPv4 connections.
  hints.ai_socktype = SOCK_STREAM; // TCP Connections
  // hints.ai_flags = AI_PASSIVE; <- DO I NEED IT? IDK

  // getaddrinfo for the TCP server
  if ((rv = getaddrinfo("127.0.0.1", TCP_PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through the result of getaddrinfo to bind the socket.
  for (p = servinfo; p != NULL; p = p->ai_next) {
    // create a socket
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }
    //Reuse the port
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    // bind address to the socket
    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  // freeaddrinfo(servinfo);
  if (p == NULL) {
    fprintf(stderr, "server failed to bind\n");
    exit(1);
  }

  // start listening
  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  // Killing all dead processes; need to research more on this thing
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  // Accept() loop - while true
  int username_byte = 0;
  while (true) {
    char username[1024], password[1024], course_code[1024], category[1024];
    int auth_result;
    int password_byte, course_code_byte, category_byte;
    char encrypted_username[1024], encrypted_password[1024];
    // call accept when recv() == 0 (meaning client closed the
    // connection).
    if (username_byte == 0) {
      sin_size = sizeof their_addr;
      new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
                      &sin_size); // accept() and create a new child socket.
      if (new_fd == -1) {
        perror("accept");
        continue;
      }
    }

    // Receive username and password from the client
    if ((username_byte = recv(new_fd, username, sizeof username, 0)) == -1) {
      // perror("serverM failed to receive username from the client");
      continue;
    }
    // client has closed the connection (by failing the auth 3 times)
    else if (username_byte == 0) {
      //LAST THING TO CHECK!!!!!! 10/31 (serverM shuts down after client shutsdown)
      //close(new_fd);
      continue;
    }

    password_byte = recv(new_fd, password, sizeof password, 0);

    printf("The main server received the authentication for %s using TCP over "
           "port %s.\n",
           username, TCP_PORT);

    strncpy(encrypted_username, username, 1024); // copy username and password
    strncpy(encrypted_password, password, 1024);

    // encrypt username
    for (size_t i = 0; i < sizeof encrypted_username; i++) {
      char &ch = encrypted_username[i];

      if (isalpha(ch)) {
        if (isupper(ch)) {
          // This code line below comes from stackoverflow:
          // https://stackoverflow.com/questions/8487255/how-do-i-increment-letters-in-c
          ch = (char)('A' + ((ch - 'A' + 4) % 26));
        } else {
          assert(islower(ch));
          // This code line below comes from stackoverflow:
          // https://stackoverflow.com/questions/8487255/how-do-i-increment-letters-in-c
          ch = (char)('a' + ((ch - 'a' + 4) % 26));
        }
        continue;
      }

      if (isdigit(ch)) {
        ch += 4;
        if (ch > '9') {
          ch -= 10;
        }
      }
    }

    // encrypt password
    for (size_t i = 0; i < sizeof encrypted_password; i++) {
      if (isalpha(encrypted_password[i])) {
        if (encrypted_password[i] >= 'A' && encrypted_password[i] <= 'Z') {
          // This code line below comes from stackoverflow:
          // https://stackoverflow.com/questions/8487255/how-do-i-increment-letters-in-c
          encrypted_password[i] =
              (char)('A' + ((encrypted_password[i] - 'A' + 4) % 26));
        } else {
          // This code line below comes from stackoverflow:
          // https://stackoverflow.com/questions/8487255/how-do-i-increment-letters-in-c
          encrypted_password[i] =
              (char)('a' + ((encrypted_password[i] - 'a' + 4) % 26));
        }
      } else if (isdigit(encrypted_password[i])) {
        encrypted_password[i] = encrypted_password[i] + 4;
        if (encrypted_password[i] > '9') {
          encrypted_password[i] = encrypted_password[i] - 10;
        }
      }
    }

    // Create UDP connection to serverC and send encrypted username/password.
    auth_result = connect_serverC(encrypted_username, encrypted_password);

    // Send authentication result back to client.
    if (send(new_fd, &auth_result, sizeof auth_result, 0) == -1) {
      perror("send auth_result from serverM to client");
    }

    printf("The main server sent the authentication result to the client.\n");
    // If auth is not successful (i.e. need another round of auth), go back to
    // the loop.
    if (auth_result != AUTH_SUCCESS) {
      continue;
    }

    while (true) {
      // Receive Course code from the client.
      course_code_byte = recv(new_fd, course_code, sizeof course_code, 0);
      category_byte = recv(new_fd, category, sizeof category, 0);

      printf("The main server received from %s to query course %s about %s using "
            "TCP over port %s.\n",
            username, course_code, category, TCP_PORT);
      // Connect serverCS or serverEE and retrieve the query result
      // query_result (string) = information if found; "COURSE NOT FOUND" if not found.
      std::string query_result = connect_serverDEP(course_code, category);
      char query_result_char[1024];
      strcpy(query_result_char, query_result.c_str());

      //Send query_result back to client.
      if (send(new_fd, query_result_char, 1024, 0) == -1) {
        perror("send query result from serverM to client");
      }

      printf("The main server sent the query information to the client.\n");
    }
  }
}

int main(void) {
  authenticate();
  return 0;
}