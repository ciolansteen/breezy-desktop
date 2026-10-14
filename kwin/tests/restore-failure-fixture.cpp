// Test-only interposition: reject the effect's explicit layout application.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

extern "C" __attribute__((visibility("default"))) int testApplyConfiguration(void *, void *)
    asm("_ZN4KWin9Workspace24applyOutputConfigurationERNS_19OutputConfigurationE");

extern "C" int testApplyConfiguration(void *workspace, void *config)
{
    Dl_info caller{};
    const char *marker = std::getenv("BREEZY_RESTORE_FAILURE_MARKER");
    if (marker && access(marker, F_OK) == 0
        && dladdr(__builtin_return_address(0), &caller) && caller.dli_fname
        && std::strstr(caller.dli_fname, "breezy_desktop.so")) {
        std::fprintf(stderr, "TEST: injected virtual layout rejection\n");
        return 1; // OutputConfigurationError::Unknown
    }
    using Apply = int (*)(void *, void *);
    static auto apply = reinterpret_cast<Apply>(dlsym(RTLD_NEXT,
        "_ZN4KWin9Workspace24applyOutputConfigurationERNS_19OutputConfigurationE"));
    if (!apply) std::abort();
    return apply(workspace, config);
}
