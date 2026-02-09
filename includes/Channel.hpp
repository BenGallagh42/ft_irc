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

    // --- Getters (accesseurs) ---
    std::string getName() const;
    std::string getTopic() const;
    std::string getKey() const;
    bool isInviteOnly() const;
    bool isTopicRestricted() const;
    int getUserLimit() const;
    std::vector<Client*>& getMembers();

    // --- Setters (mutateurs) ---
    void setTopic(const std::string& topic);
    void setKey(const std::string& key);
    void setInviteOnly(bool inviteOnly);
    void setTopicRestricted(bool restricted);
    void setUserLimit(int limit);

    // --- Gestion des membres ---
    // Ajoute un client au channel
    void addMember(Client* client);
    // Retire un client du channel
    void removeMember(Client* client);
    // Vérifie si un client est membre du channel
    bool isMember(Client* client) const;

    // --- Gestion des opérateurs ---
    // Ajoute un client comme opérateur du channel
    void addOperator(Client* client);
    // Retire un client de la liste des opérateurs
    void removeOperator(Client* client);
    // Vérifie si un client est opérateur du channel
    bool isOperator(Client* client) const;

    // --- Gestion des invitations ---
    // Ajoute un client à la liste des invités
    void addInvited(Client* client);
    // Vérifie si un client est invité au channel
    bool isInvited(Client* client) const;
    // Retire un client de la liste des invités
    void removeInvited(Client* client);

    // --- Envoi de messages ---
    // Envoie un message à tous les membres du channel (sauf l'expéditeur)
    void broadcastMessage(const std::string& message, Client* sender);
    // Envoie un message à TOUS les membres (y compris l'expéditeur)
    void broadcastMessageAll(const std::string& message);
};

#endif
