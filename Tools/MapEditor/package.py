import argparse
import os
import sys
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
BIN = os.path.join(ROOT, "bin", "Win")
FILES = ["MapEditor64.exe", "XvidDecoder.dll"]
INTERFACE = os.path.join("Interface", "MapEditor")
SKIP_DIRS = {"Cache"}


def main():
    parser = argparse.ArgumentParser(description="Pack MapEditor64.exe and its interface files into a zip.")
    parser.add_argument("output", nargs="?", default=os.path.join(ROOT, "bin", "MapEditor.zip"))
    parser.add_argument("--pdb", action="store_true", help="include MapEditor64.pdb")
    args = parser.parse_args()

    files = FILES + (["MapEditor64.pdb"] if args.pdb else [])
    missing = [name for name in files if not os.path.isfile(os.path.join(BIN, name))]

    if missing or not os.path.isdir(os.path.join(BIN, INTERFACE)):
        print("missing in %s: %s" % (BIN, ", ".join(missing or [INTERFACE])))
        return 1

    count = 0

    with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED) as archive:
        for name in files:
            archive.write(os.path.join(BIN, name), name)
            count += 1

        for directory, subdirs, names in os.walk(os.path.join(BIN, INTERFACE)):
            subdirs[:] = sorted(d for d in subdirs if d not in SKIP_DIRS)

            for name in sorted(names):
                path = os.path.join(directory, name)
                archive.write(path, os.path.relpath(path, BIN))
                count += 1

    print("wrote %s (%d files, %.1f MB)" % (args.output, count, os.path.getsize(args.output) / 1048576.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
