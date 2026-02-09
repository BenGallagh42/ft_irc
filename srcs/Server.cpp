#include "Server.hpp"
#include "utils.hpp"
#include <sys/socket.h>  // Pour socket(), bind(), listen(), accept(), send()
#include <netinet/in.h>  // Pour struct sockaddr_in
#include <arpa/inet.h>   // Pour inet_ntop()
#include <unistd.h>      // Pour close()
#include <fcntl.h>       // Pour fcntl()
#include <cstring>       // Pour memset()
#include <cerrno>        // Pour errno
#include <iostream>      // Pour std::cout, std::cerr
#include <sstream>       // Pour std::ostringstream (conversion int -> string en C++98)
#include <algorithm>     // Pour std::find()
#include <cctype>        // Pour std::toupper(), std::isalpha(), std::isdigit()

// ============================================================================
//                            CONSTRUCTEUR / DESTRUCTEUR
// ============================================================================

// Constructeur : initialise le serveur avec un port et un mot de passe
Server::Server(int port, const std::string& password)
    : _server_fd(-1), _port(port), _password(password), _serverName("ft_irc"), _running(false)
{
    std::cout << "=== IRC Server Initializing ===" << std::endl;
    std::cout << "Port: " << port << std::endl;

    // Configurer et démarrer le socket serveur
    setupServer();
}

// Destructeur : ferme tous les sockets proprement
Server::~Server()
{
    stop();
}

// ============================================================================
//                           CONFIGURATION DU SERVEUR
// ============================================================================

// Crée et configure le socket serveur
void Server::setupServer()
{
    // 1. Créer le socket (endpoint de communication)
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0)
        error_exit("Failed to create socket");

    // 2. Permettre la réutilisation de l'adresse
    // Évite l'erreur "Address already in use" si on relance rapidement
    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        close(_server_fd);
        error_exit("setsockopt SO_REUSEADDR failed");
    }

    // 3. Mettre le socket en mode non-bloquant
    if (!set_nonblocking(_server_fd))
    {
        close(_server_fd);
        error_exit("Failed to set server socket to non-blocking");
    }

    // 4. Configurer l'adresse du serveur
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;           // IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;   // Toutes les interfaces (0.0.0.0)
    server_addr.sin_port = htons(_port);        // Port (conversion en network byte order)

    // 5. Associer le socket à l'adresse (bind)
    if (bind(_server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        close(_server_fd);
        error_exit("Bind failed - port may already be in use");
    }

    // 6. Mettre le socket en mode écoute
    // 10 = taille de la file d'attente (max 10 connexions en attente)
    if (listen(_server_fd, 10) < 0)
    {
        close(_server_fd);
        error_exit("Listen failed");
    }

    // 7. Ajouter le socket serveur à la liste poll()
    struct pollfd server_pollfd;
    server_pollfd.fd = _server_fd;
    server_pollfd.events = POLLIN;  // Surveiller les nouvelles connexions
    server_pollfd.revents = 0;
    _poll_fds.push_back(server_pollfd);

    std::cout << "Server socket created and listening" << std::endl;
    std::cout << "==================================" << std::endl;
}

// ============================================================================
//                          GESTION DES CONNEXIONS
// ============================================================================

// Accepte une nouvelle connexion client
void Server::acceptNewClient()
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Accepter la connexion
    int client_fd = accept(_server_fd, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd < 0)
    {
        // En mode non-bloquant, EAGAIN = pas de connexion disponible (normal)
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            std::cerr << "Accept error: " << strerror(errno) << std::endl;
        return;
    }

    // Mettre le socket client en mode non-bloquant
    if (!set_nonblocking(client_fd))
    {
        std::cerr << "Failed to set client socket to non-blocking" << std::endl;
        close(client_fd);
        return;
    }

    // Créer l'objet Client sur le tas (allocation dynamique)
    Client* new_client = new Client(client_fd);

    // Stocker le client dans la map (fd -> Client*)
    _clients[client_fd] = new_client;

    // Ajouter le client à poll()
    struct pollfd client_pollfd;
    client_pollfd.fd = client_fd;
    client_pollfd.events = POLLIN;  // Surveiller les données entrantes
    client_pollfd.revents = 0;
    _poll_fds.push_back(client_pollfd);

    // Afficher des infos sur le nouveau client
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    std::cout << "\n[NEW CONNECTION]" << std::endl;
    std::cout << "  FD: " << client_fd << std::endl;
    std::cout << "  IP: " << client_ip << std::endl;
    std::cout << "  Total clients: " << _clients.size() << std::endl;
}

// Lit les données envoyées par un client
void Server::readFromClient(int poll_index)
{
    char buffer[512];
    memset(buffer, 0, sizeof(buffer));

    // Récupérer le fd depuis poll_fds et le client depuis la map
    int client_fd = _poll_fds[poll_index].fd;
    Client* client = _clients[client_fd];

    // Recevoir les données
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (bytes_read < 0)
    {
        // En mode non-bloquant, EAGAIN = pas de données (normal)
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            std::cerr << "Recv error from FD " << client_fd << ": " << strerror(errno) << std::endl;
            disconnectClient(poll_index);
        }
        return;
    }

    if (bytes_read == 0)
    {
        // Le client a fermé la connexion
        std::cout << "\n[DISCONNECTION]" << std::endl;
        std::cout << "  FD: " << client_fd << std::endl;
        disconnectClient(poll_index);
        return;
    }

    // Ajouter les données au buffer du client
    buffer[bytes_read] = '\0';
    client->appendToBuffer(std::string(buffer));

    std::cout << "\n[RECEIVED] FD " << client_fd << ": " << buffer;

    // Extraire et traiter les commandes complètes
    std::string client_buffer = client->getBuffer();
    size_t pos;

    // Chercher les commandes terminées par \r\n ou \n
    while ((pos = client_buffer.find('\n')) != std::string::npos)
    {
        // Extraire la commande (tout avant le \n)
        std::string command = client_buffer.substr(0, pos);

        // Supprimer \r si présent à la fin (format IRC : \r\n)
        if (!command.empty() && command[command.length() - 1] == '\r')
            command = command.substr(0, command.length() - 1);

        // Avancer dans le buffer (après le \n)
        client_buffer = client_buffer.substr(pos + 1);

        // Traiter la commande si elle n'est pas vide
        if (!command.empty())
            processCommand(*client, command);
    }

    // Remettre à jour le buffer (données incomplètes restantes)
    client->clearBuffer();
    client->appendToBuffer(client_buffer);
}

// Déconnecte un client et le retire des listes
void Server::disconnectClient(int poll_index)
{
    // Récupérer le fd et le client
    int client_fd = _poll_fds[poll_index].fd;
    Client* client = _clients[client_fd];

    // Retirer le client de tous les channels
    removeClientFromAllChannels(client);

    // Fermer le socket
    close(client_fd);

    // Libérer la mémoire et retirer de la map
    delete client;
    _clients.erase(client_fd);

    // Retirer de poll_fds
    _poll_fds.erase(_poll_fds.begin() + poll_index);

    std::cout << "  Client removed" << std::endl;
    std::cout << "  Remaining clients: " << _clients.size() << std::endl;
}

// ============================================================================
//                           HELPERS / UTILITAIRES
// ============================================================================

// Extrait le premier mot (la commande IRC) du message et le met en majuscules
std::string Server::extractCommand(const std::string& message)
{
    // Trouver le premier espace pour séparer la commande des paramètres
    size_t space = message.find(' ');
    std::string cmd = (space != std::string::npos) ? message.substr(0, space) : message;

    // Convertir en majuscules pour comparaison insensible à la casse
    for (size_t i = 0; i < cmd.size(); ++i)
        cmd[i] = std::toupper(cmd[i]);

    return cmd;
}

// Extrait tout après la commande (les paramètres)
std::string Server::extractParams(const std::string& message)
{
    // Trouver le premier espace
    size_t space = message.find(' ');

    // Si pas d'espace, il n'y a pas de paramètres
    if (space == std::string::npos)
        return "";

    // Retourner tout après l'espace
    return message.substr(space + 1);
}

// Envoie un message brut à un client via son socket
void Server::sendToClient(Client& client, const std::string& message)
{
    // Ajouter \r\n si pas déjà présent (format IRC obligatoire)
    std::string msg = message;
    if (msg.length() < 2 || msg.substr(msg.length() - 2) != "\r\n")
        msg += "\r\n";

    // Envoyer le message via le socket du client
    send(client.getFd(), msg.c_str(), msg.length(), 0);
}

// Envoie une réponse numérique IRC au format :servername CODE nick :message
void Server::sendNumericReply(Client& client, const std::string& code, const std::string& message)
{
    // Utiliser * comme nick si le client n'a pas encore de pseudo
    std::string nick = client.getNickname().empty() ? "*" : client.getNickname();

    // Construire la réponse au format IRC
    std::string reply = ":" + _serverName + " " + code + " " + nick + " " + message;

    // Envoyer la réponse
    sendToClient(client, reply);
}

// Construit le préfixe IRC d'un client (:nick!user@host)
std::string Server::getClientPrefix(Client& client)
{
    return ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
}

// Cherche un client par son pseudo dans la map des clients
Client* Server::findClientByNickname(const std::string& nickname)
{
    // Parcourir tous les clients de la map
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        // Comparer le pseudo du client avec celui recherché
        if (it->second->getNickname() == nickname)
            return it->second;
    }

    // Aucun client trouvé avec ce pseudo
    return NULL;
}

// Retire un client de tous les channels (appelé lors de la déconnexion)
void Server::removeClientFromAllChannels(Client* client)
{
    // Parcourir tous les channels
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); )
    {
        // Construire le message QUIT pour notifier les membres
        std::string quitMsg = getClientPrefix(*client) + " QUIT :Connection closed\r\n";

        // Si le client est membre de ce channel, le notifier aux autres
        if (it->second->isMember(client))
            it->second->broadcastMessage(quitMsg, client);

        // Retirer le client du channel
        it->second->removeMember(client);

        // Si le channel est vide après le retrait, le supprimer
        if (it->second->getMembers().empty())
        {
            delete it->second;
            _channels.erase(it++);  // Post-incrémentation pour éviter l'invalidation de l'itérateur
        }
        else
            ++it;
    }
}

// ============================================================================
//                         ROUTEUR DE COMMANDES
// ============================================================================

// Traite une commande IRC reçue d'un client (routeur principal)
void Server::processCommand(Client& client, const std::string& command)
{
    std::cout << "[COMMAND] FD " << client.getFd() << ": " << command << std::endl;

    // Extraire la commande et les paramètres
    std::string cmd = extractCommand(command);
    std::string params = extractParams(command);

    // Les commandes PASS, NICK, USER sont toujours autorisées (avant enregistrement)
    if (cmd == "PASS")
        return handlePass(client, params);
    if (cmd == "NICK")
        return handleNick(client, params);
    if (cmd == "USER")
        return handleUser(client, params);
    if (cmd == "QUIT")
        return handleQuit(client, params);

    // Toutes les autres commandes nécessitent un enregistrement complet
    if (!client.isRegistered())
    {
        sendNumericReply(client, "451", ":You have not registered");
        return;
    }

    // Router les commandes qui nécessitent un enregistrement
    if (cmd == "JOIN")
        handleJoin(client, params);
    else if (cmd == "PART")
        handlePart(client, params);
    else if (cmd == "PRIVMSG")
        handlePrivmsg(client, params);
    else if (cmd == "KICK")
        handleKick(client, params);
    else if (cmd == "INVITE")
        handleInvite(client, params);
    else if (cmd == "TOPIC")
        handleTopic(client, params);
    else if (cmd == "MODE")
        handleMode(client, params);
    else
        sendNumericReply(client, "421", cmd + " :Unknown command");
}

// ============================================================================
//                      HANDLERS D'AUTHENTIFICATION
// ============================================================================

// Gère la commande PASS : vérifie le mot de passe du serveur
// Format : PASS <password>
void Server::handlePass(Client& client, const std::string& params)
{
    // Vérifier si le client est déjà enregistré
    if (client.isRegistered())
    {
        sendNumericReply(client, "462", ":You may not reregister");
        return;
    }

    // Vérifier que le mot de passe est fourni
    if (params.empty())
    {
        sendNumericReply(client, "461", "PASS :Not enough parameters");
        return;
    }

    // Vérifier si le mot de passe correspond
    if (params == _password)
    {
        // Mot de passe correct : marquer le client comme authentifié
        client.setAuthenticated(true);
        std::cout << "[AUTH] FD " << client.getFd() << ": Password accepted" << std::endl;
    }
    else
    {
        // Mot de passe incorrect
        sendNumericReply(client, "464", ":Password incorrect");
    }
}

// Gère la commande NICK : définir ou changer le pseudo
// Format : NICK <nickname>
void Server::handleNick(Client& client, const std::string& params)
{
    // Vérifier que le pseudo est fourni
    if (params.empty())
    {
        sendNumericReply(client, "431", ":No nickname given");
        return;
    }

    // Vérifier que le client a fourni le mot de passe (PASS doit précéder NICK)
    if (!client.isAuthenticated())
    {
        sendNumericReply(client, "464", ":Password incorrect - use PASS first");
        return;
    }

    // Extraire juste le premier mot comme nickname (ignorer le reste)
    std::string nickname = params;
    size_t space = nickname.find(' ');
    if (space != std::string::npos)
        nickname = nickname.substr(0, space);

    // Valider le format du nickname
    // Premier caractère : doit être une lettre ou un caractère spécial IRC
    if (!std::isalpha(nickname[0]) && nickname[0] != '[' && nickname[0] != ']'
        && nickname[0] != '\\' && nickname[0] != '^' && nickname[0] != '_'
        && nickname[0] != '{' && nickname[0] != '}' && nickname[0] != '|')
    {
        sendNumericReply(client, "432", nickname + " :Erroneous nickname");
        return;
    }

    // Caractères suivants : lettres, chiffres, ou caractères spéciaux IRC
    for (size_t i = 1; i < nickname.size(); ++i)
    {
        char c = nickname[i];
        if (!std::isalnum(c) && c != '[' && c != ']' && c != '\\'
            && c != '^' && c != '_' && c != '{' && c != '}' && c != '|' && c != '-')
        {
            sendNumericReply(client, "432", nickname + " :Erroneous nickname");
            return;
        }
    }

    // Vérifier que le pseudo n'est pas déjà utilisé par un autre client
    Client* existing = findClientByNickname(nickname);
    if (existing && existing != &client)
    {
        sendNumericReply(client, "433", nickname + " :Nickname is already in use");
        return;
    }

    // Sauvegarder l'ancien pseudo pour la notification de changement
    std::string oldNick = client.getNickname();

    // Appliquer le nouveau pseudo
    client.setNickname(nickname);
    std::cout << "[NICK] FD " << client.getFd() << ": " << nickname << std::endl;

    // Si le client était déjà enregistré (changement de pseudo)
    if (client.isRegistered() && !oldNick.empty())
    {
        // Notifier tous les channels où le client est présent
        std::string nickMsg = ":" + oldNick + "!" + client.getUsername() + "@localhost NICK :" + nickname + "\r\n";
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            if (it->second->isMember(&client))
                it->second->broadcastMessageAll(nickMsg);
        }
    }

    // Vérifier si l'enregistrement est maintenant complet
    checkRegistration(client);
}

// Gère la commande USER : définir le nom d'utilisateur
// Format : USER <username> <mode> <unused> :<realname>
void Server::handleUser(Client& client, const std::string& params)
{
    // Vérifier si le client est déjà enregistré
    if (client.isRegistered())
    {
        sendNumericReply(client, "462", ":You may not reregister");
        return;
    }

    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "USER :Not enough parameters");
        return;
    }

    // Vérifier que le client a fourni le mot de passe
    if (!client.isAuthenticated())
    {
        sendNumericReply(client, "464", ":Password incorrect - use PASS first");
        return;
    }

    // Extraire le username (premier mot des paramètres)
    std::string username = params;
    size_t space = username.find(' ');
    if (space != std::string::npos)
        username = username.substr(0, space);

    // Appliquer le username
    client.setUsername(username);
    std::cout << "[USER] FD " << client.getFd() << ": " << username << std::endl;

    // Vérifier si l'enregistrement est maintenant complet
    checkRegistration(client);
}

// Vérifie si PASS + NICK + USER sont complétés et envoie RPL_WELCOME
void Server::checkRegistration(Client& client)
{
    // Si déjà enregistré, ne rien faire
    if (client.isRegistered())
        return;

    // L'enregistrement nécessite : authentification + nickname + username
    if (client.isAuthenticated() && !client.getNickname().empty() && !client.getUsername().empty())
    {
        // Marquer le client comme enregistré
        client.setRegistered(true);

        // Envoyer le message de bienvenue (RPL_WELCOME 001)
        std::string welcome = ":Welcome to the " + _serverName + " Network, "
            + client.getNickname() + "!" + client.getUsername() + "@localhost";
        sendNumericReply(client, "001", welcome);

        std::cout << "[REGISTERED] " << client.getNickname() << " is now registered" << std::endl;
    }
}

// ============================================================================
//                         HANDLERS DE CHANNELS
// ============================================================================

// Gère la commande JOIN : rejoindre un salon
// Format : JOIN #channel [key]
void Server::handleJoin(Client& client, const std::string& params)
{
    // Vérifier que le nom du channel est fourni
    if (params.empty())
    {
        sendNumericReply(client, "461", "JOIN :Not enough parameters");
        return;
    }

    // Extraire le nom du channel et la clé optionnelle
    std::string channelName = params;
    std::string key = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        channelName = params.substr(0, space);
        key = params.substr(space + 1);
    }

    // Vérifier que le nom du channel commence par #
    if (channelName.empty() || channelName[0] != '#')
    {
        sendNumericReply(client, "476", channelName + " :Bad Channel Mask");
        return;
    }

    // Chercher si le channel existe déjà
    Channel* channel = NULL;
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);

    if (it != _channels.end())
    {
        // Le channel existe déjà
        channel = it->second;

        // Vérifier si le client est déjà membre
        if (channel->isMember(&client))
            return;  // Déjà membre, ne rien faire

        // Vérifier le mode invitation seulement (+i)
        if (channel->isInviteOnly() && !channel->isInvited(&client))
        {
            sendNumericReply(client, "473", channelName + " :Cannot join channel (+i)");
            return;
        }

        // Vérifier le mot de passe du channel (+k)
        if (!channel->getKey().empty() && key != channel->getKey())
        {
            sendNumericReply(client, "475", channelName + " :Cannot join channel (+k)");
            return;
        }

        // Vérifier la limite de membres (+l)
        if (channel->getUserLimit() > 0
            && (int)channel->getMembers().size() >= channel->getUserLimit())
        {
            sendNumericReply(client, "471", channelName + " :Cannot join channel (+l)");
            return;
        }
    }
    else
    {
        // Le channel n'existe pas : le créer
        channel = new Channel(channelName);
        _channels[channelName] = channel;

        // Le créateur du channel est automatiquement opérateur
        channel->addOperator(&client);

        std::cout << "[CHANNEL] Created: " << channelName << std::endl;
    }

    // Ajouter le client au channel
    channel->addMember(&client);

    // Retirer le client de la liste des invités (il a rejoint)
    channel->removeInvited(&client);

    // Envoyer la notification JOIN à tous les membres du channel
    std::string joinMsg = getClientPrefix(client) + " JOIN " + channelName + "\r\n";
    channel->broadcastMessageAll(joinMsg);

    // Si le channel a un topic, l'envoyer au nouveau membre
    if (!channel->getTopic().empty())
        sendNumericReply(client, "332", channelName + " :" + channel->getTopic());

    // Construire la liste des membres pour RPL_NAMREPLY (353)
    std::string namesList = "";
    std::vector<Client*>& members = channel->getMembers();
    for (size_t i = 0; i < members.size(); ++i)
    {
        // Ajouter un espace entre les noms
        if (i > 0)
            namesList += " ";

        // Préfixer @ pour les opérateurs
        if (channel->isOperator(members[i]))
            namesList += "@";

        // Ajouter le pseudo du membre
        namesList += members[i]->getNickname();
    }

    // Envoyer RPL_NAMREPLY (353) et RPL_ENDOFNAMES (366)
    sendNumericReply(client, "353", "= " + channelName + " :" + namesList);
    sendNumericReply(client, "366", channelName + " :End of /NAMES list");

    std::cout << "[JOIN] " << client.getNickname() << " joined " << channelName << std::endl;
}

// Gère la commande PART : quitter un salon
// Format : PART #channel [:message]
void Server::handlePart(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "PART :Not enough parameters");
        return;
    }

    // Extraire le nom du channel et le message optionnel
    std::string channelName = params;
    std::string message = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        channelName = params.substr(0, space);
        message = params.substr(space + 1);
        // Retirer le : au début du message si présent
        if (!message.empty() && message[0] == ':')
            message = message.substr(1);
    }

    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
    if (it == _channels.end())
    {
        sendNumericReply(client, "403", channelName + " :No such channel");
        return;
    }

    Channel* channel = it->second;

    // Vérifier que le client est membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", channelName + " :You're not on that channel");
        return;
    }

    // Construire le message PART
    std::string partMsg = getClientPrefix(client) + " PART " + channelName;
    if (!message.empty())
        partMsg += " :" + message;
    partMsg += "\r\n";

    // Envoyer le message PART à tous les membres (y compris le client qui part)
    channel->broadcastMessageAll(partMsg);

    // Retirer le client du channel
    channel->removeMember(&client);

    // Si le channel est vide, le supprimer
    if (channel->getMembers().empty())
    {
        delete channel;
        _channels.erase(it);
        std::cout << "[CHANNEL] Deleted: " << channelName << std::endl;
    }

    std::cout << "[PART] " << client.getNickname() << " left " << channelName << std::endl;
}

// Gère la commande PRIVMSG : envoyer un message à un channel ou un utilisateur
// Format : PRIVMSG <target> :<message>
void Server::handlePrivmsg(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "411", ":No recipient given (PRIVMSG)");
        return;
    }

    // Extraire la cible et le message
    std::string target = params;
    std::string message = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        target = params.substr(0, space);
        message = params.substr(space + 1);
        // Retirer le : au début du message si présent
        if (!message.empty() && message[0] == ':')
            message = message.substr(1);
    }

    // Vérifier que le message n'est pas vide
    if (message.empty())
    {
        sendNumericReply(client, "412", ":No text to send");
        return;
    }

    // Construire le message au format IRC
    std::string fullMsg = getClientPrefix(client) + " PRIVMSG " + target + " :" + message + "\r\n";

    // Vérifier si la cible est un channel (commence par #)
    if (target[0] == '#')
    {
        // Message vers un channel
        std::map<std::string, Channel*>::iterator it = _channels.find(target);
        if (it == _channels.end())
        {
            sendNumericReply(client, "403", target + " :No such channel");
            return;
        }

        Channel* channel = it->second;

        // Vérifier que le client est membre du channel
        if (!channel->isMember(&client))
        {
            sendNumericReply(client, "442", target + " :You're not on that channel");
            return;
        }

        // Envoyer le message à tous les membres sauf l'expéditeur
        channel->broadcastMessage(fullMsg, &client);
    }
    else
    {
        // Message privé vers un utilisateur
        Client* targetClient = findClientByNickname(target);
        if (!targetClient)
        {
            sendNumericReply(client, "401", target + " :No such nick/channel");
            return;
        }

        // Envoyer le message directement au client cible
        sendToClient(*targetClient, fullMsg);
    }
}

// Gère la commande KICK : expulser un utilisateur d'un channel
// Format : KICK #channel <user> [:<reason>]
void Server::handleKick(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "KICK :Not enough parameters");
        return;
    }

    // Extraire le nom du channel
    std::string remaining = params;
    std::string channelName = "";
    size_t space = remaining.find(' ');
    if (space != std::string::npos)
    {
        channelName = remaining.substr(0, space);
        remaining = remaining.substr(space + 1);
    }
    else
    {
        sendNumericReply(client, "461", "KICK :Not enough parameters");
        return;
    }

    // Extraire le pseudo de la cible et la raison optionnelle
    std::string targetNick = remaining;
    std::string reason = client.getNickname();  // Par défaut, le kicker est la raison
    space = remaining.find(' ');
    if (space != std::string::npos)
    {
        targetNick = remaining.substr(0, space);
        reason = remaining.substr(space + 1);
        // Retirer le : au début de la raison si présent
        if (!reason.empty() && reason[0] == ':')
            reason = reason.substr(1);
    }

    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
    if (it == _channels.end())
    {
        sendNumericReply(client, "403", channelName + " :No such channel");
        return;
    }

    Channel* channel = it->second;

    // Vérifier que le kicker est membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", channelName + " :You're not on that channel");
        return;
    }

    // Vérifier que le kicker est opérateur
    if (!channel->isOperator(&client))
    {
        sendNumericReply(client, "482", channelName + " :You're not channel operator");
        return;
    }

    // Trouver le client cible par son pseudo
    Client* target = findClientByNickname(targetNick);
    if (!target)
    {
        sendNumericReply(client, "401", targetNick + " :No such nick/channel");
        return;
    }

    // Vérifier que la cible est membre du channel
    if (!channel->isMember(target))
    {
        sendNumericReply(client, "441", targetNick + " " + channelName + " :They aren't on that channel");
        return;
    }

    // Construire et envoyer le message KICK à tous les membres
    std::string kickMsg = getClientPrefix(client) + " KICK " + channelName + " " + targetNick + " :" + reason + "\r\n";
    channel->broadcastMessageAll(kickMsg);

    // Retirer la cible du channel
    channel->removeMember(target);

    // Si le channel est vide, le supprimer
    if (channel->getMembers().empty())
    {
        delete channel;
        _channels.erase(it);
    }

    std::cout << "[KICK] " << client.getNickname() << " kicked " << targetNick << " from " << channelName << std::endl;
}

// Gère la commande INVITE : inviter un utilisateur dans un channel
// Format : INVITE <nickname> #channel
void Server::handleInvite(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "INVITE :Not enough parameters");
        return;
    }

    // Extraire le pseudo de la cible et le nom du channel
    std::string targetNick = params;
    std::string channelName = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        targetNick = params.substr(0, space);
        channelName = params.substr(space + 1);
        // Retirer les espaces en trop
        size_t space2 = channelName.find(' ');
        if (space2 != std::string::npos)
            channelName = channelName.substr(0, space2);
    }
    else
    {
        sendNumericReply(client, "461", "INVITE :Not enough parameters");
        return;
    }

    // Trouver le client cible par son pseudo
    Client* target = findClientByNickname(targetNick);
    if (!target)
    {
        sendNumericReply(client, "401", targetNick + " :No such nick/channel");
        return;
    }

    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
    if (it == _channels.end())
    {
        sendNumericReply(client, "403", channelName + " :No such channel");
        return;
    }

    Channel* channel = it->second;

    // Vérifier que l'inviteur est membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", channelName + " :You're not on that channel");
        return;
    }

    // Si le channel est en mode +i, seuls les opérateurs peuvent inviter
    if (channel->isInviteOnly() && !channel->isOperator(&client))
    {
        sendNumericReply(client, "482", channelName + " :You're not channel operator");
        return;
    }

    // Vérifier que la cible n'est pas déjà dans le channel
    if (channel->isMember(target))
    {
        sendNumericReply(client, "443", targetNick + " " + channelName + " :is already on channel");
        return;
    }

    // Ajouter la cible à la liste des invités du channel
    channel->addInvited(target);

    // Envoyer la confirmation à l'inviteur (RPL_INVITING 341)
    sendNumericReply(client, "341", targetNick + " " + channelName);

    // Envoyer la notification d'invitation à la cible
    std::string inviteMsg = getClientPrefix(client) + " INVITE " + targetNick + " " + channelName + "\r\n";
    sendToClient(*target, inviteMsg);

    std::cout << "[INVITE] " << client.getNickname() << " invited " << targetNick << " to " << channelName << std::endl;
}

// Gère la commande TOPIC : voir ou changer le sujet d'un channel
// Format : TOPIC #channel [:<new topic>]
void Server::handleTopic(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "TOPIC :Not enough parameters");
        return;
    }

    // Extraire le nom du channel et le nouveau topic optionnel
    std::string channelName = params;
    std::string newTopic = "";
    bool hasTopic = false;
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        channelName = params.substr(0, space);
        newTopic = params.substr(space + 1);
        hasTopic = true;
        // Retirer le : au début du topic si présent
        if (!newTopic.empty() && newTopic[0] == ':')
            newTopic = newTopic.substr(1);
    }

    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
    if (it == _channels.end())
    {
        sendNumericReply(client, "403", channelName + " :No such channel");
        return;
    }

    Channel* channel = it->second;

    // Vérifier que le client est membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", channelName + " :You're not on that channel");
        return;
    }

    if (!hasTopic)
    {
        // Mode consultation : afficher le topic actuel
        if (channel->getTopic().empty())
            sendNumericReply(client, "331", channelName + " :No topic is set");
        else
            sendNumericReply(client, "332", channelName + " :" + channel->getTopic());
    }
    else
    {
        // Mode modification : changer le topic

        // Vérifier si le mode +t est actif (seuls les ops peuvent changer le topic)
        if (channel->isTopicRestricted() && !channel->isOperator(&client))
        {
            sendNumericReply(client, "482", channelName + " :You're not channel operator");
            return;
        }

        // Appliquer le nouveau topic
        channel->setTopic(newTopic);

        // Notifier tous les membres du changement de topic
        std::string topicMsg = getClientPrefix(client) + " TOPIC " + channelName + " :" + newTopic + "\r\n";
        channel->broadcastMessageAll(topicMsg);

        std::cout << "[TOPIC] " << client.getNickname() << " set topic of " << channelName << " to: " << newTopic << std::endl;
    }
}

// Gère la commande MODE : voir ou changer les modes d'un channel
// Format : MODE #channel [+/-flags] [params]
void Server::handleMode(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "MODE :Not enough parameters");
        return;
    }

    // Extraire le nom du channel
    std::string channelName = params;
    std::string modeString = "";
    std::string modeParams = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        channelName = params.substr(0, space);
        std::string rest = params.substr(space + 1);

        // Extraire la chaîne de modes et les paramètres
        size_t space2 = rest.find(' ');
        if (space2 != std::string::npos)
        {
            modeString = rest.substr(0, space2);
            modeParams = rest.substr(space2 + 1);
        }
        else
        {
            modeString = rest;
        }
    }

    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
    if (it == _channels.end())
    {
        sendNumericReply(client, "403", channelName + " :No such channel");
        return;
    }

    Channel* channel = it->second;

    // Si pas de mode string, afficher les modes actuels (RPL_CHANNELMODEIS 324)
    if (modeString.empty())
    {
        // Construire la chaîne des modes actifs
        std::string modes = "+";
        std::string mParams = "";

        if (channel->isInviteOnly())
            modes += "i";
        if (channel->isTopicRestricted())
            modes += "t";
        if (!channel->getKey().empty())
        {
            modes += "k";
            mParams += " " + channel->getKey();
        }
        if (channel->getUserLimit() > 0)
        {
            modes += "l";
            // Convertir le int en string (C++98)
            std::ostringstream oss;
            oss << channel->getUserLimit();
            mParams += " " + oss.str();
        }

        // Si aucun mode n'est actif, afficher juste +
        sendNumericReply(client, "324", channelName + " " + modes + mParams);
        return;
    }

    // Vérifier que le client est membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", channelName + " :You're not on that channel");
        return;
    }

    // Vérifier que le client est opérateur pour modifier les modes
    if (!channel->isOperator(&client))
    {
        sendNumericReply(client, "482", channelName + " :You're not channel operator");
        return;
    }

    // Découper les paramètres des modes en mots séparés
    std::vector<std::string> paramTokens;
    std::string remaining = modeParams;
    while (!remaining.empty())
    {
        size_t sp = remaining.find(' ');
        if (sp != std::string::npos)
        {
            paramTokens.push_back(remaining.substr(0, sp));
            remaining = remaining.substr(sp + 1);
        }
        else
        {
            paramTokens.push_back(remaining);
            remaining = "";
        }
    }

    // Parcourir la chaîne de modes caractère par caractère
    bool adding = true;          // +mode ou -mode
    size_t paramIndex = 0;       // Index du prochain paramètre à consommer
    std::string appliedModes = "";    // Modes effectivement appliqués
    std::string appliedParams = "";   // Paramètres des modes appliqués
    char lastSign = '\0';             // Dernier signe +/- affiché

    for (size_t i = 0; i < modeString.size(); ++i)
    {
        char c = modeString[i];

        // Changer le signe +/-
        if (c == '+')
        {
            adding = true;
            continue;
        }
        if (c == '-')
        {
            adding = false;
            continue;
        }

        // Traiter chaque mode
        switch (c)
        {
            case 'i':
            {
                // Mode invitation seulement
                channel->setInviteOnly(adding);

                // Ajouter au résumé des modes appliqués
                if ((adding && lastSign != '+') || (!adding && lastSign != '-'))
                {
                    appliedModes += adding ? "+" : "-";
                    lastSign = adding ? '+' : '-';
                }
                appliedModes += "i";
                break;
            }
            case 't':
            {
                // Mode restriction du topic aux opérateurs
                channel->setTopicRestricted(adding);

                if ((adding && lastSign != '+') || (!adding && lastSign != '-'))
                {
                    appliedModes += adding ? "+" : "-";
                    lastSign = adding ? '+' : '-';
                }
                appliedModes += "t";
                break;
            }
            case 'k':
            {
                // Mode clé/mot de passe du channel
                if (adding)
                {
                    // +k nécessite un paramètre (la clé)
                    if (paramIndex >= paramTokens.size())
                    {
                        sendNumericReply(client, "461", "MODE :Not enough parameters");
                        break;
                    }
                    std::string newKey = paramTokens[paramIndex++];
                    channel->setKey(newKey);

                    if ((adding && lastSign != '+') || (!adding && lastSign != '-'))
                    {
                        appliedModes += "+";
                        lastSign = '+';
                    }
                    appliedModes += "k";
                    appliedParams += " " + newKey;
                }
                else
                {
                    // -k retire la clé
                    channel->setKey("");

                    if (lastSign != '-')
                    {
                        appliedModes += "-";
                        lastSign = '-';
                    }
                    appliedModes += "k";
                }
                break;
            }
            case 'o':
            {
                // Mode opérateur
                // +o et -o nécessitent un paramètre (le pseudo)
                if (paramIndex >= paramTokens.size())
                {
                    sendNumericReply(client, "461", "MODE :Not enough parameters");
                    break;
                }
                std::string targetNick = paramTokens[paramIndex++];

                // Trouver le client cible
                Client* target = findClientByNickname(targetNick);
                if (!target || !channel->isMember(target))
                {
                    sendNumericReply(client, "441", targetNick + " " + channelName + " :They aren't on that channel");
                    break;
                }

                // Ajouter ou retirer le statut opérateur
                if (adding)
                    channel->addOperator(target);
                else
                    channel->removeOperator(target);

                if ((adding && lastSign != '+') || (!adding && lastSign != '-'))
                {
                    appliedModes += adding ? "+" : "-";
                    lastSign = adding ? '+' : '-';
                }
                appliedModes += "o";
                appliedParams += " " + targetNick;
                break;
            }
            case 'l':
            {
                // Mode limite de membres
                if (adding)
                {
                    // +l nécessite un paramètre (la limite)
                    if (paramIndex >= paramTokens.size())
                    {
                        sendNumericReply(client, "461", "MODE :Not enough parameters");
                        break;
                    }
                    int limit = std::atoi(paramTokens[paramIndex++].c_str());
                    if (limit <= 0)
                        break;  // Ignorer les limites invalides
                    channel->setUserLimit(limit);

                    if (lastSign != '+')
                    {
                        appliedModes += "+";
                        lastSign = '+';
                    }
                    appliedModes += "l";

                    // Convertir le int en string (C++98)
                    std::ostringstream oss;
                    oss << limit;
                    appliedParams += " " + oss.str();
                }
                else
                {
                    // -l retire la limite
                    channel->setUserLimit(0);

                    if (lastSign != '-')
                    {
                        appliedModes += "-";
                        lastSign = '-';
                    }
                    appliedModes += "l";
                }
                break;
            }
            default:
            {
                // Mode inconnu
                std::string unknownMode(1, c);
                sendNumericReply(client, "472", unknownMode + " :is unknown mode char to me");
                break;
            }
        }
    }

    // Si des modes ont été appliqués, notifier tous les membres du channel
    if (!appliedModes.empty())
    {
        std::string modeMsg = getClientPrefix(client) + " MODE " + channelName + " " + appliedModes + appliedParams + "\r\n";
        channel->broadcastMessageAll(modeMsg);

        std::cout << "[MODE] " << client.getNickname() << " set " << channelName << " " << appliedModes << appliedParams << std::endl;
    }
}

// Gère la commande QUIT : déconnexion volontaire du client
// Format : QUIT [:message]
void Server::handleQuit(Client& client, const std::string& params)
{
    // Extraire le message de départ
    std::string message = "Quit";
    if (!params.empty())
    {
        message = params;
        // Retirer le : au début du message si présent
        if (message[0] == ':')
            message = message.substr(1);
    }

    // Construire le message QUIT
    std::string quitMsg = getClientPrefix(client) + " QUIT :" + message + "\r\n";

    // Notifier tous les channels où le client est présent
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
    {
        if (it->second->isMember(&client))
            it->second->broadcastMessage(quitMsg, &client);
    }

    std::cout << "[QUIT] " << client.getNickname() << ": " << message << std::endl;

    // Trouver l'index dans poll_fds pour déconnecter le client
    for (size_t i = 1; i < _poll_fds.size(); ++i)
    {
        if (_poll_fds[i].fd == client.getFd())
        {
            disconnectClient(i);
            return;
        }
    }
}

// ============================================================================
//                          BOUCLE PRINCIPALE
// ============================================================================

// Boucle principale du serveur
void Server::run()
{
    _running = true;

    std::cout << "\n=== SERVER STARTED ===" << std::endl;
    std::cout << "Waiting for connections..." << std::endl;
    std::cout << "Press Ctrl+C to stop\n" << std::endl;

    while (_running)
    {
        // poll() surveille tous les file descriptors
        // -1 = timeout infini (attend qu'un événement arrive)
        int poll_count = poll(&_poll_fds[0], _poll_fds.size(), -1);

        if (poll_count < 0)
        {
            if (errno == EINTR)
                break;  // Signal reçu (ex: Ctrl+C) - on sort proprement
            std::cerr << "Poll error: " << strerror(errno) << std::endl;
            break;
        }

        // Parcourir tous les file descriptors surveillés
        for (size_t i = 0; i < _poll_fds.size(); ++i)
        {
            // Vérifier s'il y a un événement sur ce FD
            if (_poll_fds[i].revents & POLLIN)
            {
                if (_poll_fds[i].fd == _server_fd)
                {
                    // Événement sur le serveur = nouvelle connexion
                    acceptNewClient();
                }
                else
                {
                    // Événement sur un client = données reçues
                    size_t size_before = _poll_fds.size();
                    readFromClient(i);
                    // Si un client a été déconnecté, poll_fds a rétréci
                    // On décrémente i pour ne pas sauter le prochain client
                    if (_poll_fds.size() < size_before)
                        --i;
                }
            }
        }
    }
    std::cout << "\n=== SERVER STOPPED ===" << std::endl;
}

// Arrête le serveur proprement
void Server::stop()
{
    _running = false;

    std::cout << "\nClosing all connections..." << std::endl;

    // Fermer et libérer tous les clients
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        close(it->second->getFd());
        delete it->second;
    }
    _clients.clear();

    // Libérer tous les channels
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        delete it->second;
    _channels.clear();

    // Fermer le socket serveur
    if (_server_fd >= 0)
    {
        close(_server_fd);
        _server_fd = -1;
    }

    _poll_fds.clear();

    std::cout << "Server stopped cleanly." << std::endl;
}
