# Nom de l'exécutable
NAME = ircserv

# Compilateur et flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I./includes

# Répertoires
SRC_DIR = srcs
OBJ_DIR = objs
INC_DIR = includes

# Fichiers sources
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

# Fichiers objets (remplace srcs/ par objs/ et .cpp par .o)
OBJS = $(SRCS:$(SRC_DIR)/%=$(OBJ_DIR)/%)
OBJS := $(OBJS:.cpp=.o)

# Couleurs pour l'affichage
GREEN = \033[0;32m
RED = \033[0;31m
YELLOW = \033[0;33m
RESET = \033[0m

# Règle par défaut : compile tout
all: $(NAME)

# Crée les répertoires objs et objs/commands s'ils n'existent pas
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(OBJ_DIR)/commands
	@echo "$(YELLOW)Created objs directories$(RESET)"

# Compile un fichier .cpp en .o (gère les sous-dossiers)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	@echo "$(GREEN)Compiling $<...$(RESET)"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Crée l'exécutable à partir des .o
$(NAME): $(OBJS)
	@echo "$(GREEN)Linking $(NAME)...$(RESET)"
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "$(GREEN)✓ $(NAME) created successfully!$(RESET)"

# Supprime les fichiers objets
clean:
	@echo "$(RED)Cleaning object files...$(RESET)"
	@rm -rf $(OBJ_DIR)

# Supprime les fichiers objets et l'exécutable
fclean: clean
	@echo "$(RED)Removing $(NAME)...$(RESET)"
	@rm -f $(NAME)

# Recompile tout de zéro
re: fclean all

# Indique que ces règles ne créent pas de fichiers
.PHONY: all clean fclean re
