*This project has been created as part of the 42 curriculum by bboulmie, torsini.*

# ft_irc

## Description

ft_irc is an implementation of an IRC (Internet Relay Chat) server written in C++98. The project aims to recreate the core functionality of an IRC server, allowing multiple clients to connect simultaneously, join channels, exchange messages, and use channel operator commands.

The server handles all connections using a single `poll()` call for non-blocking I/O operations, ensuring efficient management of multiple clients without forking. The first user to join a channel becomes its operator and can manage the channel using specific commands.

This project provides hands-on experience with network programming, socket management, and the IRC protocol implementation.

## Instructions

### Compilation

To compile the project, run:
```bash
make
```

This will generate the `ircserv` executable.

### Execution

To start the server:
```bash
./ircserv <port> <password>
```

**Parameters:**
- `<port>`: The port number on which your IRC server will be listening to for incoming IRC connections
- `<password>`: The connection password. It will be needed by any IRC client that tries to connect to your server

**Example:**
```bash
./ircserv 6667 mypassword
```

### Testing

You can test the server using any IRC client (such as irssi) or netcat for basic testing.

**Using netcat:**
```bash
nc localhost 6667
PASS mypassword
NICK alice
USER alice 0 * :Alice
JOIN #test
```

**Using irssi:**
```bash
irssi
/connect localhost 6667 mypassword
/nick alice
/join #test
```

## Features

### Authentication
- Password verification (`PASS`)
- Nickname registration (`NICK`)
- Username registration (`USER`)

### Channel Operations
- Join channels (`JOIN`)
- Leave channels (`PART`)
- Send messages to channels (`PRIVMSG`)
- Send private messages to users

### Channel Operator Commands
- `KICK` - Eject a client from the channel
- `INVITE` - Invite a client to a channel
- `TOPIC` - Change or view the channel topic
- `MODE` - Change the channel's mode:
  - `i`: Set/remove Invite-only channel
  - `t`: Set/remove the restrictions of the TOPIC command to channel operators
  - `k`: Set/remove the channel key (password)
  - `o`: Give/take channel operator privilege
  - `l`: Set/remove the user limit to channel

## Resources

**Documentation:**
- [RFC 1459 - Internet Relay Chat Protocol](https://datatracker.ietf.org/doc/html/rfc1459)
- [RFC 2812 - Internet Relay Chat: Client Protocol](https://datatracker.ietf.org/doc/html/rfc2812)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [poll() man page](https://man7.org/linux/man-pages/man2/poll.2.html)

**AI Usage:**

AI was used throughout this project for the following tasks:

- **Documentation**: Clarifying C++98 standards, socket programming concepts, and IRC protocol details.
- **Protocol understanding**: Help understanding IRC protocol specifications (authentication flow, message formats, numeric replies).
- **Debugging assistance**: Identifying and fixing bugs such as use-after-free errors, memory leaks, and edge cases in command parsing.
- **Testing strategy**: Creating comprehensive test scripts to validate all functionality and edge cases.