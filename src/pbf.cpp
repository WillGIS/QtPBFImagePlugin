#include <QByteArray>
#include <QPainter>
#include <QDebug>
#include <QVariantMap>
#include "vector_tile.pb.h"
#include "style.h"
#include "tile.h"
#include "pbf.h"


#define MOVE_TO    1
#define LINE_TO    2
#define CLOSE_PATH 7

#define POLYGON vector_tile::Tile_GeomType::Tile_GeomType_POLYGON
#define LINESTRING vector_tile::Tile_GeomType::Tile_GeomType_LINESTRING
#define POINT vector_tile::Tile_GeomType::Tile_GeomType_POINT

static QVariant value(const vector_tile::Tile_Value &val)
{
	if (val.has_bool_value())
		return QVariant(val.bool_value());
	else if (val.has_int_value())
		return QVariant((qlonglong)val.int_value());
	else if (val.has_sint_value())
		return QVariant((qlonglong)val.sint_value());
	else if (val.has_uint_value())
		return QVariant((qulonglong)val.uint_value());
	else if (val.has_float_value())
		return QVariant(val.float_value());
	else if (val.has_double_value())
		return QVariant(val.double_value());
	else if (val.has_string_value())
		return QVariant(QString::fromStdString(val.string_value()));
	else
		return QVariant();
}

class Feature
{
public:
	Feature(const vector_tile::Tile_Feature *data, const QVector<QString> *keys,
	  const QVector<QVariant> *values) : _data(data)
	{
		for (int i = 0; i < data->tags_size(); i = i + 2)
			_tags.insert(keys->at(data->tags(i)), values->at(data->tags(i+1)));

		switch (data->type()) {
			case POLYGON:
				_tags.insert("$type", QVariant("Polygon"));
				break;
			case LINESTRING:
				_tags.insert("$type", QVariant("LineString"));
				break;
			case POINT:
				_tags.insert("$type", QVariant("Point"));
				break;
			default:
				break;
		}
	}

	const QVariantMap &tags() const {return _tags;}
	const vector_tile::Tile_Feature *data() const {return _data;}

private:
	QVariantMap _tags;
	const vector_tile::Tile_Feature *_data;
};

class Layer
{
public:
	Layer(const vector_tile::Tile_Layer *data) : _data(data)
	{
		QVector<QString> keys;
		QVector<QVariant> values;

		for (int i = 0; i < data->keys_size(); i++)
			keys.append(QString::fromStdString(data->keys(i)));
		for (int i = 0; i < data->values_size(); i++)
			values.append(value(data->values(i)));

		for (int i = 0; i < data->features_size(); i++)
			_features.append(Feature(&(data->features(i)), &keys, &values));
	}

	const QList<Feature> &features() const {return _features;}
	const vector_tile::Tile_Layer *data() const {return _data;}

private:
	const vector_tile::Tile_Layer *_data;
	QList<Feature> _features;
};

static QPoint parameters(quint32 v1, quint32 v2)
{
	return QPoint((v1 >> 1) ^ (-(v1 & 1)), ((v2 >> 1) ^ (-(v2 & 1))));
}

static void drawFeature(const Feature &feature, Style *style, int styleLayer,
  qreal factor, Tile &tile)
{
	if (!style->match(styleLayer, feature.tags()))
		return;

	QPoint cursor;
	QPainterPath path;

	for (int i = 0; i < feature.data()->geometry_size(); i++) {
		quint32 g = feature.data()->geometry(i);
		unsigned cmdId = g & 0x7;
		unsigned cmdCount = g >> 3;

		if (cmdId == MOVE_TO) {
			for (unsigned j = 0; j < cmdCount; j++) {
				QPoint offset = parameters(feature.data()->geometry(i+1),
				  feature.data()->geometry(i+2));
				i += 2;
				cursor += offset;
				path.moveTo(QPointF(cursor) / factor);
			}
		} else if (cmdId == LINE_TO) {
			for (unsigned j = 0; j < cmdCount; j++) {
				QPoint offset = parameters(feature.data()->geometry(i+1),
				  feature.data()->geometry(i+2));
				i += 2;
				cursor += offset;
				path.lineTo(QPointF(cursor) / factor);
			}
		} else if (cmdId == CLOSE_PATH) {
			path.closeSubpath();
			path.moveTo(cursor);
		}
	}

	style->drawFeature(styleLayer, path, feature.tags(), tile);
}

static void drawLayer(const Layer &layer, Style *style, int styleLayer,
  Tile &tile)
{
	if (layer.data()->version() > 2)
		return;

	qreal factor = layer.data()->extent() / (qreal)tile.size();

	for (int i = 0; i < layer.features().size(); i++)
		drawFeature(layer.features().at(i), style, styleLayer, factor, tile);
}

QImage PBF::image(const QByteArray &data, int zoom, Style *style, int size)
{
	vector_tile::Tile tile;
	if (!tile.ParseFromArray(data.constData(), data.size())) {
		qCritical() << "Invalid tile protocol buffer data";
		return QImage();
	}

	Tile t(size);

	style->setZoom(zoom);
	style->drawBackground(t);

	QMap<QString, Layer> layers;
	for (int i = 0; i <  tile.layers_size(); i++) {
		const vector_tile::Tile_Layer &layer = tile.layers(i);
		QString name(QString::fromStdString(layer.name()));
		if (style->sourceLayers().contains(name))
			layers.insert(name, Layer(&layer));
	}

	// Process data in order of style layers
	for (int i = 0; i < style->sourceLayers().size(); i++) {
		QMap<QString, Layer>::const_iterator it = layers.find(
		  style->sourceLayers().at(i));
		if (it == layers.constEnd())
			continue;

		drawLayer(*it, style, i, t);
	}

	return t.render();
}
