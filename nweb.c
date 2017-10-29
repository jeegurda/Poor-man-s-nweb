#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFSIZE 8096
#define ERROR 42
#define SORRY 43
#define LOG 44

struct {
  char *ext;
  char *filetype;
} extensions [] = {
  {"gif", "image/gif"},
  {"jpg", "image/jpeg"},
  {"jpeg","image/jpeg"},
  {"png", "image/png"},
  {"zip", "image/zip"},
  {"gz",  "image/gz"},
  {"tar", "image/tar"},
  {"htm", "text/html"},
  {"html","text/html"},
  {"php", "image/php"},
  {"cgi", "text/cgi"},
  {"asp", "text/asp"},
  {"jsp", "image/jsp"},
  {"xml", "text/xml"},
  {"js", "text/js"},
  {"css","test/css"},
  {0, 0}
};

void log(int type, char *s1, char *s2, int num) {
  int fd;
  char logbuffer[BUFSIZE*2];

  switch (type) {
    case ERROR: sprintf (logbuffer, "ERROR: %s:%s Errno=%d exiting pid=%d", s1, s2, errno,getpid()); break;
    case SORRY:
      sprintf(logbuffer, "<html><body><h1>Sorry: %s %s</h1></body></html>\r\n", s1, s2);
      write(num, logbuffer, strlen(logbuffer));
      sprintf(logbuffer, "Sorry: %s:%s", s1, s2);
      break;
    case LOG: sprintf(logbuffer, " Info: %s:%s:%d", s1, s2, num); break;
  }

  // prob not the best permissions
  if ((fd = open("server.log", O_CREAT | O_WRONLY | O_APPEND, 0644)) >= 0) {
    write(fd, logbuffer, strlen(logbuffer));
    write(fd, "\n", 1);
    close(fd);
  }
  if (type == ERROR || type == SORRY) exit(3);
}

void web(int fd, int hit) {
  int j, file_fd, buflen, len;
  long i, ret;
  char * fstr;
  static char buffer[BUFSIZE + 1];

  ret = read(fd, buffer, BUFSIZE);
  if (ret == 0 || ret == -1) {
    log(SORRY, "Failed to read browser request", "", fd);
  }
  if (ret > 0 && ret < BUFSIZE) {
    buffer[ret] = 0;
  } else {
    buffer[0] = 0;
  }

  for (i=0; i < ret; i++) {
    if (buffer[i] == '\r' || buffer[i] == '\n') {
      buffer[i]='*';
    }
  }
  log(LOG, "request", buffer, hit);

  if (strncmp(buffer, "GET ", 4) && strncmp(buffer, "get ", 4)) {
    log(SORRY, "Only GET method are supported!", buffer, fd);
  }

  for (i = 4; i < BUFSIZE; i++) {
    if (buffer[i] == ' ') {
      buffer[i] = 0;
      break;
    }
  }

  for (j = 0; j < i - 1; j++) {
    if (buffer[j] == '.' && buffer[j+1] == '.') {
      log(SORRY, "Parent directory (..) path names not supported", buffer, fd);
    }
  }

  if(!strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6)) {
    strcpy(buffer, "GET /index.html");
  }

  buflen = strlen(buffer);
  fstr = (char *)0;
  for (i = 0; extensions[i].ext != 0; i++) {
    len = strlen(extensions[i].ext);
    if(!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
      fstr = extensions[i].filetype;
      break;
    }
  }
  if (fstr == 0) {
    log(SORRY, "file extension type not supported", buffer, fd);
  }

  if ((file_fd = open(&buffer[5], O_RDONLY)) == -1) {
    log(SORRY, "failed to open file", &buffer[5], fd);
  }

  log(LOG, "SEND", &buffer[5], hit);

  sprintf(buffer, "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
  write(fd, buffer, strlen(buffer));

  while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
    write(fd, buffer, ret);
  }

  // should be a better way imo
  #ifdef LINUX
    sleep(1);
  #endif
    exit(1);
}

int main(int argc, char **argv) {
  int i, port, pid, listenfd, socketfd, hit;
  size_t length;
  static struct sockaddr_in cli_addr;
  static struct sockaddr_in serv_addr;

  if (argc < 3 || argc > 3 || !strcmp(argv[1], "-?")) {
    printf("usage: server [port] [server directory] &"
      "\tExample: server 80 ./ &\n\n"
      "\tOnly Supports:");
    for (i = 0; extensions[i].ext != 0; i++) {
      printf(" %s", extensions[i].ext);
    }

    printf("\n\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n");
    exit(0);
  }

  if (
    !strncmp(argv[2], "/", 2) || !strncmp(argv[2],"/etc", 5) ||
    !strncmp(argv[2],"/bin", 5) || !strncmp(argv[2],"/lib", 5) ||
    !strncmp(argv[2],"/tmp", 5) || !strncmp(argv[2],"/usr", 5) ||
    !strncmp(argv[2],"/dev", 5) || !strncmp(argv[2],"/sbin", 6)
  ) {
    printf("ERROR: Bad top directory %s, see server -?\n", argv[2]);
    exit(3);
  }

  if (chdir(argv[2]) == -1) {
    printf("ERROR: Can't Change to directory %s\n", argv[2]);
    exit(4);
  }

  if (fork() != 0) {
    return 0;
  }

  signal(SIGCHLD, SIG_IGN);
  signal(SIGHUP, SIG_IGN);

  for(i = 0; i < 32; i++) {
    close(i);
  }

  setpgrp();

  log(LOG, "http server starting", argv[1], getpid());

  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    log(ERROR, "system call", "socket", 0);
  }

  port = atoi(argv[1]);

  if (port < 0 || port > 60000) {
    log(ERROR, "Invalid port number try [1,60000]", argv[1], 0);
  }

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  if (bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    log(ERROR, "system call", "bind", 0);
  }

  if (listen(listenfd, 64) < 0) {
    log(ERROR, "system call", "listen", 0);
  }

  for (hit = 1; ; hit++) {
    length = sizeof(cli_addr);
    if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0) {
      log(ERROR, "system call", "accept", 0);
    }

    if ((pid = fork()) < 0) {
      log(ERROR, "system call", "fork", 0);
    } else {
      if (pid == 0) {
        close(listenfd);
        web(socketfd, hit);
      } else {
        close(socketfd);
      }
    }
  }
}
