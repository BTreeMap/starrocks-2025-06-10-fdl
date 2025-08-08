#!/usr/bin/env python3
"""
Disk space cleanup script for GitHub Actions CI environments.

This script removes unnecessary packages and directories to free up disk space
for Docker builds, helping prevent "no space left on device" errors.
"""

import subprocess
import logging
import sys

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(message)s",
    handlers=[logging.StreamHandler(sys.stdout)],
)
logger = logging.getLogger(__name__)


def remove_packages(pkg_patterns):
    """
    Remove packages matching the given patterns.

    Args:
        pkg_patterns (list): List of package patterns to remove
    """
    for pattern in pkg_patterns:
        logger.info(f"Removing packages matching pattern: {pattern}")
        try:
            # First check if any packages match the pattern
            result = subprocess.run(
                ["dpkg", "-l", pattern], capture_output=True, text=True, check=False
            )

            if result.returncode == 0 and result.stdout.strip():
                # Packages found, remove them
                subprocess.run(
                    ["sudo", "apt-get", "remove", "-y", pattern], check=False
                )
            else:
                logger.info(f"No packages found for pattern: {pattern}")
        except Exception as e:
            logger.warning(f"Error removing packages for pattern {pattern}: {e}")


def free_disk_space():
    """
    Removes unnecessary packages and directories to free up disk space for Docker builds.

    This function executes a series of cleanup operations targeting commonly unused
    packages in CI environments, helping prevent "no space left on device" errors.
    """
    logger.info("Current disk space before cleanup:")
    subprocess.run(["df", "-h"], check=False)

    # Group apt-get removal commands together with packages sorted alphabetically
    logger.info("Removing unnecessary packages...")
    # List packages to remove with globbing patterns
    pkg_patterns = [
        "dotnet-*",
        "golang-*",
        "llvm-*",
        "temurin-*-jdk",
        "azure-cli",
        "firefox",
        "snapd",
    ]
    remove_packages(pkg_patterns)

    # Clean up package management system
    logger.info("Performing system cleanup...")
    subprocess.run(["sudo", "apt-get", "autoremove", "-y"], check=False)
    subprocess.run(["sudo", "apt-get", "clean"], check=False)

    # Group directory removals together with paths sorted alphabetically
    logger.info("Removing large directory trees...")
    large_directories = ["/opt/ghc", "/usr/local/lib/android", "/usr/share/dotnet/"]
    for directory in large_directories:
        logger.info(f"Removing directory: {directory}")
        subprocess.run(["sudo", "rm", "-rf", directory], check=False)

    # Show available space after cleanup
    logger.info("Current disk space after cleanup:")
    subprocess.run(["df", "-h"], check=False)


if __name__ == "__main__":
    logger.info("Starting disk space cleanup...")
    try:
        free_disk_space()
        logger.info("Disk space cleanup completed successfully!")
    except Exception as e:
        logger.error(f"Error during disk space cleanup: {e}")
        sys.exit(1)
