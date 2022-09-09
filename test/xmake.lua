target("test-graphics-triangle")
    set_kind("binary")
    add_files("graphics/triangle.cpp")
    add_includedirs("../src/")
    add_deps("bismuth-graphics")
    add_packages("glfw")

target("test-graphics-texture")
    set_kind("binary")
    add_files("graphics/texture.cpp")
    add_includedirs("../src/")
    add_deps("bismuth-graphics")
    add_packages("glfw", "stb")

target("test-graphics-compute")
    set_kind("binary")
    add_files("graphics/compute.cpp")
    add_includedirs("../src/")
    add_deps("bismuth-graphics")
    add_packages("glfw", "stb")

target("test-render_graph-texture")
    set_kind("binary")
    add_files("render_graph/texture.cpp")
    add_includedirs("../src/")
    add_deps("bismuth-render_graph")
    add_packages("glfw", "stb")

target("test-render_graph-deferred")
    set_kind("binary")
    add_files("render_graph/deferred.cpp")
    add_includedirs("../src/")
    add_deps("bismuth-render_graph")
    add_packages("glfw", "glm")
