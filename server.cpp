#include <boost/asio.hpp>
#include <optional>
#include <queue>
#include <string>
#include <unordered_set>

#include "commands.h"
#include "database.h"

namespace io = boost::asio;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;
using namespace std::placeholders;

using message_handler = std::function<void(std::string)>;
using error_handler = std::function<void()>;

static constexpr auto WELCOME_MSG =
    "Server: Welcome to chat\r\n"
    "To register:    REGISTER <login> <password>\r\n"
    "To login:       LOGIN    <login> <password>\r\n";

class session : public std::enable_shared_from_this<session> {
public:
  template <typename Storage>
  session(tcp::socket &&socket, Storage &storage)
      : socket(std::move(socket)), db(storage) {}

  void start(message_handler &&on_message, error_handler &&on_error) {
    this->on_message = std::move(on_message);
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

private:
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
        auto res = handle_command(line, db);
        post(res.message);
        current_user = res.user;
      } else {
        // TODO: process messages after login
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

  bool is_logged_in() const { return static_cast<bool>(current_user); }

  tcp::socket socket;
  io::streambuf streambuf;
  std::queue<std::string> outgoing;
  message_handler on_message;
  error_handler on_error;
  std::optional<std::string> current_user;
  decltype(initStorage()) &db;
};

class server {
public:
  template <typename Storage>
  server(io::io_context &io_context, std::uint16_t port, Storage &storage)
      : io_context(io_context),
        acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
        db(storage) {}

  void async_accept() {
    socket.emplace(io_context);

    acceptor.async_accept(*socket, [&](error_code /* error */) {
      auto client = std::make_shared<session>(std::move(*socket), db);
      client->post(WELCOME_MSG);

      // post("We have a newcomer\n\r");

      clients.insert(client);

      client->start(std::bind(&server::post, this, _1),
                    [&, weak = std::weak_ptr(client)] {
                      if (auto shared = weak.lock();
                          shared && clients.erase(shared)) {
                        // post("We are one less\n\r");
                      }
                    });

      async_accept();
    });
  }

  // don't need, write not common chat
  void post(const std::string &message) {
    for (auto &client : clients) {
      client->post(message);
    }
  }


private:
  io::io_context &io_context;
  tcp::acceptor acceptor;
  std::optional<tcp::socket> socket;
  std::unordered_set<std::shared_ptr<session>> clients;
  decltype(initStorage()) &db;
};

int main() {
  std::cout << "start\n";
  auto &storage = initStorage("user.db");
  io::io_context io_context;
  server srv(io_context, 15001, storage);
  srv.async_accept();
  io_context.run();
  return 0;
}