#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <errno.h>
#include "hashsum.h"
#include <fcntl.h>
#include <semaphore.h>
#include "string.h"
#include "requestQueue.h"
#include "../utils/constants.h"
#include "../utils/types.h"
#include "../utils/sope.h"

pthread_t bankoffice[MAX_BANK_OFFICES];
account_mutex_t bankaccounts[MAX_BANK_ACCOUNTS];
sem_t full, empty;

#define SHARED 0

bool verifyAccount(u_int32_t id, char* password){
    char hash[HASH_LEN + 1];
    hashGenerator(bankaccounts[id].account.salt, password, hash);
    if (strcmp(hash, bankaccounts[id].account.hash) == 0)
        return true;
    return false;
}

void saltGenerator(char *salt)
{

    char randomSalt[] = "0123456789abcdef";
    for (int i = 0; i < SALT_LEN; i++)
    {
        salt[i] = randomSalt[rand() % (strlen(randomSalt))];
    }

    salt[SALT_LEN] = '\0';
}

ret_code_t create_account(req_value_t rval)
{
    printf("RVAL before:\n");
    printf("\theader ID: %d\n", rval.header.account_id);
    printf("\tcreate ID: %d\n", rval.create.account_id);
    
    //req_value_t temp = rval;
    
    if (rval.header.account_id != ADMIN_ACCOUNT_ID) return RC_OP_NALLOW;
    
    if (!verifyAccount(rval.header.account_id, rval.header.password)) return RC_LOGIN_FAIL;

    printf("RVAL after:\n");
    printf("\theader ID: %d\n", rval.header.account_id);
    printf("\tcreate ID: %d\n", rval.create.account_id);


    //account_mutex_t newMut;
    /*if (bankaccounts[rval.create.account_id].account.hash[0] == "\0")
    {
        newMut.account.account_id = rval.create.account_id;
        newMut.account.balance = rval.create.balance;
        saltGenerator(newMut.account.salt);
        hashGenerator(newMut.account.salt, rval.create.password, newMut.account.hash);
        pthread_mutex_init(&newMut.mutex, NULL);
        bankaccounts[newMut.account.account_id] = newMut;
        printf("account doesn't exist");
        return RC_OK;
    }

    else
    {
        printf("account exists");
        return RC_ID_IN_USE;
    }*/
    return RC_ID_IN_USE;
}

void *officeprocessing(void *requestQueue)
{
    sem_wait(&full);
    reqQ_node_t *node = deQueue((reqQ_t *)requestQueue);
    switch (node->key.type)
    {
    case 0:
        create_account(node->key.value);
        break;
    default:
        break;
    }
    sem_post(&empty);
    pthread_exit(0);
}

void create_admin_acc(char *password)
{
    bankaccounts[ADMIN_ACCOUNT_ID].account.account_id = ADMIN_ACCOUNT_ID;
    bankaccounts[ADMIN_ACCOUNT_ID].account.balance = 0;
    saltGenerator(bankaccounts[ADMIN_ACCOUNT_ID].account.salt);
    hashGenerator(bankaccounts[ADMIN_ACCOUNT_ID].account.salt, password, bankaccounts[ADMIN_ACCOUNT_ID].account.hash);
}

ret_code_t terminationRequest(tlv_request_t request)
{
    if (request.value.header.account_id != ADMIN_ACCOUNT_ID)
        return RC_OP_NALLOW;

    if (!verifyAccount(request.value.header.account_id, request.value.header.password))
        return RC_LOGIN_FAIL;
    
    else return RC_OK;
}

int main(int argc, char *argv[])
{

    if (argc != 3)
    {
        printf("Wrong number of args!\n");
        return -1;
    }

    if (atoi(argv[1]) > 99)
    {
        printf("The number of bank offices must be at max 99!\n");
        return -1;
    }
    create_admin_acc(argv[2]);

    sem_init(&empty, SHARED, 1);
    sem_init(&full, SHARED, 0);

    reqQ_t *requestQueue = createQueue();

    if (mkfifo(SERVER_FIFO_PATH, 0666) == -1)
    {
        exit(8);
    }

    int serverFifo = open(SERVER_FIFO_PATH, O_RDONLY | O_NONBLOCK);
    if (serverFifo == -1)
        return -1;

    int dummyFifo = open(SERVER_FIFO_PATH, O_WRONLY);
    if (dummyFifo == -1)
        return -2;


    //for(int i = 1; i <= atoi(argv[1]); i++) pthread_create(&bankoffice[i], NULL, officeprocessing, requestQueue);
    tlv_request_t request;
    request.type = 0;
    do
    {
        int bytesRead;
        bytesRead = read(serverFifo, &request.type, sizeof(op_type_t));
        if (bytesRead > 0)
        {
            //sem_wait(&empty);
            read(serverFifo, &request.length, sizeof(uint32_t));
            read(serverFifo, &request.value, request.length);
            enQueue(requestQueue, request);

            reqQ_node_t *node = deQueue(requestQueue);

            printf("REQUEST:\n");
            printf("\theader ID: %d\n", request.value.header.account_id);
            printf("\tcreate ID: %d\n", request.value.create.account_id);

            printf("NODE:\n");
            printf("\theader ID: %d\n", node->key.value.header.account_id);
            printf("\tcreate ID: %d\n", request.value.create.account_id);

            create_account(node->key.value);
        }
    } while (1);

    //TODO: DIZER QUE ESPERE PELA TERMINAÇÃO DOS THREADS
    close(serverFifo);
    close(dummyFifo);
    unlink(SERVER_FIFO_PATH);
}
