/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once

#include <stdlib.h>

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

// struttura posizione nella memoria condivisa
typedef struct {
    int x;
    int y;
} Position;