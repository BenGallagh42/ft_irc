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

    // --- Configuration du serveur ---
    // Crée le socket serveur, le configure et le met en écoute
    void setupServer();

    // --- Gestion des connexions ---
    // Accepte une nouvelle connexion client
    void acceptNewClient();
    // Lit les données envoyées par un client
    void readFromClient(int poll_index);
    // Déconnecte un client et le retire des listes
    void disconnectClient(int poll_index);

    // --- Parsing des commandes IRC ---
    // Extrait la commande IRC (premier mot) d'un message
    std::string extractCommand(const std::string& message);
    // Extrait les paramètres (tout après la commande) d'un message
    std::string extractParams(const std::string& message);

    // --- Envoi de réponses IRC ---
    // Envoie un message brut à un client (ajoute \r\n si absent)
    void sendToClient(Client& client, const std::string& message);
    // Envoie une réponse numérique IRC (ex: :server 001 nick :Welcome)
    void sendNumericReply(Client& client, const std::string& code, const std::string& message);
    // Construit le préfixe d'un client (ex: :nick!user@host)
    std::string getClientPrefix(Client& client);

    // --- Recherche ---
    // Trouve un client par son nickname (retourne NULL si pas trouvé)
    Client* findClientByNickname(const std::string& nickname);

    // --- Traitement des commandes ---
    // Traite une commande IRC reçue d'un client (routeur principal)
    void processCommand(Client& client, const std::string& command);

    // --- Handlers d'authentification ---
    // Gère la commande PASS (vérification du mot de passe)
    void handlePass(Client& client, const std::string& params);
    // Gère la commande NICK (définir/changer le pseudo)
    void handleNick(Client& client, const std::string& params);
    // Gère la commande USER (définir le nom d'utilisateur)
    void handleUser(Client& client, const std::string& params);
    // Vérifie si l'enregistrement est complet et envoie RPL_WELCOME
    void checkRegistration(Client& client);

    // --- Handlers de channels ---
    // Gère la commande JOIN (rejoindre un salon)
    void handleJoin(Client& client, const std::string& params);
    // Gère la commande PART (quitter un salon)
    void handlePart(Client& client, const std::string& params);
    // Gère la commande PRIVMSG (message privé ou channel)
    void handlePrivmsg(Client& client, const std::string& params);
    // Gère la commande KICK (expulser un utilisateur)
    void handleKick(Client& client, const std::string& params);
    // Gère la commande INVITE (inviter un utilisateur)
    void handleInvite(Client& client, const std::string& params);
    // Gère la commande TOPIC (voir/changer le sujet)
    void handleTopic(Client& client, const std::string& params);
    // Gère la commande MODE (changer les modes du channel)
    void handleMode(Client& client, const std::string& params);
    // Gère la commande QUIT (déconnexion volontaire)
    void handleQuit(Client& client, const std::string& params);

    // --- Nettoyage ---
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
