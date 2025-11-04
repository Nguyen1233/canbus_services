#!/usr/bin/env python3
"""
send_can_dbus_example.py

Example: send a 2-byte CAN payload over D-Bus to the CAN listener service.

Assumptions / encoding:
 - First byte: warning level (0..255)
 - Second byte: distance in meters (0..255)

The CAN listener D-Bus interface (from project README) expects:
 - Service: org.example.DMS.CAN
 - Object path: /org/example/DMS/CANListener
 - Interface: org.example.DMS.CAN
 - Method: SendCANMessage(uint32 canId, array<byte> data) -> bool

This script contains two ways to call the method:
 1) dbus-python (requires python-dbus / python3-dbus package)
 2) gdbus via subprocess (no Python DBus binding required)

Usage examples (session bus):
  # with dbus-python
  python3 send_can_dbus_example.py --method dbus --warning 2 --distance 45

  # with gdbus (subprocess)
  python3 send_can_dbus_example.py --method gdbus --warning 2 --distance 45

If your services use the system bus, pass --bus system (requires privileges to talk to system bus).

"""

import argparse
import json
import subprocess
import sys

# Try to import dbus only when needed; keep script usable when dbus not installed
try:
    import dbus
except Exception:
    dbus = None


SERVICE_NAME = 'org.example.DMS.CAN'
OBJECT_PATH = '/org/example/DMS/CANListener'
INTERFACE = 'org.example.DMS.CAN'


def send_via_dbus_python(can_id: int, warning: int, distance: int, use_system_bus: bool = False):
    if dbus is None:
        raise RuntimeError('dbus-python is not available. Install python3-dbus or use --method gdbus')

    bus = dbus.SystemBus() if use_system_bus else dbus.SessionBus()
    proxy = bus.get_object(SERVICE_NAME, OBJECT_PATH)
    iface = dbus.Interface(proxy, dbus_interface=INTERFACE)

    # Build the byte array as array of dbus.Byte
    data = [dbus.Byte(warning & 0xFF), dbus.Byte(distance & 0xFF)]

    print(f"Sending via dbus-python: canId=0x{can_id:X}, data={[hex(x) for x in (warning&0xFF, distance&0xFF)]}")
    result = iface.SendCANMessage(dbus.UInt32(can_id), data)
    print('Result from SendCANMessage ->', result)
    return result


def parse_args():
    p = argparse.ArgumentParser(description='Send 2-byte CAN payload over D-Bus to CAN listener')
    p.add_argument('--method', choices=['dbus', 'gdbus'], default='dbus', help='Which back-end to use')
    p.add_argument('--bus', choices=['session', 'system'], default='session', help='D-Bus bus type')
    p.add_argument('--can-id', type=lambda x: int(x, 0), default=0x567, help='CAN ID (hex or decimal)')
    p.add_argument('--warning', type=int, default=1, help='Warning level (0-255)')
    p.add_argument('--distance', type=int, default=10, help='Distance in meters (0-255)')
    return p.parse_args()


def main():
    args = parse_args()
    use_system_bus = args.bus == 'system'

    # Validate byte ranges
    if not (0 <= args.warning <= 0xFF and 0 <= args.distance <= 0xFF):
        print('warning and distance must be in range 0..255', file=sys.stderr)
        sys.exit(2)

    if args.method == 'dbus':
        try:
            ok = send_via_dbus_python(args.can_id, args.warning, args.distance, use_system_bus)
            print('Success' if ok else 'Failure')
        except Exception as e:
            print('Error sending via dbus-python:', e, file=sys.stderr)
            sys.exit(1)
    else:
        sys.exit('gdbus method not implemented in this snippet')


if __name__ == '__main__':
    main()
