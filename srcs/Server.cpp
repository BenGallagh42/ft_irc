#include "Server.hpp"
#include "utils.hpp"
#include <sys/socket.h>  // Pour socket(), bind(), listen(), accept(), send()
#include <netinet/in.h>  // Pour struct sockaddr_in
#include <arpa/inet.h>   // Pour inet_ntop()
#include <unistd.h>      // Pour close()
#include <cstring>       // Pour memset()
#include <cerrno>        // Pour errno
#include <iostream>      // Pour std::cout, std::cerr

// ============================================================================
// CONSTRUCTEUR / DESTRUCTEUR

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
// CONFIGURATION DU SERVEUR

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
// GESTION DES CONNEXIONS

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
    
    // Vérifier que le client existe dans la map
    if (_clients.find(client_fd) == _clients.end())
        return;  // Le client a été déconnecté entre-temps
    
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
        {
            processCommand(*client, command);
            
            // Si le client a été déconnecté (QUIT), on arrête ici
            if (_clients.find(client_fd) == _clients.end())
                return;  // Le client n'existe plus, on sort
        }
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
// BOUCLE PRINCIPALE

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
