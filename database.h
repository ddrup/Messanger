#pragma once

#include <ctime>
#include <sqlite_orm/sqlite_orm.h>
#include <string>

struct User {
  int id;            // PRIMARY KEY AUTOINCREMENT
  std::string login; // UNIQUE
  std::string passhash;
};

struct Message {
  int id;
  int sender_id;
  int receiver_id;
  std::string body;
  std::time_t ts;
  bool delivered;
};

inline auto &initStorage(const std::string &dbPath = "users.db") {
  using namespace sqlite_orm;

  static auto storage = make_storage(
      dbPath,
      make_table("users",
                 make_column("id", &User::id, primary_key().autoincrement()),
                 make_column("login", &User::login, unique()),
                 make_column("passhash", &User::passhash)),
      make_table(
          "messages",
          make_column("id", &Message::id, primary_key().autoincrement()),
          make_column("sender_id", &Message::sender_id),
          make_column("receiver_id", &Message::receiver_id),
          make_column("body", &Message::body), 
          make_column("ts", &Message::ts),
          make_column("delivered", &Message::delivered, default_value(false)),

          foreign_key(&Message::sender_id).references(&User::id),
          foreign_key(&Message::receiver_id).references(&User::id)));

  storage.sync_schema();
  return storage;
}