#include "Server.hpp"
#include <iostream>  // Pour std::cout

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
