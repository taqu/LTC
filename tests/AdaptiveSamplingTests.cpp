#include "ltc/AdaptiveSampling.h"
#include <gtest/gtest.h>

using namespace ltc;

TEST(AdaptiveSamplingSchedule, SingleStageWhenEqualBounds)
{
    AdaptiveSamplingSchedule sched(64, 64);
    ASSERT_EQ(sched.stages().size(), 1u);
    EXPECT_EQ(sched.stages()[0].resolution, 64u);
}

TEST(AdaptiveSamplingSchedule, DoublingSequence)
{
    AdaptiveSamplingSchedule sched(32, 128);
    const auto& s = sched.stages();
    ASSERT_EQ(s.size(), 3u);
    EXPECT_EQ(s[0].resolution, 32u);
    EXPECT_EQ(s[1].resolution, 64u);
    EXPECT_EQ(s[2].resolution, 128u);
}

TEST(AdaptiveSamplingSchedule, NonPowerOfTwoMaximum)
{
    AdaptiveSamplingSchedule sched(32, 100);
    const auto& s = sched.stages();
    ASSERT_GE(s.size(), 2u);
    EXPECT_EQ(s.front().resolution, 32u);
    EXPECT_EQ(s.back().resolution, 100u);
}

TEST(AdaptiveSamplingSchedule, SmallRange)
{
    AdaptiveSamplingSchedule sched(16, 32);
    const auto& s = sched.stages();
    ASSERT_EQ(s.size(), 2u);
    EXPECT_EQ(s[0].resolution, 16u);
    EXPECT_EQ(s[1].resolution, 32u);
}

TEST(AdaptiveSamplingSchedule, HasNextStage)
{
    AdaptiveSamplingSchedule sched(32, 128);
    EXPECT_TRUE(sched.hasNextStage(32));
    EXPECT_TRUE(sched.hasNextStage(64));
    EXPECT_FALSE(sched.hasNextStage(128));
    EXPECT_FALSE(sched.hasNextStage(999));
}

TEST(AdaptiveSamplingSchedule, NextResolution)
{
    AdaptiveSamplingSchedule sched(32, 128);
    EXPECT_EQ(sched.nextResolution(32),  64u);
    EXPECT_EQ(sched.nextResolution(64),  128u);
    EXPECT_EQ(sched.nextResolution(128), 128u); // at max, returns self
}

TEST(AdaptiveSamplingSchedule, SingleStageHasNoNext)
{
    AdaptiveSamplingSchedule sched(64, 64);
    EXPECT_FALSE(sched.hasNextStage(64));
    EXPECT_EQ(sched.nextResolution(64), 64u);
}

TEST(AdaptiveSamplingSchedule, ResolutionsArePositive)
{
    AdaptiveSamplingSchedule sched(1, 8);
    for (const auto& st : sched.stages())
        EXPECT_GT(st.resolution, 0u);
}

TEST(AdaptiveSamplingSchedule, ResolutionsAreMonotonicallyIncreasing)
{
    AdaptiveSamplingSchedule sched(16, 256);
    const auto& s = sched.stages();
    for (size_t i = 1; i < s.size(); ++i)
        EXPECT_GT(s[i].resolution, s[i-1].resolution);
}

TEST(AdaptiveSamplingSchedule, FirstStageMustEqualInitial)
{
    AdaptiveSamplingSchedule sched(48, 128);
    EXPECT_EQ(sched.stages().front().resolution, 48u);
}

TEST(AdaptiveSamplingSchedule, LastStageMustEqualMaximum)
{
    AdaptiveSamplingSchedule sched(32, 128);
    EXPECT_EQ(sched.stages().back().resolution, 128u);
}
