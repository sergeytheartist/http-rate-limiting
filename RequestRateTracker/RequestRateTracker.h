#ifndef REQUEST_RATE_TRACKER_H
#define REQUEST_RATE_TRACKER_H

#include <chrono>
#include <cstdint>
#include <string>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include "Poco/Mutex.h"

using Poco::Mutex;

struct RequestRate
    /// Responsible for storing together parameters for http requests
    /// rate.
{
    using Seconds = long;
    int     num;
        /// Number of requests.
    Seconds period;
        /// Sampling period in seconds.
};

class RequestRateTracker
    /// Responsible for tracking requests rates for individual clients,
    /// based on their IP address and provided request rate limit.
    /// 
    /// Instantiate RequestRateTracker with the desired request rate
    /// limit. Convert client's address to HTTPClientID and pass it to
    /// addRequest method. If request will not exceed the rate limit,
    /// return of the method is 0. When rate limit is exceeded the
    /// return of the method is number of seconds to wait before
    /// request is allowed.
    ///
    /// Client to keep track of are added with addClient() method, which
    /// is thread-safe.
    ///
    /// If no clients were added to the RequestRateTracker then all clients
    /// will be rate-limited.
{
public:
    using HTTPClientID = uint32_t;

    typedef std::chrono::steady_clock::time_point NowFunction();

    RequestRateTracker(RequestRate rateLimit,
        NowFunction* nowFunction = std::chrono::steady_clock::now);

    ~RequestRateTracker();

    RequestRate::Seconds addRequest(HTTPClientID client);

    RequestRate         getRateLimit() const { return rateLimit; }

    size_t              size() const;

    static HTTPClientID getClientId(const std::string& clientAddressStr);

    void                addClient(HTTPClientID);

private:
    RequestRate             rateLimit;
        /// Requests arriving at the rate higher than this limit must be denied.

    mutable Mutex           requestCountsMutex;
        /// This mutex must be locked to access the following members:
        ///     - currentWindowStart
        ///     - requestCounts
        ///     - clientsToTrack

    RequestRate::Seconds    currentWindowStart;
        /// Time when request counters started to accumulate for the 
        /// current rate calculation period (refered to as "window").

    using RequestCountHashTable = std::unordered_map<HTTPClientID, int>;
    RequestCountHashTable   requestCounts;
        /// Accumulated number of requests per client for the current window. 

    using ClientSet = std::unordered_set<HTTPClientID>;
    ClientSet               clients;
        /// Clients who must be tracked. If this set is empty then all
        /// clients are tracked.

    std::chrono::time_point<std::chrono::steady_clock> 
                            appStartTime;

    NowFunction*            nowFunction;
};

#endif // REQUEST_RATE_TRACKER_H

