#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define MAX_CLIENTS 2
#define NUM_MOVIES 5
#define MAX_SEATS 50
#define MAX_MOVIE_PREFERENCES 3

const char *prenom[] = {
    "Jean", "Marie", "Pierre", "Paul", "Jacques", "Francois", "Nicolas", "Michel", "Louis", "Claude"};

const char *nom[] = {
    "Martin", "Bernard", "Thomas", "Petit", "Robert", "Richard", "Durand", "Dubois", "Moreau", "Laurent"};

typedef struct
{
    char movie_name[50];
    int seats_available;
    bool is_autorized;
    int age;
} Movie;

typedef struct
{
    long msg_type;
    char movie_preferences[MAX_MOVIE_PREFERENCES][50];
    int num_tickets;
    char nom[50];
    char prenom[50];
    int age;
} Client;

Movie movies[NUM_MOVIES] = {
    {"Le Loup De Walstreet", MAX_SEATS, true, 12},
    {"Harry Potter 1", MAX_SEATS, false, 100},
    {"50 nuances de grey", MAX_SEATS, true, 16},
    {"Transformers", MAX_SEATS, false, 0},
    {"Terminator", MAX_SEATS, false, 0}};
void generate_clients(int msgid, Movie *shared_movies)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (fork() == 0)
        {
            srand(getpid());

            Client client;
            client.msg_type = 1;

            strcpy(client.nom, nom[rand() % 10]);
            strcpy(client.prenom, prenom[rand() % 10]);
            client.age = (rand() % 100) + 1;

            printf(" %s %s qui a %d ans et a le numéro de processus %d\n", client.prenom, client.nom, client.age, getpid());

            int random_movie_index = rand() % NUM_MOVIES;
            strcpy(client.movie_preferences[0], shared_movies[random_movie_index].movie_name);

            int random_ticket_num = (rand() % 5) + 1; // Générer aléatoirement le nombre de billets une seule fois
            client.num_tickets = random_ticket_num;

            msgsnd(msgid, &client, sizeof(client), 0);
            exit(0); // Terminer le processus enfant
        }
    }
}

void process_ticket_requests(int msgid, Movie *shared_movies, int semid)
{
    // Structure pour recevoir les messages des clients
    struct message
    {
        long mtype;
        char movie_preferences[MAX_MOVIE_PREFERENCES][50];
        int num_tickets;
        char nom[50];
        char prenom[50];
        int age;
    } message;

    // Boucle pour traiter les demandes de tous les clients
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        // Recevoir le message du client
        msgrcv(msgid, &message, sizeof(message), 1, 0);

        // Utiliser le même nombre de billets pour tous les films
        int random_ticket_num = message.num_tickets;

        int reserved_tickets = 0;

        // Parcourir les préférences de films du client
        for (int j = 0; j < MAX_MOVIE_PREFERENCES; j++)
        {
            int random_movie_index = rand() % NUM_MOVIES;
            strcpy(message.movie_preferences[j], shared_movies[random_movie_index].movie_name);

            // Avant de réserver des billets pour un film :
            struct sembuf p = {random_movie_index, -1, 0}; // Utiliser l'index généré aléatoirement du film
            semop(semid, &p, 1);

            // Vérifier si le client est assez âgé pour le film
            if (message.age >= shared_movies[random_movie_index].age)
            {
                // Réserver les billets...
                if (shared_movies[random_movie_index].seats_available >= random_ticket_num)
                {
                    shared_movies[random_movie_index].seats_available -= random_ticket_num;
                    reserved_tickets += random_ticket_num;

                    printf("Le client %s a réservé %d billets pour %s\n", message.nom, reserved_tickets, shared_movies[random_movie_index].movie_name);
                    break; // Sortir de la boucle après la première réservation
                }
                else
                {
                    printf("Le client %s n'a pas pu réserver %d billets pour %s car il ne reste que %d places disponibles\n", message.nom, random_ticket_num, shared_movies[random_movie_index].movie_name, shared_movies[random_movie_index].seats_available);
                }
            }
            else
            {
                printf("Le client %s n'est pas assez âgé pour voir %s\n", message.nom, shared_movies[random_movie_index].movie_name);
            }

            // Après avoir réservé les billets :
            struct sembuf v = {random_movie_index, 1, 0};
            semop(semid, &v, 1);
        }
    }
}

void print_seats_available(Movie *shared_movies)
{
    printf("\n-------------Places disponibles-------------\n");
    // Imprimer le nombre de places disponibles pour chaque film
    for (int i = 0; i < NUM_MOVIES; i++)
    {
        printf("Nombre de places disponibles pour %s = %d\n", shared_movies[i].movie_name, shared_movies[i].seats_available);
    }
}

int main()
{

    // Créer un tableau de sémaphores, un pour chaque film
    int semid = semget(IPC_PRIVATE, NUM_MOVIES, 0666 | IPC_CREAT);

    // Initialiser tous les sémaphores à 1
    for (int i = 0; i < NUM_MOVIES; i++)
    {
        semctl(semid, i, SETVAL, 1);
    }

    int msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT); // Créer une file de messages IPC

    // Créer un segment de mémoire partagée pour stocker les données de movies
    int shmid = shmget(IPC_PRIVATE, sizeof(movies), 0666 | IPC_CREAT);
    Movie *shared_movies = (Movie *)shmat(shmid, NULL, 0);
    memcpy(shared_movies, movies, sizeof(movies));

    generate_clients(msgid, shared_movies);
    process_ticket_requests(msgid, shared_movies, semid);

    print_seats_available(shared_movies);

    // Attendre que tous les processus enfants aient terminé
    while (wait(NULL) > 0)
        ;

    msgctl(msgid, IPC_RMID, NULL); // Supprimer la file de messages
    shmctl(shmid, IPC_RMID, NULL); // Supprimer le segment de mémoire partagée

    return 0;
}