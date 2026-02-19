#include "Server.hpp"
#include <sys/socket.h>  // Pour send()
#include <iostream>      // Pour std::cout, std::cerr
#include <cctype>        // Pour std::toupper()

// Envoie un message brut à un client via son socket
void Server::sendToClient(Client& client, const std::string& message)
{
    // Ajouter \r\n si pas déjà présent (format IRC obligatoire)
    std::string msg = message;
    if (msg.length() < 2 || msg.substr(msg.length() - 2) != "\r\n")
        msg += "\r\n";

    send(client.getFd(), msg.c_str(), msg.length(), 0);
}

// Envoie une réponse numérique IRC au format :servername CODE nick :message
void Server::sendNumericReply(Client& client, const std::string& code, const std::string& message)
{
    std::string nick = client.getNickname().empty() ? "*" : client.getNickname();
    std::string reply = ":" + _serverName + " " + code + " " + nick + " " + message;
    sendToClient(client, reply);
}

// Construit le préfixe IRC d'un client (:nick!user@host)
std::string Server::getClientPrefix(Client& client)
{
    return ":" + client.getNickname() + "!" + client.getUsername() + "@localhost";
}

// Extrait le premier mot (la commande IRC) du message et le met en majuscules
std::string Server::extractCommand(const std::string& message)
{
    size_t start = message.find_first_not_of(" \t\r");
    if (start == std::string::npos)
        return "";
    
    std::string trimmed = message.substr(start);
    
    size_t space = trimmed.find_first_of(" \t");
    std::string cmd = (space != std::string::npos) ? trimmed.substr(0, space) : trimmed;

    for (size_t i = 0; i < cmd.size(); ++i)
        cmd[i] = std::toupper(cmd[i]);

    return cmd;
}

// Extrait tout après la commande (les paramètres)
std::string Server::extractParams(const std::string& message)
{
    size_t start = message.find_first_not_of(" \t\r");
    if (start == std::string::npos)
        return "";
    
    std::string trimmed = message.substr(start);
    
    size_t space = trimmed.find_first_of(" \t");
    if (space == std::string::npos)
        return "";

    std::string params = trimmed.substr(space + 1);
    size_t paramStart = params.find_first_not_of(" \t");
    if (paramStart == std::string::npos)
        return "";
    
    return params.substr(paramStart);
}

// Cherche un client par son pseudo dans la map des clients
Client* Server::findClientByNickname(const std::string& nickname)
{
    for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
    {
        if (it->second->getNickname() == nickname)
            return it->second;
    }

    return NULL;
}

// Retire un client de tous les channels (appelé lors de la déconnexion)
void Server::removeClientFromAllChannels(Client* client)
{
    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); )
    {
        std::string quitMsg = getClientPrefix(*client) + " QUIT :Connection closed\r\n";

        if (it->second->isMember(client))
            it->second->broadcastMessage(quitMsg, client);

        it->second->removeMember(client);

        if (it->second->getMembers().empty())
        {
            delete it->second;
            _channels.erase(it++);  // Post-incrémentation pour éviter l'invalidation de l'itérateur
        }
        else
            ++it;
    }
}
