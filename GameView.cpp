#include "GameView.h"
#include <QPainterPath>

GameView::GameView(GameEngine *engine, QWidget *parent)
    : QWidget(parent), m_engine(engine) {

  QColor colors[] = {Qt::red,     Qt::green, Qt::cyan,    Qt::yellow,
                     Qt::magenta, Qt::blue,  Qt::darkRed, Qt::darkGreen};
  for (int i = 0; i < 8; ++i) {
    m_noteSprites.push_back(generateNoteSprite(colors[i]));
    m_noteColors.push_back(colors[i]);
  }

  connect(m_engine, &GameEngine::stateUpdated, this, [this]() {
    for (auto &effect : m_effects) {
      effect.radius += 3.0;
      effect.alpha -= 15.0;
    }
    m_effects.erase(
        std::remove_if(m_effects.begin(), m_effects.end(),
                       [](const HitEffect &e) { return e.alpha <= 0; }),
        m_effects.end());
    update();
  });
}

QPointF GameView::getPerspectivePoint(double laneRatio, double engineY) {
  double topY = height() * 0.2;
  double bottomY = height() * 0.9;
  double topWidth = width() * 0.3;
  double bottomWidth = width() * 0.8;

  double t = engineY / 1000.0;
  double curvedT = t * t;

  double screenY = topY + curvedT * (bottomY - topY);
  double currentWidth = topWidth + curvedT * (bottomWidth - topWidth);
  double startX = (width() - currentWidth) / 2.0;

  return QPointF(startX + laneRatio * currentWidth, screenY);
}

void GameView::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  double uiScale = qMax(1.0, qMin(width() / 1100.0, height() / 750.0));
  if (std::abs(uiScale - m_lastUiScale) > 0.01) {
    m_lastUiScale = uiScale;
    updateFonts(uiScale);
  }

  QString bgPath = SettingsManager::instance().getBgImagePath();
  if (!bgPath.isEmpty()) {
    if (bgPath != m_cachedBgPath || m_cachedBackground.size() != size()) {
      m_cachedBgPath = bgPath;
      QPixmap bg(bgPath);
      if (!bg.isNull()) {
        m_cachedBackground = bg.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
      }
    }
    if (!m_cachedBackground.isNull()) {
      painter.drawPixmap(0, 0, m_cachedBackground);
    }
    painter.fillRect(rect(), QColor(10, 0, 20, 180));
  } else {
    painter.fillRect(rect(), QColor(10, 10, 15));
  }

  int lanes = m_engine->laneCount;
  const std::vector<Qt::Key> &currentKeys = m_engine->activeKeys;

  painter.setPen(QPen(QColor(0, 255, 255, 150), 2 * uiScale));
  for (int i = 0; i <= lanes; ++i) {
    double ratio = static_cast<double>(i) / lanes;
    QPointF topP = getPerspectivePoint(ratio, 0);
    QPointF botP = getPerspectivePoint(ratio, 1000);
    painter.drawLine(topP, botP);
  }

  painter.setPen(QPen(QColor(255, 0, 255, 80), 2 * uiScale));
  double timeOffset = fmod(
      m_engine->activeNotes.size() > 0 ? m_engine->activeNotes[0].y : 0, 200.0);
  for (int i = 0; i < 6; i++) {
    double y = i * 200 - timeOffset;
    if (y < 0)
      y += 1200;
    QPointF left = getPerspectivePoint(0, y);
    QPointF right = getPerspectivePoint(1, y);
    painter.drawLine(left, right);
  }

  for (int i = 0; i < lanes; ++i) {
    double centerRatio = (i + 0.5) / lanes;
    QPointF targetPos =
        getPerspectivePoint(centerRatio, m_engine->hitZoneCenter);

    double targetWidth = 80 * uiScale;
    double targetHeight = 25 * uiScale;

    painter.setBrush(QColor(10, 10, 10, 200));
    painter.setPen(QPen(QColor(204, 255, 0, 200), 2 * uiScale));
    painter.drawRoundedRect(targetPos.x() - targetWidth / 2,
                            targetPos.y() - targetHeight / 2, targetWidth,
                            targetHeight, 8 * uiScale, 8 * uiScale);

    if (i < currentKeys.size()) {
      QString keyName = QKeySequence(currentKeys[i]).toString();
      painter.setPen(Qt::white);
      painter.setFont(m_fontKey);
      painter.drawText(QRectF(targetPos.x() - 20 * uiScale,
                              targetPos.y() - 15 * uiScale, 40 * uiScale,
                              30 * uiScale),
                       Qt::AlignCenter, keyName);
    }
  }

  for (const auto &note : m_engine->activeNotes) {
    if (!note.active)
      continue;
    double centerRatio = (note.lane + 0.5) / lanes;
    double leftRatio = static_cast<double>(note.lane) / lanes + 0.05;
    double rightRatio = static_cast<double>(note.lane + 1) / lanes - 0.05;

    if (note.length > 0) {
      double tailY = std::max(0.0, note.y - note.length);
      QPointF botLeft = getPerspectivePoint(leftRatio, note.y);
      QPointF botRight = getPerspectivePoint(rightRatio, note.y);
      QPointF topLeft = getPerspectivePoint(leftRatio, tailY);
      QPointF topRight = getPerspectivePoint(rightRatio, tailY);

      QPainterPath tailPath;
      tailPath.moveTo(botLeft);
      tailPath.lineTo(botRight);
      tailPath.lineTo(topRight);
      tailPath.lineTo(topLeft);
      tailPath.closeSubpath();

      QColor tailColor = m_noteColors[note.lane % m_noteColors.size()];
      tailColor.setAlpha(note.isHeld ? 220 : 120);
      painter.fillPath(tailPath, tailColor);
    }

    QPointF notePos = getPerspectivePoint(centerRatio, note.y);
    double depthScale = 0.4 + (note.y / 1000.0) * 0.8;
    double finalScale = depthScale * uiScale;

    double w = 100 * finalScale;
    double h = 40 * finalScale;

    const QPixmap &sprite = m_noteSprites[note.lane % m_noteSprites.size()];
    painter.drawPixmap(
        QRectF(notePos.x() - w / 2.0, notePos.y() - h / 2.0, w, h), sprite,
        sprite.rect());
  }

  for (const auto &effect : m_effects) {
    double centerRatio = (effect.lane + 0.5) / lanes;
    QPointF targetPos =
        getPerspectivePoint(centerRatio, m_engine->hitZoneCenter);
    painter.setPen(QPen(QColor(204, 255, 0, effect.alpha), 4 * uiScale));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(targetPos, effect.radius * uiScale,
                        effect.radius * 0.6 * uiScale);
  }

  painter.setFont(m_fontScoreHeader);

  painter.setBrush(QColor(10, 10, 12, 220));
  painter.setPen(QPen(QColor(204, 255, 0), 2 * uiScale));
  painter.drawRect(10 * uiScale, 10 * uiScale, 240 * uiScale, 90 * uiScale);

  painter.setPen(QColor(255, 255, 255));
  painter.drawText(20 * uiScale, 40 * uiScale, QString("SCORE"));

  painter.setFont(m_fontScoreValue);
  painter.setPen(QColor(204, 255, 0));
  painter.drawText(
      20 * uiScale, 80 * uiScale,
      QString("%1").arg(m_engine->currentScore, 7, 10, QChar('0')));

  painter.setFont(m_fontTitle);
  painter.setPen(QColor(204, 255, 0));

  QRect titleRect(width() - 620 * uiScale, 20 * uiScale, 600 * uiScale,
                  60 * uiScale);
  QString title = m_engine->currentSongName.toUpper();
  QFontMetrics fm(m_fontTitle);
  QString elidedTitle = fm.elidedText(title, Qt::ElideRight, titleRect.width());

  painter.drawText(titleRect, Qt::AlignRight | Qt::AlignTop | Qt::TextDontClip,
                   elidedTitle);

  if (m_engine->comboMultiplier > 1) {
    painter.setFont(m_fontCombo);
    painter.setPen(QColor(0, 255, 255));

    QString comboEmoji;
    if (m_engine->comboMultiplier >= 50) {
      comboEmoji = " 🎸🔥🚀🌟";
    } else if (m_engine->comboMultiplier >= 20) {
      comboEmoji = " 🚀🌟";
    } else if (m_engine->comboMultiplier >= 10) {
      comboEmoji = " ⚡";
    } else if (m_engine->comboMultiplier >= 5) {
      comboEmoji = " 🔥";
    } else {
      comboEmoji = " 👍";
    }

    QString comboText = QString("x%1").arg(m_engine->comboMultiplier);
    painter.drawText(20 * uiScale, 150 * uiScale, comboText);

    if (!comboEmoji.isEmpty()) {
      QFontMetrics fm(m_fontCombo);
      int textWidth = fm.horizontalAdvance(comboText);
      painter.setFont(m_fontEmoji);
      painter.drawText(20 * uiScale + textWidth, 150 * uiScale, comboEmoji);
    }
  }

  if (m_engine->isGenerating || m_engine->isWaitingForMedia) {
    painter.fillRect(rect(), QColor(10, 0, 20, 210));

    painter.setFont(m_fontGenerating);
    painter.setPen(QColor(255, 0, 255));

    if (m_engine->isGenerating && m_engine->generationProgress < 100) {
      painter.drawText(rect().adjusted(0, -60 * uiScale, 0, 0), Qt::AlignCenter,
                       "EXPERIMENTAL AUTO-CHARTING...");

      int barWidth = width() * 0.6;
      int barHeight = 24 * uiScale;
      int barX = (width() - barWidth) / 2;
      int barY = height() / 2 + 30 * uiScale;

      painter.setPen(QPen(QColor(0, 255, 255), 2 * uiScale));
      painter.setBrush(Qt::NoBrush);
      painter.drawRect(barX, barY, barWidth, barHeight);

      painter.setBrush(QColor(204, 255, 0));
      painter.setPen(Qt::NoPen);
      painter.drawRect(
          barX, barY,
          barWidth * (std::min(100, std::max(0, m_engine->generationProgress)) /
                      100.0),
          barHeight);

      painter.setFont(m_fontPercentage);
      painter.setPen(Qt::white);
      painter.drawText(QRect(barX, barY - 40 * uiScale, barWidth, 30 * uiScale),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString("%1%").arg(m_engine->generationProgress));

      painter.setFont(m_fontGenSub);
      painter.setPen(QColor(204, 255, 0));
      painter.drawText(
          QRect(barX, barY + barHeight + 10 * uiScale, barWidth, 30 * uiScale),
          Qt::AlignCenter, "POWERED BY ON-THE-FLY MP3 ANALYSIS");
    } else {
      painter.drawText(rect().adjusted(0, -60 * uiScale, 0, 0), Qt::AlignCenter,
                       "LOADING ENGINE...");
      painter.setFont(m_fontTitle);
      painter.setPen(QColor(204, 255, 0));
      painter.drawText(rect().adjusted(0, 30 * uiScale, 0, 0), Qt::AlignCenter,
                       "PLEASE WAIT (BUFFERING AUDIO)");
    }
  }
}

void GameView::keyPressEvent(QKeyEvent *event) {
  if (event->isAutoRepeat())
    return;
  if (event->key() == Qt::Key_Escape) {
    emit requestExit();
    return;
  }

  const std::vector<Qt::Key> &currentKeys = m_engine->activeKeys;
  for (int i = 0; i < currentKeys.size(); ++i) {
    if (event->key() == currentKeys[i]) {
      auto [hit, y] = m_engine->checkHit(i);
      if (hit) {
        m_effects.push_back({i, 35.0, 255.0});
      }
      break;
    }
  }
}

void GameView::keyReleaseEvent(QKeyEvent *event) {
  if (event->isAutoRepeat())
    return;
  const std::vector<Qt::Key> &currentKeys = m_engine->activeKeys;
  for (int i = 0; i < currentKeys.size(); ++i) {
    if (event->key() == currentKeys[i]) {
      m_engine->releaseHit(i);
      break;
    }
  }
}

void GameView::updateFonts(double uiScale) {
  m_fontScoreHeader = QFont("Consolas", 18 * uiScale, QFont::Bold);
  m_fontScoreValue = QFont("Consolas", 26 * uiScale, QFont::Bold);
  m_fontTitle = QFont("Consolas", 16 * uiScale, QFont::Bold);
  m_fontCombo = QFont("Consolas", 28 * uiScale, QFont::Bold);
  m_fontEmoji = QFont("Segoe UI Emoji", 28 * uiScale);
  m_fontEmoji.setFamilies({"Segoe UI Emoji", "Apple Color Emoji",
                           "Noto Color Emoji", "sans-serif"});
  m_fontGenerating = QFont("Consolas", 32 * uiScale, QFont::Bold);
  m_fontPercentage = QFont("Consolas", 18 * uiScale, QFont::Bold);
  m_fontGenSub = QFont("Consolas", 14 * uiScale);
  m_fontKey = QFont("Consolas", 14 * uiScale, QFont::Bold);
}

QPixmap GameView::generateNoteSprite(QColor color) {
  int w = 100;
  int h = 40;
  QPixmap pix(w, h);
  pix.fill(Qt::transparent);
  QPainter p(&pix);
  p.setRenderHint(QPainter::Antialiasing);

  QColor glowColor = color;
  glowColor.setAlpha(80);
  p.setBrush(glowColor);
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(0, 0, w, h, 20, 20);

  QLinearGradient grad(0, 0, w, 0);
  grad.setColorAt(0, color.darker(150));
  grad.setColorAt(0.5, color.lighter(120));
  grad.setColorAt(1, color.darker(150));
  p.setBrush(grad);
  p.setPen(QPen(Qt::white, 2));
  p.drawRoundedRect(10, 5, w - 20, h - 10, 10, 10);

  p.setBrush(Qt::white);
  p.setPen(Qt::NoPen);
  p.drawRoundedRect(20, 12, w - 40, 6, 3, 3);

  return pix;
}