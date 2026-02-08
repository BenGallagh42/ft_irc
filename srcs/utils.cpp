#include "utils.hpp"
#include <fcntl.h>
#include <iostream>
#include <cstdlib>

// Met un file descriptor en mode non-bloquant
// Permet à recv() et send() de retourner immédiatement au lieu d'attendre
bool set_nonblocking(int fd) 
{
    // Récupère les flags actuels du file descriptor
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        std::cerr << "Error: fcntl F_GETFL failed" << std::endl;
        return (false);
    }
    
    // Ajoute le flag O_NONBLOCK (mode non-bloquant)
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        std::cerr << "Error: fcntl F_SETFL failed" << std::endl;
        return (false);
    }
    
    return (true);
}

// Affiche un message d'erreur et termine le programme
void error_exit(const std::string& message)
{
    std::cerr << "Error: " << message << std::endl;
    exit(1);
}