set(PESOS_APP_SOURCES
    "audio.cpp"
    "camera.cpp"
    "counting.cpp"
    "inference.cpp"
    "main.cpp"
    "model_data_stub.cpp"
    "power.cpp"
    "speech.cpp"
)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/model_data.cpp")
    list(APPEND PESOS_APP_SOURCES "model_data.cpp")
endif()

idf_component_register(
    SRCS ${PESOS_APP_SOURCES}
    INCLUDE_DIRS "." "../include"
)
