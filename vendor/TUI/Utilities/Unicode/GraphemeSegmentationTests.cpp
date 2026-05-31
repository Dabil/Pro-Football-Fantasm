#include "Utilities/Unicode/GraphemeSegmentationTests.h"

#include <sstream>
#include <string>
#include <vector>

#include "Rendering/Text/TextCluster.h"
#include "Utilities/Unicode/GraphemeSegmentation.h"

namespace
{
    struct TestContext
    {
        std::string testName;
        std::vector<GraphemeSegmentationTests::Failure> failures;

        void fail(const std::string& message)
        {
            failures.push_back({ testName, message });
        }
    };

    using TestFunction = void(*)(TestContext&);

    struct TestCase
    {
        const char* name = "";
        TestFunction function = nullptr;
    };

    void expectEqualInt(TestContext& context, int actual, int expected, const std::string& label)
    {
        if (actual != expected)
        {
            std::ostringstream stream;
            stream << label << " expected " << expected << ", got " << actual << '.';
            context.fail(stream.str());
        }
    }

    void expectEqualU32(TestContext& context, const std::u32string& actual, const std::u32string& expected, const std::string& label)
    {
        if (actual != expected)
        {
            std::ostringstream stream;
            stream << label << " expected length " << expected.size() << ", got " << actual.size() << '.';
            context.fail(stream.str());
        }
    }

    void combiningMarkStaysWithBase(TestContext& context)
    {
        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(U"a\u0301b");

        expectEqualInt(context, static_cast<int>(clusters.size()), 2, "cluster count");
        if (clusters.size() >= 2)
        {
            expectEqualU32(context, clusters[0].codePoints, U"a\u0301", "first cluster");
            expectEqualInt(context, clusters[0].displayWidth, 1, "first cluster width");
            expectEqualU32(context, clusters[1].codePoints, U"b", "second cluster");
        }
    }

    void variationSelectorStaysWithBase(TestContext& context)
    {
        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(U"\u2764\uFE0F!");

        expectEqualInt(context, static_cast<int>(clusters.size()), 2, "cluster count");
        if (clusters.size() >= 2)
        {
            expectEqualU32(context, clusters[0].codePoints, U"\u2764\uFE0F", "heart emoji cluster");
            expectEqualInt(context, clusters[0].displayWidth, 1, "heart cluster width follows UnicodeWidth policy");
            expectEqualU32(context, clusters[1].codePoints, U"!", "punctuation cluster");
        }
    }

    void emojiModifierStaysWithBase(TestContext& context)
    {
        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(U"\U0001F44B\U0001F3FDx");

        expectEqualInt(context, static_cast<int>(clusters.size()), 2, "cluster count");
        if (clusters.size() >= 2)
        {
            expectEqualU32(context, clusters[0].codePoints, U"\U0001F44B\U0001F3FD", "emoji modifier cluster");
            expectEqualInt(context, clusters[0].displayWidth, 2, "emoji modifier cluster width");
            expectEqualU32(context, clusters[1].codePoints, U"x", "trailing text cluster");
        }
    }

    void zeroWidthJoinerSequenceIsAtomic(TestContext& context)
    {
        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(U"\U0001F469\u200D\U0001F4BB.");

        expectEqualInt(context, static_cast<int>(clusters.size()), 2, "cluster count");
        if (clusters.size() >= 2)
        {
            expectEqualU32(context, clusters[0].codePoints, U"\U0001F469\u200D\U0001F4BB", "ZWJ emoji cluster");
            expectEqualInt(context, clusters[0].displayWidth, 2, "ZWJ cluster width");
            expectEqualU32(context, clusters[1].codePoints, U".", "trailing punctuation cluster");
        }
    }

    void regionalIndicatorsPairAsFlags(TestContext& context)
    {
        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(U"\U0001F1FA\U0001F1F8\U0001F1E8\U0001F1E6x");

        expectEqualInt(context, static_cast<int>(clusters.size()), 3, "cluster count");
        if (clusters.size() >= 3)
        {
            expectEqualU32(context, clusters[0].codePoints, U"\U0001F1FA\U0001F1F8", "first flag pair");
            expectEqualInt(context, clusters[0].displayWidth, 2, "first flag width");
            expectEqualU32(context, clusters[1].codePoints, U"\U0001F1E8\U0001F1E6", "second flag pair");
            expectEqualInt(context, clusters[1].displayWidth, 2, "second flag width");
            expectEqualU32(context, clusters[2].codePoints, U"x", "trailing text cluster");
        }
    }
}

namespace GraphemeSegmentationTests
{
    Result runAll()
    {
        const TestCase tests[] =
        {
            { "combiningMarkStaysWithBase", combiningMarkStaysWithBase },
            { "variationSelectorStaysWithBase", variationSelectorStaysWithBase },
            { "emojiModifierStaysWithBase", emojiModifierStaysWithBase },
            { "zeroWidthJoinerSequenceIsAtomic", zeroWidthJoinerSequenceIsAtomic },
            { "regionalIndicatorsPairAsFlags", regionalIndicatorsPairAsFlags }
        };

        Result result;
        result.totalCount = static_cast<int>(sizeof(tests) / sizeof(tests[0]));

        for (const TestCase& test : tests)
        {
            TestContext context;
            context.testName = test.name;
            test.function(context);

            if (context.failures.empty())
            {
                ++result.passedCount;
            }
            else
            {
                ++result.failedCount;
                result.failures.insert(result.failures.end(), context.failures.begin(), context.failures.end());
            }
        }

        return result;
    }

    std::string formatSummary(const Result& result)
    {
        std::ostringstream stream;
        stream << "GraphemeSegmentationTests: "
            << result.passedCount << '/' << result.totalCount << " passed";

        if (!result.success())
        {
            stream << " (" << result.failedCount << " failed)";
            for (const Failure& failure : result.failures)
            {
                stream << "\n- " << failure.testName << ": " << failure.message;
            }
        }

        return stream.str();
    }
}

