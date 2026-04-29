#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdio.h>

#define FIFO_CONTROLLER "fifos/controller_fifo"
#define FIFO_RESPONSE "fifos/response_fifo"

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

    if (strcmp(argv[1], "-e") == 0) {

    if (argc < 4) {
        write_str("Usage: ./runner -e <user-id> \"<command>\"\n");
        return 1;
    }

    int user_id = atoi(argv[2]);
    int command_id = getpid();

    int fd = open(FIFO_CONTROLLER, O_WRONLY);

    if (fd < 0) {
        write_str("[runner] controller not running\n");
        return 1;
    }

    char exec_msg[512];
    sprintf(exec_msg, "EXEC|%s|%s\n", argv[2], argv[3]);
    write(fd, exec_msg, strlen(exec_msg));

    write_str("[runner] command submitted\n");

    int res = open(FIFO_RESPONSE, O_RDONLY);

    char reply[10];
    read(res, reply, sizeof(reply));
    close(res);

    write_str("[runner] executing...\n");

    pid_t pid = fork();

    if (pid == 0) {
        execlp("sh", "sh", "-c", argv[3], NULL);
        _exit(1);
    } else {
        wait(NULL);
        write_str("[runner] command finished\n");
    }
    
    int fd_done = open(FIFO_CONTROLLER, O_WRONLY);

    char done_msg[512];
    sprintf(done_msg, "DONE|%s|%s\n", argv[2], argv[3]);
    write(fd_done, done_msg, strlen(done_msg));

    close(fd_done);

}

else if (strcmp(argv[1], "-c") == 0) {
    int fd = open(FIFO_CONTROLLER, O_WRONLY);

    write(fd, "STATUS\n", 7);
    close(fd);

    int res = open(FIFO_RESPONSE, O_RDONLY);

    char buffer[512];
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
        return 1;
    }

    write(fd, "STOP\n", 5);
    close(fd);

    write_str("[runner] stop request\n");
}
    
    else {
        write_str("Invalid option\n");
        return 1;
    }

    return 0;
}
