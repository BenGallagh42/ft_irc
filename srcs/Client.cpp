#include "Client.hpp"

// Constructeur : initialise un nouveau client avec son file descriptor
Client::Client(int fd) : _fd(fd), _authenticated(false), _registered(false)
{
}

// Destructeur
Client::~Client()
{
}

// Retourne le file descriptor du socket client
int Client::getFd() const
{
    return (_fd);
}

// Retourne le pseudo du client
std::string Client::getNickname() const
{
    return (_nickname);
}

// Retourne le nom d'utilisateur du client
std::string Client::getUsername() const
{
    return (_username);
}

// Retourne le buffer de données reçues
std::string Client::getBuffer() const
{
    return (_buffer);
}

// Retourne true si le client a fourni le bon mot de passe
bool Client::isAuthenticated() const
{
    return (_authenticated);
}

// Retourne true si le client a complété NICK + USER
bool Client::isRegistered() const
{
    return (_registered);
}

// Définit le pseudo du client
void Client::setNickname(const std::string& nickname)
{
    _nickname = nickname;
}

// Définit le nom d'utilisateur du client
void Client::setUsername(const std::string& username)
{
    _username = username;
}

// Marque le client comme authentifié (ou non)
void Client::setAuthenticated(bool auth)
{
    _authenticated = auth;
}

// Marque le client comme enregistré (ou non)
void Client::setRegistered(bool reg)
{
    _registered = reg;
}

// Ajoute des données au buffer (accumulation des données reçues)
void Client::appendToBuffer(const std::string& data)
{
    _buffer += data;
}

// Vide complètement le buffer
void Client::clearBuffer()
{
    _buffer.clear();
}