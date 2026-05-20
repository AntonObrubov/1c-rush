#include <iostream>
#include <string>
#include <ctime>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include "httplib.h"      // Простой HTTP-сервер (заголовочный файл)
#include "json.hpp"       // Библиотека nlohmann/json (заголовочный файл)
#include "sqlite3.h"      // SQLite (только заголовок, sqlite3.o подключается при линковке)

// Удобный псевдоним для JSON
using json = nlohmann::json;

// =====================================================================
// ГЛОБАЛЬНАЯ ПЕРЕМЕННАЯ: указатель на базу данных SQLite
// =====================================================================
sqlite3* db = nullptr;

// =====================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: получение текущей даты и времени
// Формат: "YYYY-MM-DD HH:MM:SS"
// =====================================================================
std::string get_current_datetime() {
    // Получаем текущее время с помощью std::chrono
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    
    // Преобразуем в локальное время
    std::tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &now_time);   // Безопасная версия для Windows
#else
    localtime_r(&now_time, &local_tm);   // POSIX-версия
#endif
    
    // Форматируем строку
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
    return std::string(buffer);
}

// =====================================================================
// ФУНКЦИЯ: инициализация базы данных и создание таблиц
// Открывает (или создаёт) файл tickets.db, затем создаёт таблицы
// users и tickets, если они ещё не существуют.
// =====================================================================
void init_db() {
    // Открываем базу данных (файл создаётся автоматически, если его нет)
    int rc = sqlite3_open("tickets.db", &db);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(
            std::string("Не удалось открыть базу данных: ") + sqlite3_errmsg(db)
        );
    }
    std::cout << "[DB] База данных tickets.db открыта\n";

    // Включаем поддержку внешних ключей
    const char* pragma_sql = "PRAGMA foreign_keys = ON;";
    char* err_msg = nullptr;
    rc = sqlite3_exec(db, pragma_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "неизвестная ошибка";
        sqlite3_free(err_msg);
        throw std::runtime_error("Ошибка PRAGMA foreign_keys: " + error);
    }

    // SQL-запрос для создания таблицы пользователей
    const char* create_users_sql = 
        "CREATE TABLE IF NOT EXISTS users ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  email TEXT UNIQUE NOT NULL,"
        "  password TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(db, create_users_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "неизвестная ошибка";
        sqlite3_free(err_msg);
        throw std::runtime_error("Ошибка создания таблицы users: " + error);
    }
    std::cout << "[DB] Таблица users готова\n";

    // SQL-запрос для создания таблицы билетов
    const char* create_tickets_sql = 
        "CREATE TABLE IF NOT EXISTS tickets ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  user_id INTEGER NOT NULL,"
        "  event TEXT NOT NULL,"
        "  purchase_date TEXT NOT NULL"
        ");";

    rc = sqlite3_exec(db, create_tickets_sql, nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "неизвестная ошибка";
        sqlite3_free(err_msg);
        throw std::runtime_error("Ошибка создания таблицы tickets: " + error);
    }
    std::cout << "[DB] Таблица tickets готова\n";
}

// =====================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: установка заголовков JSON для ответа
// =====================================================================
void set_json_headers(httplib::Response& res) {
    res.set_header("Content-Type", "application/json");
    // Разрешаем кросс-доменные запросы (для удобства разработки)
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

// =====================================================================
// ВСПОМОГАТЕЛЬНАЯ ФУНКЦИЯ: отправка JSON-ответа с заданным статусом
// =====================================================================
void send_json(httplib::Response& res, int status, const json& body) {
    set_json_headers(res);
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

// =====================================================================
// ФУНКЦИЯ main: создание и запуск HTTP-сервера
// =====================================================================
int main() {

    #ifdef _WIN32
        system("chcp 65001 > nul"); // "> nul" убирает лишнее сообщение "Active code page: 65001"
    #endif

    try {
        // ---------------------------------------------------------------
        // 1. Инициализация базы данных
        // ---------------------------------------------------------------
        init_db();

        // ---------------------------------------------------------------
        // 2. Создание HTTP-сервера
        // ---------------------------------------------------------------
        httplib::Server svr;

        // ---------------------------------------------------------------
        // 3. Настройка раздачи статических файлов из папки ../static
        //    Путь "../static" отсчитывается от расположения server.exe
        // ---------------------------------------------------------------
        svr.set_mount_point("/", "../static");
        std::cout << "[STATIC] Раздача статики из ../static\n";

        // ---------------------------------------------------------------
        // 4. Обработчик: POST /api/register — регистрация пользователя
        // ---------------------------------------------------------------
        svr.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
            try {
                // Парсим JSON из тела запроса
                json body = json::parse(req.body);

                // Проверяем наличие полей email и password
                if (!body.contains("email") || !body.contains("password") ||
                    !body["email"].is_string() || !body["password"].is_string()) {
                    send_json(res, 400, {
                        {"success", false},
                        {"error", "Email and password required"}
                    });
                    return;
                }

                std::string email = body["email"].get<std::string>();
                std::string password = body["password"].get<std::string>();

                // Проверка на пустые строки
                if (email.empty() || password.empty()) {
                    send_json(res, 400, {
                        {"success", false},
                        {"error", "Email and password required"}
                    });
                    return;
                }

                // Подготавливаем SQL-запрос на вставку нового пользователя
                const char* insert_sql = "INSERT INTO users (email, password) VALUES (?, ?);";
                sqlite3_stmt* stmt = nullptr;
                
                int rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    throw std::runtime_error(
                        std::string("Ошибка подготовки запроса: ") + sqlite3_errmsg(db)
                    );
                }

                // Привязываем параметры
                sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

                // Выполняем запрос
                rc = sqlite3_step(stmt);

                if (rc == SQLITE_CONSTRAINT) {
                    // Нарушение уникальности — email уже существует
                    sqlite3_finalize(stmt);
                    send_json(res, 409, {
                        {"success", false},
                        {"error", "Email already exists"}
                    });
                    return;
                }

                if (rc != SQLITE_DONE) {
                    std::string err = sqlite3_errmsg(db);
                    sqlite3_finalize(stmt);
                    throw std::runtime_error("Ошибка вставки пользователя: " + err);
                }

                // Получаем ID вставленной записи
                sqlite3_int64 user_id = sqlite3_last_insert_rowid(db);
                sqlite3_finalize(stmt);

                std::cout << "[REGISTER] Новый пользователь: id=" << user_id
                          << ", email=" << email << "\n";

                send_json(res, 201, {
                    {"success", true},
                    {"user_id", user_id},
                    {"message", "User registered"}
                });

            } catch (const json::parse_error& e) {
                send_json(res, 400, {
                    {"success", false},
                    {"error", "Invalid JSON format"}
                });
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] /api/register: " << e.what() << "\n";
                send_json(res, 500, {
                    {"success", false},
                    {"error", "Internal server error"}
                });
            }
        });

        // ---------------------------------------------------------------
        // 5. Обработчик: POST /api/login — вход пользователя
        // ---------------------------------------------------------------
        svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);

                // Проверяем наличие полей
                if (!body.contains("email") || !body.contains("password") ||
                    !body["email"].is_string() || !body["password"].is_string()) {
                    send_json(res, 400, {
                        {"success", false},
                        {"error", "Email and password required"}
                    });
                    return;
                }

                std::string email = body["email"].get<std::string>();
                std::string password = body["password"].get<std::string>();

                if (email.empty() || password.empty()) {
                    send_json(res, 400, {
                        {"success", false},
                        {"error", "Email and password required"}
                    });
                    return;
                }

                // Ищем пользователя по email
                const char* select_sql = "SELECT id, password FROM users WHERE email = ?;";
                sqlite3_stmt* stmt = nullptr;

                int rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    throw std::runtime_error(
                        std::string("Ошибка подготовки запроса: ") + sqlite3_errmsg(db)
                    );
                }

                sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

                rc = sqlite3_step(stmt);

                if (rc == SQLITE_DONE) {
                    // Пользователь не найден (нет строк)
                    sqlite3_finalize(stmt);
                    send_json(res, 401, {
                        {"success", false},
                        {"error", "Invalid credentials"}
                    });
                    return;
                }

                if (rc != SQLITE_ROW) {
                    std::string err = sqlite3_errmsg(db);
                    sqlite3_finalize(stmt);
                    throw std::runtime_error("Ошибка выборки пользователя: " + err);
                }

                // Извлекаем данные
                int user_id = sqlite3_column_int(stmt, 0);
                const char* db_password = reinterpret_cast<const char*>(
                    sqlite3_column_text(stmt, 1)
                );

                std::string stored_password = db_password ? db_password : "";
                sqlite3_finalize(stmt);

                // Сравниваем пароли
                if (password != stored_password) {
                    send_json(res, 401, {
                        {"success", false},
                        {"error", "Invalid credentials"}
                    });
                    return;
                }

                std::cout << "[LOGIN] Пользователь вошёл: id=" << user_id
                          << ", email=" << email << "\n";

                send_json(res, 200, {
                    {"success", true},
                    {"user_id", user_id},
                    {"message", "Login successful"}
                });

            } catch (const json::parse_error& e) {
                send_json(res, 400, {
                    {"success", false},
                    {"error", "Invalid JSON format"}
                });
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] /api/login: " << e.what() << "\n";
                send_json(res, 500, {
                    {"success", false},
                    {"error", "Internal server error"}
                });
            }
        });

        // ---------------------------------------------------------------
        // 6. Обработчик: POST /api/buy — покупка билета
        //    Внимание: поле называется "event_name" (не "event")
        // ---------------------------------------------------------------
        svr.Post("/api/buy", [](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);

                // Проверяем наличие user_id и event_name
                if (!body.contains("user_id") || !body.contains("event_name")) {
                    send_json(res, 400, {
                        {"success", false},
                        {"error", "user_id and event_name required"}
                    });
                    return;
                }

                // Проверяем, что user_id — целое число
                if (!body["user_id"].is_number_integer()) {
                    send_json(res, 400, {
                        {"success", false},
                        {"error", "user_id and event_name required"}
                    });
                    return;
                }

                // Проверяем, что event_name — строка и не пустая
                if (!body["event_name"].is_string() || 
                    body["event_name"].get<std::string>().empty()) {
                    send_json(res, 400, {
                        {"success", false},
                        {"error", "user_id and event_name required"}
                    });
                    return;
                }

                int user_id = body["user_id"].get<int>();
                std::string event_name = body["event_name"].get<std::string>();
                std::string purchase_date = get_current_datetime();

                // Вставляем новый билет
                const char* insert_sql = 
                    "INSERT INTO tickets (user_id, event, purchase_date) VALUES (?, ?, ?);";
                sqlite3_stmt* stmt = nullptr;

                int rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    throw std::runtime_error(
                        std::string("Ошибка подготовки запроса: ") + sqlite3_errmsg(db)
                    );
                }

                sqlite3_bind_int(stmt, 1, user_id);
                sqlite3_bind_text(stmt, 2, event_name.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, purchase_date.c_str(), -1, SQLITE_TRANSIENT);

                rc = sqlite3_step(stmt);

                if (rc != SQLITE_DONE) {
                    std::string err = sqlite3_errmsg(db);
                    sqlite3_finalize(stmt);
                    throw std::runtime_error("Ошибка вставки билета: " + err);
                }

                sqlite3_int64 ticket_id = sqlite3_last_insert_rowid(db);
                sqlite3_finalize(stmt);

                std::cout << "[BUY] Билет куплен: id=" << ticket_id
                          << ", user_id=" << user_id
                          << ", event=" << event_name
                          << ", date=" << purchase_date << "\n";

                send_json(res, 201, {
                    {"success", true},
                    {"ticket_id", ticket_id},
                    {"event", event_name},
                    {"purchase_date", purchase_date}
                });

            } catch (const json::parse_error& e) {
                send_json(res, 400, {
                    {"success", false},
                    {"error", "Invalid JSON format"}
                });
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] /api/buy: " << e.what() << "\n";
                send_json(res, 500, {
                    {"success", false},
                    {"error", "Internal server error"}
                });
            }
        });

        // ---------------------------------------------------------------
        // 7. Обработчик: GET /api/tickets?user_id=<id> — получение билетов
        // ---------------------------------------------------------------
        svr.Get("/api/tickets", [](const httplib::Request& req, httplib::Response& res) {
            try {
                // Извлекаем user_id из query-параметров
                if (!req.has_param("user_id")) {
                    send_json(res, 400, {
                        {"success", false},
                        {"error", "user_id required"}
                    });
                    return;
                }

                std::string user_id_str = req.get_param_value("user_id");
                
                // Проверяем, что это целое число
                int user_id;
                try {
                    user_id = std::stoi(user_id_str);
                } catch (...) {
                    send_json(res, 400, {
                        {"success", false},
                        {"error", "user_id required"}
                    });
                    return;
                }

                // Выбираем билеты пользователя
                const char* select_sql = 
                    "SELECT id, event, purchase_date FROM tickets WHERE user_id = ?;";
                sqlite3_stmt* stmt = nullptr;

                int rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    throw std::runtime_error(
                        std::string("Ошибка подготовки запроса: ") + sqlite3_errmsg(db)
                    );
                }

                sqlite3_bind_int(stmt, 1, user_id);

                // Собираем все билеты в JSON-массив
                json tickets_array = json::array();

                while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                    int ticket_id = sqlite3_column_int(stmt, 0);
                    const char* event = reinterpret_cast<const char*>(
                        sqlite3_column_text(stmt, 1)
                    );
                    const char* purchase_date = reinterpret_cast<const char*>(
                        sqlite3_column_text(stmt, 2)
                    );

                    json ticket = {
                        {"id", ticket_id},
                        {"event", event ? event : ""},
                        {"purchase_date", purchase_date ? purchase_date : ""}
                    };
                    tickets_array.push_back(ticket);
                }

                if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
                    std::string err = sqlite3_errmsg(db);
                    sqlite3_finalize(stmt);
                    throw std::runtime_error("Ошибка выборки билетов: " + err);
                }

                sqlite3_finalize(stmt);

                std::cout << "[TICKETS] Запрошены билеты user_id=" << user_id
                          << ", найдено: " << tickets_array.size() << "\n";

                send_json(res, 200, {
                    {"success", true},
                    {"tickets", tickets_array}
                });

            } catch (const std::exception& e) {
                std::cerr << "[ERROR] /api/tickets: " << e.what() << "\n";
                send_json(res, 500, {
                    {"success", false},
                    {"error", "Internal server error"}
                });
            }
        });

        // ---------------------------------------------------------------
        // 8. Обработчик: GET /api/ai_suggest — заглушка ИИ-сервиса
        // ---------------------------------------------------------------
        svr.Get("/api/ai_suggest", [](const httplib::Request& req, httplib::Response& res) {
            try {
                send_json(res, 503, {
                    {"success", false},
                    {"error", "AI service not configured yet"}
                });
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] /api/ai_suggest: " << e.what() << "\n";
                send_json(res, 500, {
                    {"success", false},
                    {"error", "Internal server error"}
                });
            }
        });

        // ---------------------------------------------------------------
        // 9. Обработчик OPTIONS — для CORS preflight запросов
        // ---------------------------------------------------------------
        svr.Options(R"(/api/.*)", [](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            res.status = 204;
        });

        // ---------------------------------------------------------------
        // 10. Запуск сервера на порту 8080
        // ---------------------------------------------------------------
        std::cout << "Server started on port 8080\n";
        svr.listen("0.0.0.0", 8080);

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Критическая ошибка при запуске сервера: " << e.what() << "\n";
        
        // Закрываем базу данных, если она была открыта
        if (db) {
            sqlite3_close(db);
            db = nullptr;
        }
        return 1;
    }

    // ---------------------------------------------------------------
    // Завершение работы: закрываем базу данных
    // ---------------------------------------------------------------
    if (db) {
        sqlite3_close(db);
        db = nullptr;
        std::cout << "[DB] База данных закрыта\n";
    }

    return 0;
}