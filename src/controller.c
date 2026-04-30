#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FIFO_CONTROLLER "fifos/controller_fifo"
#define MAX_CMDS 100

typedef struct {
    int active;
    int user_id;
    int command_id;
    char command[256];
    char response_fifo[100];
} command_t;

command_t running;
command_t queue[MAX_CMDS];

int has_running = 0;
int q_start = 0;
int q_end = 0;
int shutdown_requested = 0;

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

void send_ok(char *fifo_name) {
    int res = open(fifo_name, O_WRONLY);
    write(res, "OK", 2);
    close(res);
}

void start_next_command() {
    if (!has_running && q_start < q_end) {
        running = queue[q_start];
        q_start++;
        has_running = 1;

        send_ok(running.response_fifo);
    }
}

int main() {
    mkdir("fifos", 0777);
    mkfifo(FIFO_CONTROLLER, 0666);

    int fd = open(FIFO_CONTROLLER, O_RDWR);

    char buffer[512];

    write_str("[controller] waiting for requests...\n");

    while (read_line(fd, buffer, sizeof(buffer)) > 0) {
        write_str("[controller] received: ");
        write_str(buffer);
        write_str("\n");

        if (strncmp(buffer, "EXEC", 4) == 0) {
            char *user = strtok(buffer + 5, "|");
            char *cmd_id = strtok(NULL, "|");
            char *resp_fifo = strtok(NULL, "|");
            char *cmd = strtok(NULL, "\n");

            command_t c;
            c.active = 1;
            c.user_id = atoi(user);
            c.command_id = atoi(cmd_id);
            strcpy(c.response_fifo, resp_fifo);
            strcpy(c.command, cmd);

            if (!has_running) {
                running = c;
                has_running = 1;
                send_ok(c.response_fifo);
            } else {
                queue[q_end] = c;
                q_end++;
            }
        }

        else if (strncmp(buffer, "DONE", 4) == 0) {
            char *user = strtok(buffer + 5, "|");
            char *cmd_id = strtok(NULL, "|");

            if (has_running &&
                running.user_id == atoi(user) &&
                running.command_id == atoi(cmd_id)) {
                has_running = 0;
                write_str("[controller] command finished\n");
            }

            start_next_command();

            if (shutdown_requested && !has_running && q_start == q_end) {
                write_str("[controller] shutting down\n");
                break;
            }
        }

        else if (strncmp(buffer, "STATUS", 6) == 0) {
            char *resp_fifo = strtok(buffer + 7, "|");

            int res = open(resp_fifo, O_WRONLY);

            char msg[2048];
            int len = 0;

            len += sprintf(msg + len, "---\nExecuting\n");

            if (has_running) {
                len += sprintf(msg + len,
                               "user-id %d - command-id %d - %s\n",
                               running.user_id,
                               running.command_id,
                               running.command);
            }

            len += sprintf(msg + len, "---\nScheduled\n");

            for (int i = q_start; i < q_end; i++) {
                len += sprintf(msg + len,
                               "user-id %d - command-id %d - %s\n",
                               queue[i].user_id,
                               queue[i].command_id,
                               queue[i].command);
            }

            write(res, msg, len);
            close(res);
        }

        else if (strncmp(buffer, "STOP", 4) == 0) {
            shutdown_requested = 1;

            if (!has_running && q_start == q_end) {
                write_str("[controller] shutting down\n");
                break;
            } else {
                write_str("[controller] shutdown requested, waiting for runners...\n");
            }
        }
    }

    close(fd);
    unlink(FIFO_CONTROLLER);
    return 0;
}