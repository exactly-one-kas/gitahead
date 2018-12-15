//
//          Copyright (c) 2016, Scientific Toolworks, Inc.
//
// This software is licensed under the MIT License. The LICENSE.md file
// describes the conditions under which this software may be distributed.
//
// Author: Jason Haslam
//

#include "PlatQt.h"
#include "Scintilla.h"
#include "FontQuality.h"
#include <QApplication>
#include <QFont>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QWidget>
#include <QPixmap>
#include <QPainter>
#include <QMenu>
#include <QAction>
#include <QListWidget>
#include <QVarLengthArray>
#include <QDesktopWidget>
#include <QTextLayout>
#include <QTextLine>
#include <QLibrary>
#include <cstdio>

#ifdef SCI_NAMESPACE
namespace Scintilla {
#endif

Font::Font() : fid(nullptr) {}

Font::~Font()
{
  Release();
}

void Font::Create(const FontParameters &fp)
{
  Release();

  QFont::StyleStrategy strategy;
  switch (fp.extraFontFlag) {
    case SC_EFF_QUALITY_NON_ANTIALIASED:
      strategy = QFont::NoAntialias;
      break;

    case SC_EFF_QUALITY_ANTIALIASED:
    case SC_EFF_QUALITY_LCD_OPTIMIZED:
      strategy = QFont::PreferAntialias;
      break;

    case SC_EFF_QUALITY_DEFAULT:
    default:
      strategy = QFont::PreferDefault;
      break;
  }

  QFont *font = new QFont;
  font->setStyleStrategy(strategy);
  font->setFamily(QString::fromUtf8(fp.faceName));
  font->setPointSize(fp.size);
  font->setBold(fp.weight > 500);
  font->setItalic(fp.italic);

  fid = font;
}

void Font::Release()
{
  delete static_cast<QFont *>(fid);
  fid = nullptr;
}


SurfaceImpl::SurfaceImpl() {}

SurfaceImpl::~SurfaceImpl()
{
  Release();
}

void SurfaceImpl::Init(WindowID wid)
{
  Release();
  device = static_cast<QWidget *>(wid);
}

void SurfaceImpl::Init(SurfaceID sid, WindowID wid)
{
  Release();
  device = static_cast<QPaintDevice *>(sid);
}

void SurfaceImpl::InitPixMap(
  int width,
  int height,
  Surface *surface,
  WindowID wid)
{
  Release();
  deviceOwned = true;
  device = new QPixmap(qMax(1, width), qMax(1, height));
}

void SurfaceImpl::Release()
{
  delete painter;
  painter = nullptr;

  if (deviceOwned)
    delete device;

  device = nullptr;
  deviceOwned = false;
}

bool SurfaceImpl::Initialised()
{
  return device;
}

void SurfaceImpl::PenColour(ColourDesired fore)
{
  QPen pen(QColorFromCA(fore));
  pen.setCapStyle(Qt::FlatCap);
  GetPainter()->setPen(pen);
}

int SurfaceImpl::LogPixelsY()
{
  return device->logicalDpiY();
}

int SurfaceImpl::DeviceHeightFont(int points)
{
  return points;
}

void SurfaceImpl::MoveTo(int x_, int y_)
{
  x = x_;
  y = y_;
}

void SurfaceImpl::LineTo(int x_, int y_)
{
  QLineF line(x, y, x_, y_);
  GetPainter()->drawLine(line);
  x = x_;
  y = y_;
}

void SurfaceImpl::Polygon(
  Point *pts,
  int npts,
  ColourDesired fore,
  ColourDesired back)
{
  QVarLengthArray<QPoint,8> qpts(npts);
  for (int i = 0; i < npts; i++)
    qpts[i] = QPoint(pts[i].x, pts[i].y);

  PenColour(fore);

  QPainter *painter = GetPainter();
  painter->setBrush(QColorFromCA(back));
  painter->drawPolygon(qpts.data(), npts);
}

void SurfaceImpl::RectangleDraw(
  PRectangle rc,
  ColourDesired fore,
  ColourDesired back)
{
  PenColour(fore);

  QPainter *painter = GetPainter();
  painter->setBrush(QColorFromCA(back));
  painter->drawRect(QRectF(rc.left, rc.top, rc.Width() - 1, rc.Height() - 1));
}

void SurfaceImpl::FillRectangle(PRectangle rc, ColourDesired back)
{
  GetPainter()->fillRect(QRectFFromPRect(rc), QColorFromCA(back));
}

void SurfaceImpl::FillRectangle(PRectangle rc, Surface &surfacePattern)
{
  // Tile pattern over rectangle
  SurfaceImpl *surface = static_cast<SurfaceImpl *>(&surfacePattern);
  // Currently assumes 8x8 pattern
  int widthPat = 8;
  int heightPat = 8;
  for (int xTile = rc.left; xTile < rc.right; xTile += widthPat) {
    int widthx = (xTile + widthPat > rc.right) ? rc.right - xTile : widthPat;
    for (int yTile = rc.top; yTile < rc.bottom; yTile += heightPat) {
      int heighty = (yTile + heightPat > rc.bottom) ? rc.bottom - yTile : heightPat;
      QRect source(0, 0, widthx, heighty);
      QRect target(xTile, yTile, widthx, heighty);
      QPixmap *pixmap = static_cast<QPixmap *>(surface->device);
      GetPainter()->drawPixmap(target, *pixmap, source);
    }
  }
}

void SurfaceImpl::RoundedRectangle(
  PRectangle rc,
  ColourDesired fore,
  ColourDesired back)
{
  PenColour(fore);

  QPainter *painter = GetPainter();
  painter->drawRoundRect(QRectFFromPRect(rc));
  painter->setBrush(QColorFromCA(back));
}

void SurfaceImpl::AlphaRectangle(
  PRectangle rc,
  int cornerSize,
  ColourDesired fill,
  int alphaFill,
  ColourDesired outline,
  int alphaOutline,
  int flags)
{
  QPainter *painter = GetPainter();

  QColor qOutline = QColorFromCA(outline);
  qOutline.setAlpha(alphaOutline);
  painter->setPen(QPen(qOutline));

  QColor qFill = QColorFromCA(fill);
  qFill.setAlpha(alphaFill);
  painter->setBrush(qFill);

  QRectF rect(rc.left, rc.top, rc.Width() - 1, rc.Height() - 1);
  painter->drawRoundedRect(rect, cornerSize, cornerSize);
}

void SurfaceImpl::DrawRGBAImage(
  PRectangle rc,
  int width,
  int height,
  const unsigned char *pixels)
{
  QImage image(pixels, width, height, QImage::Format_ARGB32_Premultiplied);
  image.setDevicePixelRatio(width / rc.Width());
  GetPainter()->drawImage(QPointF(rc.left, rc.top), image);
}

void SurfaceImpl::Ellipse(
  PRectangle rc,
  ColourDesired fore,
  ColourDesired back)
{
  PenColour(fore);

  QPainter *painter = GetPainter();
  painter->setBrush(QColorFromCA(back));
  painter->drawEllipse(QRectFFromPRect(rc));
}

void SurfaceImpl::Copy(PRectangle rc, Point from, Surface &surfaceSource)
{
  SurfaceImpl *source = static_cast<SurfaceImpl *>(&surfaceSource);
  QPixmap *pixmap = static_cast<QPixmap *>(source->device);
  GetPainter()->drawPixmap(rc.left, rc.top, *pixmap, from.x, from.y, -1, -1);
}

void SurfaceImpl::DrawTextNoClip(
  PRectangle rc,
  Font &font,
  XYPOSITION ybase,
  const char *s,
  int len,
  ColourDesired fore,
  ColourDesired back)
{
  PenColour(fore);

  QPainter *painter = GetPainter();
  if (FontID id = font.GetID())
    painter->setFont(*static_cast<QFont *>(id));

  painter->setBackground(QColorFromCA(back));
  painter->setBackgroundMode(Qt::OpaqueMode);
  painter->drawText(QPointF(rc.left, ybase), QString::fromUtf8(s, len));
}

void SurfaceImpl::DrawTextClipped(
  PRectangle rc,
  Font &font,
  XYPOSITION ybase,
  const char *s,
  int len,
  ColourDesired fore,
  ColourDesired back)
{
  SetClip(rc);
  DrawTextNoClip(rc, font, ybase, s, len, fore, back);
  GetPainter()->setClipping(false);
}

void SurfaceImpl::DrawTextTransparent(
  PRectangle rc,
  Font &font,
  XYPOSITION ybase,
  const char *s,
  int len,
  ColourDesired fore)
{
  PenColour(fore);

  QPainter *painter = GetPainter();
  if (FontID id = font.GetID())
    painter->setFont(*static_cast<QFont *>(id));

  painter->setBackgroundMode(Qt::TransparentMode);
  painter->drawText(QPointF(rc.left, ybase), QString::fromUtf8(s, len));
}

void SurfaceImpl::SetClip(PRectangle rc)
{
  GetPainter()->setClipRect(QRectFFromPRect(rc));
}

static size_t utf8LengthFromLead(unsigned char uch)
{
  if (uch >= (0x80 + 0x40 + 0x20 + 0x10)) {
    return 4;
  } else if (uch >= (0x80 + 0x40 + 0x20)) {
    return 3;
  } else if (uch >= (0x80)) {
    return 2;
  } else {
    return 1;
  }
}

void SurfaceImpl::MeasureWidths(
  Font &font,
  const char *s,
  int len,
  XYPOSITION *positions)
{
  if (!font.GetID())
    return;
  QString su = QString::fromUtf8(s, len);
  QTextLayout tlay(su, *static_cast<QFont *>(font.GetID()), device);
  tlay.beginLayout();
  QTextLine tl = tlay.createLine();
  tlay.endLayout();

  int i = 0;
  int ui = 0;
  int fit = su.size();
  const unsigned char *us = reinterpret_cast<const unsigned char *>(s);
  while (ui < fit) {
    size_t lenChar = utf8LengthFromLead(us[i]);
    int codeUnits = (lenChar < 4) ? 1 : 2;
    qreal xPosition = tl.cursorToX(ui + codeUnits);
    for (unsigned int bytePos = 0; (bytePos < lenChar) && (i < len); ++bytePos)
      positions[i++] = xPosition;
    ui += codeUnits;
  }

  XYPOSITION lastPos = 0;
  if (i > 0)
    lastPos = positions[i - 1];
  while (i < len)
    positions[i++] = lastPos;
}

XYPOSITION SurfaceImpl::WidthText(Font &font, const char *s, int len)
{
  QFontMetricsF metrics(*static_cast<QFont *>(font.GetID()), device);
  return metrics.width(QString::fromUtf8(s, len));
}

XYPOSITION SurfaceImpl::WidthChar(Font &font, char ch)
{
  QFontMetricsF metrics(*static_cast<QFont *>(font.GetID()), device);
  return metrics.width(ch);
}

XYPOSITION SurfaceImpl::Ascent(Font &font)
{
  QFontMetricsF metrics(*static_cast<QFont *>(font.GetID()), device);
  return metrics.ascent();
}

XYPOSITION SurfaceImpl::Descent(Font &font)
{
  QFontMetricsF metrics(*static_cast<QFont *>(font.GetID()), device);
  // Qt returns 1 less than true descent
  // See: QFontEngineWin::descent which says:
  // ### we subtract 1 to even out the historical +1 in QFontMetrics's
  // ### height=asc+desc+1 equation. Fix in Qt5.
  return metrics.descent() + 1;
}

XYPOSITION SurfaceImpl::InternalLeading(Font &font)
{
  return 0;
}

XYPOSITION SurfaceImpl::ExternalLeading(Font &font)
{
  QFontMetricsF metrics(*static_cast<QFont *>(font.GetID()), device);
  return metrics.leading();
}

XYPOSITION SurfaceImpl::Height(Font &font)
{
  QFontMetricsF metrics(*static_cast<QFont *>(font.GetID()), device);
  return metrics.height();
}

XYPOSITION SurfaceImpl::AverageCharWidth(Font &font)
{
  QFontMetricsF metrics(*static_cast<QFont *>(font.GetID()), device);
  return metrics.averageCharWidth();
}

void SurfaceImpl::FlushCachedState()
{
  if (device->paintingActive()) {
    QPainter *painter = GetPainter();
    painter->setPen(QPen());
    painter->setBrush(QBrush());
  }
}

QPainter *SurfaceImpl::GetPainter()
{
  Q_ASSERT(device);

  if (!painter) {
    painter = new QPainter(device);

    // Set text antialiasing unconditionally.
    // The font's style strategy will override.
    painter->setRenderHint(QPainter::TextAntialiasing, true);
  }

  return painter;
}

Surface *Surface::Allocate(int)
{
  return new SurfaceImpl;
}


//----------------------------------------------------------------------

static QWidget *window(WindowID wid)
{
  return static_cast<QWidget *>(wid);
}

Window::~Window() {}

void Window::Destroy()
{
  if (wid)
    delete window(wid);

  wid = 0;
}

bool Window::HasFocus()
{
  return wid ? window(wid)->hasFocus() : false;
}

PRectangle Window::GetPosition()
{
  // Before any size allocated pretend its 1000 wide so not scrolled
  return wid ? PRectFromQRect(window(wid)->frameGeometry()) : PRectangle(0, 0, 1000, 1000);
}

void Window::SetPosition(PRectangle rc)
{
  if (wid)
    window(wid)->setGeometry(QRectFromPRect(rc));
}

void Window::SetPositionRelative(PRectangle rc, Window relativeTo)
{
  QPoint oPos = window(relativeTo.wid)->mapToGlobal(QPoint(0,0));
  int ox = oPos.x();
  int oy = oPos.y();
  ox += rc.left;
  oy += rc.top;

  QDesktopWidget *desktop = QApplication::desktop();
  QRect rectDesk = desktop->availableGeometry(QPoint(ox, oy));
  /* do some corrections to fit into screen */
  int sizex = rc.right - rc.left;
  int sizey = rc.bottom - rc.top;
  int screenWidth = rectDesk.width();
  if (ox < rectDesk.x())
    ox = rectDesk.x();
  if (sizex > screenWidth)
    ox = rectDesk.x(); /* the best we can do */
  else if (ox + sizex > rectDesk.right())
    ox = rectDesk.right() - sizex;
  if (oy + sizey > rectDesk.bottom())
    oy = rectDesk.bottom() - sizey;

  Q_ASSERT(wid);
  window(wid)->move(ox, oy);
  window(wid)->resize(sizex, sizey);
}

PRectangle Window::GetClientPosition()
{
  // The client position is the window position
  return GetPosition();
}

void Window::Show(bool show)
{
  if (wid)
    window(wid)->setVisible(show);
}

void Window::InvalidateAll()
{
  if (wid)
    window(wid)->update();
}

void Window::InvalidateRectangle(PRectangle rc)
{
  if (wid)
    window(wid)->update(QRectFromPRect(rc));
}

void Window::SetFont(Font &font)
{
  if (wid)
    window(wid)->setFont(*static_cast<QFont *>(font.GetID()));
}

void Window::SetCursor(Cursor curs)
{
  if (wid) {
    Qt::CursorShape shape;

    switch (curs) {
      case cursorText:  shape = Qt::IBeamCursor;        break;
      case cursorArrow: shape = Qt::ArrowCursor;        break;
      case cursorUp:    shape = Qt::UpArrowCursor;      break;
      case cursorWait:  shape = Qt::WaitCursor;         break;
      case cursorHoriz: shape = Qt::SizeHorCursor;      break;
      case cursorVert:  shape = Qt::SizeVerCursor;      break;
      case cursorHand:  shape = Qt::PointingHandCursor; break;
      default:          shape = Qt::ArrowCursor;        break;
    }

    QCursor cursor = QCursor(shape);

    if (curs != cursorLast) {
      window(wid)->setCursor(cursor);
      cursorLast = curs;
    }
  }
}

void Window::SetTitle(const char *s)
{
  if (wid)
    window(wid)->setWindowTitle(s);
}

/* Returns rectangle of monitor pt is on, both rect and pt are in Window's
   window coordinates */
PRectangle Window::GetMonitorRect(Point pt)
{
  QPoint originGlobal = window(wid)->mapToGlobal(QPoint(0, 0));
  QPoint posGlobal = window(wid)->mapToGlobal(QPoint(pt.x, pt.y));
  QDesktopWidget *desktop = QApplication::desktop();
  QRect rectScreen = desktop->availableGeometry(posGlobal);
  rectScreen.translate(-originGlobal.x(), -originGlobal.y());
  return PRectangle(rectScreen.left(), rectScreen.top(),
          rectScreen.right(), rectScreen.bottom());
}


//----------------------------------------------------------------------

class ListBoxImpl : public ListBox {
public:
  ListBoxImpl();
  ~ListBoxImpl();

  virtual void SetFont(Font &font);
  virtual void Create(Window &parent, int ctrlID, Point location,
            int lineHeight, bool unicodeMode, int technology);
  virtual void SetAverageCharWidth(int width);
  virtual void SetVisibleRows(int rows);
  virtual int GetVisibleRows() const;
  virtual PRectangle GetDesiredRect();
  virtual int CaretFromEdge();
  virtual void Clear();
  virtual void Append(char *s, int type = -1);
  virtual int Length();
  virtual void Select(int n);
  virtual int GetSelection();
  virtual int Find(const char *prefix);
  virtual void GetValue(int n, char *value, int len);
  virtual void RegisterImage(int type, const char *xpmData);
  virtual void RegisterRGBAImage(int type, int width, int height,
    const unsigned char *pixels);
  virtual void RegisterQPixmapImage(int type, const QPixmap& pm);
  virtual void ClearRegisteredImages();
  virtual void SetDoubleClickAction(CallBackAction action, void *data);
  virtual void SetList(const char *list, char separator, char typesep);
private:
  bool unicodeMode;
  int visibleRows;
  QMap<int,QPixmap> images;
};

class ListWidget : public QListWidget {
public:
  explicit ListWidget(QWidget *parent);
  virtual ~ListWidget();

  void setDoubleClickAction(CallBackAction action, void *data);

protected:
  virtual void mouseDoubleClickEvent(QMouseEvent *event);
  virtual QStyleOptionViewItem viewOptions() const;

private:
  CallBackAction doubleClickAction;
  void *doubleClickActionData;
};


ListBoxImpl::ListBoxImpl()
: unicodeMode(false), visibleRows(5)
{}

ListBoxImpl::~ListBoxImpl() {}

void ListBoxImpl::Create(
  Window &parent,
  int /*ctrlID*/,
  Point location,
  int /*lineHeight*/,
  bool unicodeMode_,
  int)
{
  unicodeMode = unicodeMode_;

  QWidget *qparent = static_cast<QWidget *>(parent.GetID());
  ListWidget *list = new ListWidget(qparent);

#if defined(Q_OS_WIN)
  // On Windows, Qt::ToolTip causes a crash when the list is clicked on
  // so Qt::Tool is used.
  list->setParent(0, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
#else
  // On OS X, Qt::Tool takes focus so main window loses focus so
  // keyboard stops working. Qt::ToolTip works but its only really
  // documented for tooltips.
  // On Linux / X this setting allows clicking on list items.
  list->setParent(0, Qt::ToolTip | Qt::FramelessWindowHint);
#endif
  list->setAttribute(Qt::WA_ShowWithoutActivating);
  list->setFocusPolicy(Qt::NoFocus);
  list->setUniformItemSizes(true);
  list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  list->move(location.x, location.y);

  int maxIconWidth = 0;
  int maxIconHeight = 0;
  foreach (QPixmap im, images) {
    if (maxIconWidth < im.width())
      maxIconWidth = im.width();
    if (maxIconHeight < im.height())
      maxIconHeight = im.height();
  }
  list->setIconSize(QSize(maxIconWidth, maxIconHeight));

  wid = list;
}

void ListBoxImpl::SetFont(Font &font)
{
  ListWidget *list = static_cast<ListWidget *>(wid);
  list->setFont(*static_cast<QFont *>(font.GetID()));
}

void ListBoxImpl::SetAverageCharWidth(int /*width*/) {}

void ListBoxImpl::SetVisibleRows(int rows)
{
  visibleRows = rows;
}

int ListBoxImpl::GetVisibleRows() const
{
  return visibleRows;
}

PRectangle ListBoxImpl::GetDesiredRect()
{
  ListWidget *list = static_cast<ListWidget *>(wid);

  int rows = Length();
  if (rows == 0 || rows > visibleRows) {
    rows = visibleRows;
  }
  int rowHeight = list->sizeHintForRow(0);
  int height = (rows * rowHeight) + (2 * list->frameWidth());

  QStyle *style = QApplication::style();
  int width = list->sizeHintForColumn(0) + (2 * list->frameWidth());
  if (Length() > rows) {
    width += style->pixelMetric(QStyle::PM_ScrollBarExtent);
  }

  return PRectangle(0, 0, width, height);
}

int ListBoxImpl::CaretFromEdge()
{
  ListWidget *list = static_cast<ListWidget *>(wid);

  int maxIconWidth = 0;
  foreach (QPixmap im, images) {
    if (maxIconWidth < im.width())
      maxIconWidth = im.width();
  }

  int extra;
  // The 12 is from trial and error on OS X and the 7
  // is from trial and error on Windows - there may be
  // a better programmatic way to find any padding factors.
#ifdef Q_OS_DARWIN
  extra = 12;
#else
  extra = 7;
#endif
  return maxIconWidth + (2 * list->frameWidth()) + extra;
}

void ListBoxImpl::Clear()
{
  ListWidget *list = static_cast<ListWidget *>(wid);
  list->clear();
}

void ListBoxImpl::Append(char *s, int type)
{
  ListWidget *list = static_cast<ListWidget *>(wid);

  QString str = unicodeMode ? QString::fromUtf8(s) : QString::fromLocal8Bit(s);
  QIcon icon;
  if (type >= 0) {
    Q_ASSERT(images.contains(type));
    icon = images.value(type);
  }
  new QListWidgetItem(icon, str, list);
}

int ListBoxImpl::Length()
{
  ListWidget *list = static_cast<ListWidget *>(wid);
  return list->count();
}

void ListBoxImpl::Select(int n)
{
  ListWidget *list = static_cast<ListWidget *>(wid);
  QModelIndex index = list->model()->index(n, 0);
  if (index.isValid()) {
    QRect row_rect = list->visualRect(index);
    if (!list->viewport()->rect().contains(row_rect)) {
      list->scrollTo(index, QAbstractItemView::PositionAtTop);
    }
  }
  list->setCurrentRow(n);
}

int ListBoxImpl::GetSelection()
{
  ListWidget *list = static_cast<ListWidget *>(wid);
  return list->currentRow();
}

int ListBoxImpl::Find(const char *prefix)
{
  ListWidget *list = static_cast<ListWidget *>(wid);
  QString sPrefix = unicodeMode ? QString::fromUtf8(prefix) : QString::fromLocal8Bit(prefix);
  QList<QListWidgetItem *> ms = list->findItems(sPrefix, Qt::MatchStartsWith);

  int result = -1;
  if (!ms.isEmpty()) {
    result = list->row(ms.first());
  }

  return result;
}

void ListBoxImpl::GetValue(int n, char *value, int len)
{
  ListWidget *list = static_cast<ListWidget *>(wid);
  QListWidgetItem *item = list->item(n);
  QString str = item->data(Qt::DisplayRole).toString();
  QByteArray bytes = unicodeMode ? str.toUtf8() : str.toLocal8Bit();

  strncpy(value, bytes.constData(), len);
  value[len-1] = '\0';
}

void ListBoxImpl::RegisterQPixmapImage(int type, const QPixmap& pm)
{
  images[type] = pm;

  ListWidget *list = static_cast<ListWidget *>(wid);
  if (list != NULL) {
    QSize iconSize = list->iconSize();
    if (pm.width() > iconSize.width() || pm.height() > iconSize.height())
      list->setIconSize(QSize(qMax(pm.width(), iconSize.width()), 
             qMax(pm.height(), iconSize.height())));
  }

}

void ListBoxImpl::RegisterImage(int type, const char *xpmData)
{
  RegisterQPixmapImage(type, QPixmap(reinterpret_cast<const char * const *>(xpmData)));
}

void ListBoxImpl::RegisterRGBAImage(
  int type,
  int width,
  int height,
  const unsigned char *pixels)
{
  QImage image(pixels, width, height, QImage::Format_ARGB32_Premultiplied);
  RegisterQPixmapImage(type, QPixmap::fromImage(image));
}

void ListBoxImpl::ClearRegisteredImages()
{
  images.clear();
  
  ListWidget *list = static_cast<ListWidget *>(wid);
  if (list != NULL)
    list->setIconSize(QSize(0, 0));
}

void ListBoxImpl::SetDoubleClickAction(CallBackAction action, void *data)
{
  ListWidget *list = static_cast<ListWidget *>(wid);
  list->setDoubleClickAction(action, data);
}

void ListBoxImpl::SetList(const char *list, char separator, char typesep)
{
  // This method is *not* platform dependent.
  // It is borrowed from the GTK implementation.
  Clear();
  size_t count = strlen(list) + 1;
  std::vector<char> words(list, list+count);
  char *startword = &words[0];
  char *numword = NULL;
  int i = 0;
  for (; words[i]; i++) {
    if (words[i] == separator) {
      words[i] = '\0';
      if (numword)
        *numword = '\0';
      Append(startword, numword?atoi(numword + 1):-1);
      startword = &words[0] + i + 1;
      numword = NULL;
    } else if (words[i] == typesep) {
      numword = &words[0] + i;
    }
  }
  if (startword) {
    if (numword)
      *numword = '\0';
    Append(startword, numword?atoi(numword + 1):-1);
  }
}

ListBox::ListBox() {}

ListBox::~ListBox() {}

ListBox *ListBox::Allocate()
{
  return new ListBoxImpl;
}

ListWidget::ListWidget(QWidget *parent)
: QListWidget(parent), doubleClickAction(0), doubleClickActionData(0)
{}

ListWidget::~ListWidget() {}

void ListWidget::setDoubleClickAction(CallBackAction action, void *data)
{
  doubleClickAction = action;
  doubleClickActionData = data;
}

void ListWidget::mouseDoubleClickEvent(QMouseEvent * /* event */)
{
  if (doubleClickAction != 0) {
    doubleClickAction(doubleClickActionData);
  }
}

QStyleOptionViewItem ListWidget::viewOptions() const
{
  QStyleOptionViewItem result = QListWidget::viewOptions();
  result.state |= QStyle::State_Active;
  return result;
}

//----------------------------------------------------------------------

Menu::Menu() : mid(0) {}

void Menu::CreatePopUp()
{
  Destroy();
  mid = new QMenu;
}

void Menu::Destroy()
{
  if (mid) {
    QMenu *menu = static_cast<QMenu *>(mid);
    delete menu;
  }
  mid = 0;
}

void Menu::Show(Point pt, Window & /*w*/)
{
  QMenu *menu = static_cast<QMenu *>(mid);
  menu->exec(QPoint(pt.x, pt.y));
  Destroy();
}

//----------------------------------------------------------------------

class DynamicLibraryImpl : public DynamicLibrary {
protected:
  QLibrary *lib;
public:
  explicit DynamicLibraryImpl(const char *modulePath) {
    QString path = QString::fromUtf8(modulePath);
    lib = new QLibrary(path);
  }

  virtual ~DynamicLibraryImpl() {
    if (lib)
      lib->unload();
    lib = 0;
  }

  virtual Function FindFunction(const char *name) {
    return lib ? reinterpret_cast<Function>(lib->resolve(name)) : NULL;
  }

  virtual bool IsValid() {
    return lib != NULL;
  }
};

DynamicLibrary *DynamicLibrary::Load(const char *modulePath)
{
  return static_cast<DynamicLibrary *>(new DynamicLibraryImpl(modulePath));
}

ColourDesired Platform::Chrome()
{
  QColor c(Qt::gray);
  return ColourDesired(c.red(), c.green(), c.blue());
}

ColourDesired Platform::ChromeHighlight()
{
  QColor c(Qt::lightGray);
  return ColourDesired(c.red(), c.green(), c.blue());
}

const char *Platform::DefaultFont()
{
  static char fontNameDefault[200] = "";
  if (!fontNameDefault[0]) {
    QFont font = QApplication::font();
    strcpy(fontNameDefault, font.family().toUtf8());
  }
  return fontNameDefault;
}

int Platform::DefaultFontSize()
{
  QFont font = QApplication::font();
  return font.pointSize();
}

unsigned int Platform::DoubleClickTime()
{
  return QApplication::doubleClickInterval();
}

bool Platform::MouseButtonBounce()
{
  return false;
}

int Platform::Minimum(int a, int b)
{
  return qMin(a, b);
}

int Platform::Maximum(int a, int b)
{
  return qMax(a, b);
}

int Platform::Clamp(int val, int minVal, int maxVal)
{
  return qBound(minVal, val, maxVal);
}

void Platform::DebugDisplay(const char *s)
{
  qWarning("Scintilla: %s", s);
}

void Platform::DebugPrintf(const char *format, ...)
{
  char buffer[2000];
  va_list pArguments;
  va_start(pArguments, format);
  vsprintf(buffer, format, pArguments);
  va_end(pArguments);
  Platform::DebugDisplay(buffer);
}

void Platform::Assert(const char *c, const char *file, int line)
{
  char buffer[2000];
  sprintf(buffer, "Assertion [%s] failed at %s %d\n", c, file, line);
  Platform::DebugDisplay(buffer);
}

#ifdef SCI_NAMESPACE
}
#endif