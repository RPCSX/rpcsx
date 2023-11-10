#include "rx/Version.hpp"

rx::Version rx::getVersion() {
    return {
        .raw = RX_RAW_VERSION,
        .tag = static_cast<VersionTag>(RX_TAG),
        .tagVersion = RX_TAG_VERSION,
        .gitTag = RX_GIT_REV,
        .dirty = (RX_GIT_DIRTY != 0),
    };
}
