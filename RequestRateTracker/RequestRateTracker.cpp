//
// HTTP rate limiting module. See RequestRateTracker class header for details.
//
#include "RequestRateTracker.h"
#include "Poco/Mutex.h"
#include <regex>
#include <limits>

using Poco::Mutex;

RequestRateTracker::RequestRateTracker(RequestRate rateLimit, NowFunction* nowFunction)
    : rateLimit(rateLimit), nowFunction(nowFunction)
    , currentWindowStart(std::numeric_limits<decltype(currentWindowStart)>::lowest())
{
    appStartTime = nowFunction();
}

RequestRateTracker::~RequestRateTracker()
{
}

RequestRate::Seconds RequestRateTracker::addRequest(HTTPClientID client)
    /// Track another request for HTTP client. 'client' must be unique for
    /// each HTTP requester.
    /// If no clients were added to the RequestRateTracker then all clients
    /// will be rate-limited.
    /// The return is a number of seconds to wait before a request is
    /// allowed or 0 if current request is within preset rate limit.
{
    auto now = nowFunction();
    auto sinceStart = std::chrono::duration_cast<std::chrono::seconds>(now - appStartTime);
    RequestRate::Seconds secSinceStart = (RequestRate::Seconds)sinceStart.count();
    RequestRate::Seconds waitTime = 0;
    {
        Mutex::ScopedLock lock(requestCountsMutex);

        if (!clients.empty() 
            && (clients.find(client) == clients.end())) {
                return 0;
        }
        if (secSinceStart >= currentWindowStart &&
            secSinceStart < (currentWindowStart + rateLimit.period))
        {
            // Request was made within the current window
            int& requestCount = requestCounts[client];
            if (requestCount < rateLimit.num) {
                requestCount++;
            }
            else {
                waitTime = rateLimit.period - (secSinceStart - currentWindowStart);
            }
        }
        else {
            // Request was made beyond the current window or this is the first request.
            requestCounts.clear();
            currentWindowStart = secSinceStart - (secSinceStart % rateLimit.period);
            requestCounts[client] = 1;
        }
    }
    return waitTime;
}

RequestRateTracker::HTTPClientID RequestRateTracker::getClientId(
    const std::string& clientAddressStr)
    /// Creates unique integer client ID based on its IP address.
    /// If unique ID cannot be generated then the return is 0.
{
    std::smatch results;
    static const std::regex  
        expr("\\b([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\.([0-9]{1,3})\\b");

    // Current implementation does not support IPv6
    if (!std::regex_search(clientAddressStr, results, expr) || results.size() != 5)
        return 0;

    RequestRateTracker::HTTPClientID    clientId = 0;
    for (size_t i = 1; i < results.size(); i++) {
        clientId = (clientId << CHAR_BIT) | (std::stoul(results[i]) & 0xFF);
    }
    return clientId;
}

size_t RequestRateTracker::size() const
{
    Mutex::ScopedLock lock(requestCountsMutex);

    return requestCounts.size();
}

void RequestRateTracker::addClient(HTTPClientID id)
{
    Mutex::ScopedLock lock(requestCountsMutex);
    if (id != 0)
        clients.emplace(id);
}
