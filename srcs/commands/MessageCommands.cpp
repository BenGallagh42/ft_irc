#include "Server.hpp"

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

        // Le client doit être membre du channel
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
