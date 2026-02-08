#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <string>
#include <poll.h>
#include "Client.hpp"

// Serveur IRC gérant plusieurs clients avec poll()
class Server {
private:
    int _server_fd;                        // File descriptor du socket serveur
    int _port;                             // Port d'écoute du serveur
    std::string _password;                 // Mot de passe de connexion
    std::vector<Client> _clients;          // Liste des clients connectés
    std::vector<struct pollfd> _poll_fds;  // Liste des FD pour poll()
    bool _running;                         // Le serveur tourne-t-il ?

    // Crée le socket serveur, le configure et le met en écoute
    void setupServer();
    
    // Accepte une nouvelle connexion client
    void acceptNewClient();
    
    // Lit les données envoyées par un client
    void readFromClient(int client_index);
    
    // Traite une commande IRC reçue d'un client
    void processCommand(Client& client, const std::string& command);
    
    // Déconnecte un client et le retire des listes
    void disconnectClient(int client_index);

public:
    // Constructeur : initialise le serveur avec un port et un mot de passe
    Server(int port, const std::string& password);
    
    // Destructeur : ferme proprement tous les sockets
    ~Server();
    
    // Lance la boucle principale du serveur (poll + traitement)
    void run();
    
    // Arrête le serveur proprement
    void stop();
};

#endif