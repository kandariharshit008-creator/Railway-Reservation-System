#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Train {
    int number;
    std::string name;
    std::string source;
    std::string destination;
    std::string departure;
    int capacity;
};

struct Reservation {
    int pnr;
    int trainNumber;
    std::string passenger;
    int age;
    int seats;
};

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
};

const std::string DATA_FILE = "reservations.txt";
std::string PROJECT_DIRECTORY = ".";
const std::vector<Train> TRAINS = {
    {12001, "Coastal Express", "Mumbai", "Goa", "06:30", 5},
    {12002, "Deccan Queen", "Pune", "Mumbai", "07:15", 5},
    {12003, "Northern Star", "Delhi", "Jaipur", "09:00", 5},
    {12004, "Western Link", "Mumbai", "Ahmedabad", "13:20", 5},
    {12005, "Capital Express", "Delhi", "Lucknow", "15:10", 5},
    {12006, "Southern Star", "Chennai", "Bengaluru", "17:45", 5},
    {12007, "Desert Queen", "Jaipur", "Jodhpur", "18:25", 5},
    {12008, "Eastern Mail", "Kolkata", "Patna", "20:05", 5}
};

std::string jsonEscape(const std::string& value) {
    std::string escaped;
    for (unsigned char character : value) {
        switch (character) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (character >= 0x20) escaped += static_cast<char>(character);
        }
    }
    return escaped;
}

std::string urlDecode(const std::string& value) {
    std::string decoded;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '+') {
            decoded += ' ';
        } else if (value[index] == '%' && index + 2 < value.size()) {
            try {
                decoded += static_cast<char>(std::stoi(value.substr(index + 1, 2), nullptr, 16));
                index += 2;
            } catch (const std::exception&) {
                decoded += value[index];
            }
        } else {
            decoded += value[index];
        }
    }
    return decoded;
}

std::vector<Reservation> loadReservations() {
    std::vector<Reservation> reservations;
    std::ifstream file(PROJECT_DIRECTORY + "/" + DATA_FILE);
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream fields(line);
        std::string pnr, trainNumber, passenger, age, seats;
        if (std::getline(fields, pnr, '|')
            && std::getline(fields, trainNumber, '|')
            && std::getline(fields, passenger, '|')
            && std::getline(fields, age, '|')
            && std::getline(fields, seats, '|')) {
            try {
                reservations.push_back({std::stoi(pnr), std::stoi(trainNumber),
                                         passenger, std::stoi(age), std::stoi(seats)});
            } catch (const std::exception&) {
                continue;
            }
        }
    }
    return reservations;
}

bool saveReservations(const std::vector<Reservation>& reservations) {
    std::ofstream file(PROJECT_DIRECTORY + "/" + DATA_FILE, std::ios::trunc);
    if (!file) return false;
    for (const Reservation& reservation : reservations) {
        file << reservation.pnr << '|' << reservation.trainNumber << '|'
             << reservation.passenger << '|' << reservation.age << '|'
             << reservation.seats << '\n';
    }
    return static_cast<bool>(file);
}

const Train* findTrain(int number) {
    const auto found = std::find_if(TRAINS.begin(), TRAINS.end(),
        [number](const Train& train) { return train.number == number; });
    return found == TRAINS.end() ? nullptr : &*found;
}

int bookedSeats(int trainNumber, const std::vector<Reservation>& reservations) {
    int total = 0;
    for (const Reservation& reservation : reservations) {
        if (reservation.trainNumber == trainNumber) total += reservation.seats;
    }
    return total;
}

std::string trainsJson(const std::vector<Reservation>& reservations) {
    std::ostringstream json;
    json << '[';
    for (std::size_t index = 0; index < TRAINS.size(); ++index) {
        const Train& train = TRAINS[index];
        if (index != 0) json << ',';
        json << "{\"number\":" << train.number
             << ",\"name\":\"" << jsonEscape(train.name)
             << "\",\"source\":\"" << jsonEscape(train.source)
             << "\",\"destination\":\"" << jsonEscape(train.destination)
             << "\",\"departure\":\"" << train.departure
             << "\",\"capacity\":" << train.capacity
             << ",\"available\":" << train.capacity - bookedSeats(train.number, reservations)
             << '}';
    }
    json << ']';
    return json.str();
}

std::string reservationsJson(const std::vector<Reservation>& reservations) {
    std::ostringstream json;
    json << '[';
    for (std::size_t index = 0; index < reservations.size(); ++index) {
        const Reservation& reservation = reservations[index];
        const Train* train = findTrain(reservation.trainNumber);
        if (index != 0) json << ',';
        json << "{\"pnr\":" << reservation.pnr
             << ",\"trainNumber\":" << reservation.trainNumber
             << ",\"passenger\":\"" << jsonEscape(reservation.passenger)
             << "\",\"age\":" << reservation.age
             << ",\"seats\":" << reservation.seats
             << ",\"trainName\":\"" << (train ? jsonEscape(train->name) : "Unknown train")
             << "\",\"route\":\"" << (train ? jsonEscape(train->source + " to " + train->destination) : "")
             << "\",\"departure\":\"" << (train ? train->departure : "")
             << "\"}";
    }
    json << ']';
    return json.str();
}

bool parseRequest(SOCKET client, HttpRequest& request) {
    std::string data;
    char buffer[4096];
    std::size_t headerEnd = std::string::npos;
    std::size_t contentLength = 0;
    while (headerEnd == std::string::npos || data.size() < headerEnd + 4 + contentLength) {
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) return false;
        data.append(buffer, received);
        if (headerEnd == std::string::npos) {
            headerEnd = data.find("\r\n\r\n");
            if (headerEnd != std::string::npos) {
                std::istringstream headers(data.substr(0, headerEnd));
                std::string line;
                std::getline(headers, line);
                std::istringstream firstLine(line);
                firstLine >> request.method >> request.path;
                while (std::getline(headers, line)) {
                    std::transform(line.begin(), line.end(), line.begin(),
                        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
                    if (line.find("content-length:") == 0) {
                        contentLength = std::stoul(line.substr(15));
                    }
                }
            }
        }
    }
    request.body = data.substr(headerEnd + 4, contentLength);
    return true;
}

void sendResponse(SOCKET client, int status, const std::string& statusText,
                  const std::string& contentType, const std::string& body) {
    std::ostringstream response;
    response << "HTTP/1.1 " << status << ' ' << statusText << "\r\n"
             << "Content-Type: " << contentType << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Connection: close\r\n"
             << "X-Content-Type-Options: nosniff\r\n\r\n"
             << body;
    const std::string message = response.str();
    std::size_t sent = 0;
    while (sent < message.size()) {
        const int result = send(client, message.data() + sent,
                                static_cast<int>(message.size() - sent), 0);
        if (result <= 0) return;
        sent += result;
    }
}

std::string mimeType(const std::string& path) {
    if (path == "web/style.css") return "text/css; charset=utf-8";
    if (path == "web/app.js") return "application/javascript; charset=utf-8";
    return "text/html; charset=utf-8";
}

void handleRequest(SOCKET client, const HttpRequest& request,
                   std::vector<Reservation>& reservations) {
    if (request.method == "GET" && request.path == "/api/trains") {
        sendResponse(client, 200, "OK", "application/json; charset=utf-8", trainsJson(reservations));
        return;
    }
    if (request.method == "GET" && request.path == "/api/reservations") {
        sendResponse(client, 200, "OK", "application/json; charset=utf-8", reservationsJson(reservations));
        return;
    }
    if (request.method == "POST" && request.path == "/api/book") {
        std::istringstream fields(request.body);
        std::string field;
        int trainNumber = 0, age = 0, seats = 0;
        std::string passenger;
        try {
            while (std::getline(fields, field, '&')) {
                const std::size_t equals = field.find('=');
                if (equals == std::string::npos) continue;
                const std::string key = urlDecode(field.substr(0, equals));
                const std::string value = urlDecode(field.substr(equals + 1));
                if (key == "trainNumber") trainNumber = std::stoi(value);
                else if (key == "passenger") passenger = value;
                else if (key == "age") age = std::stoi(value);
                else if (key == "seats") seats = std::stoi(value);
            }
        } catch (const std::exception&) {
            sendResponse(client, 400, "Bad Request", "application/json; charset=utf-8",
                         "{\"error\":\"Enter valid booking details.\"}");
            return;
        }
        const Train* train = findTrain(trainNumber);
        const int available = train ? train->capacity - bookedSeats(trainNumber, reservations) : 0;
        if (!train || passenger.empty() || passenger.find_first_of("|\r\n") != std::string::npos
            || age < 1 || age > 120 || seats < 1 || seats > available) {
            sendResponse(client, 400, "Bad Request", "application/json; charset=utf-8",
                         "{\"error\":\"Invalid details or not enough seats available.\"}");
            return;
        }
        int nextPnr = 100001;
        for (const Reservation& reservation : reservations) {
            nextPnr = std::max(nextPnr, reservation.pnr + 1);
        }
        reservations.push_back({nextPnr, trainNumber, passenger, age, seats});
        if (!saveReservations(reservations)) {
            reservations.pop_back();
            sendResponse(client, 500, "Internal Server Error", "application/json; charset=utf-8",
                         "{\"error\":\"Could not save reservation data.\"}");
            return;
        }
        sendResponse(client, 201, "Created", "application/json; charset=utf-8",
                     "{\"pnr\":" + std::to_string(nextPnr) + "}");
        return;
    }

    const std::string prefix = "/api/reservations/";
    if (request.method == "DELETE" && request.path.compare(0, prefix.size(), prefix) == 0) {
        try {
            const int pnr = std::stoi(request.path.substr(prefix.size()));
            const auto found = std::find_if(reservations.begin(), reservations.end(),
                [pnr](const Reservation& reservation) { return reservation.pnr == pnr; });
            if (found == reservations.end()) {
                sendResponse(client, 404, "Not Found", "application/json; charset=utf-8",
                             "{\"error\":\"Reservation not found.\"}");
                return;
            }
            const Reservation cancelled = *found;
            reservations.erase(found);
            if (!saveReservations(reservations)) {
                reservations.push_back(cancelled);
                sendResponse(client, 500, "Internal Server Error", "application/json; charset=utf-8",
                             "{\"error\":\"Could not save cancellation.\"}");
                return;
            }
            sendResponse(client, 200, "OK", "application/json; charset=utf-8", "{\"ok\":true}");
        } catch (const std::exception&) {
            sendResponse(client, 400, "Bad Request", "application/json; charset=utf-8",
                         "{\"error\":\"Invalid PNR.\"}");
        }
        return;
    }

    if (request.method == "GET") {
        const std::string path = request.path == "/" ? "web/index.html"
            : request.path == "/style.css" ? "web/style.css"
            : request.path == "/app.js" ? "web/app.js" : "";
        if (!path.empty()) {
            std::ifstream file(PROJECT_DIRECTORY + "/" + path, std::ios::binary);
            if (file) {
                std::ostringstream content;
                content << file.rdbuf();
                sendResponse(client, 200, "OK", mimeType(path), content.str());
                return;
            }
        }
    }
    sendResponse(client, 404, "Not Found", "application/json; charset=utf-8",
                 "{\"error\":\"Not found.\"}");
}

int main(int argc, char* argv[]) {
    char executablePath[MAX_PATH] = {};
    const DWORD pathLength = GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
    if (pathLength == 0 || pathLength >= MAX_PATH) {
        std::cerr << "Could not locate the application files.\n";
        return 1;
    }
    const std::string fullExecutablePath(executablePath, pathLength);
    const std::size_t directoryEnd = fullExecutablePath.find_last_of("\\/");
    if (directoryEnd != std::string::npos) {
        PROJECT_DIRECTORY = fullExecutablePath.substr(0, directoryEnd);
    }

    WSADATA winsockData;
    if (WSAStartup(MAKEWORD(2, 2), &winsockData) != 0) {
        std::cerr << "Could not initialize Windows networking.\n";
        return 1;
    }
    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "Could not create server socket.\n";
        WSACleanup();
        return 1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(8080);
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR
        || listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Could not start server on port 8080. Is it already in use?\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    std::vector<Reservation> reservations = loadReservations();
    if (argc > 1 && std::string(argv[1]) == "--open-browser") {
        ShellExecuteA(nullptr, "open", "http://localhost:8080", nullptr, nullptr, SW_SHOWNORMAL);
    }
    std::cout << "Railway Reservation System running at http://localhost:8080\n"
              << "Press Ctrl+C to stop the server.\n";
    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        HttpRequest request;
        if (parseRequest(client, request)) handleRequest(client, request, reservations);
        shutdown(client, SD_SEND);
        closesocket(client);
    }
}
