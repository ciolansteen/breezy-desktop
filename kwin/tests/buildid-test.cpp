#include "../src/pluginbuildid.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <dlfcn.h>
#include <iostream>
#include <cstdlib>

static void require(bool condition, const char *message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    require(argc == 3, "usage: buildid-test fixture-v1 fixture-v2");
    ElfW(Phdr) load{}, note{};
    load.p_type = PT_LOAD; load.p_flags = PF_R; load.p_vaddr = 128; load.p_memsz = 256;
    note.p_type = PT_NOTE; note.p_vaddr = 192; note.p_filesz = 32;
    require(PluginBuildId::readableNoteRange(load, note, 0), "readable note range refused");
    load.p_flags = PF_X;
    require(!PluginBuildId::readableNoteRange(load, note, 0), "execute-only note range accepted");
    load.p_flags = PF_R; note.p_filesz = 512;
    require(!PluginBuildId::readableNoteRange(load, note, 0), "truncated note range accepted");
    note.p_filesz = 32;
    require(!PluginBuildId::readableNoteRange(load, note, std::numeric_limits<uintptr_t>::max()), "base overflow accepted");
    require(!PluginBuildId::readableNoteRange(load, note, std::numeric_limits<uintptr_t>::max() - note.p_vaddr), "end overflow accepted");
    QTemporaryDir tmp;
    require(tmp.isValid(), "temporary directory");
    const QString target = tmp.filePath(QStringLiteral("plugin.so"));
    require(QFile::copy(QString::fromLocal8Bit(argv[1]), target), "copy v1");
    const QByteArray first = PluginBuildId::fromFile(target);
    require(!first.isEmpty(), "v1 GNU build ID");
    require(!PluginBuildId::fromFile(QString::fromLocal8Bit(argv[2])).isEmpty(), "v2 GNU build ID");
    require(first != PluginBuildId::fromFile(QString::fromLocal8Bit(argv[2])), "fixture build IDs differ");
    void *handle = dlopen(target.toLocal8Bit().constData(), RTLD_NOW | RTLD_LOCAL);
    require(handle, "load fixture");
    void *symbol = dlsym(handle, "fixture_version");
    require(symbol, "fixture symbol");
    auto loaded = PluginBuildId::fromAddress(symbol);
    require(loaded.id == first, "mapped ID equals original disk ID");
    require(loaded.path == target, "mapped path");
    const QString replacement = tmp.filePath(QStringLiteral("replacement.so"));
    require(QFile::copy(QString::fromLocal8Bit(argv[2]), replacement), "copy replacement");
    require(::rename(replacement.toLocal8Bit().constData(), target.toLocal8Bit().constData()) == 0, "atomic upgrade");
    require(PluginBuildId::fromAddress(symbol).id == first, "mapped ID survives atomic upgrade");
    require(PluginBuildId::fromFile(target) != first, "stale library detected");
    require(PluginBuildId::fromAddress(nullptr).id.isEmpty(), "unknown address is unverified");
    require(PluginBuildId::fromFile(tmp.filePath(QStringLiteral("absent"))).isEmpty(), "missing file is unverified");
    QFile malformed(tmp.filePath(QStringLiteral("malformed")));
    require(malformed.open(QIODevice::WriteOnly), "malformed fixture");
    malformed.write("not an ELF");
    malformed.close();
    require(PluginBuildId::fromFile(malformed.fileName()).isEmpty(), "malformed ELF rejected");
    dlclose(handle);
    std::cout << "PASS build ID: loaded/disk match, atomic upgrade, stale detection, absent and malformed input\n";
}
