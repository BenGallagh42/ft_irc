#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <vector>

class Client; // Déclaration anticipée pour éviter les inclusions circulaires

// Représente un salon IRC avec ses membres, opérateurs et modes
class Channel {
private:
    std::string _name;                      // Nom du channel (ex: #general)
    std::string _topic;                     // Sujet du channel
    std::string _key;                       // Mot de passe du channel (mode +k)
    std::vector<Client*> _members;          // Liste des membres du channel
    std::vector<Client*> _operators;        // Liste des opérateurs du channel
    std::vector<Client*> _invited;          // Liste des clients invités (mode +i)
    bool _inviteOnly;                       // Mode +i : invitation seulement
    bool _topicRestricted;                  // Mode +t : seuls les ops changent le topic
    int _userLimit;                         // Mode +l : limite de membres (0 = pas de limite)

public:
    // Constructeur : crée un channel avec son nom
    Channel(const std::string& name);

    // Destructeur
    ~Channel();

    // Getters
    std::string getName() const;
    std::string getTopic() const;
    std::string getKey() const;
    bool isInviteOnly() const;
    bool isTopicRestricted() const;
    int getUserLimit() const;
    std::vector<Client*>& getMembers();

    // Setters
    void setTopic(const std::string& topic);
    void setKey(const std::string& key);
    void setInviteOnly(bool inviteOnly);
    void setTopicRestricted(bool restricted);
    void setUserLimit(int limit);

    // Gestion des membres
    void addMember(Client* client);
    void removeMember(Client* client);
    bool isMember(Client* client) const;

    // Gestion des opérateurs
    void addOperator(Client* client);
    void removeOperator(Client* client);
    bool isOperator(Client* client) const;

    // Gestion des invitations
    void addInvited(Client* client);
    bool isInvited(Client* client) const;
    void removeInvited(Client* client);

    // Envoi de messages
    void broadcastMessage(const std::string& message, Client* sender);
    void broadcastMessageAll(const std::string& message);
};

#endif
