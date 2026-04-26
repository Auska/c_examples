set_project("c_examples")
set_version("1.0.0")

add_rules("mode.debug", "mode.release")

option("ENABLE_TESTS")
option("ENABLE_BENCHMARKS")

target("celero")
    set_kind("static")
    add_files("external/Celero-2.10.0/src/*.cpp")
    add_includedirs("external/Celero-2.10.0/include")
    add_cflags("-std=c++23", {force = true})
    add_cxxflags("-std=c++23", {force = true})

target("folder_similarity")
    set_kind("binary")
    add_files("src/folder_similarity/main.cpp")
    add_includedirs("src")
    add_cflags("-std=c++23", {force = true})
    add_cxxflags("-std=c++23", {force = true})

target("oldsort")
    set_kind("binary")
    add_files("src/oldsort/main.cpp")
    add_includedirs("src")
    add_cflags("-std=c++23", {force = true})
    add_cxxflags("-std=c++23", {force = true})

target("extract_name")
    set_kind("binary")
    add_files("src/extract_name/main.cpp")
    add_includedirs("src")
    add_cflags("-std=c++23", {force = true})
    add_cxxflags("-std=c++23", {force = true})

if get_config("ENABLE_TESTS") then
    target("test_runner")
        set_kind("binary")
        add_files("tests/test_common.cpp", "external/catch_amalgamated.cpp")
        add_includedirs("src", ".")
        add_cflags("-std=c++23", {force = true})
        add_cxxflags("-std=c++23", {force = true})
end

if get_config("ENABLE_BENCHMARKS") then
    target("benchmark_runner")
        set_kind("binary")
        add_files("benchmarks/benchmark_common.cpp")
        add_includedirs("src", "external/Celero-2.10.0/include")
        add_deps("celero")
        add_cflags("-std=c++23", {force = true})
        add_cxxflags("-std=c++23", {force = true})
end

target("install")
    set_kind("phony")
    after_build("folder_similarity", "oldsort", "extract_name")
    on_run(function(target)
        import("core.project.config")
        local bindir = config.buildir() .. "/bin"
        os.mkdir(bindir)
        os.cp(target:targetdir() .. "/folder_similarity", bindir)
        os.cp(target:targetdir() .. "/oldsort", bindir)
        os.cp(target:targetdir() .. "/extract_name", bindir)
    end)
