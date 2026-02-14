#include "Server.hpp"
#include <iostream>   // Pour std::cout
#include <sstream>    // Pour std::ostringstream
#include <cstdlib>    // Pour std::atoi()

// Gère la commande KICK : expulser un utilisateur d'un channel
// Format : KICK #channel <user> [:<reason>]
void Server::handleKick(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "KICK :Not enough parameters");
        return;
    }

    // Extraire le nom du channel
    std::string remaining = params;
    std::string channelName = "";
    size_t space = remaining.find(' ');
    if (space != std::string::npos)
    {
        channelName = remaining.substr(0, space);
        remaining = remaining.substr(space + 1);
    }
    else
    {
        sendNumericReply(client, "461", "KICK :Not enough parameters");
        return;
    }

    // Extraire le pseudo de la cible et la raison optionnelle
    std::string targetNick = remaining;
    std::string reason = client.getNickname();  // Par défaut, le kicker est la raison
    space = remaining.find(' ');
    if (space != std::string::npos)
    {
        targetNick = remaining.substr(0, space);
        reason = remaining.substr(space + 1);
        // Retirer le : au début de la raison si présent
        if (!reason.empty() && reason[0] == ':')
            reason = reason.substr(1);
    }

    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
    if (it == _channels.end())
    {
        sendNumericReply(client, "403", channelName + " :No such channel");
        return;
    }

    Channel* channel = it->second;

    // Vérifier que le kicker est membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", channelName + " :You're not on that channel");
        return;
    }

    // Vérifier que le kicker est opérateur
    if (!channel->isOperator(&client))
    {
        sendNumericReply(client, "482", channelName + " :You're not channel operator");
        return;
    }

    // Trouver le client cible par son pseudo
    Client* target = findClientByNickname(targetNick);
    if (!target)
    {
        sendNumericReply(client, "401", targetNick + " :No such nick/channel");
        return;
    }

    // Vérifier que la cible est membre du channel
    if (!channel->isMember(target))
    {
        sendNumericReply(client, "441", targetNick + " " + channelName + " :They aren't on that channel");
        return;
    }

    // Construire et envoyer le message KICK à tous les membres
    std::string kickMsg = getClientPrefix(client) + " KICK " + channelName + " " + targetNick + " :" + reason + "\r\n";
    channel->broadcastMessageAll(kickMsg);

    // Retirer la cible du channel
    channel->removeMember(target);

    // Si le channel est vide, le supprimer
    if (channel->getMembers().empty())
    {
        delete channel;
        _channels.erase(it);
    }

    std::cout << "[KICK] " << client.getNickname() << " kicked " << targetNick << " from " << channelName << std::endl;
}

// Gère la commande INVITE : inviter un utilisateur dans un channel
// Format : INVITE <nickname> #channel
void Server::handleInvite(Client& client, const std::string& params)
{
    // Extraire nickname et channel
    std::string nickname = params;
    std::string channelName = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        nickname = params.substr(0, space);
        channelName = params.substr(space + 1);
    }

    // Vérifier paramètres
    if (nickname.empty() || channelName.empty())
    {
        sendNumericReply(client, "461", "INVITE :Not enough parameters");
        return;
    }

    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
    if (it == _channels.end())
    {
        sendNumericReply(client, "403", channelName + " :No such channel");
        return;
    }

    Channel* channel = it->second;

    // Le client doit être membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", channelName + " :You're not on that channel");
        return;
    }

    // Le client doit être opérateur
    if (!channel->isOperator(&client))
    {
        sendNumericReply(client, "482", channelName + " :You're not channel operator");
        return;
    }

    // Vérifier que l'utilisateur cible existe
    Client* targetClient = findClientByNickname(nickname);
    if (!targetClient)
    {
        sendNumericReply(client, "401", nickname + " :No such nick/channel");
        return;
    }

    // Vérifier que la cible n'est pas déjà dans le channel
    if (channel->isMember(targetClient))
    {
        sendNumericReply(client, "443", nickname + " " + channelName + " :is already on channel");
        return;
    }

    // Ajouter à la liste des invités
    channel->addInvited(targetClient);

    // Notifier le client qui invite
    sendNumericReply(client, "341", nickname + " " + channelName);

    // Notifier l'utilisateur invité
    std::string inviteMsg = getClientPrefix(client) + " INVITE " + nickname + " " + channelName + "\r\n";
    sendToClient(*targetClient, inviteMsg);

    std::cout << "[INVITE] " << client.getNickname() << " invited " << nickname << " to " << channelName << std::endl;
}

// Gère la commande TOPIC : voir ou changer le sujet d'un channel
// Format : TOPIC #channel [:<new topic>]
void Server::handleTopic(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "TOPIC :Not enough parameters");
        return;
    }

    // Extraire le nom du channel et le nouveau topic optionnel
    std::string channelName = params;
    std::string newTopic = "";
    bool hasTopic = false;
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        channelName = params.substr(0, space);
        newTopic = params.substr(space + 1);
        hasTopic = true;
        // Retirer le : au début du topic si présent
        if (!newTopic.empty() && newTopic[0] == ':')
            newTopic = newTopic.substr(1);
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

    if (!hasTopic)
    {
        // Mode consultation : afficher le topic actuel
        if (channel->getTopic().empty())
            sendNumericReply(client, "331", channelName + " :No topic is set");
        else
            sendNumericReply(client, "332", channelName + " :" + channel->getTopic());
    }
    else
    {
        // Mode modification : changer le topic

        // Vérifier si le mode +t est actif (seuls les ops peuvent changer le topic)
        if (channel->isTopicRestricted() && !channel->isOperator(&client))
        {
            sendNumericReply(client, "482", channelName + " :You're not channel operator");
            return;
        }

        // Appliquer le nouveau topic
        channel->setTopic(newTopic);

        // Notifier tous les membres du changement de topic
        std::string topicMsg = getClientPrefix(client) + " TOPIC " + channelName + " :" + newTopic + "\r\n";
        channel->broadcastMessageAll(topicMsg);

        std::cout << "[TOPIC] " << client.getNickname() << " set topic of " << channelName << " to: " << newTopic << std::endl;
    }
}

// Gère la commande MODE : changer les modes d'un channel
// Format : MODE #channel [+/-modes] [paramètres]
void Server::handleMode(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "MODE :Not enough parameters");
        return;
    }

    // Extraire la cible (channel ou user)
    std::string target = params;
    size_t space = params.find(' ');
    if (space != std::string::npos)
        target = params.substr(0, space);

    // Si la cible n'est pas un channel (ne commence pas par #)
    if (target.empty() || target[0] != '#')
    {
        // MODE utilisateur : on ignore silencieusement
        return;
    }

    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(target);
    if (it == _channels.end())
    {
        sendNumericReply(client, "403", target + " :No such channel");
        return;
    }

    Channel* channel = it->second;

    // Vérifier que le client est membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", target + " :You're not on that channel");
        return;
    }

    // Si pas d'autres paramètres, afficher les modes actuels
    if (space == std::string::npos)
    {
        // Construire la chaîne des modes actifs
        std::string modes = "+";
        if (channel->isInviteOnly())
            modes += "i";
        if (channel->isTopicRestricted())
            modes += "t";
        if (!channel->getKey().empty())
            modes += "k";
        if (channel->getUserLimit() > 0)
            modes += "l";

        sendNumericReply(client, "324", target + " " + modes);
        return;
    }

    // Vérifier que le client est opérateur
    if (!channel->isOperator(&client))
    {
        sendNumericReply(client, "482", target + " :You're not channel operator");
        return;
    }

    // Extraire le mode string et les paramètres
    std::string modeParams = params.substr(space + 1);
    std::string modeString = modeParams;
    std::string modeArgs = "";

    space = modeParams.find(' ');
    if (space != std::string::npos)
    {
        modeString = modeParams.substr(0, space);
        modeArgs = modeParams.substr(space + 1);
    }

    // Parser et appliquer les modes
    bool adding = true;  // true = +, false = -
	bool validModeFound = false;
    for (size_t i = 0; i < modeString.size(); ++i)
    {
        char mode = modeString[i];

        if (mode == '+')
        {
            adding = true;
            continue;
        }
        else if (mode == '-')
        {
            adding = false;
            continue;
        }

        // Appliquer le mode
        switch (mode)
        {
            case 'i':  // Invite-only
                channel->setInviteOnly(adding);
				validModeFound = true;
                break;

            case 't':  // Topic restricted
                channel->setTopicRestricted(adding);
				validModeFound = true;
                break;

            case 'k':  // Key (password)
                if (adding)
                {
                    // Extraire le premier mot comme password
                    std::string key = modeArgs;
                    size_t sp = key.find(' ');
                    if (sp != std::string::npos)
                        key = key.substr(0, sp);
                    
                    if (key.empty())
                    {
                        sendNumericReply(client, "461", "MODE +k :Not enough parameters");
                        continue;
                    }
                    channel->setKey(key);
                }
                else
                {
                    channel->setKey("");
                }
				validModeFound = true;
                break;

            case 'o':  // Operator
            {
                // Extraire le nickname
                std::string nick = modeArgs;
                size_t sp = nick.find(' ');
                if (sp != std::string::npos)
                    nick = nick.substr(0, sp);

                if (nick.empty())
                {
                    sendNumericReply(client, "461", "MODE +o :Not enough parameters");
                    return;
                }

                Client* targetClient = findClientByNickname(nick);
                if (!targetClient)
                {
                    sendNumericReply(client, "401", nick + " :No such nick/channel");
                    return;
                }

                if (!channel->isMember(targetClient))
                {
                    sendNumericReply(client, "441", nick + " " + target + " :They aren't on that channel");
                    return;
                }

                if (adding)
                    channel->addOperator(targetClient);
                else
                    channel->removeOperator(targetClient);
				validModeFound = true;
                break;
            }

            case 'l':  // User limit
                if (adding)
                {
                    // Extraire le nombre
                    std::string limitStr = modeArgs;
                    size_t sp = limitStr.find(' ');
                    if (sp != std::string::npos)
                        limitStr = limitStr.substr(0, sp);

                    if (limitStr.empty())
                    {
                        sendNumericReply(client, "461", "MODE +l :Not enough parameters");
                        continue;
                    }

                    int limit = std::atoi(limitStr.c_str());
                    if (limit <= 0)
                    {
                        sendNumericReply(client, "461", "MODE +l :Invalid limit");
                        continue;
                    }
                    channel->setUserLimit(limit);
                }
                else
                {
                    channel->setUserLimit(0);
                }
				validModeFound = true;
                break;

            default:
                sendNumericReply(client, "472", std::string(1, mode) + " :is unknown mode char to me");
                continue;
        }
    }

    // Broadcast le changement de mode à tous les membres si mode valide
    if (validModeFound)
	{
    	std::string modeMsg = getClientPrefix(client) + " MODE " + target + " " + modeParams + "\r\n";
    	channel->broadcastMessageAll(modeMsg);
    	std::cout << "[MODE] " << client.getNickname() << " set mode " << modeParams << " on " << target << std::endl;
	}
}
