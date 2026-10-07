#include "lxe/sys/file.hpp"
#include "lxe/sys/unique_fd.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <unistd.h>

#include <filesystem>
#include <format>
#include <string>

using lxe::sys::read_file;

namespace {

/// A temporary file that is removed again at the end of the test.
class TempFile
{
public:
    explicit TempFile(const std::string& content)
    : path_{std::filesystem::temp_directory_path() / std::format("lxe-read-file-test-{}", ::getpid())}
    {
        const lxe::sys::UniqueFd fd{::open(path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600)};
        REQUIRE(fd.valid());
        REQUIRE(::write(fd.get(), content.data(), content.size()) == static_cast<ssize_t>(content.size()));
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    TempFile(TempFile&&) = delete;
    TempFile& operator=(TempFile&&) = delete;

    ~TempFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

TEST_CASE("sys.read_file.reads_files_larger_than_one_chunk")
{
    const std::string content(10'000, 'x');
    const TempFile file{content};
    CHECK(read_file(file.path()) == content);
}

TEST_CASE("sys.read_file.stops_at_the_limit")
{
    const TempFile file{std::string(500, 'y')};
    CHECK(read_file(file.path(), 100) == std::string(100, 'y'));
}

TEST_CASE("sys.read_file.reads_proc_files_that_report_size_zero")
{
    const auto stat = read_file("/proc/self/stat");
    REQUIRE(stat.has_value());
    CHECK(std::filesystem::file_size("/proc/self/stat") == 0);
    CHECK_FALSE(stat->empty());
}

TEST_CASE("sys.read_file.reports_errors")
{
    const auto missing = read_file("/nonexistent/file");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == std::errc::no_such_file_or_directory);
}
