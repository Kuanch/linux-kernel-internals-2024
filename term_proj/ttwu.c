#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void *sleeper_function(void *arg) {
    printf("Sleeper thread going to sleep.\n");
    sleep(5);
    printf("Sleeper thread waking up.\n");
    return NULL;
}

void *waiter_function(void *arg) {
    pthread_t sleeper_thread = *(pthread_t *)arg;
    printf("Waiter thread waiting for sleeper thread to finish.\n");
    pthread_join(sleeper_thread, NULL);
    printf("Waiter thread continuing.\n");
    return NULL;
}

int main() {
    pthread_t sleeper_thread, waiter_thread;

    pthread_create(&sleeper_thread, NULL, sleeper_function, NULL);
    pthread_create(&waiter_thread, NULL, waiter_function, &sleeper_thread);

    pthread_join(waiter_thread, NULL);

    return 0;
}