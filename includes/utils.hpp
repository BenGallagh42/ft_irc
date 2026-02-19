#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

// Met un file descriptor en mode non-bloquant pour éviter que recv/send bloquent
bool set_nonblocking(int fd);

// Affiche un message d'erreur et quitte le programme
void error_exit(const std::string& message);

#endif