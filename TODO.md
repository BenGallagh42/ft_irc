# 🚀 ft_irc - Serveur IRC

Projet à deux : Infrastructure réseau + Protocole IRC

---

## ✅ Part 1 : Infrastructure Réseau (FAIT)

- Serveur socket TCP/IP
- Multiplexage I/O avec `poll()` (un seul appel)
- Mode non-bloquant sur tous les sockets
- Gestion multi-clients simultanés
- Buffer pour messages partiels
- Détection commandes complètes (`\r\n` ou `\n`)
- Classes `Server` et `Client`
- Gestion signaux (Ctrl+C)
- Makefile + Dockerfile
- Testé sur Mac et Linux

---

## 🚧 Part 2 : Protocole IRC (À FAIRE)

**Authentification :**
- `PASS <password>` - Vérifier mot de passe
- `NICK <nickname>` - Définir pseudo
- `USER <username> 0 * :realname` - Définir username

**Channels :**
- `JOIN #channel` - Rejoindre un salon
- `PART #channel` - Quitter un salon
- `PRIVMSG #channel :msg` - Message au salon
- `PRIVMSG user :msg` - Message privé

**Commandes opérateur :**
- `KICK #channel user` - Expulser
- `INVITE user #channel` - Inviter
- `TOPIC #channel :sujet` - Changer le sujet
- `MODE #channel +i/+t/+k/+o/+l` - Modes du channel

**Classe à créer :**
- `Channel` - Gestion des salons

---

## 📖 Manuel d'Utilisation

### Compilation
```bash
make
```

### Lancement
```bash
./ircserv <port> <password>

# Exemple :
./ircserv 6667 mypass
```

### Arrêt
```
Ctrl+C
```

---

## 🧪 Tests

### Test rapide (netcat)
```bash
# Terminal 1
./ircserv 6667 test

# Terminal 2
nc localhost 6667
HELLO
NICK alice
```

Tu devrais voir dans le serveur :
```
[RECEIVED] FD 4: HELLO
[COMMAND] FD 4: HELLO
```

### Test sur Docker (Linux)
```bash
./tests/test_docker.sh
```

### Test avec client IRC réel
```bash
# Installer irssi
brew install irssi        # Mac
sudo apt install irssi    # Linux/WSL

# Lancer
irssi
/connect localhost 6667 mypass
/nick alice
```

---

## 🔧 Point d'Intégration

### Fonction à modifier (Part 2)

**Fichier :** `srcs/Server.cpp`, ligne ~203

**Actuellement (Part 1) :**
```cpp
void Server::processCommand(Client& client, const std::string& command)
{
    std::cout << "[COMMAND] FD " << client.getFd() << ": " << command << std::endl;
    std::string response = "Server received: " + command + "\r\n";
    send(client.getFd(), response.c_str(), response.length(), 0);
}
```

**À transformer en (Part 2) :**
```cpp
void Server::processCommand(Client& client, const std::string& command)
{
    // Parser la commande
    std::string cmd = extractCommand(command);     // "NICK"
    std::string params = extractParams(command);   // "alice"
    
    // Router
    if (cmd == "PASS")
        handlePass(client, params);
    else if (cmd == "NICK")
        handleNick(client, params);
    else if (cmd == "USER")
        handleUser(client, params);
    else if (cmd == "JOIN")
        handleJoin(client, params);
    // ... etc
}
```

---

## 🎯 Fonctions Utiles (Part 1)

**Client :**
```cpp
client.getFd()                    // Socket du client
client.getNickname()              // Pseudo
client.setNickname("alice")       // Définir pseudo
client.setAuthenticated(true)     // Marquer authentifié
client.isAuthenticated()          // Vérifier auth
client.setRegistered(true)        // Marquer enregistré
client.isRegistered()             // Vérifier registration
```

**Serveur :**
```cpp
send(fd, msg.c_str(), msg.length(), 0)  // Envoyer au client
```

---

## 📚 Ressources

- **RFC 1459 :** https://tools.ietf.org/html/rfc1459
- **Codes IRC :** `001 RPL_WELCOME`, `401 ERR_NOSUCHNICK`, `433 ERR_NICKNAMEINUSE`...

---

## 🔄 Workflow

### Récupérer le code
```bash
git clone <url>
cd ft_irc
make
./ircserv 6667 test
```

### Développer Part 2
1. Implémenter parsing dans `processCommand()`
2. Créer classe `Channel`
3. Implémenter les commandes IRC
4. Tester régulièrement
5. Tester sur Docker avant de push

### Commit
```bash
git add .
git commit -m "feat: Add IRC command X"
git push
```

---

**🎯 Architecture :**
```
Part 1 = Réseau (socket, poll, buffer)
Part 2 = Logique IRC (commandes, channels, modes)
```

**🔗 Interface = `processCommand(Client& client, std::string command)`**

---

Bon courage ! 💪