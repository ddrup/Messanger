#pragma once

#include <boost/algorithm/string.hpp>
#include <functional>
#include <iostream>
#include <sodium.h>
#include <sodium/core.h>
#include <string>
#include <vector>

#include "database.h"

inline std::string hash_password(const std::string &pass) {
  if (sodium_init() < 0) {
    std::cerr << "sodium_init failed\n";
    std::hash<std::string> h;
    return std::to_string(h(pass));
  }

  char hash[crypto_pwhash_STRBYTES];
  if (crypto_pwhash_str(hash, pass.c_str(), pass.size(),
                        crypto_pwhash_OPSLIMIT_INTERACTIVE,
                        crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
    std::cerr << "Hashing failed\n";
    std::hash<std::string> h;
    return std::to_string(h(pass));
  }

  return hash;
}

struct CommandResult {
  bool success;
  std::string message;
  std::string user = "";
};

template <typename Storage>
inline CommandResult handle_auth_command(const std::string &line,
                                         Storage &storage) {
  std::vector<std::string> parts;
  boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
  if (parts.empty()) {
    return {false, "ERROR Empty command\n"};
  }

  auto cmd = parts[0];

  if (cmd == "REGISTER") {
    if (parts.size() != 3)
      return {false, "ERROR Usage: REGISTER <login> <password>\n"};

    const std::string &login = parts[1];
    const std::string &pass = parts[2];
    try {
      User u{0, login, hash_password(pass)};
      storage.insert(u);
      std::cerr << "registration\n";
      return {true, "OK Registered user '" + login + "'\n", parts[1]};
    } catch (const std::exception &e) {
      std::cerr << "no registration\n";
      return {false, std::string{"ERROR "} + e.what() + "\n"};
    }
  } else if (cmd == "LOGIN") {
    if (parts.size() != 3)
      return {false, "ERROR Usage: LOGIN <login> <password>\n"};

    const std::string &login = parts[1];
    const std::string &pass = parts[2];

    auto users = storage.template get_all<User>(
        sqlite_orm::where(sqlite_orm::c(&User::login) == login));

    if (users.empty())
      return {false, "ERROR No such user\n"};

    const auto &u = users.front();

    if (crypto_pwhash_str_verify(u.passhash.c_str(), pass.c_str(),
                                 pass.size()) != 0)
      return {false, "ERROR Invalid password\n"};

    return {true, "OK Logged in as '" + login + "'\n", parts[1]};
  }
  return {false, "ERROR Unknown command\n"};
}

template <typename Storage>
inline CommandResult handle_lobby_command(const std::string &line,
                                          Storage &storage) {
  std::vector<std::string> parts;
  boost::split(parts, line, boost::is_any_of(" "), boost::token_compress_on);
  if (parts.empty()) {
    return {false, "ERROR Empty command\n"};
  }

  auto cmd = parts[0];

  if (cmd == "CHAT") {
    if (parts.size() != 3)
      return {false, "ERROR Usage: REGISTER <login> <password>\n"};

    const std::string &login = parts[1];
    const std::string &pass = parts[2];
    try {
      User u{0, login, hash_password(pass)};
      storage.insert(u);
      return {true, "OK Registered user '" + login + "'\n", parts[1]};
    } catch (const std::exception &e) {
      return {false, std::string{"ERROR "} + e.what() + "\n"};
    }
  } else if (cmd == "LOGOUT") {
    
  } else if (cmd == "LIST") {
    
  }
  return {false, "ERROR Unknown command\n"};
}

// template <typename Storage>
// inline CommandResult handle_chat_command(const std::string &line,
//                                          Storage &storage) {}