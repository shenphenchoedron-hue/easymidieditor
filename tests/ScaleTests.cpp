#include "Test.h"
#include "theory/Scale.h"

using namespace mc::theory;

static Scale scaleOf(const char* root, const char* id)
{
    return Scale(*parsePitchClass(root), *ScaleRegistry::instance().find(id));
}

TEST(a_natural_minor_pitch_classes)
{
    CHECK_EQ(scaleOf("A", "natural_minor").pitchClasses(), (std::vector<int>{9, 11, 0, 2, 4, 5, 7}));
}

TEST(c_and_g_major)
{
    CHECK_EQ(scaleOf("C", "major").pitchClasses(), (std::vector<int>{0, 2, 4, 5, 7, 9, 11}));
    CHECK_EQ(scaleOf("G", "major").pitchClasses(), (std::vector<int>{7, 9, 11, 0, 2, 4, 6}));
}

TEST(degree_lookup)
{
    auto s = scaleOf("A", "natural_minor");
    CHECK_EQ(*s.degreeOf(57), 0); // A3
    CHECK_EQ(*s.degreeOf(60), 2); // C4
    CHECK(!s.degreeOf(61));       // C#
    CHECK_EQ(s.semitoneOffsetOfDegree(7), 12);
    CHECK_EQ(s.semitoneOffsetOfDegree(8), 14);
    CHECK_EQ(s.semitoneOffsetOfDegree(-1), -2);
}

TEST(names)
{
    CHECK_EQ(scaleOf("D", "natural_minor").name(), std::string("D Natural Minor"));
    CHECK_EQ(midiNoteName(60), std::string("C4"));
    CHECK_EQ(*parsePitchClass("Bb"), 10);
}

TEST(all_registered_scales_have_seven_ascending_intervals)
{
    for (auto& d : ScaleRegistry::instance().all()) {
        CHECK_EQ(d.intervals.front(), 0);
        for (size_t i = 1; i < d.intervals.size(); ++i) CHECK(d.intervals[i] > d.intervals[i - 1]);
    }
}
