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

#define PORT "22973" // serverCS UDP PORT

#define MAXBUFLEN 1024

//Parse the cs.txt file and verify the course query
std::string parse_and_retrieve(std::string course_code, std::string category) {
  std::string course_line;
  std::ifstream course_file("cs.txt");
  std::vector<std::vector<std::string> > course_vector;
  std::string result_information;
  //[[course1, course1_credit, course1 prof...], [course2, ...]]

  //read 'cs.txt' line by line
  while (std::getline(course_file, course_line)) {
    if (course_line.empty()) {
      break;
    }
    std::vector<std::string> inner_vector;

    //This code below refers to: https://stackoverflow.com/questions/52689845/split-comma-separated-string
    std::stringstream ss(course_line);
    std::string str;
    //For each item in the line separated by a comma, append to inner vector.
    while (getline(ss, str, ',')) {
      inner_vector.push_back(str);
    }
    course_vector.push_back(inner_vector);
  }

  //loop through the course vector
  bool course_found = false;
  for (size_t i = 0; i < course_vector.size(); i++) {
    // if course found
    if (course_vector[i][0] == course_code) {
      course_found = true;
      if (category == "Credit") {
        printf("The course information has been found: The %s of %s is %s.\n", 
        category.c_str(), course_code.c_str(), course_vector[i][1].c_str());
        result_information = course_vector[i][1];
      }
      else if (category == "Professor") {
        printf("The course information has been found: The %s of %s is %s.\n", 
        category.c_str(), course_code.c_str(), course_vector[i][2].c_str());
        result_information = course_vector[i][2];
      }
      else if (category == "Days") {
        printf("The course information has been found: The %s of %s is %s.\n", 
        category.c_str(), course_code.c_str(), course_vector[i][3].c_str());
        result_information = course_vector[i][3];
      }
      else if (category == "CourseName") {
        printf("The course information has been found: The %s of %s is %s.\n", 
        category.c_str(), course_code.c_str(), course_vector[i][4].c_str());
        result_information = course_vector[i][4];
      }
      return result_information;
    }
  }
  // if course NOT found
  if (course_found == false) {
    printf("Didn't find the course: %s.\n", course_code.c_str());
    result_information = "COURSE NOT FOUND";
  }

  //Return the 'Information' for the course_code and category back to serverM.
  return result_information;
}

/*
This function below 'heavily' uses code blocks from beej's programming guide:
https://beej.us/guide/bgnet/html/index-wide.html#cb84-15
*/
// Main function
int receive_query() {
  int rv, send_numbytes, numbytes, sockfd, yes = 1;
  struct addrinfo hints, *servinfo, *p;
  struct sockaddr_storage their_addr;
  char course_code[MAXBUFLEN];
  char category[MAXBUFLEN];
  socklen_t addr_len;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  // hints.ai_flags = AI_PASSIVE; <- DO I NEED IT? IDK

  // get address info for the serverCS UDP server.
  if ((rv = getaddrinfo("127.0.0.1", PORT, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through the result of getaddrinfo to create socket and bind.
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
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

  printf("The ServerCS is up and running using UDP on port %s.\n", PORT);

  // TODO: FIGURE OUT
  // freeaddrinfo(servinfo);
  std::string result_information;
  char result_information_char[MAXBUFLEN];

  while (true) {
    addr_len = sizeof their_addr;
    // Receive course_code from serverM using UDP
    if ((numbytes = recvfrom(sockfd, course_code, MAXBUFLEN - 1, 0,
                             (struct sockaddr *)&their_addr, &addr_len)) ==
        -1) {
      perror("recvfrom");
      exit(1);
    }

    // Receive category from serverM using UDP
    if ((numbytes = recvfrom(sockfd, category, MAXBUFLEN - 1, 0,
                             (struct sockaddr *)&their_addr, &addr_len)) ==
        -1) {
      perror("recvfrom");
      exit(1);
    }

    printf("The ServerCS received a request from the Main Server about the %s of %s.\n", 
    category, course_code);

    //Parse the txt file and get the query result.
    //result_information (string) = information if success; "COURSE NOT FOUND" if failure
    result_information = parse_and_retrieve(std::string(course_code), std::string(category));
    strcpy(result_information_char, result_information.c_str());

    //Send the query result information back to serverM.
    if ((numbytes = sendto(sockfd, result_information_char, MAXBUFLEN - 1, 0, (struct sockaddr *)&their_addr, addr_len)) == -1) {
      perror("serverCS sendto");
      exit(1);
    }

    printf("The ServerCS finished sending the response to the Main Server.\n");
  }
}

int main() {
  receive_query();
  return 0;
}