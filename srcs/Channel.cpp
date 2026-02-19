#include "Channel.hpp"
#include "Client.hpp"
#include <sys/socket.h>  // Pour send()
#include <algorithm>     // Pour std::find()
#include <iostream>      // Pour std::cout, std::cerr

// Constructeur : initialise un channel avec son nom et les modes par défaut
Channel::Channel(const std::string& name) : _name(name), _inviteOnly(false), _topicRestricted(false), _userLimit(0)
{
}

// Destructeur
Channel::~Channel()
{
}

// Retourne le nom du channel
std::string Channel::getName() const
{
    return _name;
}

// Retourne le sujet du channel
std::string Channel::getTopic() const
{
    return _topic;
}

// Retourne le mot de passe du channel
std::string Channel::getKey() const
{
    return _key;
}

// Retourne true si le channel est en mode invitation seulement
bool Channel::isInviteOnly() const
{
    return _inviteOnly;
}

// Retourne true si seuls les opérateurs peuvent changer le topic
bool Channel::isTopicRestricted() const
{
    return _topicRestricted;
}

// Retourne la limite de membres (0 = pas de limite)
int Channel::getUserLimit() const
{
    return _userLimit;
}

// Retourne la liste des membres du channel
std::vector<Client*>& Channel::getMembers()
{
    return _members;
}

// Définit le sujet du channel
void Channel::setTopic(const std::string& topic)
{
    _topic = topic;
}

// Définit le mot de passe du channel (mode +k)
void Channel::setKey(const std::string& key)
{
    _key = key;
}

// Active ou désactive le mode invitation seulement (+i)
void Channel::setInviteOnly(bool inviteOnly)
{
    _inviteOnly = inviteOnly;
}

// Active ou désactive la restriction du topic aux opérateurs (+t)
void Channel::setTopicRestricted(bool restricted)
{
    _topicRestricted = restricted;
}

// Définit la limite de membres du channel (mode +l)
void Channel::setUserLimit(int limit)
{
    _userLimit = limit;
}

// --- Gestion des membres ---

// Ajoute un client à la liste des membres du channel
void Channel::addMember(Client* client)
{
    if (!isMember(client))
        _members.push_back(client);
}

// Retire un client de la liste des membres du channel
void Channel::removeMember(Client* client)
{
    std::vector<Client*>::iterator it = std::find(_members.begin(), _members.end(), client);
    if (it != _members.end())
        _members.erase(it);

    removeOperator(client);
    removeInvited(client);
}

// Vérifie si un client est membre du channel
bool Channel::isMember(Client* client) const
{
    return std::find(_members.begin(), _members.end(), client) != _members.end();
}

// --- Gestion des opérateurs ---

// Ajoute un client comme opérateur du channel
void Channel::addOperator(Client* client)
{
    if (!isOperator(client))
        _operators.push_back(client);
}

// Retire un client de la liste des opérateurs
void Channel::removeOperator(Client* client)
{
    std::vector<Client*>::iterator it = std::find(_operators.begin(), _operators.end(), client);
    if (it != _operators.end())
        _operators.erase(it);
}

// Vérifie si un client est opérateur du channel
bool Channel::isOperator(Client* client) const
{
    return std::find(_operators.begin(), _operators.end(), client) != _operators.end();
}

// --- Gestion des invitations ---

// Ajoute un client à la liste des invités du channel
void Channel::addInvited(Client* client)
{
    if (!isInvited(client))
        _invited.push_back(client);
}

// Vérifie si un client est dans la liste des invités
bool Channel::isInvited(Client* client) const
{
    return std::find(_invited.begin(), _invited.end(), client) != _invited.end();
}

// Retire un client de la liste des invités
void Channel::removeInvited(Client* client)
{
    std::vector<Client*>::iterator it = std::find(_invited.begin(), _invited.end(), client);
    if (it != _invited.end())
        _invited.erase(it);
}

// --- Envoi de messages ---

// Envoie un message à tous les membres du channel sauf l'expéditeur
void Channel::broadcastMessage(const std::string& message, Client* sender)
{
    for (size_t i = 0; i < _members.size(); ++i)
    {
        // Ne pas envoyer le message à l'expéditeur
        if (_members[i] != sender)
        {
            send(_members[i]->getFd(), message.c_str(), message.length(), 0);
        }
    }
}

// Envoie un message à TOUS les membres du channel (y compris l'expéditeur)
void Channel::broadcastMessageAll(const std::string& message)
{
    for (size_t i = 0; i < _members.size(); ++i)
    {
        // Envoyer le message via le socket du membre
        send(_members[i]->getFd(), message.c_str(), message.length(), 0);
    }
}
