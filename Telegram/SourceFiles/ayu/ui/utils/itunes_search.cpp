// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/utils/itunes_search.h"

#include <QtCore/QCache>
#include <QtCore/QEventLoop>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QTimer>
#include <QtCore/QUrlQuery>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Ayu::Ui::Itunes {
namespace {

struct CacheEntry
{
	QPixmap pix;
};

QCache<QString, CacheEntry> &cache() {
	static QCache<QString, CacheEntry> c(50);
	return c;
}

QString translitSafe(const QString &s) {
	static const QHash<QChar, QString> trMap = []
	{
		QHash<QChar, QString> m;
		m.reserve(487); // 488
		m.insert(QChar(u'ȼ'), QStringLiteral("c"));
		m.insert(QChar(u'ᶇ'), QStringLiteral("n"));
		m.insert(QChar(u'ɖ'), QStringLiteral("d"));
		m.insert(QChar(u'ỿ'), QStringLiteral("y"));
		m.insert(QChar(u'ᴓ'), QStringLiteral("o"));
		m.insert(QChar(u'ø'), QStringLiteral("o"));
		m.insert(QChar(u'ḁ'), QStringLiteral("a"));
		m.insert(QChar(u'ʯ'), QStringLiteral("h"));
		m.insert(QChar(u'ŷ'), QStringLiteral("y"));
		m.insert(QChar(u'ʞ'), QStringLiteral("k"));
		m.insert(QChar(u'ừ'), QStringLiteral("u"));
		m.insert(QChar(u'ꜳ'), QStringLiteral("aa"));
		m.insert(QChar(u'ĳ'), QStringLiteral("ij"));
		m.insert(QChar(u'ḽ'), QStringLiteral("l"));
		m.insert(QChar(u'ɪ'), QStringLiteral("i"));
		m.insert(QChar(u'ḇ'), QStringLiteral("b"));
		m.insert(QChar(u'ʀ'), QStringLiteral("r"));
		m.insert(QChar(u'ě'), QStringLiteral("e"));
		m.insert(QChar(u'ﬃ'), QStringLiteral("ffi"));
		m.insert(QChar(u'ơ'), QStringLiteral("o"));
		m.insert(QChar(u'ⱹ'), QStringLiteral("r"));
		m.insert(QChar(u'ồ'), QStringLiteral("o"));
		m.insert(QChar(u'ǐ'), QStringLiteral("i"));
		m.insert(QChar(u'ꝕ'), QStringLiteral("p"));
		m.insert(QChar(u'ý'), QStringLiteral("y"));
		m.insert(QChar(u'ḝ'), QStringLiteral("e"));
		m.insert(QChar(u'ₒ'), QStringLiteral("o"));
		m.insert(QChar(u'ⱥ'), QStringLiteral("a"));
		m.insert(QChar(u'ʙ'), QStringLiteral("b"));
		m.insert(QChar(u'ḛ'), QStringLiteral("e"));
		m.insert(QChar(u'ƈ'), QStringLiteral("c"));
		m.insert(QChar(u'ɦ'), QStringLiteral("h"));
		m.insert(QChar(u'ᵬ'), QStringLiteral("b"));
		m.insert(QChar(u'ṣ'), QStringLiteral("s"));
		m.insert(QChar(u'đ'), QStringLiteral("d"));
		m.insert(QChar(u'ỗ'), QStringLiteral("o"));
		m.insert(QChar(u'ɟ'), QStringLiteral("j"));
		m.insert(QChar(u'ẚ'), QStringLiteral("a"));
		m.insert(QChar(u'ɏ'), QStringLiteral("y"));
		m.insert(QChar(u'ʌ'), QStringLiteral("v"));
		m.insert(QChar(u'ꝓ'), QStringLiteral("p"));
		m.insert(QChar(u'ﬁ'), QStringLiteral("fi"));
		m.insert(QChar(u'ᶄ'), QStringLiteral("k"));
		m.insert(QChar(u'ḏ'), QStringLiteral("d"));
		m.insert(QChar(u'ᴌ'), QStringLiteral("l"));
		m.insert(QChar(u'ė'), QStringLiteral("e"));
		m.insert(QChar(u'ᴋ'), QStringLiteral("k"));
		m.insert(QChar(u'ċ'), QStringLiteral("c"));
		m.insert(QChar(u'ʁ'), QStringLiteral("r"));
		m.insert(QChar(u'ƕ'), QStringLiteral("hv"));
		m.insert(QChar(u'ƀ'), QStringLiteral("b"));
		m.insert(QChar(u'ṍ'), QStringLiteral("o"));
		m.insert(QChar(u'ȣ'), QStringLiteral("ou"));
		m.insert(QChar(u'ǰ'), QStringLiteral("j"));
		m.insert(QChar(u'ᶃ'), QStringLiteral("g"));
		m.insert(QChar(u'ṋ'), QStringLiteral("n"));
		m.insert(QChar(u'ɉ'), QStringLiteral("j"));
		m.insert(QChar(u'ǧ'), QStringLiteral("g"));
		m.insert(QChar(u'ǳ'), QStringLiteral("dz"));
		m.insert(QChar(u'ź'), QStringLiteral("z"));
		m.insert(QChar(u'ꜷ'), QStringLiteral("au"));
		m.insert(QChar(u'ǖ'), QStringLiteral("u"));
		m.insert(QChar(u'ᵹ'), QStringLiteral("g"));
		m.insert(QChar(u'ȯ'), QStringLiteral("o"));
		m.insert(QChar(u'ɐ'), QStringLiteral("a"));
		m.insert(QChar(u'ą'), QStringLiteral("a"));
		m.insert(QChar(u'õ'), QStringLiteral("o"));
		m.insert(QChar(u'ɻ'), QStringLiteral("r"));
		m.insert(QChar(u'ꝍ'), QStringLiteral("o"));
		m.insert(QChar(u'ǟ'), QStringLiteral("a"));
		m.insert(QChar(u'ȴ'), QStringLiteral("l"));
		m.insert(QChar(u'ʂ'), QStringLiteral("s"));
		m.insert(QChar(u'ﬂ'), QStringLiteral("fl"));
		m.insert(QChar(u'ȉ'), QStringLiteral("i"));
		m.insert(QChar(u'ⱻ'), QStringLiteral("e"));
		m.insert(QChar(u'ṉ'), QStringLiteral("n"));
		m.insert(QChar(u'ï'), QStringLiteral("i"));
		m.insert(QChar(u'ñ'), QStringLiteral("n"));
		m.insert(QChar(u'ᴉ'), QStringLiteral("i"));
		m.insert(QChar(u'ʇ'), QStringLiteral("t"));
		m.insert(QChar(u'ẓ'), QStringLiteral("z"));
		m.insert(QChar(u'ỷ'), QStringLiteral("y"));
		m.insert(QChar(u'ȳ'), QStringLiteral("y"));
		m.insert(QChar(u'ṩ'), QStringLiteral("s"));
		m.insert(QChar(u'ɽ'), QStringLiteral("r"));
		m.insert(QChar(u'ĝ'), QStringLiteral("g"));
		m.insert(QChar(u'ᴝ'), QStringLiteral("u"));
		m.insert(QChar(u'ḳ'), QStringLiteral("k"));
		m.insert(QChar(u'ꝫ'), QStringLiteral("et"));
		m.insert(QChar(u'ī'), QStringLiteral("i"));
		m.insert(QChar(u'ť'), QStringLiteral("t"));
		m.insert(QChar(u'ꜿ'), QStringLiteral("c"));
		m.insert(QChar(u'ʟ'), QStringLiteral("l"));
		m.insert(QChar(u'ꜹ'), QStringLiteral("av"));
		m.insert(QChar(u'û'), QStringLiteral("u"));
		m.insert(QChar(u'æ'), QStringLiteral("ae"));
		m.insert(QChar(u'ă'), QStringLiteral("a"));
		m.insert(QChar(u'ǘ'), QStringLiteral("u"));
		m.insert(QChar(u'ꞅ'), QStringLiteral("s"));
		m.insert(QChar(u'ᵣ'), QStringLiteral("r"));
		m.insert(QChar(u'ᴀ'), QStringLiteral("a"));
		m.insert(QChar(u'ƃ'), QStringLiteral("b"));
		m.insert(QChar(u'ḩ'), QStringLiteral("h"));
		m.insert(QChar(u'ṧ'), QStringLiteral("s"));
		m.insert(QChar(u'ₑ'), QStringLiteral("e"));
		m.insert(QChar(u'ʜ'), QStringLiteral("h"));
		m.insert(QChar(u'ẋ'), QStringLiteral("x"));
		m.insert(QChar(u'ꝅ'), QStringLiteral("k"));
		m.insert(QChar(u'ḋ'), QStringLiteral("d"));
		m.insert(QChar(u'ƣ'), QStringLiteral("oi"));
		m.insert(QChar(u'ꝑ'), QStringLiteral("p"));
		m.insert(QChar(u'ħ'), QStringLiteral("h"));
		m.insert(QChar(u'ⱴ'), QStringLiteral("v"));
		m.insert(QChar(u'ẇ'), QStringLiteral("w"));
		m.insert(QChar(u'ǹ'), QStringLiteral("n"));
		m.insert(QChar(u'ɯ'), QStringLiteral("m"));
		m.insert(QChar(u'ɡ'), QStringLiteral("g"));
		m.insert(QChar(u'ɴ'), QStringLiteral("n"));
		m.insert(QChar(u'ᴘ'), QStringLiteral("p"));
		m.insert(QChar(u'ᵥ'), QStringLiteral("v"));
		m.insert(QChar(u'ū'), QStringLiteral("u"));
		m.insert(QChar(u'ḃ'), QStringLiteral("b"));
		m.insert(QChar(u'ṗ'), QStringLiteral("p"));
		m.insert(QChar(u'å'), QStringLiteral("a"));
		m.insert(QChar(u'ɕ'), QStringLiteral("c"));
		m.insert(QChar(u'ọ'), QStringLiteral("o"));
		m.insert(QChar(u'ắ'), QStringLiteral("a"));
		m.insert(QChar(u'ƒ'), QStringLiteral("f"));
		m.insert(QChar(u'ǣ'), QStringLiteral("ae"));
		m.insert(QChar(u'ꝡ'), QStringLiteral("vy"));
		m.insert(QChar(u'ﬀ'), QStringLiteral("ff"));
		m.insert(QChar(u'ᶉ'), QStringLiteral("r"));
		m.insert(QChar(u'ô'), QStringLiteral("o"));
		m.insert(QChar(u'ǿ'), QStringLiteral("o"));
		m.insert(QChar(u'ṳ'), QStringLiteral("u"));
		m.insert(QChar(u'ȥ'), QStringLiteral("z"));
		m.insert(QChar(u'ḟ'), QStringLiteral("f"));
		m.insert(QChar(u'ḓ'), QStringLiteral("d"));
		m.insert(QChar(u'ȇ'), QStringLiteral("e"));
		m.insert(QChar(u'ȕ'), QStringLiteral("u"));
		m.insert(QChar(u'ȵ'), QStringLiteral("n"));
		m.insert(QChar(u'ʠ'), QStringLiteral("q"));
		m.insert(QChar(u'ấ'), QStringLiteral("a"));
		m.insert(QChar(u'ǩ'), QStringLiteral("k"));
		m.insert(QChar(u'ĩ'), QStringLiteral("i"));
		m.insert(QChar(u'ṵ'), QStringLiteral("u"));
		m.insert(QChar(u'ŧ'), QStringLiteral("t"));
		m.insert(QChar(u'ɾ'), QStringLiteral("r"));
		m.insert(QChar(u'ƙ'), QStringLiteral("k"));
		m.insert(QChar(u'ṫ'), QStringLiteral("t"));
		m.insert(QChar(u'ꝗ'), QStringLiteral("q"));
		m.insert(QChar(u'ậ'), QStringLiteral("a"));
		m.insert(QChar(u'ʄ'), QStringLiteral("j"));
		m.insert(QChar(u'ƚ'), QStringLiteral("l"));
		m.insert(QChar(u'ᶂ'), QStringLiteral("f"));
		m.insert(QChar(u'ᵴ'), QStringLiteral("s"));
		m.insert(QChar(u'ꞃ'), QStringLiteral("r"));
		m.insert(QChar(u'ᶌ'), QStringLiteral("v"));
		m.insert(QChar(u'ɵ'), QStringLiteral("o"));
		m.insert(QChar(u'ḉ'), QStringLiteral("c"));
		m.insert(QChar(u'ᵤ'), QStringLiteral("u"));
		m.insert(QChar(u'ẑ'), QStringLiteral("z"));
		m.insert(QChar(u'ṹ'), QStringLiteral("u"));
		m.insert(QChar(u'ň'), QStringLiteral("n"));
		m.insert(QChar(u'ʍ'), QStringLiteral("w"));
		m.insert(QChar(u'ầ'), QStringLiteral("a"));
		m.insert(QChar(u'ǉ'), QStringLiteral("lj"));
		m.insert(QChar(u'ɓ'), QStringLiteral("b"));
		m.insert(QChar(u'ɼ'), QStringLiteral("r"));
		m.insert(QChar(u'ò'), QStringLiteral("o"));
		m.insert(QChar(u'ẘ'), QStringLiteral("w"));
		m.insert(QChar(u'ɗ'), QStringLiteral("d"));
		m.insert(QChar(u'ꜽ'), QStringLiteral("ay"));
		m.insert(QChar(u'ư'), QStringLiteral("u"));
		m.insert(QChar(u'ᶀ'), QStringLiteral("b"));
		m.insert(QChar(u'ǜ'), QStringLiteral("u"));
		m.insert(QChar(u'ẹ'), QStringLiteral("e"));
		m.insert(QChar(u'ǡ'), QStringLiteral("a"));
		m.insert(QChar(u'ɥ'), QStringLiteral("h"));
		m.insert(QChar(u'ṏ'), QStringLiteral("o"));
		m.insert(QChar(u'ǔ'), QStringLiteral("u"));
		m.insert(QChar(u'ʎ'), QStringLiteral("y"));
		m.insert(QChar(u'ȱ'), QStringLiteral("o"));
		m.insert(QChar(u'ệ'), QStringLiteral("e"));
		m.insert(QChar(u'ế'), QStringLiteral("e"));
		m.insert(QChar(u'ĭ'), QStringLiteral("i"));
		m.insert(QChar(u'ⱸ'), QStringLiteral("e"));
		m.insert(QChar(u'ṯ'), QStringLiteral("t"));
		m.insert(QChar(u'ᶑ'), QStringLiteral("d"));
		m.insert(QChar(u'ḧ'), QStringLiteral("h"));
		m.insert(QChar(u'ṥ'), QStringLiteral("s"));
		m.insert(QChar(u'ë'), QStringLiteral("e"));
		m.insert(QChar(u'ᴍ'), QStringLiteral("m"));
		m.insert(QChar(u'ö'), QStringLiteral("o"));
		m.insert(QChar(u'é'), QStringLiteral("e"));
		m.insert(QChar(u'ı'), QStringLiteral("i"));
		m.insert(QChar(u'ď'), QStringLiteral("d"));
		m.insert(QChar(u'ᵯ'), QStringLiteral("m"));
		m.insert(QChar(u'ỵ'), QStringLiteral("y"));
		m.insert(QChar(u'ŵ'), QStringLiteral("w"));
		m.insert(QChar(u'ề'), QStringLiteral("e"));
		m.insert(QChar(u'ứ'), QStringLiteral("u"));
		m.insert(QChar(u'ƶ'), QStringLiteral("z"));
		m.insert(QChar(u'ĵ'), QStringLiteral("j"));
		m.insert(QChar(u'ḍ'), QStringLiteral("d"));
		m.insert(QChar(u'ŭ'), QStringLiteral("u"));
		m.insert(QChar(u'ʝ'), QStringLiteral("j"));
		m.insert(QChar(u'ê'), QStringLiteral("e"));
		m.insert(QChar(u'ǚ'), QStringLiteral("u"));
		m.insert(QChar(u'ġ'), QStringLiteral("g"));
		m.insert(QChar(u'ṙ'), QStringLiteral("r"));
		m.insert(QChar(u'ƞ'), QStringLiteral("n"));
		m.insert(QChar(u'ḗ'), QStringLiteral("e"));
		m.insert(QChar(u'ẝ'), QStringLiteral("s"));
		m.insert(QChar(u'ᶁ'), QStringLiteral("d"));
		m.insert(QChar(u'ķ'), QStringLiteral("k"));
		m.insert(QChar(u'ᴂ'), QStringLiteral("ae"));
		m.insert(QChar(u'ɘ'), QStringLiteral("e"));
		m.insert(QChar(u'ợ'), QStringLiteral("o"));
		m.insert(QChar(u'ḿ'), QStringLiteral("m"));
		m.insert(QChar(u'ꜰ'), QStringLiteral("f"));
		m.insert(QChar(u'ẵ'), QStringLiteral("a"));
		m.insert(QChar(u'ꝏ'), QStringLiteral("oo"));
		m.insert(QChar(u'ᶆ'), QStringLiteral("m"));
		m.insert(QChar(u'ᵽ'), QStringLiteral("p"));
		m.insert(QChar(u'ữ'), QStringLiteral("u"));
		m.insert(QChar(u'ⱪ'), QStringLiteral("k"));
		m.insert(QChar(u'ḥ'), QStringLiteral("h"));
		m.insert(QChar(u'ţ'), QStringLiteral("t"));
		m.insert(QChar(u'ᵱ'), QStringLiteral("p"));
		m.insert(QChar(u'ṁ'), QStringLiteral("m"));
		m.insert(QChar(u'á'), QStringLiteral("a"));
		m.insert(QChar(u'ᴎ'), QStringLiteral("n"));
		m.insert(QChar(u'ꝟ'), QStringLiteral("v"));
		m.insert(QChar(u'è'), QStringLiteral("e"));
		m.insert(QChar(u'ᶎ'), QStringLiteral("z"));
		m.insert(QChar(u'ꝺ'), QStringLiteral("d"));
		m.insert(QChar(u'ᶈ'), QStringLiteral("p"));
		m.insert(QChar(u'ɫ'), QStringLiteral("l"));
		m.insert(QChar(u'ᴢ'), QStringLiteral("z"));
		m.insert(QChar(u'ɱ'), QStringLiteral("m"));
		m.insert(QChar(u'ṝ'), QStringLiteral("r"));
		m.insert(QChar(u'ṽ'), QStringLiteral("v"));
		m.insert(QChar(u'ũ'), QStringLiteral("u"));
		m.insert(QChar(u'ß'), QStringLiteral("ss"));
		m.insert(QChar(u'ĥ'), QStringLiteral("h"));
		m.insert(QChar(u'ᵵ'), QStringLiteral("t"));
		m.insert(QChar(u'ʐ'), QStringLiteral("z"));
		m.insert(QChar(u'ṟ'), QStringLiteral("r"));
		m.insert(QChar(u'ɲ'), QStringLiteral("n"));
		m.insert(QChar(u'à'), QStringLiteral("a"));
		m.insert(QChar(u'ẙ'), QStringLiteral("y"));
		m.insert(QChar(u'ỳ'), QStringLiteral("y"));
		m.insert(QChar(u'ᴔ'), QStringLiteral("oe"));
		m.insert(QChar(u'ₓ'), QStringLiteral("x"));
		m.insert(QChar(u'ȗ'), QStringLiteral("u"));
		m.insert(QChar(u'ⱼ'), QStringLiteral("j"));
		m.insert(QChar(u'ẫ'), QStringLiteral("a"));
		m.insert(QChar(u'ʑ'), QStringLiteral("z"));
		m.insert(QChar(u'ẛ'), QStringLiteral("s"));
		m.insert(QChar(u'ḭ'), QStringLiteral("i"));
		m.insert(QChar(u'ꜵ'), QStringLiteral("ao"));
		m.insert(QChar(u'ɀ'), QStringLiteral("z"));
		m.insert(QChar(u'ÿ'), QStringLiteral("y"));
		m.insert(QChar(u'ǝ'), QStringLiteral("e"));
		m.insert(QChar(u'ǭ'), QStringLiteral("o"));
		m.insert(QChar(u'ᴅ'), QStringLiteral("d"));
		m.insert(QChar(u'ᶅ'), QStringLiteral("l"));
		m.insert(QChar(u'ù'), QStringLiteral("u"));
		m.insert(QChar(u'ạ'), QStringLiteral("a"));
		m.insert(QChar(u'ḅ'), QStringLiteral("b"));
		m.insert(QChar(u'ụ'), QStringLiteral("u"));
		m.insert(QChar(u'ằ'), QStringLiteral("a"));
		m.insert(QChar(u'ᴛ'), QStringLiteral("t"));
		m.insert(QChar(u'ƴ'), QStringLiteral("y"));
		m.insert(QChar(u'ⱦ'), QStringLiteral("t"));
		m.insert(QChar(u'ⱡ'), QStringLiteral("l"));
		m.insert(QChar(u'ȷ'), QStringLiteral("j"));
		m.insert(QChar(u'ᵶ'), QStringLiteral("z"));
		m.insert(QChar(u'ḫ'), QStringLiteral("h"));
		m.insert(QChar(u'ⱳ'), QStringLiteral("w"));
		m.insert(QChar(u'ḵ'), QStringLiteral("k"));
		m.insert(QChar(u'ờ'), QStringLiteral("o"));
		m.insert(QChar(u'î'), QStringLiteral("i"));
		m.insert(QChar(u'ģ'), QStringLiteral("g"));
		m.insert(QChar(u'ȅ'), QStringLiteral("e"));
		m.insert(QChar(u'ȧ'), QStringLiteral("a"));
		m.insert(QChar(u'ẳ'), QStringLiteral("a"));
		m.insert(QChar(u'ɋ'), QStringLiteral("q"));
		m.insert(QChar(u'ṭ'), QStringLiteral("t"));
		m.insert(QChar(u'ꝸ'), QStringLiteral("um"));
		m.insert(QChar(u'ᴄ'), QStringLiteral("c"));
		m.insert(QChar(u'ẍ'), QStringLiteral("x"));
		m.insert(QChar(u'ủ'), QStringLiteral("u"));
		m.insert(QChar(u'ỉ'), QStringLiteral("i"));
		m.insert(QChar(u'ᴚ'), QStringLiteral("r"));
		m.insert(QChar(u'ś'), QStringLiteral("s"));
		m.insert(QChar(u'ꝋ'), QStringLiteral("o"));
		m.insert(QChar(u'ỹ'), QStringLiteral("y"));
		m.insert(QChar(u'ṡ'), QStringLiteral("s"));
		m.insert(QChar(u'ǌ'), QStringLiteral("nj"));
		m.insert(QChar(u'ȁ'), QStringLiteral("a"));
		m.insert(QChar(u'ẗ'), QStringLiteral("t"));
		m.insert(QChar(u'ĺ'), QStringLiteral("l"));
		m.insert(QChar(u'ž'), QStringLiteral("z"));
		m.insert(QChar(u'ᵺ'), QStringLiteral("th"));
		m.insert(QChar(u'ƌ'), QStringLiteral("d"));
		m.insert(QChar(u'ș'), QStringLiteral("s"));
		m.insert(QChar(u'š'), QStringLiteral("s"));
		m.insert(QChar(u'ᶙ'), QStringLiteral("u"));
		m.insert(QChar(u'ẽ'), QStringLiteral("e"));
		m.insert(QChar(u'ẜ'), QStringLiteral("s"));
		m.insert(QChar(u'ɇ'), QStringLiteral("e"));
		m.insert(QChar(u'ṷ'), QStringLiteral("u"));
		m.insert(QChar(u'ố'), QStringLiteral("o"));
		m.insert(QChar(u'ȿ'), QStringLiteral("s"));
		m.insert(QChar(u'ᴠ'), QStringLiteral("v"));
		m.insert(QChar(u'ꝭ'), QStringLiteral("is"));
		m.insert(QChar(u'ᴏ'), QStringLiteral("o"));
		m.insert(QChar(u'ɛ'), QStringLiteral("e"));
		m.insert(QChar(u'ǻ'), QStringLiteral("a"));
		m.insert(QChar(u'ﬄ'), QStringLiteral("ffl"));
		m.insert(QChar(u'ⱺ'), QStringLiteral("o"));
		m.insert(QChar(u'ȋ'), QStringLiteral("i"));
		m.insert(QChar(u'ᵫ'), QStringLiteral("ue"));
		m.insert(QChar(u'ȡ'), QStringLiteral("d"));
		m.insert(QChar(u'ⱬ'), QStringLiteral("z"));
		m.insert(QChar(u'ẁ'), QStringLiteral("w"));
		m.insert(QChar(u'ᶏ'), QStringLiteral("a"));
		m.insert(QChar(u'ꞇ'), QStringLiteral("t"));
		m.insert(QChar(u'ğ'), QStringLiteral("g"));
		m.insert(QChar(u'ɳ'), QStringLiteral("n"));
		m.insert(QChar(u'ʛ'), QStringLiteral("g"));
		m.insert(QChar(u'ᴜ'), QStringLiteral("u"));
		m.insert(QChar(u'ẩ'), QStringLiteral("a"));
		m.insert(QChar(u'ṅ'), QStringLiteral("n"));
		m.insert(QChar(u'ɨ'), QStringLiteral("i"));
		m.insert(QChar(u'ᴙ'), QStringLiteral("r"));
		m.insert(QChar(u'ǎ'), QStringLiteral("a"));
		m.insert(QChar(u'ſ'), QStringLiteral("s"));
		m.insert(QChar(u'ȫ'), QStringLiteral("o"));
		m.insert(QChar(u'ɿ'), QStringLiteral("r"));
		m.insert(QChar(u'ƭ'), QStringLiteral("t"));
		m.insert(QChar(u'ḯ'), QStringLiteral("i"));
		m.insert(QChar(u'ǽ'), QStringLiteral("ae"));
		m.insert(QChar(u'ⱱ'), QStringLiteral("v"));
		m.insert(QChar(u'ɶ'), QStringLiteral("oe"));
		m.insert(QChar(u'ṃ'), QStringLiteral("m"));
		m.insert(QChar(u'ż'), QStringLiteral("z"));
		m.insert(QChar(u'ĕ'), QStringLiteral("e"));
		m.insert(QChar(u'ꜻ'), QStringLiteral("av"));
		m.insert(QChar(u'ở'), QStringLiteral("o"));
		m.insert(QChar(u'ễ'), QStringLiteral("e"));
		m.insert(QChar(u'ɬ'), QStringLiteral("l"));
		m.insert(QChar(u'ị'), QStringLiteral("i"));
		m.insert(QChar(u'ᵭ'), QStringLiteral("d"));
		m.insert(QChar(u'ﬆ'), QStringLiteral("st"));
		m.insert(QChar(u'ḷ'), QStringLiteral("l"));
		m.insert(QChar(u'ŕ'), QStringLiteral("r"));
		m.insert(QChar(u'ᴕ'), QStringLiteral("ou"));
		m.insert(QChar(u'ʈ'), QStringLiteral("t"));
		m.insert(QChar(u'ā'), QStringLiteral("a"));
		m.insert(QChar(u'ḙ'), QStringLiteral("e"));
		m.insert(QChar(u'ᴑ'), QStringLiteral("o"));
		m.insert(QChar(u'ç'), QStringLiteral("c"));
		m.insert(QChar(u'ᶊ'), QStringLiteral("s"));
		m.insert(QChar(u'ặ'), QStringLiteral("a"));
		m.insert(QChar(u'ų'), QStringLiteral("u"));
		m.insert(QChar(u'ả'), QStringLiteral("a"));
		m.insert(QChar(u'ǥ'), QStringLiteral("g"));
		m.insert(QChar(u'ꝁ'), QStringLiteral("k"));
		m.insert(QChar(u'ẕ'), QStringLiteral("z"));
		m.insert(QChar(u'ŝ'), QStringLiteral("s"));
		m.insert(QChar(u'ḕ'), QStringLiteral("e"));
		m.insert(QChar(u'ɠ'), QStringLiteral("g"));
		m.insert(QChar(u'ꝉ'), QStringLiteral("l"));
		m.insert(QChar(u'ꝼ'), QStringLiteral("f"));
		m.insert(QChar(u'ᶍ'), QStringLiteral("x"));
		m.insert(QChar(u'ǒ'), QStringLiteral("o"));
		m.insert(QChar(u'ę'), QStringLiteral("e"));
		m.insert(QChar(u'ổ'), QStringLiteral("o"));
		m.insert(QChar(u'ƫ'), QStringLiteral("t"));
		m.insert(QChar(u'ǫ'), QStringLiteral("o"));
		// m.insert(QChar(u'i̇'), QStringLiteral("i"));
		m.insert(QChar(u'ṇ'), QStringLiteral("n"));
		m.insert(QChar(u'ć'), QStringLiteral("c"));
		m.insert(QChar(u'ᵷ'), QStringLiteral("g"));
		m.insert(QChar(u'ẅ'), QStringLiteral("w"));
		m.insert(QChar(u'ḑ'), QStringLiteral("d"));
		m.insert(QChar(u'ḹ'), QStringLiteral("l"));
		m.insert(QChar(u'œ'), QStringLiteral("oe"));
		m.insert(QChar(u'ᵳ'), QStringLiteral("r"));
		m.insert(QChar(u'ļ'), QStringLiteral("l"));
		m.insert(QChar(u'ȑ'), QStringLiteral("r"));
		m.insert(QChar(u'ȭ'), QStringLiteral("o"));
		m.insert(QChar(u'ᵰ'), QStringLiteral("n"));
		m.insert(QChar(u'ᴁ'), QStringLiteral("ae"));
		m.insert(QChar(u'ŀ'), QStringLiteral("l"));
		m.insert(QChar(u'ä'), QStringLiteral("a"));
		m.insert(QChar(u'ƥ'), QStringLiteral("p"));
		m.insert(QChar(u'ỏ'), QStringLiteral("o"));
		m.insert(QChar(u'į'), QStringLiteral("i"));
		m.insert(QChar(u'ȓ'), QStringLiteral("r"));
		m.insert(QChar(u'ǆ'), QStringLiteral("dz"));
		m.insert(QChar(u'ḡ'), QStringLiteral("g"));
		m.insert(QChar(u'ṻ'), QStringLiteral("u"));
		m.insert(QChar(u'ō'), QStringLiteral("o"));
		m.insert(QChar(u'ľ'), QStringLiteral("l"));
		m.insert(QChar(u'ẃ'), QStringLiteral("w"));
		m.insert(QChar(u'ț'), QStringLiteral("t"));
		m.insert(QChar(u'ń'), QStringLiteral("n"));
		m.insert(QChar(u'ɍ'), QStringLiteral("r"));
		m.insert(QChar(u'ȃ'), QStringLiteral("a"));
		m.insert(QChar(u'ü'), QStringLiteral("u"));
		m.insert(QChar(u'ꞁ'), QStringLiteral("l"));
		m.insert(QChar(u'ᴐ'), QStringLiteral("o"));
		m.insert(QChar(u'ớ'), QStringLiteral("o"));
		m.insert(QChar(u'ᴃ'), QStringLiteral("b"));
		m.insert(QChar(u'ɹ'), QStringLiteral("r"));
		m.insert(QChar(u'ᵲ'), QStringLiteral("r"));
		m.insert(QChar(u'ʏ'), QStringLiteral("y"));
		m.insert(QChar(u'ᵮ'), QStringLiteral("f"));
		m.insert(QChar(u'ⱨ'), QStringLiteral("h"));
		m.insert(QChar(u'ŏ'), QStringLiteral("o"));
		m.insert(QChar(u'ú'), QStringLiteral("u"));
		m.insert(QChar(u'ṛ'), QStringLiteral("r"));
		m.insert(QChar(u'ʮ'), QStringLiteral("h"));
		m.insert(QChar(u'ó'), QStringLiteral("o"));
		m.insert(QChar(u'ů'), QStringLiteral("u"));
		m.insert(QChar(u'ỡ'), QStringLiteral("o"));
		m.insert(QChar(u'ṕ'), QStringLiteral("p"));
		m.insert(QChar(u'ᶖ'), QStringLiteral("i"));
		m.insert(QChar(u'ự'), QStringLiteral("u"));
		m.insert(QChar(u'ã'), QStringLiteral("a"));
		m.insert(QChar(u'ᵢ'), QStringLiteral("i"));
		m.insert(QChar(u'ṱ'), QStringLiteral("t"));
		m.insert(QChar(u'ể'), QStringLiteral("e"));
		m.insert(QChar(u'ử'), QStringLiteral("u"));
		m.insert(QChar(u'í'), QStringLiteral("i"));
		m.insert(QChar(u'ɔ'), QStringLiteral("o"));
		m.insert(QChar(u'ɺ'), QStringLiteral("r"));
		m.insert(QChar(u'ɢ'), QStringLiteral("g"));
		m.insert(QChar(u'ř'), QStringLiteral("r"));
		m.insert(QChar(u'ẖ'), QStringLiteral("h"));
		m.insert(QChar(u'ű'), QStringLiteral("u"));
		m.insert(QChar(u'ȍ'), QStringLiteral("o"));
		m.insert(QChar(u'ḻ'), QStringLiteral("l"));
		m.insert(QChar(u'ḣ'), QStringLiteral("h"));
		m.insert(QChar(u'ȶ'), QStringLiteral("t"));
		m.insert(QChar(u'ņ'), QStringLiteral("n"));
		m.insert(QChar(u'ᶒ'), QStringLiteral("e"));
		m.insert(QChar(u'ì'), QStringLiteral("i"));
		m.insert(QChar(u'ẉ'), QStringLiteral("w"));
		m.insert(QChar(u'ē'), QStringLiteral("e"));
		m.insert(QChar(u'ᴇ'), QStringLiteral("e"));
		m.insert(QChar(u'ł'), QStringLiteral("l"));
		m.insert(QChar(u'ộ'), QStringLiteral("o"));
		m.insert(QChar(u'ɭ'), QStringLiteral("l"));
		m.insert(QChar(u'ẏ'), QStringLiteral("y"));
		m.insert(QChar(u'ᴊ'), QStringLiteral("j"));
		m.insert(QChar(u'ḱ'), QStringLiteral("k"));
		m.insert(QChar(u'ṿ'), QStringLiteral("v"));
		m.insert(QChar(u'ȩ'), QStringLiteral("e"));
		m.insert(QChar(u'â'), QStringLiteral("a"));
		m.insert(QChar(u'ş'), QStringLiteral("s"));
		m.insert(QChar(u'ŗ'), QStringLiteral("r"));
		m.insert(QChar(u'ʋ'), QStringLiteral("v"));
		m.insert(QChar(u'ₐ'), QStringLiteral("a"));
		m.insert(QChar(u'ↄ'), QStringLiteral("c"));
		m.insert(QChar(u'ᶓ'), QStringLiteral("e"));
		m.insert(QChar(u'ɰ'), QStringLiteral("m"));
		m.insert(QChar(u'ᴡ'), QStringLiteral("w"));
		m.insert(QChar(u'ȏ'), QStringLiteral("o"));
		m.insert(QChar(u'č'), QStringLiteral("c"));
		m.insert(QChar(u'ǵ'), QStringLiteral("g"));
		m.insert(QChar(u'ĉ'), QStringLiteral("c"));
		m.insert(QChar(u'ᶗ'), QStringLiteral("o"));
		m.insert(QChar(u'ꝃ'), QStringLiteral("k"));
		m.insert(QChar(u'ꝙ'), QStringLiteral("q"));
		m.insert(QChar(u'ṑ'), QStringLiteral("o"));
		m.insert(QChar(u'ꜱ'), QStringLiteral("s"));
		m.insert(QChar(u'ṓ'), QStringLiteral("o"));
		m.insert(QChar(u'ȟ'), QStringLiteral("h"));
		m.insert(QChar(u'ő'), QStringLiteral("o"));
		m.insert(QChar(u'ꜩ'), QStringLiteral("tz"));
		m.insert(QChar(u'ẻ'), QStringLiteral("e"));
		m.insert(QChar(u'і'), QStringLiteral("i"));
		m.insert(QChar(u'ї'), QStringLiteral("i"));
		return m;
	}();

	static const QHash<QChar, QString> ruMap = []
	{
		QHash<QChar, QString> m;
		m.reserve(33);
		m.insert(QChar(u'а'), QStringLiteral("a"));
		m.insert(QChar(u'б'), QStringLiteral("b"));
		m.insert(QChar(u'в'), QStringLiteral("v"));
		m.insert(QChar(u'г'), QStringLiteral("g"));
		m.insert(QChar(u'д'), QStringLiteral("d"));
		m.insert(QChar(u'е'), QStringLiteral("e"));
		m.insert(QChar(u'ё'), QStringLiteral("yo"));
		m.insert(QChar(u'ж'), QStringLiteral("zh"));
		m.insert(QChar(u'з'), QStringLiteral("z"));
		m.insert(QChar(u'и'), QStringLiteral("i"));
		m.insert(QChar(u'й'), QStringLiteral("i"));
		m.insert(QChar(u'к'), QStringLiteral("k"));
		m.insert(QChar(u'л'), QStringLiteral("l"));
		m.insert(QChar(u'м'), QStringLiteral("m"));
		m.insert(QChar(u'н'), QStringLiteral("n"));
		m.insert(QChar(u'о'), QStringLiteral("o"));
		m.insert(QChar(u'п'), QStringLiteral("p"));
		m.insert(QChar(u'р'), QStringLiteral("r"));
		m.insert(QChar(u'с'), QStringLiteral("s"));
		m.insert(QChar(u'т'), QStringLiteral("t"));
		m.insert(QChar(u'у'), QStringLiteral("u"));
		m.insert(QChar(u'ф'), QStringLiteral("f"));
		m.insert(QChar(u'х'), QStringLiteral("h"));
		m.insert(QChar(u'ц'), QStringLiteral("ts"));
		m.insert(QChar(u'ч'), QStringLiteral("ch"));
		m.insert(QChar(u'ш'), QStringLiteral("sh"));
		m.insert(QChar(u'щ'), QStringLiteral("sch"));
		m.insert(QChar(u'ы'), QStringLiteral("i"));
		m.insert(QChar(u'ь'), QStringLiteral(""));
		m.insert(QChar(u'ъ'), QStringLiteral(""));
		m.insert(QChar(u'э'), QStringLiteral("e"));
		m.insert(QChar(u'ю'), QStringLiteral("yu"));
		m.insert(QChar(u'я'), QStringLiteral("ya"));
		return m;
	}();

	if (s.isEmpty()) {
		return s;
	}

	QString out;
	out.reserve(s.size() * 2);

	for (int i = 0; i < s.size();) {
		const QChar ch = s.at(i++);
		if (const auto tg = trMap.constFind(ch); tg != trMap.cend()) {
			out += tg.value();
			continue;
		}
		if (const auto tr = ruMap.constFind(ch); tr != ruMap.cend()) {
			out += tr.value();
			continue;
		}
		out += ch;
	}

	return out;
}

QString normalized(const QString &s) {
	auto lower = s.trimmed().toLower();
	return translitSafe(lower);
}

bool hasCommonArtist(const QStringList &baseArtists, const QString &itunesArtists) {
	if (itunesArtists.trimmed().isEmpty()) return false;
	static const QRegularExpression splitter(QString::fromUtf8(R"((?i)\s*(?:,|&|feat\.?|ft\.?)\s*)"));
	const auto itunesList = QString(itunesArtists)
		.split(splitter, Qt::SkipEmptyParts)
		.replaceInStrings(QRegularExpression("^\\s+|\\s+$"), QString());
	for (const auto &base : baseArtists) {
		const auto b = normalized(base);
		for (const auto &it : itunesList) {
			const auto i = normalized(it);
			if (i == b) return true;
		}
	}
	return false;
}

QStringList splitArtists(const QString &artists) {
	static const QRegularExpression splitter(QString::fromUtf8(R"((?i)\s*(?:,|&|feat\.?|ft\.?)\s*)"));
	auto list = artists.split(splitter, Qt::SkipEmptyParts);
	for (auto &e : list) e = e.trimmed();
	return list;
}

std::unique_ptr<QNetworkReply, void(*)(QNetworkReply *)> execWithTimeout(
	QNetworkAccessManager &nam,
	const QNetworkRequest &req,
	int timeoutMs) {
	QNetworkReply *reply = nam.get(req);
	QEventLoop loop;
	QTimer timer;
	timer.setSingleShot(true);
	QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	timer.start(timeoutMs);
	loop.exec();
	if (timer.isActive()) {
		timer.stop();
	} else {
		reply->abort();
	}
	return {reply, [](QNetworkReply *r) { if (r) r->deleteLater(); }};
}

QByteArray getBytesWithTimeout(const QUrl &url, int timeoutMs, QByteArray *contentTypeOut = nullptr) {
	QNetworkAccessManager nam;
	QNetworkRequest req(url);
	req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	auto replyPtr = execWithTimeout(nam, req, timeoutMs);
	QNetworkReply *reply = replyPtr.get();
	if (!reply) return {};
	if (reply->error() != QNetworkReply::NoError) return {};
	if (contentTypeOut) {
		*contentTypeOut = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
	}
	return reply->readAll();
}

struct ItunesTrack
{
	QString trackName;
	QString artistName;
	QString collectionName;
	QString artworkUrl100;
};

QList<ItunesTrack> parseTracks(const QByteArray &json) {
	QList<ItunesTrack> out;
	auto doc = QJsonDocument::fromJson(json);
	if (!doc.isObject()) return out;
	auto root = doc.object();
	auto results = root.value(QString::fromUtf8("results")).toArray();
	out.reserve(results.size());
	for (const auto &v : results) {
		const auto o = v.toObject();
		ItunesTrack t;
		t.trackName = o.value(QString::fromUtf8("trackName")).toString();
		t.artistName = o.value(QString::fromUtf8("artistName")).toString();
		t.collectionName = o.value(QString::fromUtf8("collectionName")).toString();
		t.artworkUrl100 = o.value(QString::fromUtf8("artworkUrl100")).toString();
		out.push_back(t);

		LOG(("parsed track: %1 - %2 [%3] (art: %4 )").arg(t.artistName, t.trackName, t.collectionName, t.artworkUrl100));
	}
	return out;
}

QString pickArtworkUrl(const QList<ItunesTrack> &tracks,
					   const QString &targetTitle,
					   const QStringList &baseArtists) {
	if (tracks.isEmpty()) return {};
	for (const auto &t : tracks) {
		if (!t.trackName.compare(targetTitle, Qt::CaseInsensitive) && hasCommonArtist(baseArtists, t.artistName)) {
			return t.artworkUrl100;
		}
	}
	return tracks.first().artworkUrl100;
}

QUrl buildItunesUrl(const QString &performer, const QString &title) {
	QUrl url(QString::fromUtf8("https://itunes.apple.com/search"));
	QUrlQuery query;
	query.addQueryItem(QString::fromUtf8("term"), title + QString::fromUtf8(" - ") + performer);
	query.addQueryItem(QString::fromUtf8("entity"), QString::fromUtf8("song"));
	query.addQueryItem(QString::fromUtf8("limit"), QString::fromUtf8("5"));
	url.setQuery(query);
	return url;
}

QString upgradeArtworkSize(QString url, int sizeHint) {
	if (url.isEmpty()) return url;
	url.replace(QString::fromUtf8("100x100"),
				(sizeHint >= 600) ? QString::fromUtf8("600x600") : QString::fromUtf8("300x300"));
	return url;
}

} // namespace

QPixmap FetchCover(const QString &performer, const QString &title, int sizeHintPx, int timeoutMs) {
	const auto perf = performer.trimmed();
	const auto titl = title.trimmed();
	if (perf.isEmpty() && titl.isEmpty()) return {};

	const auto key = perf + QString::fromUtf8(" - ") + titl;
	if (auto *entry = cache().object(key)) {
		return entry->pix;
	}

	const auto url = buildItunesUrl(perf, titl);
	const auto json = getBytesWithTimeout(url, timeoutMs);
	if (json.isEmpty()) return {};
	const auto tracks = parseTracks(json);
	if (tracks.isEmpty()) return {};

	const auto baseArtists = splitArtists(perf);
	auto artwork = pickArtworkUrl(tracks, titl, baseArtists);
	artwork = upgradeArtworkSize(std::move(artwork), sizeHintPx);
	if (artwork.isEmpty()) return {};

	QByteArray contentType;
	const auto imgBytes = getBytesWithTimeout(QUrl(artwork), timeoutMs, &contentType);
	if (imgBytes.isEmpty()) return {};

	QImage img;
	if (!img.loadFromData(imgBytes)) return {};
	QPixmap pix = QPixmap::fromImage(img);

	auto *stored = new CacheEntry{pix};
	cache().insert(key, stored);
	return pix;
}

}
