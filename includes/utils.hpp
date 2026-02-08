/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: your_login <your_login@student.42.fr>    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/08 by your_login           #+#    #+#             */
/*   Updated: 2025/02/08 by your_login          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

// Met un file descriptor en mode non-bloquant pour éviter que recv/send bloquent
bool set_nonblocking(int fd);

// Affiche un message d'erreur et quitte le programme
void error_exit(const std::string& message);

#endif