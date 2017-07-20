#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 1024
static char buffer[BUFFER_SIZE];

int main(int argc, char **argv)
{
    struct stat st;
    char *file_name = NULL;
    int fd, i, counter;
    ssize_t read_out;

    if (argc < 2) {
        printf("Usage: %s <file_name>\n", argv[0]);
        return 1;
    }
    file_name = argv[1];

    if (stat(file_name, &st) != 0) {
        printf("stat file: %s failed.\n", file_name);
        return 1;
    }

    fd = open(file_name, O_RDONLY);
    if (fd < 0) {
        printf("open file: %s failed.\n", file_name);
        return 1;
    }

    memset(buffer, 0, BUFFER_SIZE);
    counter = 0;
    do {
        read_out = read(fd, buffer, BUFFER_SIZE);
        for (i = 0; i < read_out; i++) {
            printf("%c", buffer[i]);
            if (buffer[i] == '{') {
                counter++;
            } else if (buffer[i] == '}') {
                counter--;
            } else if ((counter == 0) && (buffer[i] == ',')) {
                printf("\n");
            }
        }
    } while (read_out);

    close(fd);

    return 0;
}
