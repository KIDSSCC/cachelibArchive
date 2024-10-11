#include<iostream>
#include<mutex>
using namespace std;

enum class LogLevel {
    K_DEBUG,
    INFO,
    WARNING,
    ERROR
};

class Logger {
public:
    Logger(LogLevel level = LogLevel::INFO) : logLevel(level) {}
    void setLogLevel(LogLevel level){
        logLevel = level;
    }

    void log(LogLevel level, const std::string& message){
        if(level >= logLevel){
            lock_guard<mutex> lock(mutex_);
            string logMessage = " [" + getLabel(level) + "] " + message + "\n";
            cout << logMessage;
        }
    }

    void k_debug(const std::string& message){
        log(LogLevel::K_DEBUG, message);
    }

    void info(const std::string& message) {
        log(LogLevel::INFO, message);
    }

    void warning(const std::string& message) {
        log(LogLevel::WARNING, message);
    }

    void error(const std::string& message) {
        log(LogLevel::ERROR, message);
    }

    ~Logger() {}

private:
    LogLevel logLevel;
    mutex mutex_;

    std::string getLabel(LogLevel level) {
        switch (level) {
            case LogLevel::K_DEBUG: return "DEBUG";
            case LogLevel::INFO: return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

};