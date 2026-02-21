/*
 * Jimmy Paputto 2022
 */

#ifndef UBX_CFG_PRT_HPP_
#define UBX_CFG_PRT_HPP_

#include <cassert>
#include <stdexcept>
#include <string>
#include <variant>

#include "ublox/ESpiMode.hpp"
#include "ublox/EUbxPrt.hpp"
#include "ublox/ubxmsg/IUbxMsg.hpp"

#include "common/Utils.hpp"


namespace JimmyPaputto::ubxmsg
{

struct TxReady
{
    std::vector<uint8_t> serialize() const
    {
        if (!enable)
        {
            return { 0x00, 0x00 };
        }
        uint16_t output = 0b0000000000000001;
        output |= polarity << 1;
        output |= pin << 2;
        output |= threshold << 7;
        return serializeInt2LittleEndian(output);
    }

    void deserialize(std::span<const uint8_t> frame)
    {
        const uint16_t serialized = readLE<uint16_t>(frame, 0);
        enable = getBit(serialized, 0);
        polarity = static_cast<Polarity>(getBit(serialized, 1));
        pin = (serialized >> 2) & 0x3F;
        threshold = (serialized >> 7) &  0x1FF;
    }

    enum Polarity: uint8_t
    {
        HighActive = 0x00,
        LowActive  = 0x01
    };

    bool enable;
    Polarity polarity;
    uint8_t pin;
    uint8_t threshold;  // 0x000 - 0x1FF
};

template<EUbxPrt PortType>
struct Mode
{
    static_assert(PortType == EUbxPrt::UBX_SPI || PortType == EUbxPrt::UBX_UART_1 ||
        PortType == EUbxPrt::UBX_UART_2, "Only SPI and UART ports supported");
    
    std::vector<uint8_t> serialize() const;
    void deserialize(std::span<const uint8_t> frame);
};

template<>
struct Mode<EUbxPrt::UBX_SPI>
{
    std::vector<uint8_t> serialize() const
    {
        uint32_t output = 0x00000000;
        output |= static_cast<uint8_t>(spiMode) << 1;
        output |= ffCnt << 8;
        return serializeInt2LittleEndian(output);
    }

    void deserialize(std::span<const uint8_t> frame)
    {
        const uint32_t serialized = readLE<uint32_t>(frame, 0);
        spiMode = static_cast<ESpiMode>((serialized >> 1) & 0x3);
        ffCnt = (serialized >> 8) & 0x3F;
    }

    ESpiMode spiMode;
    uint8_t ffCnt;  // 0 - 63
};

template<>
struct Mode<EUbxPrt::UBX_UART_1>
{
    std::vector<uint8_t> serialize() const
    {
        uint32_t output = 0x00000000;
        output |= static_cast<uint8_t>(charLen) << 6;
        output |= static_cast<uint8_t>(parity) << 9;
        output |= static_cast<uint8_t>(nStopBits) << 12;
        return serializeInt2LittleEndian(output);
    }

    void deserialize(std::span<const uint8_t> frame)
    {
        const uint32_t serialized = readLE<uint32_t>(frame, 0);
        charLen = static_cast<CharLen>((serialized >> 6) & 0x3);
        parity = static_cast<Parity>((serialized >> 9) & 0x7);
        nStopBits = static_cast<StopBits>((serialized >> 12) & 0x3);
    }

    enum class CharLen: uint8_t
    {
        Bits5 = 0b00,  // not supported
        Bits6 = 0b01,  // not supported
        Bits7 = 0b10,  // supported only with parity
        Bits8 = 0b11   // Standard
    };
    
    enum class Parity: uint8_t
    {
        Even     = 0b000,
        Odd      = 0b001,
        NoParity = 0b100,
        Reserved = 0b101
    };

    enum class StopBits: uint8_t
    {
        One        = 0b00,
        OneAndHalf = 0b01,
        Two        = 0b10,
        Half       = 0b11
    };

    CharLen charLen;
    Parity parity;
    StopBits nStopBits;
};

template<>
struct Mode<EUbxPrt::UBX_UART_2> : public Mode<EUbxPrt::UBX_UART_1>
{
};

struct ProtoMask
{
    std::vector<uint8_t> serialize() const
    {
        uint16_t output = 0x0000;
        output |= static_cast<uint8_t>(ubx);
        output |= static_cast<uint8_t>(nmea) << 1;
        output |= static_cast<uint8_t>(rtcm) << 2;
        output |= static_cast<uint8_t>(rtcm3) << 5;
        return serializeInt2LittleEndian(output);
    }

    void deserialize(std::span<const uint8_t> frame)
    {
        const uint16_t serialized = readLE<uint16_t>(frame, 0);
        ubx = getBit(serialized, 0);
        nmea = getBit(serialized, 1);
        rtcm = getBit(serialized, 2);
        rtcm3 = getBit(serialized, 5);
    }

    bool ubx;
    bool nmea;
    bool rtcm;
    bool rtcm3;
};

struct Flags
{
    std::vector<uint8_t> serialize() const
    {
        uint16_t output = 0x0000;
        output |= static_cast<uint8_t>(extendedTxTimeout) << 1;
        return serializeInt2LittleEndian(output);
    }

    void deserialize(std::span<const uint8_t> frame)
    {
        const uint16_t serialized = readLE<uint16_t>(frame, 0);
        extendedTxTimeout = getBit(serialized, 1);
    }

    bool extendedTxTimeout;
};

template<typename PortImpl, EUbxPrt PortType>
class UBX_CFG_PRT_BASE : public IUbxMsg
{
public:
    explicit UBX_CFG_PRT_BASE() = default;

    explicit UBX_CFG_PRT_BASE(const TxReady& txReady, const Mode<PortType>& mode,
        const ProtoMask& inProtoMask, const ProtoMask& outProtoMask,
        const Flags& flags)
    :	txReady_(txReady),
        mode_(mode),
        inProtoMask_(inProtoMask),
        outProtoMask_(outProtoMask),
        flags_(flags)
    {}

    explicit UBX_CFG_PRT_BASE(std::span<const uint8_t> frame)
    {
        deserialize(frame);
    }

    std::vector<uint8_t> serialize() const override
    {
        return static_cast<const PortImpl*>(this)->serializeImpl();
    }

    void deserialize(std::span<const uint8_t> serialized) override
    {
        static_cast<PortImpl*>(this)->deserializeImpl(serialized);
    }

    constexpr EUbxPrt portId() const { return PortType; }

protected:
    std::vector<uint8_t> getBeginning() const
    {
        constexpr auto portId = static_cast<uint8_t>(PortType);
        return {
            0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, portId, 0x00
        };
    }

    TxReady txReady_;
    Mode<PortType> mode_;
    ProtoMask inProtoMask_;
    ProtoMask outProtoMask_;
    Flags flags_;
};

class UBX_CFG_PRT_SPI : public UBX_CFG_PRT_BASE<UBX_CFG_PRT_SPI, EUbxPrt::UBX_SPI>
{
public:
    explicit UBX_CFG_PRT_SPI() = default;

    explicit UBX_CFG_PRT_SPI(const TxReady& txReady, const Mode<EUbxPrt::UBX_SPI>& mode,
        const ProtoMask& inProtoMask, const ProtoMask& outProtoMask,
        const Flags& flags)
    :	UBX_CFG_PRT_BASE<UBX_CFG_PRT_SPI, EUbxPrt::UBX_SPI>(txReady, mode, inProtoMask,
            outProtoMask, flags)
    {}

    explicit UBX_CFG_PRT_SPI(std::span<const uint8_t> frame)
    :	UBX_CFG_PRT_BASE<UBX_CFG_PRT_SPI, EUbxPrt::UBX_SPI>(frame)
    {}

    std::vector<uint8_t> serializeImpl() const
    {
        const auto begining = getBeginning();

        const auto serialized =
            begining +
            txReady_.serialize() +
            mode_.serialize() +
            std::vector<uint8_t> { 0x00, 0x00, 0x00, 0x00 } +
            inProtoMask_.serialize() +
            outProtoMask_.serialize() +
            flags_.serialize() +
            std::vector<uint8_t> { 0x00, 0x00 };

        return buildFrame(serialized);
    }

    void deserializeImpl(std::span<const uint8_t> serialized)
    {
        txReady_.deserialize(serialized.subspan(8, 2));
        mode_.deserialize(serialized.subspan(10, 4));
        inProtoMask_.deserialize(serialized.subspan(18, 2));
        outProtoMask_.deserialize(serialized.subspan(20, 2));
        flags_.deserialize(serialized.subspan(22, 2));
    }
};

template<EUbxPrt UartID>
class UBX_CFG_PRT_UART :
    public UBX_CFG_PRT_BASE<UBX_CFG_PRT_UART<UartID>, UartID>
{
public:
    explicit UBX_CFG_PRT_UART() = default;

    explicit UBX_CFG_PRT_UART(const TxReady& txReady,
        const Mode<UartID>& mode, const uint32_t baudrate,
        const ProtoMask& inProtoMask, const ProtoMask& outProtoMask,
        const Flags& flags)
    :	UBX_CFG_PRT_BASE<UBX_CFG_PRT_UART<UartID>, UartID>(txReady, mode,
            inProtoMask, outProtoMask, flags),
        baudrate_(baudrate)
    {}

    explicit UBX_CFG_PRT_UART(std::span<const uint8_t> frame)
    :	UBX_CFG_PRT_BASE<UBX_CFG_PRT_UART<UartID>, UartID>(frame)
    {}

    std::vector<uint8_t> serializeImpl() const
    {
        const auto beginning = this->getBeginning();

        const auto serialized =
            beginning +
            this->txReady_.serialize() +
            this->mode_.serialize() +
            serializeInt2LittleEndian(baudrate_) +
            this->inProtoMask_.serialize() +
            this->outProtoMask_.serialize() +
            this->flags_.serialize() +
            std::vector<uint8_t> { 0x00, 0x00 };

        return this->buildFrame(serialized);
    }

    void deserializeImpl(std::span<const uint8_t> serialized)
    {
        this->txReady_.deserialize(serialized.subspan(8, 2));
        this->mode_.deserialize(serialized.subspan(10, 4));
        baudrate_ = readLE<uint32_t>(serialized, 14);
        this->inProtoMask_.deserialize(serialized.subspan(18, 2));
        this->outProtoMask_.deserialize(serialized.subspan(20, 2));
        this->flags_.deserialize(serialized.subspan(22, 2));
    }

private:
    uint32_t baudrate_;
};

class UBX_CFG_PRT_UART1 : public UBX_CFG_PRT_UART<EUbxPrt::UBX_UART_1>
{
public:
    explicit UBX_CFG_PRT_UART1(std::span<const uint8_t> frame)
    :	UBX_CFG_PRT_UART<EUbxPrt::UBX_UART_1>(frame)
    {}
};

class UBX_CFG_PRT_UART2 : public UBX_CFG_PRT_UART<EUbxPrt::UBX_UART_2>
{
public:
    explicit UBX_CFG_PRT_UART2(std::span<const uint8_t> frame)
    :	UBX_CFG_PRT_UART<EUbxPrt::UBX_UART_2>(frame)
    {}
};

class UBX_CFG_PRT: public IUbxMsg
{
public:
    UBX_CFG_PRT() = default;

    std::vector<uint8_t> serialize() const override
    {
        return std::visit(
            [](const auto& impl) { return impl.serialize(); },
            impl_
        );
    }

    void deserialize(std::span<const uint8_t> frame) override
    {
        if (frame.size() < 9)
        {
            throw std::invalid_argument("UBX_CFG_PRT frame too short");
        }

        uint8_t portId = frame[6];
        
        switch (portId)
        {
            case static_cast<uint8_t>(EUbxPrt::UBX_SPI):
            {
                UBX_CFG_PRT_SPI spiImpl(frame);
                impl_ = std::move(spiImpl);
                break;
            }
            case static_cast<uint8_t>(EUbxPrt::UBX_UART_1):
            {
                UBX_CFG_PRT_UART1 uart1Impl(frame);
                impl_ = std::move(uart1Impl);
                break;
            }
            case static_cast<uint8_t>(EUbxPrt::UBX_UART_2):
            {
                UBX_CFG_PRT_UART2 uart2Impl(frame);
                impl_ = std::move(uart2Impl);
                break;
            }
            default:
                throw std::invalid_argument(
                    "Unknown UBX_CFG_PRT port type: " + std::to_string(portId)
                );
        }
    }

    template<EUbxPrt PortType>
    static std::vector<uint8_t> poll()
    {
        constexpr auto portId = static_cast<uint8_t>(PortType);
        return IUbxMsg::buildFrame({
            0xB5, 0x62, 0x06, 0x00, 0x01, 0x00, portId
        });
    }

private:
    using PortVariant = std::variant<
        UBX_CFG_PRT_SPI,
        UBX_CFG_PRT_UART1,
        UBX_CFG_PRT_UART2
    >;
    
    PortVariant impl_;
};

}  // JimmyPaputto::ubxmsg

#endif  // UBX_CFG_PRT_HPP_