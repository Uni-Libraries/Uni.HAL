CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    VERSION 3.7.0
)
list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
