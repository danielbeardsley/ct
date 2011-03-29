/* CT - (Relatively) Easy Unit Testing for C */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "internal.h"

static void
die(int code, const char *msg)
{
    putc('\n', stderr);

    if (msg && *msg) {
        fputs(msg, stderr);
        fputs(": ", stderr);
    }

    fputs(strerror(errno), stderr);
    putc('\n', stderr);
    exit(code);
}

static int
failed(int s)
{
    return WIFEXITED(s) && (WEXITSTATUS(s) == 255);
}

void
ct_report(T ts[], int n)
{
    int i, r, s;
    char buf[1024]; // arbitrary size
    int cf = 0, ce = 0;

    putchar('\n');
    for (i = 0; i < n; i++) {
        if (!ts[i].status) continue;

        printf("\n%s: ", ts[i].name);
        if (failed(ts[i].status)) {
            cf++;
            printf("failure");
        } else {
            ce++;
            printf("error");
            if (WIFEXITED(ts[i].status)) {
                printf(" (exit status %d)", WEXITSTATUS(ts[i].status));
            }
            if (WIFSIGNALED(ts[i].status)) {
                printf(" (signal %d)", WTERMSIG(ts[i].status));
            }
        }

        putchar('\n');
        lseek(ts[i].fd, 0, SEEK_SET);
        while ((r = read(ts[i].fd, buf, sizeof(buf)))) {
            s = fwrite(buf, 1, r, stdout);
            if (r != s) die(3, "fwrite");
        }
    }

    printf("\n%d tests; %d failures; %d errors.\n", n, cf, ce);
    exit(cf || ce);
}

void
ct_fail_(char *file, int line, char *exp, char *msg)
{
  printf("  %s:%d: (%s) %s\n", file, line, exp, msg);
  fflush(stdout);
  fflush(stderr);
  exit(-1);
}

void
ct_run(T *t, int i, ct_fn f, const char *name)
{
    pid_t pid;
    int status, r;
    FILE *out;

    if (i % 10 == 0) {
        if (i % 50 == 0) {
            putchar('\n');
        }
        printf("%5d", i);
    }

    t->name = name;

    out = tmpfile();
    if (!out) die(1, "tmpfile");
    t->fd = fileno(out);

    fflush(stdout);
    fflush(stderr);

    pid = fork();
    if (pid < 0) {
        die(1, "fork");
    } else if (!pid) {
        r = dup2(t->fd, 1); // send stdout to tmpfile
        if (r == -1) die(3, "dup2");

        r = close(t->fd);
        if (r == -1) die(3, "fclose");

        r = dup2(1, 2); // send stderr to stdout
        if (r < 0) die(3, "dup2");

        f();
        exit(0);
    }

    r = waitpid(pid, &status, 0);
    if (r != pid) die(3, "wait");

    t->status = status;
    if (!status) {
        // Since we won't need the (potentially large) output,
        // free its disk space immediately.
        close(t->fd);
        t->fd = -1;
        putchar('.');
    } else if (failed(status)) {
        putchar('F');
    } else {
        putchar('E');
    }

    fflush(stdout);
}
