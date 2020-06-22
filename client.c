/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"
#include "err_exit.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <time.h>
 
// usato per leggere un intero da tastiera
int readInt(const char *s) {
    char *endptr = NULL;

    errno = 0;
    long int res = strtol(s, &endptr, 10);

    if (errno != 0 || *endptr != '\n' || res < 0) {
        printf("invalid input argument\n");
        exit(1);
    }

    return res;
}

// converte un timestamp in stringa
char* timestampToString(time_t timestamp) {
//   struct tm time = *localtime(&timestamp);
//   char* s[30];
//   sprintf(s, "%d-%02d-%02d %02d:%02d:%02d\n", time.tm_year + 1900, time.tm_mon + 1, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
}

// crea il file con gli ack
void createFile(Message message, Acknowledgment acklist[5]) {

	printf("Messaggio %d: %s\n", message.message_id, message.message);
	// creo il file
}

char *baseDeviceFIFO = "/tmp/def_fifo.";

int main(int argc, char * argv[]) {

    // controllo che inserisca la key della message queue come paramentro
    if (argc != 2) {
        printf("Usage: %s message_queue_key\n", argv[0]);
        exit(1);
    }

    // legge la message queue dall'argomento e se è <0 fallisce
    int msg_queue_ack_key = atoi(argv[1]);
    if (msg_queue_ack_key <= 0) {
        printf("The message queue key must be greater than zero!\n");
        exit(1);
    }

    // prende l'ID della message queue se c'è altrimenti fallisce
    int msqid = msgget(msg_queue_ack_key, S_IRUSR | S_IWUSR);
    if (msqid == -1)
      ErrExit("msgget failed");

    char buffer[10];
    size_t len;

    // crea un message data struct
    Message message;

    // legge il pid del device a cui mandare il messaggio
    printf("Inserire pid del device: ");
    fgets(buffer, sizeof(buffer), stdin);
    message.pid_receiver = readInt(buffer);

    // legge l'ID del messaggio
    printf("Inserire ID messaggio: ");
    fgets(buffer, sizeof(buffer), stdin);
    message.message_id = readInt(buffer);

    // legge il messaggio
    printf("Inserire il messaggio: ");
    fgets(message.message, sizeof(message.message), stdin);
    len = strlen(message.message);
    message.message[len - 1] = '\0';

    // legge la distanza massima
    printf("Inserire la distanza massima: ");
    fgets(buffer, sizeof(buffer), stdin);
    message.max_distance = readInt(buffer);
    
    // imposta il pid proprio
    message.pid_sender = getpid();


    // MANDARE MESSAGGIO AL DEVICE

    // 1. Creazione path della FIFO del device
    char path2DeviceFIFO[25];
    sprintf(path2DeviceFIFO, "%s%d", baseDeviceFIFO, message.pid_receiver);

    // 2. Apertura FIFO in scrittura
    printf("<Client> opening FIFO %s...\n", path2DeviceFIFO);
    int deviceFIFO = open(path2DeviceFIFO, O_WRONLY);
    if (deviceFIFO == -1)
        ErrExit("open failed");

    printf("<Client> sending message to device %d on FIFO in %s\n", message.pid_receiver, path2DeviceFIFO);
    if (write(deviceFIFO, &message, sizeof(Message)) != sizeof(Message))
        ErrExit("write failed");
	printf("<Client> Message sended! Wait to receive all device!\n");

	// ATTESA MESSAGE QUEUE

	struct ack_msg {
		long mtype;
		Acknowledgment acklist[5];
	} m;
	
	// read a message from the message queue
	size_t mSize = sizeof(struct ack_msg) - sizeof(long);
	if (msgrcv(msqid, &m, mSize, message.message_id, 0) == -1)
		ErrExit("msgget failed");

	createFile(message, m.acklist);

	printf("File created. I will die bye!");
    return 0;
}