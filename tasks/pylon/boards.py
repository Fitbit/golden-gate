"""Board constants"""
def get_board_constants(board):
    if board == 'nrf52dk':
        return {
            "pselreset_reg_val": 21,
            "ram_start_addr": 0x20000000,
            "ram_end_addr": 0x20010000,
            "flash_start_addr": 0x00008000,
            "flash_end_addr": 0x00042000,
            "app_max_size": 232 * 1024,
        }
    elif board == 'nrf52840pdk' or board == 'nrf52840pdk_dle':
        return {
            "pselreset_reg_val": 18,
            "ram_start_addr": 0x20000000,
            "ram_end_addr": 0x20040000,
            "flash_start_addr": 0x0000c000,
            "flash_end_addr": 0x00082000,
            "app_max_size": 472 * 1024,
        }

    print('Unsupported board type {}!'.format(board))
