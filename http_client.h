#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <httplib.h>

class HTTPCircuitBreaker {
public:
    enum class State { CLOSED, OPEN, HALF_OPEN };
    
    HTTPCircuitBreaker(
        int failureThreshold = 3,
        int successThreshold = 2,
        int timeoutSeconds = 60
    );
    
    // Check if we should attempt a request
    bool shouldAttempt();
    
    // Record result of request
    void recordSuccess();
    void recordFailure();
    
    // Get current state for logging
    State getState() const { return state.load(); }
    std::string getStateString() const;
    
private:
    std::atomic<State> state;
    std::atomic<int> failureCount;
    std::atomic<int> successCount;
    std::atomic<std::chrono::steady_clock::time_point> lastFailureTime;
    
    const int failureThreshold;
    const int successThreshold;
    const int timeoutSeconds;
    
    mutable std::mutex mutex;
    
    bool isTimeoutExpired() const;
};

class HTTPClient {
public:
    struct Response {
        bool success;
        int statusCode;
        std::string body;
        std::string error;
    };
    
    HTTPClient(const std::string& host, int port = 443, bool useSSL = true, bool verifySSL = true);
    
    // GET request with circuit breaker protection
    Response get(const std::string& path, int timeoutSeconds = 5);
    
    // POST request with circuit breaker protection
    Response post(const std::string& path, const std::string& body,
                 const std::string& contentType = "application/json",
                 const httplib::Headers& headers = {},
                 int timeoutSeconds = 5);
    
    // Get circuit breaker state for monitoring
    HTTPCircuitBreaker::State getCircuitState() const {
        return circuitBreaker.getState();
    }
    
private:
    std::string host;
    int port;
    bool useSSL;
    bool verifySSL;
    HTTPCircuitBreaker circuitBreaker;
    
    // Helper to configure client with timeouts
    template<typename ClientType>
    void configureClient(ClientType& client, int timeoutSeconds);
};

#endif // HTTP_CLIENT_H