/****************************************************************************
** vcprotocol.cpp
** TCP/IP Protocol for VC nano 3D Z Laser Scanner
** Based on Vision Components official SDK
**
** NOTE: This file contains stub implementations only.
** The proprietary protocol implementation is not part of the public source.
** © NOTAVIS GmbH, Pirmasens. All rights reserved.
****************************************************************************/
#include "vcprotocol.h"
#include <cstring>

VcProtocol::VcProtocol()
    : m_sock(VC_INVALID_SOCKET)
    , m_connected(false)
    , m_globalCounter(0)
    , m_metaCounter(0)
{
}

VcProtocol::~VcProtocol()
{
    disconnect();
}

bool VcProtocol::connect(const std::string & /*ip*/, uint16_t /*port*/)
{
    // Proprietary implementation not included in public source.
    return false;
}

void VcProtocol::disconnect()
{
    m_connected = false;
    m_sock = VC_INVALID_SOCKET;
}

bool VcProtocol::sendCommand(int /*paramId*/, const std::string & /*payload*/, uint32_t /*hostCmdId*/)
{
    // Proprietary implementation not included in public source.
    return false;
}

bool VcProtocol::readStringResponse(int /*paramId*/, uint32_t /*hostCmdId*/,
                                     std::string & /*out_response*/,
                                     int /*timeoutMs*/)
{
    // Proprietary implementation not included in public source.
    return false;
}

bool VcProtocol::readDataFrame(int & /*out_dataMode*/, int & /*out_resultCnt*/,
                                uint64_t & /*out_timestamp*/,
                                std::vector<uint8_t> & /*out_payload*/,
                                int /*timeoutMs*/)
{
    // Proprietary implementation not included in public source.
    return false;
}

bool VcProtocol::recvAll(void * /*buf*/, int /*size*/, int /*timeoutMs*/)
{
    return false;
}

bool VcProtocol::findSync(int /*timeoutMs*/)
{
    return false;
}

bool VcProtocol::recvGlobalHeader(VcGlobalHeader & /*gh*/, int /*timeoutMs*/)
{
    return false;
}
