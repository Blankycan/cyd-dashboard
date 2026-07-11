Import("env")
import shutil, os

src = os.path.join(env["PROJECT_SRC_DIR"], "User_Setup.h")
dst = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "TFT_eSPI", "User_Setup.h")

if os.path.exists(dst):
    shutil.copy(src, dst)
    print("Copied User_Setup.h to TFT_eSPI library")
