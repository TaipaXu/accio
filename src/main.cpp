#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <optional>
#include <atomic>
#include <stdexcept>
#include <cstdlib>
#include <csignal>
#include <cstdio>
#include <filesystem>
#include <boost/program_options.hpp>
#include "./config.hpp"
#include "./core.hpp"
#include "utils/string.hpp"
#include "utils/file.hpp"

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

    namespace po = boost::program_options;

    std::optional<std::string> getOption(const po::variables_map &vm,
                                         const po::options_description &desc,
                                         const char *name,
                                         const char *fallback = nullptr)
    {
        if (!vm.count(name))
        {
            return fallback ? std::string{fallback} : std::string{};
        }
        std::string value = vm[name].as<std::string>();
        if (value.empty())
        {
            std::cerr << "Missing value for option '--" << name << "'" << std::endl;
            std::cerr << desc << std::endl;
            return std::nullopt;
        }
        return value;
    }

    std::optional<bool> parseOnOff(const po::variables_map &vm,
                                   const po::options_description &desc,
                                   const char *name)
    {
        std::optional<std::string> val = getOption(vm, desc, name);
        if (!val)
        {
            return std::nullopt;
        }
        std::string lower = Util::String::toLowerCopy(*val);
        if (lower == "on")
        {
            return true;
        }
        if (lower == "off")
        {
            return false;
        }
        std::cerr << "Invalid value for '--" << name << "': " << *val
                  << " (expected 'on' or 'off')" << std::endl;
        std::cerr << desc << std::endl;
        return std::nullopt;
    }

    std::vector<std::string> getMultiOption(const po::variables_map &vm, const char *name)
    {
        return vm.count(name) ? vm[name].as<std::vector<std::string>>() : std::vector<std::string>{};
    }
} // namespace

int main(int argc, char *argv[])
{
    namespace po = boost::program_options;

    po::options_description optionsDescription("Allowed options");
    optionsDescription.add_options()("help,h", "Show help message")                        // help option
        ("version,v", "Show version information")                                          // version option
        ("path,p", po::value<std::string>()->implicit_value(""), "Current directory path") // path option
        ("uploads,u", po::value<std::string>()->implicit_value(""),
         "Uploads directory path (default: Downloads/accio)")                                    // uploads option
        ("host", po::value<std::string>()->implicit_value(""), "Server host (default: 0.0.0.0)") // host option
        ("port", po::value<std::string>()->implicit_value(""), "Server port (default: 13396)")   // port option
        ("password", po::value<std::string>()->implicit_value(""),
         "Enable password; omit value to generate one, or pass a value to set it. Default: no password")                                                             // password option
        ("enable-textboard", po::value<std::string>()->default_value("on")->implicit_value("on"), "Enable textboard feature (on/off, default: on)")                  // enable-textboard option
        ("enable-upload", po::value<std::string>()->default_value("on")->implicit_value("on"), "Enable upload feature (on/off, default: on)")                        // enable-upload option
        ("allow-exts", po::value<std::vector<std::string>>()->multitoken(), "Allowed file extensions (e.g., --allow-exts .txt .pdf)")                                // allow-exts option
        ("allow-files", po::value<std::vector<std::string>>()->multitoken(), "Allowed specific files (relative paths, e.g., --allow-files secret.txt sub/notes.md)") // allow-files option
        ("deny-exts", po::value<std::vector<std::string>>()->multitoken(), "Denied file extensions (e.g., --deny-exts .exe .dll)")                                   // deny-exts option
        ("deny-files", po::value<std::vector<std::string>>()->multitoken(), "Denied specific files (relative paths, e.g., --deny-files secret.txt tmp/a.bin)")       // deny-files option
        ;

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
        std::optional<std::string> path = getOption(variablesMap, optionsDescription, "path");
        if (!path)
        {
            return EXIT_FAILURE;
        }

        std::optional<std::string> uploadsPath = getOption(variablesMap, optionsDescription, "uploads");
        if (!uploadsPath)
        {
            return EXIT_FAILURE;
        }

        std::optional<std::string> host = getOption(variablesMap, optionsDescription, "host", "0.0.0.0");
        if (!host)
        {
            return EXIT_FAILURE;
        }

        std::optional<std::string> portString = getOption(variablesMap, optionsDescription, "port", "13396");
        if (!portString)
        {
            return EXIT_FAILURE;
        }

        unsigned long portValue = 0;
        try
        {
            std::size_t parsed = 0;
            portValue = std::stoul(*portString, &parsed, 10);
            if (parsed != portString->size())
            {
                throw std::invalid_argument("trailing characters");
            }
        }
        catch (const std::exception &)
        {
            std::cerr << "Invalid value for option '--port': " << *portString << std::endl;
            std::cerr << optionsDescription << std::endl;
            return EXIT_FAILURE;
        }

        if (portValue > std::numeric_limits<unsigned short>::max())
        {
            std::cerr << "Invalid value for option '--port': " << *portString << std::endl;
            std::cerr << optionsDescription << std::endl;
            return EXIT_FAILURE;
        }

        const unsigned short port = static_cast<unsigned short>(portValue);

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

        std::optional<bool> textboardEnabled = parseOnOff(variablesMap, optionsDescription, "enable-textboard");
        if (!textboardEnabled)
        {
            return EXIT_FAILURE;
        }

        std::optional<bool> uploadsEnabled = parseOnOff(variablesMap, optionsDescription, "enable-upload");
        if (!uploadsEnabled)
        {
            return EXIT_FAILURE;
        }

        std::vector<std::string> allowedExtensions = getMultiOption(variablesMap, "allow-exts");
        std::vector<std::string> allowedFiles = getMultiOption(variablesMap, "allow-files");
        std::vector<std::string> deniedExtensions = getMultiOption(variablesMap, "deny-exts");
        std::vector<std::string> deniedFiles = getMultiOption(variablesMap, "deny-files");

        if (Util::File::hasAbsolutePaths(allowedFiles))
        {
            std::cerr << "--allow-files only accepts relative paths" << std::endl;
            return EXIT_FAILURE;
        }

        if (Util::File::hasAbsolutePaths(deniedFiles))
        {
            std::cerr << "--deny-files only accepts relative paths" << std::endl;
            return EXIT_FAILURE;
        }

        if (!allowedExtensions.empty() && !deniedExtensions.empty())
        {
            std::cerr << "--allow-exts and --deny-exts cannot be used together" << std::endl;
            return EXIT_FAILURE;
        }

        Core core;
        installSignalHandlers(core);
        core.start(*path, *uploadsPath, *host, port, *textboardEnabled, *uploadsEnabled, password, passwordEnabled,
                   allowedExtensions, deniedExtensions, allowedFiles, deniedFiles);

        if (shutdownRequested.load())
        {
            std::cout << "Shutdown complete. See you next time!" << std::endl;
        }
    }

    return EXIT_SUCCESS;
}
