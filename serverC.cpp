#include <arpa/inet.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

#define AUTH_SUCCESS 2
#define PASSWORD_FAILURE 1
#define USERNAME_FAILURE 0

#define PORT "21973" // serverC UDP Port

#define MAXBUFLEN 1024

// Read cred.txt and check if the credentials are correct.
// Returns 2 for success; 1 for password failure; 0 for username failure.
int compare_credentials(std::string encrypted_username,
                        std::string encrypted_password) {
  std::string cred_line;
  std::ifstream cred_file("cred.txt");
  std::vector<std::pair<std::string, std::string> > cred_vector;
  // cred_vector: [[username1, password1], [username2, password2], ...]

  // Read cred.txt and create the 'cred_vector' which contains the username and
  // password info.
  while (std::getline(cred_file, cred_line)) {
    std::string username, password;
    std::size_t comma_idx;

    if (cred_line.empty()) {
      break;
    }

    comma_idx = cred_line.find(',');
    username = cred_line.substr(0, comma_idx);
    password = cred_line.substr(comma_idx + 1);

    if (!cred_file.eof()) {
      password = password.substr(0, password.size() - 1);
    }

    std::pair<std::string, std::string> cred;
    cred.first = username;
    cred.second = password;
    cred_vector.push_back(cred);
  }

  // Now check if the input encrypted username and password are correct.
  for (size_t i = 0; i < cred_vector.size(); i++) {
    std::string username = cred_vector[i].first;
    std::string password = cred_vector[i].second;

    // cred_vector[i][0]: username in cred.txt
    // cred_vector[i][1]: password in cred.txt
    if (username == encrypted_username) {
      if (password == encrypted_password) {
        return AUTH_SUCCESS; // Both username and password match!
      } else {
        return PASSWORD_FAILURE; // Username matches but password fails.
      }
    }
  }
  return USERNAME_FAILURE; // Username not found! Authentication fails.
}

/*
This function below 'heavily' uses code blocks from beej's programming guide:
https://beej.us/guide/bgnet/html/index-wide.html#cb84-15
*/
// Main function
int authenticate() {
  int auth_result;
  int rv, send_numbytes, numbytes, sockfd, yes = 1;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  char encrypted_username[MAXBUFLEN];
  char encrypted_password[MAXBUFLEN];
  socklen_t addr_len;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  // hints.ai_flags = AI_PASSIVE; <- DO I NEED IT? IDK

  // get address info for the serverC UDP server.
  if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through the result of getaddrinfo to create socket and bind.
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
        -1) {
      perror("listener: socket");
      continue;
    }

    // reuse the UDP port
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("listener: bind");
      continue;
    }
    break;
  }

  if (p == NULL) {
    fprintf(stderr, "listener: failed to bind socket/n");
    return 2;
  }
  printf("The ServerC is up and running using UDP on port %s.\n", PORT);

  // TODO: FIGURE OUT
  // freeaddrinfo(servinfo);
  while (true) {
    addr_len = sizeof their_addr;
    // Receive encrypted username from serverM using UDP
    if ((numbytes = recvfrom(sockfd, encrypted_username, MAXBUFLEN - 1, 0,
                            (struct sockaddr *)&their_addr, &addr_len)) ==
        -1) {
      perror("recvfrom");
      exit(1);
    }

    // Receive encrypted password from serverM using UDP
    if ((numbytes = recvfrom(sockfd, encrypted_password, MAXBUFLEN - 1, 0,
                            (struct sockaddr *)&their_addr, &addr_len)) ==
        -1) {
      perror("recvfrom");
      exit(1);
    }

    printf("The ServerC received an authentication request from the Main Server.\n");

    // read cred.txt and authenticate the encrypted username and password.
    auth_result = compare_credentials(std::string(encrypted_username),
                                      std::string(encrypted_password));

    // send the auth result (0, 1, or 2) back to serverM.
    if ((send_numbytes = sendto(sockfd, &auth_result, sizeof(auth_result), 0,
                                (struct sockaddr *)&their_addr, addr_len)) == -1) {
      perror("talker: sendto"); // const void *
      exit(1);
    }
    printf("The ServerC finished sending the response to the Main Server.\n");

  }
}

int main() {
  authenticate();
  return 0;
}