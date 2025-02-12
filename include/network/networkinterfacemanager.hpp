/*
 * Copyright (C) 2025 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NETWORKINTERFACEMANAGER_HPP_
#define NETWORKINTERFACEMANAGER_HPP_

#include <optional>
#include <string>

#include <sys/socket.h>

#include <aos/sm/networkmanager.hpp>
namespace aos::sm::networkmanager {

/**
 * Link attributes.
 */
struct LinkAttrs {
    std::string mName;
    int         mParentIndex {};
    int         mTxQLen {};
};

/**
 * IP address.
 */
struct IPAddr {
    std::string mIP;
    std::string mSubnet;
    int         mFamily = AF_INET;
    std::string mLabel;
};

/**
 * Route info.
 */
struct RouteInfo {
    std::optional<std::string> mDestination;
    int                        mLinkIndex;
};

} // namespace aos::sm::networkmanager

namespace aos::common::network {

/**
 * Bridge link.
 */
class Bridge : public sm::networkmanager::LinkItf {
public:
    /**
     * @brief Construct a new Bridge object
     *
     */
    explicit Bridge(const sm::networkmanager::LinkAttrs& attrs);

    /**
     * @brief Get the type of the link
     *
     */
    const sm::networkmanager::LinkAttrs& GetAttrs() const override;

    /**
     * @brief Get the type of the link
     *
     */
    const char* GetType() const override;

private:
    sm::networkmanager::LinkAttrs mAttrs;
};

/**
 * Vlan link.
 */
class Vlan : public sm::networkmanager::LinkItf {
public:
    /**
     * @brief Construct a new Vlan object
     *
     */
    Vlan(const sm::networkmanager::LinkAttrs& attrs, int vlanId);

    /**
     * @brief Get the type of the link
     *
     */
    const sm::networkmanager::LinkAttrs& GetAttrs() const override;

    /**
     * @brief Get the type of the link
     *
     */
    const char* GetType() const override;

    /**
     * @brief Get the vlan id
     *
     */
    int GetVlanId() const;

private:
    sm::networkmanager::LinkAttrs mAttrs;
    int                           mVlanId {-1};
};

/**
 * Network interface manager.
 */
class NetworkInterfaceManager : public sm::networkmanager::NetworkInterfaceManagerItf,
                                public sm::networkmanager::NetworkInterfaceFactoryItf {
public:
    /**
     * Removes interface.
     *
     * @param ifname interface name.
     * @return Error.
     */
    Error DeleteLink(const String& ifname) override;

    /**
     * Adds link.
     *
     * @param link link.
     * @return Error.
     */
    Error AddLink(const sm::networkmanager::LinkItf* link) override;

    /**
     * Brings up interface.
     *
     * @param ifname interface name.
     * @return Error.
     */
    Error SetupLink(const String& ifname) override;

    /**
     * Gets address list.
     *
     * @param ifname interface name.
     * @param family address family.
     * @param[out] addr address list.
     * @return Error.
     */
    Error GetAddrList(const String& ifname, int family, Array<sm::networkmanager::IPAddr>& addr) const override;

    /**
     * Adds address.
     *
     * @param ifname interface name.
     * @param addr address.
     * @return Error.
     */
    Error AddAddr(const String& ifname, const sm::networkmanager::IPAddr& addr) override;

    /**
     * Deletes address.
     *
     * @param ifname interface name.
     * @param addr address.
     * @return Error.
     */
    Error DeleteAddr(const String& ifname, const sm::networkmanager::IPAddr& addr) override;

    /**
     * Sets master.
     *
     * @param ifname interface name.
     * @param master master.
     * @return Error.
     */
    Error SetMasterLink(const String& ifname, const String& master) override;

    /**
     * Gets route list.
     *
     * @param[out] routes routes.
     * @return Error.
     */
    Error GetRouteList(Array<sm::networkmanager::RouteInfo>& routes) const override;

    /**
     * Creates bridge.
     *
     * @param name bridge name.
     * @param ip ip.
     * @param subnet subnet.
     * @return Error.
     */
    Error CreateBridge(const String& name, const String& ip, const String& subnet) override;

    /**
     * Creates vlan.
     *
     * @param name vlan name.
     * @param vlanId vlan id.
     * @return Error.
     */
    Error CreateVlan(const String& name, uint64_t vlanId) override;

private:
    static constexpr size_t kMaxRouteCount = 20;

    RetWithError<int> GetMasterInterfaceIndex() const;
};

} // namespace aos::common::network

#endif // NETWORKINTERFACEMANAGER_HPP_
