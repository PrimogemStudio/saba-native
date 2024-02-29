set_languages("c++23")
add_rules("mode.debug")
add_rules("mode.release")
add_rules("mode.releasedbg")

local dep = { "spdlog", "glm", "bullet3" }
add_requires(dep)

target("saba-native")
    set_kind("shared")
    set_exceptions("cxx")
    add_files("src/**.cpp")
    add_files("src/**.cppm")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    add_sysincludedirs("include")
    set_policy("build.c++.modules", true)
    add_packages(dep)
    if is_plat("windows") then
        add_syslinks("advapi32")
        add_sysincludedirs("include/platform/windows")
    else
        add_sysincludedirs("include/platform/unix")
    end