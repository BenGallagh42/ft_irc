# Part 2 : Protocole IRC - Résumé pour Ben

## Ce qui a changé

### Fichiers créés
- `includes/Channel.hpp` - Classe Channel (salons IRC)
- `srcs/Channel.cpp` - Implémentation de Channel

### Fichiers modifiés
- `includes/Server.hpp` - Nouveaux membres et méthodes
- `srcs/Server.cpp` - Refacto + toutes les commandes IRC
- `Makefile` - Ajout de Channel.cpp

### Fichiers non touchés
- `Client.hpp` / `Client.cpp` - Aucun changement
- `main.cpp` - Aucun changement
- `utils.hpp` / `utils.cpp` - Aucun changement

---

## Changement architectural important

### Avant (Part 1)
```cpp
std::vector<Client> _clients;  // Clients stockés par copie
```

### Après (Part 2)
```cpp
std::map<int, Client*> _clients;            // fd -> Client* (allocation dynamique)
std::map<std::string, Channel*> _channels;  // nom -> Channel* (allocation dynamique)
std::string _serverName;                    // "ft_irc"
```

**Pourquoi ?** Avec un `vector<Client>`, quand le vector resize (push_back), tous les pointeurs vers les clients deviennent invalides. La classe Channel a besoin de stocker des `Client*` pour savoir qui est membre. Avec `map<int, Client*>`, les pointeurs restent stables.

**Conséquences :**
- `acceptNewClient()` fait `new Client(fd)` au lieu de créer sur la stack
- `disconnectClient()` fait `delete client` avant de retirer de la map
- `stop()` fait `delete` sur chaque client et chaque channel
- `readFromClient(int poll_index)` récupère le client via `_clients[_poll_fds[poll_index].fd]`

---

## Classe Channel

```cpp
class Channel {
private:
    std::string _name;              // Ex: "#general"
    std::string _topic;             // Sujet du channel
    std::string _key;               // Mot de passe (+k)
    std::vector<Client*> _members;  // Membres du channel
    std::vector<Client*> _operators;// Opérateurs (@)
    std::vector<Client*> _invited;  // Invités (pour mode +i)
    bool _inviteOnly;               // Mode +i
    bool _topicRestricted;          // Mode +t
    int _userLimit;                 // Mode +l (0 = pas de limite)
};
```

**Méthodes principales :**
- `addMember(Client*)` / `removeMember(Client*)` / `isMember(Client*)`
- `addOperator(Client*)` / `removeOperator(Client*)` / `isOperator(Client*)`
- `addInvited(Client*)` / `isInvited(Client*)` / `removeInvited(Client*)`
- `broadcastMessage(msg, sender)` - Envoie à tous SAUF sender
- `broadcastMessageAll(msg)` - Envoie à TOUS

---

## Commandes implémentées

### Authentification (avant enregistrement)

| Commande | Format | Ce que ça fait |
|----------|--------|----------------|
| `PASS` | `PASS <password>` | Vérifie le mot de passe du serveur. Doit être fait en premier. |
| `NICK` | `NICK <pseudo>` | Définit le pseudo. Valide le format (lettres, chiffres, `[]\_^{}|`-). Vérifie l'unicité. |
| `USER` | `USER <username> 0 * :<realname>` | Définit le username. Seul le premier mot est gardé. |

**Flow d'enregistrement :**
```
PASS mypass       → (rien, juste set authenticated)
NICK alice        → (rien, juste set nickname)
USER alice 0 * :Alice  → :ft_irc 001 alice :Welcome to the ft_irc Network, alice!alice@localhost
```
L'ordre NICK/USER n'a pas d'importance, mais PASS doit être fait avant les deux.

### Channels

| Commande | Format | Ce que ça fait |
|----------|--------|----------------|
| `JOIN` | `JOIN #channel [key]` | Rejoint un channel. Le crée si inexistant (créateur = op). Vérifie +i, +k, +l. |
| `PART` | `PART #channel [:message]` | Quitte un channel. Supprime le channel s'il est vide. |
| `PRIVMSG` | `PRIVMSG <target> :<message>` | Si target = `#channel` → message au channel. Si target = `pseudo` → message privé. |

**Réponses JOIN :**
```
:alice!alice@localhost JOIN #general          → à tous les membres
:ft_irc 332 alice #general :Le topic          → si topic existe
:ft_irc 353 alice = #general :@alice bob       → liste des membres (@ = op)
:ft_irc 366 alice #general :End of /NAMES list
```

### Commandes opérateur

| Commande | Format | Ce que ça fait |
|----------|--------|----------------|
| `KICK` | `KICK #channel <user> [:<raison>]` | Expulse un user. Faut être op. |
| `INVITE` | `INVITE <pseudo> #channel` | Invite un user. Si +i, faut être op. |
| `TOPIC` | `TOPIC #channel [:<sujet>]` | Sans sujet = affiche. Avec sujet = change. Si +t, faut être op pour changer. |
| `MODE` | `MODE #channel [+/-flags] [params]` | Change les modes du channel. |

### Modes du channel

| Mode | Param (+) | Param (-) | Description |
|------|-----------|-----------|-------------|
| `+i` | aucun | aucun | Invite-only : seuls les invités peuvent JOIN |
| `+t` | aucun | aucun | Topic restreint : seuls les ops peuvent changer le topic |
| `+k` | clé | aucun | Mot de passe : il faut la clé pour JOIN |
| `+o` | pseudo | pseudo | Donne/retire le statut opérateur |
| `+l` | nombre | aucun | Limite le nombre de membres |

**Exemples :**
```
MODE #general +it              → active invite-only et topic restreint
MODE #general +k secret        → met un mot de passe "secret"
MODE #general +o bob            → bob devient opérateur
MODE #general +l 10             → max 10 membres
MODE #general -ik               → retire invite-only et le mot de passe
MODE #general                   → affiche les modes actuels
```

### Déconnexion

| Commande | Format | Ce que ça fait |
|----------|--------|----------------|
| `QUIT` | `QUIT [:message]` | Déconnexion propre. Notifie tous les channels du départ. |

Si un client ferme la connexion sans QUIT (Ctrl+C, crash), le serveur détecte la déconnexion et fait le nettoyage automatiquement (retire de tous les channels, notifie les membres).

---

## Fonctions utilitaires ajoutées dans Server

```cpp
// Envoie un message à un client (ajoute \r\n automatiquement)
void sendToClient(Client& client, const std::string& message);

// Envoie une réponse numérique : ":ft_irc CODE nick message\r\n"
void sendNumericReply(Client& client, const std::string& code, const std::string& message);

// Retourne ":nick!user@localhost"
std::string getClientPrefix(Client& client);

// Extrait "NICK" de "NICK alice"
std::string extractCommand(const std::string& message);

// Extrait "alice" de "NICK alice"
std::string extractParams(const std::string& message);

// Cherche un client par pseudo dans la map
Client* findClientByNickname(const std::string& nickname);

// Retire un client de tous les channels (déconnexion)
void removeClientFromAllChannels(Client* client);
```

---

## Codes numériques IRC utilisés

| Code | Nom | Quand |
|------|-----|-------|
| 001 | RPL_WELCOME | Enregistrement réussi |
| 324 | RPL_CHANNELMODEIS | Affichage des modes d'un channel |
| 331 | RPL_NOTOPIC | Pas de topic défini |
| 332 | RPL_TOPIC | Affichage du topic |
| 341 | RPL_INVITING | Confirmation d'invitation |
| 353 | RPL_NAMREPLY | Liste des membres d'un channel |
| 366 | RPL_ENDOFNAMES | Fin de la liste des membres |
| 401 | ERR_NOSUCHNICK | Pseudo/channel inexistant |
| 403 | ERR_NOSUCHCHANNEL | Channel inexistant |
| 411 | ERR_NORECIPIENT | PRIVMSG sans destinataire |
| 412 | ERR_NOTEXTTOSEND | PRIVMSG sans message |
| 421 | ERR_UNKNOWNCOMMAND | Commande inconnue |
| 431 | ERR_NONICKNAMEGIVEN | NICK sans pseudo |
| 432 | ERR_ERRONEUSNICKNAME | Pseudo invalide |
| 433 | ERR_NICKNAMEINUSE | Pseudo déjà pris |
| 441 | ERR_USERNOTINCHANNEL | User pas dans le channel |
| 442 | ERR_NOTONCHANNEL | Tu n'es pas dans ce channel |
| 443 | ERR_USERONCHANNEL | User déjà dans le channel |
| 451 | ERR_NOTREGISTERED | Pas encore enregistré |
| 461 | ERR_NEEDMOREPARAMS | Paramètres manquants |
| 462 | ERR_ALREADYREGISTERED | Déjà enregistré |
| 464 | ERR_PASSWDMISMATCH | Mauvais mot de passe |
| 471 | ERR_CHANNELISFULL | Channel plein (+l) |
| 472 | ERR_UNKNOWNMODE | Mode inconnu |
| 473 | ERR_INVITEONLYCHAN | Channel invite-only (+i) |
| 475 | ERR_BADCHANNELKEY | Mauvaise clé (+k) |
| 476 | ERR_BADCHANMASK | Nom de channel invalide |
| 482 | ERR_CHANOPRIVSNEEDED | Pas opérateur |

---

## Comment tester

```bash
# Compiler
make re

# Lancer le serveur
./ircserv 6667 mypass

# Terminal 2 : test avec netcat
nc localhost 6667
PASS mypass
NICK alice
USER alice 0 * :Alice
JOIN #general
PRIVMSG #general :Hello!
TOPIC #general :Bienvenue
MODE #general +it
QUIT :Bye

# Terminal 3 : test avec un vrai client IRC
irssi
/connect localhost 6667 mypass
/nick bob
/join #general
/msg #general Hello from irssi
```

---

## Structure du routeur processCommand

```
processCommand(client, command)
  │
  ├── extractCommand() + extractParams()
  │
  ├── PASS → handlePass()      (toujours autorisé)
  ├── NICK → handleNick()      (toujours autorisé)
  ├── USER → handleUser()      (toujours autorisé)
  ├── QUIT → handleQuit()      (toujours autorisé)
  │
  ├── [vérifie isRegistered() → 451 si non]
  │
  ├── JOIN    → handleJoin()
  ├── PART    → handlePart()
  ├── PRIVMSG → handlePrivmsg()
  ├── KICK    → handleKick()
  ├── INVITE  → handleInvite()
  ├── TOPIC   → handleTopic()
  ├── MODE    → handleMode()
  │
  └── sinon   → 421 Unknown command
```
