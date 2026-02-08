# Utilise Ubuntu 22.04 comme base (similaire aux machines de l'école)
FROM ubuntu:22.04

# Met à jour le système et installe les outils nécessaires
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    make \
    netcat-openbsd \
    irssi \
    valgrind \
    gdb \
    vim \
    && rm -rf /var/lib/apt/lists/*

# Définit /app comme répertoire de travail
WORKDIR /app

# Lance bash au démarrage du container
CMD ["/bin/bash"]