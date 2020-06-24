/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once

#include <stdlib.h>
#include <stdio.h>

// struttura messaggi client-device o device-device
typedef struct {
    pid_t pid_sender;
    pid_t pid_receiver;
    int message_id;
    char message[256];
    int max_distance;
} Message;

// struttura ack salvati nella memoria condivisa
typedef struct {
    pid_t pid_sender;
    pid_t pid_receiver;
    int message_id;
    time_t timestamp;
} Acknowledgment;

typedef struct {
    long mtype;
    Acknowledgment acklist[5];
} msgq_ack;

// struttura posizione nella memoria condivisa
typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    long mtype;
    Position p;
} msgq_position;

struct deviceMessage_ {
  Message m;
  struct deviceMessage_ *next;
};

typedef struct deviceMessage_ deviceMessage;

/*inserisce un nuovo numero in coda alla lista*/
deviceMessage *putMessage(deviceMessage* lista, Message message) {
    deviceMessage *prec;
    deviceMessage *tmp;

    tmp = (deviceMessage*) malloc(sizeof(deviceMessage));
    if(tmp != NULL){
        tmp->next = NULL;
        tmp->m = message;
        if(lista == NULL)
            lista = tmp;
        else{
            /*raggiungi il termine della lista*/
            for(prec=lista; prec->next!=NULL; prec=prec->next);
            prec->next = tmp;
        }
    } else
        printf("Memoria esaurita!\n");
    return lista;
}

/*libera la memoria allocata per una lista*/
deviceMessage* removeAllDeviceQueue(deviceMessage* lista){
  deviceMessage* tmp;
  while(lista!= NULL){
    tmp = lista;
    lista = lista->next;
    free(tmp);
  }
  return NULL;
}

deviceMessage* removeMessage(deviceMessage* lista, int id){
  deviceMessage *curr, *prec, *canc;
  int found;

  found=0;
  curr = lista;
  prec = NULL;      
  while(curr && ! found){
    if(curr->m.message_id == id){
      found = 1;
      canc = curr;
      curr = curr->next;     
      if(prec!=NULL)
        prec->next = curr;
      else
        lista = curr;
      free(canc);
    }
    else{
      prec=curr;
      curr = curr->next;     
    }
  }
  return lista;
}