#include "stdafx.h"

#include "client.h"
#include "client_dialog.h"
#include "connection.h"
#include "util.h"
#include "uri.h"

#include <dirent.h>
#include <sha.h>
#include <files.h>
#include <filters.h>
#include <base64.h>
#include <hex.h>

#include <Windows.h>
#include <filesystem>
#include <regex>
#include <algorithm>
#include <sstream>

using namespace std;
using namespace asio;

int get_input_rate(char code) {
    switch (code) {
        case BRAZILIAN:
        case CHINESE:
        case GERMAN:
        case FRENCH:
        case DUTCH:
        case ITALIAN:
        case GATEWAY_64_PAL:
        case EUROPEAN_BASIC_SPEC:
        case SPANISH:
        case AUSTRALIAN:
        case SCANDINAVIAN:
        case EUROPEAN_X:
        case EUROPEAN_Y:
            return 50;
        default:
            return 60;
    }
}

bool operator==(const BUTTONS& lhs, const BUTTONS& rhs) {
    return lhs.Value == rhs.Value;
}

bool operator!=(const BUTTONS& lhs, const BUTTONS& rhs) {
    return !(lhs == rhs);
}

client::client(shared_ptr<client_dialog> dialog) :
    connection(service), timer(service), my_dialog(dialog)
{
    QOS_VERSION version;
    version.MajorVersion = 1;
    version.MinorVersion = 0;
    QOSCreateHandle(&version, &qos_handle);

    my_dialog->set_message_handler([=](string message) {
        service.post([=] { on_message(message); });
    });

    my_dialog->set_close_handler([=] {
        service.post([=] {
            if (started) {
                my_dialog->minimize();
            } else {
                my_dialog->destroy();
                close();
                map_src_to_dst();
                start_game();
            }
        });
    });

    my_dialog->info("Available Commands:\r\n\r\n"
                    "/name <name>		    	Set your name\r\n"
                    "/host [port]		    	Host a private server\r\n"
                    "/join <address>		                Join a game\r\n"
                    "/start			                Start the game\r\n"
                    "/map <src>:<dst> [...]                            Map your controller ports\r\n"
                    "/autolag			    	Toggle automatic lag on and off\r\n"
                    "/buffer <buffer>			Set the netplay input lag\r\n"
                    "/golf			                Toggle golf mode on and off\r\n"
                    "/auth <id>		                Delegate input authority to another user\r\n");

#ifdef DEBUG
    input_log.open("input.log");
#endif
}

client::~client() {
    stop();
}

void client::on_error(const error_code& error) {
    if (error) {
        my_dialog->error(error == error::eof ? "Disconnected from server" : error.message());
    }
}

bool client::input_detected(const input_data& input, uint32_t mask) {
    for (auto b : reinterpret_cast<const std::array<BUTTONS, 4>&>(input.data)) {
        b.Value &= mask;
        if (b.X_AXIS <= -16 || b.X_AXIS >= +16) return true;
        if (b.Y_AXIS <= -16 || b.Y_AXIS >= +16) return true;
        if (b.Value & 0x0000FFFF) return true;
    }
    return false;
}

void client::load_public_server_list() {
    // Fetch server list from GitHub repository
    fetch_server_list_from_web();

    my_dialog->update_server_list(public_servers);
    ping_public_server_list();
}

void client::fetch_server_list_from_web() {
    try {
        // Clear existing servers
        public_servers.clear();

        // Add hardcoded servers directly
        public_servers["us-east.marioparty.online:9065|Buffalo (New York)"] = SERVER_STATUS_PENDING;
        public_servers["germany.marioparty.online:9051|Frankfurt (Germany)"] = SERVER_STATUS_PENDING;
        public_servers["brazil.marioparty.online:9000|São Paulo (Brazil)"] = SERVER_STATUS_PENDING;

        if (!public_servers.empty()) {
            my_dialog->info("Loaded " + std::to_string(public_servers.size()) + " servers");
        }

    } catch (const std::exception& e) {
        my_dialog->error("Failed to load server list: " + std::string(e.what()));
    }
}

void client::fetch_servers_from_github() {
    try {
        // GitHub raw content URL for servers.txt
        std::string host = "raw.githubusercontent.com";
        std::string path = "/MarioPartyNetplay/Project64-MPN/refs/heads/master/servers.txt";

        // Resolve hostname
        ip::tcp::resolver resolver(service);
        std::error_code resolve_error;
        auto endpoints = resolver.resolve(host, "80", resolve_error);
        if (resolve_error) {
            throw std::runtime_error("Failed to resolve hostname: " + resolve_error.message());
        }

        // Create socket and connect
        auto socket = std::make_shared<ip::tcp::socket>(service);
        auto connect_timer = std::make_shared<asio::steady_timer>(service);
        connect_timer->expires_after(std::chrono::seconds(10)); // 10 second timeout

        // Set up timeout handler
        connect_timer->async_wait([socket](const std::error_code& error) {
            if (!error) {
                // Timer expired, close socket to cancel connection
                std::error_code ec;
                socket->close(ec);
            }
        });

        // Connect with timeout
        std::error_code connect_error;
        asio::connect(*socket, endpoints, connect_error);
        if (connect_error) {
            throw std::runtime_error("Failed to connect: " + connect_error.message());
        }

        // Send HTTP GET request
        std::string request =
            "GET " + path + " HTTP/1.1\r\n"
            "Host: " + host + "\r\n"
            "Connection: close\r\n"
            "User-Agent: Project64-Netplay\r\n"
            "\r\n";

        asio::write(*socket, asio::buffer(request));

        // Read response
        std::string response;
        std::array<char, 1024> buffer;
        std::error_code error;

        while (true) {
            size_t bytes_read = socket->read_some(asio::buffer(buffer), error);
            if (error == std::errc::connection_reset || error == std::errc::connection_aborted) {
                break; // Connection closed
            } else if (error) {
                throw std::runtime_error("Network error: " + error.message());
            }
            response.append(buffer.data(), bytes_read);
        }

        socket->close();

        // Parse HTTP response
        size_t header_end = response.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            throw std::runtime_error("Invalid HTTP response");
        }

        std::string body = response.substr(header_end + 4);

        // Check for HTTP 200 OK
        if (response.find("HTTP/1.1 200 OK") == std::string::npos &&
            response.find("HTTP/1.0 200 OK") == std::string::npos) {
            throw std::runtime_error("HTTP request failed");
        }

        // Parse server list from body
        std::istringstream stream(body);
        std::string line;
        bool found_servers = false;

        while (std::getline(stream, line)) {
            // Remove carriage return if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Parse server entry: host:port|description
            size_t pipe_pos = line.find('|');
            if (pipe_pos != std::string::npos) {
                std::string server_info = line;
                public_servers[server_info] = SERVER_STATUS_PENDING;
                found_servers = true;
            }
        }

        if (!found_servers) {
            throw std::runtime_error("No valid servers found in response");
        }

    } catch (const std::exception& e) {
        throw std::runtime_error("Error fetching servers from GitHub: " + std::string(e.what()));
    }
}

void client::load_servers_from_file() {
    try {
        std::ifstream file("servers.txt");
        if (!file.is_open()) {
            throw std::runtime_error("Could not open servers.txt file");
        }

        std::string line;
        while (std::getline(file, line)) {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Parse server entry: host:port|description
            size_t pipe_pos = line.find('|');
            if (pipe_pos != std::string::npos) {
                std::string server_info = line;
                public_servers[server_info] = SERVER_STATUS_PENDING;
            }
        }

        file.close();

        if (public_servers.empty()) {
            throw std::runtime_error("No valid servers found in servers.txt");
        }

    } catch (const std::exception& e) {
        throw std::runtime_error("Error loading servers from file: " + std::string(e.what()));
    }
}

void client::ping_public_server_list() {
    auto done = [=](const string& host, double ping, shared_ptr<ip::udp::socket> socket = nullptr) {
        public_servers[host] = ping;
        my_dialog->update_server_list(public_servers);
        if (socket && socket->is_open()) {
            socket->close();
        }
    };
    auto s(weak_from_this());
    for (auto& e : public_servers) {
        uri u(e.first.substr(0, e.first.find('|')));
        udp_resolver.async_resolve(u.host, to_string(u.port ? u.port : 6400), [=](const auto& error, auto iterator) {
            if (s.expired()) return;
            if (error) return done(e.first, SERVER_STATUS_ERROR);
            auto socket = make_shared<ip::udp::socket>(service);
            socket->open(iterator->endpoint().protocol());
            socket->connect(*iterator);
            if (qos_handle != NULL) {
                QOS_FLOWID flowId = 0;
                QOSAddSocketToFlow(qos_handle, socket->native_handle(), socket->remote_endpoint().data(), QOSTrafficTypeAudioVideo, QOS_NON_ADAPTIVE_FLOW, &flowId);
            }
            auto p(make_shared<packet>());
            *p << SERVER_PING << timestamp();
            socket->async_send(buffer(*p), [=](const error_code& error, size_t transferred) {
                if (s.expired()) return;
                p->reset();
                if (error) return done(e.first, SERVER_STATUS_ERROR, socket);

                auto timer = make_shared<asio::steady_timer>(service);
                timer->expires_after(std::chrono::seconds(3));
                socket->async_wait(ip::udp::socket::wait_read, [=](const error_code& error) {
                    if (s.expired()) return;
                    timer->cancel();
                    if (error) return done(e.first, SERVER_STATUS_ERROR, socket);
                    error_code ec;
                    p->resize(socket->available(ec));
                    if (ec) return done(e.first, SERVER_STATUS_ERROR, socket);
                    p->resize(socket->receive(buffer(*p), 0, ec));
                    if (ec) return done(e.first, SERVER_STATUS_ERROR, socket);
                    if (p->size() < 13 || p->read<query_type>() != SERVER_PONG) {
                        return done(e.first, SERVER_STATUS_VERSION_MISMATCH, socket);
                    }
                    auto server_version = p->read<uint32_t>();
                    if (PROTOCOL_VERSION < server_version) {
                        return done(e.first, SERVER_STATUS_OUTDATED_CLIENT, socket);
                    } else if (PROTOCOL_VERSION > server_version) {
                        return done(e.first, SERVER_STATUS_OUTDATED_SERVER, socket);
                    }
                    done(e.first, timestamp() - p->read<double>(), socket);
                });

                timer->async_wait([timer, socket](const asio::error_code& error) {
                    if (error) return;
                    socket->close();
                });
            });
        });
    }
}

void client::get_external_address() {
    service.post([&] {
        auto s(weak_from_this());
        udp_resolver.async_resolve(ip::udp::v4(), "query.play64.com", "6400", [=](const auto& error, auto iterator) {
            if (s.expired()) return;
            auto socket = make_shared<ip::udp::socket>(service);
            socket->open(iterator->endpoint().protocol());
            socket->connect(*iterator);
            auto p(make_shared<packet>());
            *p << EXTERNAL_ADDRESS;
            socket->async_send(buffer(*p), [=](const error_code& error, size_t transferred) {
                if (s.expired()) return;
                p->reset();
                if (error) return;

                auto timer = make_shared<asio::steady_timer>(service);
                timer->expires_after(std::chrono::seconds(3));
                socket->async_wait(ip::udp::socket::wait_read, [=](const error_code& error) {
                    if (s.expired()) return;
                    timer->cancel();
                    if (error) return;
                    error_code ec;
                    p->resize(socket->available(ec));
                    if (ec) return;
                    p->resize(socket->receive(buffer(*p), 0, ec));
                    if (ec) return;
                    if (p->read<query_type>() != EXTERNAL_ADDRESS) return;
                    if (p->available() >= sizeof(uint16_t)) p->read<uint16_t>();
                    if (p->available() == 4) {
                        std::array<uint8_t, 4> bytes;
                        for (auto& b : bytes) b = p->read<uint8_t>();
                        external_address = ip::make_address_v4(bytes);
                    } else if (p->available() == 16) {
                        std::array<uint8_t, 16> bytes;
                        for (auto& b : bytes) b = p->read<uint8_t>();
                        external_address = ip::make_address_v6(bytes);
                    }
                });

                timer->async_wait([timer, socket](const asio::error_code& error) {
                    if (error) return;
                    socket->close();
                });
            });
        });
    });
}

string client::get_name() {
    return run([&] { return me->name; });
}

void client::set_name(const string& name) {
    run([&] {
        me->name = name;
        trim(me->name);
        my_dialog->info("Your name is " + name);
    });
}

void client::set_rom_info(const rom_info& rom) {
    run([&] {
        me->rom = rom;
        my_dialog->info("Your game is " + me->rom.to_string());
        if (rom.name == "MarioGolf64") {
            golf_mode_mask = MARIO_GOLF_MASK;
        } else {
            golf_mode_mask = 0xFFFFFFFF;
        }
    });
}

void client::set_save_info(const string& save_path) {
    this->save_path = save_path;
    run([&] {
        update_save_info();
        });
}

void client::set_src_controllers(CONTROL controllers[4]) {
    run([&] {
        for (int i = 0; i < 4; i++) {
            me->controllers[i].plugin = controllers[i].Plugin;
            me->controllers[i].present = controllers[i].Present;
            me->controllers[i].raw_data = controllers[i].RawData;
        }
        send_controllers();
    });
}

void client::set_dst_controllers(CONTROL controllers[4]) {
    run([&] { this->controllers = controllers; });
}

void client::process_input(array<BUTTONS, 4>& buttons) {
    run([&] {
#ifdef DEBUG
        //static uniform_int_distribution<uint32_t> dist(16, 63);
        //static random_device rd;
        //static uint32_t i = 0;
        //while (input_id >= i) i += dist(rd);
        //if (golf) buttons[0].A_BUTTON = (i & 1);
#endif
        input_data input = { buttons[0].Value, buttons[1].Value, buttons[2].Value, buttons[3].Value, me->map };
        repeated_input = (input == me->input ? repeated_input + 1 : 0);
        me->input = input;

        for (auto& u : user_list) {
            if (u->authority != me->id) continue;
            while (u->input_id <= input_id + u->lag) {
                send_input(*u);
            }
        }

        if (me->authority != me->id) {
            if (golf && input_detected(me->input, golf_mode_mask)) {
                me->pending = me->input;
                for (auto& u : user_list) {
                    change_input_authority(u->id, me->id);
                }
            } else if (udp_established) {
                if (repeated_input < INPUT_HISTORY_LENGTH || input_id % 30 == 0) {
                    send_input_update(me->input);
                }
            } else if (repeated_input == 0) {
                send_input_update(me->input);
            }
        }

        flush_all();
        
        on_input();
    });
    
    unique_lock<mutex> lock(next_input_mutex);
    next_input_condition.wait(lock, [=] { return !next_input.empty(); });
    buttons = next_input.front();
    next_input.pop_front();

}

void client::on_input() {
    for (auto& u : user_list) {
        if (u->input_queue.empty()) {
            return;
        }
    }

    unique_lock<mutex> lock(next_input_mutex);
    if (!next_input.empty()) {
        return;
    }

    array<BUTTONS, 4> result = { 0, 0, 0, 0 };
    array<int16_t, 4> analog_x = { 0, 0, 0, 0 };
    array<int16_t, 4> analog_y = { 0, 0, 0, 0 };

    for (auto& u : user_list) {
        if (u->input_queue.empty()) continue;
        auto input = u->input_queue.front();
        u->input_queue.pop_front();
        assert(u->input_id > input_id);

        auto b = input.map.bits;
        for (int i = 0; b && i < 4; i++) {
            BUTTONS buttons = { input.data[i] };
            for (int j = 0; b && j < 4; j++, b >>= 1) {
                if (b & 1) {
                    result[j].Value |= buttons.Value;
                    analog_x[j] += buttons.X_AXIS;
                    analog_y[j] += buttons.Y_AXIS;
                }
            }
        }
    }
    
    for (int i = 0; i < 4; i++) {
        double x = analog_x[i] + 0.5;
        double y = analog_y[i] + 0.5;
        double r = max(abs(x), abs(y));
        if (r > 127.5) {
            result[i].X_AXIS = (int)floor(x / r * 127.5);
            result[i].Y_AXIS = (int)floor(y / r * 127.5);
        } else {
            result[i].X_AXIS = analog_x[i];
            result[i].Y_AXIS = analog_y[i];
        }
    }

    next_input.push_back(result);
    next_input_condition.notify_one();

#ifdef DEBUG
    constexpr static char B[] = "><v^SZBA><v^RL";
    static array<BUTTONS, 4> prev = { 0, 0, 0, 0 };
    if (input_log.is_open() && result != prev) {
        input_log << setw(8) << setfill('0') << input_id << '|';
        for (int i = 0; i < 4; i++) {
            (result[i].X_AXIS ? input_log << setw(4) << setfill(' ') << showpos << result[i].X_AXIS << ' ' : input_log << "     ");
            (result[i].Y_AXIS ? input_log << setw(4) << setfill(' ') << showpos << result[i].Y_AXIS << ' ' : input_log << "     ");
            for (int j = static_cast<int>(strlen(B)) - 1; j >= 0; j--) {
                input_log << (result[i].Value & (1 << j) ? B[j] : ' ');
            }
            input_log << '|';
        }
        input_log << '\n';
    }
    prev = result;
#endif

    input_id++;

    input_times.push_back(timestamp());
    while (input_times.front() < input_times.back() - 2.0) {
        input_times.pop_front();
    }
}

void client::post_close() {
    service.post([&] { close(); });
}

client_dialog& client::get_dialog() {
    return *my_dialog;
}

bool client::wait_until_start() {
    if (started) return false;
    unique_lock<mutex> lock(start_mutex);
    start_condition.wait(lock, [=] { return started; });
    return true;
}

bool client::has_started() const {
    return started;
}

void client::on_message(string message) {
    try {
        if (message.substr(0, 1) == "/") {
            vector<string> params;
            for (auto start = message.begin(), end = start; start != message.end(); start = end) {
                start = find_if(start, message.end(), [](char ch) { return !isspace<char>(ch, locale::classic()); });
                end   = find_if(start, message.end(), [](char ch) { return  isspace<char>(ch, locale::classic()); });
                if (end > start) {
                    params.push_back(string(start, end));
                }
            }

            if (params[0] == "/name") {
                if (params.size() < 2) throw runtime_error("Missing parameter");

                me->name = params[1];
                trim(me->name);
                my_dialog->info("Your name is now " + me->name);
                send_name();
            } else if (params[0] == "/host" || params[0] == "/server") {
                if (started) throw runtime_error("Game has already started");

                host = "127.0.0.1";
                port = params.size() >= 2 ? stoi(params[1]) : 6400;
                path = "/";
                close();
                my_server = make_shared<server>(service, false);
                port = my_server->open(port);
                set_host_status(false);
                my_dialog->info("Server is listening on port " + to_string(port) + "...");
                connect(host, port, path);
            } else if (params[0] == "/join" || params[0] == "/connect") {
                if (started) throw runtime_error("Game has already started");
                if (params.size() < 2) throw runtime_error("Missing parameter");

                uri u(params[1]);
                if (!u.scheme.empty() && u.scheme != "play64") {
                    throw runtime_error("Unsupported protocol: " + u.scheme);
                }
                host = u.host;
                port = params.size() >= 3 ? stoi(params[2]) : (u.port ? u.port : 6400);
                path = u.path;
                close();
                connect(host, port, path);
            } else if (params[0] == "/start") {
                if (started) throw runtime_error("Game has already started");
                if (!is_host()) {
                    my_dialog->info("Only the host can start the game...");
                } else {
                    Sleep(1000);
                    send_savesync();
                    Sleep(250);
                    Sleep(1000);

                    if (is_open()) {
                        send_start_game();
                    } else {
                        map_src_to_dst();
                        set_lag(0);
                        start_game();
                    }
                }
            } else if (params[0] == "/buffer" || params[0] == "/lag") {
                if (params.size() < 2) throw runtime_error("Missing parameter");
                uint8_t lag = stoi(params[1]);
                if (!is_open()) throw runtime_error("Not connected");
                send_autolag(0);
                send_lag(lag, true, true);
                set_lag(lag);
            } else if (params[0] == "/my_lag") {
                if (params.size() < 2) throw runtime_error("Missing parameter");
                uint8_t lag = stoi(params[1]);
                send_lag(lag, true, false);
                set_lag(lag);
            } else if (params[0] == "/your_lag") {
                if (params.size() < 2) throw runtime_error("Missing parameter");
                if (!is_open()) throw runtime_error("Not connected");
                uint8_t lag = stoi(params[1]);
                send_autolag(0);
                send_lag(lag, false, true);
            } else if (params[0] == "/autolag") {
                if (!is_open()) throw runtime_error("Not connected");
                send_autolag();
            } else if (params[0] == "/golf") {
                if (!is_open()) throw runtime_error("Not connected");
                set_golf_mode(!golf);
                send(packet() << GOLF << golf);
                for (auto& u : user_list) {
                    change_input_authority(u->id, (golf ? me->id : u->id));
                }
            } else if (params[0] == "/map") {
                if (!is_open()) throw runtime_error("Not connected");

                input_map map;
                for (size_t i = 1; i < params.size(); i++) {
                    if (params[i].size() < 2 || params[i][1] != ':') throw runtime_error("Invalid controller map: \"" + params[i] + "\"");
                    int src = stoi(params[i].substr(0, 1)) - 1;
                    for (size_t j = 2; j < params[i].size(); j++) {
                        int dst = stoi(params[i].substr(j, 1)) - 1;
                        map.set(src, dst);
                    }
                }
                set_input_map(map);
            } else if (params[0] == "/auth") {
                if (!is_open()) throw runtime_error("Not connected");

                uint32_t authority_id = params.size() >= 2 ? stoi(params[1]) - 1 : me->id;
                uint32_t user_id      = params.size() >= 3 ? stoi(params[2]) - 1 : me->id;

                if (authority_id >= user_map.size() || !user_map[authority_id]) throw runtime_error("Invalid authority user ID");
                if (user_id      >= user_map.size() || !user_map[user_id])      throw runtime_error("Invalid user ID");

                if (user_map[user_id]->authority != authority_id) {
                    change_input_authority(user_id, authority_id);

                    if (user_id == authority_id) {
                        my_dialog->info("Input authority restored - enable frame rate limit");
                    } else {
                        my_dialog->info("Input authority delegated to " + user_map[authority_id]->name);
                    }
                }
            } else {
                throw runtime_error("Unknown command: " + params[0]);
            }
        } else {
            my_dialog->message(me->name, message);
            send_message(message);
        }
    } catch (const exception& e) {
        my_dialog->error(e.what());
    } catch (const error_code& e) {
        my_dialog->error(e.message());
    }
}

void client::set_lag(uint8_t lag) {
    me->lag = lag;
}

void client::set_golf_mode(bool golf) {
    if (golf == this->golf) return;
    this->golf = golf;
    if (golf) {
        my_dialog->info("Golf mode is enabled");
    } else {
        my_dialog->info("Golf mode is disabled");
    }
}

void client::remove_user(uint32_t user_id) {
    if (user_map.at(user_id) == me) return;

    if (started) {
        my_dialog->error(user_map.at(user_id)->name + " has quit");
    } else {
        my_dialog->info(user_map.at(user_id)->name + " has quit");
    }

    user_map.at(user_id) = nullptr;

    user_list.clear();
    for (auto& u : user_map) {
        if (u) user_list.push_back(u);
    }

    if (me->authority == user_id) {
        me->authority = me->id;
        send_delegate_authority(me->id, me->authority);

        if (started) {
            send_input(*me);
        }
    }

    update_user_list();

    if (started) {
        on_input();
    }
}

void client::message_received(uint32_t user_id, const string& message) {
    switch (user_id) {
        case ERROR_MSG:
            my_dialog->error(message);
            break;

        case INFO_MSG:
            my_dialog->info(message);
            break;

        default:
            my_dialog->message(user_map.at(user_id)->name, message);
            break;
    }
}

void client::close(const std::error_code& error) {
    connection::close(error);

    timer.cancel();

    if (my_server) {
        my_server->close();
        my_server.reset();
    }

    user_map.clear();
    user_list.clear();

    update_user_list();

    user_map.push_back(me);
    user_list.push_back(me);

    me->id = 0;
    me->authority = 0;
    me->lag = 0;
    me->latency = NAN;

    if (started) {
        send_input(*me);
        on_input();
    }
}

void client::start_game() {
    unique_lock<mutex> lock(start_mutex);
    if (started) return;
    started = true;
    start_condition.notify_all();
    // Note: Save reversion happens in RomClosed() after the game writes its final save
}

void client::connect(const string& host, uint16_t port, const string& room) {
    my_dialog->info("Connecting to " + host + (port == 6400 ? "" : ":" + to_string(port)) + "...");

    if (room.length() == 4) {
        set_host_status(false);
    } else {
        set_host_status(true);
    }

    ip::tcp::resolver tcp_resolver(service);
    error_code error;
    auto endpoint = tcp_resolver.resolve(host, to_string(port), error);
    if (error) {
        return my_dialog->error(error.message());
    }

    if (!tcp_socket) {
        tcp_socket = make_shared<ip::tcp::socket>(service);
    }
    tcp_socket->connect(*endpoint, error);
    if (error) {
        return my_dialog->error(error.message());
    }
    tcp_socket->set_option(ip::tcp::no_delay(true), error);
    if (error) {
        return my_dialog->error(error.message());
    }
    if (qos_handle != NULL) {
        QOS_FLOWID flowId = 0;
        QOSAddSocketToFlow(qos_handle, tcp_socket->native_handle(), tcp_socket->remote_endpoint().data(), QOSTrafficTypeAudioVideo, QOS_NON_ADAPTIVE_FLOW, &flowId);
    }

    try {
        if (!udp_socket) {
            udp_socket = make_shared<ip::udp::socket>(service);
        }
        auto udp_endpoint = ip::udp::endpoint(tcp_socket->local_endpoint().address(), 0);
        udp_socket->open(udp_endpoint.protocol());
        udp_socket->bind(udp_endpoint);
    } catch (error_code e) {
        if (udp_socket) {
            udp_socket->close(e);
            udp_socket.reset();
        }
    }

    my_dialog->info("Connected!");

    query_udp_port([=]() {
        send_join(room, external_udp_port);
    });

    receive_tcp_packet();
}

bool client::replace_save_file(const save_info& save_data) {
    // This function should only be called with non-empty saves
    // Empty saves are handled in the SAVE_SYNC handler
    if (save_data.save_name.empty() || save_data.save_data.empty()) {
        return false;
    }
    
    std::string save_path_full = save_path + save_data.save_name;
    
    try {
        // Ensure directory exists
        std::filesystem::path file_path(save_path_full);
        if (file_path.has_parent_path()) {
            std::filesystem::create_directories(file_path.parent_path());
        }
        
        // Delete existing file if it exists (to replace it)
        if (std::filesystem::exists(save_path_full)) {
            if (!DeleteFileA(save_path_full.c_str())) {
                DWORD error = GetLastError();
                my_dialog->error("Failed to delete existing save file " + save_data.save_name + " (Error: " + std::to_string(error) + ")");
                return false;
            }
        }
        
        // Write new save data
        std::ofstream of(save_path_full.c_str(), std::ofstream::binary | std::ofstream::trunc);
        if (!of.is_open()) {
            DWORD error = GetLastError();
            my_dialog->error("Failed to open save file for writing: " + save_data.save_name + " (Error: " + std::to_string(error) + ")");
            return false;
        }
        
        of.write(save_data.save_data.data(), save_data.save_data.size());
        of.flush();
        of.close();
        
        // Verify file was written correctly
        if (!of.good()) {
            my_dialog->error("Error writing save file: " + save_data.save_name);
            return false;
        }
        
        // Verify file size matches
        if (std::filesystem::exists(save_path_full)) {
            auto file_size = std::filesystem::file_size(save_path_full);
            if (file_size != save_data.save_data.size()) {
                my_dialog->error("Save file size mismatch for " + save_data.save_name + 
                               " (expected " + std::to_string(save_data.save_data.size()) + 
                               ", got " + std::to_string(file_size) + ")");
                return false;
            }
        }
        
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        my_dialog->error("Filesystem error writing save " + save_data.save_name + ": " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        my_dialog->error("Error writing save " + save_data.save_name + ": " + std::string(e.what()));
        return false;
    }
}

std::vector<string> client::find_rom_save_files(const string& rom_name) {
    struct dirent* entry;
    DIR* dp;
    std::vector<string> ret;

    dp = opendir(save_path.c_str());
    if (dp == NULL)
        return ret;


    std::regex accept = std::regex("\\b((\\w+\\s?)+\\.((sra)|(eep)|(fla)|(mpk)))");
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_type && entry->d_type == DT_DIR) {
            continue;
        }

        std::string filename = std::string(entry->d_name);
        if (filename.find(rom_name) != std::string::npos
            && std::regex_search(entry->d_name, accept)) {
            ret.push_back(filename);
        }
    }
    closedir(dp);
    return ret;
}

string client::slurp(const string& path) {
    ostringstream buffer;
    ifstream input(path.c_str());
    buffer << input.rdbuf();
    return buffer.str();
}

string client::slurp2(const string& path) {
    std::ifstream t(path.c_str(), std::ios::binary);
    std::string str;

    t.seekg(0, std::ios::end);
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(t)),
        std::istreambuf_iterator<char>());
    return str;
}

void client::update_save_info()
{
    std::vector<string> save_files = find_rom_save_files(me->rom.name);

    std::sort(save_files.begin(), save_files.end());

    while (me->saves.size() > save_files.size()) {
        save_files.push_back("");
    }

    for (int i = 0; i < me->saves.size(); i++) {
        auto& save_info = me->saves[i];
        save_info.rom_name = me->rom.name;
        save_info.save_name = save_files.at(i);
        save_info.save_data = save_info.save_name.empty() ? "" : slurp2(save_path + save_info.save_name);
        save_info.sha1_data = save_info.save_name.empty() ? "" : sha1_save_info(save_info);
    }
    
    // Calculate cheat file hash
    me->cheat_file_hash = calculate_cheat_file_hash();
    
    // Calculate state hash if game has started
    if (started) {
        update_state_hash();
    }
}


string client::sha1_save_info(const save_info& saveInfo)
{
    CryptoPP::SHA256 hash;
    string digest;
    string filename = (save_path + saveInfo.save_name);
    CryptoPP::FileSource file(filename.c_str(), true, new CryptoPP::HashFilter(hash,
        new CryptoPP::HexEncoder(
            new CryptoPP::StringSink(digest)))
    );
    return digest;
}

string client::calculate_cheat_file_hash()
{
    std::string cheat_file = get_cheat_file_path();
    std::wstring wcheat_file = utf8_to_wstring(cheat_file);
    
    // Check if cheat file exists
    if (GetFileAttributes(wcheat_file.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return ""; // No cheat file, return empty hash
    }
    
    try {
        CryptoPP::SHA256 hash;
        string digest;
        CryptoPP::FileSource file(cheat_file.c_str(), true, new CryptoPP::HashFilter(hash,
            new CryptoPP::HexEncoder(
                new CryptoPP::StringSink(digest)))
        );
        return digest;
    } catch (...) {
        return ""; // Error reading file, return empty hash
    }
}

string client::calculate_state_hash()
{
    HMODULE hModule = GetModuleHandle(NULL);
    if (!hModule) {
        return "";
    }
    
    typedef bool(__cdecl* GetStateHashFunc)(char*, size_t);
    GetStateHashFunc getStateHash = (GetStateHashFunc)GetProcAddress(hModule, "GetEmulatorStateHashForNetplay");
    if (!getStateHash) {
        return ""; // Function not available
    }
    
    char hash_buffer[65];
    if (getStateHash(hash_buffer, sizeof(hash_buffer))) {
        return std::string(hash_buffer);
    }
    
    return "";
}

void client::update_state_hash()
{
    if (!started) {
        return; // Game not started yet
    }

    me->state_hash = calculate_state_hash();

    // Send state hash update to other players
    if (!me->state_hash.empty() && is_open()) {
        // State hash is automatically included in user_info when sent
        // For now, we'll rely on periodic user_info updates or add a dedicated packet type if needed
    }
}

void client::handle_desync_detection(const char* hash)
{
    if (!started || !hash || !hash[0]) {
        return; // Game not started or invalid hash
    }

    // Store the save state hash for desync detection
    me->desync_hash = hash;

    // Send desync hash update to other players
    if (is_open()) {
        // For now, we'll rely on periodic user_info updates or add a dedicated packet type if needed
        // The desync hash is automatically included in user_info when sent
    }

    // Compare desync hashes with other players immediately
    compare_all_players_desync_hashes();
}

void client::on_receive(packet& p, bool udp) {
    switch (p.read<packet_type>()) {
        case VERSION: {
            auto protocol_version = p.read<uint32_t>();
            if (protocol_version != PROTOCOL_VERSION) {
                close();
                my_dialog->error("Server protocol version does not match client protocol version. Visit www.play64.com to get the latest version of the plugin.");
            }
            break;
        }

        case JOIN: {
            auto info = p.read<user_info>();
            my_dialog->info(info.name + " has joined");
            auto u = make_shared<user_info>(info);
            user_map.push_back(u);
            user_list.push_back(u);
            update_user_list();
            // Log cheat information for the joining player
            log_player_cheat_info(u);
            // Compare all players' save hashes when a new player joins
            compare_all_players_save_hashes();
            compare_all_players_cheat_file_hashes();
            break;
        }

        case SAVE_INFO: {
            auto userId = p.read<uint32_t>();
            auto user = user_map.at(userId);

            for (int i = 0; i < me->saves.size(); i++) {
                auto save_data = p.read<save_info>();
                user->saves[i] = save_data;
            }

            // Compare all players' save hashes when save info is updated
            compare_all_players_save_hashes();
            compare_all_players_cheat_file_hashes();
            break;
        }

        case SAVE_SYNC: {
            // Non-hosts backup their original saves BEFORE replacing them with host's saves
            // Host doesn't need backup since their save is the one being synced
            if (!is_host()) {
                move_original_saves_to_temp();
            }

            int synced_count = 0;
            int skipped_count = 0;
            int error_count = 0;


            for (int i = 0; i < me->saves.size(); i++) {
                try {
                    auto save_data = p.read<save_info>();
                    auto my_save = me->saves[i];
                    
                    // Validate save data
                    if (save_data.rom_name != me->rom.name) {
                        my_dialog->error("Save sync error: ROM name mismatch for save slot " + std::to_string(i));
                        error_count++;
                        continue;
                    }
                    
                    // Check for oversized save files that could cause crashes
                    const size_t MAX_SAVE_SIZE = 2 * 1024 * 1024; // 2MB limit
                    if (save_data.save_data.size() > MAX_SAVE_SIZE) {
                        my_dialog->error("Save file too large for sync: " + save_data.save_name +
                                       " (" + std::to_string(save_data.save_data.size()) + " bytes), skipping");
                        error_count++;
                        continue;
                    }

                    // Always sync if hashes differ, or if one is empty and the other isn't
                    bool needs_sync = (my_save.sha1_data != save_data.sha1_data);
                    bool my_save_empty = my_save.save_name.empty() || my_save.save_data.empty();
                    bool incoming_save_empty = save_data.save_name.empty() || save_data.save_data.empty();
                    
                    // Also sync if one is empty and the other isn't (even if hashes match, which shouldn't happen)
                    if (!needs_sync && my_save_empty != incoming_save_empty) {
                        needs_sync = true;
                    }
                    
                    if (needs_sync) {
                        synced_count++;
                        
                        // Backup existing save before replacing (for non-hosts)
                        if (!is_host() && !my_save.save_name.empty()) {
                            std::string original_dir = save_path + "Original\\";
                            std::string original_path = save_path + my_save.save_name;
                            std::string backup_path = original_dir + my_save.save_name;
                            
                            // Backup if file exists and backup doesn't exist yet
                            if (std::filesystem::exists(original_path) && !std::filesystem::exists(backup_path)) {
                                try {
                                    std::filesystem::create_directories(original_dir);
                                    // Add a small delay to avoid potential race conditions with directory creation
                                    Sleep(10);
                                    std::filesystem::copy(original_path, backup_path, std::filesystem::copy_options::overwrite_existing);
                                } catch (const std::filesystem::filesystem_error& e) {
                                    my_dialog->error("Failed to backup save " + my_save.save_name + ": " + std::string(e.what()));
                                    error_count++;
                                }
                            }
                        }
                        
                        // Handle empty save - delete existing file before updating
                        bool incoming_empty = save_data.save_name.empty() || save_data.save_data.empty();
                        if (incoming_empty && !my_save.save_name.empty()) {
                            std::string existing_path = save_path + my_save.save_name;
                            if (std::filesystem::exists(existing_path)) {
                                if (!DeleteFileA(existing_path.c_str())) {
                                    DWORD error = GetLastError();
                                    my_dialog->error("Failed to delete save file " + my_save.save_name + " (Error: " + std::to_string(error) + ")");
                                    error_count++;
                                }
                            }
                        }
                        
                        // Update local save and replace file
                        // This will write the new save or skip if empty (already deleted above)
                        me->saves[i] = save_data;
                        if (!incoming_empty && !replace_save_file(save_data)) {
                            error_count++;
                        }
                    } else {
                        skipped_count++;
                    }

                    // Update local client's save data in user_map for hash tracking
                    // Only update the local player (me) in the user_map
                    for (auto& user : user_map) {
                        if (user && user.get() == me.get()) {
                            user->saves[i] = save_data;
                            break;
                        }
                    }
                } catch (const std::exception& e) {
                    my_dialog->error("Error processing save slot " + std::to_string(i) + ": " + std::string(e.what()));
                    error_count++;
                }
            }

            if (error_count > 0) {
                my_dialog->error("Save sync completed with " + std::to_string(error_count) + " error(s)");
            }

            // Process cheat files
            try {
                std::string cheat_file_content = p.read<std::string>();
                std::string enabled_file_content = p.read<std::string>();

                if (!cheat_file_content.empty() || !enabled_file_content.empty()) {
                    apply_cheats(cheat_file_content, enabled_file_content);
                }
            } catch (const std::exception& e) {
                my_dialog->error("Error processing cheat files: " + std::string(e.what()));
            }

            update_save_info();
            send_save_info();
            break;
        }

        case ACCEPT: {
            auto udp_port = p.read<uint16_t>();
            if (udp_socket && udp_port) {
                udp_socket->connect(ip::udp::endpoint(tcp_socket->remote_endpoint().address(), udp_port));
                if (qos_handle != NULL) {
                    QOS_FLOWID flowId = 0;
                    QOSAddSocketToFlow(qos_handle, udp_socket->native_handle(), udp_socket->remote_endpoint().data(), QOSTrafficTypeAudioVideo, QOS_NON_ADAPTIVE_FLOW, &flowId);
                }
                receive_udp_packet();
            } else {
                udp_socket.reset();
            }
            on_tick();

            user_map.clear();
            user_list.clear();
            while (p.available()) {
                if (p.read<bool>()) {
                    auto u = make_shared<user_info>(p.read<user_info>());
                    user_map.push_back(u);
                    user_list.push_back(u);
                } else {
                    user_map.push_back(nullptr);
                }
            }
            me = user_list.back();
            
            // Set host status: host is the first user in the list (user_list[0])
            if (!user_list.empty() && me == user_list[0]) {
                set_host_status(true);
            } else {
                set_host_status(false);
            }
            // Log cheat information for all players in the room
            for (auto& user : user_list) {
                if (user) {
                    log_player_cheat_info(user);
                }
            }
            // Compare all players' save hashes after accepting into room
            compare_all_players_save_hashes();
            compare_all_players_cheat_file_hashes();
            break;
        }

        case PATH: {
            path = p.read<string>();
            my_dialog->info(
                "Others may join with the following command:\r\n\r\n"
                "/join " + (host == "127.0.0.1" ? (external_address.is_unspecified() ? "<Your IP>" : external_address.to_string()) : host) + (port == 6400 ? "" : ":" + to_string(port)) + (path == "/" ? "" : path) + "\r\n"
            );
            break;
        }

        case PING: {
            packet pong;
            pong << PONG;
            while (p.available()) {
                pong << p.read<uint8_t>();
            }
            if (udp) {
                send_udp(pong);
            } else {
                send(pong);
            }
            break;
        }

        case PONG: {
            if (udp && !udp_established) {
                udp_established = true;
                tcp_socket->set_option(ip::tcp::no_delay(false));
            }
            break;
        }

        case QUIT: {
            remove_user(p.read<uint32_t>());
            break;
        }

        case NAME: {
            auto user = user_map.at(p.read<uint32_t>());
            auto name = p.read<string>();
            my_dialog->info(user->name + " is now " + name);
            user->name = name;
            update_user_list();
            break;
        }

        case LATENCY: {
            for (auto& u : user_list) {
                u->latency = p.read<double>();
            }
            update_user_list();
            break;
        }

        case MESSAGE: {
            auto user_id = p.read<uint32_t>();
            auto message = p.read<string>();
            message_received(user_id, message);
            break;
        }

        case LAG: {
            auto lag = p.read<uint8_t>();
            auto source_id = p.read<uint32_t>();
            while (p.available()) {
                user_map.at(p.read<uint32_t>())->lag = lag;
            }
            update_user_list();
            break;
        }

        case CONTROLLERS: {
            for (auto& u : user_list) {
                for (auto& c : u->controllers) {
                    p >> c;
                }
                p >> u->map;
            }

            if (!started) {
                for (int j = 0; j < 4; j++) {
                    controllers[j].Present = 1;
                    controllers[j].RawData = 0;
                    controllers[j].Plugin = PLUGIN_NONE;
                    for (int i = 0; i < 4; i++) {
                        for (auto& u : user_list) {
                            if (u->map.get(i, j)) {
                                controllers[j].Plugin = max(controllers[j].Plugin, u->controllers[i].plugin);
                            }
                        }
                    }
                }
            }

            update_user_list();
            break;
        }

        case START: {
            start_game();
            break;
        }

        case GOLF: {
            set_golf_mode(p.read<bool>());
            break;
        }

        case INPUT_MAP: {
            auto user = user_map.at(p.read<uint32_t>());
            if (!user) break;
            user->map = p.read<input_map>();
            update_user_list();
            break;
        }

        case INPUT_DATA: {
            while (p.available()) {
                auto user = user_map.at(p.read_var<uint32_t>());
                if (!user) continue;
                auto input_id = p.read_var<uint32_t>();
                packet pin;
                pin.transpose(p.read_rle(), input_data::SIZE);
                while (pin.available()) {
                    auto input = pin.read<input_data>();
                    if (!user->add_input_history(input_id++, input)) continue;
                    user->input_queue.push_back(input);
                    if (golf && me->authority == me->id && input_detected(input, golf_mode_mask)) {
                        change_input_authority(me->id, user->id);
                    }
                }
            }
            on_input();
            break;
        }

        case INPUT_UPDATE: {
            auto user = user_map.at(p.read<uint32_t>());
            if (!user) break;
            user->input = p.read<input_data>();
            break;
        }

        case REQUEST_AUTHORITY: {
            auto user = user_map.at(p.read<uint32_t>());
            auto authority = user_map.at(p.read<uint32_t>());
            if (!user || !authority) break;
            if (user->authority == me->id) {
                change_input_authority(user->id, authority->id);
            }
            break;
        }

        case DELEGATE_AUTHORITY: {
            auto user = user_map.at(p.read<uint32_t>());
            auto authority = user_map.at(p.read<uint32_t>());
            if (!user || !authority) break;
            user->authority = authority->id;
            if (user->authority == me->id) {
                user->input = user->pending;
                user->pending = input_data();
                send_input(*user);
                send_input(*user);
                on_input();
            }
            update_user_list();
            break;
        }
    }
}

void client::map_src_to_dst() {
    me->map = input_map::IDENTITY_MAP;
    for (int i = 0; i < 4; i++) {
        controllers[i].Plugin = me->controllers[i].plugin;
        controllers[i].Present = me->controllers[i].present;
        controllers[i].RawData = me->controllers[i].raw_data;
    }
}

void client::update_user_list() {
    vector<vector<string>> lines;
    lines.reserve(user_list.size());

    for (auto& u : user_list) {
        vector<string> line;

        line.push_back(to_string(u->id + 1));


        line.push_back(u->id == u->authority ? "" : to_string(u->authority + 1));

        line.push_back(u->name);

        for (int j = 0; j < 4; j++) {
            string m;
            for (int i = 0; i < 4; i++) {
                if (u->map.get(i, j)) {
                    if (!m.empty()) m += ",";
                    m += to_string(i + 1);
                    switch (u->controllers[i].plugin) {
                        case PLUGIN_MEMPAK: m += 'M'; break;
                        case PLUGIN_RUMBLE_PAK: m += 'R'; break;
                        case PLUGIN_TANSFER_PAK: m += 'T'; break;
                    }
                }
            }
            line.push_back(m);
        }


        line.push_back(to_string(u->lag));

        if (!isnan(u->latency)) {
            line.push_back(to_string((int)(u->latency * 1000)) + " ms");
        } else {
            line.push_back("");
        }

        lines.push_back(line);
    }

    my_dialog->update_user_list(lines);
}

void client::change_input_authority(uint32_t user_id, uint32_t authority_id) {
    auto user = user_map.at(user_id);
    if (user->authority == authority_id) return;

    if (user->authority == me->id) {
        user->authority = authority_id;
        send_delegate_authority(user->id, authority_id);
    } else {
        send_request_authority(user->id, authority_id);
    }
}

void client::set_input_map(input_map new_map) {
    if (me->map == new_map) return;

    me->map = new_map;
    send_input_map(new_map);

    update_user_list();
}

void client::on_tick() {
    if (!input_times.empty()) {
        send_input_rate((input_times.size() - 1) / (float)(input_times.back() - input_times.front()));
    }

    if (!udp_established) {
        send_udp_ping();
    }
    
    // Note: Desync detection now uses save state hashing every 1800 frames (1 minute at 30 FPS)
    // This is handled by the core emulator calling HandleNetplayDesyncDetection()

    timer.expires_after(500ms);
    auto self(weak_from_this());
    timer.async_wait([self, this](const error_code& error) { 
        if (self.expired()) return;
        if (!error) on_tick();
    });
}

void client::send_join(const string& room, uint16_t udp_port) {
    send(packet() << JOIN << PROTOCOL_VERSION << room << *me << udp_port);
}

void client::send_name() {
    send(packet() << NAME << me->name);
}

void client::send_message(const string& message) {
    send(packet() << MESSAGE << message);
}

void client::send_save_info() {
    packet p;
    p << SAVE_INFO;
    for (auto& save : me->saves) {
        p << save;
    }
    send(p);
}

void client::compare_all_players_save_hashes() {
    if (user_list.size() < 2) {
        return; // Need at least 2 players to compare
    }
    
    // Compare save hashes for each save slot across all players
    for (int slot = 0; slot < me->saves.size(); slot++) {
        std::map<std::string, std::vector<std::string>> hash_to_players; // hash -> list of player names
        int players_with_saves = 0;
        
        // Collect all players' hashes for this slot
        for (size_t i = 0; i < user_list.size(); i++) {
            if (!user_list[i]) continue;
            
            const auto& user = user_list[i];
            if (slot >= user->saves.size()) continue;
            
            const auto& save = user->saves[slot];
            std::string hash = save.sha1_data;
            std::string player_name = user->name;
            
            // Skip empty saves (no hash)
            if (hash.empty()) {
                continue;
            }
            
            players_with_saves++;
            hash_to_players[hash].push_back(player_name);
        }
        
        // Only compare if we have at least 2 players with saves for this slot
        if (players_with_saves < 2) {
            continue; // Not enough players with saves to compare
        }
        
        // Check if all non-empty saves have the same hash
        if (hash_to_players.size() > 1) {
            // Multiple different hashes found - mismatch!
            std::string mismatch_msg = "Save slot " + std::to_string(slot) + " hash mismatch:";
            for (const auto& hash_group : hash_to_players) {
                std::string players_str;
                for (size_t j = 0; j < hash_group.second.size(); j++) {
                    if (j > 0) players_str += ", ";
                    players_str += hash_group.second[j];
                }
                mismatch_msg += "\r\n  " + hash_group.first.substr(0, 16) + "... (" + players_str + ")";
            }
            my_dialog->error(mismatch_msg);
        } else if (hash_to_players.size() == 1) {
            // All players with saves have matching hashes
            std::string players_str;
            const auto& players = hash_to_players.begin()->second;
            for (size_t j = 0; j < players.size(); j++) {
                if (j > 0) players_str += ", ";
                players_str += players[j];
            }
        }
        // If hash_to_players is empty, all saves are empty (no mismatch, nothing to report)
    }
}

void client::compare_all_players_cheat_file_hashes() {
    if (user_list.size() < 2) {
        return; // Need at least 2 players to compare
    }
    
    std::map<std::string, std::vector<std::string>> hash_to_players; // hash -> list of player names
    int players_with_cheats = 0;
    
    // Collect all players' cheat file hashes
    for (size_t i = 0; i < user_list.size(); i++) {
        if (!user_list[i]) continue;
        
        const auto& user = user_list[i];
        std::string hash = user->cheat_file_hash;
        std::string player_name = user->name;
        
        // Skip empty cheat files (no hash)
        if (hash.empty()) {
            continue;
        }
        
        players_with_cheats++;
        hash_to_players[hash].push_back(player_name);
    }
    
    // Only compare if we have at least 2 players with cheat files
    if (players_with_cheats < 2) {
        return; // Not enough players with cheat files to compare
    }
    
    // Check if all non-empty cheat files have the same hash
    if (hash_to_players.size() > 1) {
        // Multiple different hashes found - mismatch!
        std::string game_name = me->rom.name.empty() ? "current game" : me->rom.name;
        std::string cheat_file_name = game_name + ".cht";
        std::string mismatch_msg = "Cheat file hash mismatch for " + cheat_file_name + ":";
        for (const auto& hash_group : hash_to_players) {
            std::string players_str;
            for (size_t j = 0; j < hash_group.second.size(); j++) {
                if (j > 0) players_str += ", ";
                players_str += hash_group.second[j];
            }
            mismatch_msg += "\r\n  " + hash_group.first.substr(0, 16) + "... (" + players_str + ")";
        }
        my_dialog->error(mismatch_msg);
    } else if (hash_to_players.size() == 1) {
        // All players with cheat files have matching hashes
        std::string players_str;
        const auto& players = hash_to_players.begin()->second;
        for (size_t j = 0; j < players.size(); j++) {
            if (j > 0) players_str += ", ";
            players_str += players[j];
        }
    }
    // If hash_to_players is empty, all cheat files are empty (no mismatch, nothing to report)
}

void client::log_player_cheat_info(std::shared_ptr<user_info> user) {
    if (!user) return;
    
    std::string player_name = user->name;
    std::string cheat_hash = user->cheat_file_hash;
    
    // If this is us (me), also load and log our enabled cheats
    if (user == me || user->id == me->id) {
        std::vector<cheat_info> cheats = load_cheats();
    }
}

void client::compare_all_players_state_hashes() {
    if (user_list.size() < 2) {
        return; // Need at least 2 players to compare
    }
    
    std::map<std::string, std::vector<std::string>> hash_to_players; // hash -> list of player names
    int players_with_state = 0;
    
    // Collect all players' state hashes
    for (size_t i = 0; i < user_list.size(); i++) {
        if (!user_list[i]) continue;
        
        const auto& user = user_list[i];
        std::string hash = user->state_hash;
        std::string player_name = user->name;
        
        // Skip empty state hashes (game not started or hash not calculated yet)
        if (hash.empty()) {
            continue;
        }
        
        players_with_state++;
        hash_to_players[hash].push_back(player_name);
    }
    
    // Only compare if we have at least 2 players with state hashes
    if (players_with_state < 2) {
        return; // Not enough players with state to compare
    }
    
    // Check if all non-empty state hashes have the same hash
    if (hash_to_players.size() > 1) {
        // Multiple different hashes found - DESYNC DETECTED!
        std::string mismatch_msg = "DESYNC DETECTED! Emulator states do not match:\r\n";
        mismatch_msg += "Players have diverged and are no longer synchronized.\r\n";
        mismatch_msg += "This usually indicates:\r\n";
        mismatch_msg += "- Different inputs were processed\r\n";
        mismatch_msg += "- Timing differences\r\n";
        mismatch_msg += "- Emulator differences\r\n";
        mismatch_msg += "- Network issues\r\n\r\n";
        mismatch_msg += "Player groups with different states:";
        for (const auto& hash_group : hash_to_players) {
            std::string players_str;
            for (size_t j = 0; j < hash_group.second.size(); j++) {
                if (j > 0) players_str += ", ";
                players_str += hash_group.second[j];
            }
            mismatch_msg += "\r\n  " + hash_group.first.substr(0, 16) + "... (" + players_str + ")";
        }
        mismatch_msg += "\r\n\r\nYou may need to restart the netplay session.";
        my_dialog->error(mismatch_msg);
    }
    // If hash_to_players.size() == 1, all players are in sync (good!)
    // If hash_to_players is empty, no players have state hashes yet (game not started)
}

void client::compare_all_players_desync_hashes()
{
    if (user_list.size() < 2) {
        return; // Need at least 2 players to compare
    }

    std::map<std::string, std::vector<std::string>> hash_to_players; // hash -> list of player names
    int players_with_desync_hash = 0;

    // Collect all players' desync hashes
    for (size_t i = 0; i < user_list.size(); i++) {
        if (!user_list[i]) continue;

        std::shared_ptr<user_info> user = user_list[i];
        std::string hash = user->desync_hash;
        std::string player_name = user->name;

        // Skip empty desync hashes (not calculated yet)
        if (hash.empty()) {
            continue;
        }

        players_with_desync_hash++;
        hash_to_players[hash].push_back(player_name);
    }

    // Only compare if we have at least 2 players with desync hashes
    if (players_with_desync_hash < 2) {
        return; // Not enough players with desync hashes to compare
    }

    // Check if all non-empty desync hashes have the same hash
    if (hash_to_players.size() > 1) {
        // Multiple different hashes found - DESYNC DETECTED!
        std::string mismatch_msg = "DESYNC DETECTED! Save state hashes do not match:\r\n";
        mismatch_msg += "Players have diverged and are no longer synchronized.\r\n";
        mismatch_msg += "This usually indicates:\r\n";
        mismatch_msg += "- Different inputs were processed\r\n";
        mismatch_msg += "- Different timing or lag\r\n";
        mismatch_msg += "- Emulator state corruption\r\n";

        for (const auto& hash_group : hash_to_players) {
            std::string players_str;
            for (size_t j = 0; j < hash_group.second.size(); j++) {
                if (j > 0) players_str += ", ";
                players_str += hash_group.second[j];
            }
            mismatch_msg += "\r\n  " + hash_group.first.substr(0, 16) + "... (" + players_str + ")";
        }
        mismatch_msg += "\r\n\r\nYou may need to restart the netplay session.";
        my_dialog->error(mismatch_msg);

    }
    // If hash_to_players.size() == 1, all players are in sync (good!)
    // If hash_to_players is empty, no players have desync hashes yet
}

void client::send_savesync() {
    if (!is_host()) {
        my_dialog->error("Only the host can initiate save sync");
        return;
    }

    if (user_map.empty() || !user_map[0]) {
        my_dialog->error("Cannot sync saves: host user not found");
        return;
    }

    my_dialog->info("Syncing saves...");

    packet p;
    p << SAVE_SYNC;
    auto host_user = user_map[0];

    int save_count = 0;
    for (auto& save : host_user->saves) {
        // Skip saves that are too large for the packet
        if (save.save_data.size() > (packet::MAX_SIZE / 2)) {  // Leave room for other packet data
            my_dialog->error("Save file too large for network sync: " + save.save_name +
                           " (" + std::to_string(save.save_data.size()) + " bytes), skipping");
            // Send empty save to maintain packet structure
            save_info empty_save;
            p << empty_save;
            continue;
        }

        p << save; // Send the host's saves
        if (!save.save_name.empty() && !save.save_data.empty()) {
            save_count++;
        }
    }

    // Send cheat file content
    std::string cheat_file_content = "";
    std::string enabled_file_content = "";

    try {
        // Read cheat file content
        std::string cheat_file_path = get_cheat_file_path();
        if (std::filesystem::exists(cheat_file_path)) {
            std::ifstream cheat_file(cheat_file_path, std::ios::binary);
            if (cheat_file.is_open()) {
                std::stringstream buffer;
                buffer << cheat_file.rdbuf();
                cheat_file_content = buffer.str();
                cheat_file.close();
            }
        }

        // Read enabled file content (same as cheat file for enabled status)
        std::string enabled_file_path = get_cheat_enabled_file_path();
        if (std::filesystem::exists(enabled_file_path)) {
            std::ifstream enabled_file(enabled_file_path, std::ios::binary);
            if (enabled_file.is_open()) {
                std::stringstream buffer;
                buffer << enabled_file.rdbuf();
                enabled_file_content = buffer.str();
                enabled_file.close();
            }
        }
    } catch (const std::exception& e) {
        my_dialog->error("Error reading cheat files: " + std::string(e.what()));
    }

    p << cheat_file_content;
    p << enabled_file_content;



    send(p);
}

void client::send_controllers() {
    packet p;
    p << CONTROLLERS;
    for (auto& c : me->controllers) {
        p << c;
    }
    send(p);
}

void client::send_start_game() {
    send(packet() << START);
}

void client::send_lag(uint8_t lag, bool my_lag, bool your_lag) {
    send(packet() << LAG << lag << my_lag << your_lag);
}

void client::send_autolag(int8_t value) {
    send(packet() << AUTOLAG << value);
}

void client::send_input(user_info& user) {
    user.add_input_history(user.input_id, user.input);
    user.input_queue.push_back(user.input);

    if (!is_open()) return;

    if (udp_established) {
        packet p;
        p << INPUT_DATA;
        p.write_var(user.id);
        p.write_var(user.input_id - user.input_history.size());
        p.write_rle(packet() << user.input_history);
        send_udp(p, false);
    }

    packet p;
    p << INPUT_DATA;
    p.write_var(user.id);
    p.write_var(user.input_id - 1);
    p.write_rle(packet() << user.input_history.back());
    send(p, false);
}

void client::send_input_update(const input_data& input) {
    if (udp_established) {
        send_udp(packet() << INPUT_UPDATE << input);
    } else {
        send(packet() << INPUT_UPDATE << input);
    }
}

void client::send_input_map(input_map map) {
    send(packet() << INPUT_MAP << map);
}

void client::send_input_rate(float rate) {
    send(packet() << INPUT_RATE << rate);
}

void client::send_udp_ping() {
    send_udp(packet() << PING << timestamp());
}

void client::send_request_authority(uint32_t user_id, uint32_t authority_id) {
    send(packet() << REQUEST_AUTHORITY << user_id << authority_id);
}

void client::send_delegate_authority(uint32_t user_id, uint32_t authority_id) {
    send(packet() << DELEGATE_AUTHORITY << user_id << authority_id);
}

void client::revert_save_data() {
    std::string original_dir = save_path + "Original\\";
    std::string netplay_temp_dir = save_path + "NetplayTemp\\";

    for (auto& save : me->saves) {
        if (!save.save_name.empty()) {
            try {
                std::string netplay_path = save_path + save.save_name;
                std::string temp_path = netplay_temp_dir + save.save_name;
                std::string original_path = original_dir + save.save_name;

                // Only revert if we have a backup (non-hosts have backups, hosts don't)
                if (!std::filesystem::exists(original_path)) {
                    continue;
                }

                // Move current (netplay) save to NetplayTemp if it exists
                if (std::filesystem::exists(netplay_path)) {
                    // Ensure NetplayTemp directory exists
                    std::filesystem::create_directories(netplay_temp_dir);
                    std::filesystem::rename(netplay_path, temp_path);
                }

                // Restore original save from backup
                std::filesystem::rename(original_path, netplay_path);
            } catch (const std::filesystem::filesystem_error& e) {
                my_dialog->error("Failed to revert save " + save.save_name + ": " + e.what());
            }
        }
    }
}

void client::ensure_save_directories() {
    // Create save backup directories
    std::string original_dir = save_path + "Original\\";
    std::string temp_dir = save_path + "NetplayTemp\\";

    std::filesystem::create_directories(original_dir);
    std::filesystem::create_directories(temp_dir);
}

void client::restore_leftover_backups() {
    // Check for leftover backups from a previous session that was force-closed (Alt+F4)
    // If any exist in the Original folder, restore them now
    std::string original_dir = save_path + "Original\\";
    std::string netplay_temp_dir = save_path + "NetplayTemp\\";

    try {
        if (std::filesystem::exists(original_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(original_dir)) {
                if (!entry.is_regular_file()) continue;
                
                std::string filename = entry.path().filename().string();
                std::string ext = entry.path().extension().string();
                
                // Process save files (.sra, .eep, .fla, .mpk)
                if (ext == ".sra" || ext == ".eep" || ext == ".fla" || ext == ".mpk") {
                    std::string backup_path = entry.path().string();
                    std::string main_path = save_path + filename;
                    std::string temp_path = netplay_temp_dir + filename;

                    my_dialog->info("Restoring leftover backup: " + filename);

                    // Move current (netplay) save to NetplayTemp if it exists
                    if (std::filesystem::exists(main_path)) {
                        std::filesystem::create_directories(netplay_temp_dir);
                        std::filesystem::rename(main_path, temp_path);
                    }

                    // Restore the backup
                    std::filesystem::rename(backup_path, main_path);
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        my_dialog->error("Failed to restore leftover backups: " + std::string(e.what()));
    }

}


std::string client::get_config_path() const {
    std::string config_path = save_path;
    if (config_path.length() >= 5) {
        std::string last_five = config_path.substr(config_path.length() - 5);
        if (last_five == "Save\\") {
            config_path = config_path.substr(0, config_path.length() - 5);
        }
    }
    if (config_path.length() >= 5) {
        std::string last_five = config_path.substr(config_path.length() - 5);
        if (last_five != "User\\") {
            config_path += "User\\";
        }
    } else {
        config_path += "User\\";
    }
    return config_path;
}


void client::move_original_saves_to_temp() {
    std::string original_dir = save_path + "Original\\";
    
    // Ensure the Original directory exists
    try {
        std::filesystem::create_directories(original_dir);
    } catch (const std::filesystem::filesystem_error& e) {
        my_dialog->error("Failed to create Original directory: " + std::string(e.what()));
        return;
    }

    for (auto& save : me->saves) {
        if (!save.save_name.empty()) {
            try {
                std::string original_path = save_path + save.save_name;
                std::string backup_path = original_dir + save.save_name;

                // Only backup if the original save exists and we don't already have a backup
                // (prevents overwriting backup during reconnect scenarios)
                if (std::filesystem::exists(original_path) && !std::filesystem::exists(backup_path)) {
                    std::filesystem::copy(original_path, backup_path, std::filesystem::copy_options::overwrite_existing);
                }
            } catch (const std::filesystem::filesystem_error& e) {
                my_dialog->error("Failed to backup save " + save.save_name + ": " + e.what());
            }
        }
    }
}

bool client::is_host() const {
    return host_status;
}

void client::set_host_status(bool status) {
    host_status = status;
}

std::string client::get_game_identifier() const {
    char identifier[100];
    sprintf_s(identifier, sizeof(identifier), "%08X-%08X-C:%X", me->rom.crc1, me->rom.crc2, (unsigned char)me->rom.country_code);
    return std::string(identifier);
}

std::string client::sanitize_filename(const std::string& filename) const {
    std::string sanitized = filename;
    
    // Remove or replace invalid filename characters
    const std::string invalid_chars = "<>:\"/\\|?*";
    for (char& c : sanitized) {
        if (invalid_chars.find(c) != std::string::npos) {
            c = '_'; // Replace invalid chars with underscore
        }
    }
    
    // Remove leading/trailing spaces and dots
    while (!sanitized.empty() && (sanitized.front() == ' ' || sanitized.front() == '.')) {
        sanitized.erase(0, 1);
    }
    while (!sanitized.empty() && (sanitized.back() == ' ' || sanitized.back() == '.')) {
        sanitized.pop_back();
    }
    
    // If empty after sanitization, use a default
    if (sanitized.empty()) {
        sanitized = "Unknown";
    }
    
    // Limit length to avoid filesystem issues
    if (sanitized.length() > 200) {
        sanitized = sanitized.substr(0, 200);
    }
    
    return sanitized;
}

std::string client::get_cheat_file_path() const {
    std::string config_path = get_config_path();
    config_path += "Cheats\\";
    
    // Ensure Cheats directory exists
    std::filesystem::create_directories(config_path);
    
    // Use ROM's internal name for game-specific cheat file
    if (!me->rom.name.empty()) {
        std::string rom_name = sanitize_filename(me->rom.name);
        config_path += rom_name + ".cht";
    } else {
        // Fallback to game identifier if name is empty
        config_path += get_game_identifier() + ".cht";
    }
    
    return config_path;
}

std::string client::get_cheat_enabled_file_path() const {
    std::string config_path = get_config_path();
    config_path += "Cheats\\";
    
    // Ensure Cheats directory exists
    std::filesystem::create_directories(config_path);
    
    // Use ROM's internal name for game-specific cheat enabled file (same as cheat file)
    if (!me->rom.name.empty()) {
        std::string rom_name = sanitize_filename(me->rom.name);
        config_path += rom_name + ".cht";
    } else {
        // Fallback to game identifier if name is empty
        config_path += get_game_identifier() + ".cht";
    }
    
    return config_path;
}

std::vector<cheat_info> client::load_cheats() {
    std::vector<cheat_info> cheats;
    
    try {
        std::string cheat_file = get_cheat_file_path();
        std::string game_id = get_game_identifier();
        std::wstring wcheat_file = utf8_to_wstring(cheat_file);
        std::wstring wgame_id = utf8_to_wstring(game_id);

        // Check if cheat file exists
        if (GetFileAttributes(wcheat_file.c_str()) == INVALID_FILE_ATTRIBUTES) {
            my_dialog->info("Cheat file does not exist: " + cheat_file);
            return cheats; // File doesn't exist, return empty
        }

        // Read up to MaxCheats cheats (50000)
        for (int i = 0; i < 50000; i++) {
            std::wstring key = L"Cheat" + std::to_wstring(i);
            
            wchar_t cheat_entry[4096];
            DWORD len = GetPrivateProfileString(wgame_id.c_str(), key.c_str(), L"", cheat_entry, sizeof(cheat_entry) / sizeof(wchar_t), wcheat_file.c_str());
            
            if (len == 0) {
                break; // No more cheats
            }

            std::string entry = wstring_to_utf8(cheat_entry);
            if (entry.empty()) {
                continue;
            }

            // Parse cheat entry: format is "Name" code1,code2,code3,...
            // Find the name between quotes
            size_t name_start = entry.find('"');
            if (name_start == std::string::npos) {
                continue;
            }
            size_t name_end = entry.find('"', name_start + 1);
            if (name_end == std::string::npos) {
                continue;
            }

            cheat_info cheat;
            cheat.name = entry.substr(name_start + 1, name_end - name_start - 1);
            
            // Get the code part (after the closing quote and space)
            size_t code_start = name_end + 1;
            while (code_start < entry.length() && (entry[code_start] == ' ' || entry[code_start] == '\t')) {
                code_start++;
            }
            cheat.code = entry.substr(code_start);

            // Check if cheat is active - read from .cht_enabled file
            // Wrap in try-catch in case file is locked or corrupted
            try {
                std::string enabled_file = get_cheat_enabled_file_path();
                std::wstring wenabled_file = utf8_to_wstring(enabled_file);
                std::wstring active_key = L"Cheat" + std::to_wstring(i);
                wchar_t active_value[16];
                GetPrivateProfileString(wgame_id.c_str(), active_key.c_str(), L"0", active_value, sizeof(active_value) / sizeof(wchar_t), wenabled_file.c_str());
                cheat.active = (_wtoi(active_value) != 0);
            } catch (...) {
                cheat.active = false; // Default to inactive if we can't read it
            }

            cheats.push_back(cheat);
        }
    } catch (const std::exception& e) {
        my_dialog->error("Error loading cheats: " + std::string(e.what()));
        return cheats; // Return whatever we loaded so far
    } catch (...) {
        my_dialog->error("Unknown error loading cheats");
        return cheats; // Return whatever we loaded so far
    }

    return cheats;
}

void client::save_cheats(const std::vector<cheat_info>& cheats) {
    std::string cheat_file = get_cheat_file_path();
    std::string enabled_file = get_cheat_enabled_file_path();
    std::string game_id = get_game_identifier();
    std::wstring wcheat_file = utf8_to_wstring(cheat_file);
    std::wstring wenabled_file = utf8_to_wstring(enabled_file);
    std::wstring wgame_id = utf8_to_wstring(game_id);

    // Ensure Config directory exists
    std::string config_dir = get_config_path();
    std::filesystem::create_directories(utf8_to_wstring(config_dir));

    // Remove write protection from cheat file if it exists
    DWORD cheat_attrs = GetFileAttributes(wcheat_file.c_str());
    if (cheat_attrs != INVALID_FILE_ATTRIBUTES) {
        if (cheat_attrs & FILE_ATTRIBUTE_READONLY) {
            SetFileAttributes(wcheat_file.c_str(), cheat_attrs & ~FILE_ATTRIBUTE_READONLY);
        }
    }

    // Remove write protection from enabled file if it exists
    DWORD enabled_attrs = GetFileAttributes(wenabled_file.c_str());
    if (enabled_attrs != INVALID_FILE_ATTRIBUTES) {
        if (enabled_attrs & FILE_ATTRIBUTE_READONLY) {
            SetFileAttributes(wenabled_file.c_str(), enabled_attrs & ~FILE_ATTRIBUTE_READONLY);
        }
    }

    // First, clear existing cheats for this game in cheat file
    for (int i = 0; i < 50000; i++) {
        std::wstring key = L"Cheat" + std::to_wstring(i);
        wchar_t value[4096];
        DWORD len = GetPrivateProfileString(wgame_id.c_str(), key.c_str(), L"", value, sizeof(value) / sizeof(wchar_t), wcheat_file.c_str());
        if (len == 0) {
            break;
        }
        WritePrivateProfileString(wgame_id.c_str(), key.c_str(), NULL, wcheat_file.c_str());
    }

    // Write new cheats to Project64.cht (replacing existing ones)
    for (size_t i = 0; i < cheats.size(); i++) {
        const auto& cheat = cheats[i];
        std::wstring key = L"Cheat" + std::to_wstring((int)i);
        
        // Format: "Name" code
        std::string entry = "\"" + cheat.name + "\" " + cheat.code;
        std::wstring wentry = utf8_to_wstring(entry);
        WritePrivateProfileString(wgame_id.c_str(), key.c_str(), wentry.c_str(), wcheat_file.c_str());
    }

    // Write enabled status to Project64.cht_enabled file
    for (size_t i = 0; i < cheats.size(); i++) {
        const auto& cheat = cheats[i];
        std::wstring enabled_key = L"Cheat" + std::to_wstring((int)i);
        std::wstring enabled_value = cheat.active ? L"1" : L"0";
        WritePrivateProfileString(wgame_id.c_str(), enabled_key.c_str(), enabled_value.c_str(), wenabled_file.c_str());
    }
}

void client::apply_cheats(const std::string& cheat_file_content, const std::string& enabled_file_content) {
    // Queue cheat application to happen asynchronously to avoid blocking network operations
    service.post([this, cheat_file_content, enabled_file_content]() {
        try {
            apply_cheats_async(cheat_file_content, enabled_file_content);
        } catch (const std::exception& e) {
            my_dialog->error("Error applying cheats asynchronously: " + std::string(e.what()));
        }
    });
}

void client::apply_cheats_async(const std::string& cheat_file_content, const std::string& enabled_file_content) {
    HMODULE hModule = GetModuleHandle(NULL);

    // For non-host clients (p2-4), try to write cheats directly to memory first
    if (!is_host()) {
        // Get game identifier
        std::string game_id = get_game_identifier();

        // Apply cheats directly to memory if function is available
        if (hModule) {
            typedef void(__cdecl* ApplyCheatsDirectlyFunc)(const char*, const char*, const char*);
            ApplyCheatsDirectlyFunc applyCheatsDirectly = (ApplyCheatsDirectlyFunc)GetProcAddress(hModule, "ApplyCheatsDirectlyForNetplay");
            if (applyCheatsDirectly) {
                // Try to apply cheats directly to memory - they'll be loaded into m_Codes and applied every frame automatically
                applyCheatsDirectly(cheat_file_content.c_str(), enabled_file_content.c_str(), game_id.c_str());
                my_dialog->info("Cheats applied directly to memory from host (" + std::to_string(cheat_file_content.length()) + " bytes cheat data)");
                return;
            } else {
                my_dialog->info("ApplyCheatsDirectlyForNetplay function not found, falling back to file method");
            }
        }
    }

    // Host (p1) or fallback: write to .ini files and reload (original behavior)
    // First, close the INI file handle so we can write to it
    if (hModule) {
        typedef void(__cdecl* CloseCheatFileFunc)(void);
        CloseCheatFileFunc closeCheatFile = (CloseCheatFileFunc)GetProcAddress(hModule, "CloseCheatFileForNetplay");
        if (closeCheatFile) {
            closeCheatFile();
            // Reduced sleep time to minimize blocking
            Sleep(50);
        }
    }

    bool cheat_file_written = false;
    bool enabled_file_written = false;

    try {
        // Write the entire .cht file content
        std::string cheat_file = get_cheat_file_path();

        // Ensure parent directory exists
        std::filesystem::path cheat_path(cheat_file);
        std::filesystem::create_directories(cheat_path.parent_path());

        // Write the entire .cht file content (creates file if it doesn't exist)
        std::ofstream cheat_of(cheat_file.c_str(), std::ofstream::binary | std::ofstream::trunc);
        if (cheat_of.is_open()) {
            cheat_of << cheat_file_content;
            cheat_of.flush();
            cheat_of.close();
            cheat_file_written = true;
        } else {
            DWORD error = GetLastError();
            my_dialog->error("Failed to open cheat file for writing: " + cheat_file + " (Error: " + std::to_string(error) + ")");
        }

        // Write the entire .cht_enabled file content only if we have content
        if (!enabled_file_content.empty()) {
            std::string enabled_file = get_cheat_enabled_file_path();

            // Ensure parent directory exists
            std::filesystem::path enabled_path(enabled_file);
            std::filesystem::create_directories(enabled_path.parent_path());

            // Write the entire file content (creates file if it doesn't exist)
            std::ofstream enabled_of(enabled_file.c_str(), std::ofstream::binary | std::ofstream::trunc);
            if (enabled_of.is_open()) {
                enabled_of << enabled_file_content;
                enabled_of.flush();
                enabled_of.close();
                enabled_file_written = true;
            } else {
                DWORD error = GetLastError();
                my_dialog->error("Failed to open cheat enabled file for writing: " + enabled_file + " (Error: " + std::to_string(error) + ")");
            }
        }

        if (cheat_file_written) {
            my_dialog->info("Cheat file synced from host (" + std::to_string(cheat_file_content.length()) + " bytes)");
        }
        if (enabled_file_written) {
            my_dialog->info("Cheat enabled file synced from host (" + std::to_string(enabled_file_content.length()) + " bytes)");
        }

        // Trigger Project64 core to reload cheats from the files
        // Use force reload for complete cache clearing and full file re-scan
        if (hModule && (cheat_file_written || enabled_file_written)) {
            typedef void(__cdecl* TriggerForceCheatReloadFunc)(void);
            TriggerForceCheatReloadFunc triggerForceCheatReload = (TriggerForceCheatReloadFunc)GetProcAddress(hModule, "TriggerForceCheatReloadForNetplay");
            if (triggerForceCheatReload) {
                triggerForceCheatReload();
                my_dialog->info("Triggered force cheat reload after writing files");
            } else {
                // Fallback to regular reload
                typedef void(__cdecl* TriggerCheatReloadFunc)(void);
                TriggerCheatReloadFunc triggerCheatReload = (TriggerCheatReloadFunc)GetProcAddress(hModule, "TriggerCheatReloadForNetplay");
                if (triggerCheatReload) {
                    triggerCheatReload();
                    my_dialog->info("Triggered cheat reload after writing files (fallback)");
                } else {
                    my_dialog->info("Could not find cheat reload function (cheats may need manual refresh)");
                }
            }

            // Give Project64 core minimal time to process the file changes
            // Reduced from 1000ms to 200ms to prevent network timeouts
            Sleep(200);
        }

        my_dialog->info("Cheats synced from host");

    } catch (const std::exception& e) {
        my_dialog->error("Error during cheat file sync: " + std::string(e.what()));
    }
}
