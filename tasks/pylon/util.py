"""Pylon helper functions"""
import sys

from . import jlink

def select_device(sn_list):
    '''Select Pylon board to which to connect from a list of S/N'''
    if not sn_list:
        print("No Pylon board connected to host!")
    elif len(sn_list) == 1:
        return sn_list[0]
    else:
        # Prompt device selection
        print("\nMultiple Pylon boards detected:")
        for (i, serialno) in enumerate(sn_list):
            print("{idx}) S/N: {sn}".format(idx=i, sn=serialno))
        print("Enter S/N index: ", end="")

        # Get input and select correct S/N
        try:
            idx = int(sys.stdin.readline())
            if idx >= 0 and idx < len(sn_list):
                return sn_list[idx]
        except ValueError:
            pass

        print("Invalid value selected!")

    return None

def get_device_sn(ctx, sn):  # pylint: disable=C0103
    '''Validate S/N or prompt to select device is sn=None'''
    sn_list = jlink.get_emu_sn_list(ctx, product_name="J-Link OB-SAM3U128-V2-NordicSem")

    if sn:
        if sn not in sn_list:
            print("Invalid S/N selected!")
            sn = None
    else:
        sn = select_device(sn_list)

    return sn
