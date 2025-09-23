if(NOT TARGET uni.common)
    CPMAddPackage(
            NAME uni.common
            GITHUB_REPOSITORY Uni-Libraries/Uni.Common
            GIT_TAG 97e548c2664aef0a7bc7a1bcb29f77243c7200e7
    )
endif()
