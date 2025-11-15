#include "http_client.h"
#include "logger.h"
#include <sstream>
#include <type_traits>

// Circuit Breaker Implementation
HTTPCircuitBreaker::HTTPCircuitBreaker(
    int failureThreshold,
    int successThreshold,
    int timeoutSeconds
) : state(State::CLOSED),
    failureCount(0),
    successCount(0),
    failureThreshold(failureThreshold),
    successThreshold(successThreshold),
    timeoutSeconds(timeoutSeconds)
{
    lastFailureTime.store(std::chrono::steady_clock::time_point::min());
}

bool HTTPCircuitBreaker::isTimeoutExpired() const {
    auto now = std::chrono::steady_clock::now();
    auto lastFailure = lastFailureTime.load();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastFailure);
    return elapsed.count() >= timeoutSeconds;
}

bool HTTPCircuitBreaker::shouldAttempt() {
    State currentState = state.load();
    
    if (currentState == State::CLOSED) {
        return true;  // Normal operation
    }
    
    if (currentState == State::OPEN) {
        // Check if timeout has expired
        if (isTimeoutExpired()) {
            LOG_INFO("Circuit breaker timeout expired, entering HALF_OPEN state");
            std::lock_guard<std::mutex> lock(mutex);
            state.store(State::HALF_OPEN);
            successCount.store(0);
            return true;  // Allow one attempt
        }
        LOG_DEBUG("Circuit breaker is OPEN, rejecting request");
        return false;  // Still in cooldown
    }
    
    // HALF_OPEN state - allow attempts to test if service recovered
    return true;
}

void HTTPCircuitBreaker::recordSuccess() {
    State currentState = state.load();
    
    if (currentState == State::HALF_OPEN) {
        int successes = successCount.fetch_add(1) + 1;
        LOG_DEBUG("Circuit breaker success count: %d/%d", successes, successThreshold);
        
        if (successes >= successThreshold) {
            std::lock_guard<std::mutex> lock(mutex);
            LOG_INFO("Circuit breaker entering CLOSED state (service recovered)");
            state.store(State::CLOSED);
            failureCount.store(0);
            successCount.store(0);
        }
    } else if (currentState == State::CLOSED) {
        // Reset failure count on success
        failureCount.store(0);
    }
}

void HTTPCircuitBreaker::recordFailure() {
    State currentState = state.load();
    
    lastFailureTime.store(std::chrono::steady_clock::now());
    int failures = failureCount.fetch_add(1) + 1;
    
    LOG_WARNING("Circuit breaker failure count: %d/%d", failures, failureThreshold);
    
    if (currentState == State::CLOSED && failures >= failureThreshold) {
        std::lock_guard<std::mutex> lock(mutex);
        LOG_ERROR("Circuit breaker opening due to repeated failures");
        state.store(State::OPEN);
    } else if (currentState == State::HALF_OPEN) {
        std::lock_guard<std::mutex> lock(mutex);
        LOG_WARNING("Circuit breaker reopening (failure during recovery test)");
        state.store(State::OPEN);
        failureCount.store(0);  // Reset for next timeout
    }
}

std::string HTTPCircuitBreaker::getStateString() const {
    switch (state.load()) {
        case State::CLOSED: return "CLOSED";
        case State::OPEN: return "OPEN";
        case State::HALF_OPEN: return "HALF_OPEN";
        default: return "UNKNOWN";
    }
}

// HTTP Client Implementation
HTTPClient::HTTPClient(const std::string& host, int port, bool useSSL, bool verifySSL)
    : host(host), port(port), useSSL(useSSL), verifySSL(verifySSL), circuitBreaker(3, 2, 60)
{
    LOG_INFO("HTTPClient created for %s://%s:%d", useSSL ? "https" : "http", host.c_str(), port);
}

template<typename ClientType>
void HTTPClient::configureClient(ClientType& client, int timeoutSeconds) {
    client.set_connection_timeout(timeoutSeconds, 0);
    client.set_read_timeout(timeoutSeconds, 0);
    client.set_write_timeout(timeoutSeconds, 0);
    client.set_follow_location(true);
    
    if constexpr (std::is_same_v<ClientType, httplib::SSLClient>) {
        client.enable_server_certificate_verification(verifySSL);
    }
}

HTTPClient::Response HTTPClient::get(const std::string& path, int timeoutSeconds) {
    Response response{false, 0, "", ""};
    
    // Check circuit breaker
    if (!circuitBreaker.shouldAttempt()) {
        response.error = "Circuit breaker is OPEN";
        LOG_WARNING("HTTP GET blocked by circuit breaker: %s", path.c_str());
        return response;
    }
    
    LOG_DEBUG("HTTP GET: %s://%s:%d%s", useSSL ? "https" : "http", host.c_str(), port, path.c_str());
    
    try {
        if (useSSL) {
            httplib::SSLClient client(host, port);
            configureClient(client, timeoutSeconds);
            
            auto res = client.Get(path.c_str());
            
            if (res) {
                response.success = true;
                response.statusCode = res->status;
                response.body = res->body;
                circuitBreaker.recordSuccess();
                LOG_DEBUG("HTTP GET successful, status: %d", res->status);
            } else {
                response.error = httplib::to_string(res.error());
                circuitBreaker.recordFailure();
                LOG_ERROR("HTTP GET failed: %s", response.error.c_str());
            }
        } else {
            httplib::Client client(host, port);
            configureClient(client, timeoutSeconds);
            
            auto res = client.Get(path.c_str());
            
            if (res) {
                response.success = true;
                response.statusCode = res->status;
                response.body = res->body;
                circuitBreaker.recordSuccess();
            } else {
                response.error = httplib::to_string(res.error());
                circuitBreaker.recordFailure();
            }
        }
    } catch (const std::exception& e) {
        response.error = e.what();
        circuitBreaker.recordFailure();
        LOG_ERROR("HTTP GET exception: %s", e.what());
    }
    
    return response;
}

HTTPClient::Response HTTPClient::post(
    const std::string& path,
    const std::string& body,
    const std::string& contentType,
    const httplib::Headers& headers,
    int timeoutSeconds
) {
    Response response{false, 0, "", ""};
    
    // Check circuit breaker
    if (!circuitBreaker.shouldAttempt()) {
        response.error = "Circuit breaker is OPEN";
        LOG_WARNING("HTTP POST blocked by circuit breaker: %s", path.c_str());
        return response;
    }
    
    LOG_DEBUG("HTTP POST: %s://%s:%d%s", useSSL ? "https" : "http", host.c_str(), port, path.c_str());
    
    try {
        if (useSSL) {
            httplib::SSLClient client(host, port);
            configureClient(client, timeoutSeconds);
            
            auto res = client.Post(path.c_str(), headers, body, contentType.c_str());
            
            if (res) {
                response.success = true;
                response.statusCode = res->status;
                response.body = res->body;
                circuitBreaker.recordSuccess();
                LOG_DEBUG("HTTP POST successful, status: %d", res->status);
            } else {
                response.error = httplib::to_string(res.error());
                circuitBreaker.recordFailure();
                LOG_ERROR("HTTP POST failed: %s", response.error.c_str());
            }
        } else {
            httplib::Client client(host, port);
            configureClient(client, timeoutSeconds);
            
            auto res = client.Post(path.c_str(), headers, body, contentType.c_str());
            
            if (res) {
                response.success = true;
                response.statusCode = res->status;
                response.body = res->body;
                circuitBreaker.recordSuccess();
            } else {
                response.error = httplib::to_string(res.error());
                circuitBreaker.recordFailure();
            }
        }
    } catch (const std::exception& e) {
        response.error = e.what();
        circuitBreaker.recordFailure();
        LOG_ERROR("HTTP POST exception: %s", e.what());
    }
    
    return response;
}