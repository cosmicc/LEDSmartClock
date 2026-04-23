Import("env")

from datetime import datetime, timezone
from pathlib import Path
import subprocess


def read_git_value(args, default="unknown"):
    try:
        output = subprocess.check_output(args, stderr=subprocess.DEVNULL, text=True).strip()
    except Exception:
        return default
    return output if output else default


def quote_define(value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'\\"{escaped}\\"'


project_dir = Path(env.subst("$PROJECT_DIR"))
commit = read_git_value(["git", "-C", str(project_dir), "rev-parse", "--short", "HEAD"])
dirty_state = read_git_value(
    ["git", "-C", str(project_dir), "status", "--porcelain", "--untracked-files=no"],
    default="",
)
if dirty_state:
    commit = f"{commit}-dirty"

board_target = env.subst("$PIOENV") or env.subst("$BOARD") or "unknown"
build_date_utc = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

env.Append(
    CPPDEFINES=[
        ("BUILD_GIT_COMMIT", quote_define(commit)),
        ("BUILD_TARGET_BOARD", quote_define(board_target)),
        ("BUILD_DATE_UTC", quote_define(build_date_utc)),
    ]
)

print(
    f"Embedded build metadata: commit={commit}, board={board_target}, build_date_utc={build_date_utc}"
)
