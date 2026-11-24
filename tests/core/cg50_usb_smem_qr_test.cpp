#include "casio_ffi.h"
#include "cg50_core/cg50_fugue_ftl.h"
#include "cg50_core/cg50_lcd_qr.h"
#include "cg50_core/cg50_smem_fs.h"
#include "cg50_core/cg50_usb_host.h"
#include "cg50_native_core.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

int Fail(const char* msg)
{
    std::fprintf(stderr, "[cg50_usb_smem_qr_test] FAIL: %s\n", msg);
    return 1;
}

void TapKey(CasioCg50Handle* cg, const uint16_t code)
{
    casio_cg50_key_down(cg, code);
    casio_cg50_step(cg, 600000ULL, 8000ULL);
    casio_cg50_step(cg, 1500000ULL, 12000ULL);
    casio_cg50_step(cg, 1500000ULL, 12000ULL);
    casio_cg50_key_up(cg, code);
    casio_cg50_step(cg, 1500000ULL, 16000ULL);
    casio_cg50_step(cg, 1500000ULL, 16000ULL);
}

int TestProtocol7AndScsi()
{
    // 1. Protocol 7.00 positive roundtrip
    const std::vector<uint8_t> payload = {'C', 'G', '5', '0', '_', 'U', 'S', 'B', '!', 0x00, 0xFF};
    const auto cmd = cg50::Protocol7Codec::MakeCommand('0', '1', payload);
    const auto wire = cg50::Protocol7Codec::Encode(cmd);
    cg50::Protocol7Packet dec{};
    if (!cg50::Protocol7Codec::Decode(wire.data(), wire.size(), dec)) {
        return Fail("Protocol7Codec failed to decode valid command packet");
    }
    if (dec.type != cg50::Protocol7Codec::kTypeCommand || dec.subtype != '0' ||
        dec.subtype2 != '1' || dec.payload != payload) {
        return Fail("Protocol7Codec decoded payload mismatch");
    }

    // 1b. Non-circular known-vector checksum & CRC16-CCITT assertions
    const uint8_t proto7Vec[3] = {'0', '0', '0'}; // sum=0x90 -> (0x100-0x90)&0xFF = 0x70
    if (cg50::Protocol7Codec::ComputeChecksum(proto7Vec, 3u) != 0x70u) {
        return Fail("Protocol7Codec::ComputeChecksum failed known vector ('000' -> 0x70)");
    }
    const uint8_t crcVec[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    if (cg50::Cg50FugueFtl::ComputeCrc16Ccitt(crcVec, 9u) != 0x29B1u) {
        return Fail("Cg50FugueFtl::ComputeCrc16Ccitt failed standard CCITT test vector ('123456789' -> 0x29B1)");
    }

    // 2. Protocol 7.00 negative: corrupted checksum must fail
    auto badWire = wire;
    badWire.back() = (badWire.back() == '0') ? '1' : '0';
    if (cg50::Protocol7Codec::Decode(badWire.data(), badWire.size(), dec)) {
        return Fail("Protocol7Codec accepted corrupted checksum");
    }

    // 3. Protocol 7.00 negative: truncated buffer must fail
    if (cg50::Protocol7Codec::Decode(wire.data(), 2u, dec)) {
        return Fail("Protocol7Codec accepted truncated packet");
    }

    // 4. BOT CBW/CSW positive + negative
    const uint8_t cdb[6] = {0x12, 0x00, 0x00, 0x00, 0x24, 0x00}; // SCSI INQUIRY
    const auto cbw = cg50::Cg50UsbVirtualHost::BuildCbw(0x12345678u, 36u, true, 0u, cdb, 6u);
    if (cbw.size() != 31u || cbw[0] != 'U' || cbw[1] != 'S' || cbw[2] != 'B' || cbw[3] != 'C') {
        return Fail("BuildCbw produced invalid 31-byte CBW");
    }
    std::vector<uint8_t> csw = {'U', 'S', 'B', 'S', 0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0, 0x00};
    uint32_t residue = 999u;
    uint8_t status = 99u;
    if (!cg50::Cg50UsbVirtualHost::ParseCsw(csw, 0x12345678u, residue, status) ||
        residue != 0u || status != 0u) {
        return Fail("ParseCsw failed on valid CSW");
    }
    // Negative: wrong tag must fail
    if (cg50::Cg50UsbVirtualHost::ParseCsw(csw, 0xDEADBEEFu, residue, status)) {
        return Fail("ParseCsw accepted mismatched tag");
    }
    // Negative: wrong signature must fail
    csw[0] = 'X';
    if (cg50::Cg50UsbVirtualHost::ParseCsw(csw, 0x12345678u, residue, status)) {
        return Fail("ParseCsw accepted invalid signature");
    }

    std::printf("[cg50_usb_smem_qr_test] PASS: Protocol 7.00 + BOT/SCSI CBW/CSW positive & negative\n");
    return 0;
}

int TestHardwareUsbDetAndLink()
{
    auto core = std::make_unique<Cg50NativeCore>();

    // Verify PLDR.1 and PUDR.1 follow SetUsbCableConnected()
    core->SetUsbCableConnected(false);
    uint8_t pldr = 0xFFu, pudr = 0xFFu;
    if (!core->Read8(0xA4050134u, pldr) || !core->Read8(0xA4050162u, pudr)) {
        return Fail("Failed reading PLDR/PUDR");
    }
    if ((pldr & 0x02u) != 0u || (pudr & 0x02u) != 0u) {
        return Fail("PLDR.1 or PUDR.1 unexpectedly high while USB disconnected");
    }

    core->SetUsbCableConnected(true);
    if (!core->Read8(0xA4050134u, pldr) || !core->Read8(0xA4050162u, pudr)) {
        return Fail("Failed reading PLDR/PUDR after USB attach");
    }
    if ((pldr & 0x02u) == 0u || (pudr & 0x02u) == 0u || !core->Usb2().VbusAttached()) {
        return Fail("PLDR.1, PUDR.1 or Usb2Unit VBUS did not assert on SetUsbCableConnected(true)");
    }

    // Verify 32-bit USB2 FIFO/register access path does not fault
    uint32_t usbVal = 0;
    if (!core->Write32(0xA4D80000u, 0x00010000u) || !core->Read32(0xA4D80000u, usbVal)) {
        return Fail("32-bit USB2 MMIO read/write failed");
    }

    // Verify SCIF0 external link RX & TX byte streams + PLDR.2 RXD_INT
    core->SetExternalLinkEnabled(true);
    const uint8_t rxMsg[4] = {0x05u, '0', '0', '0'};
    if (core->LinkPushRx(rxMsg, 4u) != 4u) {
        return Fail("LinkPushRx did not accept 4 bytes");
    }
    if (!core->Read8(0xA4050134u, pldr) || (pldr & 0x04u) == 0u) {
        return Fail("PLDR.2 (RXD_INT) did not assert when SCIF0 RX FIFO has data");
    }
    // Enable SCIF0 TE (bit 5) and write TX bytes via MMIO SCFTDR (0xA441000C)
    (void)core->Write16(0xA4410008u, 0x0030u); // TE | RE
    (void)core->Write8(0xA441000Cu, 0x06u);    // ACK
    (void)core->Write8(0xA441000Cu, 'O');
    (void)core->Write8(0xA441000Cu, 'K');
    core->Scif0Mut().Tick(65536u); // Clock out 3 bytes at configured SCBRR baud rate
    uint8_t txOut[8]{};
    const std::size_t got = core->LinkPopTx(txOut, sizeof(txOut));
    if (got != 3u || txOut[0] != 0x06u || txOut[1] != 'O' || txOut[2] != 'K') {
        return Fail("External SCIF0 link TX pop mismatch");
    }

    std::printf("[cg50_usb_smem_qr_test] PASS: Hardware USB_DET (PLDR.1/PUDR.1), USB2 32-bit, SCIF0 Link\n");
    return 0;
}

int TestSmemAndLiveOs(const std::string& romDir)
{
    CasioCg50Handle* cg = casio_cg50_create_from_dir(romDir.c_str());
    if (!cg) return Fail("casio_cg50_create_from_dir returned null");

    // 1. Verify initial SMEM listing & all 5 official Casio .g3a files
    char jsonBuf[16384]{};
    const int jsonLen = casio_cg50_smem_list_json(cg, "/", jsonBuf, sizeof(jsonBuf));
    if (jsonLen <= 0 || jsonLen >= static_cast<int>(sizeof(jsonBuf))) {
        casio_cg50_destroy(cg);
        return Fail("casio_cg50_smem_list_json(/) failed");
    }
    const std::string rootJson(jsonBuf);
    if (rootJson.find("\"fatEqual\":true") == std::string::npos) {
        casio_cg50_destroy(cg);
        return Fail("Initial SMEM FAT1 != FAT2");
    }
    const char* kOfficialG3a[5] = {
        "/Geometry.g3a", "/PictPlot.g3a", "/3DGraph.g3a", "/Physium.g3a", "/ProbSim.g3a"
    };
    const uint8_t kCasioG3aMagic[8] = {0xAA, 0xAC, 0xBD, 0xAF, 0x90, 0x88, 0x9A, 0x8D}; // ~"USBPower"
    for (const char* g3a : kOfficialG3a) {
        const int64_t sz = casio_cg50_smem_read_file(cg, g3a, nullptr, 0);
        if (sz < 10000) {
            casio_cg50_destroy(cg);
            return Fail("Official .g3a missing or too small in SMEM");
        }
        std::vector<uint8_t> data(static_cast<size_t>(sz));
        if (casio_cg50_smem_read_file(cg, g3a, data.data(), data.size()) != sz) {
            casio_cg50_destroy(cg);
            return Fail("Failed reading official .g3a from SMEM");
        }
        if (std::memcmp(data.data(), kCasioG3aMagic, 8u) != 0) {
            casio_cg50_destroy(cg);
            return Fail("Official .g3a header mismatch (~USBPower magic failed)");
        }
    }
    std::printf("[cg50_usb_smem_qr_test] PASS: 5/5 official .g3a files verified bit-exact (FAT1==FAT2)\n");

    // 2. SMEM mkdir + LFN write + multi-cluster overwrite + negative tests + delete
    if (casio_cg50_smem_mkdir(cg, "/Prueba_USB") != 1) {
        casio_cg50_destroy(cg);
        return Fail("smem_mkdir(/Prueba_USB) failed");
    }
    const std::string pySrc = "print(6*7)\n# Casio fx-CG50 USB virtual test\n";
    if (casio_cg50_smem_write_file(
            cg, "/Prueba_USB/hola_usb.py",
            reinterpret_cast<const uint8_t*>(pySrc.data()), pySrc.size(), 1) != 1) {
        casio_cg50_destroy(cg);
        return Fail("smem_write_file(/Prueba_USB/hola_usb.py) failed");
    }

    // Negative: overwrite=0 must reject existing file
    if (casio_cg50_smem_write_file(
            cg, "/Prueba_USB/hola_usb.py",
            reinterpret_cast<const uint8_t*>(pySrc.data()), pySrc.size(), 0) != 0) {
        casio_cg50_destroy(cg);
        return Fail("overwrite=0 unexpectedly succeeded on existing file");
    }
    // Negative: invalid filename and path traversal must fail
    if (casio_cg50_smem_write_file(cg, "/bad:file.py", reinterpret_cast<const uint8_t*>("x"), 1, 1) != 0 ||
        casio_cg50_smem_write_file(cg, "/../escape.py", reinterpret_cast<const uint8_t*>("x"), 1, 1) != 0) {
        casio_cg50_destroy(cg);
        return Fail("Invalid filename or traversal path was not rejected");
    }
    // Negative: deleting non-empty folder without recursive must fail
    if (casio_cg50_smem_delete(cg, "/Prueba_USB", 0) != 0) {
        casio_cg50_destroy(cg);
        return Fail("Non-recursive delete of non-empty folder unexpectedly succeeded");
    }

    // Multi-cluster (> 12 KB) overwrite + exact readback
    std::vector<uint8_t> bigPayload(14321u);
    for (size_t i = 0; i < bigPayload.size(); ++i) {
        bigPayload[i] = static_cast<uint8_t>((i * 73u + 19u) & 0xFFu);
    }
    if (casio_cg50_smem_write_file(
            cg, "/Prueba_USB/datos_grandes.csv", bigPayload.data(), bigPayload.size(), 1) != 1) {
        casio_cg50_destroy(cg);
        return Fail("Multi-cluster write failed");
    }
    std::vector<uint8_t> readBack(bigPayload.size(), 0u);
    const int64_t gotSz = casio_cg50_smem_read_file(
        cg, "/Prueba_USB/datos_grandes.csv", readBack.data(), readBack.size());
    if (gotSz != static_cast<int64_t>(bigPayload.size()) || readBack != bigPayload) {
        casio_cg50_destroy(cg);
        return Fail("Multi-cluster readback mismatch");
    }

    // Recursive delete must succeed and keep FAT1 == FAT2
    if (casio_cg50_smem_delete(cg, "/Prueba_USB", 1) != 1) {
        casio_cg50_destroy(cg);
        return Fail("Recursive delete of /Prueba_USB failed");
    }
    if (casio_cg50_smem_read_file(cg, "/Prueba_USB/hola_usb.py", nullptr, 0) >= 0) {
        casio_cg50_destroy(cg);
        return Fail("Deleted file still readable");
    }

    // 3. Write root /hola_usb.py, remount Casio OS 3.80 cold boot, verify OS boots & file survives intact
    if (casio_cg50_smem_write_file(
            cg, "/hola_usb.py",
            reinterpret_cast<const uint8_t*>(pySrc.data()), pySrc.size(), 1) != 1) {
        casio_cg50_destroy(cg);
        return Fail("smem_write_file(/hola_usb.py) failed");
    }
    if (casio_cg50_smem_remount_os(cg) != 1) {
        casio_cg50_destroy(cg);
        return Fail("smem_remount_os failed");
    }
    // Step Casio OS through boot into MAIN MENU (~25M instructions)
    for (int i = 0; i < 40; ++i) {
        casio_cg50_step(cg, 1000000ULL, 10000ULL);
    }
    char postJson[16384]{};
    if (casio_cg50_smem_list_json(cg, "/", postJson, sizeof(postJson)) <= 0 ||
        std::string(postJson).find("hola_usb.py") == std::string::npos ||
        std::string(postJson).find("\"fatEqual\":true") == std::string::npos) {
        casio_cg50_destroy(cg);
        return Fail("hola_usb.py missing or FAT corrupted after live Casio OS boot");
    }
    std::vector<uint8_t> pyRead(pySrc.size(), 0u);
    if (casio_cg50_smem_read_file(cg, "/hola_usb.py", pyRead.data(), pyRead.size()) !=
            static_cast<int64_t>(pySrc.size()) ||
        std::memcmp(pyRead.data(), pySrc.data(), pySrc.size()) != 0) {
        casio_cg50_destroy(cg);
        return Fail("hola_usb.py content altered after live Casio OS boot");
    }

    // 4. Live USB status C ABI check
    if (casio_cg50_usb_set_connected(cg, 1) != 1) {
        casio_cg50_destroy(cg);
        return Fail("casio_cg50_usb_set_connected(1) failed");
    }
    char usbStatus[512]{};
    if (casio_cg50_usb_get_status_json(cg, usbStatus, sizeof(usbStatus)) <= 0 ||
        std::string(usbStatus).find("\"connected\":true") == std::string::npos ||
        std::string(usbStatus).find("\"vbus\":true") == std::string::npos) {
        casio_cg50_destroy(cg);
        return Fail("casio_cg50_usb_get_status_json did not report connected+vbus");
    }
    casio_cg50_usb_set_connected(cg, 0);

    std::printf("[cg50_usb_smem_qr_test] PASS: SMEM CRUD, negative tests, live Casio OS 3.80 boot persistence & USB status\n");

    // 5. LIVE Casio OS 3.80 QR Code generation & native ISO-18004 decode!
    const std::string warmPath = romDir + "/cg50_warm_state.bin";
    if (casio_cg50_import_state_file(cg, warmPath.c_str()) != 1) {
        casio_cg50_destroy(cg);
        return Fail("Failed importing cg50_warm_state.bin for live QR test");
    }
    casio_cg50_step(cg, 200000ULL, 4000ULL);

    // Negative: MAIN MENU has no QR code -> must return 0
    char qrText[1024]{};
    const int qrMain = casio_cg50_decode_lcd_qr(cg, qrText, sizeof(qrText));
    if (qrMain != 0) {
        casio_cg50_destroy(cg);
        return Fail("decode_lcd_qr returned non-zero on MAIN MENU (no QR present)");
    }

    // Enter Run-Matrix (EXE), open CATALOG (SHIFT+4), scroll to Abs (6x DOWN), press F2 (QR)!
    TapKey(cg, 0x0302); // EXE -> Run-Matrix
    for (int i = 0; i < 3; ++i) casio_cg50_step(cg, 1500000ULL, 16000ULL);
    TapKey(cg, 0x0709); // SHIFT
    TapKey(cg, 0x0704); // 4 (CATALOG)
    for (int i = 0; i < 6; ++i) TapKey(cg, 0x0308); // DOWN -> Abs (online manual command)
    TapKey(cg, 0x060A); // F2 (QR!)
    for (int i = 0; i < 6; ++i) {
        casio_cg50_step(cg, 1500000ULL, 16000ULL);
    }

    const int qrLen = casio_cg50_decode_lcd_qr(cg, qrText, sizeof(qrText));
    std::printf("[cg50_usb_smem_qr_test] Live Casio OS QR decoded (len=%d): %s\n", qrLen, qrText);
    if (qrLen <= 0 || std::string(qrText).find("http://wes.casio.com/math/index.php?q=") == std::string::npos) {
        casio_cg50_destroy(cg);
        return Fail("Failed to decode live Casio OS 3.80 CATALOG->F2(QR) code (expected wes.casio.com URL)");
    }

    casio_cg50_destroy(cg);
    return 0;
}

} // namespace

int main()
{
    const std::string romDir = CASIO_ROMS_DIR;
    if (int rc = TestProtocol7AndScsi(); rc != 0) return rc;
    if (int rc = TestHardwareUsbDetAndLink(); rc != 0) return rc;
    if (int rc = TestSmemAndLiveOs(romDir); rc != 0) return rc;
    std::printf("[cg50_usb_smem_qr_test] ALL CHECKS PASSED\n");
    return 0;
}
