add_rules("mode.debug","mode.release")
add_requires("imgui",{configs={glfw_opengl3=true}})
target("opacity")
    set_toolchains("msvc")
    set_kind("binary")
    add_packages("imgui")
    add_files("main.cpp")
    set_languages("c++23")
    set_symbols("debug")
