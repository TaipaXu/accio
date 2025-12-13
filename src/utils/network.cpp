#include "./network.hpp"
#include <string>
#include <vector>
#include <utility>
#include <memory>
#include <algorithm>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace Util::Network
{
    std::vector<std::pair<std::string, int>> collectNetworkAddresses()
    {
        std::vector<std::pair<std::string, int>> addresses;
#ifdef _WIN32
        using GetAdaptersAddressesPtr = ULONG(WINAPI *)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);
        static const GetAdaptersAddressesPtr getAdaptersAddressesPtr = []() -> GetAdaptersAddressesPtr {
            if (HMODULE module = LoadLibraryA("iphlpapi.dll"))
            {
                return reinterpret_cast<GetAdaptersAddressesPtr>(GetProcAddress(module, "GetAdaptersAddresses"));
            }
            return nullptr;
        }();

        if (getAdaptersAddressesPtr == nullptr)
        {
            return addresses;
        }

        ULONG bufferLength = 0;
        const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
        ULONG result = getAdaptersAddressesPtr(AF_UNSPEC, flags, nullptr, nullptr, &bufferLength);
        if (result == ERROR_NO_DATA)
        {
            return addresses;
        }

        if (result != ERROR_BUFFER_OVERFLOW)
        {
            return addresses;
        }

        std::vector<unsigned char> buffer(bufferLength);
        auto *adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
        result = getAdaptersAddressesPtr(AF_UNSPEC, flags, nullptr, adapters, &bufferLength);
        if (result != NO_ERROR)
        {
            return addresses;
        }

        for (auto *adapter = adapters; adapter != nullptr; adapter = adapter->Next)
        {
            if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            {
                continue;
            }

            for (auto *unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
            {
                const auto *sockAddr = unicast->Address.lpSockaddr;
                if (sockAddr == nullptr)
                {
                    continue;
                }

                const int family = sockAddr->sa_family;
                if (family != AF_INET && family != AF_INET6)
                {
                    continue;
                }

                char host[NI_MAXHOST] = {};
                if (getnameinfo(sockAddr,
                                static_cast<socklen_t>(unicast->Address.iSockaddrLength),
                                host,
                                sizeof(host),
                                nullptr,
                                0,
                                NI_NUMERICHOST)
                    != 0)
                {
                    continue;
                }

                std::string addr{host};
                if (auto percent = addr.find('%'); percent != std::string::npos)
                {
                    addr.erase(percent);
                }

                if (addr == "127.0.0.1" || addr == "::1" || addr == "0.0.0.0" || addr == "::")
                {
                    continue;
                }

                if (family == AF_INET6 && addr.rfind("fe80", 0) == 0)
                {
                    continue;
                }

                addresses.emplace_back(std::move(addr), family);
            }
        }
#else
        ifaddrs *ifaddr = nullptr;
        if (getifaddrs(&ifaddr) != 0)
        {
            return addresses;
        }

        std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> guard{ifaddr, freeifaddrs};
        for (auto *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr)
            {
                continue;
            }

            const int family = ifa->ifa_addr->sa_family;
            if (family != AF_INET && family != AF_INET6)
            {
                continue;
            }

            if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_LOOPBACK) != 0)
            {
                continue;
            }

            char host[NI_MAXHOST] = {};
            const socklen_t addrLen = family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
            if (getnameinfo(ifa->ifa_addr, addrLen, host, sizeof(host), nullptr, 0, NI_NUMERICHOST) != 0)
            {
                continue;
            }

            std::string addr{host};
            if (auto percent = addr.find('%'); percent != std::string::npos)
            {
                addr.erase(percent);
            }

            if (addr == "0.0.0.0" || addr == "::" || addr == "127.0.0.1" || addr == "::1")
            {
                continue;
            }

            if (family == AF_INET6 && addr.rfind("fe80", 0) == 0)
            {
                continue;
            }

            addresses.emplace_back(std::move(addr), family);
        }
#endif

        std::sort(addresses.begin(), addresses.end(), [](const auto &lhs, const auto &rhs) {
            if (lhs.second != rhs.second)
            {
                return lhs.second < rhs.second;
            }
            return lhs.first < rhs.first;
        });
        addresses.erase(std::unique(addresses.begin(), addresses.end(), [](const auto &lhs, const auto &rhs) {
                            return lhs.second == rhs.second && lhs.first == rhs.first;
                        }),
                        addresses.end());
        return addresses;
    }
} // namespace Util::Network
