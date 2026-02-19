#include "Server.hpp"
#include <sys/socket.h>  // Pour send()
#include <iostream>      // Pour std::cout, std::cerr
#include <cctype>        // Pour std::toupper()

// ============================================================================
// ENVOI DE MESSAGES

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

// ============================================================================
// PARSING DE COMMANDES

// Extrait le premier mot (la commande IRC) du message et le met en majuscules
std::string Server::extractCommand(const std::string& message)
{
    // Trim les espaces/tabs au début
    size_t start = message.find_first_not_of(" \t\r");
    if (start == std::string::npos)
        return "";  // Message vide
    
    std::string trimmed = message.substr(start);
    
    // Trouver le premier espace
    size_t space = trimmed.find_first_of(" \t");
    std::string cmd = (space != std::string::npos) ? trimmed.substr(0, space) : trimmed;

    // Convertir en majuscules
    for (size_t i = 0; i < cmd.size(); ++i)
        cmd[i] = std::toupper(cmd[i]);

    return cmd;
}

// Extrait tout après la commande (les paramètres)
std::string Server::extractParams(const std::string& message)
{
    // Trim les espaces au début
    size_t start = message.find_first_not_of(" \t\r");
    if (start == std::string::npos)
        return "";
    
    std::string trimmed = message.substr(start);
    
    // Trouver le premier espace
    size_t space = trimmed.find_first_of(" \t");
    if (space == std::string::npos)
        return "";

    // Retourner tout après l'espace, en trimmant les espaces de début
    std::string params = trimmed.substr(space + 1);
    size_t paramStart = params.find_first_not_of(" \t");
    if (paramStart == std::string::npos)
        return "";
    
    return params.substr(paramStart);
}

// ============================================================================
// RECHERCHE ET CLEANUP

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
