if(NOT DEFINED CMKR_TARGET)
    message(FATAL_ERROR "CMKR_TARGET not defined. Usage: include-after = [\"Qt5DeployTarget.cmake\"]")
endif()

# Enable Qt moc/rrc/uic support
target_qt(${CMKR_TARGET})

# Copy Qt DLLs next to the application
target_windeployqt(${CMKR_TARGET})