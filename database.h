#pragma once

#include <sqlite_orm/sqlite_orm.h>
#include <string>

struct User {
  int id;            // PRIMARY KEY AUTOINCREMENT
  std::string login; // UNIQUE
  std::string passhash;
};

inline auto &initStorage(const std::string &dbPath = "users.db") {
  static auto storage = sqlite_orm::make_storage(
      dbPath,
      sqlite_orm::make_table(
          "users",
          sqlite_orm::make_column("id", &User::id, sqlite_orm::primary_key().autoincrement()),
          sqlite_orm::make_column("login", &User::login, sqlite_orm::unique()),
          sqlite_orm::make_column("passhash", &User::passhash)));
          
  storage.sync_schema();
  return storage;
}