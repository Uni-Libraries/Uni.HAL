CPMAddPackage(
        NAME nanoprintf
        GITHUB_REPOSITORY charlesnicholson/nanoprintf
        GIT_TAG 43828b53048ab342540e21cf967dc5f81807c698
)

if(nanoprintf_ADDED)
    add_library(nanoprintf INTERFACE)
    target_include_directories(nanoprintf INTERFACE ${nanoprintf_SOURCE_DIR})
endif()
