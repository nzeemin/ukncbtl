/* This file is part of UKNCBTL, distributed under the GNU LGPL v3 or later. */
#include "stdafx.h"
#include "BreakpointSet.h"
#include <gtest/gtest.h>

namespace {
constexpr auto none = BreakpointSet::NoBreakpoint;

class BreakpointTests : public testing::Test
{
protected:
    BreakpointSet points;

    void FillPermanent()
    {
        for (uint16_t address = 0; address < BreakpointSet::Capacity - 1; ++address)
            ASSERT_TRUE(points.Add(address));
    }

    void ExpectTerminated(const BreakpointSet& set)
    {
        for (size_t i = set.Addresses().size(); i <= BreakpointSet::Capacity; ++i)
            EXPECT_EQ(set.Data()[i], none) << "slot " << i;
    }
};
}

TEST_F(BreakpointTests, StartsEmptyAndSentinelTerminated)
{
    EXPECT_TRUE(points.Addresses().empty());
    EXPECT_FALSE(points.Contains(0));
    EXPECT_FALSE(points.Contains(none));
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, AddsBoundaryAddressesAndRejectsDuplicatesAndSentinel)
{
    for (const uint16_t address : {0, 1, 0177776})
    {
        EXPECT_TRUE(points.Add(address));
        EXPECT_TRUE(points.Contains(address));
        EXPECT_FALSE(points.Add(address));
    }
    EXPECT_FALSE(points.Add(none));
    EXPECT_EQ(points.Addresses().size(), 3u);
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, PermanentCapacityReservesOneTemporarySlot)
{
    FillPermanent();
    EXPECT_FALSE(points.Add(01000));
    points.SetTemporary(02000);
    EXPECT_EQ(points.Addresses().size(), BreakpointSet::Capacity);
    EXPECT_TRUE(points.Contains(02000));
    for (int attempt = 0; attempt < 3; ++attempt)
        EXPECT_FALSE(points.Add(01000));
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, TemporaryFirstStillAllowsFullPermanentCapacity)
{
    points.SetTemporary(02000);
    FillPermanent();
    EXPECT_EQ(points.Addresses().size(), BreakpointSet::Capacity);
    EXPECT_FALSE(points.Add(01000));
    points.SetTemporary(none);
    EXPECT_EQ(points.Addresses().size(), BreakpointSet::Capacity - 1);
    for (uint16_t address = 0; address < BreakpointSet::Capacity - 1; ++address)
        EXPECT_TRUE(points.Contains(address));
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, RemovesFirstMiddleAndLastWithoutLeavingHoles)
{
    for (const uint16_t removed : {0100, 0200, 0300})
    {
        points.Clear();
        for (const uint16_t address : {0100, 0200, 0300}) ASSERT_TRUE(points.Add(address));
        ASSERT_TRUE(points.Remove(removed));
        EXPECT_EQ(points.Addresses().size(), 2u);
        EXPECT_FALSE(points.Contains(removed));
        for (const uint16_t address : {0100, 0200, 0300})
            EXPECT_EQ(points.Contains(address), address != removed);
        EXPECT_TRUE(points.Add(0400));
        ExpectTerminated(points);
    }
}

TEST_F(BreakpointTests, MissingRemovalDoesNotChangeTheList)
{
    EXPECT_FALSE(points.Remove(0100));
    ASSERT_TRUE(points.Add(0200));
    EXPECT_FALSE(points.Remove(0100));
    EXPECT_FALSE(points.Remove(none));
    ASSERT_EQ(points.Addresses().size(), 1u);
    EXPECT_EQ(points.Data()[0], 0200);
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, RemovingPermanentFromFullListMakesRoomForAnother)
{
    FillPermanent();
    points.SetTemporary(02000);
    ASSERT_TRUE(points.Remove(3));
    EXPECT_TRUE(points.Add(01000));
    EXPECT_FALSE(points.Add(01001));
    points.SetTemporary(none);
    EXPECT_TRUE(points.Contains(01000));
    EXPECT_FALSE(points.Contains(02000));
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, ReplacesAndCancelsTemporaryAtCapacity)
{
    FillPermanent();
    points.SetTemporary(02000);
    points.SetTemporary(03000);
    EXPECT_FALSE(points.Contains(02000));
    EXPECT_TRUE(points.Contains(03000));
    EXPECT_EQ(points.Addresses().size(), BreakpointSet::Capacity);
    points.SetTemporary(none);
    points.SetTemporary(none);
    EXPECT_FALSE(points.Contains(03000));
    EXPECT_EQ(points.Addresses().size(), BreakpointSet::Capacity - 1);
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, RepeatedTemporaryTargetDoesNotDuplicate)
{
    for (int attempt = 0; attempt < 20; ++attempt) points.SetTemporary(0100);
    ASSERT_EQ(points.Addresses().size(), 1u);
    EXPECT_EQ(points.Data()[0], 0100);
    EXPECT_FALSE(points.Add(0100));
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, TemporaryCollisionPreservesPermanentOnCancellation)
{
    ASSERT_TRUE(points.Add(0100));
    points.SetTemporary(0200);
    points.SetTemporary(0100);
    EXPECT_FALSE(points.Contains(0200));
    // Reusing the old temporary address must not leave stale ownership behind.
    ASSERT_TRUE(points.Add(0200));
    points.SetTemporary(none);
    EXPECT_TRUE(points.Contains(0100));
    EXPECT_TRUE(points.Contains(0200));
    EXPECT_EQ(points.Addresses().size(), 2u);
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, RemovingTemporaryThenReaddingAsPermanentSurvivesCancellation)
{
    points.SetTemporary(0100);
    ASSERT_TRUE(points.Remove(0100));
    ASSERT_TRUE(points.Add(0100));
    points.SetTemporary(none);
    EXPECT_TRUE(points.Contains(0100));
    ExpectTerminated(points);
}

TEST_F(BreakpointTests, ClearResetsTemporaryOwnershipAndAllSentinels)
{
    FillPermanent();
    points.SetTemporary(02000);
    points.Clear();
    EXPECT_TRUE(points.Addresses().empty());
    ExpectTerminated(points);
    ASSERT_TRUE(points.Add(02000));
    points.SetTemporary(none);
    EXPECT_TRUE(points.Contains(02000));
    points.SetTemporary(03000);
    EXPECT_TRUE(points.Contains(02000));
    EXPECT_TRUE(points.Contains(03000));
}

TEST_F(BreakpointTests, CpuAndPpuInstancesAreIndependent)
{
    BreakpointSet ppu;
    ASSERT_TRUE(points.Add(0100));
    ASSERT_TRUE(ppu.Add(0100));
    points.SetTemporary(0200);
    ppu.SetTemporary(0300);
    EXPECT_FALSE(points.Contains(0300));
    EXPECT_FALSE(ppu.Contains(0200));
    points.Clear();
    EXPECT_TRUE(ppu.Contains(0100));
    EXPECT_TRUE(ppu.Contains(0300));
    ppu.SetTemporary(none);
    EXPECT_TRUE(ppu.Contains(0100));
    ExpectTerminated(ppu);
}

TEST_F(BreakpointTests, ReturnedStorageStaysStableAcrossMutations)
{
    const auto* storage = points.Data();
    ASSERT_TRUE(points.Add(0100));
    points.SetTemporary(0200);
    ASSERT_TRUE(points.Remove(0100));
    points.Clear();
    EXPECT_EQ(storage, points.Data());
    ExpectTerminated(points);
}
