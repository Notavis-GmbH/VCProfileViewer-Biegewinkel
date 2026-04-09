/****************************************************************************
** vcprotocol.cpp
****************************************************************************/
#include "vcprotocol.h"
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <cstdio>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
static bool wsaStarted = false;
#else
#  define closesocket close
#endif

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------
static uint32_t calcChecksum(const uint8_t *data, uint32_t len)
{
    uint32_t cs = 0;
    for (uint32_t i = 0; i < len; ++i)
        cs += data[i];
    return cs;
}

// -----------------------------------------------------------------------
// VcProtocol
// -----------------------------------------------------------------------
VcProtocol::VcProtocol()
    : m_sock(VC_INVALID_SOCKET)
    , m_connected(false)
    , m_globalCounter(0)
    , m_metaCounter(0)
{
#ifdef _WIN32
    if (!wsaStarted) {
        WSADATA wd;
        WSAStartup(MAKEWORD(2,2), &wd);
        wsaStarted = true;
    }
#endif
}

VcProtocol::~VcProtocol()
{
    disconnect();
}

bool VcProtocol::connect(const std::string &ip, uint16_t port)
{
    disconnect();

    m_sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sock == VC_INVALID_SOCKET) return false;

    // Set socket receive timeout (500 ms)
#ifdef _WIN32
    DWORD tv = 500;
    setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv{0, 500000};
    setsockopt(m_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (::connect(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(m_sock);
        m_sock = VC_INVALID_SOCKET;
        return false;
    }

    m_connected = true;
    m_globalCounter = 0;
    m_metaCounter   = 0;
    return true;
}

void VcProtocol::disconnect()
{
    if (m_sock != VC_INVALID_SOCKET) {
        closesocket(m_sock);
        m_sock = VC_INVALID_SOCKET;
    }
    m_connected = false;
}

// -----------------------------------------------------------------------
// recvAll: receive exactly `size` bytes with timeout
// -----------------------------------------------------------------------
bool VcProtocol::recvAll(void *buf, int size, int timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(timeoutMs);
    int received = 0;
    uint8_t *p = static_cast<uint8_t*>(buf);

    while (received < size) {
        if (std::chrono::steady_clock::now() > deadline) return false;

        int r = ::recv(m_sock, reinterpret_cast<char*>(p + received),
                       size - received, 0);
        if (r <= 0) {
#ifdef _WIN32
            int e = WSAGetLastError();
            if (e == WSAETIMEDOUT || e == WSAEWOULDBLOCK) continue;
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
#endif
            return false;
        }
        received += r;
    }
    return true;
}

// -----------------------------------------------------------------------
// findSync: scan byte-by-byte for 0xAA0F55F0 (little-endian: F0 55 0F AA)
// -----------------------------------------------------------------------
bool VcProtocol::findSync(int timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(timeoutMs);
    uint8_t b;
    int p = 0;
    const uint8_t seq[4] = {0xF0, 0x55, 0x0F, 0xAA};

    while (std::chrono::steady_clock::now() < deadline) {
        int r = ::recv(m_sock, reinterpret_cast<char*>(&b), 1, 0);
        if (r == 1) {
            if (b == seq[p]) { ++p; if (p == 4) return true; }
            else              { p = (b == seq[0]) ? 1 : 0; }
        }
    }
    return false;
}

// -----------------------------------------------------------------------
// sendCommand
// -----------------------------------------------------------------------
bool VcProtocol::sendCommand(int paramId, const std::string &payload, uint32_t hostCmdId)
{
    if (!m_connected) return false;

    // Build meta header
    VcMetaHeader mh;
    mh.type     = 0;
    mh.id       = static_cast<uint32_t>(paramId);
    mh.hostCnt  = hostCmdId;
    mh.length   = static_cast<uint32_t>(payload.size() + (payload.empty() ? 0 : 1)); // include NUL

    uint32_t dataLen = sizeof(VcMetaHeader) + mh.length;

    // Build global header
    VcGlobalHeader gh;
    gh.syncByte     = VC_SYNC_BYTE;
    gh.counter      = m_globalCounter++;
    gh.timeMs       = 0;
    gh.reserved     = 0;
    gh.error        = 0;
    gh.dataLength   = dataLen;

    // Checksum over meta+payload
    std::vector<uint8_t> data(sizeof(VcMetaHeader) + mh.length, 0);
    memcpy(data.data(), &mh, sizeof(VcMetaHeader));
    if (!payload.empty())
        memcpy(data.data() + sizeof(VcMetaHeader), payload.c_str(), payload.size());

    gh.checksumData = calcChecksum(data.data(), static_cast<uint32_t>(data.size()));
    gh.checksumHead = calcChecksum(
        reinterpret_cast<uint8_t*>(&gh) + 8,  // skip syncByte + checksumHead
        sizeof(VcGlobalHeader) - 8);

    // Send
    std::vector<uint8_t> pkt(sizeof(VcGlobalHeader) + data.size());
    memcpy(pkt.data(), &gh, sizeof(VcGlobalHeader));
    memcpy(pkt.data() + sizeof(VcGlobalHeader), data.data(), data.size());

    int sent = ::send(m_sock, reinterpret_cast<const char*>(pkt.data()),
                      static_cast<int>(pkt.size()), 0);
    return sent == static_cast<int>(pkt.size());
}

// -----------------------------------------------------------------------
// readStringResponse
// -----------------------------------------------------------------------
bool VcProtocol::readStringResponse(int paramId, uint32_t hostCmdId,
                                    std::string &out_response, int timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        int remaining = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count());
        if (remaining <= 0) break;

        if (!findSync(remaining)) continue;

        // Read rest of global header (28 bytes after sync)
        uint8_t ghBuf[VC_GLOBAL_HDR_SIZE];
        memcpy(ghBuf, &VC_SYNC_BYTE, 4);
        if (!recvAll(ghBuf + 4, VC_GLOBAL_HDR_SIZE - 4, 500)) continue;
        VcGlobalHeader gh;
        memcpy(&gh, ghBuf, sizeof(gh));

        if (gh.dataLength == 0 || gh.dataLength > 256*1024) continue;

        std::vector<uint8_t> payload(gh.dataLength);
        if (!recvAll(payload.data(), gh.dataLength, 1000)) continue;

        // Parse meta header
        if (gh.dataLength < sizeof(VcMetaHeader)) continue;
        VcMetaHeader mh;
        memcpy(&mh, payload.data(), sizeof(VcMetaHeader));

        // Match: type==0 (string response), id==paramId, hostCnt==hostCmdId
        if (mh.type == CMDTYPE_STRING &&
            static_cast<int>(mh.id) == paramId &&
            mh.hostCnt == hostCmdId)
        {
            // Payload starts right after the 16-byte VcMetaHeader
            uint32_t strOffset = static_cast<uint32_t>(sizeof(VcMetaHeader));
            if (gh.dataLength > strOffset)
                out_response = std::string(
                    reinterpret_cast<char*>(payload.data() + strOffset),
                    gh.dataLength - strOffset);
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------
// readDataFrame
// -----------------------------------------------------------------------
bool VcProtocol::readDataFrame(int &out_dataMode, int &out_resultCnt,
                               uint64_t &out_timestamp,
                               std::vector<uint8_t> &out_payload,
                               int timeoutMs)
{
    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(timeoutMs);

    while (std::chrono::steady_clock::now() < deadline) {
        int remaining = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count());
        if (remaining <= 0) break;

        if (!findSync(remaining)) continue;

        uint8_t ghBuf[VC_GLOBAL_HDR_SIZE];
        memcpy(ghBuf, &VC_SYNC_BYTE, 4);
        if (!recvAll(ghBuf + 4, VC_GLOBAL_HDR_SIZE - 4, 500)) continue;
        VcGlobalHeader gh;
        memcpy(&gh, ghBuf, sizeof(gh));

        if (gh.dataLength == 0 || gh.dataLength > 16*1024*1024) continue;

        std::vector<uint8_t> payload(gh.dataLength);
        if (!recvAll(payload.data(), gh.dataLength, 2000)) continue;

        // After global header we have VcMetaHeader (16 bytes)
        if (gh.dataLength < static_cast<uint32_t>(sizeof(VcMetaHeader))) continue;

        VcMetaHeader mh2;
        memcpy(&mh2, payload.data(), sizeof(VcMetaHeader));

        // Only handle data frames (type == CMDTYPE_DATA == 100)
        if (mh2.type != static_cast<uint32_t>(CMDTYPE_DATA)) continue;

        // After VcMetaHeader: dataMode(4) + resultCnt(4) + timestamp(8) + actual payload
        const uint32_t dataHdrSize = static_cast<uint32_t>(sizeof(VcMetaHeader)) + 16u;
        if (gh.dataLength < dataHdrSize) continue;

        const uint8_t *dp = payload.data() + sizeof(VcMetaHeader);
        int32_t dm, rc;
        uint64_t ts;
        memcpy(&dm, dp + 0, 4);
        memcpy(&rc, dp + 4, 4);
        memcpy(&ts, dp + 8, 8);

        out_dataMode  = dm;
        out_resultCnt = rc;
        out_timestamp = ts;
        out_payload   = std::vector<uint8_t>(payload.begin() + dataHdrSize, payload.end());
        return true;
    }
    return false;
}
