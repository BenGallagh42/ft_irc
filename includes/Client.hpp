#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

// Représente un client connecté au serveur IRC
class Client {
private:
    int _fd;                    // File descriptor du socket client
    std::string _nickname;      // Pseudo du client (défini avec NICK)
    std::string _username;      // Nom d'utilisateur (défini avec USER)
    std::string _buffer;        // Buffer pour accumuler les données reçues
    bool _authenticated;        // Le client a-t-il fourni le bon mot de passe ?
    bool _registered;           // Le client a-t-il complété NICK + USER ?

public:
    // Constructeur : crée un nouveau client avec son file descriptor
    Client(int fd);
    ~Client();
    
    // Getters
    int getFd() const;
    std::string getNickname() const;
    std::string getUsername() const;
    std::string getBuffer() const;
    bool isAuthenticated() const;
    bool isRegistered() const;
    
    // Setters
    void setNickname(const std::string& nickname);
    void setUsername(const std::string& username);
    void setAuthenticated(bool auth);
    void setRegistered(bool reg);
    
    // Ajoute des données reçues au buffer du client
    void appendToBuffer(const std::string& data);
    
    // Vide le buffer du client
    void clearBuffer();
};

#endif