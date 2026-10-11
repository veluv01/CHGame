"""Select the pinned WCH GCC 8 toolchain; no separate Arduino framework copy."""
from platformio import util
from platformio.public import PlatformBase


class ChgameweactPlatform(PlatformBase):
    TOOLCHAINS = {
        "linux_x86_64": "linux.git#e8e7ba93c9c1ea30366d98f28318d4ad09b369e2",
        "windows_amd64": "windows.git#8ee4117c75e131974d47ddaba2c77c2a520ecae4",
        "windows_x86": "windows.git#8ee4117c75e131974d47ddaba2c77c2a520ecae4",
        "darwin_x86_64": "mac.git#99cb62f72304e1a7afe8e7c191143975793ccfab",
        "darwin_arm64": "mac.git#99cb62f72304e1a7afe8e7c191143975793ccfab",
    }

    def configure_default_packages(self, variables, targets):
        system = util.get_systype()
        if system not in self.TOOLCHAINS:
            raise RuntimeError("No pinned WCH GCC 8 toolchain for " + system)
        self.packages["toolchain-riscv"]["version"] = (
            "https://github.com/Community-PIO-CH32V/toolchain-riscv-"
            + self.TOOLCHAINS[system]
        )
        return super().configure_default_packages(variables, targets)
