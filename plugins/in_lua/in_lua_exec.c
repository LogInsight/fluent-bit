//
// Created by mayabin on 16/1/5.
//

#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>

#include "in_lua_exec.h"
#include "in_lua.h"

static sem_t sem_signal = -1;

struct flb_in_lua_exec_info *exec_entry;

void *in_lua_exec_done(void *msg);

void in_lua_exec_init(struct flb_in_lua_config *ctx) {
    int res = 0;
    pid_t pid = -1;


    /*
    res = sem_init(&sem_signal, 0, 0);
    if (-1 == res) {
        flb_error("lua exec init failed. sem init failed.");
        return;
    }

    res = pipe(stdout_pipe);
    if (-1 == res) {
        flb_error("lua exec init failed. stdout pipe create failed.");
        sem_destroy(&sem_signal);
        sem_signal = -1;
        return;
    }

    res = pipe(stderr_pipe);
    if (-1 == res) {
        flb_error("lua exec init failed. stdout pipe create failed.");
        close(stdout_pipe[0]);
        close[stdout_pipe[1]];
        sem_destroy(&sem_signal);
        sem_signal = -1;
        return;
    }

    exec_entry = NULL;
    res = pthread_create(&pid, NULL, in_lua_exec_done, exec_entry);

    if (-1 == res) {
        flb_error("lua exec init failed. pthread create failed.");
        close(stdout_pipe[0]);
        close[stdout_pipe[1]];
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        sem_destroy(&sem_signal);
        return;
    }
*/
    return;
}



void *in_lua_exec_done(void *msg)
{
    struct flb_in_lua_exec_info *entry = msg;


    for (;;) {
        sem_wait(&sem_signal);
    }
}