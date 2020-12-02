#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include "zlib/zlib.h"

#define CIRCULAR_BUFFER_SIZE 1048576

/*
 * This program does the following:
 *
 * - reads data from stdin
 * - upon receiving SIGUSR1, print a suffix of the read data (as
 *   long as possible; therefore internally uses compression)
 */

void handle_sigusr1(int ignore);
void dump_zstream(z_stream *, unsigned char *, size_t);
int received_sigusr1 = 0;

int main(int argc, char **argv) {
    /* the circular buffer */
    unsigned char buf[CIRCULAR_BUFFER_SIZE];

    /* we read from stdin and store it in input */
    unsigned char input[BUFSIZ+1];

    /* a place to write to; is never going to be read */
    unsigned char scratch[BUFSIZ+1];

    z_stream compress_input;
    z_stream decompress_buf;

    scratch[BUFSIZ] = '\0';
    input[BUFSIZ] = '\0';

    /* clear both `z_stream`s */
    memset(&compress_input, 0, sizeof(z_stream));
    memset(&decompress_buf, 0, sizeof(z_stream));

    if (signal(SIGUSR1, handle_sigusr1)) {
        perror("circular: setting up handler of SIGUSR1");
    }

    /* 
     * compress_input starts reading from `input` (which has no
     * content yet) and writing to `buf` (which has
     * CIRCULAR_BUFFER_SIZE space)
     */

    compress_input.next_in = input;
    compress_input.avail_in = 0;
    compress_input.next_out = buf;
    compress_input.avail_out = CIRCULAR_BUFFER_SIZE;

    decompress_buf.next_in = buf;
    decompress_buf.avail_in = 0;
    decompress_buf.next_out = scratch;
    decompress_buf.avail_out = BUFSIZ;

    if (Z_OK != deflateInit(&compress_input, 9)) {
        fprintf(stderr, "Couldn't initialise compress_input: %s\n", 
                compress_input.msg);
        exit(2);
    }

    if (Z_OK != inflateInit(&decompress_buf)) {
        fprintf(stderr, "Couldn't initialise decompress_buf: %s\n", 
                decompress_buf.msg);
        exit(2);
    }

    /* initialise `decompress_buf` later */

    for (;;) {
        /* wait for input from stdin */
        fd_set fds;
        int fileno_stdin = fileno(stdin);
        fd_set empty;
        struct timeval timeout;
        timeout.tv_sec = 0xffff; /* wait long */
        timeout.tv_usec = 0xffff;
        FD_ZERO(&fds);
        FD_ZERO(&empty);

        FD_SET(fileno_stdin, &fds);

        /* wait forever for stdin or a signal */
        select(fileno_stdin+1, &fds, &empty, &empty, &timeout);

        if (received_sigusr1) {
            signal(SIGUSR1, handle_sigusr1); /* may need to set again */
            received_sigusr1 = 0;
            dump_zstream(&decompress_buf, buf, compress_input.next_out-buf);
        }

        if (FD_ISSET(fileno_stdin, &fds)) {
            ssize_t read_bytes = read(fileno_stdin, input, BUFSIZ);
            if (read_bytes == -1) {
                perror("circular: read from stdin");
            } else {
                compress_input.next_in = input;
                compress_input.avail_in = (uInt) read_bytes;

                do {
                    deflate(&compress_input, Z_NO_FLUSH);

                    /* do we need to make space in `buf`? */
                    while (!compress_input.avail_out) {
                        if (received_sigusr1) {
                            signal(SIGUSR1, handle_sigusr1); /* may need to set again */
                            received_sigusr1 = 0;
                            /* if decompress_buf was never read from, its field avail_in is not reliable */
                            if (!decompress_buf.total_in) decompress_buf.avail_in = compress_input.next_out-buf;
                            dump_zstream(&decompress_buf, buf, decompress_buf.total_in? compress_input.next_out-buf : 0);
                        }

                        decompress_buf.next_out = scratch;
                        decompress_buf.avail_out = BUFSIZ;

                        /* reached end of `buf`? */
                        if (!decompress_buf.avail_in) {
                            decompress_buf.next_in = buf;
                            decompress_buf.avail_in = CIRCULAR_BUFFER_SIZE;
                        }

                        inflate(&decompress_buf, Z_SYNC_FLUSH);

                        /* reached end of `buf`? */
                        if (compress_input.next_out == buf+CIRCULAR_BUFFER_SIZE) {
                            compress_input.next_out = buf;
                        }

                        compress_input.avail_out = decompress_buf.next_in - compress_input.next_out;
                    }
                } while (compress_input.avail_in);
            }
        }
    }
}

void handle_sigusr1(int ignore) {
    received_sigusr1 = 1;
}

void dump_zstream(z_stream *input, unsigned char *buf, size_t space_left) {
    z_stream copy;
    unsigned char scratch[BUFSIZ];
    int fileno_stdout = fileno(stdout);

    inflateCopy(&copy, input);

    /* first, decompress from current position till end of `buf` */
    while (copy.avail_in) {
        copy.avail_out = BUFSIZ;
        copy.next_out = scratch;
        inflate(&copy, Z_SYNC_FLUSH);
        write(fileno_stdout, scratch, BUFSIZ-copy.avail_out);
    }

    /* then, decompress from beginning of `buf` */
    copy.next_in = buf;
    copy.avail_in = space_left;

    while (copy.avail_in) {
        copy.avail_out = BUFSIZ;
        copy.next_out = scratch;
        inflate(&copy, Z_SYNC_FLUSH);
        write(fileno_stdout, scratch, BUFSIZ-copy.avail_out);
    }

    inflateEnd(&copy);
}
