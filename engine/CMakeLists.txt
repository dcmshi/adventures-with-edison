add_library(edison_core
    src/formats/ne_file.cpp
    src/formats/codeview.cpp
    src/audio/artech_fm_driver.cpp
    src/audio/opl_synth.cpp
    src/audio/fm_renderer.cpp
)
target_include_directories(edison_core PUBLIC src)
target_link_libraries(edison_core PUBLIC nuked_opl3)
target_compile_options(edison_core PRIVATE ${EDISON_WARNINGS})

# Replays every sound through the native driver and diffs the OPL register
# stream against the reference logs from tools/oplref.py.
add_executable(seqtest tests/seqtest.cpp)
target_link_libraries(seqtest PRIVATE edison_core)
target_compile_options(seqtest PRIVATE ${EDISON_WARNINGS})

add_test(NAME sequencer_matches_reference
         COMMAND seqtest ${PROJECT_SOURCE_DIR}/original/cd/DSK3 ${PROJECT_SOURCE_DIR}/extracted/oplref)

# Command-line FM music player: plays or renders sounds from the game's DLLs.
add_executable(fmplay apps/fmplay.cpp)
target_link_libraries(fmplay PRIVATE edison_core SDL3::SDL3)
target_compile_options(fmplay PRIVATE ${EDISON_WARNINGS})
