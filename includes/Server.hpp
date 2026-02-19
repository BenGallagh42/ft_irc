#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <map>
#include <string>
#include <poll.h>
#include "Client.hpp"
#include "Channel.hpp"

// Serveur IRC gérant plusieurs clients avec poll()
class Server {
private:
    int _server_fd;                                // File descriptor du socket serveur
    int _port;                                     // Port d'écoute du serveur
    std::string _password;                         // Mot de passe de connexion
    std::string _serverName;                       // Nom du serveur pour les réponses IRC
    std::map<int, Client*> _clients;               // Map fd -> Client* (allocation dynamique)
    std::map<std::string, Channel*> _channels;     // Map nom -> Channel* (allocation dynamique)
    std::vector<struct pollfd> _poll_fds;           // Liste des FD pour poll()
    bool _running;                                 // Le serveur tourne-t-il ?

    // Crée le socket serveur, le configure et le met en écoute
    void setupServer();

    // Gestion des connexions
    void acceptNewClient();
    void readFromClient(int poll_index);
    void disconnectClient(int poll_index);

    // Parsing des commandes IRC
    std::string extractCommand(const std::string& message);
    std::string extractParams(const std::string& message);

    // Envoi de réponses IRC
    void sendToClient(Client& client, const std::string& message);
    void sendNumericReply(Client& client, const std::string& code, const std::string& message);
    std::string getClientPrefix(Client& client);

    // Trouve un client par son nickname (retourne NULL si pas trouvé)
    Client* findClientByNickname(const std::string& nickname);

    // Traite une commande IRC reçue d'un client (routeur principal)
    void processCommand(Client& client, const std::string& command);

    // Handlers d'authentification
    void handlePass(Client& client, const std::string& params);
    void handleNick(Client& client, const std::string& params);
    void handleUser(Client& client, const std::string& params);
    void checkRegistration(Client& client);

    // Handlers de channels
    void handleJoin(Client& client, const std::string& params);
    void handlePart(Client& client, const std::string& params);
    void handlePrivmsg(Client& client, const std::string& params);
    void handleKick(Client& client, const std::string& params);
    void handleInvite(Client& client, const std::string& params);
    void handleTopic(Client& client, const std::string& params);
    void handleMode(Client& client, const std::string& params);
    void handleQuit(Client& client, const std::string& params);

    // Retire un client de tous les channels quand il se déconnecte
    void removeClientFromAllChannels(Client* client);

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
