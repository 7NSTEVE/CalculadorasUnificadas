#include "cg50_usb_host.h"

#include <algorithm>
#include <cstring>

namespace cg50 {
namespace {

inline char HexDigit(const std::uint8_t nibble) noexcept
{
    const std::uint8_t v = nibble & 0x0Fu;
    return static_cast<char>(v < 10u ? ('0' + v) : ('A' + (v - 10u)));
}

inline int ParseHexDigit(const std::uint8_t c) noexcept
{
    if (c >= '0' && c <= '9') return static_cast<int>(c - '0');
    if (c >= 'A' && c <= 'F') return static_cast<int>(c - 'A' + 10);
    if (c >= 'a' && c <= 'f') return static_cast<int>(c - 'a' + 10);
    return -1;
}

inline std::uint16_t ReadLe16(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

inline std::uint32_t ReadLe32(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

inline std::uint32_t ReadBe32(const std::uint8_t* p) noexcept
{
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

inline void WriteLe32(std::uint8_t* p, const std::uint32_t v) noexcept
{
    p[0] = static_cast<std::uint8_t>(v & 0xFFu);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
    p[2] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
    p[3] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
}

} // namespace

// ============================================================================
// Casio Protocol 7.00 Codec
// Format:
//   [type:1B] [sub1:1B] [sub2:1B] [ext:'0'/'1'] ([len:4B ASCII hex] [payload...]) [chk:2B ASCII hex]
// Checksum covers everything after [type] up to (not including) [chk].
// ============================================================================

std::uint8_t Protocol7Codec::ComputeChecksum(const std::uint8_t* data, const std::size_t len) noexcept
{
    std::uint32_t sum = 0u;
    for (std::size_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return static_cast<std::uint8_t>((0x100u - (sum & 0xFFu)) & 0xFFu);
}

std::vector<std::uint8_t> Protocol7Codec::Encode(const Protocol7Packet& pkt)
{
    std::vector<std::uint8_t> out;
    out.reserve(10u + pkt.payload.size());
    out.push_back(pkt.type);
    out.push_back(pkt.subtype);
    out.push_back(pkt.subtype2);
    const bool hasPayload = pkt.extended || !pkt.payload.empty();
    out.push_back(static_cast<std::uint8_t>(hasPayload ? '1' : '0'));
    if (hasPayload) {
        const std::uint16_t len = static_cast<std::uint16_t>(pkt.payload.size() & 0xFFFFu);
        out.push_back(static_cast<std::uint8_t>(HexDigit(static_cast<std::uint8_t>((len >> 12) & 0xFu))));
        out.push_back(static_cast<std::uint8_t>(HexDigit(static_cast<std::uint8_t>((len >> 8) & 0xFu))));
        out.push_back(static_cast<std::uint8_t>(HexDigit(static_cast<std::uint8_t>((len >> 4) & 0xFu))));
        out.push_back(static_cast<std::uint8_t>(HexDigit(static_cast<std::uint8_t>(len & 0xFu))));
        out.insert(out.end(), pkt.payload.begin(), pkt.payload.end());
    }
    const std::uint8_t chk = ComputeChecksum(out.data() + 1u, out.size() - 1u);
    out.push_back(static_cast<std::uint8_t>(HexDigit(static_cast<std::uint8_t>(chk >> 4))));
    out.push_back(static_cast<std::uint8_t>(HexDigit(static_cast<std::uint8_t>(chk & 0xFu))));
    return out;
}

bool Protocol7Codec::Decode(const std::uint8_t* data, const std::size_t len, Protocol7Packet& out)
{
    if (!data || len < 6u) return false;
    const std::uint8_t type = data[0];
    if (type != kTypeCommand && type != kTypeData && type != kTypeCheck &&
        type != kTypeAck && type != kTypeNak && type != kTypeTerminate) {
        return false;
    }
    const std::uint8_t extChar = data[3];
    if (extChar != '0' && extChar != '1') return false;

    std::size_t payloadLen = 0u;
    std::size_t payloadOff = 4u;
    if (extChar == '1') {
        if (len < 10u) return false;
        int d0 = ParseHexDigit(data[4]);
        int d1 = ParseHexDigit(data[5]);
        int d2 = ParseHexDigit(data[6]);
        int d3 = ParseHexDigit(data[7]);
        if (d0 < 0 || d1 < 0 || d2 < 0 || d3 < 0) return false;
        payloadLen = static_cast<std::size_t>((d0 << 12) | (d1 << 8) | (d2 << 4) | d3);
        payloadOff = 8u;
    }
    const std::size_t expectedTotal = payloadOff + payloadLen + 2u;
    if (len < expectedTotal) return false;

    const int cHi = ParseHexDigit(data[payloadOff + payloadLen]);
    const int cLo = ParseHexDigit(data[payloadOff + payloadLen + 1u]);
    if (cHi < 0 || cLo < 0) return false;
    const std::uint8_t receivedChk = static_cast<std::uint8_t>((cHi << 4) | cLo);
    const std::uint8_t expectedChk = ComputeChecksum(data + 1u, (payloadOff + payloadLen) - 1u);
    if (receivedChk != expectedChk) return false;

    out.type = type;
    out.subtype = data[1];
    out.subtype2 = data[2];
    out.extended = (extChar == '1');
    out.payload.assign(data + payloadOff, data + payloadOff + payloadLen);
    return true;
}

Protocol7Packet Protocol7Codec::MakeCheck() noexcept
{
    return Protocol7Packet{kTypeCheck, '0', '0', false, {}};
}

Protocol7Packet Protocol7Codec::MakeAck(const std::uint8_t sub1, const std::uint8_t sub2) noexcept
{
    return Protocol7Packet{kTypeAck, sub1, sub2, false, {}};
}

Protocol7Packet Protocol7Codec::MakeNak(const std::uint8_t sub1, const std::uint8_t sub2) noexcept
{
    return Protocol7Packet{kTypeNak, sub1, sub2, false, {}};
}

Protocol7Packet Protocol7Codec::MakeTerminate() noexcept
{
    return Protocol7Packet{kTypeTerminate, '0', '0', false, {}};
}

Protocol7Packet Protocol7Codec::MakeCommand(const std::uint8_t sub1,
                                            const std::uint8_t sub2,
                                            const std::vector<std::uint8_t>& payload)
{
    return Protocol7Packet{kTypeCommand, sub1, sub2, !payload.empty(), payload};
}

Protocol7Packet Protocol7Codec::MakeData(const std::uint8_t sub1,
                                         const std::uint8_t sub2,
                                         const std::vector<std::uint8_t>& payload)
{
    return Protocol7Packet{kTypeData, sub1, sub2, !payload.empty(), payload};
}

// ============================================================================
// Cg50UsbVirtualHost
// ============================================================================

Cg50UsbVirtualHost::Cg50UsbVirtualHost(Usb2Unit& usb, StepCallback stepFn)
    : usb_(usb), stepFn_(std::move(stepFn))
{
}

bool Cg50UsbVirtualHost::PumpUntil(const std::function<bool()>& pred, const int maxSteps)
{
    if (pred()) return true;
    if (!stepFn_) return false;
    for (int i = 0; i < maxSteps; ++i) {
        stepFn_(25000u);
        if (pred()) return true;
    }
    return false;
}

bool Cg50UsbVirtualHost::ControlInTransfer(const std::uint8_t bmRequestType,
                                           const std::uint8_t bRequest,
                                           const std::uint16_t wValue,
                                           const std::uint16_t wIndex,
                                           const std::uint16_t wLength,
                                           std::vector<std::uint8_t>& outData)
{
    outData.clear();
    if (!usb_.VbusAttached()) return false;
    // SH7305 USBREQ stores (bRequest << 8) | bmRequestType
    const std::uint16_t usbReq = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bRequest) << 8) | bmRequestType);
    usb_.InjectSetupPacket(usbReq, wValue, wIndex, wLength);
    if (!PumpUntil([this]() { return usb_.PipeTxPending(0u) > 0u; })) {
        return false;
    }
    outData = usb_.HostPopInPipe(0u);
    return !outData.empty();
}

bool Cg50UsbVirtualHost::ControlNoDataTransfer(const std::uint8_t bmRequestType,
                                               const std::uint8_t bRequest,
                                               const std::uint16_t wValue,
                                               const std::uint16_t wIndex)
{
    if (!usb_.VbusAttached()) return false;
    const std::uint16_t usbReq = static_cast<std::uint16_t>((static_cast<std::uint16_t>(bRequest) << 8) | bmRequestType);
    usb_.InjectSetupPacket(usbReq, wValue, wIndex, 0u);
    if (stepFn_) stepFn_(25000u);
    return true;
}

bool Cg50UsbVirtualHost::Enumerate(UsbDeviceIdentity& outId,
                                   const std::size_t bulkOutPipe,
                                   const std::size_t bulkInPipe)
{
    outId = UsbDeviceIdentity{};
    bulkOutPipe_ = bulkOutPipe;
    bulkInPipe_ = bulkInPipe;
    if (!usb_.VbusAttached()) return false;

    std::vector<std::uint8_t> devDesc;
    // Standard GET_DESCRIPTOR (Device = 0x0100, len = 18)
    if (!ControlInTransfer(0x80u, 0x06u, 0x0100u, 0x0000u, 18u, devDesc) || devDesc.size() < 18u) {
        return false;
    }
    if (devDesc[0] < 18u || devDesc[1] != 0x01u) return false;

    outId.deviceClass = devDesc[4];
    outId.vendorId = ReadLe16(devDesc.data() + 8u);
    outId.productId = ReadLe16(devDesc.data() + 10u);

    // SET_ADDRESS (1)
    if (!ControlNoDataTransfer(0x00u, 0x05u, 0x0001u, 0x0000u)) return false;
    outId.address = 1u;

    // SET_CONFIGURATION (1)
    if (!ControlNoDataTransfer(0x00u, 0x09u, 0x0001u, 0x0000u)) return false;
    outId.configuration = 1u;
    outId.enumerated = true;

    if (outId.vendorId == 0x07CFu) {
        if (outId.productId == 0x6102u) outId.modeName = "CASIO MassStorage (USB Flash)";
        else if (outId.productId == 0x6101u) outId.modeName = "CASIO Protocol 7.00 (DataTrans)";
        else if (outId.productId == 0x6103u) outId.modeName = "CASIO ScreenRecv / Projector";
        else outId.modeName = "CASIO USB Device";
    } else {
        outId.modeName = "Generic USB Device";
    }
    identity_ = outId;
    return true;
}

std::vector<std::uint8_t> Cg50UsbVirtualHost::BuildCbw(const std::uint32_t tag,
                                                       const std::uint32_t transferLen,
                                                       const bool directionIn,
                                                       const std::uint8_t lun,
                                                       const std::uint8_t* cdb,
                                                       const std::uint8_t cdbLen)
{
    std::vector<std::uint8_t> cbw(31u, 0u);
    // Signature 'USBC' (0x43425355 LE)
    cbw[0] = 'U'; cbw[1] = 'S'; cbw[2] = 'B'; cbw[3] = 'C';
    WriteLe32(cbw.data() + 4u, tag);
    WriteLe32(cbw.data() + 8u, transferLen);
    cbw[12] = directionIn ? 0x80u : 0x00u;
    cbw[13] = lun & 0x0Fu;
    const std::uint8_t clen = std::min<std::uint8_t>(cdbLen, 16u);
    cbw[14] = clen;
    if (cdb && clen > 0u) {
        std::memcpy(cbw.data() + 15u, cdb, clen);
    }
    return cbw;
}

bool Cg50UsbVirtualHost::ParseCsw(const std::vector<std::uint8_t>& bytes,
                                  const std::uint32_t expectedTag,
                                  std::uint32_t& outResidue,
                                  std::uint8_t& outStatus) noexcept
{
    if (bytes.size() < 13u) return false;
    if (bytes[0] != 'U' || bytes[1] != 'S' || bytes[2] != 'B' || bytes[3] != 'S') return false;
    const std::uint32_t tag = ReadLe32(bytes.data() + 4u);
    if (tag != expectedTag) return false;
    outResidue = ReadLe32(bytes.data() + 8u);
    outStatus = bytes[12];
    return true;
}

bool Cg50UsbVirtualHost::ScsiTestUnitReady()
{
    if (!usb_.VbusAttached()) return false;
    const std::uint32_t tag = nextTag_++;
    const std::uint8_t cdb[6] = {0x00u, 0, 0, 0, 0, 0};
    const auto cbw = BuildCbw(tag, 0u, false, 0u, cdb, 6u);
    usb_.HostPushOutPipe(bulkOutPipe_, cbw.data(), cbw.size());
    if (!PumpUntil([this]() { return usb_.PipeTxPending(bulkInPipe_) >= 13u; })) return false;
    const auto csw = usb_.HostPopInPipe(bulkInPipe_);
    std::uint32_t residue = 0u;
    std::uint8_t status = 0xFFu;
    return ParseCsw(csw, tag, residue, status) && status == 0u;
}

bool Cg50UsbVirtualHost::ScsiInquiry(std::string& outVendor, std::string& outProduct)
{
    if (!usb_.VbusAttached()) return false;
    const std::uint32_t tag = nextTag_++;
    const std::uint8_t cdb[6] = {0x12u, 0, 0, 0, 36u, 0};
    const auto cbw = BuildCbw(tag, 36u, true, 0u, cdb, 6u);
    usb_.HostPushOutPipe(bulkOutPipe_, cbw.data(), cbw.size());
    if (!PumpUntil([this]() { return usb_.PipeTxPending(bulkInPipe_) >= 36u + 13u; })) return false;
    const auto rx = usb_.HostPopInPipe(bulkInPipe_);
    if (rx.size() < 36u + 13u) return false;
    outVendor.assign(reinterpret_cast<const char*>(rx.data() + 8u), 8u);
    outProduct.assign(reinterpret_cast<const char*>(rx.data() + 16u), 16u);
    std::vector<std::uint8_t> csw(rx.begin() + 36, rx.begin() + 49);
    std::uint32_t residue = 0u;
    std::uint8_t status = 0xFFu;
    return ParseCsw(csw, tag, residue, status) && status == 0u;
}

bool Cg50UsbVirtualHost::ScsiReadCapacity(ScsiCapacity& outCap)
{
    outCap = ScsiCapacity{};
    if (!usb_.VbusAttached()) return false;
    const std::uint32_t tag = nextTag_++;
    const std::uint8_t cdb[10] = {0x25u, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const auto cbw = BuildCbw(tag, 8u, true, 0u, cdb, 10u);
    usb_.HostPushOutPipe(bulkOutPipe_, cbw.data(), cbw.size());
    if (!PumpUntil([this]() { return usb_.PipeTxPending(bulkInPipe_) >= 8u + 13u; })) return false;
    const auto rx = usb_.HostPopInPipe(bulkInPipe_);
    if (rx.size() < 21u) return false;
    const std::uint32_t lastLba = ReadBe32(rx.data());
    const std::uint32_t blkSz = ReadBe32(rx.data() + 4u);
    std::vector<std::uint8_t> csw(rx.begin() + 8, rx.begin() + 21);
    std::uint32_t residue = 0u;
    std::uint8_t status = 0xFFu;
    if (!ParseCsw(csw, tag, residue, status) || status != 0u) return false;
    outCap.valid = true;
    outCap.blockCount = lastLba + 1u;
    outCap.blockSize = blkSz;
    return true;
}

bool Cg50UsbVirtualHost::ScsiRead10(const std::uint32_t lba,
                                    const std::uint16_t sectorCount,
                                    std::vector<std::uint8_t>& outData)
{
    outData.clear();
    if (!usb_.VbusAttached() || sectorCount == 0u) return false;
    const std::uint32_t bytes = static_cast<std::uint32_t>(sectorCount) * 512u;
    const std::uint32_t tag = nextTag_++;
    const std::uint8_t cdb[10] = {
        0x28u, 0u,
        static_cast<std::uint8_t>((lba >> 24) & 0xFFu),
        static_cast<std::uint8_t>((lba >> 16) & 0xFFu),
        static_cast<std::uint8_t>((lba >> 8) & 0xFFu),
        static_cast<std::uint8_t>(lba & 0xFFu),
        0u,
        static_cast<std::uint8_t>((sectorCount >> 8) & 0xFFu),
        static_cast<std::uint8_t>(sectorCount & 0xFFu),
        0u
    };
    const auto cbw = BuildCbw(tag, bytes, true, 0u, cdb, 10u);
    usb_.HostPushOutPipe(bulkOutPipe_, cbw.data(), cbw.size());
    if (!PumpUntil([this, bytes]() { return usb_.PipeTxPending(bulkInPipe_) >= bytes + 13u; })) {
        return false;
    }
    const auto rx = usb_.HostPopInPipe(bulkInPipe_);
    if (rx.size() < bytes + 13u) return false;
    std::vector<std::uint8_t> csw(rx.begin() + bytes, rx.begin() + bytes + 13u);
    std::uint32_t residue = 0u;
    std::uint8_t status = 0xFFu;
    if (!ParseCsw(csw, tag, residue, status) || status != 0u) return false;
    outData.assign(rx.begin(), rx.begin() + bytes);
    return true;
}

bool Cg50UsbVirtualHost::ScsiWrite10(const std::uint32_t lba,
                                     const std::uint8_t* data,
                                     const std::uint16_t sectorCount)
{
    if (!usb_.VbusAttached() || !data || sectorCount == 0u) return false;
    const std::uint32_t bytes = static_cast<std::uint32_t>(sectorCount) * 512u;
    const std::uint32_t tag = nextTag_++;
    const std::uint8_t cdb[10] = {
        0x2Au, 0u,
        static_cast<std::uint8_t>((lba >> 24) & 0xFFu),
        static_cast<std::uint8_t>((lba >> 16) & 0xFFu),
        static_cast<std::uint8_t>((lba >> 8) & 0xFFu),
        static_cast<std::uint8_t>(lba & 0xFFu),
        0u,
        static_cast<std::uint8_t>((sectorCount >> 8) & 0xFFu),
        static_cast<std::uint8_t>(sectorCount & 0xFFu),
        0u
    };
    const auto cbw = BuildCbw(tag, bytes, false, 0u, cdb, 10u);
    usb_.HostPushOutPipe(bulkOutPipe_, cbw.data(), cbw.size());
    usb_.HostPushOutPipe(bulkOutPipe_, data, bytes);
    if (!PumpUntil([this]() { return usb_.PipeTxPending(bulkInPipe_) >= 13u; })) {
        return false;
    }
    const auto csw = usb_.HostPopInPipe(bulkInPipe_);
    std::uint32_t residue = 0u;
    std::uint8_t status = 0xFFu;
    return ParseCsw(csw, tag, residue, status) && status == 0u;
}

bool Cg50UsbVirtualHost::Protocol7Exchange(const Protocol7Packet& request, Protocol7Packet& response)
{
    if (!usb_.VbusAttached()) return false;
    const auto rawReq = Protocol7Codec::Encode(request);
    usb_.HostPushOutPipe(bulkOutPipe_, rawReq.data(), rawReq.size());
    if (!PumpUntil([this]() { return usb_.PipeTxPending(bulkInPipe_) >= 6u; })) {
        return false;
    }
    const auto rawResp = usb_.HostPopInPipe(bulkInPipe_);
    return Protocol7Codec::Decode(rawResp.data(), rawResp.size(), response);
}

} // namespace cg50
