#pragma once

#include "cg50_usb2.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cg50 {

// ============================================================================
// Method 1: Virtual USB 2.0 Host Stack (BOT/SCSI + Casio Protocol 7.00 + ScreenRecv)
// Operates directly over SH7305 Usb2Unit (0x04D80000) without any Casio DLLs
// or host kernel drivers.
// ============================================================================

struct UsbDeviceIdentity {
    bool enumerated = false;
    std::uint16_t vendorId = 0;   // 0x07CF = CASIO
    std::uint16_t productId = 0;  // 0x6102=MassStorage, 0x6101=Protocol7, 0x6103=ScreenRecv
    std::uint8_t deviceClass = 0;
    std::uint8_t address = 0;
    std::uint8_t configuration = 0;
    std::string modeName;
};

struct ScsiCapacity {
    bool valid = false;
    std::uint32_t blockCount = 0;
    std::uint32_t blockSize = 512;
};

// Casio Protocol 7.00 packet framing (USB Bulk & SCIF0 3-pin compatible)
struct Protocol7Packet {
    std::uint8_t type = 0;       // 0x05=Check, 0x06=ACK, 0x15=NAK, 0x01=Command, 0x02=Data, 0x18=Terminate
    std::uint8_t subtype = 0;    // Two ASCII hex/char subtype bytes
    std::uint8_t subtype2 = 0;
    bool extended = false;
    std::vector<std::uint8_t> payload;
};

class Protocol7Codec final {
public:
    static constexpr std::uint8_t kTypeCommand   = 0x01u;
    static constexpr std::uint8_t kTypeData      = 0x02u;
    static constexpr std::uint8_t kTypeCheck     = 0x05u;
    static constexpr std::uint8_t kTypeAck       = 0x06u;
    static constexpr std::uint8_t kTypeNak       = 0x15u;
    static constexpr std::uint8_t kTypeTerminate = 0x18u;

    // Casio Protocol 7.00 checksum: (0x100 - (sum(bytes) & 0xFF)) & 0xFF encoded as 2 uppercase ASCII hex chars
    static std::uint8_t ComputeChecksum(const std::uint8_t* data, std::size_t len) noexcept;
    static std::vector<std::uint8_t> Encode(const Protocol7Packet& pkt);
    static bool Decode(const std::uint8_t* data, std::size_t len, Protocol7Packet& out);

    // High-level Protocol 7.00 packet builders
    static Protocol7Packet MakeCheck() noexcept;
    static Protocol7Packet MakeAck(std::uint8_t sub1 = '0', std::uint8_t sub2 = '0') noexcept;
    static Protocol7Packet MakeNak(std::uint8_t sub1 = '0', std::uint8_t sub2 = '0') noexcept;
    static Protocol7Packet MakeTerminate() noexcept;
    static Protocol7Packet MakeCommand(std::uint8_t sub1, std::uint8_t sub2, const std::vector<std::uint8_t>& payload);
    static Protocol7Packet MakeData(std::uint8_t sub1, std::uint8_t sub2, const std::vector<std::uint8_t>& payload);
};

class Cg50UsbVirtualHost final {
public:
    using StepCallback = std::function<void(std::uint64_t instructions)>;

    explicit Cg50UsbVirtualHost(Usb2Unit& usb, StepCallback stepFn = nullptr);

    // Standard USB 2.0 enumeration over DCP (Pipe 0)
    bool Enumerate(UsbDeviceIdentity& outId, std::size_t bulkOutPipe = 1u, std::size_t bulkInPipe = 2u);

    // Build / parse Bulk-Only Transport (BOT) CBW (31B) and CSW (13B)
    static std::vector<std::uint8_t> BuildCbw(std::uint32_t tag,
                                              std::uint32_t transferLen,
                                              bool directionIn,
                                              std::uint8_t lun,
                                              const std::uint8_t* cdb,
                                              std::uint8_t cdbLen);
    static bool ParseCsw(const std::vector<std::uint8_t>& bytes,
                         std::uint32_t expectedTag,
                         std::uint32_t& outResidue,
                         std::uint8_t& outStatus) noexcept;

    // USB Mass Storage BOT/SCSI transactions over Bulk pipes
    bool ScsiTestUnitReady();
    bool ScsiInquiry(std::string& outVendor, std::string& outProduct);
    bool ScsiReadCapacity(ScsiCapacity& outCap);
    bool ScsiRead10(std::uint32_t lba, std::uint16_t sectorCount, std::vector<std::uint8_t>& outData);
    bool ScsiWrite10(std::uint32_t lba, const std::uint8_t* data, std::uint16_t sectorCount);

    // Protocol 7.00 transaction over USB Bulk pipes
    bool Protocol7Exchange(const Protocol7Packet& request, Protocol7Packet& response);

    const UsbDeviceIdentity& Identity() const noexcept { return identity_; }
    void SetBulkPipes(std::size_t outPipe, std::size_t inPipe) noexcept
    {
        bulkOutPipe_ = outPipe;
        bulkInPipe_ = inPipe;
    }

private:
    bool ControlInTransfer(std::uint8_t bmRequestType,
                           std::uint8_t bRequest,
                           std::uint16_t wValue,
                           std::uint16_t wIndex,
                           std::uint16_t wLength,
                           std::vector<std::uint8_t>& outData);
    bool ControlNoDataTransfer(std::uint8_t bmRequestType,
                               std::uint8_t bRequest,
                               std::uint16_t wValue,
                               std::uint16_t wIndex);
    bool PumpUntil(const std::function<bool()>& pred, int maxSteps = 200);

    Usb2Unit& usb_;
    StepCallback stepFn_;
    UsbDeviceIdentity identity_{};
    std::size_t bulkOutPipe_ = 1u;
    std::size_t bulkInPipe_ = 2u;
    std::uint32_t nextTag_ = 1u;
};

} // namespace cg50
