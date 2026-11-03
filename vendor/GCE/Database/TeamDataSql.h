#pragma once
#include <sqlite3.h>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <type_traits>
#include <vector>

// Internal implementation utilities; domain callers use typed editing services.
namespace TeamDataSql
{
    struct Error : std::runtime_error
    {
        int code;
        explicit Error(sqlite3* db)
            : std::runtime_error(sqlite3_errmsg(db)), code(sqlite3_extended_errcode(db))
        {
        }
    };
    using Value = std::variant<std::nullptr_t, std::int64_t, std::string>;
    struct Query
    {
        sqlite3* db;
        sqlite3_stmt* statement = nullptr;
        Query(sqlite3* database, const char* sql, std::initializer_list<Value> values = {}) : db(database)
        {
            if (sqlite3_prepare_v2(db, sql, -1, &statement, nullptr) != SQLITE_OK)
                throw Error(db);
            int index = 0;
            try
            {
                if (sqlite3_bind_parameter_count(statement) != static_cast<int>(values.size()))
                    throw std::runtime_error("SQL parameter count does not match typed arguments");
                for (const auto& v : values)
                {
                    ++index;
                    int rc = std::visit(
                        [&](const auto& x) {
                            using T = std::decay_t<decltype(x)>;
                            if constexpr (std::is_same_v<T, std::nullptr_t>)
                                return sqlite3_bind_null(statement, index);
                            else if constexpr (std::is_same_v<T, std::int64_t>)
                                return sqlite3_bind_int64(statement, index, x);
                            else
                                return sqlite3_bind_text(statement, index, x.c_str(),
                                                         static_cast<int>(x.size()), SQLITE_TRANSIENT);
                        },
                        v);
                    if (rc != SQLITE_OK)
                        throw Error(db);
                }
            }
            catch (...)
            {
                sqlite3_finalize(statement);
                throw;
            }
        }
        ~Query()
        {
            sqlite3_finalize(statement);
        }
        Query(const Query&) = delete;
        Query& operator=(const Query&) = delete;
        bool row()
        {
            int rc = sqlite3_step(statement);
            if (rc == SQLITE_ROW)
                return true;
            if (rc == SQLITE_DONE)
                return false;
            throw Error(db);
        }
        void done()
        {
            if (row())
                throw std::runtime_error("Unexpected SQL result row");
        }
        std::int64_t integer(int i = 0) const
        {
            return sqlite3_column_int64(statement, i);
        }
        bool null(int i) const
        {
            return sqlite3_column_type(statement, i) == SQLITE_NULL;
        }
        std::string text(int i = 0) const
        {
            const auto* p = sqlite3_column_text(statement, i);
            return p ? std::string(reinterpret_cast<const char*>(p), sqlite3_column_bytes(statement, i))
                     : std::string{};
        }
        std::optional<std::string> optionalText(int i) const
        {
            return null(i) ? std::nullopt : std::optional<std::string>(text(i));
        }
    };
    inline void run(sqlite3* db, const char* sql, std::initializer_list<Value> v = {})
    {
        Query(db, sql, v).done();
    }
    inline void exec(sqlite3* db, const char* sql)
    {
        char* msg = nullptr;
        int rc = sqlite3_exec(db, sql, nullptr, nullptr, &msg);
        sqlite3_free(msg);
        if (rc != SQLITE_OK)
            throw Error(db);
    }
    inline std::int64_t scalar(sqlite3* db, const char* sql, std::initializer_list<Value> v = {})
    {
        Query q(db, sql, v);
        if (!q.row())
            throw std::runtime_error("Required SQL result is missing");
        return q.integer();
    }
    inline Value nullable(const std::optional<std::string>& s)
    {
        return s ? Value(*s) : Value(nullptr);
    }
    inline std::int64_t inserted(sqlite3* db)
    {
        return sqlite3_last_insert_rowid(db);
    }
    struct Transaction
    {
        sqlite3* db;
        bool committed = false;
        explicit Transaction(sqlite3* d) : db(d)
        {
            exec(db, "BEGIN IMMEDIATE");
        }
        ~Transaction()
        {
            if (!committed)
                sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        }
        void commit()
        {
            exec(db, "COMMIT");
            committed = true;
        }
    };
    // Canonical timestamp validation, including Gregorian month/leap-day bounds.
    inline bool utc(const std::string& s)
    {
        if (s.size() != 20 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':' ||
            s[19] != 'Z')
            return false;
        for (int i = 0; i < 19; ++i)
            if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && (s[i] < '0' || s[i] > '9'))
                return false;
        const int y = std::stoi(s.substr(0, 4)), m = std::stoi(s.substr(5, 2)), d = std::stoi(s.substr(8, 2));
        if (y < 1 || m < 1 || m > 12)
            return false;
        const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        const int max = days[m - 1] + (m == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
        return d >= 1 && d <= max && std::stoi(s.substr(11, 2)) < 24 && std::stoi(s.substr(14, 2)) < 60 &&
               std::stoi(s.substr(17, 2)) < 60;
    }
} // namespace TeamDataSql
