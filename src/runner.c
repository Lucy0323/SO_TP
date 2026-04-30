#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>

#define FIFO_CONTROLLER "fifos/controller_fifo"

void write_str(char *s) {
    write(1, s, strlen(s));
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        write_str("Usage:\n");
        write_str("./runner -e <user-id> \"<command>\"\n");
        write_str("./runner -c\n");
        write_str("./runner -s\n");
        return 1;
    }

    pid_t mypid = getpid();

    char fifo_response[100];
    sprintf(fifo_response, "fifos/resp_%d", mypid);
    mkfifo(fifo_response, 0666);

    if (strcmp(argv[1], "-e") == 0) {
        if (argc < 4) {
            write_str("Usage: ./runner -e <user-id> \"<command>\"\n");
            unlink(fifo_response);
            return 1;
        }

        int fd = open(FIFO_CONTROLLER, O_WRONLY);
        if (fd < 0) {
            write_str("[runner] controller not running\n");
            unlink(fifo_response);
            return 1;
        }

        char msg[512];
        sprintf(msg, "EXEC|%s|%d|%s|%s\n", argv[2], mypid, fifo_response, argv[3]);

        write(fd, msg, strlen(msg));
        close(fd);

        write_str("[runner] command ");
        char idbuf[32];
        sprintf(idbuf, "%d", mypid);
        write_str(idbuf);
        write_str(" submitted\n");

        int res = open(fifo_response, O_RDONLY);
        char buffer[32];
        read(res, buffer, sizeof(buffer));
        close(res);

        write_str("[runner] executing command ");
        write_str(idbuf);
        write_str("...\n");

        pid_t pid = fork();

        if (pid == 0) {
            execlp("sh", "sh", "-c", argv[3], NULL);
            _exit(1);
        } else {
            wait(NULL);
        }

        write_str("[runner] command ");
        write_str(idbuf);
        write_str(" finished\n");

        fd = open(FIFO_CONTROLLER, O_WRONLY);
        sprintf(msg, "DONE|%s|%d|%s\n", argv[2], mypid, argv[3]);
        write(fd, msg, strlen(msg));
        close(fd);
    }

    else if (strcmp(argv[1], "-c") == 0) {
        int fd = open(FIFO_CONTROLLER, O_WRONLY);
        if (fd < 0) {
            write_str("[runner] controller not running\n");
            unlink(fifo_response);
            return 1;
        }

        char msg[256];
        sprintf(msg, "STATUS|%s\n", fifo_response);

        write(fd, msg, strlen(msg));
        close(fd);

        int res = open(fifo_response, O_RDONLY);
        char buffer[2048];
        int n = read(res, buffer, sizeof(buffer) - 1);

        if (n > 0) {
            buffer[n] = '\0';
            write(1, buffer, n);
        }

        close(res);
    }

    else if (strcmp(argv[1], "-s") == 0) {
        int fd = open(FIFO_CONTROLLER, O_WRONLY);
        if (fd < 0) {
            write_str("[runner] controller not running\n");
            unlink(fifo_response);
            return 1;
        }

        char msg[256];
        sprintf(msg, "STOP|%s\n", fifo_response);

        write(fd, msg, strlen(msg));
        close(fd);

        write_str("[runner] sent shutdown notification\n");
    }

    unlink(fifo_response);
    return 0;
}