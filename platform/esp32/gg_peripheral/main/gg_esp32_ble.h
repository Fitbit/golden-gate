//-----------------------------------------------------
// Includes
//-----------------------------------------------------
#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"
#include "xp/stack_builder/gg_stack_builder.h"

//-----------------------------------------------------
// Functions
//-----------------------------------------------------
bool gg_esp32_ble_init(GG_Loop* loop);
void gg_esp32_ble_attach_stack(GG_DataSource* source);
void gg_on_link_up(void);
void gg_on_link_down(void);
void gg_on_mtu_changed(unsigned int mtu);
void gg_on_packet_received(GG_Buffer* packet);
