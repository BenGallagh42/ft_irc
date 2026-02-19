#include "Server.hpp"
#include <iostream>  // Pour std::cout

// Traite une commande IRC reçue d'un client (routeur principal)
void Server::processCommand(Client& client, const std::string& command)
{
    std::cout << "[COMMAND] FD " << client.getFd() << ": " << command << std::endl;

    std::string cmd = extractCommand(command);
    std::string params = extractParams(command);

    // Les commandes PASS, NICK, USER sont toujours autorisées (avant enregistrement)
    if (cmd == "PASS")
        return handlePass(client, params);
    if (cmd == "NICK")
        return handleNick(client, params);
    if (cmd == "USER")
        return handleUser(client, params);
    if (cmd == "QUIT")
        return handleQuit(client, params);

    // Toutes les autres commandes nécessitent un enregistrement complet
    if (!client.isRegistered())
    {
        sendNumericReply(client, "451", ":You have not registered");
        return;
    }

    // Router les commandes qui nécessitent un enregistrement
    if (cmd == "JOIN")
        handleJoin(client, params);
    else if (cmd == "PART")
        handlePart(client, params);
    else if (cmd == "PRIVMSG")
        handlePrivmsg(client, params);
    else if (cmd == "KICK")
        handleKick(client, params);
    else if (cmd == "INVITE")
        handleInvite(client, params);
    else if (cmd == "TOPIC")
        handleTopic(client, params);
    else if (cmd == "MODE")
        handleMode(client, params);
    else
        sendNumericReply(client, "421", cmd + " :Unknown command");
}
