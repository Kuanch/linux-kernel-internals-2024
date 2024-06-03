#include <syslog.h>
#include <signal.h>

static int stop = 0;

static void sigint_handler(int sig)
{
    (void) sig;
    stop = 1;
}

int main()
{
    int i = 0;
    signal(SIGINT, sigint_handler);
    while(1){
        i++;
        printf("i = %d\n", i);
        if (stop) {
            syslog(LOG_INFO, "Caught SIGINT, exiting now");
            break;
        }
    }
}