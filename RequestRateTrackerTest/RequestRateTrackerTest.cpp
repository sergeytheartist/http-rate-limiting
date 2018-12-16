// 
// Console-based set of tests for RequestRateTracker.
//
// CppUnit test framework:
//   Poco's restricted version of CppUnit is reused: CppUnit extensions are not available
//   to make test less verbose.
//
//   Poco's CppUnit was fixed to supporte assertEqual for long long.
//
// Status: 
//   Current implementation passes all the tests for "Fixed Window" implementation
//   of rate-limiting. The first test for "Sliding Window" rate-limiting algorithm reveals
//   disadvantage of "Fixed Window" algorithm (and it fails). The other planned tests for
//   "Sliding Window" algorithm are listed at the bottom of this file.
//
// References:
//   https://konghq.com/blog/how-to-design-a-scalable-rate-limiting-algorithm/
//      A good short summary of rate-limiting algorithms and their pros and cons.
//
#include "pch.h"
#include "RequestRateTracker.h"

class RequestRateTrackerTest : public CppUnit::TestCase
{
public:
    RequestRateTrackerTest(const std::string& name) : CppUnit::TestCase(name)
    {
    }
    ~RequestRateTrackerTest() = default;

    void testFirstRequest();
    void testNonFirstRequestsForSamePeriod();
    void testBinaryClientId();
    void testInvalidBinaryClientId();
    void testNonFirstRequestsForNextPeriod();
    void testMemoryReclaimed();
    void testOneClientIsRateLimited();
    void testRequestDeniedWhenManyRequestsAreAtBoundary();

    void setUp()
    {
        ManualClock::reset();
        rateLimit.num = 2;
        rateLimit.period = RequestRate::Seconds(10);
        requestRateTracker = new RequestRateTracker(rateLimit, ManualClock::now);
    }
    void tearDown() 
    {
        delete requestRateTracker;
    }

    static CppUnit::Test* suite();

private:
    RequestRate         rateLimit;
    // Object under test
    RequestRateTracker* requestRateTracker;

    class ManualClock 
        /// Provides clock which does not move, unless it is advanced manually.
    {
    public:
        using rep = std::chrono::steady_clock::rep;
        using period = std::chrono::steady_clock::period;
        using duration = std::chrono::steady_clock::duration;
        using time_point = std::chrono::time_point<std::chrono::steady_clock>;
        static const bool is_steady = false;

        static void advance(duration d) {
            timeNow += d;
        }
        static void reset() {
            timeNow = time_point(duration(0));
        }
        static time_point now() {
            return timeNow;
        }

    private:
        static time_point timeNow;
    };
};

RequestRateTrackerTest::ManualClock::time_point
    RequestRateTrackerTest::ManualClock::timeNow(time_point(duration(0)));

CppUnit::Test* RequestRateTrackerTest::suite()
{
    CppUnit::TestSuite* pSuite = new CppUnit::TestSuite("RequestRateTrackerTest");

    CppUnit_addTest(pSuite, RequestRateTrackerTest, testFirstRequest);
    CppUnit_addTest(pSuite, RequestRateTrackerTest, testNonFirstRequestsForSamePeriod);
    CppUnit_addTest(pSuite, RequestRateTrackerTest, testBinaryClientId);
    CppUnit_addTest(pSuite, RequestRateTrackerTest, testInvalidBinaryClientId);
    CppUnit_addTest(pSuite, RequestRateTrackerTest, testNonFirstRequestsForNextPeriod);
    CppUnit_addTest(pSuite, RequestRateTrackerTest, testMemoryReclaimed);
    CppUnit_addTest(pSuite, RequestRateTrackerTest, testOneClientIsRateLimited);
    CppUnit_addTest(pSuite, RequestRateTrackerTest,
        testRequestDeniedWhenManyRequestsAreAtBoundary);

    return pSuite;
}

void RequestRateTrackerTest::testFirstRequest()
{
    auto waitTime = requestRateTracker->addRequest(33);
    assertEqual(RequestRate::Seconds(0), waitTime);
}

void RequestRateTrackerTest::testNonFirstRequestsForSamePeriod()
{
    RequestRate::Seconds waitTime[3];
    waitTime[0] = requestRateTracker->addRequest(33);
    waitTime[1] = requestRateTracker->addRequest(33);
    waitTime[2] = requestRateTracker->addRequest(33);
    assertEqual(RequestRate::Seconds(0), waitTime[1]);
    assertEqual(RequestRate::Seconds(10), waitTime[2]);
}

void RequestRateTrackerTest::testBinaryClientId()
{
    std::string addr("127.0.0.1");
    RequestRateTracker::HTTPClientID clientId = RequestRateTracker::getClientId(addr);
    assertEqual(0x7F000001, clientId);
}

void RequestRateTrackerTest::testInvalidBinaryClientId()
{
    std::string addr("127.0.XXX.XXX");
    RequestRateTracker::HTTPClientID clientId = RequestRateTracker::getClientId(addr);
    assertEqual(0, clientId);
}

void RequestRateTrackerTest::testNonFirstRequestsForNextPeriod()
{
    // Request issued in the (k+1)th sampling period must not be
    // affected by requests in the (k)th sampling period.
    RequestRate::Seconds waitTime[4];
    waitTime[0] = requestRateTracker->addRequest(33);

    ManualClock::advance(std::chrono::seconds(11));

    waitTime[1] = requestRateTracker->addRequest(33);
    waitTime[2] = requestRateTracker->addRequest(33);
    waitTime[3] = requestRateTracker->addRequest(33);
    assertEqual(0, waitTime[1]);
    assertEqual(0, waitTime[2]);
    assertEqual(9, waitTime[3]);
}

void RequestRateTrackerTest::testMemoryReclaimed()
{
    RequestRate rate = requestRateTracker->getRateLimit();
    assertEqual(10, rate.period);
    assertEqual(0, requestRateTracker->size());

    ManualClock::advance(std::chrono::seconds(103));    // t = +3 in window 10
    requestRateTracker->addRequest(11);
    ManualClock::advance(std::chrono::seconds(1));      // t = +4 in window 10
    requestRateTracker->addRequest(22);
    requestRateTracker->addRequest(11);
    assertEqual(2, requestRateTracker->size());

    ManualClock::advance(std::chrono::seconds(9));      // t = +3 in window 11
    requestRateTracker->addRequest(33);
    assertEqual(1, requestRateTracker->size());

    ManualClock::advance(std::chrono::seconds(16));     // t = +9 in window 12
    requestRateTracker->addRequest(33);
    assertEqual(1, requestRateTracker->size());
}

void RequestRateTrackerTest::testOneClientIsRateLimited()
    /// It is required that a particular requester is rate-limited.
    /// Tests that only particular HTTP client is rate-limited.
{
    RequestRate rate = requestRateTracker->getRateLimit();
    assertEqual(2, rate.num);
    auto id1 = RequestRateTracker::getClientId("127.0.0.1");
    auto id2 = RequestRateTracker::getClientId("127.0.0.2");
    assert(id1 != id2);

    requestRateTracker->addClient(id1);

    requestRateTracker->addRequest(id1);
    requestRateTracker->addRequest(id2);
    requestRateTracker->addRequest(id1);
    requestRateTracker->addRequest(id2);
    auto waitTime1 = requestRateTracker->addRequest(id1);
    auto waitTime2 = requestRateTracker->addRequest(id2);

    assertEqual(rate.period, waitTime1);
    assertEqual(0, waitTime2);
}

void RequestRateTrackerTest::testRequestDeniedWhenManyRequestsAreAtBoundary()
    /// Sliding window logic must deny adding the 1st request early in the current fixed window
    /// when current request is less than 30% through in the current fixed window and 2 requests
    /// (maximum allowed) were added at the end of the previous fixed window.
{
    // Ensure setup did not change
    RequestRate rate = requestRateTracker->getRateLimit();
    assertEqual(2, rate.num);
    assertEqual(10, rate.period);

    // Add two requests at the end of the 1st fixed window
    ManualClock::advance(std::chrono::seconds(rate.period - 1)); // time = +9
    RequestRate::Seconds waitTime[3];
    waitTime[0] = requestRateTracker->addRequest(33);
    waitTime[1] = requestRateTracker->addRequest(33);

    // Check that attempt to add at the start of the 2nd fixed window denied
    ManualClock::advance(std::chrono::seconds(3)); // time = +12
    waitTime[2] = requestRateTracker->addRequest(33);
    assertEqual(0, waitTime[0]);
    assertEqual(0, waitTime[1]);
    // Expected wait time = 
    // period * (1 - (num - (curWinCount + 1))/prevWinCount) + curWinStart - 1 - now =
    // 10 * (1 - (2 - (0 + 1))/2) + 10 - 1 - 12 = 2.
    assertEqual(2, waitTime[2]);
}

// TODO: Implement 3 more cases for sliding window checks:
// - Same as testRequestDeniedWhenManyRequestsAreAtBoundary but only 1 request in
//   the previous fixed window. The 1st add must be ok, 2nd add must be denied.
// - Same as testRequestDeniedWhenManyRequestsAreAtBoundary but 60% through in
//   the current fixed window. The 1st add must be ok, 2nd add must be denied.
// - Same as testRequestDeniedWhenManyRequestsAreAtBoundary but 100% through in
//   the current fixed window. Both requests must be allowed, without being affected
//   by the requests in the previous fixed window.

CppUnitMain(RequestRateTrackerTest)
