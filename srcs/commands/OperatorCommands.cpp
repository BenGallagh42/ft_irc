#include "Server.hpp"
#include <iostream>   // Pour std::cout
#include <sstream>    // Pour std::ostringstream
#include <cstdlib>    // Pour std::atoi()

// Gère la commande KICK : expulser un utilisateur d'un channel
void Server::handleKick(Client& client, const std::string& params)
{
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
void Server::handleTopic(Client& client, const std::string& params)
{
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
void Server::handleMode(Client& client, const std::string& params) {
    // Parser: MODE <channel> <modes> [<params>...]
    
    std::istringstream iss(params);
    std::string target;
    std::string modeString;
    
    // Extraire le channel
    if (!(iss >> target)) {
        sendNumericReply(client, "461", "MODE :Not enough parameters");
        return;
    }
    
    // Extraire la chaîne de modes
    if (!(iss >> modeString)) {
        // Pas de modes fournis → afficher les modes actuels
        if (target[0] == '#') {
            std::map<std::string, Channel*>::iterator it = _channels.find(target);
            if (it == _channels.end()) {
                sendNumericReply(client, "403", target + " :No such channel");
                return;
            }
            
            Channel* channel = it->second;
            if (!channel->isMember(&client)) {
                sendNumericReply(client, "442", target + " :You're not on that channel");
                return;
            }
            
            // Construire la chaîne des modes actifs
            std::string activeModes = "+";
            std::string modeParams = "";
            
            if (channel->isInviteOnly())
                activeModes += "i";
            if (channel->isTopicRestricted())
                activeModes += "t";
            if (!channel->getKey().empty()) {
                activeModes += "k";
                modeParams += " " + channel->getKey();
            }
            if (channel->getUserLimit() > 0) {
                activeModes += "l";
                std::ostringstream oss;
                oss << channel->getUserLimit();
                modeParams += " " + oss.str();
            }
            
            if (activeModes == "+")
                activeModes = "+";
            
            sendNumericReply(client, "324", target + " " + activeModes + modeParams);
            return;
        }
        sendNumericReply(client, "461", "MODE :Not enough parameters");
        return;
    }
    
    // Vérifier que c'est un channel
    if (target[0] != '#') {
        sendNumericReply(client, "501", ":Unknown MODE flag");
        return;
    }
    
    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(target);
    if (it == _channels.end()) {
        sendNumericReply(client, "403", target + " :No such channel");
        return;
    }
    
    Channel* channel = it->second;
    
    // Vérifier que le client est dans le channel
    if (!channel->isMember(&client)) {
        sendNumericReply(client, "442", target + " :You're not on that channel");
        return;
    }
    
    // Vérifier que le client est opérateur
    if (!channel->isOperator(&client)) {
        sendNumericReply(client, "482", target + " :You're not channel operator");
        return;
    }
    
    // Extraire les paramètres restants
    std::vector<std::string> modeParams;
    std::string param;
    while (iss >> param) {
        modeParams.push_back(param);
    }
    
    // Parser et appliquer les modes
    bool adding = true;
    size_t paramIndex = 0;
    std::string broadcastModes = "";
    std::string broadcastParams = "";
    bool validModeFound = false;
    
    for (size_t i = 0; i < modeString.size(); ++i) {
        char mode = modeString[i];
        
        if (mode == '+') {
            adding = true;
            if (broadcastModes.empty() || broadcastModes[broadcastModes.size() - 1] != '+')
                broadcastModes += '+';
            continue;
        }
        else if (mode == '-') {
            adding = false;
            if (broadcastModes.empty() || broadcastModes[broadcastModes.size() - 1] != '-')
                broadcastModes += '-';
            continue;
        }
        
        // Appliquer le mode
        switch (mode) {
            case 'i':
                channel->setInviteOnly(adding);
                broadcastModes += 'i';
                validModeFound = true;
                break;
            
            case 't':
                channel->setTopicRestricted(adding);
                broadcastModes += 't';
                validModeFound = true;
                break;
            
            case 'k': {
                if (adding) {
                    // +k nécessite un paramètre
                    if (paramIndex >= modeParams.size()) {
                        sendNumericReply(client, "461", "MODE +k :Not enough parameters");
                        continue;
                    }
                    std::string key = modeParams[paramIndex++];
                    channel->setKey(key);
                    broadcastModes += 'k';
                    broadcastParams += " " + key;
                } else {
                    // -k
                    channel->setKey("");
                    broadcastModes += 'k';
                }
                validModeFound = true;
                break;
            }
            
            case 'o': {
                // +o/-o nécessite un paramètre (nickname)
                if (paramIndex >= modeParams.size()) {
                    sendNumericReply(client, "461", "MODE +o :Not enough parameters");
                    continue;
                }
                
                std::string targetNick = modeParams[paramIndex++];
                Client* targetClient = findClientByNickname(targetNick);
                
                if (!targetClient) {
                    sendNumericReply(client, "401", targetNick + " :No such nick/channel");
                    continue;
                }
                
                if (!channel->isMember(targetClient)) {
                    sendNumericReply(client, "441", targetNick + " " + target + " :They aren't on that channel");
                    continue;
                }
                
                if (adding) {
                    channel->addOperator(targetClient);
                } else {
                    channel->removeOperator(targetClient);
                }
                
                broadcastModes += 'o';
                broadcastParams += " " + targetNick;
                validModeFound = true;
                break;
            }
            
            case 'l': {
                if (adding) {
                    // +l nécessite un paramètre (limite)
                    if (paramIndex >= modeParams.size()) {
                        sendNumericReply(client, "461", "MODE +l :Not enough parameters");
                        continue;
                    }
                    
                    std::string limitStr = modeParams[paramIndex++];
                    int limit = std::atoi(limitStr.c_str());
                    
                    if (limit <= 0) {
                        sendNumericReply(client, "461", "MODE +l :Invalid user limit");
                        continue;
                    }
                    
                    channel->setUserLimit(limit);
                    broadcastModes += 'l';
                    broadcastParams += " " + limitStr;
                } else {
                    // -l
                    channel->setUserLimit(0);
                    broadcastModes += 'l';
                }
                validModeFound = true;
                break;
            }
            
            default:
                sendNumericReply(client, "472", std::string(1, mode) + " :is unknown mode char to me");
                continue;
        }
    }
    
    // Broadcast seulement si au moins un mode valide a été appliqué
    if (validModeFound && !broadcastModes.empty()) {
        std::string modeMsg = getClientPrefix(client) + " MODE " + target + " " + broadcastModes + broadcastParams + "\r\n";
        channel->broadcastMessageAll(modeMsg);
        std::cout << "[MODE] " << client.getNickname() << " set mode " << broadcastModes << broadcastParams << " on " << target << std::endl;
    }
}
