# http-rate-limiting
A rate-limiting module that stops a particular requestor from making too many
requests within a particular period of time.

## Project structure
RequestRateTracker/
	Source code for the rate-limiting module.

RequestRateTrackerTest/
	Suite of automated tests for the rate-limiting module. Tests use Poco's version
	of CppUnit test framework.

HttpBasicServer/
	A basic http server which serves current time to demonstrate rate-limiting module
	usage in real server. Rate is limited to 100 requests per hour by default for
	all requesters. The default rate can be overwritten in HttpBasicServer.properties
	file which is placed in the executable directory.
	
HttpBasicServer/Debug/HttpBasicServer.properties
	Example of properties file for HttpBasicServer which limits rate for all
	at 2 requests per 10 seconds (RPS = requests per second)

## How to run HttBasicServer demo
1. Open HttpRateLimiting.sln and buid debug x86 application
2. Run the application. This step will start the server with rate limit=2/10 RPS
3. Open page http://localhost:9980/ with any browser
4. Keep refreshing the page to see that rate is limited at 2/10 RPS
   (You must have HttpBasicServer.properties in Debug folder or the default 100 RPH
   will be applied!)

## Configuring Http Server
Available options are documented in /Debug/HttpBasicServer.properties file.
If a particular client is not set in the properties file then ALL clients will
be rate-limited.

## Building notes
This solution uses Poco networking libraries https://pocoproject.org/, which must
be installed for the server and tests to compile and link.

You can build tests and the rate-limiting module without Poco 
(but not the server) if you replace Poco::Mutex and Poco::ScopedLock with
std::mutex and std::lock_guard.

## Limitations
Due to time constraints I used only Win32 installation of Poco and solution is
not configured to build x64 configurations.

Current implementation uses "Fixed Window" algorithm. "Sliding window" algorithm
tests are documented and one of them is implemented.
