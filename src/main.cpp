#include <iostream>
#include <string>
#include <limits>
#include <atomic>
#include <stdexcept>
#include <cstdlib>
#include <csignal>
#include <cstdio>
#include <boost/program_options.hpp>
#include "./config.hpp"
#include "./core.hpp"
#include "utils/string.hpp"

namespace
{
    std::atomic_bool shutdownRequested = false;
    Core *activeCore = nullptr;

    void handleShutdownSignal(int)
    {
        if (shutdownRequested.exchange(true))
        {
            return;
        }

        std::cout << "\nCtrl+C detected, stopping server..." << std::endl;

        if (activeCore)
        {
            activeCore->stop();
        }
    }

    void installSignalHandlers(Core &core)
    {
        activeCore = &core;
        std::signal(SIGINT, handleShutdownSignal);
#ifdef SIGTERM
        std::signal(SIGTERM, handleShutdownSignal);
#endif
    }
} // namespace

int main(int argc, char *argv[])
{
    namespace po = boost::program_options;

    po::options_description optionsDescription("Allowed options");
    optionsDescription.add_options()("help,h", "Show help message")                                                      // help option
        ("version,v", "Show version information")                                                                        // version option
        ("path,p", po::value<std::string>()->implicit_value(""), "Current directory path")                               // path option
        ("uploads,u", po::value<std::string>()->implicit_value(""), "Uploads directory path (default: Downloads/accio)") // uploads option
        ("host", po::value<std::string>()->implicit_value(""), "Server host (default: 0.0.0.0)")                         // host option
        ("port", po::value<std::string>()->implicit_value(""), "Server port (default: 13396)")                           // port option
        ("password", po::value<std::string>()->implicit_value(""),
         "Enable password; omit value to generate one, or pass a value to set it. Default: no password") // password option
        ("enable-upload", po::value<std::string>()->default_value("on")->implicit_value("on"),
         "Enable upload feature (on/off, default: on)");

    po::positional_options_description positionalOptionsDescription;
    positionalOptionsDescription.add("path", -1);

    po::variables_map variablesMap;
    try
    {
        po::store(po::command_line_parser(argc, argv)
                      .options(optionsDescription)
                      .positional(positionalOptionsDescription)
                      .run(),
                  variablesMap);
        po::notify(variablesMap);
    }
    catch (const po::error &e)
    {
        std::cerr << "bad options: " << e.what() << std::endl;
        std::cerr << optionsDescription << std::endl;
        return EXIT_FAILURE;
    }
    catch (const std::exception &e)
    {
        std::cerr << "unexpected error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if (variablesMap.count("help"))
    {
        std::cout << optionsDescription << std::endl;
    }
    else if (variablesMap.count("version"))
    {
        std::cout << DISPLAY_NAME << " " << PROJECT_VERSION << std::endl;
    }
    else
    {
        std::string path;
        if (variablesMap.count("path"))
        {
            path = variablesMap["path"].as<std::string>();
            if (path.empty())
            {
                std::cerr << "Missing value for option '--path'" << std::endl;
                std::cerr << optionsDescription << std::endl;
                return EXIT_FAILURE;
            }
        }

        std::string uploadsPath;
        if (variablesMap.count("uploads"))
        {
            uploadsPath = variablesMap["uploads"].as<std::string>();
            if (uploadsPath.empty())
            {
                std::cerr << "Missing value for option '--uploads'" << std::endl;
                std::cerr << optionsDescription << std::endl;
                return EXIT_FAILURE;
            }
        }

        std::string host = "0.0.0.0";
        if (variablesMap.count("host"))
        {
            host = variablesMap["host"].as<std::string>();
            if (host.empty())
            {
                std::cerr << "Missing value for option '--host'" << std::endl;
                std::cerr << optionsDescription << std::endl;
                return EXIT_FAILURE;
            }
        }

        std::string portString = "13396";
        if (variablesMap.count("port"))
        {
            portString = variablesMap["port"].as<std::string>();
            if (portString.empty())
            {
                std::cerr << "Missing value for option '--port'" << std::endl;
                std::cerr << optionsDescription << std::endl;
                return EXIT_FAILURE;
            }
        }

        unsigned long portValue = 0;
        try
        {
            std::size_t parsed = 0;
            portValue = std::stoul(portString, &parsed, 10);
            if (parsed != portString.size())
            {
                throw std::invalid_argument("trailing characters");
            }
        }
        catch (const std::exception &)
        {
            std::cerr << "Invalid value for option '--port': " << portString << std::endl;
            std::cerr << optionsDescription << std::endl;
            return EXIT_FAILURE;
        }

        if (portValue > std::numeric_limits<unsigned short>::max())
        {
            std::cerr << "Invalid value for option '--port': " << portString << std::endl;
            std::cerr << optionsDescription << std::endl;
            return EXIT_FAILURE;
        }

        const auto port = static_cast<unsigned short>(portValue);

        bool passwordEnabled = variablesMap.count("password") != 0U;
        std::string password;
        if (passwordEnabled)
        {
            password = variablesMap["password"].as<std::string>();
            if (password.empty())
            {
                password = Util::String::generateRandomString(12U);
            }
        }

        std::string uploadsEnabledValue = "on";
        if (variablesMap.count("enable-upload"))
        {
            uploadsEnabledValue = variablesMap["enable-upload"].as<std::string>();
            if (uploadsEnabledValue.empty())
            {
                std::cerr << "Missing value for option '--enable-upload' (use 'on' or 'off')" << std::endl;
                std::cerr << optionsDescription << std::endl;
                return EXIT_FAILURE;
            }
        }

        const std::string uploadsEnabledLower = Util::String::toLowerCopy(uploadsEnabledValue);

        bool uploadsEnabled = false;
        if (uploadsEnabledLower == "on")
        {
            uploadsEnabled = true;
        }
        else if (uploadsEnabledLower == "off")
        {
            uploadsEnabled = false;
        }
        else
        {
            std::cerr << "Invalid value for '--enable-upload': " << uploadsEnabledValue << " (expected 'on' or 'off')" << std::endl;
            std::cerr << optionsDescription << std::endl;
            return EXIT_FAILURE;
        }

        Core core;
        installSignalHandlers(core);
        core.start(path, uploadsPath, host, port, uploadsEnabled, password, passwordEnabled);

        if (shutdownRequested.load())
        {
            std::cout << "Shutdown complete. See you next time!" << std::endl;
        }
    }

    return EXIT_SUCCESS;
}
