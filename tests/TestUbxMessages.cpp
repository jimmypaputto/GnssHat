#include <gtest/gtest.h>
#include <cstring>
#include <vector>

#include "ublox/ubxmsg/UBX_NAV_PVT.hpp"
#include "ublox/ubxmsg/UBX_NAV_DOP.hpp"
#include "ublox/ubxmsg/UBX_NAV_SAT.hpp"
#include "ublox/ubxmsg/UBX_MON_RF.hpp"
#include "ublox/ubxmsg/UBX_NAV_GEOFENCE.hpp"
#include "ublox/ubxmsg/UBX_CFG_VALSET.hpp"
#include "ublox/UbxParser.hpp"
#include "common/Utils.hpp"

using namespace JimmyPaputto;
using namespace JimmyPaputto::ubxmsg;


class NavPvtTest : public ::testing::Test
{
protected:
    std::vector<uint8_t> buildPvtFrame()
    {
        std::vector<uint8_t> frame(92, 0);

        // year=2025 at offset 10
        uint16_t year = 2025;
        std::memcpy(&frame[10], &year, 2);
        frame[12] = 3;   // month
        frame[13] = 15;  // day
        frame[14] = 12;  // hh
        frame[15] = 30;  // mm
        frame[16] = 45;  // ss
        frame[17] = 0x03; // dateValid(bit1)=1, timeValid(bit0)=1

        // tAcc at offset 18
        int32_t tAcc = 50;
        std::memcpy(&frame[18], &tAcc, 4);

        frame[26] = 0x03; // fixType = Fix3D
        frame[27] = 0x01; // gnssFixOK (bit 0)
        frame[29] = 12;   // numSV

        // lon at offset 30: 12.4567890° → 124567890
        int32_t lon = 124567890;
        std::memcpy(&frame[30], &lon, 4);

        // lat at offset 34: 41.9028000° → 419028000
        int32_t lat = 419028000;
        std::memcpy(&frame[34], &lat, 4);

        // altitude at offset 38: 150000 mm → 150.0 m
        int32_t alt = 150000;
        std::memcpy(&frame[38], &alt, 4);

        // altitudeMSL at offset 42: 148500 mm → 148.5 m
        int32_t altMSL = 148500;
        std::memcpy(&frame[42], &altMSL, 4);

        // hAcc at offset 46: 2500 mm → 2.5 m
        uint32_t hAcc = 2500;
        std::memcpy(&frame[46], &hAcc, 4);

        // vAcc at offset 50: 3200 mm → 3.2 m
        uint32_t vAcc = 3200;
        std::memcpy(&frame[50], &vAcc, 4);

        // speed at offset 66: 1500 mm/s → 1.5 m/s
        int32_t speed = 1500;
        std::memcpy(&frame[66], &speed, 4);

        // heading at offset 70: 18000000 → 180.0 deg
        int32_t heading = 18000000;
        std::memcpy(&frame[70], &heading, 4);

        // sAcc at offset 74: 500 mm/s → 0.5 m/s
        uint32_t sAcc = 500;
        std::memcpy(&frame[74], &sAcc, 4);

        // headAcc at offset 78: 1000000 → 10.0 deg
        uint32_t headAcc = 1000000;
        std::memcpy(&frame[78], &headAcc, 4);

        return frame;
    }
};

TEST_F(NavPvtTest, DeserializesDateAndTime)
{
    auto frame = buildPvtFrame();
    UBX_NAV_PVT msg(frame);
    auto pvt = msg.pvt();

    EXPECT_EQ(pvt.date.year, 2025);
    EXPECT_EQ(pvt.date.month, 3);
    EXPECT_EQ(pvt.date.day, 15);
    EXPECT_TRUE(pvt.date.valid);

    EXPECT_EQ(pvt.utc.hh, 12);
    EXPECT_EQ(pvt.utc.mm, 30);
    EXPECT_EQ(pvt.utc.ss, 45);
    EXPECT_TRUE(pvt.utc.valid);
    EXPECT_EQ(pvt.utc.accuracy, 50);
}

TEST_F(NavPvtTest, DeserializesFixInfo)
{
    auto frame = buildPvtFrame();
    UBX_NAV_PVT msg(frame);
    auto pvt = msg.pvt();

    EXPECT_EQ(pvt.fixType, EFixType::Fix3D);
    EXPECT_EQ(pvt.fixStatus, EFixStatus::Active);
    EXPECT_EQ(pvt.fixQuality, EFixQuality::GpsFix2D3D);
    EXPECT_EQ(pvt.visibleSatellites, 12);
}

TEST_F(NavPvtTest, DeserializesPosition)
{
    auto frame = buildPvtFrame();
    UBX_NAV_PVT msg(frame);
    auto pvt = msg.pvt();

    EXPECT_NEAR(pvt.longitude, 12.456789, 0.0000002);
    EXPECT_NEAR(pvt.latitude, 41.9028, 0.0000002);
    EXPECT_NEAR(pvt.altitude, 150.0f, 0.01f);
    EXPECT_NEAR(pvt.altitudeMSL, 148.5f, 0.01f);
}

TEST_F(NavPvtTest, DeserializesAccuracyAndSpeed)
{
    auto frame = buildPvtFrame();
    UBX_NAV_PVT msg(frame);
    auto pvt = msg.pvt();

    EXPECT_NEAR(pvt.horizontalAccuracy, 2.5f, 0.01f);
    EXPECT_NEAR(pvt.verticalAccuracy, 3.2f, 0.01f);
    EXPECT_NEAR(pvt.speedOverGround, 1.5f, 0.01f);
    EXPECT_NEAR(pvt.heading, 180.0f, 0.01f);
    EXPECT_NEAR(pvt.speedAccuracy, 0.5f, 0.01f);
    EXPECT_NEAR(pvt.headingAccuracy, 10.0f, 0.01f);
}

TEST_F(NavPvtTest, DGNSSQuality)
{
    auto frame = buildPvtFrame();
    frame[27] = 0x03; // gnssFixOK + diffSoln (bit 1)
    UBX_NAV_PVT msg(frame);
    EXPECT_EQ(msg.pvt().fixQuality, EFixQuality::DGNSS);
}

TEST_F(NavPvtTest, FloatRtkQuality)
{
    auto frame = buildPvtFrame();
    frame[27] = 0x41; // gnssFixOK + carrSoln bit6=1
    UBX_NAV_PVT msg(frame);
    EXPECT_EQ(msg.pvt().fixQuality, EFixQuality::FloatRtk);
}

TEST_F(NavPvtTest, FixedRtkQuality)
{
    auto frame = buildPvtFrame();
    frame[27] = 0x81; // gnssFixOK + carrSoln bit7=1
    UBX_NAV_PVT msg(frame);
    EXPECT_EQ(msg.pvt().fixQuality, EFixQuality::FixedRTK);
}

TEST_F(NavPvtTest, NoFix)
{
    auto frame = buildPvtFrame();
    frame[26] = 0x00; // NoFix
    frame[27] = 0x00;
    UBX_NAV_PVT msg(frame);
    EXPECT_EQ(msg.pvt().fixType, EFixType::NoFix);
    EXPECT_EQ(msg.pvt().fixQuality, EFixQuality::Invalid);
}


TEST(NavDop, DeserializesAllFields)
{
    std::vector<uint8_t> frame(24, 0);

    auto setU16 = [&](size_t off, uint16_t val) {
        std::memcpy(&frame[off], &val, 2);
    };

    setU16(10, 150);  // gDOP = 1.50
    setU16(12, 120);  // pDOP = 1.20
    setU16(14, 80);   // tDOP = 0.80
    setU16(16, 100);  // vDOP = 1.00
    setU16(18, 70);   // hDOP = 0.70
    setU16(20, 60);   // nDOP = 0.60
    setU16(22, 50);   // eDOP = 0.50

    UBX_NAV_DOP msg(frame);
    auto dop = msg.dop();

    EXPECT_NEAR(dop.geometric, 1.50f, 0.01f);
    EXPECT_NEAR(dop.position, 1.20f, 0.01f);
    EXPECT_NEAR(dop.time, 0.80f, 0.01f);
    EXPECT_NEAR(dop.vertical, 1.00f, 0.01f);
    EXPECT_NEAR(dop.horizontal, 0.70f, 0.01f);
    EXPECT_NEAR(dop.northing, 0.60f, 0.01f);
    EXPECT_NEAR(dop.easting, 0.50f, 0.01f);
}


class NavSatTest : public ::testing::Test
{
protected:
    std::vector<uint8_t> buildSatFrame(uint8_t numSvs,
        const std::vector<SatelliteInfo>& sats)
    {
        constexpr uint8_t headerSize = 14;
        constexpr uint8_t blockSize = 12;
        std::vector<uint8_t> frame(headerSize + numSvs * blockSize, 0);
        frame[11] = numSvs;

        for (uint8_t i = 0; i < numSvs && i < sats.size(); i++)
        {
            size_t off = headerSize + i * blockSize;
            frame[off + 0] = static_cast<uint8_t>(sats[i].gnssId);
            frame[off + 1] = sats[i].svId;
            frame[off + 2] = sats[i].cno;
            frame[off + 3] = static_cast<uint8_t>(sats[i].elevation);

            int16_t az = sats[i].azimuth;
            std::memcpy(&frame[off + 4], &az, 2);

            uint32_t flags = static_cast<uint8_t>(sats[i].quality) & 0x07;
            if (sats[i].usedInFix) flags |= (1 << 3);
            if (sats[i].healthy) flags |= (1 << 4);
            if (sats[i].diffCorr) flags |= (1 << 6);
            if (sats[i].ephAvail) flags |= (1 << 8);
            if (sats[i].almAvail) flags |= (1 << 9);
            std::memcpy(&frame[off + 8], &flags, 4);
        }
        return frame;
    }
};

TEST_F(NavSatTest, DeserializesOneSatellite)
{
    SatelliteInfo sat {};
    sat.gnssId = EGnssId::GPS;
    sat.svId = 5;
    sat.cno = 42;
    sat.elevation = 45;
    sat.azimuth = 270;
    sat.quality = ESvQuality::CodeAndCarrierLocked3;
    sat.usedInFix = true;
    sat.healthy = true;
    sat.ephAvail = true;

    auto frame = buildSatFrame(1, { sat });
    UBX_NAV_SAT msg(frame);

    ASSERT_EQ(msg.satellites().size(), 1u);
    const auto& s = msg.satellites()[0];
    EXPECT_EQ(s.gnssId, EGnssId::GPS);
    EXPECT_EQ(s.svId, 5);
    EXPECT_EQ(s.cno, 42);
    EXPECT_EQ(s.elevation, 45);
    EXPECT_EQ(s.azimuth, 270);
    EXPECT_EQ(s.quality, ESvQuality::CodeAndCarrierLocked3);
    EXPECT_TRUE(s.usedInFix);
    EXPECT_TRUE(s.healthy);
    EXPECT_TRUE(s.ephAvail);
}

TEST_F(NavSatTest, DeserializesMultipleSatellites)
{
    SatelliteInfo gps { .gnssId = EGnssId::GPS, .svId = 1, .cno = 35 };
    SatelliteInfo gal { .gnssId = EGnssId::Galileo, .svId = 10, .cno = 28 };
    SatelliteInfo glo { .gnssId = EGnssId::GLONASS, .svId = 20, .cno = 15 };

    auto frame = buildSatFrame(3, { gps, gal, glo });
    UBX_NAV_SAT msg(frame);

    ASSERT_EQ(msg.satellites().size(), 3u);
    EXPECT_EQ(msg.satellites()[0].gnssId, EGnssId::GPS);
    EXPECT_EQ(msg.satellites()[1].gnssId, EGnssId::Galileo);
    EXPECT_EQ(msg.satellites()[2].gnssId, EGnssId::GLONASS);
}

TEST_F(NavSatTest, ZeroSatellites)
{
    auto frame = buildSatFrame(0, {});
    UBX_NAV_SAT msg(frame);
    EXPECT_TRUE(msg.satellites().empty());
}

TEST_F(NavSatTest, NegativeElevation)
{
    SatelliteInfo sat {};
    sat.elevation = -10;
    auto frame = buildSatFrame(1, { sat });
    UBX_NAV_SAT msg(frame);
    EXPECT_EQ(msg.satellites()[0].elevation, -10);
}

TEST(MonRf, DeserializesTwoBlocksM9)
{
    std::vector<uint8_t> frame(10 + 2 * 24, 0x00);
    frame[7] = 2; // numberOfRfBlocks

    // Block 0: L1
    frame[10] = 0x00; // L1
    frame[11] = 0x01; // jammingState=Ok
    frame[12] = 0x02; // antennaStatus=Ok
    frame[13] = 0x01; // antennaPower=On
    frame[31] = 0x00; // gnssBand 0x00 for M9

    // Block 1: L2/L5
    frame[10 + 24] = 0x01; // L2/L5
    frame[11 + 24] = 0x02; // jammingState=Warning
    frame[12 + 24] = 0x02;
    frame[13 + 24] = 0x01;
    frame[31 + 24] = 0x00;

    UBX_MON_RF msg(frame);
    ASSERT_EQ(msg.rfBlocks().size(), 2u);

    EXPECT_EQ(msg.rfBlocks()[0].id, 0x00);
    EXPECT_EQ(msg.rfBlocks()[0].jammingState, EJammingState::Ok_NoSignifantJamming);
    EXPECT_EQ(msg.rfBlocks()[0].antennaStatus, EAntennaStatus::Ok);
    EXPECT_EQ(msg.rfBlocks()[0].antennaPower, EAntennaPower::On);
    EXPECT_EQ(msg.rfBlocks()[0].gnssBand, EGnssBand::L1);

    EXPECT_EQ(msg.rfBlocks()[1].id, 0x01);
    EXPECT_EQ(msg.rfBlocks()[1].jammingState, EJammingState::Warning_InferenceVisibleButFixOk);
    EXPECT_EQ(msg.rfBlocks()[1].antennaStatus, EAntennaStatus::Ok);
    EXPECT_EQ(msg.rfBlocks()[1].antennaPower, EAntennaPower::On);
    EXPECT_EQ(msg.rfBlocks()[1].gnssBand, EGnssBand::L2orL5);
}

TEST(MonRf, DeserializesTwoBlocksF10T)
{
    std::vector<uint8_t> frame(10 + 2 * 24, 0x00);
    frame[7] = 2; // numberOfRfBlocks

    // Block 0: L1
    frame[10] = 0x00; // L1
    frame[11] = 0x01; // jammingState=Ok
    frame[12] = 0x02; // antennaStatus=Ok
    frame[13] = 0x01; // antennaPower=On
    frame[31] = 0x01; // gnssBand 0x00 for M9

    // Block 1: L2/L5
    frame[10 + 24] = 0x01; // L2/L5
    frame[11 + 24] = 0x02; // jammingState=Warning
    frame[12 + 24] = 0x02;
    frame[13 + 24] = 0x01;
    frame[31 + 24] = 0x04;

    UBX_MON_RF msg(frame);
    ASSERT_EQ(msg.rfBlocks().size(), 2u);

    EXPECT_EQ(msg.rfBlocks()[0].id, 0x00);
    EXPECT_EQ(msg.rfBlocks()[0].jammingState, EJammingState::Ok_NoSignifantJamming);
    EXPECT_EQ(msg.rfBlocks()[0].antennaStatus, EAntennaStatus::Ok);
    EXPECT_EQ(msg.rfBlocks()[0].antennaPower, EAntennaPower::On);
    EXPECT_EQ(msg.rfBlocks()[0].gnssBand, EGnssBand::L1);

    EXPECT_EQ(msg.rfBlocks()[1].id, 0x01);
    EXPECT_EQ(msg.rfBlocks()[1].jammingState, EJammingState::Warning_InferenceVisibleButFixOk);
    EXPECT_EQ(msg.rfBlocks()[1].antennaStatus, EAntennaStatus::Ok);
    EXPECT_EQ(msg.rfBlocks()[1].antennaPower, EAntennaPower::On);
    EXPECT_EQ(msg.rfBlocks()[1].gnssBand, EGnssBand::L5);
}


TEST(NavGeofence, DeserializesActiveGeofencing)
{
    std::vector<uint8_t> frame(22, 0);

    // iTOW at offset 6
    uint32_t iTOW = 123456;
    std::memcpy(&frame[6], &iTOW, 4);

    frame[11] = 0x01; // Active
    frame[12] = 2;    // 2 geofences
    frame[13] = 0x01; // combinedState = Inside

    frame[14] = 0x01; // fence 0 = Inside
    frame[16] = 0x02; // fence 1 = Outside

    UBX_NAV_GEOFENCE msg(frame);
    auto nav = msg.nav();

    EXPECT_EQ(nav.iTOW, 123456u);
    EXPECT_EQ(nav.geofencingStatus, EGeofencingStatus::Active);
    EXPECT_EQ(nav.numberOfGeofences, 2);
    EXPECT_EQ(nav.combinedState, EGeofenceStatus::Inside);
    EXPECT_EQ(nav.geofencesStatus[0], EGeofenceStatus::Inside);
    EXPECT_EQ(nav.geofencesStatus[1], EGeofenceStatus::Outside);
}

TEST(NavGeofence, NotAvailable)
{
    std::vector<uint8_t> frame(14, 0);
    frame[11] = 0x00; // NotAvailable

    UBX_NAV_GEOFENCE msg(frame);
    auto nav = msg.nav();

    EXPECT_EQ(nav.geofencingStatus, EGeofencingStatus::NotAvailable);
    EXPECT_EQ(nav.numberOfGeofences, 0);
}


TEST(CfgValset, SerializesU1)
{
    auto frame = UBX_CFG_VALSET::setU1(0x20640001, 0x04);

    EXPECT_GE(frame.size(), 8u);
    EXPECT_EQ(frame[0], 0xB5);
    EXPECT_EQ(frame[1], 0x62);
    EXPECT_EQ(frame[2], 0x06); // class
    EXPECT_EQ(frame[3], 0x8A); // id

    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));
}

TEST(CfgValset, SerializesU2)
{
    auto frame = UBX_CFG_VALSET::setU2(0x30210001, 1000);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));
}

TEST(CfgValset, SerializesU4)
{
    auto frame = UBX_CFG_VALSET::setU4(0x40520001, 115200);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));
}

TEST(CfgValset, DifferentLayerRAM)
{
    auto frame = UBX_CFG_VALSET::setU1(0x20640001, 0x01, EUbxMemoryLayer::RAM);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));
    EXPECT_EQ(frame[7], 0x01); // layer byte = RAM
}

TEST(CfgValset, DifferentLayerBBR)
{
    auto frame = UBX_CFG_VALSET::setU1(0x20640001, 0x01, EUbxMemoryLayer::BBR);
    EXPECT_TRUE(JimmyPaputto::UbxParser::checkFrame(frame));
    EXPECT_EQ(frame[7], 0x02); // layer byte = BBR
}
