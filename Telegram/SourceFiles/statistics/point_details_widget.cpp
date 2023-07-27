/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "statistics/point_details_widget.h"

#include "ui/cached_round_corners.h"
#include "ui/rect.h"
#include "ui/widgets/shadow.h"
#include "styles/style_layers.h"
#include "styles/style_statistics.h"

namespace Statistic {
namespace {

[[nodiscard]] QString FormatTimestamp(
		float64 timestamp,
		const QString &longFormat,
		const QString &shortFormat) {
	const auto dateTime = QDateTime::fromSecsSinceEpoch(timestamp / 1000);
	if (dateTime.toUTC().time().hour() || dateTime.toUTC().time().minute()) {
		return QLocale().toString(dateTime, longFormat);
	} else {
		return QLocale().toString(dateTime.date(), shortFormat);
	}
}

} // namespace

PointDetailsWidget::PointDetailsWidget(
	not_null<Ui::RpWidget*> parent,
	const Data::StatisticalChart &chartData,
	float64 maxAbsoluteValue,
	bool zoomEnabled)
: Ui::AbstractButton(parent)
, _zoomEnabled(zoomEnabled)
, _chartData(chartData)
, _textStyle(st::statisticsDetailsPopupStyle)
, _headerStyle(st::semiboldTextStyle)
, _longFormat(u"ddd, MMM d hh:mm"_q)
, _shortFormat(u"ddd, MMM d"_q) {
	const auto calculatedWidth = [&]{
		const auto maxValueText = Ui::Text::String(
			_textStyle,
			QString("%L1").arg(maxAbsoluteValue));
		const auto maxValueTextWidth = maxValueText.maxWidth();

		auto maxNameTextWidth = 0;
		for (const auto &dataLine : _chartData.lines) {
			const auto maxNameText = Ui::Text::String(
				_textStyle,
				dataLine.name);
			maxNameTextWidth = std::max(
				maxNameText.maxWidth(),
				maxNameTextWidth);
		}
		{
			const auto maxHeaderText = Ui::Text::String(
				_headerStyle,
				FormatTimestamp(
					_chartData.x.front(),
					_longFormat,
					_shortFormat));
			maxNameTextWidth = std::max(
				maxHeaderText.maxWidth()
					+ st::statisticsDetailsPopupPadding.left(),
				maxNameTextWidth);
		}
		return maxValueTextWidth
			+ rect::m::sum::h(st::statisticsDetailsPopupMargins)
			+ rect::m::sum::h(st::statisticsDetailsPopupPadding)
			+ st::statisticsDetailsPopupPadding.left() // Between strings.
			+ maxNameTextWidth;
	}();
	sizeValue(
	) | rpl::start_with_next([=](const QSize &s) {
		const auto fullRect = s.isNull()
			? Rect(Size(calculatedWidth))
			: Rect(s);
		_innerRect = fullRect - st::statisticsDetailsPopupPadding;
		_textRect = _innerRect - st::statisticsDetailsPopupMargins;
	}, lifetime());

	resize(calculatedWidth, height());
	resizeHeight();
}

void PointDetailsWidget::setLineAlpha(int lineId, float64 alpha) {
	for (auto &line : _lines) {
		if (line.id == lineId) {
			line.alpha = alpha;
		}
	}
	update();
	resizeHeight();
}

void PointDetailsWidget::resizeHeight() {
	resize(
		width(),
		lineYAt(_chartData.lines.size())
			+ st::statisticsDetailsPopupMargins.bottom());
}

int PointDetailsWidget::xIndex() const {
	return _xIndex;
}

void PointDetailsWidget::setXIndex(int xIndex) {
	_xIndex = xIndex;
	if (xIndex < 0) {
		return;
	}
	_header.setText(
		_headerStyle,
		FormatTimestamp(_chartData.x[xIndex], _longFormat, _shortFormat));

	_lines.clear();
	_lines.reserve(_chartData.lines.size());
	auto hasPositiveValues = false;
	for (const auto &dataLine : _chartData.lines) {
		auto textLine = Line();
		textLine.id = dataLine.id;
		textLine.name.setText(_textStyle, dataLine.name);
		textLine.value.setText(
			_textStyle,
			QString("%L1").arg(dataLine.y[xIndex]));
		hasPositiveValues |= (dataLine.y[xIndex] > 0);
		textLine.valueColor = QColor(dataLine.color);
		_lines.push_back(std::move(textLine));
	}
	const auto clickable = _zoomEnabled && hasPositiveValues;
	setAttribute(
		Qt::WA_TransparentForMouseEvents,
		!clickable);
}

void PointDetailsWidget::setAlpha(float64 alpha) {
	_alpha = alpha;
	update();
}

int PointDetailsWidget::lineYAt(int index) const {
	auto linesHeight = 0.;
	for (auto i = 0; i < index; i++) {
		const auto alpha = (i >= _lines.size()) ? 1. : _lines[i].alpha;
		linesHeight += alpha
			* (_textStyle.font->height
				+ st::statisticsDetailsPopupMidLineSpace);
	}

	return _textRect.y()
		+ _headerStyle.font->height
		+ st::statisticsDetailsPopupMargins.bottom()
		+ std::ceil(linesHeight);
}

void PointDetailsWidget::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);

	p.setOpacity(_alpha);

	const auto fullRect = rect();

	Ui::Shadow::paint(p, _innerRect, width(), st::boxRoundShadow);
	Ui::FillRoundRect(p, _innerRect, st::boxBg, Ui::BoxCorners);

	p.setPen(st::boxTextFg);
	const auto headerContext = Ui::Text::PaintContext{
		.position = _textRect.topLeft(),
		.availableWidth = _textRect.width(),
	};
	_header.draw(p, headerContext);
	for (auto i = 0; i < _lines.size(); i++) {
		const auto &line = _lines[i];
		const auto lineY = lineYAt(i);
		const auto valueWidth = line.value.maxWidth();
		const auto valueContext = Ui::Text::PaintContext{
			.position = QPoint(rect::right(_textRect) - valueWidth, lineY),
		};
		const auto nameContext = Ui::Text::PaintContext{
			.position = QPoint(_textRect.x(), lineY),
			.outerWidth = _textRect.width() - valueWidth,
			.availableWidth = _textRect.width(),
		};
		p.setOpacity(line.alpha * line.alpha);
		p.setPen(st::boxTextFg);
		line.name.draw(p, nameContext);
		p.setPen(line.valueColor);
		line.value.draw(p, valueContext);
	}
}

} // namespace Statistic
