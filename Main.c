#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_CLIENTS 5
#define NUM_MOVIES 5
#define MAX_SEATS 50

// if is_autorized est vrai, le client doit avoir un certain age pour acheter des billets
typedef struct
{
    char movie_name[50];
    int seats_available;
    bool is_autorized;
    int age;
} Movie;

Movie movies[NUM_MOVIES] = {
    {"Le Loup De Walstreet", MAX_SEATS, true, 12},
    {"Harry Potter 1", MAX_SEATS, false, 0},
    {"50 nuances de grey", MAX_SEATS, true, 16},
    {"Transformers", MAX_SEATS, false, 0},
    {"Terminator", MAX_SEATS, false, 0}};

typedef struct
{
    long msg_type;
    char movie_name[50];
    int num_tickets;
    int age;
} Client;

int main()
{
    int msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT); // Créer une file de messages IPC

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        printf("Client %d\n", i + 1);
        if (fork() == 0)
        {                    // Créer un processus enfant
            srand(getpid()); // Utiliser le PID comme graine pour rand()

            Client client;
            client.msg_type = 1;
            client.age = (rand() % 100) + 1;

            int random_movie_index = rand() % NUM_MOVIES; // Génère un index aléatoire pour le film
            int random_ticket_num = (rand() % 5) + 1;     // Génère un nombre aléatoire de billets entre 1 et 5

            strcpy(client.movie_name, movies[random_movie_index].movie_name);
            client.num_tickets = random_ticket_num;

            msgsnd(msgid, &client, sizeof(client), 0); // Envoyer la demande de billets à la file de messages

            exit(0); // Terminer le processus enfant
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++)
    { // Traiter les demandes de billets
        Client client;
        msgrcv(msgid, &client, sizeof(client), 1, 0); // Recevoir une demande de billets de la file de messages

        // Décompter le nombre de places disponibles pour le film correspondant
        for (int j = 0; j < NUM_MOVIES; j++)
        {
            if (strcmp(client.movie_name, movies[j].movie_name) == 0)
            {
                movies[j].seats_available -= client.num_tickets;
                break;
            }
        }
    }

    // Imprimer le nombre de places disponibles pour chaque film
    for (int i = 0; i < NUM_MOVIES; i++)
    {
        printf("Nombre de places disponibles pour %s = %d\n", movies[i].movie_name, movies[i].seats_available);
    }

    msgctl(msgid, IPC_RMID, NULL); // Supprimer la file de messages

    return 0;
}
