#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdio.h>
#include <assert.h>

#define FIFO_CONTROLLER "fifos/controller_fifo"

void write_str(char *s) {
    write(1, s, strlen(s));
}

int exec_command(char *arg){
    char *exec_args[20];
    int args_count = 0;
    char *token, *string, *tofree;

    tofree = string = strdup(arg);
    assert(string != NULL);

    while ((token = strsep(&string, " ")) != NULL){
        if(strlen(token) > 0){
            exec_args[args_count++] = token;
        }
    }

    exec_args[args_count] = NULL;

    char *clean_args[20];
    int clean_count = 0;

    for(int i = 0; i < args_count; i++){
        if(strcmp(exec_args[i], "<") == 0){
            int fd = open(exec_args[i + 1], O_RDONLY);
            if(fd < 0) {
                perror("Erro ao abrir ficheiro de entrada");
                _exit(1);
            }
            dup2(fd, 0);
            close(fd);
            i++;
        }
        else if (strcmp(exec_args[i], ">") == 0){
            int fd = open(exec_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                perror("Erro ao criar ficheiro de erros");
                _exit(1);
            }
            dup2(fd, 1);
            close(fd);
            i++;
        }
        else if (strcmp(exec_args[i], "2>") == 0){
            int fd = open(exec_args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd < 0) {
                perror("Erro ao criar ficheiro de erros");
                _exit(1);
            }
            dup2(fd, 2);
            close(fd);
            i++;
        }
        else { 
            clean_args[clean_count++] = exec_args[i];
        }
    }

    clean_args[clean_count] = NULL;

    execvp(clean_args[0], clean_args);
    perror("Exec error");
    free(tofree);
    return -1;
    

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
////////////////////////////////////////////////////////////
        char *commands[20];
        int number_of_commands = 0;
        char *full_cmd = strdup(argv[3]);
        char *token = strsep(&full_cmd, "|");
        int fildes[20][2];

        while (token != NULL){
            commands[number_of_commands++] = token;
            token = strsep(&full_cmd, "|");
        }
        
        if(number_of_commands == 1){
            if(fork() == 0){
                exec_command(commands[0]);
            }       
        } else{
            for(int i = 0; i < number_of_commands; i++){
                if(i == 0) {
                    pipe(fildes[0]);
                    if(fork() == 0){
                        close(fildes[0][0]);
                        dup2(fildes[0][1], 1);
                        close(fildes[0][1]);
                        exec_command(commands[0]);
                        _exit(1);
                    }
                    close(fildes[0][1]);
                }
                else if(i == number_of_commands - 1) {
                    if (fork() == 0){
                            dup2(fildes[i - 1][0], 0);
                            close(fildes[i - 1][0]);
                            exec_command(commands[i]);
                            _exit(1);
                    }
                    close(fildes[i - 1][0]);
                }
                else{
                    pipe(fildes[i]);
                    if(fork() == 0){
                        close(fildes[i][0]);
                        dup2(fildes[i - 1][0], 0);
                        close(fildes[i - 1][0]);
                        dup2(fildes[i][1], 1);
                        close(fildes[i][1]);
                        exec_command(commands[i]);
                        _exit(1);
                    }
                    close(fildes[i][1]);
                    close(fildes[i - 1][0]);
                }
            }
        }
    
        for (int i = 0; i < number_of_commands; i++){
            wait(NULL);
        }
////////////////////////////////////////////////////////////
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