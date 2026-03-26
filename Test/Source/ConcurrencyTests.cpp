#include "TestSupport.h"

using namespace Life::Tests;

TEST_CASE("ApplicationHost initializes shared JobSystem and AsyncIO services")
{
    CHECK_FALSE(Life::GetJobSystem().IsInitialized());
    CHECK_FALSE(Life::Async::GetAsyncIO().IsInitialized());

    {
        auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        CHECK(Life::GetJobSystem().IsInitialized());
        CHECK(Life::Async::GetAsyncIO().IsInitialized());
        CHECK(&host->GetServices().Get<Life::JobSystem>() == &Life::GetJobSystem());
        CHECK(&host->GetServices().Get<Life::Async::AsyncIO>() == &Life::Async::GetAsyncIO());
    }

    CHECK_FALSE(Life::GetJobSystem().IsInitialized());
    CHECK_FALSE(Life::Async::GetAsyncIO().IsInitialized());
}

TEST_CASE("Rejecting a second ApplicationHost preserves shared JobSystem and AsyncIO lifetime")
{
    auto outerHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    REQUIRE(Life::GetJobSystem().IsInitialized());
    REQUIRE(Life::Async::GetAsyncIO().IsInitialized());

    try
    {
        [[maybe_unused]] auto innerHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        FAIL("Expected second host creation to throw");
    }
    catch (const Life::Error& error)
    {
        CHECK(error.GetCode() == Life::ErrorCode::InvalidState);
    }

    CHECK(Life::GetJobSystem().IsInitialized());
    CHECK(Life::Async::GetAsyncIO().IsInitialized());

    outerHost.reset();

    CHECK_FALSE(Life::GetJobSystem().IsInitialized());
    CHECK_FALSE(Life::Async::GetAsyncIO().IsInitialized());
}

TEST_CASE("Repeated ApplicationHost churn cleanly reinitializes shared JobSystem and AsyncIO services")
{
    for (int iteration = 0; iteration < 3; ++iteration)
    {
        CHECK_FALSE(Life::GetJobSystem().IsInitialized());
        CHECK_FALSE(Life::Async::GetAsyncIO().IsInitialized());

        {
            auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
            REQUIRE(Life::GetJobSystem().IsInitialized());
            REQUIRE(Life::Async::GetAsyncIO().IsInitialized());

            std::atomic<int> completedJobs = 0;
            REQUIRE(Life::GetJobSystem().Submit([&completedJobs]() {
                completedJobs.fetch_add(1, std::memory_order_acq_rel);
            }));
            Life::GetJobSystem().Wait();
            CHECK(completedJobs.load(std::memory_order_acquire) == 1);

            Life::Async::Task<int> task = Life::Async::GetAsyncIO().RunAsync([iteration]() {
                return iteration + 10;
            });
            CHECK(task.Get() == iteration + 10);
        }

        CHECK_FALSE(Life::GetJobSystem().IsInitialized());
        CHECK_FALSE(Life::Async::GetAsyncIO().IsInitialized());
        CHECK_FALSE(Life::GetJobSystem().TrySubmit([]() {}));

        Life::Async::Task<int> inlineTask = Life::Async::GetAsyncIO().RunAsync([iteration]() {
            return iteration + 100;
        });
        CHECK(inlineTask.IsDone());
        CHECK(inlineTask.Get() == iteration + 100);
        CHECK_FALSE(Life::Async::GetAsyncIO().IsInitialized());
    }
}

TEST_CASE("AsyncIO result-returning file reads preserve missing-path failures")
{
    auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    const std::filesystem::path missingPath = std::filesystem::temp_directory_path() / "LifeTests" / "MissingAsyncRead" / "does-not-exist.txt";

    const Life::Result<std::string> result = Life::Async::GetAsyncIO().ReadFileAsyncResult(missingPath.string()).Get();

    REQUIRE(result.IsFailure());
    CHECK(result.GetError().GetCode() == Life::ErrorCode::FileNotFound);
    CHECK(result.GetError().GetErrorMessage().find(missingPath.string()) != std::string::npos);
}

TEST_CASE("JobSystem shutdown rejects racing submissions and drains in-flight jobs")
{
    auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    auto& jobSystem = Life::GetJobSystem();

    std::atomic<bool> blockerStarted = false;
    std::atomic<bool> releaseBlocker = false;
    std::atomic<bool> blockerFinished = false;

    REQUIRE(jobSystem.Submit([&]() {
        blockerStarted.store(true, std::memory_order_release);
        while (!releaseBlocker.load(std::memory_order_acquire))
            std::this_thread::yield();
        blockerFinished.store(true, std::memory_order_release);
    }));
    REQUIRE(WaitForCondition([&]() { return blockerStarted.load(std::memory_order_acquire); }, std::chrono::milliseconds(1000)));

    std::thread destroyer([&]() {
        host.reset();
    });

    const bool sawRejectedSubmission = WaitForCondition([&]() {
        return !jobSystem.TrySubmit([]() {});
    }, std::chrono::milliseconds(2000));

    releaseBlocker.store(true, std::memory_order_release);
    destroyer.join();

    CHECK(sawRejectedSubmission);
    CHECK(blockerFinished.load(std::memory_order_acquire));
    CHECK_FALSE(jobSystem.IsInitialized());
    CHECK_FALSE(jobSystem.TrySubmit([]() {}));
}

TEST_CASE("AsyncIO queue saturation executes overflow work inline")
{
    auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    auto& asyncIO = Life::Async::GetAsyncIO();

    std::atomic<bool> blockerStarted = false;
    std::atomic<bool> releaseBlocker = false;
    Life::Async::Task<void> blockerTask = asyncIO.RunAsync([&]() {
        blockerStarted.store(true, std::memory_order_release);
        while (!releaseBlocker.load(std::memory_order_acquire))
            std::this_thread::yield();
    });
    REQUIRE(WaitForCondition([&]() { return blockerStarted.load(std::memory_order_acquire); }, std::chrono::milliseconds(1000)));

    constexpr std::size_t taskCount = 8704;
    std::atomic<int> completedTasks = 0;
    std::vector<Life::Async::Task<void>> tasks;
    tasks.reserve(taskCount);
    for (std::size_t index = 0; index < taskCount; ++index)
    {
        tasks.emplace_back(asyncIO.RunAsync([&]() {
            completedTasks.fetch_add(1, std::memory_order_acq_rel);
        }));
    }

    const int completedBeforeRelease = completedTasks.load(std::memory_order_acquire);
    releaseBlocker.store(true, std::memory_order_release);
    blockerTask.Wait();
    for (Life::Async::Task<void>& task : tasks)
        task.Wait();

    CHECK(completedBeforeRelease > 0);
    CHECK(completedTasks.load(std::memory_order_acquire) == static_cast<int>(taskCount));
}

TEST_CASE("AsyncIO submissions racing shutdown complete inline without reinitializing")
{
    auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    auto& asyncIO = Life::Async::GetAsyncIO();

    std::atomic<bool> blockerStarted = false;
    std::atomic<bool> releaseBlocker = false;
    Life::Async::Task<void> blockerTask = asyncIO.RunAsync([&]() {
        blockerStarted.store(true, std::memory_order_release);
        while (!releaseBlocker.load(std::memory_order_acquire))
            std::this_thread::yield();
    });
    REQUIRE(WaitForCondition([&]() { return blockerStarted.load(std::memory_order_acquire); }, std::chrono::milliseconds(1000)));

    std::thread destroyer([&]() {
        host.reset();
    });

    bool sawInlineCompletion = false;
    int inlineResult = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (std::chrono::steady_clock::now() < deadline)
    {
        Life::Async::Task<int> task = asyncIO.RunAsync([]() {
            return 77;
        });
        if (task.IsDone())
        {
            sawInlineCompletion = true;
            inlineResult = task.Get();
            break;
        }
        std::this_thread::yield();
    }

    releaseBlocker.store(true, std::memory_order_release);
    blockerTask.Wait();
    destroyer.join();

    CHECK(sawInlineCompletion);
    CHECK(inlineResult == 77);
    CHECK_FALSE(asyncIO.IsInitialized());

    Life::Async::Task<int> postShutdownTask = asyncIO.RunAsync([]() {
        return 91;
    });
    CHECK(postShutdownTask.IsDone());
    CHECK(postShutdownTask.Get() == 91);
    CHECK_FALSE(asyncIO.IsInitialized());
}
