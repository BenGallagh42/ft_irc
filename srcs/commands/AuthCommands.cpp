#include "Server.hpp"
#include <iostream>  // Pour std::cout
#include <cctype>    // Pour std::isalpha(), std::isalnum()

// Gère la commande PASS : vérifie le mot de passe du serveur
void Server::handlePass(Client& client, const std::string& params)
{
    if (client.isRegistered())
    {
        sendNumericReply(client, "462", ":You may not reregister");
        return;
    }

    if (params.empty())
    {
        sendNumericReply(client, "461", "PASS :Not enough parameters");
        return;
    }

    if (params == _password)
    {
        client.setAuthenticated(true);
        std::cout << "[AUTH] FD " << client.getFd() << ": Password accepted" << std::endl;
    }
    else
    {
        sendNumericReply(client, "464", ":Password incorrect");
    }
}

// Gère la commande NICK : définir ou changer le pseudo
void Server::handleNick(Client& client, const std::string& params)
{
    if (params.empty())
    {
        sendNumericReply(client, "431", ":No nickname given");
        return;
    }

    if (!client.isAuthenticated())
    {
        sendNumericReply(client, "464", ":Password incorrect - use PASS first");
        return;
    }

    std::string nickname = params;
    size_t space = nickname.find(' ');
    if (space != std::string::npos)
        nickname = nickname.substr(0, space);

    if (!std::isalpha(nickname[0]) && nickname[0] != '[' && nickname[0] != ']'
        && nickname[0] != '\\' && nickname[0] != '^' && nickname[0] != '_'
        && nickname[0] != '{' && nickname[0] != '}' && nickname[0] != '|')
    {
        sendNumericReply(client, "432", nickname + " :Erroneous nickname");
        return;
    }

    for (size_t i = 1; i < nickname.size(); ++i)
    {
        char c = nickname[i];
        if (!std::isalnum(c) && c != '[' && c != ']' && c != '\\'
            && c != '^' && c != '_' && c != '{' && c != '}' && c != '|' && c != '-')
        {
            sendNumericReply(client, "432", nickname + " :Erroneous nickname");
            return;
        }
    }

    Client* existing = findClientByNickname(nickname);
    if (existing && existing != &client)
    {
        sendNumericReply(client, "433", nickname + " :Nickname is already in use");
        return;
    }

    std::string oldNick = client.getNickname();

    client.setNickname(nickname);
    std::cout << "[NICK] FD " << client.getFd() << ": " << nickname << std::endl;

    if (client.isRegistered() && !oldNick.empty())
    {
        std::string nickMsg = ":" + oldNick + "!" + client.getUsername() + "@localhost NICK :" + nickname + "\r\n";
        sendToClient(client, nickMsg);
        
        for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
        {
            if (it->second->isMember(&client))
                it->second->broadcastMessage(nickMsg, &client);
        }
    }

    checkRegistration(client);
}

// Gère la commande USER : définir le nom d'utilisateur
void Server::handleUser(Client& client, const std::string& params)
{
    if (client.isRegistered())
    {
        sendNumericReply(client, "462", ":You may not reregister");
        return;
    }

    if (params.empty())
    {
        sendNumericReply(client, "461", "USER :Not enough parameters");
        return;
    }

    if (!client.isAuthenticated())
    {
        sendNumericReply(client, "464", ":Password incorrect - use PASS first");
        return;
    }

    std::string username = params;
    size_t space = username.find(' ');
    if (space != std::string::npos)
        username = username.substr(0, space);

    client.setUsername(username);
    std::cout << "[USER] FD " << client.getFd() << ": " << username << std::endl;

    checkRegistration(client);
}

// Vérifie si PASS + NICK + USER sont complétés et envoie RPL_WELCOME
void Server::checkRegistration(Client& client)
{
    if (client.isRegistered())
        return;

    if (client.isAuthenticated() && !client.getNickname().empty() && !client.getUsername().empty())
    {
        client.setRegistered(true);

        std::string welcome = ":Welcome to the " + _serverName + " Network, "
            + client.getNickname() + "!" + client.getUsername() + "@localhost";
        sendNumericReply(client, "001", welcome);

        std::cout << "[REGISTERED] " << client.getNickname() << " is now registered" << std::endl;
    }
}

// Gère la commande QUIT : déconnexion volontaire du client
void Server::handleQuit(Client& client, const std::string& params)
{
    std::string message = "Quit";
    if (!params.empty())
    {
        message = params;
        if (message[0] == ':')
            message = message.substr(1);
    }

    std::string quitMsg = getClientPrefix(client) + " QUIT :" + message + "\r\n";

    for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
    {
        if (it->second->isMember(&client))
            it->second->broadcastMessage(quitMsg, &client);
    }

    std::cout << "[QUIT] " << client.getNickname() << ": " << message << std::endl;

    for (size_t i = 1; i < _poll_fds.size(); ++i)
    {
        if (_poll_fds[i].fd == client.getFd())
        {
            disconnectClient(i);
            return;
        }
    }
}