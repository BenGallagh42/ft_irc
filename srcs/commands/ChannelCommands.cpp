#include "Server.hpp"
#include <iostream>  // Pour std::cout

// Gère la commande JOIN : rejoindre un salon
void Server::handleJoin(Client& client, const std::string& params)
{
    if (params.empty())
    {
        sendNumericReply(client, "461", "JOIN :Not enough parameters");
        return;
    }

    std::string channelName = params;
    std::string key = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        channelName = params.substr(0, space);
        key = params.substr(space + 1);
    }

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
        channel = it->second;

        // Vérifier si le client est déjà membre
        if (channel->isMember(&client))
            return;

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
        channel = new Channel(channelName);
        _channels[channelName] = channel;

        channel->addOperator(&client);

        std::cout << "[CHANNEL] Created: " << channelName << std::endl;
    }

    channel->addMember(&client);
    channel->removeInvited(&client);

    std::string joinMsg = getClientPrefix(client) + " JOIN " + channelName + "\r\n";
    channel->broadcastMessageAll(joinMsg);

    if (!channel->getTopic().empty())
        sendNumericReply(client, "332", channelName + " :" + channel->getTopic());

    // Construire la liste des membres pour RPL_NAMREPLY (353)
    std::string namesList = "";
    std::vector<Client*>& members = channel->getMembers();
    for (size_t i = 0; i < members.size(); ++i)
    {
        if (i > 0)
            namesList += " ";

        if (channel->isOperator(members[i]))
            namesList += "@";

        namesList += members[i]->getNickname();
    }

    // Envoyer RPL_NAMREPLY (353) et RPL_ENDOFNAMES (366)
    sendNumericReply(client, "353", "= " + channelName + " :" + namesList);
    sendNumericReply(client, "366", channelName + " :End of /NAMES list");

    std::cout << "[JOIN] " << client.getNickname() << " joined " << channelName << std::endl;
}

// Gère la commande PART : quitter un salon
void Server::handlePart(Client& client, const std::string& params)
{
    if (params.empty())
    {
        sendNumericReply(client, "461", "PART :Not enough parameters");
        return;
    }

    std::string channelName = params;
    std::string message = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        channelName = params.substr(0, space);
        message = params.substr(space + 1);
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

    channel->broadcastMessageAll(partMsg);
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
