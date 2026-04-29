#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FIFO_CONTROLLER "fifos/controller_fifo"
#define FIFO_RESPONSE "fifos/response_fifo"

#define MAX_CMDS 100

typedef struct {
    int active;  
    int user_id;
    char command[256];
} command_t;

command_t running[MAX_CMDS];
int running_count = 0;

void write_str(char *s) {
    write(1, s, strlen(s));
}

int read_line(int fd, char *buffer, int max) {
    int i = 0;
    char c;

    while (i < max - 1) {
        int n = read(fd, &c, 1);

        if (n <= 0) return n;

        if (c == '\n') break;

        buffer[i++] = c;
    }

    buffer[i] = '\0';
    return i;
}

int main() {
    mkfifo(FIFO_RESPONSE, 0666);

    mkfifo(FIFO_CONTROLLER, 0666);

    write_str("[controller] waiting for requests...\n");

    int fd = open(FIFO_CONTROLLER, O_RDWR);

    char buffer[512];
    int n;

    while ((n = read_line(fd, buffer, sizeof(buffer))) > 0) {

    write_str("[controller] received: ");
    write_str(buffer);
    write_str("\n");

    if (strncmp(buffer, "EXEC", 4) == 0) {
        char *user = strtok(buffer + 5, "|");
        char *cmd = strtok(NULL, "|");

        running[running_count].active = 1;
        running[running_count].user_id = atoi(user);
        strcpy(running[running_count].command, cmd);
        running_count++;

        int res = open(FIFO_RESPONSE, O_WRONLY);
        write(res, "OK", 2);
        close(res);
    }
    else if (strncmp(buffer, "DONE", 4) == 0) {
        char *user = strtok(buffer + 5, "|");
        char *cmd = strtok(NULL, "|");

        for (int i = 0; i < running_count; i++) {
            if (running[i].active &&
                running[i].user_id == atoi(user) &&
                strcmp(running[i].command, cmd) == 0) {
                running[i].active = 0;
                break;
            }
        }

        write_str("[controller] command finished\n");
    }
    else if (strncmp(buffer, "STOP", 4) == 0) {
        write_str("[controller] shutting down\n");
        break;
    }
    else if (strncmp(buffer, "STATUS", 6) == 0) {
        int res = open(FIFO_RESPONSE, O_WRONLY);

        char msg[1024];
        int len = 0;

        len += sprintf(msg + len, "Running:\n");

        for (int i = 0; i < running_count; i++) {
            if (running[i].active) {
                len += sprintf(msg + len, "User %d: %s\n",
                               running[i].user_id,
                               running[i].command);
            }
        }

        if (len == 9) {
            len += sprintf(msg + len, "None\n");
        }

        write(res, msg, len);
        close(res);
    }
}   

    close(fd);
    return 0;
}
