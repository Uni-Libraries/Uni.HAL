CPMAddPackage(
        NAME nanoprintf
        GITHUB_REPOSITORY charlesnicholson/nanoprintf
        GIT_TAG f02dc69c45a422e9156b11a44f21e5e0fd222076
        VERSION 2025.05.26
        DOWNLOAD_ONLY TRUE
)

if(nanoprintf_ADDED)
    add_library(nanoprintf INTERFACE)
    target_include_directories(nanoprintf INTERFACE ${nanoprintf_SOURCE_DIR})
endif()
