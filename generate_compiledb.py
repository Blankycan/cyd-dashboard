Import("env")

# Refresh compile_commands.json after each successful build so clangd/Cursor
# can resolve Arduino and PlatformIO library headers.
def generate_compiledb(source, target, env):
    env.Execute("pio run -t compiledb -e %s --silent" % env["PIOENV"])

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", generate_compiledb)
