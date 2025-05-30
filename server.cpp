#include <boost/asio.hpp>
#include <exception>
#include <memory>
#include <optional>
#include <queue>
#include <sqlite_orm/sqlite_orm.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "includes/commands.hpp"
#include "includes/database.hpp"
#include "includes/server.hpp"

class session : public std::enable_shared_from_this<session> {
public:
  template <typename Storage>
  session(tcp::socket &&socket, Storage &storage)
      : socket(std::move(socket)), db(storage) {}

  void start(message_handler &&on_message, add_online_user &&on_add,
             delete_online_user &&on_delete, list_online_user &&on_list,
             error_handler &&on_error) {
    this->on_message = std::move(on_message);
    this->on_add = std::move(on_add);
    this->on_delete = std::move(on_delete);
    this->on_list = std::move(on_list);
    this->on_error = std::move(on_error);
    async_read();
  }

  void post(const std::string &message) {

    bool idle = outgoing.empty();
    outgoing.push(message);

    if (idle) {
      async_write();
    }
  }

  bool chatting_with(const std::string &login) const {
    return current_peer && *current_peer == login;
  }

private:
  void deliver_undelivered_messages() {
    using namespace sqlite_orm;
    int sender_id =
        *db.select(&User::id, where(c(&User::login) == *current_peer)).begin();
    int receiver_id =
        *db.select(&User::id, where(c(&User::login) == *current_user)).begin();

    auto undelivered =
        db.get_all<Message>(where(c(&Message::sender_id) == sender_id &&
                                  c(&Message::receiver_id) == receiver_id &&
                                  c(&Message::delivered) == false),
                            order_by(&Message::ts));

    for (auto &msg : undelivered) {
      std::time_t ts = msg.ts;
      char buf[20];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&ts));
      std::string current_time(buf);

      post("[" + current_time + "] " + *current_peer + ": " + msg.body +
           "\r\n");

      msg.delivered = true;
      db.update(msg);
    }
  }

  void deliver_history_messages(int n) {
    using namespace sqlite_orm;
    int sender_id =
        *db.select(&User::id, where(c(&User::login) == *current_peer)).begin();
    int receiver_id =
        *db.select(&User::id, where(c(&User::login) == *current_user)).begin();

    auto history =
        db.get_all<Message>(where(c(&Message::sender_id) == sender_id &&
                                      c(&Message::receiver_id) == receiver_id ||
                                  c(&Message::sender_id) == receiver_id &&
                                      c(&Message::receiver_id) == sender_id),
                            order_by(&Message::ts), limit(n));

    for (auto &msg : history) {
      std::time_t ts = msg.ts;
      char buf[20];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&ts));
      std::string current_time(buf);

      post("[" + current_time + "] " + *current_peer + ": " + msg.body +
           "\r\n");

      msg.delivered = true;
      db.update(msg);
    }
  }

  void chat_message() {
    post("\033[2J\033[H\n");
    post("========================================\r\n");
    post("  Chat with  " + current_peer.value() + "\r\n");
    post("========================================\r\n");
    post("Type /exit           — back to lobby\r\n");
    post("Type /history <N>    — show last N messages\r\n");
    post("Type /who            — show chat partner\r\n");
    post("----------------------------------------\r\n");
  }

  void async_read() {
    io::async_read_until(
        socket, streambuf, "\n",
        std::bind(&session::on_read, shared_from_this(), _1, _2));
  }

  void on_read(error_code error, std::size_t bytes_transferred) {
    if (!error) {
      auto begin = boost::asio::buffers_begin(streambuf.data());
      std::string line(begin, begin + bytes_transferred - 1);
      streambuf.consume(bytes_transferred);

      if (!is_logged_in()) {
        auto res = handle_auth_command(line, db);
        post("Server: " + res.message);
        if (res.success) {
          current_user = res.user;
          post(LOBBY_MSG);

          on_add(*current_user, shared_from_this());
        }
      } else {
        if (in_chat()) {
          auto res = handle_chat_command(line);
          if (!res.success) {
            post("Server: " + res.message);
          } else {
            if (res.message == "exit") {
              current_peer.reset();
              post("\033[2J\033[H\n");
              post(LOBBY_MSG);
            } else if (res.message == "who") {
              post("Chat with " + current_peer.value() + "\r\n");
            } else if (res.message == "history") {
              chat_message();
              deliver_history_messages(res.n);
            } else {
              on_message(res.message, *current_user, *current_peer);
            }
          }

        } else {
          auto res = handle_lobby_command(line, db);
          if (!res.success) {
            post("Server: " + res.message);
          } else {
            if (res.message == "logout") {
              on_delete(*current_user);

              post("\033[2J\033[H\n");
              post(WELCOME_MSG);

              current_peer.reset();
              current_user.reset();
            } else if (res.message == "chat") {
              current_peer = res.user;
              chat_message();
              deliver_undelivered_messages();
            } else if (res.message == "list") {
              post(on_list(*current_user));
            }
          }
        }
      }

      async_read();
    } else {
      socket.close(error);
      on_error();
    }
  }

  void async_write() {
    io::async_write(socket, io::buffer(outgoing.front()),
                    std::bind(&session::on_write, shared_from_this(), _1, _2));
  }

  void on_write(error_code error, std::size_t /* bytes_transferred */) {
    if (!error) {
      outgoing.pop();

      if (!outgoing.empty()) {
        async_write();
      }
    } else {
      socket.close(error);
      on_error();
    }
  }

  bool is_logged_in() const { return current_user.has_value(); }
  bool in_chat() const { return current_peer.has_value(); }

  tcp::socket socket;
  decltype(initStorage()) &db;

  io::streambuf streambuf;
  std::queue<std::string> outgoing;

  message_handler on_message;
  add_online_user on_add;
  delete_online_user on_delete;
  list_online_user on_list;
  error_handler on_error;

  std::optional<std::string> current_user;
  std::optional<std::string> current_peer;
};

class server {
public:
  using session_ptr = std::shared_ptr<session>;

  template <typename Storage>
  server(io::io_context &io_context, std::uint16_t port, Storage &storage)
      : io_context(io_context),
        acceptor(io_context, tcp::endpoint(tcp::v4(), port)), db(storage) {}

  void async_accept() {
    socket.emplace(io_context);

    acceptor.async_accept(*socket, [&](error_code /* error */) {
      auto client = std::make_shared<session>(std::move(*socket), db);
      client->post(WELCOME_MSG);

      // post("We have a newcomer\n\r");

      clients.insert(client);

      client->start(std::bind(&server::post, this, _1, _2, _3),
                    std::bind(&server::add_online, this, _1, _2),
                    std::bind(&server::del_online, this, _1),
                    std::bind(&server::list_online, this, _1),
                    [&, weak = std::weak_ptr(client)] {
                      if (auto shared = weak.lock();
                          shared && clients.erase(shared)) {
                        // post("We are one less\n\r");
                      }
                    });

      async_accept();
    });
  }

  int get_user_id(const std::string &login) const {
    using namespace sqlite_orm;

    auto rows = db.select(&User::id, where(c(&User::login) == login));
    int id = *rows.begin();
    return id;
  }

  void post(const std::string &message, const std::string &from_login,
            const std::string &to_login) {
    auto it = online.find(to_login);
    if (it != online.end()) {

      std::time_t ts = std::time(nullptr);
      char buf[20];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&ts));
      std::string current_time(buf);

      bool peer_in_chat = it->second->chatting_with(from_login);

      if (peer_in_chat) {
        it->second->post("[" + current_time + "] " + from_login + ": " +
                         message + "\r\n");
      }

      try {
        db.insert(Message{0, get_user_id(from_login), get_user_id(to_login),
                          message, ts, peer_in_chat});
      } catch (const std::exception &e) {
        std::cerr << "[DB] cannot insert message: " << e.what() << '\n';
      }

      return;
    }
    try {
      Message msg;
      msg.id = 0;
      msg.sender_id = get_user_id(from_login);
      msg.receiver_id = get_user_id(to_login);
      msg.body = message;
      msg.ts = std::time(nullptr);
      msg.delivered = false;

      db.insert(msg);
    } catch (const std::exception &e) {
      std::cerr << "[DB] cannot insert message: " << e.what() << '\n';
    }
  }

  void add_online(const std::string &login, session_ptr s) {
    online[login] = s;
  }
  void del_online(const std::string &login) { online.erase(login); }

  std::string list_online(const std::string &login) const {
    std::string out = "USERS:";
    for (auto &&kv : online) {
      if (kv.first != login)
        out += ' ' + kv.first;
    }
    return out + "\r\n";
  }

private:
  io::io_context &io_context;
  tcp::acceptor acceptor;
  std::optional<tcp::socket> socket;
  std::unordered_set<std::shared_ptr<session>> clients;
  std::unordered_map<std::string, session_ptr> online;

  decltype(initStorage()) &db;
};

int main() {
  auto &storage = initStorage("user.db");
  io::io_context io_context;
  server srv(io_context, 15001, storage);
  srv.async_accept();
  io_context.run();
  return 0;
}