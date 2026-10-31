#include "MusicWheelStats.h"

#include <algorithm>
#include <set>

#include "MusicWheelItem.h"
#include "Profile.h"
#include "Song.h"
#include "Steps.h"

MusicWheelSectionScoreStats GetMusicWheelSectionScoreStats(
    const std::vector<MusicWheelItemData*>& items, const std::string& section,
    const std::string& parent, bool isParent, StepsType stepsType,
    Difficulty difficulty, const Profile* profile, const Steps* selectedSteps,
    bool isFolderSelection) {
  MusicWheelSectionScoreStats stats;
  if (selectedSteps && !isFolderSelection) {
    difficulty = selectedSteps->GetDifficulty();
    stepsType = selectedSteps->m_StepsType;
  }
  stats.difficulty = difficulty;
  if (stepsType < 0 || stepsType >= NUM_StepsType || difficulty < 0 ||
      difficulty >= NUM_Difficulty) {
    return stats;
  }

  std::set<const Song*> seen;
  double meterSum = 0;
  for (const auto* item : items) {
    if (item->m_Type != WheelItemDataType_Song || !item->m_pSong) {
      continue;
    }
    if (isParent
            ? section.empty() || item->m_sParentSection != section
            : item->m_sText != section || item->m_sParentSection != parent) {
      continue;
    }
    const Song* song = item->m_pSong;
    if (!seen.insert(song).second) {
      continue;
    }
    bool hasCharts = false;
    for (const Steps* steps : song->GetStepsByStepsType(stepsType)) {
      if (steps->GetDifficulty() != difficulty || steps->IsAutogen()) {
        continue;
      }
      hasCharts = true;
      ++stats.charts;
      meterSum += steps->GetMeter();
      Grade grade = Grade_NoData;
      bool quint = false;
      if (profile) {
        const auto& scores = profile->GetStepsHighScoreList(song, steps);
        grade = scores.HighGrade;
        // Also handle lists that have scores but no cached HighGrade.
        for (const auto& score : scores.vHighScores) {
          grade = std::min(grade, score.GetGrade());
          // zmod stores quints as Tier01 with a numeric score of zero.
          quint |= score.GetGrade() == Grade_Tier01 && score.GetScore() == 0;
        }
      }
      if (grade >= 0 && grade < NUM_Grade) {
        ++stats.grades[grade];
        if (quint) {
          ++stats.quintCharts;
        }
        ++stats.scoredCharts;
        if (grade != Grade_Failed) {
          ++stats.passedCharts;
        }
      }
    }
    if (hasCharts) {
      ++stats.songs;
    }
  }
  if (stats.charts != 0) {
    stats.averageMeter = meterSum / stats.charts;
  }
  return stats;
}
