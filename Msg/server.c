//
// Created by s-gheldd on 4/27/16.
//
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <zconf.h>
#include <pthread.h>
#include <memory.h>
#include "mq.h"
#include "msg_linked_set.h"


struct thread_information {
    pid_t pid_client;
    int msq_id;
};

void *answer_challenge(void *information) {

    struct thread_information *thread_info = (struct thread_information *) information;
    pid_t pid_client = thread_info->pid_client;
    int msq_id = thread_info->msq_id;
    pthread_t thread_id = pthread_self();
    struct msg_message send_message;
    send_message.mtyp = pid_client;
    sprintf(send_message.text, "%d\n", thread_id);
    msgsnd(msq_id, &send_message, MSG_SIZE, 0);
    free(information);
    pthread_exit(NULL);
}

int main() {
    int msq_id = msgget(MQ_KEY, IPC_PRIVATE | IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    struct msqid_ds msqid_ds;
    int child_count = 0;
    struct list_node *root = NULL;
    struct list_node **root_ptr = &root;
    pid_t pid_server, pid_client;
    pid_server = getpid();

    if (msq_id == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    while (1) {
        struct msg_message rec_message, send_message;
        struct thread_information *thread_info;
        pthread_t thread_id;
        int rc;


        rec_message.mtyp = 1;
        msgrcv(msq_id, &rec_message, MSG_SIZE, 1, 0);
        sscanf(rec_message.text, "%d", &pid_client);

        if (pid_client == -1) {
            child_count--;
        } else if (msg_linked_set_add(root_ptr, pid_client)) {
            child_count++;
            printf("New Client: %d\n", pid_client);
        }
        if (!child_count) {
            break;
        }
        thread_info = malloc(sizeof(struct thread_information));
        if (thread_info == NULL) {
            perror("malloc thread info");
            break;
        }
        memset(thread_info, '\0', sizeof(struct thread_information));

        thread_info->msq_id = msq_id;
        thread_info->pid_client = pid_client;

        rc = pthread_create(&thread_id, NULL, answer_challenge, (void *) thread_info);
        if (rc) {
            perror("pthread create");
            break;
        }

    }

    sleep(1);
    if (msgctl(msq_id, IPC_RMID, &msqid_ds) == -1) {
        perror("msgctl");
        exit(EXIT_FAILURE);
    }
    msg_linked_set_free(root_ptr);
    printf("Server done, shutting down.\n");
    pthread_exit(NULL);
    return EXIT_SUCCESS;
}