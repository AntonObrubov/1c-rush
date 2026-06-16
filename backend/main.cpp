#include <iostream>
#include <string>
#include <ctime>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <vector>

#include "httplib.h"
#include "json.hpp"
#include "sqlite3.h"
#include "picosha2.h"

using json = nlohmann::json;
sqlite3* db = nullptr;

std::string get_current_datetime() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &now_time);
#else
    localtime_r(&now_time, &local_tm);
#endif
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm);
    return std::string(buffer);
}

void init_db() {
    int rc = sqlite3_open("1crush.db", &db);
    if (rc != SQLITE_OK)
        throw std::runtime_error("DB open error: " + std::string(sqlite3_errmsg(db)));

    const char* pragma_sql = "PRAGMA foreign_keys = ON;";
    sqlite3_exec(db, pragma_sql, nullptr, nullptr, nullptr);

    const char* create_users_sql = R"(
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            email TEXT UNIQUE NOT NULL,
            password_hash TEXT NOT NULL,
            display_name TEXT DEFAULT '',
            avatar_base64 TEXT DEFAULT '',
            created_at TEXT DEFAULT (datetime('now'))
        );
    )";
    rc = sqlite3_exec(db, create_users_sql, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error("Create users table error");

    const char* create_stats_sql = R"(
        CREATE TABLE IF NOT EXISTS user_stats (
            user_id INTEGER PRIMARY KEY,
            tasks_solved INTEGER DEFAULT 0,
            days_count INTEGER DEFAULT 1,
            last_login_date TEXT,
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
    )";
    rc = sqlite3_exec(db, create_stats_sql, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error("Create stats table error");

    const char* create_solved_sql = R"(
        CREATE TABLE IF NOT EXISTS solved_tasks (
            user_id INTEGER NOT NULL,
            task_index INTEGER NOT NULL,
            PRIMARY KEY (user_id, task_index),
            FOREIGN KEY (user_id) REFERENCES users(id)
        );
    )";
    rc = sqlite3_exec(db, create_solved_sql, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error("Create solved_tasks table error");

    std::cout << "[DB] Database ready\n";
}

void set_json_headers(httplib::Response& res) {
    res.set_header("Content-Type", "application/json");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

void send_json(httplib::Response& res, int status, const json& body) {
    set_json_headers(res);
    res.status = status;
    res.set_content(body.dump(), "application/json");
}

int main() {
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
    try {
        init_db();
        httplib::Server svr;

        svr.set_mount_point("/", "../static");
        std::cout << "[STATIC] Serving files from ../static\n";

        // ---------- REGISTER ----------
        svr.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);
                if (!body.contains("email") || !body.contains("password")) {
                    send_json(res, 400, {{"success", false}, {"error", "Email and password required"}});
                    return;
                }
                std::string email = body["email"];
                std::string password = body["password"];
                if (email.empty() || password.empty()) {
                    send_json(res, 400, {{"success", false}, {"error", "Empty fields"}});
                    return;
                }
                std::string hash = picosha2::hash256_hex_string(password);
                sqlite3_stmt* stmt;
                const char* sql = "INSERT INTO users (email, password_hash) VALUES (?, ?);";
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
                    throw std::runtime_error("Prepare error");
                sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
                int rc = sqlite3_step(stmt);
                if (rc == SQLITE_CONSTRAINT) {
                    sqlite3_finalize(stmt);
                    send_json(res, 409, {{"success", false}, {"error", "Email already exists"}});
                    return;
                }
                if (rc != SQLITE_DONE) {
                    sqlite3_finalize(stmt);
                    throw std::runtime_error("Insert error");
                }
                sqlite3_int64 user_id = sqlite3_last_insert_rowid(db);
                sqlite3_finalize(stmt);
                const char* stats_sql = "INSERT INTO user_stats (user_id, days_count, last_login_date) VALUES (?, 1, date('now'));";
                sqlite3_prepare_v2(db, stats_sql, -1, &stmt, nullptr);
                sqlite3_bind_int64(stmt, 1, user_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                send_json(res, 201, {{"success", true}, {"user_id", user_id}});
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] /api/register: " << e.what() << "\n";
                send_json(res, 500, {{"success", false}, {"error", "Internal server error"}});
            }
        });

        // ---------- LOGIN ----------
        svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);
                std::string email = body["email"];
                std::string password = body["password"];
                if (email.empty() || password.empty()) {
                    send_json(res, 400, {{"success", false}, {"error", "Empty fields"}});
                    return;
                }
                std::string hash = picosha2::hash256_hex_string(password);
                sqlite3_stmt* stmt;
                const char* sql = "SELECT id, display_name, avatar_base64 FROM users WHERE email = ? AND password_hash = ?;";
                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK)
                    throw std::runtime_error("Prepare error");
                sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) != SQLITE_ROW) {
                    sqlite3_finalize(stmt);
                    send_json(res, 401, {{"success", false}, {"error", "Invalid credentials"}});
                    return;
                }
                int user_id = sqlite3_column_int(stmt, 0);
                std::string display_name = (const char*)sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "";
                std::string avatar = (const char*)sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
                sqlite3_finalize(stmt);
                const char* update_stats = R"(
                    UPDATE user_stats SET days_count = days_count + 1, last_login_date = date('now')
                    WHERE user_id = ? AND last_login_date != date('now');
                )";
                sqlite3_prepare_v2(db, update_stats, -1, &stmt, nullptr);
                sqlite3_bind_int(stmt, 1, user_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                int tasks_solved = 0, days_count = 1;
                const char* stats_sql = "SELECT tasks_solved, days_count FROM user_stats WHERE user_id = ?;";
                sqlite3_prepare_v2(db, stats_sql, -1, &stmt, nullptr);
                sqlite3_bind_int(stmt, 1, user_id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    tasks_solved = sqlite3_column_int(stmt, 0);
                    days_count = sqlite3_column_int(stmt, 1);
                }
                sqlite3_finalize(stmt);
                json resp = {
                    {"success", true},
                    {"user_id", user_id},
                    {"email", email},
                    {"display_name", display_name},
                    {"avatar_base64", avatar},
                    {"tasks_solved", tasks_solved},
                    {"days_count", days_count}
                };
                send_json(res, 200, resp);
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] /api/login: " << e.what() << "\n";
                send_json(res, 500, {{"success", false}, {"error", "Internal server error"}});
            }
        });

        // ---------- PROFILE (GET) ----------
        svr.Get("/api/profile", [](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_param("user_id")) {
                send_json(res, 400, {{"success", false}, {"error", "user_id required"}});
                return;
            }
            int user_id = std::stoi(req.get_param_value("user_id"));
            sqlite3_stmt* stmt;
            const char* sql = R"(
                SELECT u.email, u.display_name, u.avatar_base64, u.created_at, s.tasks_solved, s.days_count
                FROM users u LEFT JOIN user_stats s ON u.id = s.user_id
                WHERE u.id = ?;
            )";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                send_json(res, 500, {{"success", false}, {"error", "DB error"}});
                return;
            }
            sqlite3_bind_int(stmt, 1, user_id);
            if (sqlite3_step(stmt) != SQLITE_ROW) {
                sqlite3_finalize(stmt);
                send_json(res, 404, {{"success", false}, {"error", "User not found"}});
                return;
            }
            std::string email = (const char*)sqlite3_column_text(stmt, 0);
            std::string display_name = (const char*)sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "";
            std::string avatar = (const char*)sqlite3_column_text(stmt, 2) ? (const char*)sqlite3_column_text(stmt, 2) : "";
            std::string created_at = (const char*)sqlite3_column_text(stmt, 3);
            int tasks = sqlite3_column_int(stmt, 4);
            int days = sqlite3_column_int(stmt, 5);
            sqlite3_finalize(stmt);
            send_json(res, 200, {
                {"success", true},
                {"email", email},
                {"display_name", display_name},
                {"avatar_base64", avatar},
                {"created_at", created_at},
                {"tasks_solved", tasks},
                {"days_count", days}
            });
        });

        // ---------- UPDATE PROFILE (PUT) - ИСПРАВЛЕНО ----------
        svr.Put("/api/profile", [](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);
                int user_id = body["user_id"];
                std::string display_name = body.value("display_name", "");
                std::string avatar = body.value("avatar_base64", "");

                sqlite3_stmt* stmt;
                std::string sql = "UPDATE users SET ";
                bool need_update = false;
                if (!display_name.empty()) {
                    sql += "display_name = ?";
                    need_update = true;
                }
                if (!avatar.empty()) {
                    if (need_update) sql += ", ";
                    sql += "avatar_base64 = ?";
                    need_update = true;
                }
                if (!need_update) {
                    send_json(res, 200, {{"success", true}});
                    return;
                }
                sql += " WHERE id = ?;";

                if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                    send_json(res, 500, {{"success", false}, {"error", "DB prepare error"}});
                    return;
                }
                int bindIndex = 1;
                if (!display_name.empty()) {
                    sqlite3_bind_text(stmt, bindIndex++, display_name.c_str(), -1, SQLITE_TRANSIENT);
                }
                if (!avatar.empty()) {
                    sqlite3_bind_text(stmt, bindIndex++, avatar.c_str(), -1, SQLITE_TRANSIENT);
                }
                sqlite3_bind_int(stmt, bindIndex, user_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                // Возвращаем обновлённые данные
                sqlite3_prepare_v2(db, "SELECT display_name, avatar_base64 FROM users WHERE id = ?;", -1, &stmt, nullptr);
                sqlite3_bind_int(stmt, 1, user_id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    std::string new_display = (const char*)sqlite3_column_text(stmt, 0) ? (const char*)sqlite3_column_text(stmt, 0) : "";
                    std::string new_avatar = (const char*)sqlite3_column_text(stmt, 1) ? (const char*)sqlite3_column_text(stmt, 1) : "";
                    send_json(res, 200, {{"success", true}, {"display_name", new_display}, {"avatar_base64", new_avatar}});
                } else {
                    send_json(res, 200, {{"success", true}});
                }
                sqlite3_finalize(stmt);
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] /api/profile PUT: " << e.what() << "\n";
                send_json(res, 500, {{"success", false}, {"error", "Internal server error"}});
            }
        });

        // ---------- TASK SOLVED (сохранение + увеличение счётчика) - ИСПРАВЛЕНО ----------
        svr.Post("/api/task-solved/save", [](const httplib::Request& req, httplib::Response& res) {
            try {
                json body = json::parse(req.body);
                int user_id = body["user_id"];
                int task_index = body["task_index"];

                // 1. Проверяем, решена ли уже задача
                sqlite3_stmt* stmt;
                const char* check_sql = "SELECT 1 FROM solved_tasks WHERE user_id = ? AND task_index = ?;";
                if (sqlite3_prepare_v2(db, check_sql, -1, &stmt, nullptr) != SQLITE_OK) {
                    throw std::runtime_error("Prepare check failed");
                }
                sqlite3_bind_int(stmt, 1, user_id);
                sqlite3_bind_int(stmt, 2, task_index);
                bool already_solved = (sqlite3_step(stmt) == SQLITE_ROW);
                sqlite3_finalize(stmt);

                if (!already_solved) {
                    // 2. Сохраняем факт решения
                    const char* insert_sql = "INSERT INTO solved_tasks (user_id, task_index) VALUES (?, ?);";
                    sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
                    sqlite3_bind_int(stmt, 1, user_id);
                    sqlite3_bind_int(stmt, 2, task_index);
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);

                    // 3. Увеличиваем общий счётчик
                    const char* update_sql = "UPDATE user_stats SET tasks_solved = tasks_solved + 1 WHERE user_id = ?;";
                    sqlite3_prepare_v2(db, update_sql, -1, &stmt, nullptr);
                    sqlite3_bind_int(stmt, 1, user_id);
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                }

                // 4. Возвращаем обновлённую статистику
                int new_tasks_solved = 0, new_days_count = 0;
                sqlite3_prepare_v2(db, "SELECT tasks_solved, days_count FROM user_stats WHERE user_id = ?;", -1, &stmt, nullptr);
                sqlite3_bind_int(stmt, 1, user_id);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    new_tasks_solved = sqlite3_column_int(stmt, 0);
                    new_days_count = sqlite3_column_int(stmt, 1);
                }
                sqlite3_finalize(stmt);

                send_json(res, 200, {{"success", true}, {"tasks_solved", new_tasks_solved}, {"days_count", new_days_count}});
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] /api/task-solved/save: " << e.what() << "\n";
                send_json(res, 500, {{"success", false}, {"error", "Internal server error"}});
            } catch (...) {
                send_json(res, 500, {{"success", false}});
            }
        });

        // ---------- GET SOLVED TASKS ----------
        svr.Get("/api/solved-tasks", [](const httplib::Request& req, httplib::Response& res) {
            if (!req.has_param("user_id")) {
                send_json(res, 400, {{"success", false}, {"error", "user_id required"}});
                return;
            }
            int user_id = std::stoi(req.get_param_value("user_id"));
            sqlite3_stmt* stmt;
            const char* sql = "SELECT task_index FROM solved_tasks WHERE user_id = ?;";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                send_json(res, 500, {{"success", false}});
                return;
            }
            sqlite3_bind_int(stmt, 1, user_id);
            json tasks = json::array();
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                tasks.push_back(sqlite3_column_int(stmt, 0));
            }
            sqlite3_finalize(stmt);
            send_json(res, 200, {{"success", true}, {"tasks", tasks}});
        });

        // CORS preflight
        svr.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
            res.status = 204;
        });

        std::cout << "Server started on port 8080\n";
        svr.listen("0.0.0.0", 8080);
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        if (db) sqlite3_close(db);
        return 1;
    }
    if (db) sqlite3_close(db);
    return 0;
}