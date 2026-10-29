#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Forward declaration keeps sqlite3.h out of the public API header.
struct sqlite3;

// -----------------------------------------------------------------------------
// Database connection
// -----------------------------------------------------------------------------

enum class DatabaseOpenMode
{
    ReadOnly,
    ReadWrite
};

class Database
{
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;

    // Opens an existing SQLite database.
    // Core databases should normally be opened ReadOnly at runtime.
    bool open(const std::filesystem::path& databasePath,
        DatabaseOpenMode mode = DatabaseOpenMode::ReadOnly);

    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] const std::string& lastError() const noexcept;

private:
    sqlite3* db_ = nullptr;
    std::filesystem::path path_;
    std::string lastError_;

    // The Core builder will need direct SQLite access while constructing
    // the disposable compiled database. Runtime callers should not.
    friend struct CoreDatabaseBuildAccess;
};

// -----------------------------------------------------------------------------
// Core database build API
// -----------------------------------------------------------------------------

struct CoreDatabaseBuildRequest
{
    // Canonical source root, normally:
    //
    //   Data/Source
    //
    // The builder resolves ruleset- and season-specific content beneath it.
    std::filesystem::path sourceRoot;

    // Initial PFF Classic values:
    //
    //   ruleSet = "FirstEdition"
    //   season  = 1989
    //
    // These are inputs rather than hardcoded assumptions so the same API
    // can build later PFF/GCE packages.
    std::string ruleSet;
    std::int32_t season = 0;

    // Final compiled Core database path.
    std::filesystem::path outputDatabase;

    // False by default so an existing Core is never destroyed accidentally.
    bool overwriteExisting = false;
};

enum class DatabaseDiagnosticSeverity
{
    Info,
    Warning,
    Error
};

struct DatabaseDiagnostic
{
    DatabaseDiagnosticSeverity severity = DatabaseDiagnosticSeverity::Error;

    std::string message;

    // Optional source location. Empty/zero when the error is not tied to a
    // specific authored source record.
    std::filesystem::path sourceFile;
    std::size_t sourceLine = 0;
};

struct CoreDatabaseBuildResult
{
    bool success = false;

    std::filesystem::path databasePath;

    std::size_t filesProcessed = 0;
    std::uint64_t recordsInserted = 0;

    std::vector<DatabaseDiagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept
    {
        return success;
    }
};

// Builds a disposable/reproducible Core database from canonical editable
// source. The implementation will:
//
//   1. discover the selected ruleset + season .pffdata sources;
//   2. create the initial Core schema;
//   3. parse and validate supported source records;
//   4. populate the Core database transactionally;
//   5. run integrity/foreign-key validation;
//   6. publish the completed database only after a successful build.
//
// The first implementation can support only the initial source families we
// are loading now. Unsupported source families should produce an explicit
// diagnostic rather than being silently ignored.
CoreDatabaseBuildResult createCoreDatabaseFromSource(
    const CoreDatabaseBuildRequest& request);

