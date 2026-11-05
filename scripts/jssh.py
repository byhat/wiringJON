#!/usr/bin/env python3
"""SSH runner for the Jetson Orin Nano test bench.

Usage:
  jssh.py 'command'              -- run command, stream output
  jssh.py --sudo 'command'       -- run with sudo (password injected)
  jssh.py --put LOCAL REMOTE     -- copy file to board
  jssh.py --get REMOTE LOCAL     -- copy file from board

Connection settings come from env: JON_HOST, JON_USER, JON_PASS
(defaults: 192.168.109.9 / admin / admin).
"""
import os
import sys

import paramiko

HOST = os.environ.get("JON_HOST", "192.168.109.9")
USER = os.environ.get("JON_USER", "admin")
PASS = os.environ.get("JON_PASS", "admin")


def connect():
    cli = paramiko.SSHClient()
    cli.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    cli.connect(HOST, username=USER, password=PASS, timeout=10,
                look_for_keys=False, allow_agent=False)
    return cli


def run(cmd, use_sudo=False):
    cli = connect()
    try:
        if use_sudo:
            cmd = "sudo -S -p '' sh -c " + shell_quote(cmd)
        stdin, stdout, stderr = cli.exec_command(cmd, timeout=600, get_pty=False)
        if use_sudo:
            stdin.write(PASS + "\n")
            stdin.flush()
        out = stdout.read().decode(errors="replace")
        err = stderr.read().decode(errors="replace")
        rc = stdout.channel.recv_exit_status()
        if out:
            sys.stdout.write(out)
        if err:
            sys.stderr.write(err)
        return rc
    finally:
        cli.close()


def shell_quote(s):
    return "'" + s.replace("'", "'\\''") + "'"


def main():
    args = sys.argv[1:]
    use_sudo = False
    if args and args[0] == "--sudo":
        use_sudo = True
        args = args[1:]
    if not args:
        sys.exit(__doc__)
    if args[0] == "--put":
        cli = connect()
        try:
            sftp = cli.open_sftp()
            sftp.put(args[1], args[2])
            return 0
        finally:
            cli.close()
    if args[0] == "--get":
        cli = connect()
        try:
            sftp = cli.open_sftp()
            sftp.get(args[1], args[2])
            return 0
        finally:
            cli.close()
    if args[0] == "--sync":
        # --sync LOCALDIR REMOTEDIR [exclude1 exclude2 ...]
        import io
        import os as _os
        import tarfile
        local, remote = args[1], args[2]
        excludes = args[3:]
        buf = io.BytesIO()
        with tarfile.open(fileobj=buf, mode="w:gz") as tf:
            for root, dirs, files in os.walk(local):
                dirs[:] = [d for d in dirs if d not in excludes]
                for fn in files:
                    if fn in excludes:
                        continue
                    full = _os.path.join(root, fn)
                    tf.add(full, arcname=_os.path.relpath(full, local))
        cli = connect()
        try:
            sftp = cli.open_sftp()
            try:
                sftp.mkdir(remote)
            except IOError:
                pass
            tf = tarfile.open(fileobj=io.BytesIO(buf.getvalue()))
            count = 0
            for mem in tf.getmembers():
                if not mem.isfile():
                    continue
                path = remote + "/" + mem.name
                parts = path.split("/")[:-1]
                for i in range(2, len(parts) + 1):
                    p = "/".join(parts[:i])
                    try:
                        sftp.mkdir(p)
                    except IOError:
                        pass
                f = sftp.open(path, "wb")
                f.write(tf.extractfile(mem).read())
                f.close()
                count += 1
            print("synced %d files to %s" % (count, remote))
            return 0
        finally:
            cli.close()
    return run(args[0], use_sudo)


if __name__ == "__main__":
    sys.exit(main())
