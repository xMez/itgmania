#include <gtest/gtest.h>

#include "CommonMetrics.h"
#include "CryptManager.h"
#include "GameManager.h"
#include "LuaManager.h"
#include "MusicWheelItem.h"
#include "MusicWheelStats.h"
#include "Profile.h"
#include "Steps.h"

namespace {
class MusicWheelStats : public testing::Test {
 protected:
  void SetUp() override {
    LUA = new LuaManager;
    CRYPTMAN = new CryptManager;
    GAMEMAN = new GameManager;
    class EmptyImageCacheMetric : public ThemeMetric<std::string> {
     public:
      EmptyImageCacheMetric() : ThemeMetric("Common", "ImageCache") {
        Lua* state = LUA->Get();
        lua_pushstring(state, "");
        m_Value.SetFromStack(state);
        LUA->Release(state);
      }
    } imageCache;
    CommonMetrics::IMAGES_TO_CACHE = imageCache;
  }
  void TearDown() override {
    CommonMetrics::IMAGES_TO_CACHE.Clear();
    delete GAMEMAN;
    GAMEMAN = nullptr;
    delete CRYPTMAN;
    CRYPTMAN = nullptr;
    delete LUA;
    LUA = nullptr;
  }
  Steps* Chart(
      Song& song, Difficulty difficulty, int meter,
      StepsType type = StepsType_dance_single) {
    auto* steps = new Steps(&song);
    steps->m_StepsType = type;
    steps->SetDifficulty(difficulty);
    steps->SetMeter(meter);
    song.AddSteps(steps);
    return steps;
  }
  MusicWheelItemData Item(
      Song& song, const std::string& section, const std::string& parent = "") {
    MusicWheelItemData item;
    item.m_Type = WheelItemDataType_Song;
    item.m_pSong = &song;
    item.m_sText = section;
    item.m_sParentSection = parent;
    return item;
  }
};

TEST_F(MusicWheelStats, ExactDifficultyAndStepsTypeOnly) {
  Song song, hardOnly;
  Chart(song, Difficulty_Medium, 6);
  Chart(song, Difficulty_Hard, 12);
  Chart(song, Difficulty_Medium, 9, StepsType_dance_double);
  Chart(hardOnly, Difficulty_Hard, 15);
  auto first = Item(song, "A");
  auto second = Item(hardOnly, "A");
  std::vector<MusicWheelItemData*> items{&first, &second};
  auto medium = GetMusicWheelSectionScoreStats(
      items, "A", "", false, StepsType_dance_single, Difficulty_Medium,
      nullptr);
  EXPECT_EQ(medium.songs, 1);
  EXPECT_EQ(medium.charts, 1);
  EXPECT_EQ(medium.scoredCharts, 0);
  EXPECT_DOUBLE_EQ(medium.averageMeter, 6);
  auto hard = GetMusicWheelSectionScoreStats(
      items, "A", "", false, StepsType_dance_single, Difficulty_Hard, nullptr);
  EXPECT_EQ(hard.charts, 2);
  EXPECT_DOUBLE_EQ(hard.averageMeter, 13.5);
  auto doubles = GetMusicWheelSectionScoreStats(
      items, "A", "", false, StepsType_dance_double, Difficulty_Medium,
      nullptr);
  EXPECT_EQ(doubles.charts, 1);
  EXPECT_DOUBLE_EQ(doubles.averageMeter, 9);
}

TEST_F(MusicWheelStats, SelectedChartOverridesPreferredDifficulty) {
  Song song;
  song.SetSongDir("Songs/test/fallback/");
  auto* expert = Chart(song, Difficulty_Challenge, 14);
  auto* medium = Chart(song, Difficulty_Medium, 7);
  auto item = Item(song, "A");
  Profile player;
  player.GetStepsHighScoreList(&song, expert).HighGrade = Grade_Tier03;
  auto read = [&](const Steps* selected) {
    return GetMusicWheelSectionScoreStats(
        {&item}, "A", "", false, StepsType_dance_single, Difficulty_Medium,
        &player, selected);
  };
  auto fallback = read(expert);
  EXPECT_EQ(fallback.difficulty, Difficulty_Challenge);
  EXPECT_EQ(fallback.charts, 1);
  EXPECT_EQ(fallback.passedCharts, 1);
  EXPECT_DOUBLE_EQ(fallback.averageMeter, 14);
  auto restored = read(medium);
  EXPECT_EQ(restored.difficulty, Difficulty_Medium);
  EXPECT_EQ(restored.passedCharts, 0);
  EXPECT_DOUBLE_EQ(restored.averageMeter, 7);
  // Headers retain the previous song's current steps in GameState.
  // Preferred difficulty must win even when those steps still point to Expert.
  auto folder = GetMusicWheelSectionScoreStats(
      {&item}, "A", "", false, StepsType_dance_single, Difficulty_Medium,
      &player, expert, true);
  EXPECT_EQ(folder.difficulty, Difficulty_Medium);
  EXPECT_EQ(folder.charts, 1);
  EXPECT_EQ(folder.passedCharts, 0);
}

TEST_F(MusicWheelStats, UsesWheelMembershipForAnyGroupingAndParent) {
  Song first, second, third;
  first.m_sGroupName = "Original pack";
  second.m_sGroupName = "Different pack";
  Chart(first, Difficulty_Medium, 4);
  Chart(second, Difficulty_Medium, 8);
  Chart(third, Difficulty_Medium, 12);
  auto a = Item(first, "120-139", "Series");
  auto b = Item(second, "120-139", "Series");
  auto c = Item(third, "140-159", "Series");
  auto otherParent = Item(third, "120-139", "Other series");
  std::vector<MusicWheelItemData*> items{&a, &b, &a, &c, &otherParent};
  auto section = GetMusicWheelSectionScoreStats(
      items, "120-139", "Series", false, StepsType_dance_single,
      Difficulty_Medium, nullptr);
  EXPECT_EQ(section.songs, 2);
  EXPECT_EQ(section.charts, 2);
  EXPECT_DOUBLE_EQ(section.averageMeter, 6);
  auto parent = GetMusicWheelSectionScoreStats(
      items, "Series", "", true, StepsType_dance_single, Difficulty_Medium,
      nullptr);
  EXPECT_EQ(parent.charts, 3);
  EXPECT_DOUBLE_EQ(parent.averageMeter, 8);
  // Filtering a song out of the wheel also removes it from the stats.
  items = {&b};
  auto filtered = GetMusicWheelSectionScoreStats(
      items, "120-139", "Series", false, StepsType_dance_single,
      Difficulty_Medium, nullptr);
  EXPECT_EQ(filtered.charts, 1);
  EXPECT_DOUBLE_EQ(filtered.averageMeter, 8);
}

TEST_F(MusicWheelStats, BestGradePerChartAndFreshPlayerScores) {
  Song song;
  song.SetSongDir("Songs/test/stats/");
  auto* medium = Chart(song, Difficulty_Medium, 6);
  auto* hard = Chart(song, Difficulty_Hard, 12);
  auto item = Item(song, "A");
  Profile player, otherPlayer;
  auto& scores = player.GetStepsHighScoreList(&song, medium);
  HighScore failed, passed;
  failed.SetGrade(Grade_Failed);
  passed.SetGrade(Grade_Tier03);
  scores.vHighScores = {failed, passed, passed};
  player.GetStepsHighScoreList(&song, hard).HighGrade = Grade_Tier01;
  auto read = [&](const Profile* profile) {
    return GetMusicWheelSectionScoreStats(
        {&item}, "A", "", false, StepsType_dance_single, Difficulty_Medium,
        profile);
  };
  auto stats = read(&player);
  EXPECT_EQ(stats.charts, 1);
  EXPECT_EQ(stats.scoredCharts, 1);
  EXPECT_EQ(stats.passedCharts, 1);
  EXPECT_EQ(stats.grades[Grade_Tier03], 1);
  EXPECT_EQ(stats.grades[Grade_Tier01], 0);
  EXPECT_EQ(stats.grades[Grade_Failed], 0);
  EXPECT_EQ(read(&otherPlayer).scoredCharts, 0);
  scores.vHighScores = {failed};
  EXPECT_EQ(read(&player).grades[Grade_Failed], 1);
  EXPECT_EQ(read(&player).passedCharts, 0);
  // Cached best grades remain usable when the score list is trimmed.
  scores.HighGrade = Grade_Tier02;
  scores.vHighScores.clear();
  EXPECT_EQ(read(&player).grades[Grade_Tier02], 1);
}

TEST_F(MusicWheelStats, QuintsAreCountedOnceAndRemainASubsetOfTierOne) {
  Song song;
  song.SetSongDir("Songs/test/quints/");
  auto* medium = Chart(song, Difficulty_Medium, 6);
  auto* hard = Chart(song, Difficulty_Hard, 12);
  auto item = Item(song, "A");
  Profile player, otherPlayer;
  HighScore quint, quad, failed, lower;
  quint.SetGrade(Grade_Tier01);
  quint.SetScore(0);
  quad.SetGrade(Grade_Tier01);
  quad.SetScore(100000000);
  failed.SetGrade(Grade_Failed);
  failed.SetScore(0);
  lower.SetGrade(Grade_Tier02);
  lower.SetScore(0);
  auto& scores = player.GetStepsHighScoreList(&song, medium);
  player.GetStepsHighScoreList(&song, hard).vHighScores = {quint};
  auto read = [&](const Profile* profile) {
    return GetMusicWheelSectionScoreStats(
        {&item, &item}, "A", "", false, StepsType_dance_single,
        Difficulty_Medium, profile);
  };
  scores.vHighScores = {quad};
  EXPECT_EQ(read(&player).quintCharts, 0);
  scores.vHighScores = {quad, quint, quint};
  auto stats = read(&player);
  EXPECT_EQ(stats.quintCharts, 1);
  EXPECT_EQ(stats.grades[Grade_Tier01], 1);
  EXPECT_EQ(stats.scoredCharts, 1);
  EXPECT_EQ(stats.passedCharts, 1);
  EXPECT_EQ(stats.charts, 1);
  EXPECT_EQ(read(&otherPlayer).quintCharts, 0);
  EXPECT_EQ(read(nullptr).quintCharts, 0);
  scores.vHighScores = {failed, lower};
  EXPECT_EQ(read(&player).quintCharts, 0);
  scores.vHighScores.clear();
  scores.HighGrade = Grade_Tier01;
  EXPECT_EQ(read(&player).quintCharts, 0);
  EXPECT_EQ(read(&player).grades[Grade_Tier01], 1);
}

TEST_F(MusicWheelStats, EmptyMissingInvalidAndUnsectionedResults) {
  Song song;
  Chart(song, Difficulty_Medium, 7);
  auto item = Item(song, "");
  auto read = [&](const std::string& section, Difficulty difficulty) {
    return GetMusicWheelSectionScoreStats(
        {&item}, section, "", false, StepsType_dance_single, difficulty,
        nullptr);
  };
  EXPECT_EQ(read("", Difficulty_Medium).charts, 1);
  EXPECT_EQ(read("missing", Difficulty_Medium).charts, 0);
  EXPECT_DOUBLE_EQ(read("missing", Difficulty_Medium).averageMeter, 0);
  EXPECT_EQ(read("", Difficulty_Invalid).charts, 0);
  EXPECT_EQ(read("", Difficulty_Easy).songs, 0);
  EXPECT_EQ(
      GetMusicWheelSectionScoreStats(
          {}, "", "", false, StepsType_Invalid, Difficulty_Medium, nullptr)
          .charts,
      0);
}

TEST_F(MusicWheelStats, CountsMultipleEditsButExcludesAutogeneratedCharts) {
  Song song;
  Chart(song, Difficulty_Edit, 8);
  Chart(song, Difficulty_Edit, 10);
  auto* original = Chart(song, Difficulty_Edit, 14, StepsType_dance_double);
  auto* generated = new Steps(&song);
  generated->AutogenFrom(original, StepsType_dance_single);
  song.AddSteps(generated);
  auto item = Item(song, "Edits");
  auto stats = GetMusicWheelSectionScoreStats(
      {&item}, "Edits", "", false, StepsType_dance_single, Difficulty_Edit,
      nullptr);
  EXPECT_EQ(stats.songs, 1);
  EXPECT_EQ(stats.charts, 2);
  EXPECT_DOUBLE_EQ(stats.averageMeter, 9);
}
}  // namespace
