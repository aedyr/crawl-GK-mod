#include "../platform.h"
#if defined(TARGET_OS_MACOSX)
  #include <util.h>
#else
  #include <pty.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

static pid_t crawl;
static int tty;

static void sigalrm(int signum)
{
    kill(crawl, SIGTERM);
}

static void kill_crawl(int signum)
{
    // I can't figure out any other signal than this that works. SIGTERM
    // prints a crash log, which is not what I'm going for. SIGHUP works
    // normally, but just seems to freeze a crawl process run from fake_pty
    // for some reason...
    kill(crawl, SIGKILL);
}

static void slurp_output()
{
    struct pollfd pfd;
    char buf[1024];

    signal(SIGALRM, sigalrm);
    // a hard limit of an hour (big tests on a Raspberry...), or 9 minutes on
    // Travis
    alarm(TIMEOUT * 60);

    pfd.fd     = crawl;
    pfd.events = POLLIN;

    while (poll(&pfd, 1, 60000) > 0) // 60 seconds with no output -> die die die!
    {
        if (read(tty, buf, sizeof(buf)) <= 0)
            break;
    }

    kill(crawl, SIGTERM); // shooting a zombie is ok, let's make sure it's dead
}

int main(int argc, char * const *argv)
{
    struct winsize ws;
    int replica;
    int ret;

    if (argc <= 1)
    {
        fprintf(stderr, "Usage: fake_pty program [args]\n");
        return 1;
    }

    ws.ws_row = 24;
    ws.ws_col = 80;
    ws.ws_xpixel = ws.ws_ypixel = 0;

    // We want to let stderr through, thus can't use forkpty().
    if (openpty(&tty, &replica, 0, 0, &ws))
    {
        fprintf(stderr, "Can't create a pty: %s\n", strerror(errno));
        return 1;
    }

    switch (crawl = fork())
    {
    case -1:
        fprintf(stderr, "Can't fork: %s\n", strerror(errno));
        return 1;

    case 0:
        close(tty);
        dup2(replica, 0);
        dup2(replica, 1); // but _not_ stderr!
        close(replica);
        execvp(argv[1], argv + 1);
        fprintf(stderr, "Can't run '%s': %s\n", argv[1], strerror(errno));
        return 1;

    default:
    {
        struct sigaction sa;
        sa.sa_handler = kill_crawl;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);

        /* Handle error */;
        close(replica);
        slurp_output();
        if (waitpid(crawl, &ret, 0) != crawl)
            return 1; // can't happen
        if (WIFEXITED(ret))
            return WEXITSTATUS(ret);
        if (WIFSIGNALED(ret))
            return 128 + WTERMSIG(ret);
        // Neither exited nor signaled?  Did the process eat mushrooms meant
        // fo the mother-in-law or what?
        return 1;
    }
    }
}
