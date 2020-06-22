/// @file server.c
/// @brief Contiene l'implementazione del SERVER.

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <math.h>
#include <unistd.h>
#include <time.h>

#define MAX_ACK 20
#define N_DEVICE 5
#define SIZE 10
#define LINESIZE 19

char *baseDeviceFIFO = "/tmp/def_fifo.";

int msg_queue_ack_key = -1;

int msqid_client = -1;
int shmidScacchiera = -1;
int *shmptrScacchiera;
int shmidAckList = -1;
Acknowledgment *shmptrAckList;

int semid_ack_list = -1;
int semid_scacchiera = -1;
int semid_devices = -1;
int semid_ack_delete = -1;

// sigterm handler for server
void sigTermServerHandler(int sig) {
    printf("<Server> SIGTERM Ricevuta, termino i figli!\n");
	kill(0, SIGTERM);
	
	int status = 0;
	while (wait(&status) != -1);

    printf("<Server> Removing Shared Memory Scacchiera...");
	free_shared_memory(shmptrScacchiera);
    remove_shared_memory(shmidScacchiera);
	printf(" [DONE]\n");
	

    printf("<Server> Removing Shared Memory Ack List...");
	free_shared_memory(shmptrAckList);
    remove_shared_memory(shmidAckList);
	printf(" [DONE]\n");

    printf("<Server> Removing the Ack List semaphore set...");
    if (semctl(semid_ack_list, 0 /*ignored*/, IPC_RMID, NULL) == -1) {
        ErrExit("semctl IPC_RMID failed");	
	}
	printf(" [DONE]\n");

	printf("<Server> Removing the Ack Delete semaphore set...");
    if (semctl(semid_ack_delete, 0 /*ignored*/, IPC_RMID, NULL) == -1) {
        ErrExit("semctl IPC_RMID failed");	
	}
	printf(" [DONE]\n");

	printf("<Server> Removing the Devices semaphore set...");
    if (semctl(semid_devices, 0 /*ignored*/, IPC_RMID, NULL) == -1){
        ErrExit("semctl IPC_RMID failed");
	}
	printf(" [DONE]\n");

	printf("<Server> Removing the Scacchiera semaphore set...");
    if (semctl(semid_scacchiera, 0 /*ignored*/, IPC_RMID, NULL) == -1){
        ErrExit("semctl IPC_RMID failed");
	}
	printf(" [DONE]\n");

	exit(0);
}

// sigterm handler for devices
void sigTermDeviceHandler(int sig) {
	printf("<Device X> Chiudo la mia FIFO...");
	char pathDeviceFIFO[25];
	sprintf(pathDeviceFIFO, "%s%d", baseDeviceFIFO, getpid());
	if (unlink(pathDeviceFIFO) != 0)
        ErrExit("unlink failed");
	printf(" [DONE]\n");
	exit(0);
}

void sigTermAckHandler(int sig) {
	// if (msqid_client >= 0) {
    //     if (msgctl(msqid_client, IPC_RMID, NULL) == -1)
    //         ErrExit("msgctl failed");
    //     else
    //         printf("<Ack Manager> Message Queue removed successfully\n");
    // }

	printf("<Ack Manager> Rimuovo la Message Queue...");
	if (msgctl(msqid_client, IPC_RMID, NULL) == -1)
    	ErrExit("msgctl failed");
	printf(" [DONE]\n");

    exit(0);
}

void initScacchiera() {
	for(int i=0; i<SIZE; i++)
		for(int j=0; j<SIZE; j++)
			shmptrScacchiera[SIZE * i + j] = 0;
}

int findNearDevice(int near_device[], int x, int y, int max_distance) {
	int n = 0;
	int distance;
	for(int i=0; i<SIZE; i++) {
		for(int j=0; j<SIZE; j++) {
			if(shmptrScacchiera[SIZE * i + j] != 0) {
				distance = sqrt(pow (i-y, 2) + pow (j-x, 2));
				// printf("<Device> This is distant %d from my device", distance);
				if(distance <= max_distance && distance != 0) {
					near_device[n] = shmptrScacchiera[SIZE * i + j];
					n++;
				}
			}
		}
	}
	return n;
}

int readPositionFromFile(Position device_position[], int file, int line) {	
	char buffer;

	lseek(file, (LINESIZE+1)*line, SEEK_SET);

	for(int i=0; i<N_DEVICE; i++){

		if(i>0) read(file, &buffer, 1);
		
		ssize_t ok = read(file, &buffer, 1);
		if (!ok) //EOF
			return 0;
		else lseek(file, -1, SEEK_CUR);

		read(file, &buffer, 1);
		device_position[i].x = atoi(&buffer);

		read(file, &buffer, 1);

		read(file, &buffer, 1);
		device_position[i].y = atoi(&buffer);
	}
	return 1;
}

void changeDevicePosition(int pid, Position old, Position new) {
	printf("Cambio posizione a %d mettendolo da [%d, %d] a [%d, %d] \n", pid, old.x, old.y, new.x, new.y);	
	shmptrScacchiera[SIZE * old.y + old.x] = 0;
	shmptrScacchiera[SIZE * new.y + new.x] = pid;
}

void signalInit() {
	sigset_t mySet;
    sigfillset(&mySet);
    sigdelset(&mySet, SIGTERM);
    sigprocmask(SIG_SETMASK, &mySet, NULL);

    if (signal(SIGTERM, sigTermServerHandler) == SIG_ERR) {
        ErrExit("change signal handler failed\n");
	}
}

void shmAndSemInit() {
	// genera la shared memory della scacchiera
	int shm_scacchiera_key = (int)(((double)rand() / RAND_MAX) * 255);
    printf("<Server> allocating a shared memory segment for scacchiera...");
    shmidScacchiera = alloc_shared_memory(shm_scacchiera_key, sizeof(int)*SIZE*SIZE);
	shmptrScacchiera = (int*)get_shared_memory(shmidScacchiera, 0);
	printf(" [DONE]\n");
	initScacchiera();	

	// genera la shared memory della lista di ack
	int shm_ack_list_key = (int)(((double)rand() / RAND_MAX) * 255);
    printf("<Server> allocating a shared memory segment for ack list...");
    shmidAckList = alloc_shared_memory(shm_ack_list_key, sizeof(Acknowledgment)*MAX_ACK);
	shmptrAckList = (Acknowledgment*) get_shared_memory(shmidAckList, 0);
	printf(" [DONE]\n");

	// genera il semaforo per la shared memory della scacchiera
	printf("<Server> Generating semaphore for scacchiera");
    semid_scacchiera = semget(IPC_PRIVATE, 1, S_IRUSR | S_IWUSR);
    if (semid_scacchiera == -1){
        ErrExit("semget failed");
	}
	printf(" [DONE]\n");

	printf("<Server> Setting initial value semaphore for scacchiera");
	union semun arg_sem_scacchiera;
    arg_sem_scacchiera.val = 1;
	if (semctl(semid_scacchiera, 0, SETVAL, arg_sem_scacchiera) == -1)
        ErrExit("semctl scacchiera SETALL failed");
	printf(" [DONE]\n");

	// genera il semaforo per la shared memory della lista di ack
	printf("<Server> Generating semaphore for Ack List");
    semid_ack_list = semget(IPC_PRIVATE, 1, S_IRUSR | S_IWUSR);
    if (semid_ack_list == -1) {
        ErrExit("semget failed");
	}
	printf(" [DONE]\n");

	printf("<Server> Setting initial value semaphore for Ack List");
	union semun arg_sem_ack_list;
    arg_sem_ack_list.val = 1;
	if (semctl(semid_ack_list, 0, SETVAL, arg_sem_ack_list) == -1)
        ErrExit("semctl ack list SETALL failed");
	printf(" [DONE]\n");

	// FARE IL SEMAFORO PER GESTIRE LA CANCELLAZIONE SICURA DEGLI ACK

	// genera il semaforo per il delete degli ack
	printf("<Server> Generating semaphore for Ack Delete");
    semid_ack_delete = semget(IPC_PRIVATE, 1, S_IRUSR | S_IWUSR);
    if (semid_ack_delete == -1) {
        ErrExit("semget failed");
	}
	printf(" [DONE]\n");

	printf("<Server> Setting initial value semaphore for Ack Delete");
	union semun arg_sem_ack_delete;
    arg_sem_ack_delete.val = 0;
	if (semctl(semid_ack_delete, 0, SETVAL, arg_sem_ack_delete) == -1)
        ErrExit("semctl ack delete SETALL failed");
	printf(" [DONE]\n");

	// genera semafori per muovere i device
	printf("<Server> Generating semaphore for Devices");
    semid_devices = semget(IPC_PRIVATE, 5, S_IRUSR | S_IWUSR);
    if (semid_devices == -1){
        ErrExit("semget failed");
	}
	printf(" [DONE]\n");

	printf("<Server> Setting initial value semaphore for Devices");
    unsigned short semInitVal[] = {1, 0, 0, 0, 0};
    union semun arg_sem_devices;
    arg_sem_devices.array = semInitVal;
    if (semctl(semid_devices, 0 /*ignored*/, SETALL, arg_sem_devices) == -1){
        ErrExit("semctl devices SETALL failed");
	}
	printf(" [DONE]\n");
	
}

int howmuch(int message_id) {
	int n = 0;
	for(int i=0; i<MAX_ACK; i++) {
		if(shmptrAckList[i].message_id == message_id) n++;
	}
	return n;
}

void ackManager() {
	printf("<Ack Manager> Making Message Queue...");
	msqid_client = msgget(msg_queue_ack_key, IPC_CREAT | S_IRUSR | S_IWUSR);
	if (msqid_client == -1)
		ErrExit("msgget failed");
	printf(" [DONE]\n");

	if (signal(SIGTERM, sigTermAckHandler) == SIG_ERR) {
		ErrExit("change signal handler failed\n");
	}

	while(1) {
		// printf("<Ack Manager> Controllo se i device hanno ricevuto il messaggio!\n");

		for(int i=0; i<MAX_ACK; i++) {
			printf("<ACK %2d> %d %d %d\n", i, shmptrAckList[i].pid_sender, shmptrAckList[i].pid_receiver, shmptrAckList[i].message_id);
		}

		for(int i=0; i<MAX_ACK; i++) {
			if(howmuch(shmptrAckList[i].message_id) == N_DEVICE) {
				semOp(semid_ack_delete, 0, -1);
				printf("<Ack Manager> Tolgo gli ack dalla lista!");
				// preparo il messaggio da scrivere nella shared memory del client
				
			}
		}

		sleep(5);
	}
}

int checkAckList(int pid, int message_id) {
	for(int k=0; k<MAX_ACK; k++) {
		if(shmptrAckList[k].message_id == message_id && shmptrAckList[k].pid_receiver == pid) {
			return 1;
		}
	}
	return 0;
}

void device(int device_number, Position p) {
	semOp(semid_devices, (unsigned short) device_number, -1);
	pid_t pid = getpid();
	printf("<Server> Device %d created with PID %d\n", device_number, pid);
	if (signal(SIGTERM, sigTermDeviceHandler) == SIG_ERR) {
		ErrExit("change signal handler failed\n");
	}

	printf("<Device %d> Making FIFO...", device_number);
	char pathDeviceFIFO[25];
	sprintf(pathDeviceFIFO, "%s%d", baseDeviceFIFO, pid);
	if (mkfifo(pathDeviceFIFO, S_IRUSR | S_IWUSR | S_IWGRP) == -1)
		ErrExit("mkfifo failed");
	printf(" [DONE]\n");

	printf("<Device %d> waiting for a message...\n", device_number);
	int deviceFIFO = open(pathDeviceFIFO, O_RDONLY | O_NONBLOCK);
	if (deviceFIFO == -1)
		ErrExit("open failed");

	semOp(semid_scacchiera, 0, -1);
	shmptrScacchiera[SIZE * p.y + p.x] = pid;
	semOp(semid_scacchiera, 0, 1);

	semOp(semid_devices, (unsigned short) device_number, 1);

	int device_near[N_DEVICE];
	int near;

	while(1) {
		semOp(semid_devices, (unsigned short) device_number, -1);
		semOp(semid_scacchiera, 0, -1);

		/*
			4. Cambia posizione nella shared memory leggendola nella shared memory
		*/

		Message m;
		int bR = -1;
		int errno;
		char path2NearDeviceFIFO[25];
		int deviceNearFIFO;

		do {
			bR = read(deviceFIFO, &m, sizeof(Message));
			if (bR == -1 && errno != EAGAIN)
				printf("<Server> it looks like the FIFO is broken\n");
			if (bR == sizeof(Message)) {
				// WRITE ACK ON ACK LIST
				semOp(semid_ack_list, 0, -1);
				int alreadyReceived = checkAckList(pid, m.message_id);
				if(!alreadyReceived) {
					printf("<Device %d> Un nuovo messaggio trovato!\n", device_number);
					for(int k=0; k<MAX_ACK; k++) {
						if(shmptrAckList[k].message_id == 0) {
							printf("<Device %d> Salvo un ACK...", device_number);
							Acknowledgment ack;
							ack.pid_sender = m.pid_sender;
							ack.pid_receiver = pid;
							ack.message_id = m.message_id;
							ack.timestamp = time(NULL);
							shmptrAckList[k] = ack;
							printf(" [DONE]\n");
							break;
						}
					}

					near = findNearDevice(device_near, p.x, p.y, m.max_distance);
					m.pid_sender = pid;
					for(int i=0; i<near; i++) {
						// SEND MESSAGE TO NEAR DEVICE
						m.pid_receiver = device_near[i];
						sprintf(path2NearDeviceFIFO, "%s%d", baseDeviceFIFO, device_near[i]);
						deviceNearFIFO = open(path2NearDeviceFIFO, O_WRONLY);
						if (deviceNearFIFO == -1)
							ErrExit("open failed");
						if (write(deviceNearFIFO, &m, sizeof(Message)) != sizeof(Message))
							ErrExit("write failed");
					}
				} else if(howmuch(m.message_id) == 5) {
					// sblocca il semaforo della cancellazione con ack list
					semOp(semid_ack_delete, 0, 1);
				}
				semOp(semid_ack_list, 0, 1);
			}
		} while(!((bR == -1 && errno == EAGAIN) || (bR == 0)));

		
		semOp(semid_scacchiera, 0, 1);
		semOp(semid_devices, (unsigned short) (device_number == 4) ? 0 : device_number + 1, 1);
	}
}

void printScacchiera() {
	for(int i=0; i<SIZE; i++) {
		for(int j=0; j<SIZE; j++){
			printf("%5d", shmptrScacchiera[SIZE * i + j]);
		}
		printf("\n");
	}
	printf("##################### \n");
}

int main(int argc, char * argv[]) {

	// controllo che inserisca i giusti paramentri
    if (argc != 3) {
        printf("Usage: %s msg_queue_key file_posizioni\n", argv[0]);
        exit(1);
    }

    // legge la message queue dall'argomento e se è <0 fallisce
    msg_queue_ack_key = atoi(argv[1]);
    if (msg_queue_ack_key <= 0) {
        printf("The message queue key must be greater than zero!\n");
        exit(1);
    }

	// legge il nome del file dall'argomento
	char* filename = argv[2];

	// Stampo il PID del server per comodità
	printf("<Server> PID: %d\n", getpid());

	// GESTIONE SEGNALI
	signalInit();

	// GENERO LE MEMORIE E SEMAFORI NECESSARI
	shmAndSemInit();

	// ACK MANAGER
	pid_t pid = fork();
	if (pid == -1)
		printf("Ack managare not created!\n");
	else if (pid == 0) {
		ackManager();
	}
	
	Position device_position[N_DEVICE];

	int file = open(filename, O_RDONLY);
	if (file == -1) {
		ErrExit("File not found");
	}
	int line = 0;

	semOp(semid_scacchiera, 0, -1);

	readPositionFromFile(device_position, file, line);

	semOp(semid_scacchiera, 0, 1);

	// DEVICE
	for (int i = 0; i < 5; ++i) {
        pid = fork();
        if (pid == -1)
            printf("Device %d not created!\n", i);
        else if (pid == 0) {
			device(i, device_position[i]);
        }
    }

	line++;

	// ogni due secondi muove i device con le posizioni lette dal file	
	while(1) {

		for(int i=0; i<N_DEVICE; i++) {
			// aggiungo alla shared memory del device le posizioni
		}

		if(readPositionFromFile(device_position, file, line))
			line++;
		else {
			line = 0;
		}

		sleep(2);
	}

    return 0;
}