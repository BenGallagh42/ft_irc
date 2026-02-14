#include "Server.hpp"
#include <iostream>    // Pour std::cout, std::cerr
#include <cstdlib>     // Pour std::atoi(), exit()
#include <csignal>     // Pour signal(), SIGINT, SIGTERM, SIGQUIT

// Pointeur global pour gérer Ctrl+C proprement
Server* g_server = NULL;

// Gestionnaire de signal : appelé quand on appuie sur Ctrl+C
void signal_handler(int signum)
{
    (void)signum;
    std::cout << "\n\nReceived interrupt signal" << std::endl;
    if (g_server)
        g_server->stop();
    exit(0);
}

int main(int argc, char** argv)
{
    // Vérifier le nombre d'arguments
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 6667 mypassword" << std::endl;
        return (1);
    }
    
    // Convertir le port en entier
    int port = std::atoi(argv[1]);
    if (port <= 0 || port > 65535) // 16 bits = 2^16 = 65536 valeurs possibles
    {
        std::cerr << "Error: Invalid port (must be 1-65535)" << std::endl;
        return (1);
    }
    
    // Récupérer le mot de passe
    std::string password = argv[2];
    if (password.empty())
    {
        std::cerr << "Error: Password cannot be empty" << std::endl;
        return (1);
    }
    
    // Installer les gestionnaires de signaux
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // kill
    signal(SIGQUIT, signal_handler);  // Ctrl+D
    
    try
    {
        // Créer et lancer le serveur
        Server server(port, password);
        g_server = &server;
        
        server.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return (1);
    }
    
    return (0);
}