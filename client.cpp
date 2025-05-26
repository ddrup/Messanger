#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/impl/read_until.hpp>
#include <boost/asio/impl/write.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <deque>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace io = boost::asio;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;
using namespace std::placeholders;

class client : public std::enable_shared_from_this<client> {
public:
  client(io::io_context &ctx, const std::string &host, const std::string &port)
      : io_context(ctx), resolver(ctx), socket(ctx), host(host), port(port) {}

  void start() {
    auto self = shared_from_this();
    resolver.async_resolve(
        host, port,
        [self](error_code error, tcp::resolver::results_type result) {
          if (error) {
            std::cerr << "Resolve error: " << error.message() << "\n";
            return;
          }
          self->do_connect(std::move(result));
        });
  }

private:
  void write(const std::string &line) {
    bool idle = write_queue.empty();
    write_queue.push_back(line);

    if (idle) {
      do_write();
    }
  }

  void do_connect(tcp::resolver::results_type &&endpoints) {
    auto self = shared_from_this();
    io::async_connect(
        socket, endpoints, [self](error_code error, const tcp::endpoint &) {
          if (error) {
            std::cerr << "Connect error: " << error.message() << "\n";
          } else {
            self->do_read();

            std::thread([self]() {
              std::string line;
              while (std::getline(std::cin, line)) {
                self->io_context.post(
                    [self, line]() { self->write(line + '\n'); });
              }
            }).detach();
          }
        });
  }

  void do_read() {
    auto self = shared_from_this();
    io::async_read_until(
        socket, streambuf, '\n',
        [self](error_code error, std::size_t bytes_transferred) {
          if (error) {
            self->socket.close();
            std::cerr << "Read error: " << error.message() << "\n";
          } else {
            std::stringstream message;
            message << std::istream(&self->streambuf).rdbuf();
            self->streambuf.consume(bytes_transferred);

            std::cout << message.str();
            self->do_read();
          }
        });
  }

  void do_write() {
    auto self = shared_from_this();
    io::async_write(socket, io::buffer(write_queue.front()),
                    [self](error_code error, std::size_t) {
                      if (error) {
                        std::cerr << "Write error: " << error.message() << "\n";
                        self->socket.close(error);
                      }

                      self->write_queue.pop_front();
                      if (!self->write_queue.empty()) {
                        self->do_write();
                      }
                    });
  }

  io::io_context &io_context;
  tcp::resolver resolver;
  tcp::socket socket;
  io::streambuf streambuf;
  std::string host, port;
  std::deque<std::string> write_queue;
};

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: client <host> <port>" << std::endl;
    return 1;
  }

  io::io_context io_context;
  auto c = std::make_shared<client>(io_context, argv[1], argv[2]);
  c->start();
  io_context.run();

  return 0;
}