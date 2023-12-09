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
#include <sys/wait.h>

jmp_buf env; // Variable globale pour stocker l'environnement de saut

#define MAX_CLIENTS 60          // Nombre de clients à générer
#define NUM_MOVIES 5            // Nombre de films à projeter
#define MAX_SEATS 50            // Nombre de sièges par film
#define MAX_MOVIE_PREFERENCES 3 // Nombre de films préférés par client
#define RESERVATION_LIMIT 45    // Limite de réservation totale

#define EXCHANGE_PERCENTAGE 5 // Pourcentage de clients qui échangent leur billet

const char *prenom[] = {
    "Jean", "Marie", "Pierre", "Paul", "Jacques", "Francois", "Nicolas", "Michel", "Louis", "Claude",
    "Julien", "Benoit", "Maxime", "Alexandre", "Antoine", "Christophe", "David", "Etienne", "Frederic", "Gregoire"};

const char *nom[] = {
    "Martin", "Bernard", "Thomas", "Petit", "Robert", "Richard", "Durand", "Dubois", "Moreau", "Laurent",
    "Lefevre", "Leroy", "Perrin", "Clement", "Mercier", "Blanc", "Guerin", "Muller", "Faure", "Roux"};

typedef struct
{
    char movie_name[50];
    bool seats_available[MAX_SEATS];
    bool is_authorized;
    int age;
    int num_available_seats;
    int available_seats[MAX_SEATS];
    int total_reserved_tickets;
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

void initialize_movie(Movie *movie)
{
    movie->num_available_seats = MAX_SEATS;
    for (int i = 0; i < MAX_SEATS; i++)
    {
        movie->seats_available[i] = true;
        movie->available_seats[i] = i + 1; // Numéro de siège commence à 1
    }
}

int count_available_seats(const Movie *movie)
{
    return movie->num_available_seats;
}

bool reserve_seats(Movie *movie, int num_seats, int *reserved_seats)
{
    *reserved_seats = 0;
    for (int i = 0; i < MAX_SEATS; i++)
    {
        if (movie->seats_available[i])
        {
            movie->seats_available[i] = false;
            movie->num_available_seats--;
            movie->available_seats[*reserved_seats] = i + 1; // Numéro de siège commence à 1
            (*reserved_seats)++;

            if (*reserved_seats == num_seats)
            {
                return true; // Réservation réussie
            }
        }
    }

    return false; // Réservation échouée (pas assez de sièges disponibles)
}

void adjust_projections(Movie *movies, int num_movies)
{
    // Trouver le film le moins demandé
    int least_demanded_movie = 0;
    for (int i = 1; i < num_movies; i++)
    {
        if (movies[i].total_reserved_tickets < movies[least_demanded_movie].total_reserved_tickets)
        {
            least_demanded_movie = i;
        }
    }

    // Modifier la projection du film le plus demandé
    int most_demanded_movie = 0;
    for (int i = 1; i < num_movies; i++)
    {
        if (movies[i].total_reserved_tickets > movies[most_demanded_movie].total_reserved_tickets)
        {
            most_demanded_movie = i;
        }
    }

    if (strcmp(movies[least_demanded_movie].movie_name, movies[most_demanded_movie].movie_name) != 0)
    {
        // Afficher le message de modification
        printf("\033[1;32mModification de la projection : %s -> %s (Demande élevée)\033[0m\n",
               movies[least_demanded_movie].movie_name, movies[most_demanded_movie].movie_name);
        printf("\033[1;33mTous les clients du film %s restent dans la salle mais regarderont le film %s. \033[4mLe cinéma s'excuse du désagrément.\033[0m\n",
               movies[least_demanded_movie].movie_name, movies[most_demanded_movie].movie_name);

        // changer le nom du film le moins demandé au nom du film le plus demandé
        snprintf(movies[least_demanded_movie].movie_name, sizeof(movies[least_demanded_movie].movie_name), "%s", movies[most_demanded_movie].movie_name);
    }
    else
    {
        printf("\033[1;32m[Modification de la projection]\033[0m Erreur le film a déjà été remplacé\n");
    }
}

void generate_clients(int msgid, Movie *movies, int num_movies, pid_t *child_pids, int semid)
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

                // Générer le nom et le prénom du client
                snprintf(client.nom, sizeof(client.nom), "%s", nom[rand() % 10]);
                snprintf(client.prenom, sizeof(client.prenom), "%s", prenom[rand() % 10]);
                client.age = (rand() % 100) + 1;

                int order_location = rand() % 2;
                const char *location = order_location == 0 ? "à la borne" : "à la caisse";

                printf("\033[1;36m[Create Client]\033[0m Le client %s %s avec le pid : %d se dirige %s\n", client.prenom, client.nom, getpid(), location);

                // Générer les préférences de film du client
                for (int j = 0; j < MAX_MOVIE_PREFERENCES; j++)
                {
                    int random_movie_index = rand() % num_movies;
                    snprintf(client.movie_preferences[j], sizeof(client.movie_preferences[j]), "%s", movies[random_movie_index].movie_name);
                }

                int random_ticket_num = (rand() % 5) + 1;
                client.num_tickets = random_ticket_num;

                msgsnd(msgid, &client, sizeof(client), 0);

                int random_number = rand() % 100;

                // Si le nombre est dans les 20% supérieurs, échanger le ticket du client
                if (random_number >= 100 - EXCHANGE_PERCENTAGE)
                {
                    // Choisir un nouveau film pour le client
                    int new_movie_index = rand() % num_movies;

                    // Échanger le ticket du client pour le nouveau film
                    strcpy(client.movie_preferences[0], movies[new_movie_index].movie_name);
                    printf("\033[1;35mLe client %s %s a échangé son billet pour %s\033[0m\n", client.prenom, client.nom, movies[new_movie_index].movie_name);
                }
            }

            // Chaque processus fils doit également utiliser les sémaphores
            for (int j = 0; j < num_movies; j++)
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

void process_ticket_requests(int msgid, Movie *movies, int num_movies, int semid)
{
    struct message
    {
        long mtype;
        char movie_preferences[MAX_MOVIE_PREFERENCES][50];
        int num_tickets;
        char nom[50];
        char prenom[50];
        int age;
    } message;

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        msgrcv(msgid, &message, sizeof(message), 1, 0);

        int random_ticket_num = message.num_tickets;
        int reserved_tickets = 0;

        for (int j = 0; j < MAX_MOVIE_PREFERENCES; j++)
        {
            int random_movie_index = rand() % num_movies;

            struct sembuf p = {random_movie_index, -1, 0};
            semop(semid, &p, 1);

            if (message.age >= movies[random_movie_index].age)
            {
                int current_reserved_seats;
                if (reserve_seats(&movies[random_movie_index], random_ticket_num, &current_reserved_seats))
                {
                    reserved_tickets += current_reserved_seats;

                    printf("\033[1;33m[Reservation]\033[0m Le client %s a réservé %d billets pour %s aux sièges ", message.nom, reserved_tickets, movies[random_movie_index].movie_name);

                    for (int k = 0; k < current_reserved_seats; k++)
                    {
                        printf("%d ", movies[random_movie_index].available_seats[k]);
                    }

                    printf("\n");

                    movies[random_movie_index].total_reserved_tickets += reserved_tickets;

                    if (movies[random_movie_index].total_reserved_tickets >= RESERVATION_LIMIT)
                    {
                        adjust_projections(movies, num_movies);
                    }

                    break;
                }
                else
                {
                    printf("\033[1;31mLe client %s avec le pid %d n'a pas pu réserver %d billets pour %s car il ne reste que %d places disponibles\033[0m\n",
                           message.nom, getpid(), random_ticket_num, movies[random_movie_index].movie_name, count_available_seats(&movies[random_movie_index]));
                }
            }
            else
            {
                printf("\033[1;31mLe client %s avec le pid %d n'est pas assez âgé pour voir %s\033[0m\n",
                       message.nom, getpid(), movies[random_movie_index].movie_name);
            }

            struct sembuf v = {random_movie_index, 1, 0};
            semop(semid, &v, 1);
        }
    }
}

void print_seats_available(const Movie *movies, int num_movies)
{
    printf("\n-------------Places disponibles-------------\n");
    for (int i = 0; i < num_movies; i++)
    {
        printf("Nombre de places disponibles pour %s = %d\n", movies[i].movie_name, count_available_seats(&movies[i]));
        printf("Numéros de sièges disponibles pour %s: ", movies[i].movie_name);
        for (int j = 0; j < movies[i].num_available_seats; j++)
        {
            printf("%d ", movies[i].available_seats[j]);
        }
        printf("\n");
    }
}

void initialize_semaphores(int semid, int num_sems)
{
    for (int i = 0; i < num_sems; i++)
    {
        semctl(semid, i, SETVAL, 1);
    }
}
void exchange_tickets(Client *clients, Movie *movies, int num_clients, int num_movies)
{
    srand(time(NULL));
    int num_exchanges = (num_clients * EXCHANGE_PERCENTAGE) / 100;

    for (int i = 0; i < num_exchanges; i++)
    {
        int client_index = rand() % num_clients;
        int movie_index = rand() % num_movies;

        // Ensure the client is not already booked for this movie
        bool already_booked = false;
        for (int j = 0; j < MAX_MOVIE_PREFERENCES; j++)
        {
            if (strcmp(clients[client_index].movie_preferences[j], movies[movie_index].movie_name) == 0)
            {
                already_booked = true;
                break;
            }
        }

        if (!already_booked)
        {
            // Exchange the ticket
            strcpy(clients[client_index].movie_preferences[0], movies[movie_index].movie_name);
            printf("Client %s %s a changé son ticket  %s\n", clients[client_index].prenom, clients[client_index].nom, movies[movie_index].movie_name);
        }
    }
}

int main()
{
    srand(time(NULL));

    int semid = semget(IPC_PRIVATE, NUM_MOVIES, 0666 | IPC_CREAT);
    initialize_semaphores(semid, NUM_MOVIES);

    Movie movies[NUM_MOVIES] = {
        {"Le Loup De Wall Street", MAX_SEATS, true, 12},
        {"Harry Potter 1", MAX_SEATS, false, 50},
        {"50 nuances de grey", MAX_SEATS, true, 16},
        {"Transformers", MAX_SEATS, false, 0},
        {"Terminator", MAX_SEATS, false, 0}};

    for (int i = 0; i < NUM_MOVIES; i++)
    {
        initialize_movie(&movies[i]);
    }

    int msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);

    int shmid = shmget(IPC_PRIVATE, sizeof(movies), 0666 | IPC_CREAT);
    Movie *shared_movies = (Movie *)shmat(shmid, NULL, 0);
    memcpy(shared_movies, movies, sizeof(movies));

    pid_t child_pids[MAX_CLIENTS];
    generate_clients(msgid, shared_movies, NUM_MOVIES, child_pids, semid);
    process_ticket_requests(msgid, shared_movies, NUM_MOVIES, semid);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        int status;
        pid_t child_pid = waitpid(child_pids[i], &status, 0);
    }

    print_seats_available(shared_movies, NUM_MOVIES);

    msgctl(msgid, IPC_RMID, NULL);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID); // Supprimer l'ensemble de sémaphores

    return 0;
}
