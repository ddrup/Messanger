#pragma once

#include <boost/asio.hpp>
#include <string>

static constexpr auto WELCOME_MSG =
    "Server: Welcome to chat\r\n"
    "===========================================\r\n"
    "To register:    REGISTER <login> <password>\r\n"
    "To login:       LOGIN    <login> <password>\r\n"
    "===========================================\r\n";

static constexpr auto LOBBY_MSG =
    "Server: you are in the lobby.\r\n"
    "Server: available commands:\r\n"
    "===========================================\r\n"
    "  CHAT  <login>   — start chat with user\r\n"
    "  LIST           — show online users\r\n"
    "  LOGOUT         — log out\r\n"
    "===========================================\r\n";

class server;
class session;

namespace io = boost::asio;
using tcp = io::ip::tcp;
using error_code = boost::system::error_code;
using namespace std::placeholders;

using message_handler =
    std::function<void(std::string, std::string, std::string)>;
using error_handler = std::function<void()>;

using add_online_user =
    std::function<void(std::string, std::shared_ptr<session>)>;
using delete_online_user = std::function<void(std::string)>;
using list_online_user = std::function<std::string(std::string)>;