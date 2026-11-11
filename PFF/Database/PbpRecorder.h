#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace PffPbp
{
using Fields = std::initializer_list<std::pair<std::string_view, std::string>>;

struct Tag
{
    std::string name, value;
    std::size_t depth;
    std::string source;
};

// Reads the ordered tags and validates the block structure of the live profile.
inline std::vector<Tag> read(std::istream &input)
{
    std::vector<Tag> tags;
    std::vector<std::string> blocks;
    std::string line;
    while (std::getline(input, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] != '[')
            throw std::runtime_error("Invalid live PBP tag");
        const auto bracket = line.find(']', start);
        if (bracket == std::string::npos || bracket - start < 2)
            throw std::runtime_error("Invalid live PBP tag");
        auto name = line.substr(start + 1, bracket - start - 1);
        std::string value;
        for (auto i = bracket + 1; i < line.size(); ++i)
        {
            if (line[i] != '\\')
            {
                value += line[i];
                continue;
            }
            if (++i == line.size())
                throw std::runtime_error("Truncated live PBP escape");
            switch (line[i])
            {
            case '\\': value += '\\'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            default: throw std::runtime_error("Invalid live PBP escape");
            }
        }
        const auto suffix = [&](std::string_view ending) {
            return name.size() > ending.size() && name.compare(name.size() - ending.size(), ending.size(), ending) == 0;
        };
        if (suffix("_END"))
        {
            if (blocks.empty() || !value.empty() || blocks.back() != name.substr(0, name.size() - 4))
                throw std::runtime_error("Mismatched live PBP block");
            blocks.pop_back();
        }
        if (blocks.empty() && (name != "PBP_BEGIN" || !tags.empty()) && name != "PBP_END")
            throw std::runtime_error("Unexpected live PBP root");
        tags.push_back({name, value, blocks.size(), line.substr(start)});
        if (suffix("_BEGIN"))
        {
            if (!value.empty())
                throw std::runtime_error("Unexpected value on live PBP block");
            blocks.push_back(name.substr(0, name.size() - 6));
        }
    }
    if (input.bad() || !blocks.empty() || tags.size() < 4 || tags.front().name != "PBP_BEGIN" ||
        tags[1].name != "FORMAT_VERSION" || tags[1].value != "2" || tags.back().name != "PBP_END")
        throw std::runtime_error("Incomplete or unsupported live PBP");
    return tags;
}

// Rebuild the presentation log from tagged records in timeline order.
inline std::vector<std::string> readDisplayLog(std::istream &input)
{
    std::vector<std::string> lines;
    bool final = false;
    bool hasLog = false;
    for (const auto &tag : read(input))
    {
        if (tag.depth == 2 && tag.name == "CompletionStatus" && tag.value == "FINAL")
            final = true;
        if (tag.depth != 2 || tag.name != "DisplayText")
            continue;
        hasLog = true;
        std::istringstream display(tag.value);
        for (std::string line; std::getline(display, line);)
            lines.push_back(std::move(line));
    }
    if (!final || !hasLog)
        throw std::runtime_error("Final tagged PBP is missing its play log");
    return lines;
}

class Recorder
{
  public:
    struct Mark
    {
        std::size_t bytes;
        std::int64_t lastRecord;
    };

    Mark mark() const { return {records.size(), last}; }

    void rewind(Mark checkpoint)
    {
        records.resize(checkpoint.bytes);
        last = checkpoint.lastRecord;
        current.clear();
    }

    void reset(Fields context, std::string history = {})
    {
        header = "[PBP_BEGIN]\n[FORMAT_VERSION]2\n[DATA_VERSION]1\n";
        for (const auto &field : context)
            append(header, field.first, field.second);
        records = std::move(history);
        current.clear();
        last = 0;
        const auto position = records.rfind("\n[RecordID]");
        if (position != std::string::npos)
            last = std::stoll(records.substr(position + 11));
    }

    void begin(std::int64_t sequence, std::string_view kind, Fields state)
    {
        current.clear();
        id = sequence;
        draws = lookups = actions = 0;
        append(current, "RECORD_BEGIN", "");
        append(current, "RecordID", std::to_string(id));
        append(current, "SequenceNo", std::to_string(id));
        append(current, "RecordKind", kind);
        if (kind == "PLAY")
        {
            append(current, "PLAY_ID", std::to_string(id));
            block("START_STATE", state);
        }
    }

    void block(std::string_view kind, Fields fields)
    {
        if (current.empty())
            return;
        const std::string name(kind);
        append(current, name + "_BEGIN", "");
        for (const auto &field : fields)
            append(current, field.first, field.second);
        append(current, name + "_END", "");
    }

    void roll(int sides, std::string_view purpose, std::string_view color, int face)
    {
        const auto draw = std::to_string(++draws);
        block("ROLL", {{"RollID", std::to_string(id) + "." + draw}, {"DrawNo", draw},
                       {"RollPurpose", std::string(purpose)}, {"DieType", sides == 2 ? "COIN" : "D" + std::to_string(sides)},
                       {"DieColor", std::string(color)}, {"RawFace", std::to_string(face)}});
    }

    std::string rollRef(int offset = 0) const
    {
        return draws > offset ? std::to_string(id) + "." + std::to_string(draws - offset) : "";
    }

    void lookup(std::string_view source, int adjusted, int row, int column, std::string_view result,
                int rowRollOffset = 1, int columnRollOffset = 0)
    {
        if (current.empty())
            return;
        block("LOOKUP", {{"LookupID", std::to_string(id) + "." + std::to_string(++lookups)},
                         {"LookupPurpose", std::string(source)}, {"SourceRef", std::string(source)},
                         {"RowRollRef", rollRef(rowRollOffset)}, {"ColumnRollRef", rollRef(columnRollOffset)},
                         {"AdjustedValue", std::to_string(adjusted)}, {"LookupRow", std::to_string(row)},
                         {"LookupColumn", std::to_string(column)}, {"LookupResult", std::string(result)}});
    }

    void action(std::string_view kind, std::string_view team, int value, std::string_view detail = "")
    {
        block("ACTION", {{"ACTION_ID", std::to_string(id) + "." + std::to_string(++actions)},
                         {"ActionKind", std::string(kind)}, {"TeamKey", std::string(team)},
                         {"ActionValue", std::to_string(value)}, {"ActionDetail", std::string(detail)}});
    }

    void complete()
    {
        append(current, "RECORD_END", "");
        records += current;
        current.clear();
        last = id;
    }

    std::int64_t nextSequence() const { return last + 1; }
    const std::string &history() const { return records; }

    std::string final(Fields state, std::string_view committedLog = {}) const
    {
        auto bytes = header + records;
        if (!committedLog.empty())
        {
            append(bytes, "FINAL_PLAY_LOG_BEGIN", "");
            append(bytes, "DisplayText", committedLog);
            append(bytes, "FINAL_PLAY_LOG_END", "");
        }
        append(bytes, "GAME_ENDING_BEGIN", "");
        append(bytes, "CompletionStatus", "FINAL");
        for (const auto &field : state)
            append(bytes, field.first, field.second);
        return formatDocument(bytes + "[GAME_ENDING_END]\n[PBP_END]\n");
    }

    void writePartial(const std::filesystem::path &path, std::string_view phase, std::string_view reason) const
    {
        auto bytes = header + records;
        if (!current.empty())
            bytes += current + "[RecordStatus]INCOMPLETE\n[RECORD_END]\n";
        append(bytes, "GAME_ENDING_BEGIN", "");
        append(bytes, "CompletionStatus", "PARTIAL");
        append(bytes, "LastCompletedRecordID", std::to_string(last));
        if (!current.empty())
            append(bytes, "IncompleteRecordID", std::to_string(id));
        append(bytes, "FailurePhase", phase);
        append(bytes, "FailureReason", reason);
        bytes += "[GAME_ENDING_END]\n[PBP_END]\n";
        bytes = formatDocument(bytes);
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size())).flush();
        if (!output)
            throw std::runtime_error("Unable to write partial PBP");
    }

  private:
    // Keep buffered history unchanged so existing checkpoints can resume.
    static std::string formatDocument(std::string bytes)
    {
        constexpr std::size_t spacesPerLevel = 5;
        std::istringstream input(std::move(bytes));
        std::string formatted;
        for (const auto &tag : read(input))
        {
            formatted.append(tag.depth * spacesPerLevel, ' ');
            append(formatted, tag.name, tag.value);
        }
        return formatted;
    }

    static void append(std::string &out, std::string_view key, std::string_view value)
    {
        out += '[';
        out += key;
        out += ']';
        for (char ch : value)
        {
            if (ch == '\\')
                out += "\\\\";
            else if (ch == '\n')
                out += "\\n";
            else if (ch == '\r')
                out += "\\r";
            else
                out += ch;
        }
        out += '\n';
    }

    std::string header, records, current;
    std::int64_t id = 0, last = 0;
    int draws = 0, lookups = 0, actions = 0;
};
} // namespace PffPbp
