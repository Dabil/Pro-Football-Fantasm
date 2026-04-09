#pragma once

#include <string>
#include <vector>

namespace XpArtLoaderTests
{
    struct Failure
    {
        std::string testName;
        std::string message;
    };

    struct Result
    {
        int totalCount = 0;
        int passedCount = 0;
        int failedCount = 0;
        std::vector<Failure> failures;

        bool success() const
        {
            return failedCount == 0;
        }
    };

    Result runAll();
    std::string formatSummary(const Result& result);
}