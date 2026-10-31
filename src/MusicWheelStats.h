#ifndef MUSIC_WHEEL_STATS_H
#define MUSIC_WHEEL_STATS_H

#include <array>
#include <string>
#include <vector>

#include "Difficulty.h"
#include "GameConstantsAndTypes.h"
#include "Grade.h"

struct MusicWheelItemData;
class Profile;
class Steps;

// Counts charts of exactly one difficulty and steps type, in a filtered wheel
// section. Grade counts use each chart's best saved grade (including failures).
struct MusicWheelSectionScoreStats {
  Difficulty difficulty = Difficulty_Invalid;
  int songs = 0;
  int charts = 0;
  int scoredCharts = 0;
  int passedCharts = 0;
  // Subset of Grade_Tier01 charts with a saved zmod quint marker.
  int quintCharts = 0;
  double averageMeter = 0;
  std::array<int, NUM_Grade> grades{};
};

MusicWheelSectionScoreStats GetMusicWheelSectionScoreStats(
    const std::vector<MusicWheelItemData*>& items, const std::string& section,
    const std::string& parent, bool isParent, StepsType stepsType,
    Difficulty difficulty, const Profile* profile,
    const Steps* selectedSteps = nullptr, bool isFolderSelection = false);

#endif
