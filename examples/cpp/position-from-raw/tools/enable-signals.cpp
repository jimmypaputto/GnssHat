/*
 * Jimmy Paputto 2026
 *
 * enable-signals — restore CFG-SIGNAL keys on ZED-F9P via direct SPI.
 *
 * Purpose: after a CFG-SIGNAL reset (e.g. caused by a crashed session
 * during signal reconfiguration) the receiver can stop tracking
 * GPS L1C/A, Galileo E1, and/or BeiDou B1I — even though these are
 * required by the position-from-raw decoders.
 *
 * This helper opens /dev/spidev0.0 directly and sends a UBX-CFG-VALSET
 * message that enables:
 *
 *   CFG-SIGNAL-GPS_ENA       0x1031001f  (GPS constellation)
 *   CFG-SIGNAL-GPS_L1CA_ENA  0x10310001
 *   CFG-SIGNAL-GPS_L5_ENA    0x10310004  (required by GPS L5 CNAV)
 *   CFG-SIGNAL-GAL_ENA       0x10310021  (Galileo constellation)
 *   CFG-SIGNAL-GAL_E1_ENA    0x10310007
 *   CFG-SIGNAL-BDS_ENA       0x10310022  (BeiDou constellation)
 *   CFG-SIGNAL-BDS_B1_ENA    0x1031000d
 *   CFG-SIGNAL-GLO_ENA       0x10310025  (GLONASS constellation)
 *   CFG-SIGNAL-GLO_L1_ENA    0x10310018
 *
 * Layers: RAM | BBR | Flash (0x07) — persists across resets.
 *
 * IMPORTANT: this MUST be run with the position-from-raw and any other
 *            GnssHat-based process STOPPED (they hold /dev/spidev0.0).
 *
 * Changing CFG-SIGNAL triggers a GNSS subsystem reset; wait ~0.5 s
 * after the ACK before starting position-from-raw.
 */

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace
{

constexpr const char* kSpiDev    = "/dev/spidev0.0";
constexpr uint32_t    kSpiHz     = 5'000'000;
constexpr uint8_t     kSpiBits   = 8;
constexpr uint8_t     kSpiMode   = SPI_MODE_0;

// CFG-SIGNAL keys (U1 values, each 0x01 = enable)
constexpr std::array<uint32_t, 9> kKeys = {
    0x1031001Fu,  // GPS_ENA
    0x10310001u,  // GPS_L1CA_ENA
    0x10310004u,  // GPS_L5_ENA
    0x10310021u,  // GAL_ENA
    0x10310007u,  // GAL_E1_ENA
    0x10310022u,  // BDS_ENA
    0x1031000Du,  // BDS_B1_ENA
    0x10310025u,  // GLO_ENA
    0x10310018u,  // GLO_L1_ENA
};

// ── Fletcher-8 UBX checksum over class+id+length+payload ──────────
void ubxChecksum(const uint8_t* data, size_t n,
                 uint8_t& ckA, uint8_t& ckB)
{
    ckA = 0;
    ckB = 0;
    for (size_t i = 0; i < n; ++i)
    {
        ckA = static_cast<uint8_t>(ckA + data[i]);
        ckB = static_cast<uint8_t>(ckB + ckA);
    }
}

// ── Build a UBX-CFG-VALSET frame enabling all kKeys ───────────────
std::vector<uint8_t> buildCfgValsetFrame()
{
    // Payload: version(1) + layers(1) + reserved(2) + N*(key 4 + U1 1)
    constexpr size_t kPerKey = 4 + 1;
    const size_t payloadLen  = 4 + kKeys.size() * kPerKey;

    std::vector<uint8_t> frame;
    frame.reserve(6 + payloadLen + 2);

    frame.push_back(0xB5);
    frame.push_back(0x62);
    frame.push_back(0x06);                         // class: CFG
    frame.push_back(0x8A);                         // id:    VALSET
    frame.push_back(static_cast<uint8_t>(payloadLen & 0xFF));
    frame.push_back(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));

    frame.push_back(0x00);                         // version 0 (no txn)
    frame.push_back(0x07);                         // layers: RAM|BBR|Flash
    frame.push_back(0x00);                         // reserved0[0]
    frame.push_back(0x00);                         // reserved0[1]

    for (const uint32_t k : kKeys)
    {
        frame.push_back(static_cast<uint8_t>(k        & 0xFFu));
        frame.push_back(static_cast<uint8_t>((k >>  8) & 0xFFu));
        frame.push_back(static_cast<uint8_t>((k >> 16) & 0xFFu));
        frame.push_back(static_cast<uint8_t>((k >> 24) & 0xFFu));
        frame.push_back(0x01);                     // value U1 = enable
    }

    uint8_t ckA, ckB;
    ubxChecksum(frame.data() + 2, frame.size() - 2, ckA, ckB);
    frame.push_back(ckA);
    frame.push_back(ckB);
    return frame;
}

// ── SPI open/configure ────────────────────────────────────────────
int openSpi()
{
    const int fd = open(kSpiDev, O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "[enable-signals] open %s failed: ", kSpiDev);
        perror("");
        return -1;
    }

    uint8_t mode = kSpiMode;
    uint8_t bits = kSpiBits;
    uint32_t hz  = kSpiHz;

    if (ioctl(fd, SPI_IOC_WR_MODE,          &mode) < 0 ||
        ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ,  &hz)   < 0)
    {
        perror("[enable-signals] ioctl SPI config");
        close(fd);
        return -1;
    }
    return fd;
}

// ── Single full-duplex SPI transfer ───────────────────────────────
bool spiXfer(int fd, const uint8_t* tx, uint8_t* rx, size_t n)
{
    struct spi_ioc_transfer t{};
    t.tx_buf        = reinterpret_cast<unsigned long>(tx);
    t.rx_buf        = reinterpret_cast<unsigned long>(rx);
    t.len           = static_cast<uint32_t>(n);
    t.speed_hz      = kSpiHz;
    t.bits_per_word = kSpiBits;
    return ioctl(fd, SPI_IOC_MESSAGE(1), &t) >= 0;
}

// ── Scan an RX buffer for a UBX-ACK-ACK / ACK-NAK matching
//    (cls=0x06, id=0x8A). Returns +1 on ACK, -1 on NAK, 0 none. ──
int scanForAck(const std::vector<uint8_t>& buf)
{
    for (size_t i = 0; i + 9 < buf.size(); ++i)
    {
        if (buf[i]   != 0xB5) continue;
        if (buf[i+1] != 0x62) continue;
        if (buf[i+2] != 0x05) continue;               // ACK class
        const uint8_t id  = buf[i+3];
        if (id != 0x00 && id != 0x01) continue;       // NAK or ACK
        const uint16_t len = buf[i+4] | (buf[i+5] << 8);
        if (len != 2) continue;
        if (buf[i+6] != 0x06) continue;               // acked class: CFG
        if (buf[i+7] != 0x8A) continue;               // acked id:    VALSET
        return (id == 0x01) ? +1 : -1;
    }
    return 0;
}

}  // namespace

int main()
{
    const int fd = openSpi();
    if (fd < 0) return 1;

    const auto frame = buildCfgValsetFrame();

    // -- Transmit the VALSET frame --
    std::vector<uint8_t> rxDummy(frame.size(), 0xFF);
    if (!spiXfer(fd, frame.data(), rxDummy.data(), frame.size()))
    {
        perror("[enable-signals] SPI TX failed");
        close(fd);
        return 1;
    }

    printf("[enable-signals] sent CFG-VALSET (%zu keys, %zu bytes)\n",
           kKeys.size(), frame.size());

    // -- Poll RX for up to ~1 s looking for ACK --
    //
    // ZED-F9P SPI: idle bytes are 0xFF; messages arrive as the
    // receiver has data to send. We read in 256-byte bursts with a
    // short sleep between polls.
    using clock = std::chrono::steady_clock;
    const auto deadline = clock::now() + std::chrono::seconds(2);

    std::vector<uint8_t> all;
    all.reserve(16 * 1024);

    int ack = 0;
    while (clock::now() < deadline)
    {
        std::array<uint8_t, 256> tx;
        tx.fill(0xFF);
        std::array<uint8_t, 256> rx{};
        if (!spiXfer(fd, tx.data(), rx.data(), tx.size()))
        {
            perror("[enable-signals] SPI RX failed");
            close(fd);
            return 1;
        }
        all.insert(all.end(), rx.begin(), rx.end());

        ack = scanForAck(all);
        if (ack != 0) break;

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    close(fd);

    if (ack > 0)
    {
        printf("[enable-signals] CFG-VALSET ACK received. "
               "Wait ~0.5 s for GNSS subsystem reset before "
               "starting position-from-raw.\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        return 0;
    }
    if (ack < 0)
    {
        fprintf(stderr, "[enable-signals] receiver returned NAK — "
                        "one or more keys rejected.\n");
        return 2;
    }
    fprintf(stderr, "[enable-signals] no ACK within 2 s. Receiver may "
                    "not be ready or another process holds the SPI.\n");
    return 3;
}
