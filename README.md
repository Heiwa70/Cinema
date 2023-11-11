# Projet Cinema

## Description
Une simulation de gestion de cinéma utilisant des files de messages pour optimiser les interactions entre les clients et les salles de cinéma.

## Techniques utilisées
- Files de messages
- Sémaphores

## Structure du Projet
V1 : 
- `client.c` : Gestion du processus client.
- `hotesse.c` : Gestion du processus hôtesse.
- `salle.c` : Gestion du processus salle de cinéma.
- `communication.c` : Fonctions de communication basées sur les files de messages et les sémaphores.

## Installation
1. Compiler le code avec un compilateur C standard.
    ```bash
    gcc client.c -o client
    gcc hotesse.c -o hotesse
    gcc salle.c communication.c -o salle
    ```

2. Exécuter les fichiers éxécutables générés.

## Utilisation
1. Lancer `salle` pour démarrer la simulation des salles de cinéma.
    ```bash
    ./salle
    ```

2. Lancer autant de processus `client` que nécessaire.
    ```bash
    ./client
    ```

3. Lancer le processus `hotesse` pour gérer les transactions et les échanges de billets.
    ```bash
    ./hotesse
    ```

## Gestion des Erreurs
- La salle informe les clients lorsque la capacité maximale est atteinte.
- L'hôtesse détecte et gère les films non autorisés.

## Date limite
Le projet doit être soumis avant le Vendredi 08 décembre, 23h59.

## Licence
Ce projet est sous licence [MIT](LICENSE).

## Contact
Pour toute question, ne contactez pas Fayçal 😭

[Remarque]
Le projet est développé et testé sur Linux il est donc pour l'instant recommandé
