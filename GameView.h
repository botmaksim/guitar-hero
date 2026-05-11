#pragma once
#include "GameEngine.h"
#include "SettingsManager.h"
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

struct HitEffect {
  int lane;
  double radius;
  double alpha;
};

class GameView : public QWidget {
  Q_OBJECT
public:
  explicit GameView(GameEngine *engine, QWidget *parent = nullptr);

signals:
  void requestExit();

protected:
  QPointF getPerspectivePoint(double laneRatio, double engineY);
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;

private:
  GameEngine *m_engine;
  std::vector<QPixmap> m_noteSprites;
  std::vector<QColor> m_noteColors;
  std::vector<HitEffect> m_effects;

  QPixmap m_cachedBackground;
  QString m_cachedBgPath;

  double m_lastUiScale = -1.0;
  QFont m_fontScoreHeader;
  QFont m_fontScoreValue;
  QFont m_fontTitle;
  QFont m_fontCombo;
  QFont m_fontEmoji;
  QFont m_fontGenerating;
  QFont m_fontPercentage;
  QFont m_fontGenSub;
  QFont m_fontKey;

  void updateFonts(double uiScale);
  QPixmap generateNoteSprite(QColor color);
};