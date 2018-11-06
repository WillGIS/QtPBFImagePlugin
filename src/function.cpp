#include <cmath>
#include <QJsonObject>
#include <QJsonArray>
#include "color.h"
#include "function.h"


#define f(y0, y1, ratio) (y0 * (1.0 - ratio)) + (y1 * ratio)

static qreal interpolate(const QPointF &p0, const QPointF &p1, qreal base,
  qreal x)
{
	qreal difference = p1.x() - p0.x();
	if (difference < 1e-6)
		return p0.y();

	qreal progress = x - p0.x();
	qreal ratio = (base == 1.0)
	  ? progress / difference
	  : (pow(base, progress) - 1) / (pow(base, difference) - 1);

	return f(p0.y(), p1.y(), ratio);
}

static QColor interpolate(const QPair<qreal, QColor> &p0,
  const QPair<qreal, QColor> &p1, qreal base, qreal x)
{
	qreal difference = p1.first - p0.first;
	if (difference < 1e-6)
		return p0.second;

	qreal progress = x - p0.first;
	qreal ratio = (base == 1.0)
	  ? progress / difference
	  : (pow(base, progress) - 1) / (pow(base, difference) - 1);


	qreal p0h, p0s, p0l, p0a;
	p0.second.getHslF(&p0h, &p0s, &p0l, &p0a);
	qreal p1h, p1s, p1l, p1a;
	p1.second.getHslF(&p1h, &p1s, &p1l, &p1a);

	return QColor::fromHslF(f(p0h, p1h, ratio), f(p0s, p1s, ratio),
	  f(p0l, p1l, ratio), f(p0a, p1a, ratio));
}

FunctionF::FunctionF(const QJsonObject &json, qreal dflt)
  : _default(dflt), _base(1.0)
{
	if (!(json.contains("stops") && json["stops"].isArray()))
		return;

	QJsonArray stops = json["stops"].toArray();
	for (int i = 0; i < stops.size(); i++) {
		if (!stops.at(i).isArray())
			return;
		QJsonArray stop = stops.at(i).toArray();
		if (stop.size() != 2)
			return;
		_stops.append(QPointF(stop.at(0).toDouble(), stop.at(1).toDouble()));
	}

	if (json.contains("base") && json["base"].isDouble())
		_base = json["base"].toDouble();
}

qreal FunctionF::value(qreal x) const
{
	if (_stops.isEmpty())
		return _default;

	QPointF v0(_stops.first());
	for (int i = 0; i < _stops.size(); i++) {
		if (x < _stops.at(i).x())
			return interpolate(v0, _stops.at(i), _base, x);
		else
			v0 = _stops.at(i);
	}

	return _stops.last().y();
}


FunctionC::FunctionC(const QJsonObject &json, const QColor &dflt)
  : _default(dflt), _base(1.0)
{
	if (!(json.contains("stops") && json["stops"].isArray()))
		return;

	QJsonArray stops = json["stops"].toArray();
	for (int i = 0; i < stops.size(); i++) {
		if (!stops.at(i).isArray())
			return;
		QJsonArray stop = stops.at(i).toArray();
		if (stop.size() != 2)
			return;
		_stops.append(QPair<qreal, QColor>(stop.at(0).toDouble(),
		  Color::fromJsonString(stop.at(1).toString())));
	}

	if (json.contains("base") && json["base"].isDouble())
		_base = json["base"].toDouble();
}

QColor FunctionC::value(qreal x) const
{
	if (_stops.isEmpty())
		return _default;

	QPair<qreal, QColor> v0(_stops.first());
	for (int i = 0; i < _stops.size(); i++) {
		if (x < _stops.at(i).first)
			return interpolate(v0, _stops.at(i), _base, x);
		else
			v0 = _stops.at(i);
	}

	return _stops.last().second;
}

FunctionB::FunctionB(const QJsonObject &json, bool dflt) : _default(dflt)
{
	if (!(json.contains("stops") && json["stops"].isArray()))
		return;

	QJsonArray stops = json["stops"].toArray();
	for (int i = 0; i < stops.size(); i++) {
		if (!stops.at(i).isArray())
			return;
		QJsonArray stop = stops.at(i).toArray();
		if (stop.size() != 2)
			return;
		_stops.append(QPair<qreal, bool>(stop.at(0).toDouble(),
		  stop.at(1).toBool()));
	}
}

bool FunctionB::value(qreal x) const
{
	if (_stops.isEmpty())
		return _default;

	QPair<qreal, bool> v0(_stops.first());
	for (int i = 0; i < _stops.size(); i++) {
		if (x < _stops.at(i).first)
			return v0.second;
		else
			v0 = _stops.at(i);
	}

	return _stops.last().second;
}
