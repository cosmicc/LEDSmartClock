Import("env")

import os
import shutil


def copy_ota_update_artifact(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    source_path = os.path.join(build_dir, "firmware.bin")
    update_path = os.path.join(build_dir, "update.bin")

    if not os.path.exists(source_path):
        print("OTA artifact copy skipped; the PlatformIO app image was not found.")
        return

    shutil.copyfile(source_path, update_path)
    print(f"Copied OTA update artifact: {update_path}")


env.AddPostAction("$BUILD_DIR/firmware.bin", copy_ota_update_artifact)
