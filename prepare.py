"""Build a device-specific Magisk ZIP from an authorized ADB connection."""
import argparse
import shlex
import subprocess
import tempfile
import uuid
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--serial", help="adb device serial")
parser.add_argument("--probe", action="store_true", help="only list input devices")
args = parser.parse_args()
adb = ["adb"] + (["-s", args.serial] if args.serial else [])


def shell(command, root=False):
    if root:
        command = "su -c " + shlex.quote(command)
    return subprocess.check_output(adb + ["shell", command], stdin=subprocess.DEVNULL).replace(b"\r\n", b"\n")


def select(rows, column, label):
    candidates = [r for r in rows if r[column] == "1" and r[1] != "Local PIN touchscreen"]
    if not candidates:
        raise SystemExit("缺少兼容的输入设备：" + label)
    for i, row in enumerate(candidates, 1):
        print(f"  {i}. {row[1]} ({row[0]}, {row[2] or '无物理路径'})")
    index = 1 if len(candidates) == 1 else int(input(f"选择{label}的实体设备编号："))
    if not 1 <= index <= len(candidates):
        raise SystemExit("无效编号")
    name = candidates[index-1][1]
    if sum(r[1] == name for r in rows) != 1:
        raise SystemExit("输入设备重名，无法按名称安全匹配：" + name)
    print(label, name)
    return name


if shell("getprop ro.product.cpu.abi").strip() != b"arm64-v8a":
    raise SystemExit("预编译程序需要 ARM64 Android")
remote = "/data/local/tmp/touch-probe-" + uuid.uuid4().hex
try:
    subprocess.run(adb + ["push", str(ROOT/"touch-window"), remote], check=True, stdin=subprocess.DEVNULL)
    output = shell(f"chmod 755 {remote} && {remote} --probe", root=True)
finally:
    shell("rm -f " + remote)
rows = [line.split("\t") for line in output.decode().splitlines()]
if not rows or any(len(row) != 7 for row in rows):
    raise SystemExit("无法读取输入设备列表")
if args.probe:
    print("event\tname\tphys\ttouch\tpower\tup\tdown")
    print(output.decode(), end="")
    raise SystemExit(0)
shell("test -w /dev/uinput", root=True)
names = [select(rows, i, label) for i,label in enumerate(
    ["触摸屏", "电源键", "音量加", "音量减"], 3)]
path = "/system/etc/excluded-input-devices.xml"
original = shell(f"if [ -e {path} ]; then cat {path}; else echo '<devices/>'; fi", root=True)
tree = ET.fromstring(original, parser=ET.XMLParser(target=ET.TreeBuilder(insert_comments=True)))
if tree.tag != "devices":
    raise SystemExit("设备排除表格式不兼容")
if not any(node.get("name") == names[0] for node in tree.findall("device")):
    ET.SubElement(tree, "device", name=names[0])
with tempfile.TemporaryDirectory(dir=ROOT) as tmp:
    package = Path(tmp)/"AndroidTouchBlock-local.zip"
    with zipfile.ZipFile(package, "w", zipfile.ZIP_DEFLATED) as z:
        for file in ["service.sh", "module.prop", "touch-window", "customize.sh",
                     "system/app/TouchWindowNotice/TouchWindowNotice.apk"]:
            z.write(ROOT/file, file)
        z.writestr("inputs.conf", "\n".join(names) + "\n")
        z.writestr("system/etc/excluded-input-devices.xml", ET.tostring(tree, encoding="utf-8", xml_declaration=True))
    package.replace(ROOT/package.name)
print("已生成 AndroidTouchBlock-local.zip。请仅在刚才探测的设备上安装。")
