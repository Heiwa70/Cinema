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
#include <time.h>
#include <setjmp.h>

jmp_buf env; // Variable globale pour stocker l'environnement de saut

#define MAX_CLIENTS 30
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

typedef struct
{
    long msg_type;
    char current_movie[50];
    char new_movie[50];
    int num_tickets;
    char nom[50];
    char prenom[50];
    int age;
} TicketExchangeRequest;

Movie movies[NUM_MOVIES] = {
    {"Le Loup De Walstreet", MAX_SEATS, true, 12},
    {"Harry Potter 1", MAX_SEATS, false, 50},
    {"50 nuances de grey", MAX_SEATS, true, 16},
    {"Transformers", MAX_SEATS, false, 0},
    {"Terminator", MAX_SEATS, false, 0}};

void exchange_ticket(TicketExchangeRequest *exchange_request, Movie *shared_movies, int semid)
{
    // Logique de traitement de la demande d'échange et mise à jour des sièges disponibles
    // ...

    printf("Le client %s a échangé ses billets de %s pour %s avec succès!\n", exchange_request->nom, exchange_request->current_movie, exchange_request->new_movie);
}

void generate_clients(int msgid, Movie *shared_movies, pid_t *child_pids, int semid, int exchange_msgid)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            if (setjmp(env) == 0)
            {
                srand(getpid());

                Client client;
                client.msg_type = 1;

                strcpy(client.nom, nom[rand() % 10]);
                strcpy(client.prenom, prenom[rand() % 10]);
                client.age = (rand() % 100) + 1;

                printf("Le client %s %s avec le pid : %d\n", client.prenom, client.nom, getpid());

                for (int j = 0; j < MAX_MOVIE_PREFERENCES; j++)
                {
                    int random_movie_index = rand() % NUM_MOVIES;
                    strcpy(client.movie_preferences[j], shared_movies[random_movie_index].movie_name);
                }

                int random_ticket_num = (rand() % 5) + 1;
                client.num_tickets = random_ticket_num;

                msgsnd(msgid, &client, sizeof(client), 0);

                // 10% de chance de demander un échange
                int random_exchange_chance = rand() % 10;
                if (random_exchange_chance == 0)
                {
                    TicketExchangeRequest exchange_request;
                    exchange_request.msg_type = 2;
                    strcpy(exchange_request.nom, client.nom);
                    strcpy(exchange_request.prenom, client.prenom);
                    exchange_request.age = client.age;

                    int current_movie_index = rand() % NUM_MOVIES;
                    strcpy(exchange_request.current_movie, shared_movies[current_movie_index].movie_name);

                    int new_movie_index;
                    do
                    {
                        new_movie_index = rand() % NUM_MOVIES;
                    } while (new_movie_index == current_movie_index); // Assurez-vous que le nouveau film est différent du film actuel

                    strcpy(exchange_request.new_movie, shared_movies[new_movie_index].movie_name);

                    exchange_request.num_tickets = random_ticket_num;

                    msgsnd(exchange_msgid, &exchange_request, sizeof(exchange_request), 0);
                }
            }

            // Chaque processus fils doit également utiliser les sémaphores
            for (int j = 0; j < NUM_MOVIES; j++)
            {
                struct sembuf v = {j, 1, 0};
                semop(semid, &v, 1);
            }

            exit(0);
        }
        else
        {
            child_pids[i] = pid;
        }
    }
}

void process_ticket_requests(int msgid, Movie *shared_movies, int semid, int exchange_msgid)
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

    // Liste pour stocker les ID des clients ayant fait une demande d'échange
    int processed_exchange_requests[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        processed_exchange_requests[i] = 0;
    }

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
                    printf("\033[1;31mLe client %s n'a pas pu réserver %d billets pour %s car il ne reste que %d places disponibles\033[0m\n", message.nom, random_ticket_num, shared_movies[random_movie_index].movie_name, shared_movies[random_movie_index].seats_available);
                }
            }
            else
            {
                printf("\033[1;31mLe client %s n'est pas assez âgé pour voir %s\033[0m\n", message.nom, shared_movies[random_movie_index].movie_name);
            }

            // Après avoir réservé les billets :
            struct sembuf v = {random_movie_index, 1, 0};
            semop(semid, &v, 1);
        }

        // Traitement des demandes d'échange
        TicketExchangeRequest exchange_request;
        if (msgrcv(exchange_msgid, &exchange_request, sizeof(exchange_request), 2, IPC_NOWAIT) > 0)
        {
            // Vérifier si la demande d'échange a déjà été traitée
            if (!processed_exchange_requests[i])
            {
                // Logique de traitement de la demande d'échange et mise à jour des sièges disponibles
                exchange_ticket(&exchange_request, shared_movies, semid);

                printf("Demande d'échange reçue de %s pour passer de %s à %s\n", exchange_request.nom, exchange_request.current_movie, exchange_request.new_movie);

                // Marquer la demande d'échange comme traitée
                processed_exchange_requests[i] = 1;
            }
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
    srand(time(NULL));

    int semid = semget(IPC_PRIVATE, NUM_MOVIES, 0666 | IPC_CREAT);

    for (int i = 0; i < NUM_MOVIES; i++)
    {
        semctl(semid, i, SETVAL, 1); // Initialiser chaque sémaphore à 1
    }

    int exchange_msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);

    int msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);

    int shmid = shmget(IPC_PRIVATE, sizeof(movies), 0666 | IPC_CREAT);
    Movie *shared_movies = (Movie *)shmat(shmid, NULL, 0);
    memcpy(shared_movies, movies, sizeof(movies));

    pid_t child_pids[MAX_CLIENTS];
    generate_clients(msgid, shared_movies, child_pids, semid, exchange_msgid);
    process_ticket_requests(msgid, shared_movies, semid, exchange_msgid);

    print_seats_available(shared_movies);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        int status;
        waitpid(child_pids[i], &status, 0);
    }

    msgctl(msgid, IPC_RMID, NULL);
    msgctl(exchange_msgid, IPC_RMID, NULL);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID); // Supprimer l'ensemble de sémaphores

    return 0;
}
