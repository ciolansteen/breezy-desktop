#pragma once
#include <QByteArray>
#include <QFile>
#include <QString>
#include <elf.h>
#include <link.h>
#include <cstring>
#include <cstdint>
#include <limits>

namespace PluginBuildId {
struct Result { QByteArray id; QString path; };
static QByteArray fromNotes(const char *data, size_t size)
{
    size_t offset = 0;
    while (data && offset <= size && size - offset >= sizeof(Elf64_Nhdr)) {
        Elf64_Nhdr note;
        std::memcpy(&note, data + offset, sizeof(note));
        offset += sizeof(note);
        const size_t nameSize = (size_t(note.n_namesz) + 3) & ~size_t(3);
        const size_t descSize = (size_t(note.n_descsz) + 3) & ~size_t(3);
        if (nameSize > size - offset || descSize > size - offset - nameSize) return {};
        if (note.n_type == NT_GNU_BUILD_ID && note.n_namesz == 4 && note.n_descsz > 0
            && std::memcmp(data + offset, "GNU\0", 4) == 0) {
            return QByteArray(data + offset + nameSize, note.n_descsz).toHex();
        }
        offset += nameSize + descSize;
    }
    return {};
}
static QByteArray fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    Elf64_Ehdr header;
    if (file.read(reinterpret_cast<char *>(&header), sizeof(header)) != sizeof(header)
        || std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0
        || header.e_ident[EI_CLASS] != ELFCLASS64 || header.e_ident[EI_DATA] != ELFDATA2LSB
        || header.e_phentsize != sizeof(Elf64_Phdr) || header.e_phnum > 4096) return {};
    const quint64 fileSize = file.size();
    if (header.e_phoff > fileSize || quint64(header.e_phnum) * sizeof(Elf64_Phdr) > fileSize - header.e_phoff) return {};
    for (quint64 i = 0; i < header.e_phnum; ++i) {
        Elf64_Phdr segment;
        if (!file.seek(header.e_phoff + i * sizeof(segment))
            || file.read(reinterpret_cast<char *>(&segment), sizeof(segment)) != sizeof(segment)) return {};
        if (segment.p_type != PT_NOTE) continue;
        if (segment.p_filesz > 1024 * 1024 || segment.p_offset > fileSize
            || segment.p_filesz > fileSize - segment.p_offset || !file.seek(segment.p_offset)) continue;
        const auto notes = file.read(segment.p_filesz);
        if (quint64(notes.size()) != segment.p_filesz) continue;
        const auto id = fromNotes(notes.constData(), notes.size());
        if (!id.isEmpty()) return id;
    }
    return {};
}
static bool readableNoteRange(const ElfW(Phdr) &load, const ElfW(Phdr) &note, uintptr_t base)
{
    const auto limit = std::numeric_limits<uintptr_t>::max();
    if (load.p_type != PT_LOAD || !(load.p_flags & PF_R) || note.p_vaddr < load.p_vaddr
        || note.p_vaddr > limit - base || note.p_filesz > limit - base - note.p_vaddr) return false;
    const auto offset = note.p_vaddr - load.p_vaddr;
    return offset <= load.p_memsz && note.p_filesz <= load.p_memsz - offset;
}
static Result fromAddress(const void *address)
{
    if (!address) return {};
    struct Search { uintptr_t address; Result result; } search{reinterpret_cast<uintptr_t>(address), {}};
    dl_iterate_phdr([](dl_phdr_info *info, size_t, void *opaque) -> int {
        auto &search = *static_cast<Search *>(opaque);
        const uintptr_t base = info->dlpi_addr;
        bool matches = false;
        for (int i = 0; i < info->dlpi_phnum; ++i) {
            const auto &load = info->dlpi_phdr[i];
            if (load.p_type == PT_LOAD && load.p_vaddr <= std::numeric_limits<uintptr_t>::max() - base) {
                const uintptr_t start = base + load.p_vaddr;
                if (search.address >= start && search.address - start < load.p_memsz) matches = true;
            }
        }
        if (!matches) return 0;
        search.result.path = QString::fromLocal8Bit(info->dlpi_name);
        for (int i = 0; i < info->dlpi_phnum; ++i) {
            const auto &note = info->dlpi_phdr[i];
            if (note.p_type != PT_NOTE || note.p_filesz > 1024 * 1024) continue;
            for (int j = 0; j < info->dlpi_phnum; ++j) {
                const auto &load = info->dlpi_phdr[j];
                if (!readableNoteRange(load, note, base)) continue;
                const auto id = fromNotes(reinterpret_cast<const char *>(base + note.p_vaddr), note.p_filesz);
                if (!id.isEmpty()) { search.result.id = id; return 1; }
            }
        }
        return 1;
    }, &search);
    return search.result;
}
}
