/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <string>

#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#include <netlink/netlink.h>
#include <netlink/route/addr.h>
#include <netlink/route/link.h>
#include <netlink/route/link/bridge.h>
#include <netlink/route/link/vlan.h>
#include <netlink/route/route.h>

#include "logger/logmodule.hpp"
#include "network/networkinterfacemanager.hpp"

namespace aos::common::network {

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Bridge::Bridge(const sm::networkmanager::LinkAttrs& attrs)
    : mAttrs(attrs)
{
}

const sm::networkmanager::LinkAttrs& Bridge::GetAttrs() const
{
    return mAttrs;
}

const char* Bridge::GetType() const
{
    return "bridge";
}

Vlan::Vlan(const sm::networkmanager::LinkAttrs& attrs, int vlanId)
    : mAttrs(attrs)
    , mVlanId(vlanId)
{
}

const sm::networkmanager::LinkAttrs& Vlan::GetAttrs() const
{
    return mAttrs;
}

const char* Vlan::GetType() const
{
    return "vlan";
}

int Vlan::GetVlanId() const
{
    return mVlanId;
}

Error NetworkInterfaceManager::DeleteLink(const String& ifname)
{
    LOG_DBG() << "Remove interface: ifname=" << ifname;

    auto sockDeleter = [](nl_sock* sock) { nl_socket_free(sock); };
    auto sock        = std::unique_ptr<nl_sock, decltype(sockDeleter)>(nl_socket_alloc(), sockDeleter);
    if (!sock) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate netlink socket: " + std::string(nl_geterror(errno))).c_str());
    }

    if (nl_connect(sock.get(), NETLINK_ROUTE) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to connect to netlink: " + std::string(nl_geterror(errno))).c_str());
    }

    auto linkDeleter = [](rtnl_link* link) { rtnl_link_put(link); };
    auto link        = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(rtnl_link_alloc(), linkDeleter);
    if (!link) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate link object: " + std::string(nl_geterror(errno))).c_str());
    }

    rtnl_link_set_name(link.get(), ifname.CStr());

    int err = rtnl_link_delete(sock.get(), link.get());
    if (err < 0) {
        return Error(ErrorEnum::eFailed, ("failed to delete link: " + std::string(nl_geterror(err))).c_str());
    }

    return ErrorEnum::eNone;
}

Error NetworkInterfaceManager::SetupLink(const String& ifname)
{
    LOG_DBG() << "Bring up interface: ifname=" << ifname;

    auto sockDeleter = [](nl_sock* sock) { nl_socket_free(sock); };
    auto sock        = std::unique_ptr<nl_sock, decltype(sockDeleter)>(nl_socket_alloc(), sockDeleter);
    if (!sock) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate netlink socket: " + std::string(nl_geterror(errno))).c_str());
    }

    if (nl_connect(sock.get(), NETLINK_ROUTE) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to connect to netlink: " + std::string(nl_geterror(errno))).c_str());
    }

    auto linkDeleter = [](rtnl_link* link) { rtnl_link_put(link); };
    auto link        = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(rtnl_link_alloc(), linkDeleter);
    if (!link) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate link object: " + std::string(nl_geterror(errno))).c_str());
    }

    rtnl_link_set_name(link.get(), ifname.CStr());
    rtnl_link_set_flags(link.get(), IFF_UP);

    if (auto err = rtnl_link_change(sock.get(), link.get(), link.get(), 0); err < 0) {
        return Error(ErrorEnum::eFailed, ("failed to set link up: " + std::string(nl_geterror(err))).c_str());
    }

    return ErrorEnum::eNone;
}

Error NetworkInterfaceManager::AddLink(const sm::networkmanager::LinkItf* link)
{
    const auto& attrs = link->GetAttrs();

    LOG_DBG() << "Add link: name=" << attrs.mName.c_str() << ", type=" << link->GetType();

    auto sockDeleter = [](nl_sock* sock) { nl_socket_free(sock); };
    auto sock        = std::unique_ptr<nl_sock, decltype(sockDeleter)>(nl_socket_alloc(), sockDeleter);
    if (!sock) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate netlink socket: " + std::string(nl_geterror(errno))).c_str());
    }

    if (nl_connect(sock.get(), NETLINK_ROUTE) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to connect to netlink: " + std::string(nl_geterror(errno))).c_str());
    }

    auto linkDeleter = [](rtnl_link* linkObj) { rtnl_link_put(linkObj); };
    auto linkObj     = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(rtnl_link_alloc(), linkDeleter);
    if (!linkObj) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate link object: " + std::string(nl_geterror(errno))).c_str());
    }

    rtnl_link_set_name(linkObj.get(), attrs.mName.c_str());

    if (attrs.mTxQLen >= 0) {
        rtnl_link_set_txqlen(linkObj.get(), attrs.mTxQLen);
    }

    rtnl_link_set_type(linkObj.get(), link->GetType());

    if (auto* vlan = dynamic_cast<const Vlan*>(link)) {
        rtnl_link_vlan_set_id(linkObj.get(), vlan->GetVlanId());
    }

    if (attrs.mParentIndex > 0) {
        auto parent = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(rtnl_link_alloc(), linkDeleter);

        rtnl_link_set_ifindex(parent.get(), attrs.mParentIndex);
        rtnl_link_set_link(linkObj.get(), attrs.mParentIndex);
    }

    if (auto err = rtnl_link_add(sock.get(), linkObj.get(), NLM_F_CREATE); err < 0) {
        return Error(ErrorEnum::eFailed, ("failed to add link: " + std::string(nl_geterror(err))).c_str());
    }

    return ErrorEnum::eNone;
}

Error NetworkInterfaceManager::GetAddrList(
    const String& ifname, int family, Array<sm::networkmanager::IPAddr>& addresses) const
{
    LOG_DBG() << "List addresses for interface: ifname=" << ifname;

    auto sockDeleter = [](nl_sock* sock) { nl_socket_free(sock); };
    auto sock        = std::unique_ptr<nl_sock, decltype(sockDeleter)>(nl_socket_alloc(), sockDeleter);
    if (!sock) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate netlink socket: " + std::string(nl_geterror(errno))).c_str());
    }

    if (nl_connect(sock.get(), NETLINK_ROUTE) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to connect to netlink: " + std::string(nl_geterror(errno))).c_str());
    }

    nl_cache* cacheRaw;
    if (rtnl_addr_alloc_cache(sock.get(), &cacheRaw) < 0) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate address cache: " + std::string(nl_geterror(errno))).c_str());
    }

    auto cacheDeleter = [](nl_cache* cache) { nl_cache_free(cache); };
    auto addrCache    = std::unique_ptr<nl_cache, decltype(cacheDeleter)>(cacheRaw, cacheDeleter);

    auto linkDeleter = [](rtnl_link* link) { rtnl_link_put(link); };
    auto link        = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(rtnl_link_alloc(), linkDeleter);
    if (!link) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate link object: " + std::string(nl_geterror(errno))).c_str());
    }

    rtnl_link_set_name(link.get(), ifname.CStr());

    for (struct nl_object* obj = nl_cache_get_first(addrCache.get()); obj != nullptr; obj = nl_cache_get_next(obj)) {
        struct rtnl_addr* addr = reinterpret_cast<struct rtnl_addr*>(obj);

        if (rtnl_addr_get_ifindex(addr) == rtnl_link_get_ifindex(link.get())
            && (family == AF_UNSPEC || rtnl_addr_get_family(addr) == family)) {

            sm::networkmanager::IPAddr ipAddr;
            ipAddr.mFamily = rtnl_addr_get_family(addr);

            struct nl_addr* local = rtnl_addr_get_local(addr);
            if (local) {
                char buf[INET6_ADDRSTRLEN];

                nl_addr2str(local, buf, sizeof(buf));
                ipAddr.mIP = buf;
            }

            const char* label = rtnl_addr_get_label(addr);
            if (label) {
                ipAddr.mLabel = label;
            }

            if (auto err = addresses.PushBack(ipAddr); !err.IsNone()) {
                return err;
            }
        }
    }

    return ErrorEnum::eNone;
}

Error NetworkInterfaceManager::AddAddr(const String& ifname, const sm::networkmanager::IPAddr& addr)
{
    LOG_DBG() << "Add address to interface: ifname=" << ifname.CStr() << ", IP=" << addr.mIP.c_str();

    auto sockDeleter = [](nl_sock* sock) { nl_socket_free(sock); };
    auto sock        = std::unique_ptr<nl_sock, decltype(sockDeleter)>(nl_socket_alloc(), sockDeleter);
    if (!sock) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate netlink socket: " + std::string(nl_geterror(errno))).c_str());
    }

    if (nl_connect(sock.get(), NETLINK_ROUTE) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to connect to netlink: " + std::string(nl_geterror(errno))).c_str());
    }

    nl_cache* cacheRaw;
    if (rtnl_link_alloc_cache(sock.get(), AF_UNSPEC, &cacheRaw) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to allocate link cache: " + std::string(nl_geterror(errno))).c_str());
    }

    auto cacheDeleter = [](nl_cache* cache) { nl_cache_free(cache); };
    auto linkCache    = std::unique_ptr<nl_cache, decltype(cacheDeleter)>(cacheRaw, cacheDeleter);

    auto addrDeleter = [](rtnl_addr* addr) { rtnl_addr_put(addr); };
    auto addrObj     = std::unique_ptr<rtnl_addr, decltype(addrDeleter)>(rtnl_addr_alloc(), addrDeleter);
    if (!addrObj) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate address object: " + std::string(nl_geterror(errno))).c_str());
    }

    auto linkDeleter = [](rtnl_link* link) { rtnl_link_put(link); };
    auto link        = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(
        rtnl_link_get_by_name(linkCache.get(), ifname.CStr()), linkDeleter);
    if (!link) {
        return Error(ErrorEnum::eFailed, ("failed to get interface: " + std::string(nl_geterror(errno))).c_str());
    }

    int ifindex = rtnl_link_get_ifindex(link.get());
    if (ifindex <= 0) {
        return Error(ErrorEnum::eFailed,
            ("failed to get interface index for " + std::string(ifname.CStr()) + ": " + std::string(nl_geterror(errno)))
                .c_str());
    }

    rtnl_addr_set_ifindex(addrObj.get(), ifindex);

    struct nl_addr* local;
    if (nl_addr_parse(addr.mIP.c_str(), addr.mFamily, &local) < 0) {
        return Error(ErrorEnum::eFailed,
            ("failed to parse IP address: " + addr.mIP + " - " + std::string(nl_geterror(errno))).c_str());
    }

    auto localDeleter = [](nl_addr* addr) { nl_addr_put(addr); };
    auto localAddr    = std::unique_ptr<nl_addr, decltype(localDeleter)>(local, localDeleter);

    rtnl_addr_set_local(addrObj.get(), localAddr.get());

    if (!addr.mSubnet.empty()) {
        struct nl_addr* subnet;

        if (nl_addr_parse(addr.mSubnet.c_str(), addr.mFamily, &subnet) < 0) {
            return Error(ErrorEnum::eFailed,
                ("failed to parse subnet CIDR: " + addr.mSubnet + " - " + std::string(nl_geterror(errno))).c_str());
        }

        auto subnetAddr = std::unique_ptr<nl_addr, decltype(localDeleter)>(subnet, localDeleter);
        int  prefixlen  = nl_addr_get_prefixlen(subnetAddr.get());

        rtnl_addr_set_prefixlen(addrObj.get(), prefixlen);

        struct in_addr ipAddr, netmask, broadcast;
        inet_pton(AF_INET, addr.mIP.c_str(), &ipAddr);

        netmask.s_addr = htonl(~((1UL << (32 - prefixlen)) - 1));

        // Calculate broadcast: broadcast = ip | ~netmask
        broadcast.s_addr = (ipAddr.s_addr & netmask.s_addr) | ~netmask.s_addr;

        char brdStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &broadcast, brdStr, INET_ADDRSTRLEN);

        struct nl_addr* brd;
        if (nl_addr_parse(brdStr, addr.mFamily, &brd) >= 0) {
            auto brdAddr = std::unique_ptr<nl_addr, decltype(localDeleter)>(brd, localDeleter);
            rtnl_addr_set_broadcast(addrObj.get(), brdAddr.get());
        }
    }

    if (!addr.mLabel.empty()) {
        rtnl_addr_set_label(addrObj.get(), addr.mLabel.c_str());
    }

    if (auto err = rtnl_addr_add(sock.get(), addrObj.get(), 0); err < 0 && err != -NLE_EXIST) {
        return Error(ErrorEnum::eFailed, ("failed to add address: " + std::string(nl_geterror(err))).c_str());
    }

    return ErrorEnum::eNone;
}

Error NetworkInterfaceManager::DeleteAddr(const String& ifname, const sm::networkmanager::IPAddr& addr)
{
    LOG_DBG() << "Delete address from interface: ifname=" << ifname.CStr() << ", IP=" << addr.mIP.c_str();

    // Create socket with RAII
    auto sockDeleter = [](nl_sock* sock) { nl_socket_free(sock); };
    auto sock        = std::unique_ptr<nl_sock, decltype(sockDeleter)>(nl_socket_alloc(), sockDeleter);
    if (!sock) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate netlink socket: " + std::string(nl_geterror(errno))).c_str());
    }

    if (nl_connect(sock.get(), NETLINK_ROUTE) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to connect to netlink: " + std::string(nl_geterror(errno))).c_str());
    }

    auto addrDeleter = [](rtnl_addr* addr) { rtnl_addr_put(addr); };
    auto addrObj     = std::unique_ptr<rtnl_addr, decltype(addrDeleter)>(rtnl_addr_alloc(), addrDeleter);

    auto linkDeleter = [](rtnl_link* link) { rtnl_link_put(link); };
    auto link        = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(rtnl_link_alloc(), linkDeleter);

    rtnl_link_set_name(link.get(), ifname.CStr());
    rtnl_addr_set_ifindex(addrObj.get(), rtnl_link_get_ifindex(link.get()));

    struct nl_addr* local;
    if (nl_addr_parse(addr.mIP.c_str(), addr.mFamily, &local) < 0) {
        return Error(ErrorEnum::eFailed,
            ("failed to parse IP address: " + addr.mIP + " - " + std::string(nl_geterror(errno))).c_str());
    }

    auto localDeleter = [](nl_addr* addr) { nl_addr_put(addr); };
    auto localAddr    = std::unique_ptr<nl_addr, decltype(localDeleter)>(local, localDeleter);

    rtnl_addr_set_local(addrObj.get(), localAddr.get());

    if (auto err = rtnl_addr_delete(sock.get(), addrObj.get(), 0); err < 0) {
        return Error(ErrorEnum::eFailed, ("failed to delete address: " + std::string(nl_geterror(err))).c_str());
    }

    return ErrorEnum::eNone;
}

Error NetworkInterfaceManager::SetMasterLink(const String& ifname, const String& master)
{
    LOG_DBG() << "Set master for interface: ifname=" << ifname << ", master=" << master;

    auto sockDeleter = [](nl_sock* sock) { nl_socket_free(sock); };
    auto sock        = std::unique_ptr<nl_sock, decltype(sockDeleter)>(nl_socket_alloc(), sockDeleter);
    if (!sock) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate netlink socket: " + std::string(nl_geterror(errno))).c_str());
    }

    if (nl_connect(sock.get(), NETLINK_ROUTE) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to connect to netlink: " + std::string(nl_geterror(errno))).c_str());
    }

    nl_cache* cacheRaw;
    if (rtnl_link_alloc_cache(sock.get(), AF_UNSPEC, &cacheRaw) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to allocate link cache: " + std::string(nl_geterror(errno))).c_str());
    }

    auto cacheDeleter = [](nl_cache* cache) { nl_cache_free(cache); };
    auto linkCache    = std::unique_ptr<nl_cache, decltype(cacheDeleter)>(cacheRaw, cacheDeleter);

    auto linkDeleter = [](rtnl_link* link) { rtnl_link_put(link); };
    auto masterLink  = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(
        rtnl_link_get_by_name(linkCache.get(), master.CStr()), linkDeleter);
    if (!masterLink) {
        return Error(ErrorEnum::eFailed,
            ("master interface not found: " + std::string(master.CStr()) + " - " + std::string(nl_geterror(errno)))
                .c_str());
    }

    auto slaveLink = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(
        rtnl_link_get_by_name(linkCache.get(), ifname.CStr()), linkDeleter);
    if (!slaveLink) {
        return Error(ErrorEnum::eFailed,
            ("slave interface not found: " + std::string(ifname.CStr()) + " - " + std::string(nl_geterror(errno)))
                .c_str());
    }

    auto change = std::unique_ptr<rtnl_link, decltype(linkDeleter)>(rtnl_link_alloc(), linkDeleter);
    if (!change) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate link change object: " + std::string(nl_geterror(errno))).c_str());
    }

    int masterIndex = rtnl_link_get_ifindex(masterLink.get());
    rtnl_link_set_master(change.get(), masterIndex);

    if (auto err = rtnl_link_change(sock.get(), slaveLink.get(), change.get(), 0); err < 0) {
        return Error(ErrorEnum::eFailed,
            ("failed to set master for " + std::string(ifname.CStr()) + " to bridge " + std::string(master.CStr())
                + ": " + std::string(nl_geterror(err)))
                .c_str());
    }

    return ErrorEnum::eNone;
}

Error NetworkInterfaceManager::GetRouteList(const String& ifname, Array<sm::networkmanager::RouteInfo>& routes) const
{
    LOG_DBG() << "List routes for interface: ifname=" << ifname;

    auto sockDeleter = [](nl_sock* sock) { nl_socket_free(sock); };
    auto sock        = std::unique_ptr<nl_sock, decltype(sockDeleter)>(nl_socket_alloc(), sockDeleter);
    if (!sock) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate netlink socket: " + std::string(nl_geterror(errno))).c_str());
    }

    if (nl_connect(sock.get(), NETLINK_ROUTE) < 0) {
        return Error(ErrorEnum::eFailed, ("failed to connect to netlink: " + std::string(nl_geterror(errno))).c_str());
    }

    nl_cache* cacheRaw;
    if (rtnl_route_alloc_cache(sock.get(), AF_INET, 0, &cacheRaw) < 0) {
        return Error(
            ErrorEnum::eFailed, ("failed to allocate route cache: " + std::string(nl_geterror(errno))).c_str());
    }

    auto cacheDeleter = [](nl_cache* cache) { nl_cache_free(cache); };
    auto routeCache   = std::unique_ptr<nl_cache, decltype(cacheDeleter)>(cacheRaw, cacheDeleter);

    for (struct nl_object* obj = nl_cache_get_first(routeCache.get()); obj != nullptr; obj = nl_cache_get_next(obj)) {
        struct rtnl_route* route = reinterpret_cast<struct rtnl_route*>(obj);

        if (struct rtnl_nexthop* nh = rtnl_route_nexthop_n(route, 0); nh) {
            sm::networkmanager::RouteInfo info;

            info.mLinkIndex = rtnl_route_nh_get_ifindex(nh);

            if (rtnl_route_get_table(route) == RT_TABLE_MAIN) {
                struct nl_addr* dst = rtnl_route_get_dst(route);

                if (dst && nl_addr_get_prefixlen(dst) > 0) {
                    char buf[INET6_ADDRSTRLEN];

                    nl_addr2str(dst, buf, sizeof(buf));
                    info.mDestination = buf;
                }
            }

            if (auto err = routes.PushBack(info); !err.IsNone()) {
                return err;
            }
        }
    }

    return ErrorEnum::eNone;
}

} // namespace aos::common::network
