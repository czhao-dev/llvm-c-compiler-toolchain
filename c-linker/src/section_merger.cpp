#include "section_merger.h"

#include <algorithm>

namespace clnk {

namespace {

std::uint64_t alignUp(std::uint64_t value, std::uint64_t align) {
    align = std::max<std::uint64_t>(align, 1);
    return ((value + align - 1) / align) * align;
}

} // namespace

MergedLayout mergeSections(const std::vector<ObjectFile> &files, std::uint64_t textBase, std::uint64_t dataBase) {
    MergedLayout layout;
    layout.textBase = textBase;
    layout.dataBase = dataBase;
    layout.textFileOffset.resize(files.size());
    layout.dataFileOffset.resize(files.size());

    for (std::size_t fi = 0; fi < files.size(); ++fi) {
        const ObjectFile &file = files[fi];
        if (!file.text.empty()) {
            std::uint64_t offset = alignUp(layout.text.size(), file.textAlign);
            layout.text.resize(offset, std::byte{0});
            layout.textFileOffset[fi] = offset;
            layout.text.insert(layout.text.end(), file.text.begin(), file.text.end());
        }
        if (!file.data.empty()) {
            std::uint64_t offset = alignUp(layout.data.size(), file.dataAlign);
            layout.data.resize(offset, std::byte{0});
            layout.dataFileOffset[fi] = offset;
            layout.data.insert(layout.data.end(), file.data.begin(), file.data.end());
        }
    }

    return layout;
}

} // namespace clnk
