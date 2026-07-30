#include "tfx-bridge/src/lib.rs.h"

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <cstdint>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace {
rust::Vec<std::uint8_t> toRustBytes(const QByteArray &bytes)
{
    rust::Vec<std::uint8_t> result;
    result.reserve(static_cast<std::size_t>(bytes.size()));
    for (const char byte : bytes) {
        result.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(byte)));
    }
    return result;
}

QByteArray fromRustBytes(const rust::Vec<std::uint8_t> &bytes)
{
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}
}

class RustPathBridgeTest : public QObject
{
    Q_OBJECT

private slots:
    void preservesNonUtf8UnixPath();
    void rejectsEmbeddedNul();
    void rejectsEncodingForAnotherPlatform();
};

void RustPathBridgeTest::preservesNonUtf8UnixPath()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    QByteArray nativePath = QFile::encodeName(temp.path());
    nativePath += "/report-";
    nativePath += static_cast<char>(0xff);
    nativePath += ".txt";

    const int descriptor = ::open(nativePath.constData(), O_CREAT | O_EXCL | O_WRONLY, 0600);
    QVERIFY2(descriptor >= 0, "Could not create a file with a non-UTF-8 native name");
    QVERIFY(::close(descriptor) == 0);
    QVERIFY(::access(nativePath.constData(), F_OK) == 0);

    tfx::rust_bridge::NativePath input;
    input.encoding = tfx::rust_bridge::NativePathEncoding::UnixBytes;
    input.data = toRustBytes(nativePath);

    const tfx::rust_bridge::NativePathResult result =
        tfx::rust_bridge::round_trip_native_path(std::move(input));
    QCOMPARE(result.error, tfx::rust_bridge::BridgeErrorCode::Ok);
    QCOMPARE(result.path.encoding, tfx::rust_bridge::NativePathEncoding::UnixBytes);
    QCOMPARE(fromRustBytes(result.path.data), nativePath);
    QVERIFY(::unlink(nativePath.constData()) == 0);
}

void RustPathBridgeTest::rejectsEmbeddedNul()
{
    QByteArray nativePath("/tmp/a");
    nativePath += '\0';
    nativePath += 'b';

    tfx::rust_bridge::NativePath input;
    input.encoding = tfx::rust_bridge::NativePathEncoding::UnixBytes;
    input.data = toRustBytes(nativePath);

    const tfx::rust_bridge::NativePathResult result =
        tfx::rust_bridge::round_trip_native_path(std::move(input));
    QCOMPARE(result.error, tfx::rust_bridge::BridgeErrorCode::InvalidInput);
    QVERIFY(result.path.data.empty());
}

void RustPathBridgeTest::rejectsEncodingForAnotherPlatform()
{
    tfx::rust_bridge::NativePath input;
    input.encoding = tfx::rust_bridge::NativePathEncoding::WindowsUtf16Le;
    input.data = toRustBytes(QByteArray("C\0:\0", 4));

    const tfx::rust_bridge::NativePathResult result =
        tfx::rust_bridge::round_trip_native_path(std::move(input));
    QCOMPARE(result.error, tfx::rust_bridge::BridgeErrorCode::Unsupported);
    QVERIFY(result.path.data.empty());
}

QTEST_GUILESS_MAIN(RustPathBridgeTest)

#include "RustPathBridgeTest.moc"
