add_executable(cbl-log EXCLUDE_FROM_ALL
                ${LOGCAT_SRC}
                ${LINENOISE_SRC}
                Tool.cc
                ArgumentTokenizer.cc)
 
 target_include_directories(
     cbl-log PRIVATE
     ${PROJECT_SOURCE_DIR}
     ${LITECORE}C/include/
     vendor/linenoise-ng/include/
     ${LITECORE}LiteCore/Support/
     ${LITECORE}vendor/fleece/API/
     ${LITECORE}vendor/fleece/Fleece/Support/ # PlatformCompat.hh
     ${PROJECT_BINARY_DIR}/generated_headers/
 )
 target_compile_definitions(cbl-log PRIVATE -DCBLTOOL_NO_C_API -DCMAKE)
 target_link_libraries(cbl-log FleeceStatic Support BLIPStatic ${PLATFORM_LIBS})
