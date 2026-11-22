if(NOT TARGET uni.common)
    CPMAddPackage(
            NAME uni.common
            GITHUB_REPOSITORY Uni-Libraries/Uni.Common
            GIT_TAG 97e548c2664aef0a7bc7a1bcb29f77243c7200e7
    )
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        # The pinned public uni_common_bytes.h lacks the declarations for _byteswap_*.
        target_compile_options(uni.common PUBLIC "$<$<COMPILE_LANG_AND_ID:C,Clang>:/FIstdlib.h>")
    endif()
endif()
