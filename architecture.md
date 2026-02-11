# Architecture ft_irc - Organisation granulaire

## Structure complète

```
ft_irc/
├── includes/
│   ├── Server.hpp         - Déclarations serveur
│   ├── Client.hpp         - Classe Client
│   ├── Channel.hpp        - Classe Channel
│   └── utils.hpp          - Utils système
│
├── srcs/
│   ├── main.cpp                    (~65 lignes)  - Point d'entrée
│   │
│   ├── Server.cpp                  (~400 lignes) - Core réseau
│   ├── ServerUtils.cpp             (~100 lignes) - Utils serveur
│   │
│   ├── commands/
│   │   ├── AuthCommands.cpp        (~200 lignes) - PASS, NICK, USER
│   │   ├── ChannelCommands.cpp     (~250 lignes) - JOIN, PART
│   │   ├── MessageCommands.cpp     (~100 lignes) - PRIVMSG
│   │   ├── OperatorCommands.cpp    (~400 lignes) - KICK, INVITE, TOPIC, MODE
│   │   └── CommandRouter.cpp       (~50 lignes)  - processCommand()
│   │
│   ├── Client.cpp                  (~90 lignes)  - Implémentation Client
│   ├── Channel.cpp                 (~150 lignes) - Implémentation Channel
│   └── utils.cpp                   (~35 lignes)  - Utils système
│
└── Makefile
```

---

## Détail des fichiers

### **main.cpp** (65 lignes)
**Rôle :** Point d'entrée

- Parse les arguments (port, password)
- Installe les signal handlers (Ctrl+C)
- Crée et lance le serveur
- Catch les exceptions

---

### **Server.cpp** (~400 lignes)
**Rôle :** Core réseau (sockets, poll, connexions)

**Fonctions :**
```cpp
Server::Server(int port, const string& password)  // Constructeur
Server::~Server()                                  // Destructeur

void setupServer()          // Crée socket, bind, listen, poll
void acceptNewClient()      // Accept nouvelle connexion
void readFromClient(int)    // Recv et parse buffer
void disconnectClient(int)  // Ferme connexion + cleanup
void run()                  // Boucle poll() infinie
void stop()                 // Arrêt propre
```

**Dépendances :** `sys/socket.h`, `poll.h`, `Client.hpp`

**Responsabilité :** Tout ce qui touche aux sockets, `poll()`, et aux connexions TCP.

---

### **ServerUtils.cpp** (~100 lignes)
**Rôle :** Fonctions utilitaires partagées

**Fonctions :**
```cpp
// Envoi
void sendToClient(Client&, const string&)           // Envoie msg + \r\n
void sendNumericReply(Client&, code, msg)           // Envoie :server CODE nick msg
string getClientPrefix(Client&)                     // Retourne :nick!user@host

// Parsing
string extractCommand(const string&)                // "NICK" de "NICK alice"
string extractParams(const string&)                 // "alice" de "NICK alice"

// Recherche
Client* findClientByNickname(const string&)         // Cherche dans _clients map

// Cleanup
void removeClientFromAllChannels(Client*)           // Retire de tous les channels
```

**Dépendances :** `Server.hpp`, `Client.hpp`, `Channel.hpp`

**Responsabilité :** Helpers réutilisables pour simplifier les autres fichiers.

---

### **commands/CommandRouter.cpp** (~50 lignes)
**Rôle :** Router principal des commandes

**Fonction :**
```cpp
void processCommand(Client& client, const string& command)
{
    string cmd = extractCommand(command);
    string params = extractParams(command);

    // Commandes pre-registration
    if (cmd == "PASS") return handlePass(client, params);
    if (cmd == "NICK") return handleNick(client, params);
    if (cmd == "USER") return handleUser(client, params);
    if (cmd == "QUIT") return handleQuit(client, params);

    // Vérifie enregistrement
    if (!client.isRegistered())
        return sendNumericReply(client, "451", ":Not registered");

    // Commandes post-registration
    if (cmd == "JOIN")    return handleJoin(client, params);
    if (cmd == "PART")    return handlePart(client, params);
    if (cmd == "PRIVMSG") return handlePrivmsg(client, params);
    if (cmd == "KICK")    return handleKick(client, params);
    if (cmd == "INVITE")  return handleInvite(client, params);
    if (cmd == "TOPIC")   return handleTopic(client, params);
    if (cmd == "MODE")    return handleMode(client, params);

    // Commande inconnue
    sendNumericReply(client, "421", cmd + " :Unknown command");
}
```

**Responsabilité :** Parse et dispatch vers le bon handler.

---

### **commands/AuthCommands.cpp** (~200 lignes)
**Rôle :** Authentification et enregistrement

**Fonctions :**
```cpp
void handlePass(Client&, params)    // Vérifie password serveur
void handleNick(Client&, params)    // Set pseudo (valide format + unicité)
void handleUser(Client&, params)    // Set username
void checkRegistration(Client&)     // Vérifie PASS+NICK+USER → envoie 001
void handleQuit(Client&, params)    // Déconnexion propre
```

**Codes numériques :**
- `001 RPL_WELCOME` - Bienvenue après enregistrement
- `431 ERR_NONICKNAMEGIVEN` - Pas de pseudo fourni
- `432 ERR_ERRONEUSNICKNAME` - Pseudo invalide
- `433 ERR_NICKNAMEINUSE` - Pseudo déjà pris
- `461 ERR_NEEDMOREPARAMS` - Paramètres manquants
- `462 ERR_ALREADYREGISTERED` - Déjà enregistré
- `464 ERR_PASSWDMISMATCH` - Mauvais mot de passe

**Responsabilité :** Tout ce qui concerne l'authentification et l'enregistrement initial.

---

### **commands/ChannelCommands.cpp** (~250 lignes)
**Rôle :** Gestion des channels (rejoindre/quitter)

**Fonctions :**
```cpp
void handleJoin(Client&, params)    // Rejoint/crée channel (vérifie +i/+k/+l)
void handlePart(Client&, params)    // Quitte channel (supprime si vide)
```

**handleJoin détails :**
- Parse `#channel [key]`
- Vérifie format (`#` obligatoire)
- Si channel existe :
  - Vérifie mode `+i` (invite-only) → 473 si pas invité
  - Vérifie mode `+k` (key) → 475 si mauvaise clé
  - Vérifie mode `+l` (limit) → 471 si plein
- Si channel n'existe pas :
  - Crée le channel
  - Créateur = opérateur auto
- Ajoute membre
- Broadcast JOIN à tous
- Envoie 353 (NAMES) + 366 (END OF NAMES)

**handlePart détails :**
- Vérifie channel existe → 403 sinon
- Vérifie membership → 442 sinon
- Broadcast PART à tous
- Retire membre
- Si channel vide → delete

**Codes numériques :**
- `353 RPL_NAMREPLY` - Liste membres (@op member)
- `366 RPL_ENDOFNAMES` - Fin liste
- `403 ERR_NOSUCHCHANNEL` - Channel inexistant
- `442 ERR_NOTONCHANNEL` - Pas dans le channel
- `471 ERR_CHANNELISFULL` - Channel plein (+l)
- `473 ERR_INVITEONLYCHAN` - Invite-only (+i)
- `475 ERR_BADCHANNELKEY` - Mauvaise clé (+k)
- `476 ERR_BADCHANMASK` - Nom invalide

**Responsabilité :** JOIN et PART uniquement.

---

### **commands/MessageCommands.cpp** (~100 lignes)
**Rôle :** Envoi de messages (channel et privé)

**Fonction :**
```cpp
void handlePrivmsg(Client&, params)    // Envoie message channel ou user
```

**Logique :**
```
Parse "target :message"

Si target commence par # → message channel
  - Vérifie channel existe → 403 sinon
  - Vérifie membership → 442 sinon
  - Broadcast à tous SAUF sender

Sinon → message privé
  - Cherche user par pseudo
  - Vérifie existe → 401 sinon
  - Envoie direct à la cible
```

**Codes numériques :**
- `401 ERR_NOSUCHNICK` - User/channel inexistant
- `411 ERR_NORECIPIENT` - Pas de destinataire
- `412 ERR_NOTEXTTOSEND` - Pas de message
- `442 ERR_NOTONCHANNEL` - Pas dans le channel

**Responsabilité :** PRIVMSG uniquement (channel + privé).

---

### **commands/OperatorCommands.cpp** (~400 lignes)
**Rôle :** Commandes opérateur (KICK, INVITE, TOPIC, MODE)

**Fonctions :**
```cpp
void handleKick(Client&, params)     // Expulse un user
void handleInvite(Client&, params)   // Invite un user
void handleTopic(Client&, params)    // Affiche/change sujet
void handleMode(Client&, params)     // Change modes channel
```

**handleKick :**
- Parse `#channel user [:raison]`
- Vérifie op → 482 sinon
- Vérifie cible existe et dans channel → 401/441
- Broadcast KICK à tous
- Retire cible du channel

**handleInvite :**
- Parse `user #channel`
- Vérifie channel existe → 403
- Vérifie inviteur membre → 442
- Si +i actif : vérifie op → 482
- Vérifie cible pas déjà membre → 443
- Ajoute à liste invités
- Envoie 341 à inviteur + notification à cible

**handleTopic :**
- Parse `#channel [:<nouveau>]`
- Sans nouveau → affiche (332 ou 331)
- Avec nouveau :
  - Si +t actif : vérifie op → 482
  - Change topic
  - Broadcast à tous

**handleMode :**
- Parse `#channel [+/-flags] [params]`
- Sans flags → affiche modes actuels (324)
- Avec flags :
  - Vérifie op → 482
  - Parse caractère par caractère
  - `+i/-i` : invite-only
  - `+t/-t` : topic restricted
  - `+k/-k` : channel key (nécessite param si +)
  - `+o/-o` : opérateur (nécessite pseudo)
  - `+l/-l` : user limit (nécessite nombre si +)
  - Broadcast MODE à tous

**Codes numériques :**
- `324 RPL_CHANNELMODEIS` - Modes actuels
- `331 RPL_NOTOPIC` - Pas de topic
- `332 RPL_TOPIC` - Topic actuel
- `341 RPL_INVITING` - Confirmation invitation
- `401 ERR_NOSUCHNICK` - User inexistant
- `403 ERR_NOSUCHCHANNEL` - Channel inexistant
- `441 ERR_USERNOTINCHANNEL` - User pas dans channel
- `442 ERR_NOTONCHANNEL` - Pas dans channel
- `443 ERR_USERONCHANNEL` - User déjà dans channel
- `461 ERR_NEEDMOREPARAMS` - Params manquants
- `472 ERR_UNKNOWNMODE` - Mode inconnu
- `482 ERR_CHANOPRIVSNEEDED` - Pas opérateur

**Responsabilité :** Toutes les commandes réservées aux opérateurs.

---

### **Client.cpp** (~90 lignes)
**Rôle :** Représente un client connecté

**Membres :**
```cpp
int _fd                  // Socket fd
string _nickname         // Pseudo
string _username         // Username
string _buffer           // Buffer réception
bool _authenticated      // PASS ok ?
bool _registered         // PASS+NICK+USER ok ?
```

**Méthodes :** Getters/setters simples.

---

### **Channel.cpp** (~150 lignes)
**Rôle :** Représente un salon IRC

**Membres :**
```cpp
string _name                    // Ex: "#general"
string _topic                   // Sujet
string _key                     // Mot de passe (+k)
vector<Client*> _members        // Tous les membres
vector<Client*> _operators      // Opérateurs (@)
vector<Client*> _invited        // Liste invités (+i)
bool _inviteOnly                // Mode +i
bool _topicRestricted           // Mode +t
int _userLimit                  // Mode +l (0 = pas de limite)
```

**Méthodes :**
- Membres : `addMember()`, `removeMember()`, `isMember()`
- Ops : `addOperator()`, `removeOperator()`, `isOperator()`
- Invités : `addInvited()`, `isInvited()`, `removeInvited()`
- Broadcast : `broadcastMessage()`, `broadcastMessageAll()`
- Modes : setters pour tous les modes

---

### **utils.cpp** (~35 lignes)
**Rôle :** Utils système

```cpp
bool set_nonblocking(int fd)              // fcntl(O_NONBLOCK)
void error_exit(const string& message)    // Affiche erreur + exit(1)
```

---

## Makefile

```makefile
SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/Server.cpp \
       $(SRC_DIR)/ServerUtils.cpp \
       $(SRC_DIR)/commands/CommandRouter.cpp \
       $(SRC_DIR)/commands/AuthCommands.cpp \
       $(SRC_DIR)/commands/ChannelCommands.cpp \
       $(SRC_DIR)/commands/MessageCommands.cpp \
       $(SRC_DIR)/commands/OperatorCommands.cpp \
       $(SRC_DIR)/Client.cpp \
       $(SRC_DIR)/Channel.cpp \
       $(SRC_DIR)/utils.cpp
```

---

## Avantages de cette architecture

### ✅ Ultra-lisible
- **Fichiers courts** (50-400 lignes)
- **Un fichier = une catégorie** (Auth, Channel, Message, Operator)
- **Facile à trouver** : Bug dans KICK ? → `OperatorCommands.cpp`

### ✅ Facile à expliquer pendant l'éval

**Question :** "Comment est organisé le code ?"

**Réponse :**
> On a séparé en couches :
> 1. **Server.cpp** - Le réseau (sockets, poll)
> 2. **ServerUtils.cpp** - Les helpers (envoi, parsing)
> 3. **commands/** - Les commandes IRC, une catégorie par fichier :
>    - `AuthCommands.cpp` - PASS, NICK, USER (authentification)
>    - `ChannelCommands.cpp` - JOIN, PART (rejoindre/quitter)
>    - `MessageCommands.cpp` - PRIVMSG (messages)
>    - `OperatorCommands.cpp` - KICK, INVITE, TOPIC, MODE (admin)
>    - `CommandRouter.cpp` - Le routeur qui dispatch
> 4. **Client** et **Channel** - Les données
>
> Chaque fichier a une seule responsabilité, c'est facile à naviguer.

### ✅ Maintenance simple
- Ajouter une commande ? → Modifier le bon fichier
- Bug dans JOIN ? → `ChannelCommands.cpp` ligne XX
- Bug réseau ? → `Server.cpp`

### ✅ Pas sur-ingéniéré
- Pas de classes abstraites inutiles
- Pas d'héritage compliqué
- Juste une séparation logique par responsabilité

---

## Flow d'exécution

### Réception d'une commande
```
Client envoie "JOIN #general"
    ↓
Server::readFromClient()
    ├── recv() → buffer
    ├── Parse buffer → extrait "JOIN #general"
    └── appelle processCommand(client, "JOIN #general")
            ↓
        CommandRouter::processCommand()
            ├── extractCommand() → "JOIN"
            ├── extractParams() → "#general"
            ├── Vérifie isRegistered()
            └── dispatch → handleJoin(client, "#general")
                    ↓
                ChannelCommands::handleJoin()
                    ├── Vérifie format, modes (+i/+k/+l)
                    ├── Crée/rejoint channel
                    ├── channel->addMember()
                    ├── channel->broadcastMessageAll(JOIN)
                    ├── sendNumericReply(353) - NAMES
                    └── sendNumericReply(366) - END OF NAMES
```

---

## Résumé pour Ben

**Structure finale :**
```
11 fichiers sources
├── 1 main
├── 2 core (Server + Utils)
├── 5 commands (Router + Auth + Channel + Message + Operator)
└── 3 data (Client + Channel + utils)

Tous les fichiers < 400 lignes
Séparation claire par responsabilité
Facile à expliquer en 30 secondes
```

**Pendant l'évaluation :**
> "Notre code est organisé par responsabilité : Server.cpp gère le réseau, puis on a un dossier commands/ avec un fichier par catégorie de commandes IRC. AuthCommands pour l'authentification, ChannelCommands pour JOIN/PART, etc. Comme ça, si le correcteur cherche comment fonctionne KICK, il va direct dans OperatorCommands.cpp."
