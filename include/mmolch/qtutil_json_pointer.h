#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringView>
#include <QList>
#include <expected>

namespace mmolch::qtutil {

class JsonPointer
{
public:
    using Error = QString;
    using Result = std::expected<JsonPointer, Error>;

    // Factory from RFC 6901 pointer string (e.g. "/foo/bar/0")
    static Result create(QStringView pointer);

    // Factory from pre-parsed segments (already unescaped)
    static Result fromSegments(QList<QString> segments);

    // Return original pointer string (RFC 6901 form)
    const QString& toString() const noexcept { return m_original; }

    // Return parsed, unescaped segments
    const QList<QString>& segments() const noexcept { return m_segments; }

    // Is this the root pointer (empty string)?
    bool isRoot() const noexcept { return m_segments.isEmpty(); }

    // Resolve against a QJsonValue (undefined if not found / invalid path)
    QJsonValue resolve(const QJsonValue& root) const;

    // Convenience: does this pointer resolve to a value?
    bool contains(const QJsonValue& root) const { return !resolve(root).isUndefined(); }

    // Parent pointer (empty/root if already root)
    JsonPointer parent() const;

    // Child pointer (append one segment, escaping as needed)
    JsonPointer child(QStringView segment) const;

    // Join with another pointer (append its segments)
    JsonPointer join(const JsonPointer& other) const;

private:
    QString m_original;
    QList<QString> m_segments;

    JsonPointer() = default;
    JsonPointer(QString original, QList<QString> segments)
        : m_original(std::move(original)), m_segments(std::move(segments)) {}

    static Result parse(QStringView pointer);
    static QString unescape(QStringView s, bool& ok, QString& error);
    static QString escape(QStringView s);
};

// Convenience free function
inline QJsonValue jsonResolvePointer(const QJsonValue& root, QStringView pointer)
{
    if (auto r = JsonPointer::create(pointer)) {
        return r->resolve(root);
    }
    return QJsonValue(); // undefined
}

} // namespace mmolch::qtutil
