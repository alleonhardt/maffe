set(HIGHS_FOUND TRUE)

if(NOT TARGET highs::highs)
    add_library(highs::highs INTERFACE IMPORTED)
endif()
