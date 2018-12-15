// 
// Demo which shows how to use rate-limiting module (RequestRateTracker).
// 
// Running HttpBasicServer will start a simple application that serves
// time. When number of requests exceeds maximum rate specified in 
// HttpBasicServer.properties file a page with limit exceeded message is
// presented.
// 
// Use http://localhost:9980/ to try it manually. If HttpBasicServer.properties
// is not created, then default rate is limited to 100 requests per hour.
//
// This code uses some of the ideas presented in Poco framework samples.
//
#include "pch.h"
#include "RequestRateTracker.h"

using Poco::Net::ServerSocket;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;
using Poco::Net::HTTPResponse;
using Poco::Net::HTTPServer;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;
using Poco::Net::HTTPServerParams;
using Poco::Timespan;
using Poco::Timestamp;
using Poco::DateTimeFormatter;
using Poco::DateTimeFormat;
using Poco::ThreadPool;
using Poco::Util::ServerApplication;
using Poco::Util::Application;

class ServiceUnavailableHandler : public HTTPRequestHandler
    /// Returns HTTP response with status 503 (Service Unavailable)
{
public:
    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
    {
        Application& app = Application::instance();
        app.logger().information("Cannot limit rate for " + request.clientAddress().toString());
        response.setContentLength(0);
        response.setStatusAndReason(HTTPResponse::HTTP_SERVICE_UNAVAILABLE);
        response.setContentType("text/html");
        auto& ostr = response.send();
    }
};

class RateLimitExceededHandler : public HTTPRequestHandler
    /// Returns HTTP response with status 429 (Too Many Requests) and text showing
    /// how long before next request will be allowed.
{
public:
    RateLimitExceededHandler(RequestRate::Seconds waitTime) : waitTime(waitTime)
    {
    }

    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
    {
        Application& app = Application::instance();
        app.logger().information("Request from " + request.clientAddress().toString()
            + " ignored");

        response.setChunkedTransferEncoding(true);
        response.setContentType("text/html");

        std::string waitTimeStr = std::to_string(waitTime);
        std::string reason("Rate limit exceeded. Try again in " + waitTimeStr + " seconds.");
        HTTPResponse::HTTPStatus status = HTTPResponse::HTTP_TOO_MANY_REQUESTS;
        response.setStatusAndReason(status, reason);

        auto& ostr = response.send();
        ostr << "<html><head><title>HTTPBaseServer with limited requests rate</title></head>";
        ostr << "<body>";
        ostr << "<p style=\"text-align: center;\">" << reason << "</p>";
        ostr << "</body></html>";
    }

private:
    RequestRate::Seconds waitTime;
};

class TimeRequestHandler : public HTTPRequestHandler
    /// Returns a HTML document with the current date and time.
{
public:
    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response)
    {
        Application& app = Application::instance();
        app.logger().information("Request from " + request.clientAddress().toString());

        Timestamp now;
        std::string dt(DateTimeFormatter::format(now, DateTimeFormat::SORTABLE_FORMAT));

        response.setChunkedTransferEncoding(true);
        response.setContentType("text/html");

        std::ostream& ostr = response.send();
        ostr << "<html><head><title>HTTPBaseServer with limited requests rate</title></head>";
        ostr << "<body>";
        ostr << "<p style=\"text-align: center; font-size: 48px;\">" << dt << "</p>";
        ostr << "</body></html>";
    }
};

class TimeRequestHandlerFactory : public HTTPRequestHandlerFactory
{
public:
    TimeRequestHandlerFactory(RequestRate rateLimit) : rateTracker(rateLimit)
    {
    }
    
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest& request)
    {
        if (request.getURI() == "/") {
            // Track requests rate and deny service if rate exceeded the limit
            std::string clientAddrStr = request.clientAddress().toString();
            RequestRateTracker::HTTPClientID clientId = 
                RequestRateTracker::getClientId(clientAddrStr);

            if (clientId == 0)
                return new ServiceUnavailableHandler();

            RequestRate::Seconds waitTime = rateTracker.addRequest(clientId);
            if (waitTime > 0)
                return new RateLimitExceededHandler(waitTime);

            // Provide the client with the timer service
            return new TimeRequestHandler();
        } 
        else {
            return 0;
        }
    }

private:
    RequestRateTracker  rateTracker;
};

class HTTPBasicServer : public Poco::Util::ServerApplication
    /// The main application class.
    ///
    /// This class handles command-line arguments and
    /// configuration files.
    /// Start the HTTPBasicServer executable with the help
    /// option (/help on Windows, --help on Unix) for
    /// the available command line options.
    ///
    /// To use the sample configuration file (HTTPBasicServer.properties),
    /// copy the file to the directory where the HTTPTimeServer executable
    /// resides. If you start the debug version of the HTTPBasicServer
    /// (HTTPBasicServerd[.exe]), you must also create a copy of the configuration
    /// file named HTTPBasicServerd.properties. In the configuration file, you
    /// can specify the port on which the server is listening (default
    /// 9980) and the format of the date/time string sent back to the client.
    ///
    /// To test the rate limiting abilities of HTTPBasicServer you can use any
    /// web browser (http://localhost:9980/).
{
public:
    HTTPBasicServer()
    {
    }

    ~HTTPBasicServer()
    {
    }

protected:
    void initialize(Application& self)
    {
        loadConfiguration(); // load default configuration files, if present
        ServerApplication::initialize(self);
    }

    void uninitialize()
    {
        ServerApplication::uninitialize();
    }

    int main(const std::vector<std::string>& args)
    {
        // get parameters from configuration file
        unsigned short port = (unsigned short)config().getInt("HTTPBasicServer.port", 9980);
        RequestRate rateLimit{
            config().getInt("HTTPBasicServer.rateLimitRequests", 100),
            config().getInt("HTTPBasicServer.rateLimitPeriod", 3600)
        };

        HTTPServerParams* params = new HTTPServerParams;
        ServerSocket socket(port);
        HTTPServer server(new TimeRequestHandlerFactory(rateLimit), socket, params);
        server.start();
        std::cout << "HTTPBasicServer started. Port=" << port 
            << " RequestsPerSecondLimit=" << rateLimit.num << "/" << rateLimit.period
            << std::endl;
        // wait for CTRL-C or kill
        waitForTerminationRequest();
        server.stop();
        std::cout << "HTTPBasicServer stopped" << std::endl;

        return Application::EXIT_OK;
    }
};

int main(int argc, char** argv)
{
    HTTPBasicServer app;
    return app.run(argc, argv);
}
