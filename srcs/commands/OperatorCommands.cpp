#include "Server.hpp"
#include <iostream>   // Pour std::cout
#include <sstream>    // Pour std::ostringstream
#include <cstdlib>    // Pour std::atoi()

// ============================================================================
//                         HANDLERS OPÉRATEUR
// ============================================================================

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
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "INVITE :Not enough parameters");
        return;
    }

    // Extraire le pseudo de la cible et le nom du channel
    std::string targetNick = params;
    std::string channelName = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        targetNick = params.substr(0, space);
        channelName = params.substr(space + 1);
        // Retirer les espaces en trop
        size_t space2 = channelName.find(' ');
        if (space2 != std::string::npos)
            channelName = channelName.substr(0, space2);
    }
    else
    {
        sendNumericReply(client, "461", "INVITE :Not enough parameters");
        return;
    }

    // Trouver le client cible par son pseudo
    Client* target = findClientByNickname(targetNick);
    if (!target)
    {
        sendNumericReply(client, "401", targetNick + " :No such nick/channel");
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

    // Vérifier que l'inviteur est membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", channelName + " :You're not on that channel");
        return;
    }

    // Si le channel est en mode +i, seuls les opérateurs peuvent inviter
    if (channel->isInviteOnly() && !channel->isOperator(&client))
    {
        sendNumericReply(client, "482", channelName + " :You're not channel operator");
        return;
    }

    // Vérifier que la cible n'est pas déjà dans le channel
    if (channel->isMember(target))
    {
        sendNumericReply(client, "443", targetNick + " " + channelName + " :is already on channel");
        return;
    }

    // Ajouter la cible à la liste des invités du channel
    channel->addInvited(target);

    // Envoyer la confirmation à l'inviteur (RPL_INVITING 341)
    sendNumericReply(client, "341", targetNick + " " + channelName);

    // Envoyer la notification d'invitation à la cible
    std::string inviteMsg = getClientPrefix(client) + " INVITE " + targetNick + " " + channelName + "\r\n";
    sendToClient(*target, inviteMsg);

    std::cout << "[INVITE] " << client.getNickname() << " invited " << targetNick << " to " << channelName << std::endl;
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

// Gère la commande MODE : voir ou changer les modes d'un channel
// Format : MODE #channel [+/-flags] [params]
void Server::handleMode(Client& client, const std::string& params)
{
    // Vérifier que les paramètres sont fournis
    if (params.empty())
    {
        sendNumericReply(client, "461", "MODE :Not enough parameters");
        return;
    }

    // Extraire le nom du channel
    std::string channelName = params;
    std::string modeString = "";
    std::string modeParams = "";
    size_t space = params.find(' ');
    if (space != std::string::npos)
    {
        channelName = params.substr(0, space);
        std::string rest = params.substr(space + 1);

        // Extraire la chaîne de modes et les paramètres
        size_t space2 = rest.find(' ');
        if (space2 != std::string::npos)
        {
            modeString = rest.substr(0, space2);
            modeParams = rest.substr(space2 + 1);
        }
        else
        {
            modeString = rest;
        }
    }

    // Vérifier que le channel existe
    std::map<std::string, Channel*>::iterator it = _channels.find(channelName);
    if (it == _channels.end())
    {
        sendNumericReply(client, "403", channelName + " :No such channel");
        return;
    }

    Channel* channel = it->second;

    // Si pas de mode string, afficher les modes actuels (RPL_CHANNELMODEIS 324)
    if (modeString.empty())
    {
        // Construire la chaîne des modes actifs
        std::string modes = "+";
        std::string mParams = "";

        if (channel->isInviteOnly())
            modes += "i";
        if (channel->isTopicRestricted())
            modes += "t";
        if (!channel->getKey().empty())
        {
            modes += "k";
            mParams += " " + channel->getKey();
        }
        if (channel->getUserLimit() > 0)
        {
            modes += "l";
            // Convertir le int en string (C++98)
            std::ostringstream oss;
            oss << channel->getUserLimit();
            mParams += " " + oss.str();
        }

        // Si aucun mode n'est actif, afficher juste +
        sendNumericReply(client, "324", channelName + " " + modes + mParams);
        return;
    }

    // Vérifier que le client est membre du channel
    if (!channel->isMember(&client))
    {
        sendNumericReply(client, "442", channelName + " :You're not on that channel");
        return;
    }

    // Vérifier que le client est opérateur pour modifier les modes
    if (!channel->isOperator(&client))
    {
        sendNumericReply(client, "482", channelName + " :You're not channel operator");
        return;
    }

    // Découper les paramètres des modes en mots séparés
    std::vector<std::string> paramTokens;
    std::string remaining = modeParams;
    while (!remaining.empty())
    {
        size_t sp = remaining.find(' ');
        if (sp != std::string::npos)
        {
            paramTokens.push_back(remaining.substr(0, sp));
            remaining = remaining.substr(sp + 1);
        }
        else
        {
            paramTokens.push_back(remaining);
            remaining = "";
        }
    }

    // Parcourir la chaîne de modes caractère par caractère
    bool adding = true;          // +mode ou -mode
    size_t paramIndex = 0;       // Index du prochain paramètre à consommer
    std::string appliedModes = "";    // Modes effectivement appliqués
    std::string appliedParams = "";   // Paramètres des modes appliqués
    char lastSign = '\0';             // Dernier signe +/- affiché

    for (size_t i = 0; i < modeString.size(); ++i)
    {
        char c = modeString[i];

        // Changer le signe +/-
        if (c == '+')
        {
            adding = true;
            continue;
        }
        if (c == '-')
        {
            adding = false;
            continue;
        }

        // Traiter chaque mode
        switch (c)
        {
            case 'i':
            {
                // Mode invitation seulement
                channel->setInviteOnly(adding);

                // Ajouter au résumé des modes appliqués
                if ((adding && lastSign != '+') || (!adding && lastSign != '-'))
                {
                    appliedModes += adding ? "+" : "-";
                    lastSign = adding ? '+' : '-';
                }
                appliedModes += "i";
                break;
            }
            case 't':
            {
                // Mode restriction du topic aux opérateurs
                channel->setTopicRestricted(adding);

                if ((adding && lastSign != '+') || (!adding && lastSign != '-'))
                {
                    appliedModes += adding ? "+" : "-";
                    lastSign = adding ? '+' : '-';
                }
                appliedModes += "t";
                break;
            }
            case 'k':
            {
                // Mode clé/mot de passe du channel
                if (adding)
                {
                    // +k nécessite un paramètre (la clé)
                    if (paramIndex >= paramTokens.size())
                    {
                        sendNumericReply(client, "461", "MODE :Not enough parameters");
                        break;
                    }
                    std::string newKey = paramTokens[paramIndex++];
                    channel->setKey(newKey);

                    if ((adding && lastSign != '+') || (!adding && lastSign != '-'))
                    {
                        appliedModes += "+";
                        lastSign = '+';
                    }
                    appliedModes += "k";
                    appliedParams += " " + newKey;
                }
                else
                {
                    // -k retire la clé
                    channel->setKey("");

                    if (lastSign != '-')
                    {
                        appliedModes += "-";
                        lastSign = '-';
                    }
                    appliedModes += "k";
                }
                break;
            }
            case 'o':
            {
                // Mode opérateur
                // +o et -o nécessitent un paramètre (le pseudo)
                if (paramIndex >= paramTokens.size())
                {
                    sendNumericReply(client, "461", "MODE :Not enough parameters");
                    break;
                }
                std::string targetNick = paramTokens[paramIndex++];

                // Trouver le client cible
                Client* target = findClientByNickname(targetNick);
                if (!target || !channel->isMember(target))
                {
                    sendNumericReply(client, "441", targetNick + " " + channelName + " :They aren't on that channel");
                    break;
                }

                // Ajouter ou retirer le statut opérateur
                if (adding)
                    channel->addOperator(target);
                else
                    channel->removeOperator(target);

                if ((adding && lastSign != '+') || (!adding && lastSign != '-'))
                {
                    appliedModes += adding ? "+" : "-";
                    lastSign = adding ? '+' : '-';
                }
                appliedModes += "o";
                appliedParams += " " + targetNick;
                break;
            }
            case 'l':
            {
                // Mode limite de membres
                if (adding)
                {
                    // +l nécessite un paramètre (la limite)
                    if (paramIndex >= paramTokens.size())
                    {
                        sendNumericReply(client, "461", "MODE :Not enough parameters");
                        break;
                    }
                    int limit = std::atoi(paramTokens[paramIndex++].c_str());
                    if (limit <= 0)
                        break;  // Ignorer les limites invalides
                    channel->setUserLimit(limit);

                    if (lastSign != '+')
                    {
                        appliedModes += "+";
                        lastSign = '+';
                    }
                    appliedModes += "l";

                    // Convertir le int en string (C++98)
                    std::ostringstream oss;
                    oss << limit;
                    appliedParams += " " + oss.str();
                }
                else
                {
                    // -l retire la limite
                    channel->setUserLimit(0);

                    if (lastSign != '-')
                    {
                        appliedModes += "-";
                        lastSign = '-';
                    }
                    appliedModes += "l";
                }
                break;
            }
            default:
            {
                // Mode inconnu
                std::string unknownMode(1, c);
                sendNumericReply(client, "472", unknownMode + " :is unknown mode char to me");
                break;
            }
        }
    }

    // Si des modes ont été appliqués, notifier tous les membres du channel
    if (!appliedModes.empty())
    {
        std::string modeMsg = getClientPrefix(client) + " MODE " + channelName + " " + appliedModes + appliedParams + "\r\n";
        channel->broadcastMessageAll(modeMsg);

        std::cout << "[MODE] " << client.getNickname() << " set " << channelName << " " << appliedModes << appliedParams << std::endl;
    }
}
