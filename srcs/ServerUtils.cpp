#include "Server.hpp"
#include <sys/socket.h>  // Pour send()
#include <iostream>      // Pour std::cout, std::cerr
#include <cctype>        // Pour std::toupper()

// ============================================================================
//                           ENVOI DE MESSAGES
// ============================================================================

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
//                           PARSING DE COMMANDES
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

// ============================================================================
//                           RECHERCHE ET CLEANUP
// ============================================================================

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
