import os

Import("env")

def patch_bluefruit(source, target, env):
    # Dynamically find the framework directory for the current environment
    framework_dir = env.PioPlatform().get_package_dir("framework-arduinoadafruitnrf52-seeed")
    
    if not framework_dir:
        print("Warning: Could not find framework-arduinoadafruitnrf52-seeed package. Skipping patch.")
        return

    filepath = os.path.join(framework_dir, "libraries", "Bluefruit52Lib", "src", "BLEConnection.cpp")

    if not os.path.exists(filepath):
        print(f"Warning: Could not find BLEConnection.cpp at {filepath}. Skipping patch.")
        return

    with open(filepath, "r") as f:
        content = f.read()

    # The buggy original code
    old_code = "xSemaphoreTake(_hvc_sem, portMAX_DELAY);"
    # The patched code (1000ms timeout)
    new_code = "xSemaphoreTake(_hvc_sem, pdMS_TO_TICKS(1000));"

    if old_code in content:
        print(f"Applying indicate() timeout patch to {filepath}...")
        content = content.replace(old_code, new_code)
        with open(filepath, "w") as f:
            f.write(content)
        print("Patch applied successfully.")
    else:
        # Check if already patched to avoid spamming the console
        if new_code in content:
            pass # Already patched
        else:
            print(f"Warning: Could not find the expected code block in {filepath}. The library might have been updated.")

# Attach the pre-action to the build environment
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", patch_bluefruit)
