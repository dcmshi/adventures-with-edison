add_library(edison_core
    src/formats/ne_file.cpp
    src/audio/artech_fm_driver.cpp
)
target_include_directories(edison_core PUBLIC src)

# Replays every sound through the native driver and diffs the OPL register
# stream against the reference logs from tools/oplref.py.
add_executable(seqtest tests/seqtest.cpp)
target_link_libraries(seqtest PRIVATE edison_core)

add_test(NAME sequencer_matches_reference
         COMMAND seqtest ${PROJECT_SOURCE_DIR}/original/cd/DSK3 ${PROJECT_SOURCE_DIR}/extracted/oplref)
