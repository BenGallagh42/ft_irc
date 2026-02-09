# **************************************************************************** #
#                                   VARIABLES                                  #
# **************************************************************************** #

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
       $(SRC_DIR)/Client.cpp \
       $(SRC_DIR)/Channel.cpp \
       $(SRC_DIR)/utils.cpp

# Fichiers objets (conversion .cpp -> .o)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Couleurs pour l'affichage
GREEN = \033[0;32m
RED = \033[0;31m
YELLOW = \033[0;33m
RESET = \033[0m

# **************************************************************************** #
#                                   RÈGLES                                     #
# **************************************************************************** #

# Règle par défaut : compile tout
all: $(NAME)

# Crée le répertoire objs s'il n'existe pas
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)
	@echo "$(YELLOW)Created objs directory$(RESET)"

# Compile un fichier .cpp en .o
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