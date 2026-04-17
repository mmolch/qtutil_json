#include "mmolch/qtutil_json_pointer.h"

#include <QJsonArray>
#include <QJsonObject>

namespace mmolch::qtutil {

JsonPointer::Result JsonPointer::create(QStringView pointer)
{
    return parse(pointer);
}

JsonPointer::Result JsonPointer::fromSegments(QList<QString> segments)
{
    // Rebuild original string with escaping
    QString original;
    if (!segments.isEmpty())
        original.reserve(segments.size() * 4); // rough guess

    for (int i = 0; i < segments.size(); ++i) {
        original += u'/';
        original += escape(segments[i]);
    }

    JsonPointer p;
    p.m_original = std::move(original);
    p.m_segments = std::move(segments);
    return p;
}

JsonPointer::Result JsonPointer::parse(QStringView pointer)
{
    JsonPointer p;
    p.m_original = pointer.toString();

    if (pointer.isEmpty()) {
        // root pointer
        return p;
    }

    if (pointer.front() != u'/') {
        return std::unexpected(QStringLiteral("JSON Pointer must start with '/'"));
    }

    // Skip leading '/'
    pointer = pointer.mid(1);

    const auto parts = pointer.split(u'/');
    p.m_segments.reserve(parts.size());

    for (QStringView part : parts) {
        bool ok = false;
        QString error;
        QString seg = unescape(part, ok, error);
        if (!ok) {
            return std::unexpected(error);
        }
        p.m_segments.push_back(std::move(seg));
    }

    return p;
}

QString JsonPointer::unescape(QStringView s, bool& ok, QString& error)
{
    QString out;
    out.reserve(s.size());
    ok = true;
    error.clear();

    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (c == u'~') {
            if (i + 1 >= s.size()) {
                ok = false;
                error = QStringLiteral("Invalid '~' escape in JSON Pointer");
                return {};
            }
            const QChar next = s[i + 1];
            if (next == u'0') {
                out += u'~';
            } else if (next == u'1') {
                out += u'/';
            } else {
                ok = false;
                error = QStringLiteral("Invalid '~x' escape in JSON Pointer");
                return {};
            }
            ++i; // skip next char
        } else {
            out += c;
        }
    }

    return out;
}

QString JsonPointer::escape(QStringView s)
{
    QString out;
    out.reserve(s.size());

    for (QChar c : s) {
        if (c == u'~') {
            out += QStringLiteral("~0");
        } else if (c == u'/') {
            out += QStringLiteral("~1");
        } else {
            out += c;
        }
    }

    return out;
}

QJsonValue JsonPointer::resolve(const QJsonValue& root) const
{
    QJsonValue current = root;

    for (const QString& seg : m_segments) {
        if (current.isObject()) {
            const QJsonObject obj = current.toObject();
            auto it = obj.find(seg);
            if (it == obj.end())
                return QJsonValue(); // undefined
            current = *it;
        } else if (current.isArray()) {
            bool ok = false;
            int index = seg.toInt(&ok);
            if (!ok)
                return QJsonValue(); // invalid index
            const QJsonArray arr = current.toArray();
            if (index < 0 || index >= arr.size())
                return QJsonValue(); // out of range
            current = arr.at(index);
        } else {
            return QJsonValue(); // cannot descend further
        }
    }

    return current;
}

JsonPointer JsonPointer::parent() const
{
    if (m_segments.isEmpty())
        return *this; // root stays root

    QList<QString> parentSegs = m_segments;
    parentSegs.removeLast();
    auto r = JsonPointer::fromSegments(parentSegs);
    // fromSegments cannot fail here
    return *r;
}

JsonPointer JsonPointer::child(QStringView segment) const
{
    QList<QString> segs = m_segments;
    segs.push_back(segment.toString());
    auto r = JsonPointer::fromSegments(segs);
    return *r;
}

JsonPointer JsonPointer::join(const JsonPointer& other) const
{
    if (other.m_segments.isEmpty())
        return *this;
    if (m_segments.isEmpty())
        return other;

    QList<QString> segs;
    segs.reserve(m_segments.size() + other.m_segments.size());
    segs << m_segments << other.m_segments;

    auto r = JsonPointer::fromSegments(segs);
    return *r;
}

} // namespace mmolch::qtutil
